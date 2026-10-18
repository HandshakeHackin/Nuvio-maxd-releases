// Build the complete history row independently of catalog page budgets.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.hpp"
#include <map>
#include <functional>
namespace sx {
std::vector<Item> make_continue_watching(const std::map<std::string,json>& library,
    const std::map<std::string,Progress>& progress, bool account_ready,
    const std::function<void(Item&)>& apply_meta);
}
