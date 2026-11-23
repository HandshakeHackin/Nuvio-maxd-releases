// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../src/live_usb.hpp"
namespace sx::usb_reader_client {
struct Scan {std::vector<live_usb::File> files;std::vector<live_usb::MountDiagnostics> mounts;std::string error;};
Scan scan(const std::atomic<bool>& cancel);
live_tv::Result import_playlist(const live_usb::File&,const std::string& profile,const std::string& id,const std::atomic<bool>& cancel);
}
