#include "pin_entry.hpp"
#include <cassert>
#include <iostream>
using namespace hui;
InputFrame press(Action a) { InputFrame in;in.pressed=action_bit(a);return in; }
void move(sx::PinEntry& p,Direction d,int count=1) {for(int i=0;i<count;++i){InputFrame in;in.nav=d;p.input(in);}}
int main() {
    using Result=sx::PinEntry::Result;
    sx::PinEntry p;
    move(p,Direction::down,3);move(p,Direction::right,2);
    assert(p.selected()==11 && p.input(press(Action::confirm))==Result::none && p.size()==0);
    p.clear();p.input(press(Action::confirm));
    move(p,Direction::right);p.input(press(Action::confirm));
    move(p,Direction::down);p.input(press(Action::confirm));
    move(p,Direction::down,2);p.input(press(Action::confirm));
    assert(p.size()==4 && p.masked()=="••••");
    move(p,Direction::right);assert(p.input(press(Action::confirm))==Result::submit);
    assert(p.take()=="1250" && p.size()==0 && p.masked().empty() && p.selected()==0);
    move(p,Direction::down,3);move(p,Direction::right);
    p.input(press(Action::confirm));p.input(press(Action::confirm));
    assert(p.take()=="00");
    InputFrame ignored=press(Action::north);p.input(ignored);assert(p.size()==0);
    InputFrame held;held.held=action_bit(Action::confirm);
    for(int i=0;i<30;++i)p.input(held);
    assert(p.size()==0);
    for(std::size_t i=0;i<sx::PinEntry::capacity+4;++i)p.input(press(Action::confirm));
    assert(p.size()==sx::PinEntry::capacity);
    p.input(press(Action::west));assert(p.size()==sx::PinEntry::capacity-1);
    move(p,Direction::down,3);assert(p.selected()==9);
    p.input(press(Action::confirm));assert(p.size()==sx::PinEntry::capacity-2);
    auto both=press(Action::back);both.pressed|=action_bit(Action::confirm);
    assert(p.input(both)==Result::cancel && p.size()==0 && p.masked().empty());
    move(p,Direction::left,20);move(p,Direction::up,20);assert(p.selected()==0);
    move(p,Direction::right,20);move(p,Direction::down,20);assert(p.selected()==11);
    p.clear();p.input(press(Action::west));assert(p.size()==0);
    std::cout<<"Profile PIN keypad: numeric mapping, masking, limit, delete, cancellation, submit clearing, held-button suppression and bounded navigation passed\n";
}
