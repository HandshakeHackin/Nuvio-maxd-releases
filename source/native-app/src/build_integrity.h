// Offline release provenance only. No network, account or device identifiers.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <string>
namespace build_integrity {
inline constexpr const char* project="HandshakeHackin/Nuvio-maxd-releases";
inline constexpr const char* project_url="https://github.com/HandshakeHackin/Nuvio-maxd-releases";
inline constexpr std::size_t max_manifest=128*1024,max_files=512;
inline constexpr unsigned long long max_file=128ull<<20,max_total=256ull<<20;
enum class Status {checking,verified,unverified,cancelled};
enum class Reason {none,missing_manifest,invalid_signature,invalid_manifest,unreadable_file,changed_file,deadline,crypto};
struct Result {Status status=Status::checking;Reason reason=Reason::none;std::size_t files_checked=0;bool credits_checked=false;};
Result verify(const std::string& root,const std::string& version,
              const std::array<unsigned char,32>& public_key,
              const std::atomic<bool>* cancel=nullptr,unsigned budget_ms=30000) noexcept;
}
