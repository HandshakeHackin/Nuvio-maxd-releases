// Authenticated encrypted, profile-scoped configuration backups.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "json.hpp"
#include <atomic>
#include <string>
namespace config_backup {
using Json = nlohmann::json;
constexpr size_t max_plain = 16u << 20, max_encrypted = max_plain + 52;
struct Result {
  std::string bytes, hash, error;
  Json snapshot;
};
bool valid_passphrase(const std::string &);
std::string hash(const std::string &);
std::string scope(const std::string &account, int profile);
Result capture(const std::string &directory, const std::string &scope,
               const std::atomic<bool> * = nullptr);
Result encrypt(const Json &, const std::string &passphrase,
               const std::atomic<bool> * = nullptr);
Result decrypt(const std::string &, const std::string &passphrase,
               const std::string &scope, const std::atomic<bool> * = nullptr);
bool validate(const Json &, const std::string &scope, std::string &error);
// All payloads validate before the first write. A durable rollback journal
// makes interrupted restores recoverable before profile settings are loaded
// again.
bool restore(const std::string &directory, const Json &,
             const std::string &scope, std::string &error);
bool recover(const std::string &directory, std::string &error);
bool save(const std::string &path, const std::string &bytes);
bool read(const std::string &path, std::string &bytes, size_t limit,
          bool *missing = nullptr);
} // namespace config_backup
