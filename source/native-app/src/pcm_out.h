// Sound out: negotiated 48 kHz stereo or 7.1 PCM; 16-bit PCM preferred.
//
// On the console it is sceAudioOut, one 256-frame grain at a time from a thread
// of its own (the call blocks for each grain, which paces the thread). Elsewhere
// a thread takes the samples at the same speed and drops them, so the player's
// clock, which is the audio queue, runs the same.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <pthread.h>
#include <vector>
#include <string>

// The console gives the app one audio port at a time. The interface sounds (gl/ui_sound.cpp) hold it
// while no video plays; the player says it wants it, and they let go.
struct AudioPort {
	static std::atomic<bool> player_wants;
	static std::atomic<bool> ui_holds;
};

class PcmOut {
public:
	static const int kRate = 48000;


	PcmOut() = default;
	PcmOut(const PcmOut&) = delete;
	PcmOut& operator=(const PcmOut&) = delete;
	~PcmOut() { close(); }

	bool open(bool surround = true);
	bool open_bitstream(int kind, bool check_receiver);
	int bitstream_kind() const { return bitstream_kind_; }
	int channels() const { return channels_; }
	bool floating_point() const { return floating_; }
	size_t bytes_per_frame() const { return size_t(channels_) * (floating_ ? 4 : 2); }
	double queued_seconds() const { return double(queued_bytes()) / (rate_.load() * bytes_per_frame()); }
	uint64_t played_frames() const { return played_frames_; }
	double played_seconds() const;
	std::string description() const;
	void close(bool release_claim = true);
	bool is_open() const { return open_; }
	bool failed() const { return failed_; }

	// Fixed two-second ring. False means no room; the caller waits without dropping.
	bool queue(const void* data, size_t bytes);
	size_t queued_bytes() const;
	// Paused: silence goes out and the queue stays where it is.
	void pause(bool p) { paused_ = p; }
	void clear();

private:
	static void* thread_main(void* self);
	void run();

	std::atomic<bool> open_{false};
	int handle_ = -1;
	pthread_t thread_{};
	std::atomic<bool> stop_{false};
	std::atomic<bool> failed_{false};
	std::atomic<bool> paused_{true};
	mutable std::mutex m_;
	std::vector<uint8_t> ring_;
	size_t head_ = 0, count_ = 0;
	std::atomic<int> channels_{2};
	std::atomic<int> rate_{kRate}, grain_frames_{256}, bitstream_kind_{0};
	bool start_thread();
	std::atomic<bool> floating_{true};
	std::atomic<uint64_t> played_frames_{0};
	uint64_t generation_ = 0;
	size_t inflight_frames_=0;
	double grain_started_=0;
};
