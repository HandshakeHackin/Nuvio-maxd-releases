// Native presentation of addon text. Badges describe supplied metadata; they
// do not claim that a stream is cached or that its codec can be decoded.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "content.hpp"
#include "stremio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>

namespace sx::stream_display
{
inline std::string plain(const std::string &s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size();)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const std::size_t len = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        if (i + len > s.size()) break;
        unsigned cp = c & (len == 1 ? 0x7f : len == 2 ? 0x1f : len == 3 ? 0x0f : 0x07);
        for (std::size_t k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i+k]) & 0x3f);
        // AIO formatters use mathematical bold/sans-serif letters. Normalize
        // these to the readable equivalents in the console's font.
        bool mapped = false;
        for (unsigned base : {0x1d400u, 0x1d434u, 0x1d468u, 0x1d5a0u, 0x1d5d4u, 0x1d608u, 0x1d63cu, 0x1d670u})
            if (cp >= base && cp < base + 52)
            {
                out += static_cast<char>(cp < base + 26 ? 'A' + cp - base : 'a' + cp - base - 26);
                mapped = true; break;
            }
        if (!mapped && cp >= 0x1d7ceu && cp <= 0x1d7ffu)
        { out += static_cast<char>('0' + (cp - 0x1d7ce) % 10); mapped = true; }
        if (!mapped && cp != 0xfe0f && cp != 0xfe0e && cp != 0x200d && cp < 0x1f000 &&
            !(cp >= 0x2600 && cp <= 0x27ff)) out.append(s, i, len);
        i += len;
    }
    return trim(out);
}
inline std::string first_line(const std::string &s)
{ return plain(s.substr(0, s.find('\n'))); }

// Token boundaries keep DV out of DVD and HDR out of unrelated filenames.
inline bool has(const std::string &s, const std::string &needle)
{
    std::size_t p = 0;
    while ((p = s.find(needle, p)) != std::string::npos)
    {
        const auto word = [](unsigned char c) { return std::isalnum(c) != 0; };
        if ((p == 0 || !word(s[p-1])) &&
            (p + needle.size() == s.size() || !word(s[p + needle.size()]))) return true;
        p += needle.size();
    }
    return false;
}
inline std::string metadata(const ::Stream &s)
{ return lower(plain(s.name + " " + s.description + " " + s.filename)); }
inline std::string resolution_of(const ::Stream &st)
{
    const std::string s = metadata(st);
    if (has(s,"2160p") || has(s,"4k") || has(s,"uhd") || has(s,"uhd⁴ᴷ")) return "4K";
    if (has(s,"1440p") || has(s,"2k")) return "1440p";
    if (has(s,"1080p") || has(s,"1080i") || has(s,"fhd")) return "1080p";
    if (has(s,"720p")) return "720p";
    if (has(s,"576p") || has(s,"480p") || has(s,"360p") || has(s,"dvdrip") || has(s,"dvdscr")) return "SD";
    return "Other";
}
inline int seeds_of(const std::string &s)
{
    const auto p = s.find("\xf0\x9f\x91\xa4");
    return p == std::string::npos ? -1 : std::atoi(s.c_str() + p + 4);
}
inline std::string size_of(const ::Stream &st)
{
    const std::string s = lower(st.description);
    for (const char *unit : {"gib", "mib", "tib", "gb", "mb", "tb"})
    {
        std::size_t u = 0;
        while ((u = s.find(unit, u)) != std::string::npos)
        {
            const std::size_t end = u + std::char_traits<char>::length(unit);
            if (end < s.size() && std::isalpha(static_cast<unsigned char>(s[end]))) { ++u; continue; }
            std::size_t b = u;
            while (b > 0 && s[b-1] == ' ') --b;
            const auto number_end = b;
            while (b > 0 && (std::isdigit(static_cast<unsigned char>(s[b-1])) || s[b-1] == '.' || s[b-1] == ',')) --b;
            if (b < number_end && std::isdigit(static_cast<unsigned char>(s[b])))
            {
                std::string label = unit;
                for (char &c : label) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                return st.description.substr(b, number_end - b) + " " + label;
            }
            ++u;
        }
    }
    if (st.video_size > 0 && std::isfinite(st.video_size))
    {
        double n = st.video_size / 1000000.0;
        const char *unit = "MB";
        if (n >= 1000) { n /= 1000; unit = "GB"; }
        if (n >= 1000) { n /= 1000; unit = "TB"; }
        char buf[48]; std::snprintf(buf, sizeof(buf), "%.1f %s", n, unit); return buf;
    }
    return "";
}
inline std::string language_of(const ::Stream &st)
{
    const std::string raw = st.name + "\n" + st.description;
    for (const char *mark : {"\xf0\x9f\x9b\xb0", "\xf0\x9f\x8c\x90", "\xf0\x9f\x97\xa3"})
    {
        auto p = raw.find(mark);
        if (p == std::string::npos) continue;
        std::string line = plain(raw.substr(p + 4, raw.find('\n', p) - (p + 4)));
        // Do not absorb the next decorated metadata field.
        const auto cut = line.find("  ");
        if (cut != std::string::npos) line.resize(cut);
        if (!line.empty()) return line;
    }
    std::vector<std::string> codes;
    for (std::size_t i = 0; i + 7 < raw.size(); ++i)
        if (static_cast<unsigned char>(raw[i]) == 0xf0 && static_cast<unsigned char>(raw[i+1]) == 0x9f &&
            static_cast<unsigned char>(raw[i+2]) == 0x87 && static_cast<unsigned char>(raw[i+3]) >= 0xa6 &&
            static_cast<unsigned char>(raw[i+3]) <= 0xbf && raw.substr(i+4, 3) == "\xf0\x9f\x87" &&
            static_cast<unsigned char>(raw[i+7]) >= 0xa6 && static_cast<unsigned char>(raw[i+7]) <= 0xbf)
        {
            std::string code; code += 'A' + static_cast<unsigned char>(raw[i+3]) - 0xa6;
            code += 'A' + static_cast<unsigned char>(raw[i+7]) - 0xa6;
            if (std::find(codes.begin(), codes.end(), code) == codes.end()) codes.push_back(code);
            i += 7;
        }
    if (!codes.empty()) return join(codes, " / ");
    const std::string m = metadata(st);
    if (has(m,"multi") || has(m,"multi audio") || has(m,"multilingual")) return "MULTI";
    for (const char *lang : {"English", "Spanish", "French", "German", "Italian", "Portuguese", "Japanese", "Korean", "Hindi"})
        if (has(m,lower(lang))) return lang;
    return "";
}
inline Stream view_of(const ::Stream &st)
{
    Stream v;
    v.resolution = resolution_of(st);
    v.addon = st.addon;
    v.identity = st.addon + "|" + st.url + "|" + st.info_hash + "|" + std::to_string(st.file_idx);
    if (st.client_resolve.is_object()) v.identity += "|" + json::array({jstr(st.client_resolve, "service"),
        jstr(st.client_resolve, "infoHash"), jnum(st.client_resolve, "fileIdx", -1),
        jstr(st.client_resolve, "filename"), jstr(st.client_resolve, "magnetUri")}).dump();
    v.name = first_line(st.name);
    if (v.name.empty()) v.name = st.addon;
    v.detail = !st.filename.empty() ? plain(st.filename) : first_line(st.description);
    if (v.detail.empty()) v.detail = v.name;
    v.size = size_of(st); v.language = language_of(st);
    const int seeds = seeds_of(st.description);
    if (st.direct_http()) v.seeds = "Direct / debrid";
    else if (st.debrid_resolvable) v.seeds = (st.debrid_service == "torbox" ? "TorBox" : "Premiumize") +
        std::string(st.client_resolve.is_object() && jbool(st.client_resolve, "isCached") ? " / cached" : " / resolve");
    else if (seeds >= 0) v.seeds = std::to_string(seeds) + (seeds == 1 ? " seed" : " seeds");
    else if (st.torrent()) v.seeds = "Torrent";
    const std::string m = metadata(st);
    const auto any = [&](std::initializer_list<const char *> terms) { for (auto t : terms) if (has(m,t)) return true; return false; };
    const auto add = [&](StreamBadge badge) { v.badges.push_back(badge); };
    using B = StreamBadge;
    if (any({"web-dl", "web dl", "webdl"})) add(B::web_dl);
    else if (any({"webrip", "web-rip", "web rip"})) add(B::web_rip);
    if (has(m,"remux")) add(B::remux);
    else if (any({"bluray", "blu-ray", "blu ray", "bdrip"})) add(B::bluray);
    if (v.resolution == "4K") add(B::uhd);
    else if (v.resolution == "1080p") add(B::full_hd);
    else if (v.resolution == "720p") add(B::hd);
    if (any({"dolby vision", "dovi", "dv"})) add(B::dolby_vision);
    if (any({"hdr10+", "hdr10plus", "hdr10 plus"})) add(B::hdr10_plus);
    else if (any({"hdr", "hdr10"})) add(B::hdr);
    else if (has(m,"sdr")) add(B::sdr);
    if (any({"hevc", "x265", "h265", "h.265"})) add(B::hevc);
    else if (any({"avc", "h264", "h.264", "x264"})) add(B::avc);
    else if (has(m,"av1")) add(B::av1);
    if (has(m,"atmos")) add(B::atmos);
    if (has(m,"truehd")) add(B::truehd);
    else if (any({"ddp", "dd+", "ddp5.1", "ddp7.1", "dd+5.1", "dd+7.1", "eac3", "e-ac3", "e-ac-3", "dolby digital plus"})) add(B::dolby_plus);
    else if (any({"ac3", "ac-3", "dd", "dolby digital"})) add(B::dolby);
    else if (any({"dts-hd", "dts hd", "dts-hdma", "dts:x"})) add(B::dts_hd);
    else if (has(m,"dts")) add(B::dts);
    if (has(m,"7.1")) add(B::channels_71);
    else if (has(m,"5.1")) add(B::channels_51);
    else if (any({"2.0", "stereo"})) add(B::stereo);
    // Preserve the launch screen's concise text alongside the card badges.
    std::vector<std::string> tags;
    if (any({"dolby vision", "dovi", "dv"})) tags.push_back("Dolby Vision");
    if (any({"hdr", "hdr10", "hdr10+"})) tags.push_back("HDR");
    if (has(m,"hevc") || has(m,"x265")) tags.push_back("HEVC");
    if (has(m,"atmos")) tags.push_back("Atmos");
    if (has(m,"5.1")) tags.push_back("5.1");
    v.tags = join(tags,"  \xc2\xb7  ");
    return v;
}
} // namespace sx::stream_display
