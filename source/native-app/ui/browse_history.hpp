// Bounded, profile-scoped navigation snapshots, owned by the UI thread.
#pragma once
#include <optional>
#include <cstddef>
#include <vector>
namespace sx {
template<class Frame> class BrowseHistory {
public:
 static constexpr size_t capacity=8;
 bool push(Frame frame){if(frames_.size()>=capacity)return false;frames_.push_back(std::move(frame));return true;}
 std::optional<Frame> pop(){if(frames_.empty())return {};Frame frame=std::move(frames_.back());frames_.pop_back();return frame;}
 void scope(unsigned epoch){if(epoch!=epoch_){clear();epoch_=epoch;}}
 void clear(){frames_.clear();}
 bool full()const{return frames_.size()>=capacity;}
 size_t size()const{return frames_.size();}
private:
 unsigned epoch_=0;std::vector<Frame> frames_;
};
}
