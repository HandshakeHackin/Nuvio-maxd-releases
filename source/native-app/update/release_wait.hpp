// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core.hpp"
namespace nuvio_update {
enum class ReleaseState { retry, released, refused };
// Unmount success only releases the title runtime. In persistent-image mode
// the backing image remains mapped, so the independent snapshot is required.
inline ReleaseState release_state(const Json& unmount, const Json& images,
                                  const std::string& path) {
    if (!unmount.is_object() || !unmount.contains("status") || !unmount["status"].is_number_integer())
        return ReleaseState::retry;
    const int status=unmount["status"].get<int>();
    if(status==16) return ReleaseState::retry; // EBUSY: app exit is still in progress.
    if(status!=0) return ReleaseState::refused;
    return mount_released(images,path) ? ReleaseState::released : ReleaseState::retry;
}
}
