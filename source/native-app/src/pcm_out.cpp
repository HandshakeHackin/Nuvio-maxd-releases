#include "pcm_out.h"
#include "bitstream.h"
#include "audio_pcm.h"

#include <algorithm>
#include <cstring>
#include <time.h>

#include "util.h"

#ifdef PLATFORM_PS5_NATIVE
extern "C" {
int sceAudioOutInit(void);
int sceAudioOutOpen(int user, int type, int index, unsigned int grain_frames, unsigned int frequency,
                    unsigned int format);
int sceAudioOutOutput(int handle, const void* samples);
int sceAudioOutClose(int handle);
}
#endif

namespace {
const int kGrainFrames = 256;
#ifdef PLATFORM_PS5_NATIVE
const int kAlreadyInitialized = int(0x8026000e);
const int kSystemUser = 0xff;
const int kPortMain = 0;
#endif
}  // namespace

std::atomic<bool> AudioPort::player_wants{false};
std::atomic<bool> AudioPort::ui_holds{false};

bool PcmOut::open(bool surround) {
	if (open_) return true;
    if(!bitstream::ensure_pcm()) {dlog("audio: PCM restore refused; output stays silent");return false;}
	channels_ = surround ? 8 : 2; floating_ = false;
	rate_=kRate;grain_frames_=kGrainFrames;bitstream_kind_=0;
#ifdef PLATFORM_PS5_NATIVE
	// The interface may have the port: say we want it, and give it up to 1.5 s to let go.
	AudioPort::player_wants = true;
	for (int i = 0; i < 300 && AudioPort::ui_holds.load(); ++i) {
		timespec ts{0, 5 * 1000 * 1000};
		nanosleep(&ts, nullptr);
	}
	int init = sceAudioOutInit();
	if (init != 0 && init != kAlreadyInitialized) {
		dlog("audio: sceAudioOutInit 0x%08x", unsigned(init));
		AudioPort::player_wants = false;
		return false;
	}
	// Prefer the S16 path used by the PS5 media players. Keep float as a last
	// stereo fallback only when the console refuses both compatible S16 ports.
	for (int candidate : (surround ? std::vector<int>{2, 1, 4} : std::vector<int>{1, 4})) {
		handle_ = sceAudioOutOpen(kSystemUser, kPortMain, 0, kGrainFrames, kRate, unsigned(candidate));
		if (handle_ >= 0) {
			channels_ = candidate == 5 || candidate == 2 ? 8 : 2;
			floating_ = candidate >= 4; break;
		}
		dlog("audio: output format %d refused 0x%08x", candidate, unsigned(handle_));
	}
	if (handle_ < 0) {
		handle_ = -1; AudioPort::player_wants = false; return false;
	}
#endif
    return start_thread();
}
bool PcmOut::open_bitstream(int kind, bool check_receiver) {
    if(open_) return false;
    AudioPort::player_wants=true;
    for(int i=0;i<300 && AudioPort::ui_holds;++i) {timespec ts{0,5000000};nanosleep(&ts,nullptr);}
    if(AudioPort::ui_holds) return false;
    handle_=bitstream::open_port(kind,check_receiver);
    if(handle_<0) return false;
    bitstream_kind_=kind; channels_=2; floating_=false;
    rate_=bitstream::rate(kind); grain_frames_=kind==bitstream::Eac3?1024:256;
    return start_thread();
}
bool PcmOut::start_thread() {
	{
		std::lock_guard<std::mutex> lock(m_);
		ring_.assign(rate_.load() * bytes_per_frame() * 2, 0);
		head_ = count_ = 0; inflight_frames_=0; played_frames_ = 0; ++generation_;
	}
	stop_ = false; failed_ = false;
	paused_ = true;
	// Never name the thread: pthread_setname_np hangs on the console.
	if (pthread_create(&thread_, nullptr, &PcmOut::thread_main, this) != 0) {
		dlog("audio: can't start the output thread");
#ifdef PLATFORM_PS5_NATIVE
		if(bitstream_kind_) bitstream::close_port(handle_); else sceAudioOutClose(handle_);
#endif
		handle_ = -1;
		AudioPort::player_wants = false;
		return false;
	}
	dlog("audio: output %s", description().c_str());
	if (!bitstream_kind_) dlog("audio: PCM clock accounts for two output grains (%.2f ms estimate)", 2000.0*grain_frames_.load()/rate_.load());
	open_ = true;
	return true;
}

void PcmOut::close(bool release_claim) {
	if (!open_) return;
	stop_ = true;
	pthread_join(thread_, nullptr);
#ifdef PLATFORM_PS5_NATIVE
	if (handle_ >= 0) { if(bitstream_kind_) bitstream::close_port(handle_); else sceAudioOutClose(handle_); }
#endif
	handle_ = -1;
	open_ = false;
	if(release_claim && bitstream::ensure_pcm()) AudioPort::player_wants = false;  // the interface may have the port back
}

std::string PcmOut::description() const {
	if(bitstream_kind_) return std::string(bitstream::name(bitstream_kind_))+" bitstream";
	return std::string(channels_ == 8 ? "7.1 PCM" : "stereo PCM") + (floating_ ? " 32-bit float" : " 16-bit");
}

bool PcmOut::queue(const void* data, size_t bytes) {
	std::lock_guard<std::mutex> lock(m_);
	if (!data || bytes % bytes_per_frame() || bytes > ring_.size() - count_) return false;
	const size_t tail = (head_ + count_) % ring_.size();
	const size_t first = std::min(bytes, ring_.size() - tail);
	std::memcpy(ring_.data() + tail, data, first);
	std::memcpy(ring_.data(), static_cast<const uint8_t*>(data) + first, bytes - first);
	count_ += bytes; return true;
}

size_t PcmOut::queued_bytes() const {
	std::lock_guard<std::mutex> lock(m_); return count_;
}

double PcmOut::played_seconds() const {
    std::lock_guard<std::mutex> lock(m_);
    // Interpolate within the current grain, bounded by real queued media.
    // This removes 5.3 ms clock steps at 120 Hz without advancing through silence.
    const double fraction=paused_ ? 0 : std::clamp(now_seconds()-grain_started_,0.0,double(inflight_frames_)/rate_.load());
    return pcm_audible_seconds(double(played_frames_.load())/rate_.load()+fraction,
                              rate_.load(), grain_frames_.load(), bitstream_kind_.load()!=0);
}

void PcmOut::clear() {
	std::lock_guard<std::mutex> lock(m_);
	head_ = count_ = 0; inflight_frames_=0; played_frames_ = 0; ++generation_;
}

void* PcmOut::thread_main(void* self) {
	static_cast<PcmOut*>(self)->run();
	return nullptr;
}

void PcmOut::run() {
	alignas(64) uint8_t grain[kGrainFrames * 8 * sizeof(float)];
	const int frames=grain_frames_,rate=rate_;
	const size_t grain_bytes = frames * bytes_per_frame();
    int errors=0;
	while (!stop_) {
		size_t got = 0; uint64_t epoch;
		{
			std::lock_guard<std::mutex> lock(m_); epoch = generation_;
			if (!paused_) {
				got = std::min(grain_bytes, count_);
				const size_t first = std::min(got, ring_.size() - head_);
				std::memcpy(grain, ring_.data() + head_, first);
				std::memcpy(grain + first, ring_.data(), got - first);
				head_ = (head_ + got) % ring_.size(); count_ -= got;
                inflight_frames_=got/bytes_per_frame(); grain_started_=now_seconds();
			}
		}
		if (got < grain_bytes) std::memset(grain + got, 0, grain_bytes - got);
#ifdef PLATFORM_PS5_NATIVE
		const double output_started=now_seconds();
		const bool output_ok = sceAudioOutOutput(handle_, grain) >= 0;
		if (!output_ok) { timespec ts{0, 5000000}; nanosleep(&ts, nullptr); }
#else
		const double output_started=now_seconds();
		const bool output_ok = true;
		timespec ts{0, long(1e9 * frames / rate)}; nanosleep(&ts, nullptr);
#endif
        if(bitstream_kind_) {
            const double left=output_started+double(frames)/rate-now_seconds();
            if(left>0 && left<1) {timespec ts{0,long(left*1e9)};nanosleep(&ts,nullptr);}
        }
		if(output_ok) errors=0; else if(++errors>=3) { failed_=true; break; }
		// Silence, pause and the grain from before a seek never advance media time.
		std::lock_guard<std::mutex> lock(m_);
		if (epoch == generation_) {
            if(output_ok) played_frames_ += got / bytes_per_frame();
            inflight_frames_=0;
        }
	}
#ifdef PLATFORM_PS5_NATIVE
	sceAudioOutOutput(handle_, nullptr);
#endif
}
