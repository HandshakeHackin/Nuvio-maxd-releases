#pragma once
#include "plex.h"
#include <cassert>
#include <openssl/evp.h>
struct PlexFixture {
  json jwk;
  std::string device, part = "/library/parts/42/file.mkv";
  bool approved = false, renewed = false, reject = false, deny = false,
       bad_json = false, multipart = false, bad_page = false;
  int refreshes = 0, calls = 0, timeline = 0, stopped = 0;
  std::vector<std::string> paths;
  static std::string un64(std::string s) {
    for (auto &c : s) {
      if (c == '-')
        c = '+';
      else if (c == '_')
        c = '/';
    }
    int padding = (4 - s.size() % 4) % 4;
    s.append(padding, '=');
    std::string r(s.size(), 0);
    int n = EVP_DecodeBlock((unsigned char *)r.data(),
                            (const unsigned char *)s.data(), s.size());
    assert(n >= padding);
    r.resize(n - padding);
    return r;
  }
  void verify(const std::string &token, bool refresh) {
    auto a = token.find('.'), b = token.find('.', a + 1);
    assert(a != std::string::npos && b != std::string::npos);
    auto h = json::parse(un64(token.substr(0, a))),
         p = json::parse(un64(token.substr(a + 1, b - a - 1)));
    assert(h["alg"] == "EdDSA" && h["kid"] == jwk["kid"] &&
           p["aud"] == "plex.tv" && p["iss"] == device);
    assert(p["exp"].get<double>() > now_epoch_ms() / 1000.);
    if (refresh)
      assert(p["nonce"] == "challenge" && p["scope"] == "username");
    auto pub = un64(jwk["x"]), sig = un64(token.substr(b + 1));
    auto key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                                           (const unsigned char *)pub.data(),
                                           pub.size());
    auto ctx = EVP_MD_CTX_new();
    assert(EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1);
    assert(EVP_DigestVerify(ctx, (const unsigned char *)sig.data(), sig.size(),
                            (const unsigned char *)token.data(), b) == 1);
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
  }
  json metadata() const {
    json parts = json::array({{{"key", part}}});
    if (multipart)
      parts.push_back({{"key", "/library/parts/43/file.mkv"}});
    return {{"ratingKey", "42"},
            {"title", "Fixture movie"},
            {"type", "movie"},
            {"duration", 120000},
            {"viewOffset", 25000},
            {"Media", json::array({{{"id", 70},
                                    {"videoResolution", "1080"},
                                    {"videoCodec", "h264"},
                                    {"audioChannels", 2},
                                    {"Part", parts}}})}};
  }
  HttpResponse request(const media_server::Connection &c,
                       const std::string &path, const json *body,
                       const std::atomic<bool> *) {
    ++calls;
    paths.push_back(path);
    auto reply = [](json j, long status = 200) {
      HttpResponse r;
      r.status = status;
      r.body = j.dump();
      return r;
    };
    if (c.base == "https://clients.plex.tv") {
      if (path == "/api/v2/pins") {
        assert(body && c.token.empty() && (*body)["strong"] == false);
        jwk = (*body)["jwk"];
        device = c.device;
        assert(jwk["crv"] == "Ed25519" && un64(jwk["x"]).size() == 32);
        return reply({{"id", 123}, {"code", "ABCD"}, {"expiresIn", 600}});
      }
      if (path.starts_with("/api/v2/pins/123?deviceJWT=")) {
        assert(c.token.empty());
        verify(path.substr(path.find('=') + 1), false);
        return reply({{"authToken", approved ? "account-token" : ""}});
      }
      if (path == "/api/v2/auth/nonce") {
        assert(c.token.empty());
        return reply({{"nonce", "challenge"}});
      }
      if (path == "/api/v2/auth/token") {
        assert(body && c.token.empty());
        verify((*body)["jwt"], true);
        ++refreshes;
        renewed = true;
        return reply({{"auth_token", "new-account-token"}});
      }
      if (path.starts_with("/api/v2/resources?")) {
        assert(c.token == (renewed ? "new-account-token" : "account-token"));
        return reply(json::array(
            {{{"provides", "server"},
              {"clientIdentifier", "server-1"},
              {"name", "Test Plex"},
              {"accessToken", renewed ? "new-server-token" : "server-token"},
              {"connections",
               json::array(
                   {{{"uri", "http://remote.invalid:32400"}, {"local", false}},
                    {{"uri", "https://remote.invalid:32400"}, {"local", false}},
                    {{"uri", "http://127.0.0.1:32400"}, {"local", true}},
                    {{"uri", "https://user:password@remote.invalid"},
                     {"local", true}}})}},
             {{"provides", "player"}, {"clientIdentifier", "not-a-server"}}}));
      }
      assert(false);
    }
    assert(c.kind == "plex" &&
           c.token == (renewed ? "new-server-token" : "server-token"));
    assert(c.token.find("account") == std::string::npos);
    if (reject && !renewed)
      return reply({}, 498);
    if (path == "/")
      return reply({{"MediaContainer", {{"machineIdentifier", "server-1"}}}});
    if (path.starts_with("/:/timeline?")) {
      ++timeline;
      assert(path.find("time=12500") != std::string::npos);
      HttpResponse r;
      r.status = 200;
      r.body = "<MediaContainer/>";
      return r;
    }
    if (path.starts_with("/video/:/transcode/universal/stop?")) {
      ++stopped;
      return reply({});
    }
    if (path.starts_with("/video/:/transcode/universal/decision?"))
      return reply(
          {{"MediaContainer", {{"generalDecisionCode", deny ? 2000 : 1001}}}});
    if (path == "/library/metadata/42/thumb") {
      HttpResponse r;
      r.status = 200;
      r.body = "image";
      return r;
    }
    if (path == "/library/metadata/42")
      return reply(
          {{"MediaContainer", {{"Metadata", json::array({metadata()})}}}});
    if (bad_json)
      return reply({{"wrong", "schema"}});
    if (path.starts_with("/library/sections?"))
      return reply({{"MediaContainer",
                     {{"Directory", json::array({{{"key", "1"},
                                                  {"title", "Movies"},
                                                  {"type", "movie"}}})}}}});
    json mc = {{"Metadata", json::array({metadata()})},
               {"totalSize", 101},
               {"offset",
                bad_page
                    ? 0
                    : (path.find("Start=100") != std::string::npos ? 100 : 0)}};
    if (path.starts_with("/hubs/search?"))
      mc = {{"Hub", json::array({{{"Metadata", json::array({metadata()})}}})}};
    return reply({{"MediaContainer", mc}});
  }
  plex::Client client() {
    return plex::Client(
        [this](const auto &c, const auto &p, const auto *b, const auto *stop) {
          return request(c, p, b, stop);
        });
  }
};
