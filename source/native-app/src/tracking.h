// Profile-scoped tracker connections and library/resume adapters.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "http.h"
#include "util.h"
#include <functional>

namespace tracking {
std::string name(const std::string& provider);
bool provider(const std::string& id);
// Optional registered app identities supplied by the build owner, never account tokens.
void set_applications(const json& applications);
using Transport = std::function<HttpResponse(const std::string&, const std::string*, const std::vector<std::string>&)>;
class Store {
public:
    explicit Store(std::string directory, Transport transport = {}, const std::atomic<bool>* cancel = nullptr);
    json status() const; // Safe for UI: never includes credentials or pairing tokens.
    json action(const std::string& action, const json& args, std::string& error);
    std::string source(bool progress) const;
    bool library(json& items, std::string& error);
    bool progress(json& rows, std::string& error);
    bool write(const json& items, std::string& error);
    bool watched(json& rows, std::string& error);
    bool unsent(const json& item) const;
    bool mark(const std::string& content, int season, int episode, bool watched, std::string& error);
private:
    std::string dir_, path_;
    json state_;
    bool readable_ = true;
    const std::atomic<bool>* cancel_ = nullptr;
    Transport transport_;
    bool save(std::string& error);
    bool connected(const std::string& p) const;
    HttpResponse request(const std::string& p, const std::string& path, const json* body = nullptr, bool auth = true);
    bool validate(const std::string& p, std::string& error);
    bool refresh(const std::string& p, std::string& error);
    bool fetch(const std::string& p, const std::string& path, json& out, std::string& error);
    bool flush(const std::string& p, std::string& error);
    bool read_library(json& items, std::string& error);
    bool read_progress(json& rows, std::string& error);
};
}
