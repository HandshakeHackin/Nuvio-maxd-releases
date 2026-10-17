#pragma once
#include <algorithm>

namespace sx {
struct DropdownWindow { int first, visible; };
inline DropdownWindow dropdown_window(int count, int selected, float message_height = 0) {
    count = std::max(0, count);
    // Leave room for the title, message, range indicator and screen margins.
    const int capacity = std::clamp(static_cast<int>((910.0f - 150.0f - message_height - 28.0f) / 76.0f), 1, 9);
    const int visible = std::min(count, capacity);
    selected = std::clamp(selected, 0, std::max(0, count - 1));
    return {std::clamp(selected - visible / 2, 0, count - visible), visible};
}
} // namespace sx
