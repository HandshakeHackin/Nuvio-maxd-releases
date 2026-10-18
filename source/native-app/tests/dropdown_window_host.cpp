#include "dropdown_window.hpp"
#include <cassert>
#include <cstdio>
int main() {
    for (int count : {0, 1, 3, 9, 10, 32, 258}) {
        for (float message : {0.0f, 110.0f, 270.0f}) {
            for (int selected = 0; selected < count; ++selected) {
                const auto w = sx::dropdown_window(count, selected, message);
                assert(w.first <= selected && selected < w.first + w.visible);
                assert(w.first >= 0 && w.first + w.visible <= count);
                assert(130 + 150 + message + w.visible * 76 + (w.visible < count ? 28 : 0) <= 1040);
            }
        }
    }
    assert(sx::dropdown_window(0, 0).visible == 0);
    assert(sx::dropdown_window(32, 31).first + sx::dropdown_window(32, 31).visible == 32);
    puts("Dropdown scrolling: every selection stays visible, first/last choices reachable, panel fits screen");
}
