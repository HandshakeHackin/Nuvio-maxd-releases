// Controller-only profile PIN entry. Never routed through the system IME.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/input.hpp"
#include <array>
#include <string>

namespace sx {
class PinEntry {
public:
    enum class Result { none, cancel, submit };
    static constexpr std::size_t capacity = 16;
    PinEntry() = default;
    PinEntry(const PinEntry&) = delete;
    PinEntry& operator=(const PinEntry&) = delete;
    ~PinEntry() { clear(); }
    void clear() {
        volatile char* p = digits_.data();
        for (std::size_t i=0;i<digits_.size();++i) p[i]=0;
        size_=0; selected_=0;
    }
    std::size_t size() const { return size_; }
    int selected() const { return selected_; }
    std::string masked() const {
        std::string out;
        for (std::size_t i=0;i<size_;++i) out+="•";
        return out;
    }
    std::string take() {
        std::string value(digits_.data(),size_);
        clear();
        return value;
    }
    Result input(const hui::InputFrame& in) {
        if (in.is_pressed(hui::Action::back)) { clear(); return Result::cancel; }
        int row=selected_/3,col=selected_%3;
        switch (in.nav) {
        case hui::Direction::up: if(row>0)--row; break;
        case hui::Direction::down: if(row<3)++row; break;
        case hui::Direction::left: if(col>0)--col; break;
        case hui::Direction::right: if(col<2)++col; break;
        default: break;
        }
        selected_=row*3+col;
        if (in.is_pressed(hui::Action::west)) erase();
        else if (in.is_pressed(hui::Action::confirm)) {
            if (selected_==9) erase();
            else if (selected_==11) { if(size_) return Result::submit; }
            else if (size_<capacity) digits_[size_++]=selected_==10?'0':char('1'+selected_);
        }
        return Result::none;
    }
private:
    void erase() { if(size_) { volatile char* p=digits_.data();p[--size_]=0; } }
    std::array<char,capacity> digits_{};
    std::size_t size_=0;
    int selected_=0;
};
} // namespace sx
