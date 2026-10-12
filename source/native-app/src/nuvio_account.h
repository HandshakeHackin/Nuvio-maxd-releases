// Native Nuvio device pairing and persistent sessions.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

bool nuvio_account_init(const std::string& config_path, const std::string& data_dir);
bool nuvio_account_saved();
std::string nuvio_account_user_id(); // cached identity only; no network request
void nuvio_pairing_cancel();
