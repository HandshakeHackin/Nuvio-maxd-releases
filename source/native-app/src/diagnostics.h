#pragma once
#include "json.hpp"
#include <string>
namespace nuvio_diagnostics {
// UI-thread API. Only whitelisted scalar fields are persisted; no URLs, names or credentials.
nlohmann::json filter_context(const nlohmann::json&);
void init(const std::string& directory, const std::string& version);
void record(const nlohmann::json& context);
void shutdown();
std::string classify_error(const std::string& error);
std::string support_summary(const std::string& version);
}
