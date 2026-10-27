#include "player.h"
#include "player_streams.hpp"
#include "bitstream.h"
#include "evo_backend.h"
#include "frame_selection.h"
#include "audio_pcm.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dovi_meta.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

#include "http.h"
#include "netstream.h"
#include "stremio.h"
#include "torrent/torrent_stream.h"
#include "util.h"
#include "hdr_tone_map.h"

static const int kOutRate = 48000;
static const double kMaxQueuedAudio = 0.35;            // seconds kept queued for output
// Seconds buffered before playback starts. Each time the download falls
// behind and playback has to stop, the next wait is longer (up to 30 s), so a
// stream that's barely fast enough stops a few times for longer instead of
// every few seconds (4K over a torrent, PS5 2026-10-06).
static const double kBufferTargets[] = {2, 5, 10, 20, 30};
// Packets held in memory: 30 s of a 4K remux at ~60 Mbit/s.
static const size_t kMaxQueueBytes = 256u << 20;
static const size_t kHardQueueBytes = 320u << 20; // bounded headroom while the other stream starves
static const int kMaxFrames = 4;

static void delay_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static std::string av_err(int err) {
	char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
	av_strerror(err, buf, sizeof(buf));
	return buf;
}

// ---------------------------------------------------------------------------
// Packet queue

void Player::PacketQueue::put(AVPacket* pkt, double pts_s, double dur_s) {
	std::lock_guard<std::mutex> lock(m_);
	q_.push_back(Item{pkt, pts_s, dur_s});
	bytes_ += pkt->size;
	if (pts_s >= 0) last_pts_ = std::max(last_pts_, pts_s);
	dur_sum_ += dur_s;
	cv_.notify_one();
}

int Player::PacketQueue::get(AVPacket** pkt, int* serial, int timeout_ms) {
	std::unique_lock<std::mutex> lock(m_);
	if (q_.empty() && !eof_ && !abort_)
		cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return !q_.empty() || eof_ || abort_; });
	if (abort_) return 0;
	if (q_.empty()) return eof_ ? -1 : 0;
	Item it = q_.front();
	q_.pop_front();
	*pkt = it.pkt;
	bytes_ -= it.pkt->size;
	dur_sum_ = std::max(0.0, dur_sum_ - it.dur);
	*serial = serial_;
	if (q_.empty()) {
		last_pts_ = -1;
		dur_sum_ = 0;
	}
	return 1;
}

void Player::PacketQueue::flush() {
	std::lock_guard<std::mutex> lock(m_);
	for (auto& it : q_) av_packet_free(&it.pkt);
	q_.clear();
	bytes_ = 0;
	last_pts_ = -1;
	dur_sum_ = 0;
	eof_ = false;
	serial_++;
	cv_.notify_all();
}

void Player::PacketQueue::set_eof(bool eof) {
	std::lock_guard<std::mutex> lock(m_);
	eof_ = eof;
	cv_.notify_all();
}

void Player::PacketQueue::abort() {
	std::lock_guard<std::mutex> lock(m_);
	abort_ = true;
	cv_.notify_all();
}

void Player::PacketQueue::reset() {
	std::lock_guard<std::mutex> lock(m_);
	for (auto& it : q_) av_packet_free(&it.pkt);
	q_.clear();
	bytes_ = 0;
	last_pts_ = -1;
	dur_sum_ = 0;
	eof_ = false;
	abort_ = false;
	serial_++;
}

size_t Player::PacketQueue::count() const {
	std::lock_guard<std::mutex> lock(m_);
	return q_.size();
}

size_t Player::PacketQueue::bytes() const {
	std::lock_guard<std::mutex> lock(m_);
	return bytes_;
}

double Player::PacketQueue::duration() const {
	std::lock_guard<std::mutex> lock(m_);
	if (q_.empty()) return 0;
	double first = q_.front().pts;
	double spread = (first >= 0 && last_pts_ >= first) ? last_pts_ - first : 0;
	return std::max(spread, dur_sum_);
}

int Player::PacketQueue::serial() const {
	std::lock_guard<std::mutex> lock(m_);
	return serial_;
}

// ---------------------------------------------------------------------------
// Lifecycle

Player::Player() = default;
Player::~Player() { close(); }

void Player::open(const Options& opts) {
	close();
	if(opts.vlc) {
        subtitle_renderer_.clear();
        {std::lock_guard<std::mutex> lock(sub_m_);embedded_.clear();}
		opts_=opts;
		vlc_=std::make_unique<VlcBackend>();
		VlcBackend::Options v{opts.url,opts.headers,opts.audio_langs,opts.start,opts.surround,opts.start_paused};
		v.audio=opts.initial_audio_stream;v.gain_percent=opts.gain_percent;v.audio_delay=opts.audio_delay;vlc_->open(v);
		return;
	}
	opts_ = opts;
    audio_delay_=std::isfinite(opts.audio_delay)?std::clamp(opts.audio_delay,-2.0,2.0):0;
    gain_percent_=std::clamp(opts.gain_percent,50,300);pcm_.set_gain(gain_percent_);
	evo_presenter_ = opts.evo ? std::make_unique<EvoPresenter>() : nullptr;
	evo_presenter_serial_ = -1;
	dlog("player: playback engine %s", opts.evo ? "EVO Player 0.13.0 (experimental)" : "Nuvio native");
	abort_ = false;
	quick_ = false;
	eof_ = false;
	paused_ = opts.start_paused;
	buffering_ = true;
	buffer_percent_ = 0;
	dropped_ = decoded_ = shown_ = 0;
	diagnostic_shown_ = 0; diagnostic_time_ = 0;
	log_at_ = 0;
	stalls_ = 0;
	stall_count_ = 0;
	start_target_ = 0;
	too_heavy_ = false;
	heavy_window_start_ = now_seconds();
	heavy_window_dropped_ = heavy_window_decoded_ = 0;
	video_drained_ = audio_drained_ = false;
	have_picture_ = false;
	video_lead_ = 0;
	seek_req_ = false;
	seek_pending_ = demux_reading_ = seek_refill_ = false;
	reopen_target_ = -1; displayed_serial_ = -1;
	seek_started_at_ = 0; seek_first_picture_ms_ = -1;
	vdrop_ = adrop_ = opts.start > 0 ? opts.start : -1;
	audio_base_pts_ = audio_end_pts_ = -1;
	wall_base_pts_ = opts.start;
	wall_base_time_ = now_seconds();
	wall_running_ = false;
	duration_ = 0;
	fps_ = 0;
	start_offset_ = 0;
	{
		std::lock_guard<std::mutex> lock(err_m_);
		error_.clear();
	}
	{
		std::lock_guard<std::mutex> lock(sub_m_);
		embedded_.clear();
	}
	subtitle_renderer_.clear();
	packet_storage_=std::make_unique<PacketStorage>(); recovering_=false;
	vq_.reset();
	aq_.reset();
	state_ = State::Opening;
	open_started_ = now_seconds();
	demux_ = std::thread([this] { demux_thread(); });
}

void Player::close() {
	if(vlc_) {vlc_->close();vlc_.reset();}
	abort_ = true;
	vq_.abort();
	aq_.abort();
	fq_cv_.notify_all();
	if (demux_.joinable()) demux_.join();  // joins the decoder threads too
	evo_presenter_.reset();
	pcm_.close();
	vq_.flush(); aq_.flush(); packet_storage_.reset();
	frames_.clear();
	current_ = Frame();
	pool_.clear();
	state_ = State::Idle;
}

void Player::fail(const std::string& msg) {
	dlog("player: %s", msg.c_str());
	{
		std::lock_guard<std::mutex> lock(err_m_);
		error_ = msg;
	}
	state_ = State::Failed;
}

std::string Player::error() const {
	if(vlc_) return vlc_->error();
	std::lock_guard<std::mutex> lock(err_m_);
	return error_;
}

int Player::interrupt_cb(void* opaque) {
	auto* p = static_cast<Player*>(opaque);
	// Interrupt a packet read for a pending seek, not the seek operation itself.
	// There is no wall deadline that cuts a slow stream off.
	return p->abort_ || (p->demux_reading_ && p->seek_pending_) ? 1 : 0;
}

double Player::to_sec(int64_t ts, double tb) const {
	if (ts == AV_NOPTS_VALUE) return -1;
	return double(ts) * tb - start_offset_;
}

bool Player::open_stream_codec(int stream, AVCodecContext** out) {
	AVStream* st = fmt_->streams[stream];
	const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
	if (!codec) {
		dlog("player: no decoder for %s", avcodec_get_name(st->codecpar->codec_id));
		return false;
	}
	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(ctx, st->codecpar);
	ctx->pkt_timebase = st->time_base;
	if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
		// The PS5 has 8 Zen 2 cores; FFmpeg can't count them here, so say it.
		ctx->thread_count = opts_.preview ? 1 : 8;
		ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
	}
	int r = avcodec_open2(ctx, codec, nullptr);
	if (r < 0) {
		dlog("player: can't open %s decoder: %s", codec->name, av_err(r).c_str());
		avcodec_free_context(&ctx);
		return false;
	}
	*out = ctx;
	return true;
}

static bool is_supported_subtitle(const AVCodecParameters* par) {
	if (par->codec_type != AVMEDIA_TYPE_SUBTITLE) return false;
	const AVCodecDescriptor* d = avcodec_descriptor_get(par->codec_id);
	return d && (d->props & (AV_CODEC_PROP_TEXT_SUB | AV_CODEC_PROP_BITMAP_SUB));
}

// ---------------------------------------------------------------------------
// Demuxer

void Player::demux_thread() {
	demux_run();
	// The input is closed by now; the network stream behind it goes last.
	if (TorrentStream::is_ours(main_pb_)) TorrentStream::close_avio(&main_pb_);
	else if (main_pb_) NetStream::close_avio(&main_pb_);
}

void Player::demux_run() {
	std::string input_url = opts_.url;
	const AVInputFormat* content_format = nullptr;
	AVDictionary* dict = nullptr;
	av_dict_set(&dict, "user_agent", kUserAgent, 0);
	if (!opts_.headers.empty()) av_dict_set(&dict, "headers", (join(opts_.headers, "\r\n") + "\r\n").c_str(), 0);
	av_dict_set(&dict, "reconnect", "1", 0);
	av_dict_set(&dict, "reconnect_streamed", "1", 0);
	av_dict_set(&dict, "reconnect_on_network_error", "1", 0);
	av_dict_set(&dict, "reconnect_delay_max", "5", 0);
	// A torrent's first bytes can take a while (the server is still finding
	// peers); Circle still cancels at once through the interrupt callback.
	av_dict_set(&dict, "rw_timeout", "120000000", 0);
	av_dict_set(&dict, "tls_verify", "0", 0);
	av_dict_set(&dict, "seekable", "-1", 0);
	// HLS keepalive assumes FFmpeg-owned URLContexts; our AVIOs belong to
	// NetStream. Open each playlist/segment through the hooks instead.
	av_dict_set(&dict, "http_persistent", "0", 0);
    if(opts_.live_tv) {
        // Xtream services commonly use signed/extensionless HTTP segment paths.
        // Restrict protocols before relaxing HLS filename-extension checks.
        av_dict_set(&dict,"protocol_whitelist","http,https,tcp,tls,crypto",0);
        av_dict_set(&dict,"allowed_extensions","ALL",0);
        av_dict_set(&dict,"allowed_segment_extensions","ALL",0);
        av_dict_set(&dict,"extension_picky","0",0);
    }
	if (!http_ca_bundle().empty()) av_dict_set(&dict, "ca_file", http_ca_bundle().c_str(), 0);

	fmt_ = avformat_alloc_context();
	fmt_->interrupt_callback.callback = interrupt_cb;
	fmt_->interrupt_callback.opaque = this;
	fmt_->probesize = 5 << 20;
	fmt_->max_analyze_duration = 5 * AV_TIME_BASE;

	// HTTP(S) goes through libcurl (see netstream.h): the stream itself here,
	// and whatever the demuxer opens later (HLS) through the hooks.
	io_hooks_.http_only = opts_.live_tv;
	io_hooks_.headers = opts_.headers;
	io_hooks_.abort = &abort_;
	io_hooks_.read_interrupt = &seek_pending_;
	NetStream::install(fmt_, &io_hooks_);
	dlog("player: opening stream");
	bool torrent = TorrentStream::is_url(opts_.url);
	if (torrent || starts_with(opts_.url, "http://") || starts_with(opts_.url, "https://")) {
		std::string err;
		// A torrent from the built-in engine, or HTTP(S) through libcurl.
		main_pb_ = torrent ? TorrentStream::open_avio(opts_.url, &abort_, &err)
		                   : NetStream::open_avio(opts_.url, opts_.headers, &abort_, &err, opts_.parallel);
		if (!main_pb_) {
			avformat_free_context(fmt_);
			fmt_ = nullptr;
			av_dict_free(&dict);
			if (!abort_) fail("Could not open the stream (" + err + ")");
			return;
		}
		NetStream::set_read_interrupt(main_pb_, &seek_pending_);
		fmt_->pb = main_pb_;
		fmt_->flags |= AVFMT_FLAG_CUSTOM_IO;
		// Matroska keeps tags, attachments and its index at the end of the
		// file, and FFmpeg reads them while opening. For a torrent that's
		// the part the server has least of, so it could take minutes before
		// the first frame. Start from the beginning instead, without them;
		// the first seek opens the file again with the index (see seek()).
		std::string magic = torrent ? TorrentStream::of(main_pb_)->peek(4) : NetStream::of(main_pb_)->peek(64);
		if (!torrent) {
			input_url = NetStream::of(main_pb_)->effective_url();
			// Some providers serve genuine HLS at opaque addresses as octet-stream.
			// FFmpeg's URL/MIME probe refuses these even after seeing EXT-X tags.
			// Select HLS only from its actual header; keep the real resolved URL
			// and existing I/O restrictions for relative segments and cancellation.
			if (starts_with(magic, "#EXTM3U") && magic.find("#EXT-X-") != std::string::npos) {
				content_format = av_find_input_format("hls");
				dlog("player: identified HLS from playlist contents");
			}
			std::string prefix = lower(trim(magic));
			if (starts_with(prefix, "<!doctype html") || starts_with(prefix, "<html")) {
				avformat_free_context(fmt_);
				fmt_ = nullptr;
				av_dict_free(&dict);
				if (!abort_) fail("The source returned a web page instead of video. Refresh the source or choose another stream.");
				return;
			}
		}
		if (opts_.start <= 0 && starts_with(magic, "\x1A\x45\xDF\xA3")) {
			main_pb_->seekable = 0;
			quick_ = true;
			// Matroska describes its tracks in the header, so a short look
			// is enough; with few seeders every MB here is a long wait.
			fmt_->probesize = 1 << 20;
			fmt_->max_analyze_duration = AV_TIME_BASE / 2;
			dlog("player: Matroska, starting without reading the end of the file");
		}
	}
	int r = avformat_open_input(&fmt_, input_url.c_str(), content_format, &dict);
	av_dict_free(&dict);
	if (r < 0) {
		fmt_ = nullptr;  // freed by avformat_open_input on failure
		if (!abort_) fail("Could not open the stream (" + av_err(r) + ")");
		return;
	}
	r = avformat_find_stream_info(fmt_, nullptr);
	if (r < 0 && !abort_) dlog("player: find_stream_info: %s", av_err(r).c_str());
	if (abort_) {
		avformat_close_input(&fmt_);
		return;
	}
	if (fmt_->start_time != AV_NOPTS_VALUE) start_offset_ = double(fmt_->start_time) / AV_TIME_BASE;
	if (fmt_->duration != AV_NOPTS_VALUE && fmt_->duration > 0) duration_ = double(fmt_->duration) / AV_TIME_BASE;

	int vs = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	int as = -1;
	// Preferred audio language first, else FFmpeg's pick.
	for (auto& want : opts_.audio_langs) {
		for (unsigned i = 0; i < fmt_->nb_streams && as < 0; i++) {
			AVStream* st = fmt_->streams[i];
			if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO || !playback_streams::compatible_audio(fmt_, vs, int(i))) continue;
			AVDictionaryEntry* e = av_dict_get(st->metadata, "language", nullptr, 0);
			if (e && language_to_iso639_2(e->value) == want) as = int(i);
		}
		if (as >= 0) break;
	}
	if (opts_.initial_audio_stream >= 0 && opts_.initial_audio_stream < int(fmt_->nb_streams) &&
	    fmt_->streams[opts_.initial_audio_stream]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
        playback_streams::compatible_audio(fmt_, vs, opts_.initial_audio_stream))
		as = opts_.initial_audio_stream;
	if (as < 0) as = av_find_best_stream(fmt_, AVMEDIA_TYPE_AUDIO, -1, vs, nullptr, 0);
    if (as >= 0 && !playback_streams::compatible_audio(fmt_, vs, as)) {
        as = -1;
        for (unsigned i = 0; i < fmt_->nb_streams; ++i)
            if (fmt_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                playback_streams::compatible_audio(fmt_, vs, int(i))) { as = int(i); break; }
    }
	if (vs < 0 && as < 0) {
		avformat_close_input(&fmt_);
		fail("No video or audio in this stream");
		return;
	}

	// Keep source colour tags. The UI thread chooses native HDR10 or explicit
	// SDR conversion; a 10-bit codec alone is not evidence of an HDR source.
	if (vs >= 0) {
		const AVCodecParameters* p = fmt_->streams[vs]->codecpar;
		bool sd_space = p->color_space == AVCOL_SPC_BT470BG || p->color_space == AVCOL_SPC_SMPTE170M;
		colors_ = p->color_trc == AVCOL_TRC_SMPTE2084 ? YuvColors::Hdr10
		          : (sd_space || (p->color_space == AVCOL_SPC_UNSPECIFIED && p->height < 720)) ? YuvColors::Bt601
		                                                                                       : YuvColors::Bt709;
		full_range_ = p->color_range == AVCOL_RANGE_JPEG;
		double content_peak = 0, mastering_peak = 0;
		const AVPacketSideData* light = av_packet_side_data_get(p->coded_side_data,
		    p->nb_coded_side_data, AV_PKT_DATA_CONTENT_LIGHT_LEVEL);
		if (light && light->size >= sizeof(AVContentLightMetadata)) {
			AVContentLightMetadata metadata;
			std::memcpy(&metadata, light->data, sizeof(metadata));
			content_peak = metadata.MaxCLL;
		}
		const AVPacketSideData* mastering = av_packet_side_data_get(p->coded_side_data,
		    p->nb_coded_side_data, AV_PKT_DATA_MASTERING_DISPLAY_METADATA);
		if (mastering && mastering->size >= sizeof(AVMasteringDisplayMetadata)) {
			AVMasteringDisplayMetadata metadata;
			std::memcpy(&metadata, mastering->data, sizeof(metadata));
			if (metadata.has_luminance && metadata.max_luminance.den > 0)
				mastering_peak = av_q2d(metadata.max_luminance);
		}
		hdr_peak_nits_ = hdr_peak_nits(content_peak, mastering_peak);
		if (colors_ == YuvColors::Hdr10)
			dlog("player: SDR tone-map reference peak %.0f nits (%s)", hdr_peak_nits_,
			     content_peak == hdr_peak_nits_ ? "MaxCLL" : mastering_peak == hdr_peak_nits_ ? "mastering display" : "default");
		hdr10_compatible_ = colors_ == YuvColors::Hdr10 &&
		    p->color_space == AVCOL_SPC_BT2020_NCL && p->color_primaries == AVCOL_PRI_BT2020;
		const AVPacketSideData* dv = av_packet_side_data_get(p->coded_side_data,
		    p->nb_coded_side_data, AV_PKT_DATA_DOVI_CONF);
		if (dv && dv->size >= sizeof(AVDOVIDecoderConfigurationRecord)) {
			AVDOVIDecoderConfigurationRecord config;
			std::memcpy(&config,dv->data,sizeof(config));
			if (config.dv_profile == 5) hdr10_compatible_ = false;
			dlog("player: Dolby Vision profile %u; HDR10 base compatible %d (DV metadata is not output)",
			     config.dv_profile, hdr10_compatible_);
		}
		dlog("player: colours %s (transfer %d, matrix %d, primaries %d)",
		     colors_ == YuvColors::Hdr10 ? "PQ source" : colors_ == YuvColors::Bt601 ? "BT.601" : "BT.709",
		     int(p->color_trc), int(p->color_space), int(p->color_primaries));
	}
	// Video: the hardware decoder when it takes the stream; FFmpeg's software
	// decoder otherwise (and if the hardware one fails later).
    if(opts_.preview && (vs<0 || fmt_->streams[vs]->codecpar->width>1920 ||
       fmt_->streams[vs]->codecpar->height>1088 || colors_==YuvColors::Hdr10 ||
       fmt_->streams[vs]->codecpar->color_trc==AVCOL_TRC_ARIB_STD_B67)) {
        avformat_close_input(&fmt_);fail("Four-screen TV supports SDR channels up to 1080p. Choose a lower-quality channel.");return;
    }
	if (vs >= 0 && opts_.evo && !opts_.preview) {
		std::string why;
		evo_ = EvoDecoder::open(fmt_->streams[vs]->codecpar, why);
		if (!evo_) { avformat_close_input(&fmt_); fail(why); return; }
		hw_active_ = evo_->hardware();
	}
	if (vs >= 0 && !opts_.evo && !opts_.preview) {
		std::string why;
		hw_.reset(HwDecoder::open(fmt_->streams[vs]->codecpar, &why));
		hw_active_ = hw_ != nullptr;
		hw_shift_ = -1;
		if (!hw_) dlog("player: software video decoding (%s)", why.c_str());
	}
	if (vs >= 0 && !hw_ && !evo_ && !open_stream_codec(vs, &vctx_)) {
		AVStream* st = fmt_->streams[vs];
		std::string name = avcodec_get_name(st->codecpar->codec_id);
		avformat_close_input(&fmt_);
		fail("This video's format (" + name + ") can't be played. Pick another stream.");
		return;
	}
    if(opts_.video_only)as=-1;
	if (as >= 0 && !open_stream_codec(as, &actx_)) as = -1;
	for (unsigned i = 0; i < fmt_->nb_streams; i++) {
		AVStream* st = fmt_->streams[i];
		bool keep = int(i) == vs || int(i) == as;
		if (!opts_.preview && is_supported_subtitle(st->codecpar)) {
			AVCodecContext* sc = nullptr;
			if (open_stream_codec(int(i), &sc)) {
				sctx_[int(i)] = sc;
				AVDictionaryEntry* language=av_dict_get(st->metadata,"language",nullptr,0);
				subtitle_renderer_.add_track(int(i),st->codecpar,language?language->value:"");
				keep = true;
			}
		}
		// Keep ordinary file tracks readable for switching. For HLS, inactive
        // audio playlists must not compete with the selected video.
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            keep = !opts_.video_only && (!playback_streams::is_hls(fmt_) || int(i) == as);
		st->discard = keep ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
	}
    if (playback_streams::is_hls(fmt_))
        dlog("player: HLS selected video=%d audio=%d programs=%u; unused audio playlists disabled",vs,as,fmt_->nb_programs);
	video_stream_ = vs;
	audio_stream_ = as;
	if (vs < 0) vdrop_ = -1;
	if (vs >= 0) {
		AVRational fr = av_guess_frame_rate(fmt_, fmt_->streams[vs], nullptr);
		if (fr.num > 0 && fr.den > 0) fps_ = av_q2d(fr);
	}

	if (as >= 0) {
		open_audio_output(as);
	}

	dlog("player: %s, %.0fs, video #%d %s, audio #%d %s", fmt_->iformat->name, duration_, vs,
	     vs >= 0 ? avcodec_get_name(fmt_->streams[vs]->codecpar->codec_id) : "-", as,
	     as >= 0 ? avcodec_get_name(fmt_->streams[as]->codecpar->codec_id) : "-");

	// How much to buffer before playing (and after a seek): a high-bitrate
	// file (a 4K remux) needs more, or playback starts before the download
	// is up to speed and runs dry within the first minute (PS5 2026-10-06:
	// Avengers: Endgame 4K, ~25 Mbit/s, stopped once at 45 s with 2 s buffered).
	{
		int64_t size = fmt_->pb ? avio_size(fmt_->pb) : -1;
		double mbps = fmt_->bit_rate > 0 ? fmt_->bit_rate / 1e6
		              : (size > 0 && duration_ > 0) ? size * 8.0 / duration_ / 1e6
		                                            : 0;
		start_target_ = opts_.live_tv ? 2 : mbps >= 40 ? 15 : mbps >= 20 ? 10 : mbps >= 10 ? 5 : 2;
		dlog("player: about %.0f Mbit/s, buffering %.0f s before playing", mbps, start_target_);
	}
	if (opts_.start > 0) do_seek(opts_.start);
	state_ = State::Playing;
	apply_run_state();

	if (vs >= 0) video_ = std::thread([this] { video_thread(); });
	if (as >= 0 && pcm_.is_open()) audio_ = std::thread([this] { audio_thread(); });

	AVPacket* pkt = av_packet_alloc();
	int errors = 0;
	while (!abort_) {
		double target = -1;
		{
			std::lock_guard<std::mutex> lock(seek_m_);
			if (seek_req_) {
				target = seek_target_;
				seek_req_ = false;
                // Retire the finished timeline before releasing the UI seek gate.
                // do_seek can block on I/O while the old queues still look drained.
                eof_ = false;
				seek_pending_ = false;
			}
		}
		int sw = audio_switch_.exchange(-1);
		if (sw >= 0 && sw < int(fmt_->nb_streams) && sw != audio_stream_ &&
            fmt_->streams[sw]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
            playback_streams::compatible_audio(fmt_, vs, sw)) {
			AVCodecContext* nc = nullptr;
			if (open_stream_codec(sw, &nc)) {
                // Capture the timeline before reopening the port resets played_seconds().
                // Keep an explicit seek (including one queued during codec open) ahead
                // of this anchor, and retire every packet from the previous track.
                const double anchor = target >= 0 ? target : position();
				std::lock_guard<std::mutex> lock(actx_m_);
                aq_.flush();
				avcodec_free_context(&actx_);
				actx_ = nc;
                if (playback_streams::is_hls(fmt_)) {
                    if (audio_stream_ >= 0) fmt_->streams[audio_stream_]->discard = AVDISCARD_ALL;
                    fmt_->streams[sw]->discard = AVDISCARD_DEFAULT;
                }
				audio_stream_ = sw;
                {
                    std::lock_guard<std::mutex> clock(clock_m_);
                    pcm_.close(false);
                    audio_base_pts_ = audio_end_pts_ = -1;
                    wall_base_pts_ = anchor; wall_base_time_ = now_seconds();
                    open_audio_output(sw);
                }
                if (!pcm_.is_open()) {
                    fail("Could not reopen audio for the selected track. Restart playback.");
                    break;
                }
                dlog("player: audio switch to #%d at %.3f s (paused %d)", sw, anchor, int(paused_.load()));
                // A quick Matroska input must load its cues too. Otherwise the
                // demuxer remains at its read-ahead position, seconds past playback.
                // Keep the decoder lock until queues and clocks share the new serial.
                if (playback_streams::is_hls(fmt_) && duration_ <= 0) {
                    adrop_ = anchor;
                    apply_run_state(); // Live playlists cannot rewind to a VOD timestamp.
                } else {
                    do_seek(anchor, true);
                }
                target = -1;
			}
		}
		if (target >= 0) do_seek(target);

		update_buffering();

        const size_t queued=vq_.bytes()+aq_.bytes();
        const bool other_starving=(vs>=0 && vq_.duration()<0.5) ||
            (audio_stream_>=0 && aq_.duration()<0.5);
        if(queued>=(opts_.preview?16u<<20:kHardQueueBytes) || (queued>=(opts_.preview?8u<<20:kMaxQueueBytes) && !other_starving) ||
            vq_.count()+aq_.count()>65536 ||
            ((vs<0 || vq_.duration()>(opts_.live_tv ? 2 : std::clamp(opts_.read_ahead_seconds,30,300))) && (audio_stream_<0 || aq_.duration()>(opts_.live_tv ? 2 : std::clamp(opts_.read_ahead_seconds,30,300)))) || eof_) {
            delay_ms(10); continue;
        }

		demux_reading_ = true;
		r = av_read_frame(fmt_, pkt);
		demux_reading_ = false;
		if (seek_pending_) { av_packet_unref(pkt); continue; }
		if (r < 0) {
			if (abort_) break;
			// The download gave up (not the end of the video): say so,
			// rather than ending playback as if the video were over.
			std::string why;
			if (NetStream* ns = main_pb_ ? NetStream::of(main_pb_) : nullptr) why = ns->failure();
			if (TorrentStream* ts = main_pb_ ? TorrentStream::of(main_pb_) : nullptr) why = ts->failure();
			if (!why.empty()) {
				fail("The stream stopped (" + why + ")");
				break;
			}
			bool at_end = r == AVERROR_EOF || (fmt_->pb && avio_feof(fmt_->pb));
			if (!at_end && ++errors < 60) {
				delay_ms(100);
				continue;
			}
			if (!at_end) dlog("player: read error %s, treating as end", av_err(r).c_str());
			eof_ = true;
			vq_.set_eof(true);
			aq_.set_eof(true);
			continue;
		}
		errors = 0;
		AVStream* st = fmt_->streams[pkt->stream_index];
		double tb = av_q2d(st->time_base);
		double pts = to_sec(pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts, tb);
		double dur = pkt->duration > 0 ? pkt->duration * tb : 0;
		if (pkt->stream_index == vs) {
			AVPacket* q = av_packet_alloc();
			av_packet_move_ref(q, pkt);
			packet_storage_->store(q);
			vq_.put(q, pts, dur);
		} else if (pkt->stream_index == audio_stream_) {
			AVPacket* q = av_packet_alloc();
			av_packet_move_ref(q, pkt);
			packet_storage_->store(q);
			aq_.put(q, pts, dur);
		} else {
			if (sctx_.count(pkt->stream_index)) handle_subtitle_packet(pkt);
			av_packet_unref(pkt);
		}
	}
	av_packet_free(&pkt);

	vq_.abort();
	aq_.abort();
	fq_cv_.notify_all();
	if (video_.joinable()) video_.join();
	if (audio_.joinable()) audio_.join();

	avcodec_free_context(&vctx_);
	evo_.reset();
	hw_.reset();
	hw_active_ = false;
	{
		std::lock_guard<std::mutex> lock(actx_m_);
		avcodec_free_context(&actx_);
	}
	for (auto& kv : sctx_) avcodec_free_context(&kv.second);
	sctx_.clear();
	if (sws_) {
		sws_freeContext(sws_);
		sws_ = nullptr;
	}
	if (swr_) swr_free(&swr_);
	avformat_close_input(&fmt_);
	video_stream_ = audio_stream_ = -1;
}

void Player::do_seek(double target, bool switching_audio) {
	const bool short_refill = state_ == State::Playing && !recovering_;
	// Promote a quick Matroska open to a seekable input on its demux thread.
	// FFmpeg can load deferred cues here without discarding the decoders/tracks.
	const bool was_quick = quick_.exchange(false);
	if (was_quick && main_pb_) main_pb_->seekable = AVIO_SEEKABLE_NORMAL;
	if (fmt_->pb) { fmt_->pb->error = 0; fmt_->pb->eof_reached = 0; }
	int64_t ts = int64_t((target + start_offset_) * AV_TIME_BASE);
	// A playlist may begin its video a few milliseconds after its audio.
	// HLS refuses timestamps before its first video packet, even for target 0.
	const int index = video_stream_ >= 0 ? video_stream_.load() : audio_stream_.load();
	if (index >= 0 && fmt_->streams[index]->start_time != AV_NOPTS_VALUE)
		ts = std::max(ts, av_rescale_q(fmt_->streams[index]->start_time,
		    fmt_->streams[index]->time_base, AV_TIME_BASE_Q));
	int r = avformat_seek_file(fmt_, -1, INT64_MIN, ts, ts, 0);
	if (r < 0) r = av_seek_frame(fmt_, -1, ts, AVSEEK_FLAG_BACKWARD);
	if (r < 0) {
		dlog("player: seek to %.1f failed: %s", target, av_err(r).c_str());
		if (seek_pending_) return; // a newer request superseded this attempt
		if (was_quick && !recovering_) {
			// Some Matroska inputs cannot load their index in place. The UI
			// performs the old reopen fallback; never join this demux thread here.
			reopen_target_ = target;
			return;
		}
		if(recovering_.exchange(false)) { fail("Decoder recovery could not seek this source. Choose another stream."); return; }
        if (switching_audio) { fail("Could not resynchronize the selected audio track. Restart playback or choose another stream."); return; }
		vdrop_ = adrop_ = -1; seek_refill_ = false; buffering_ = false; apply_run_state(); return;
	}
	subtitle_renderer_.seek();
	{ std::lock_guard<std::mutex> lock(sub_m_); embedded_.clear(); }
	{
		std::lock_guard<std::mutex> lock(fq_m_);
		vdrop_ = video_stream_ >= 0 ? target : -1;
		vq_.flush();
		for (auto& f : frames_) pool_.push_back(std::move(f));
		frames_.clear();
	}
	fq_cv_.notify_all();
	aq_.flush();
	for (auto& entry : sctx_) avcodec_flush_buffers(entry.second);
	eof_ = false;
	video_drained_ = audio_drained_ = false;
	adrop_ = target;
	{
		std::lock_guard<std::mutex> lock(clock_m_);
		audio_base_pts_ = audio_end_pts_ = -1;
		wall_base_pts_ = target;
		wall_base_time_ = now_seconds();
		pcm_.clear();
	}
	recovering_=false;
	seek_refill_ = short_refill;
	buffering_ = true;
	buffer_percent_ = 0;
	apply_run_state();
}

void Player::handle_subtitle_packet(AVPacket* pkt) {
	AVCodecContext* ctx = sctx_[pkt->stream_index];
	AVSubtitle sub;
	int got = 0;
	if (avcodec_decode_subtitle2(ctx, &sub, &got, pkt) < 0 || !got) return;
	double tb = av_q2d(fmt_->streams[pkt->stream_index]->time_base);
	double pts = to_sec(pkt->pts, tb);
	if (pts < 0) {
		avsubtitle_free(&sub);
		return;
	}
	double start = pts + sub.start_display_time / 1000.0;
	double end = sub.end_display_time > sub.start_display_time ? pts + sub.end_display_time / 1000.0
	             : pkt->duration > 0                         ? pts + pkt->duration * tb
	                                                         : start + 4;
	subtitle_renderer_.packet(pkt->stream_index,pkt,pts,end-start,&sub);
	std::string text;
	for (unsigned i = 0; i < sub.num_rects; i++) {
		AVSubtitleRect* rect = sub.rects[i];
		std::string t;
		if (rect->ass) {
			// "ReadOrder,Layer,Style,Name,MarginL,MarginR,MarginV,Effect,Text"
			const char* p = rect->ass;
			for (int commas = 0; *p && commas < 8; p++)
				if (*p == ',') commas++;
			t = p;
		} else if (rect->text) {
			t = rect->text;
		}
		if (t.empty()) continue;
		if (!text.empty()) text += "\\N";
		text += t;
	}
	avsubtitle_free(&sub);
	if (text.empty()) return;
	Cue c;
	c.start = start;
	c.end = end;
	c.rml = subtitle_text_to_rml(text);
	std::lock_guard<std::mutex> lock(sub_m_);
	auto& cues = embedded_[pkt->stream_index];
	auto it = std::lower_bound(cues.begin(), cues.end(), c.start, [](const Cue& a, double v) { return a.start < v; });
	for (auto j = it; j != cues.end() && j->start == c.start; ++j)
		if (j->rml == c.rml) return;  // seen before (seeking back re-reads packets)
	cues.insert(it, c);
	if(cues.size()>20000) cues.erase(cues.begin(),cues.begin()+10000);
}

// ---------------------------------------------------------------------------
// Video

void Player::video_thread() {
	if (evo_) { video_thread_evo(); return; }
	if (hw_ && video_thread_hw()) return;
	AVPacket* pkt = nullptr;
	AVFrame* frame = av_frame_alloc();
	int serial = vq_.serial();
	bool sent_eof = false;
	double tb = av_q2d(fmt_->streams[video_stream_]->time_base);

	while (!abort_) {
		int s = 0;
		int r = vq_.get(&pkt, &s, 50);
		if (abort_) break;
		if (r == 0) continue;
		if (recovering_) { av_packet_free(&pkt); delay_ms(1); continue; }
		if (r == 1 && s != serial) {
			avcodec_flush_buffers(vctx_);
			serial = s;
			sent_eof = false;
			video_drained_ = false;
		}
		if (r == -1) {
			if (sent_eof) {
				delay_ms(10);
				if (vq_.serial() != serial) {
					avcodec_flush_buffers(vctx_);
					serial = vq_.serial();
					sent_eof = false;
					video_drained_ = false;
				}
				continue;
			}
			avcodec_send_packet(vctx_, nullptr);  // drain
			sent_eof = true;
		} else {
			int sr = avcodec_send_packet(vctx_, pkt);
			if (sr < 0 && sr != AVERROR(EAGAIN)) dlog("player: video send: %s", av_err(sr).c_str());
			av_packet_free(&pkt);
		}

		while (!abort_) {
			int rr = avcodec_receive_frame(vctx_, frame);
			if (rr == AVERROR_EOF) {
				video_drained_ = true;
				break;
			}
			if (rr < 0) break;
			decoded_++;
			double pts = to_sec(frame->best_effort_timestamp, tb);
			double sar = frame->sample_aspect_ratio.num > 0 ? av_q2d(frame->sample_aspect_ratio) : 1.0;
			bool ok = emit_frame(pts, serial, [&](Frame& f) {
				// Keep common 4:2:0 formats intact for native-resolution GPU
				// conversion, including software-decoded HDR and P010.
				if (!opts_.preview && (frame->format == AV_PIX_FMT_YUV420P10LE || frame->format == AV_PIX_FMT_YUV420P ||
				    frame->format == AV_PIX_FMT_NV12 || frame->format == AV_PIX_FMT_P010LE)) {
					YuvPicture yp;
					yp.y = frame->data[0];
					yp.u = frame->data[1];
					yp.v = frame->data[2];
					yp.y_stride = frame->linesize[0];
					yp.c_stride = frame->linesize[1];
					yp.v_stride = frame->linesize[2];
					yp.wide = frame->format == AV_PIX_FMT_YUV420P10LE || frame->format == AV_PIX_FMT_P010LE;
					yp.interleaved = frame->format == AV_PIX_FMT_NV12 || frame->format == AV_PIX_FMT_P010LE;
					yp.shift = frame->format == AV_PIX_FMT_P010LE ? 6 : 0;
					yp.width = frame->width;
					yp.height = frame->height;
					if (frame->color_range != AVCOL_RANGE_UNSPECIFIED)
						f.full_range = frame->color_range == AVCOL_RANGE_JPEG;
					return pack_video_yuv(yp, f);
				}
				sws_ = sws_getCachedContext(sws_, frame->width, frame->height, AVPixelFormat(frame->format), f.w, f.h,
				                            AV_PIX_FMT_RGBA,
				                            (f.w == frame->width && f.h == frame->height) ? SWS_POINT : SWS_BILINEAR,
				                            nullptr, nullptr, nullptr);
				if (!sws_) return false;
				f.format = VideoFormat::Rgba;
				f.pixels.resize(size_t(f.w) * f.h * 4);
				uint8_t* dst[4] = {f.pixels.data(), nullptr, nullptr, nullptr};
				int dst_stride[4] = {f.w * 4, 0, 0, 0};
				return sws_scale(sws_, frame->data, frame->linesize, 0, frame->height, dst, dst_stride) == f.h;
			}, frame->width, frame->height, sar);
			av_frame_unref(frame);
			if (!ok) break;
		}
	}
	if (pkt) av_packet_free(&pkt);
	av_frame_free(&frame);
}

void Player::video_thread_evo() {
	AVStream* st = fmt_->streams[video_stream_];
	const double tb = av_q2d(st->time_base);
	const double sar = st->codecpar->sample_aspect_ratio.num > 0 ? av_q2d(st->codecpar->sample_aspect_ratio) : 1.0;
	int serial = vq_.serial();
	bool sent_eof = false, failed = false;
	double next_pts = opts_.start;
	auto drain = [&]() {
		int produced = 0;
		while (!abort_ && vq_.serial() == serial) {
			pp_frame frame{};
			int rr = evo_->receive(frame);
			if (rr < 0) { failed = true; return -1; }
			if (!rr) return produced;
			decoded_++; produced++;
			double pts = frame.pts_us == INT64_MIN ? next_pts : double(frame.pts_us) / 1e6;
			next_pts = pts + (fps_ > 0 ? 1.0 / fps_ : 1.0 / 24);
			if (!emit_frame(pts, serial, [&](Frame& out) { return evo_->pack(frame, out); },
			                frame.width, frame.height, sar)) return -1;
		}
		return -1;
	};
	while (!abort_ && !failed) {
		AVPacket* pkt = nullptr;
		int packet_serial = 0;
		int r = vq_.get(&pkt, &packet_serial, 50);
		if (abort_) { av_packet_free(&pkt); break; }
		if (!r) continue;
		int current_serial = r == 1 ? packet_serial : vq_.serial();
		if (current_serial != serial) {
			evo_->flush(); serial = current_serial; sent_eof = false; video_drained_ = false;
			next_pts = std::max(0.0, vdrop_.load());
		}
		if (r == -1 && sent_eof) { delay_ms(10); continue; }
		int64_t pts = r == 1 && pkt->pts != AV_NOPTS_VALUE
		    ? int64_t(to_sec(pkt->pts, tb) * 1e6) : INT64_MIN;
		while (!abort_ && vq_.serial() == serial) {
			int sr = evo_->send(pkt ? pkt->data : nullptr, pkt ? pkt->size : 0, pts);
			if (sr < 0) { failed = true; break; }
			if (!sr) break;
			int received = drain();
			if (received <= 0) { if (!received) failed = true; break; }
		}
		av_packet_free(&pkt);
		if (!failed) drain();
		if (r == -1 && vq_.serial() == serial) { sent_eof = true; video_drained_ = true; }
	}
	if(failed && !abort_) {
		evo_.reset();
		if(recover_video()) video_thread();
	}
}

// A decoded picture into the frame queue, copied at its native size by `fill`.
// Skips pictures before a seek target or hopelessly late ones. False when
// playback was closed or seeked meanwhile.
bool Player::emit_frame(double pts, int serial, const std::function<bool(Frame&)>& fill, int src_w, int src_h,
                        double sar) {
    if(opts_.preview && (src_w>1920 || src_h>1088)) {fail("Four-screen channel switched above 1080p. Choose an SD/HD channel.");return false;}
	if (!video_dimensions_valid(src_w, src_h)) {
		fail("Unsupported video dimensions (maximum 4096 pixels per side)");
		return false;
	}
	double drop = vdrop_;
	if (drop >= 0) {
		if (pts >= 0 && pts < drop - 0.02) return true;
		// Release the seek gate only after the target picture is queued.
	}
	// Hopelessly late and something else is ready: skip the conversion.
	// (Never hold clock_m_ while taking fq_m_: present() nests them the other way.)
	{
		double clk;
		{
			std::lock_guard<std::mutex> cl(clock_m_);
			clk = clock_locked() + audio_delay_.load();
		}
		std::lock_guard<std::mutex> fl(fq_m_);
		if (!buffering_ && !frames_.empty() && pts >= 0 && pts < clk - 0.2) {
			dropped_++;
			return true;
		}
	}

	// Wait for room in the frame queue.
	std::unique_lock<std::mutex> lock(fq_m_);
	fq_cv_.wait_for(lock, std::chrono::milliseconds(500), [&] {
		return abort_ || int(frames_.size()) < kMaxFrames || vq_.serial() != serial;
	});
	while (!abort_ && int(frames_.size()) >= kMaxFrames && vq_.serial() == serial)
		fq_cv_.wait_for(lock, std::chrono::milliseconds(50));
	if (abort_ || vq_.serial() != serial) return false;
	Frame f;
	if (!pool_.empty()) {
		f = std::move(pool_.back());
		pool_.pop_back();
	}
	lock.unlock();

	f.w = src_w; f.h = src_h;
    if(opts_.preview) {
        const double scale=std::min(1.0,std::min(640.0/src_w,360.0/src_h));
        f.w=std::max(2,int(src_w*scale)/2*2);f.h=std::max(2,int(src_h*scale)/2*2);
    }
	f.format = VideoFormat::Rgba; f.shift = 0;
	f.colors = colors_; f.full_range = full_range_;
	f.hdr10_compatible = hdr10_compatible_;
	f.hdr_peak_nits = hdr_peak_nits_;
	f.sar = std::isfinite(sar) && sar > 0 ? sar : 1.0;
	if (!fill(f)) {
		fail("Cannot prepare the decoded video picture");
		return false;
	}
	f.pts = pts;
	f.serial = serial;

	lock.lock();
	if (abort_ || vq_.serial() != serial) return false;
	frames_.push_back(std::move(f));
	if (drop >= 0 && vq_.serial() == serial) vdrop_ = -1;
	return true;
}

// ---------------------------------------------------------------------------
// Hardware decoding

// A hardware picture: two planes (Y, then interleaved UV), 8 or 16-bit samples.
bool Player::pack_hardware(const HwDecoder::Picture& pic, Frame& f) {
	YuvPicture yp;
	yp.y = pic.y;
	yp.u = pic.uv;
	yp.y_stride = yp.c_stride = pic.pitch;
	yp.interleaved = true;
	yp.wide = pic.ten_bit;
	yp.width = pic.width;
	yp.height = pic.height;
	if (pic.ten_bit && hw_shift_ < 0) {
		hw_shift_ = video_sample_shift(yp);
		dlog("player: 10-bit pictures, samples in the %s bits", hw_shift_ ? "high" : "low");
	}
	yp.shift = pic.ten_bit ? hw_shift_ : 0;
	return pack_video_yuv(yp, f);
}

// The video thread while the hardware decoder plays. Returns true when
// playback ended or closed; false when the decoder failed and the software
// decoder should take over from here.
bool Player::video_thread_hw() {
	AVPacket* pkt = nullptr;
	int serial = vq_.serial();
	bool sent_eof = false;
	AVStream* st = fmt_->streams[video_stream_];
	double tb = av_q2d(st->time_base);
	double sar = st->codecpar->sample_aspect_ratio.num > 0 ? av_q2d(st->codecpar->sample_aspect_ratio) : 1.0;
	bool failed = false;

	auto drain = [&]() -> bool {  // decoded pictures out; false if playback moved on
		HwDecoder::Picture pic;
		for (;;) {
			int rr = hw_->receive(&pic);
			if (rr < 0) {
				failed = true;
				return true;
			}
			if (rr == 0) return true;
			decoded_++;
			double pts = pic.pts_us == INT64_MIN ? -1 : double(pic.pts_us) / 1e6;
			if (!emit_frame(pts, serial, [&](Frame& f) { return pack_hardware(pic, f); }, pic.width, pic.height, sar))
				return false;
		}
	};

	while (!abort_ && !failed) {
		int s = 0;
		int r = vq_.get(&pkt, &s, 50);
		if (abort_) break;
		if (r == 0) continue;
		if (r == 1 && s != serial) {
			hw_->flush();
			serial = s;
			sent_eof = false;
			video_drained_ = false;
		}
		if (r == -1) {
			if (sent_eof) {
				delay_ms(10);
				if (vq_.serial() != serial) {
					hw_->flush();
					serial = vq_.serial();
					sent_eof = false;
					video_drained_ = false;
				}
				continue;
			}
			hw_->send(nullptr, 0, 0);
			sent_eof = true;
			drain();
			video_drained_ = true;
			continue;
		}
		double ps = pkt->pts != AV_NOPTS_VALUE ? to_sec(pkt->pts, tb) : -1;
		int64_t pts_us = pkt->pts != AV_NOPTS_VALUE ? int64_t(ps * 1e6) : INT64_MIN;
		for (;;) {
			int sr = hw_->send(pkt->data, pkt->size, pts_us);
			if (sr == 1) {  // pictures waiting: take them first
				if (!drain()) break;
				continue;
			}
			if (sr < 0) failed = true;
			break;
		}
		av_packet_free(&pkt);
		if (!failed) drain();
	}
	if (pkt) av_packet_free(&pkt);
	if (!failed) return true;

    hw_.reset();
    if(!recover_video()) return true;
	return false;
}

bool Player::recover_video() {
    double target;
    { std::lock_guard<std::mutex> lock(clock_m_); target=clock_locked(); }
    hw_active_=false;
    if(!open_stream_codec(video_stream_,&vctx_)) { fail("This video cannot be decoded. Choose another stream."); return false; }
    buffering_=true; pcm_.pause(true); recovering_=true; vdrop_=std::max(0.0,target);
    {
        std::lock_guard<std::mutex> lock(seek_m_);
        // Preserve a newer user seek instead of recovering to an old position.
        if(!seek_req_) { seek_req_=true; seek_target_=std::max(0.0,target); }
    }
    dlog("player: decoder recovery to software at %.3f s; rebuffering audio and video together",target);
    return true;
}

// ---------------------------------------------------------------------------
// Audio

double Player::queued_audio_seconds() const {
	if (!pcm_.is_open()) return 0;
	return pcm_.queued_seconds();
}

void Player::open_audio_output(int stream, bool pcm_only) {
    pcm_only=pcm_only || gain_percent_.load()!=100;
    const auto* p=fmt_->streams[stream]->codecpar;
    const int kind=bitstream::kind(p);
    // HDMI EDID cannot describe a TV's downstream optical connection. Honor
    // the user's stereo choice rather than sending compressed surround there.
    if (!pcm_only && opts_.audio_mode=="auto" && !opts_.surround) {
        pcm_only=true;
        dlog("audio: Auto honors stereo output; using PCM instead of HDMI passthrough");
    }
    if(!pcm_only && opts_.audio_mode!="pcm" && kind && pcm_.open_bitstream(kind,opts_.audio_mode!="bitstream")) return;
    if(!pcm_only && opts_.audio_mode!="pcm") dlog("audio: %s uses PCM fallback",avcodec_get_name(p->codec_id));
    if(!pcm_.open(opts_.surround)) dlog("player: can't open the audio output");
}
void Player::audio_thread() {
	AVPacket* pkt = nullptr;
	AVFrame* frame = av_frame_alloc();
	int serial = aq_.serial();
	bool sent_eof = false;
    bitstream::Packer packer;
    int packed_serial=-1; double burst_pts=-1;
	std::vector<uint8_t> out;
	int swr_rate = 0, swr_fmt = -1;
	PcmLevels levels;
	AVChannelLayout swr_layout;
	memset(&swr_layout, 0, sizeof(swr_layout));

	while (!abort_) {
		int s = 0;
		int r = aq_.get(&pkt, &s, 50);
		if (abort_) break;
		if (r == 0 && !(pcm_.bitstream_kind() && pcm_.failed())) continue;
		std::unique_lock<std::mutex> alock(actx_m_);
        // A packet may have been popped just before the demux thread changed
        // tracks. Never submit an old-track packet to the replacement decoder.
        if (r == 1 && s != aq_.serial()) {
            av_packet_free(&pkt);
            continue;
        }
		if (!actx_) {
			if (r == 1) av_packet_free(&pkt);
			continue;
		}
		if (r == 1 && s != serial) {
			avcodec_flush_buffers(actx_);
			serial = s;
			sent_eof = false;
			audio_drained_ = false;
			if (swr_) swr_free(&swr_);
		}
        if(pcm_.bitstream_kind()) {
            bool ok=!pcm_.failed();
            if(ok && r==1 && packed_serial!=serial) {ok=packer.start(fmt_->streams[audio_stream_]->codecpar);packed_serial=serial;burst_pts=-1;}
            const double tb=av_q2d(fmt_->streams[audio_stream_]->time_base);
            const double pts=r==1?to_sec(pkt->pts,tb):-1;
            if(ok && r==1) {
                if(burst_pts<0) burst_pts=pts;
                ok=packer.pack(pkt,out);
                if(ok && !out.empty()) {
                    const double duration=double(out.size())/(bitstream::rate(pcm_.bitstream_kind())*4.0);
                    const double drop=adrop_;
                    if(drop>=0 && burst_pts>=0 && burst_pts<drop) {
                        // A compressed frame cannot be sample-trimmed. Skip the
                        // boundary frame and start with the first whole burst.
                        burst_pts=-1;av_packet_free(&pkt);continue;
                    }
                    alock.unlock();
                    while(!abort_ && aq_.serial()==serial && !pcm_.failed() && queued_audio_seconds()>kMaxQueuedAudio) delay_ms(5);
                    alock.lock();
                    if(abort_ || aq_.serial()!=serial) {av_packet_free(&pkt);continue;}
                    std::lock_guard<std::mutex> clock(clock_m_);
                    if(pcm_.failed()) ok=false;
                    else if(aq_.serial()==serial) {
                        adrop_=-1;
                        const double start=burst_pts>=0?burst_pts:std::max(0.0,audio_end_pts_);
                        if(audio_base_pts_<0)audio_base_pts_=start;
                        ok=pcm_.queue(out.data(),out.size());audio_end_pts_=start+duration;
                    }
                    burst_pts=-1;
                }
            }
            if(!ok) {
                double target;
                { std::lock_guard<std::mutex> clock(clock_m_);
                  target=clock_locked();pcm_.close(false);open_audio_output(audio_stream_,true);
                  audio_base_pts_=audio_end_pts_=-1;wall_base_pts_=target;wall_base_time_=now_seconds(); }
                if(!pcm_.is_open()) {
                    fail("The audio output could not be released or restored safely. Close and reopen the app before playing again.");
                    break;
                }
                dlog("audio: bitstream failed; restoring PCM and rebuffering at %.3f s",target);
                buffering_=true;pcm_.pause(true);packer.clear();packed_serial=-1;
                {std::lock_guard<std::mutex> seek(seek_m_);seek_target_=target;seek_req_=true;}
            }
            if(r==1)av_packet_free(&pkt);else if(r==-1) {if(ok)audio_drained_=true;alock.unlock();delay_ms(10);}
            continue;
        }
        packer.clear();packed_serial=-1;burst_pts=-1;
		if (r == -1) {
			if (sent_eof) {
				alock.unlock();
				delay_ms(10);
				continue;
			}
			avcodec_send_packet(actx_, nullptr);
			sent_eof = true;
		} else {
			avcodec_send_packet(actx_, pkt);
			av_packet_free(&pkt);
		}
		double tb = av_q2d(fmt_->streams[audio_stream_]->time_base);

		while (!abort_) {
			int rr = avcodec_receive_frame(actx_, frame);
			if (rr == AVERROR_EOF) {
				audio_drained_ = true;
				break;
			}
			if (rr < 0) break;
			double pts = to_sec(frame->best_effort_timestamp, tb);
			double drop = adrop_;
			if (drop >= 0) {
				if (pts >= 0 && pts + double(frame->nb_samples) / frame->sample_rate < drop) {
					av_frame_unref(frame);
					continue;
				}
				adrop_ = -1;
			}

			AVChannelLayout in_layout;
			if (frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC || frame->ch_layout.nb_channels == 0)
				av_channel_layout_default(&in_layout, frame->ch_layout.nb_channels ? frame->ch_layout.nb_channels : 2);
			else
				av_channel_layout_copy(&in_layout, &frame->ch_layout);
			if (!swr_ || swr_rate != frame->sample_rate || swr_fmt != frame->format ||
			    av_channel_layout_compare(&swr_layout, &in_layout) != 0) {
				if (swr_) swr_free(&swr_);
				AVChannelLayout stereo;
				av_channel_layout_default(&stereo, pcm_.channels());
				swr_alloc_set_opts2(&swr_, &stereo, pcm_.floating_point() ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16, kOutRate, &in_layout,
				                    AVSampleFormat(frame->format), frame->sample_rate, 0, nullptr);
				if (!swr_ || !configure_pcm_mix(swr_) || swr_init(swr_) < 0) {
					dlog("player: can't resample audio");
					if (swr_) swr_free(&swr_);
					av_channel_layout_uninit(&in_layout);
					av_frame_unref(frame);
					continue;
				}
				swr_rate = frame->sample_rate;
				swr_fmt = frame->format;
				char layout[128]{};
				av_channel_layout_describe(&in_layout, layout, sizeof(layout));
				dlog("audio: PCM conversion input=%s %d Hz %s output=%d channels %d Hz %s; explicit normalized downmix",
				     layout, frame->sample_rate, av_get_sample_fmt_name(AVSampleFormat(frame->format)), pcm_.channels(), kOutRate,
				     pcm_.floating_point() ? "float" : "S16");
				levels = {};
				av_channel_layout_uninit(&swr_layout);
				av_channel_layout_copy(&swr_layout, &in_layout);
			}
			av_channel_layout_uninit(&in_layout);

			int max_out = swr_get_out_samples(swr_, frame->nb_samples) + 32;
			out.resize(size_t(max_out) * pcm_.bytes_per_frame());
			uint8_t* outp[1] = {out.data()};
			int n = swr_convert(swr_, outp, max_out, (const uint8_t**)frame->extended_data, frame->nb_samples);
			av_frame_unref(frame);
			if (n <= 0) continue;

			// Keep the output queue short so the clock (and seeking) stay responsive.
			alock.unlock();
			while (!abort_ && aq_.serial() == serial && queued_audio_seconds() > kMaxQueuedAudio) delay_ms(5);
			alock.lock();
			if (abort_ || aq_.serial() != serial || !actx_) break;

			std::lock_guard<std::mutex> lock(clock_m_);
			if (aq_.serial() != serial) break;
			int trim = 0;
			if (drop >= 0 && pts >= 0 && pts < drop) trim = std::min(n, int(std::ceil((drop - pts) * kOutRate)));
			if (trim == n) continue;
			levels.add(out.data() + size_t(trim)*pcm_.bytes_per_frame(), size_t(n-trim)*pcm_.channels(), pcm_.floating_point());
			if (levels.samples >= size_t(kOutRate)*pcm_.channels()*5) {
				dlog("audio: PCM levels peak=%.1f dBFS rms=%.1f dBFS queued=%.1f ms", levels.peak_db(), levels.rms_db(), pcm_.queued_seconds()*1000);
				levels = {};
			}
			const double first_pts = pts >= 0 ? pts + double(trim) / kOutRate : std::max(0.0, audio_end_pts_);
			if (audio_base_pts_ < 0) audio_base_pts_ = first_pts;
			if (!pcm_.queue(out.data() + size_t(trim) * pcm_.bytes_per_frame(), size_t(n-trim) * pcm_.bytes_per_frame())) {
				fail("Audio buffer exceeded its limit"); break;
			}
			double end = (pts >= 0 ? pts : std::max(0.0, audio_end_pts_)) + double(n) / kOutRate;
			if (pts < 0 && audio_end_pts_ >= 0) end = audio_end_pts_ + double(n) / kOutRate;
			audio_end_pts_ = end;
		}
	}
	av_channel_layout_uninit(&swr_layout);
	if (pkt) av_packet_free(&pkt);
	av_frame_free(&frame);
}

// ---------------------------------------------------------------------------
// Clock, buffering, presentation

double Player::clock_locked() {
	if (audio_stream_ >= 0 && pcm_.is_open()) {
		if (audio_end_pts_ < 0) return wall_base_pts_;
		return audio_base_pts_ < 0 ? wall_base_pts_ : std::min(audio_end_pts_,audio_base_pts_ + pcm_.played_seconds());
	}
	if (!wall_running_) return wall_base_pts_;
	return wall_base_pts_ + (now_seconds() - wall_base_time_);
}

void Player::set_clock_running(bool running) {
	std::lock_guard<std::mutex> lock(clock_m_);
	if (running == wall_running_) return;
	if (running) {
		wall_base_time_ = now_seconds();
	} else {
		wall_base_pts_ = wall_base_pts_ + (now_seconds() - wall_base_time_);
	}
	wall_running_ = running;
}

void Player::apply_run_state() {
	bool run = !paused_ && !buffering_ && !seek_pending_ && reopen_target_ < 0 && vdrop_ < 0 && state_ == State::Playing;
	pcm_.pause(!run);
	set_clock_running(run);
}

void Player::set_paused(bool p) {
	if(vlc_) {vlc_->set_paused(p);return;}
	paused_ = p;
	apply_run_state();
}

bool Player::buffering(int* percent) const {
	if(vlc_) return vlc_->buffering(percent);
	if (percent) *percent = buffer_percent_;
	return state_ == State::Opening || (state_ == State::Playing && buffering_);
}

void Player::update_buffering() {
	if (state_ != State::Playing || recovering_ || seek_pending_ || reopen_target_ >= 0) return;
    if(AudioPort::shutdown_failed) {
        fail("The audio output could not be released safely. Close and reopen the app before playing again.");return;
    }
    bool pcm_failed;
    {
        // The audio worker changes kind/failed together while holding this lock
        // when it restores PCM. Do not mistake its closed-port intermediate
        // state for a failed PCM output and terminate otherwise recoverable playback.
        std::lock_guard<std::mutex> clock(clock_m_);
        pcm_failed=pcm_.failed() && !pcm_.bitstream_kind();
    }
    if(pcm_failed) { fail("The PS5 audio output stopped. Try stereo audio output in Settings."); return; }
	bool has_v = video_stream_ >= 0, has_a = audio_stream_ >= 0 && pcm_.is_open();
	if (buffering_) {
		double target = seek_refill_ ? 0.5 : opts_.live_tv ? 2.0 : std::max(kBufferTargets[stalls_], start_target_);
		double vbuf = has_v ? vq_.duration() : 1e9, abuf = has_a ? aq_.duration() : 1e9;
		double buf = std::min(vbuf, abuf);
		buffer_percent_ = int(std::min(100.0, buf / target * 100));
		bool have_frame;
		{
			std::lock_guard<std::mutex> lock(fq_m_);
			have_frame = !has_v || !frames_.empty() || displayed_serial_ == vq_.serial();
		}
		// Also go on when memory is nearly full (a very high bitrate).
		if (have_frame && (!seek_refill_ || !has_v || displayed_serial_ == vq_.serial()) && vdrop_ < 0 && (!has_a || queued_audio_seconds()>0 || audio_drained_) &&
		    (eof_ || buf >= target || vq_.bytes() + aq_.bytes() > kMaxQueueBytes * 9 / 10)) {
			if (stalls_ > 0) dlog("player: buffered %.1f s, playing again", buf);
			buffering_ = false;
			seek_refill_ = false;
			apply_run_state();
		}
		return;
	}
	if (eof_) return;
	bool starving = false;
	if (has_v) {
		std::lock_guard<std::mutex> lock(fq_m_);
		if (frames_.empty() && vq_.count() == 0 && !video_drained_) starving = true;
	}
	if (has_a && aq_.count() == 0 && queued_audio_seconds() < 0.05 && !audio_drained_) starving = true;
	if (starving) {
		// The download fell behind: wait longer this time.
		const int last = int(sizeof(kBufferTargets) / sizeof(kBufferTargets[0])) - 1;
		if (stalls_ < last) stalls_++;
		dlog("player: the stream fell behind (stop %d); buffering %.0f s before going on", ++stall_count_,
		     opts_.live_tv ? 2.0 : std::max(kBufferTargets[stalls_], start_target_));
		buffering_ = true;
		buffer_percent_ = 0;
		apply_run_state();
	}
}

double Player::position() {
	if(vlc_) return vlc_->position();
	{
		std::lock_guard<std::mutex> lock(seek_m_);
		if (seek_req_) return seek_target_;
	}
	double drop = vdrop_;
	if (drop >= 0) return drop;
	std::lock_guard<std::mutex> lock(clock_m_);
	return clock_locked();
}

void Player::seek(double t) {
	if(vlc_) {vlc_->seek(t);return;}
	if (duration_ > 0) t = std::min(t, std::max(0.0, duration_ - 2));
	t = std::max(0.0, t);
	if (state_ != State::Playing && state_ != State::Ended) return;
	seek_started_at_ = now_seconds(); seek_first_picture_ms_ = -1;
	seek_log_target_ = t; seek_log_serial_ = vq_.serial();
	buffering_ = true; pcm_.pause(true);
	std::lock_guard<std::mutex> lock(seek_m_);
	seek_req_ = true;
	seek_target_ = t;
	seek_pending_ = true;
	video_lead_ = 0;
	if (state_ == State::Ended) state_ = State::Playing;
}

bool Player::set_audio_gain(int percent) {
    if(vlc_) {vlc_->set_audio_gain(percent);return true;}
    if(pcm_.bitstream_kind())return false;
    opts_.gain_percent=std::clamp(percent,50,300);gain_percent_=opts_.gain_percent;pcm_.set_gain(gain_percent_);return true;
}
void Player::set_audio_delay(double seconds) {
    seconds=std::isfinite(seconds)?std::clamp(seconds,-2.0,2.0):0;
    opts_.audio_delay=seconds;
    if(vlc_)vlc_->set_audio_delay(seconds);else audio_delay_=seconds;
}
const Player::Picture* Player::present() {
	if(vlc_) return vlc_->present();
	const double reopen = reopen_target_.exchange(-1);
	if (reopen >= 0 && !seek_pending_ && !abort_) {
		Options o = opts_; o.start = reopen; o.start_paused = paused_;
		o.initial_audio_stream = audio_stream_;
		dlog("player: seek reopen target=%.3f (index unavailable in place)", reopen);
		open(o);
		return nullptr;
	}
	const Picture* out = nullptr;
	if (state_ == State::Playing) update_buffering();
	if ((state_ == State::Playing || state_ == State::Ended) && !seek_pending_) {
		double clk;
		{
			std::lock_guard<std::mutex> lock(clock_m_);
			clk = clock_locked() + audio_delay_.load() + ((!paused_ && !buffering_) ? video_lead_ : 0);
		}
		std::lock_guard<std::mutex> lock(fq_m_);
		int cur = vq_.serial();
		while (!frames_.empty() && frames_.front().serial != cur) {
			pool_.push_back(std::move(frames_.front()));
			frames_.pop_front();
		}
		const bool same_timeline = have_picture_ && current_.serial == cur;
		int pick = -1;
		if (evo_presenter_) {
			if (cur != evo_presenter_serial_) { evo_presenter_->reset(); evo_presenter_serial_ = cur; }
			// EVO consumes frames in PTS order instead of jumping to the newest
			// queued frame on every UI tick. Its clock drops only genuinely late
			// frames; paused/buffering playback holds the owned current picture.
			while (!frames_.empty() && (!same_timeline || (!paused_ && !buffering_))) {
				Frame& f = frames_.front();
				if (same_timeline && f.pts > clk) break;
				int decision = evo_presenter_->publish(f.picture(), f.pts, clk);
				if (decision == 0) { pick = 0; break; }
				if (decision == 2) break;
				if (decision < 0) { fail("EVO could not present this video frame"); break; }
				dropped_++; heavy_window_dropped_++; heavy_window_decoded_++;
				pool_.push_back(std::move(frames_.front())); frames_.pop_front(); fq_cv_.notify_all();
			}
		} else {
			pick = select_video_frame(frames_, clk, same_timeline, paused_ || buffering_, fps_);
		}
		if (pick >= 0) {
			Frame& f = frames_[pick];
			have_picture_ = true;
			shown_++;
			// No audio: the first picture after a seek starts the clock.
			if (audio_stream_ < 0 && pick == 0 && !wall_running_) {
				std::lock_guard<std::mutex> cl(clock_m_);
				wall_base_pts_ = f.pts;
			}
			dropped_ += pick;
			heavy_window_dropped_ += pick;
			heavy_window_decoded_ += pick + 1;
			// The picked frame is the one on show now; the buffer it replaces goes
			// back to the pool with the frames that were skipped.
			std::swap(current_, f);
			for (int i = 0; i <= pick; i++) {
				pool_.push_back(std::move(frames_.front()));
				frames_.pop_front();
			}
			pic_ = current_.picture();
			displayed_serial_ = current_.serial;
			if (seek_started_at_ > 0 && current_.serial > seek_log_serial_ && seek_first_picture_ms_ < 0) {
				seek_first_picture_ms_ = (now_seconds() - seek_started_at_) * 1000;
				dlog("player: seek inline target=%.3f landed=%.3f first_picture_ms=%.1f paused=%d",
				     seek_log_target_, current_.pts, seek_first_picture_ms_, int(paused_.load()));
			}
			out = &pic_;
			fq_cv_.notify_all();
		}

		// Ended?
		if (state_ == State::Playing && eof_ && vq_.count() == 0 && aq_.count() == 0 &&
		    (video_stream_ < 0 || (video_drained_ && frames_.empty())) &&
		    (audio_stream_ < 0 || !pcm_.is_open() || (audio_drained_ && queued_audio_seconds() < 0.02)))
			state_ = State::Ended;
	}

	if (seek_started_at_ > 0 && seek_first_picture_ms_ >= 0 && !paused_ && !buffering_) {
		dlog("player: seek playing target=%.3f first_picture_ms=%.1f playing_ms=%.1f",
		     seek_log_target_, seek_first_picture_ms_, (now_seconds() - seek_started_at_) * 1000);
		seek_started_at_ = 0;
	}
	double now = now_seconds();
	if (now - heavy_window_start_ > 8) {
		if (!paused_ && !buffering_ && heavy_window_decoded_ > 60 && heavy_window_dropped_ * 4 > heavy_window_decoded_)
			too_heavy_ = true;
		heavy_window_start_ = now;
		heavy_window_dropped_ = heavy_window_decoded_ = 0;
	}
	return out;
}
// ---------------------------------------------------------------------------
// Tracks and stats

std::string Player::track_label(int stream) const {
	if (!fmt_ || stream < 0 || stream >= int(fmt_->nb_streams)) return "";
	AVStream* st = fmt_->streams[stream];
	AVDictionaryEntry* lang = av_dict_get(st->metadata, "language", nullptr, 0);
	AVDictionaryEntry* title = av_dict_get(st->metadata, "title", nullptr, 0);
	std::vector<std::string> parts;
	if (lang && strcmp(lang->value, "und") != 0) parts.push_back(language_name(lang->value));
	if (title && title->value[0]) parts.push_back(title->value);
	std::string codec = avcodec_get_name(st->codecpar->codec_id);
	for (auto& c : codec) c = char(toupper((unsigned char)c));
	if (codec == "SUBRIP") codec = "SRT";
	if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
		int ch = st->codecpar->ch_layout.nb_channels;
		std::string chs = ch == 1 ? "mono" : ch == 2 ? "stereo" : ch == 6 ? "5.1" : ch == 8 ? "7.1" : std::to_string(ch) + "ch";
		codec += " " + chs;
	}
	parts.push_back(codec);
	if (st->disposition & AV_DISPOSITION_FORCED) parts.push_back("forced");
	return join(parts, " · ");
}

std::vector<Player::Track> Player::audio_tracks() const {
	if(vlc_) {std::vector<Track> out;for(const auto& t:vlc_->tracks(false))out.push_back({t.stream,t.label,t.lang,t.is_default,false,false});return out;}
	std::vector<Track> out;
	if (!fmt_ || state_ != State::Playing) return out;
	for (unsigned i = 0; i < fmt_->nb_streams; i++) {
		AVStream* st = fmt_->streams[i];
		if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO || !playback_streams::compatible_audio(fmt_, video_stream_, int(i))) continue;
		AVDictionaryEntry* lang = av_dict_get(st->metadata, "language", nullptr, 0);
		out.push_back(Track{int(i), track_label(int(i)), lang ? language_to_iso639_2(lang->value) : ""});
	}
	return out;
}

std::vector<Player::Track> Player::subtitle_tracks() const {
	if(vlc_) {std::vector<Track> out;for(const auto& t:vlc_->tracks(true))out.push_back({t.stream,t.label,t.lang,t.is_default,false,false});return out;}
	std::vector<Track> out;
	if (!fmt_ || state_ != State::Playing) return out;
	for (auto& kv : sctx_) {
		AVStream* st = fmt_->streams[kv.first];
		AVDictionaryEntry* lang = av_dict_get(st->metadata, "language", nullptr, 0);
		out.push_back(Track{kv.first, track_label(kv.first), lang ? language_to_iso639_2(lang->value) : "",
            bool(st->disposition & AV_DISPOSITION_DEFAULT), bool(st->disposition & AV_DISPOSITION_FORCED),
            bool(st->disposition & AV_DISPOSITION_HEARING_IMPAIRED)});
	}
	return out;
}

void Player::select_audio(int stream) { if(vlc_) vlc_->select_audio(stream);else audio_switch_ = stream; }

std::string Player::embedded_subtitle(int stream, double t) {
	if(vlc_) return "";
	std::lock_guard<std::mutex> lock(sub_m_);
	auto it = embedded_.find(stream);
	if (it == embedded_.end()) return "";
	return cues_at(it->second, t);
}

std::string Player::codec_summary() const {
	if(vlc_) return vlc_->codec_summary();
	if (!fmt_ || state_ != State::Playing) return "";
	std::vector<std::string> parts;
	int vs = video_stream_, as = audio_stream_;
	if (vs >= 0) {
		AVCodecParameters* p = fmt_->streams[vs]->codecpar;
		std::string c = avcodec_get_name(p->codec_id);
		for (auto& ch : c) ch = char(toupper((unsigned char)ch));
		parts.push_back(c + " " + std::to_string(p->width) + "x" + std::to_string(p->height) +
		                (opts_.evo ? (hw_active_ ? " (EVO hardware)" : " (EVO software)")
		                           : (hw_active_ ? " (hardware)" : " (software)")));
	}
	if (as >= 0) parts.push_back(track_label(as));
	return join(parts, " · ");
}

void Player::log_stats() {
	if(vlc_) {vlc_->log_stats();return;}
	double now = now_seconds();
	if (state_ != State::Playing || paused_ || buffering_) {
		log_at_ = 0;
		return;
	}
	if (log_at_ == 0) {  // start counting
		log_at_ = now;
		log_shown_ = shown_;
		log_dropped_ = dropped_;
		return;
	}
	if (now - log_at_ < 30) return;
	double secs = now - log_at_;
	int shown = shown_ - log_shown_, dropped = dropped_ - log_dropped_;
	dlog("player: %s | shown %.1f fps (video %.3g fps), dropped %d in %.0f s, buffered %.1f s%s%s", codec_summary().c_str(),
	     shown / secs, fps_, dropped, secs, std::max(vq_.duration(), aq_.duration()), paused_ ? ", paused" : "",
	     buffering_ ? ", buffering" : "");
	if(packet_storage_) dlog("player: packet storage pooled=%llu fallback=%llu live=%zu bytes; audio %s",
		(unsigned long long)packet_storage_->pooled(),(unsigned long long)packet_storage_->fallbacks(),packet_storage_->live_bytes(),pcm_.description().c_str());
	log_at_ = now;
	log_shown_ = shown_;
	log_dropped_ = dropped_;
}

nlohmann::json Player::diagnostic_snapshot() {
	if(vlc_) return {{"engine","vlc"},{"paused",vlc_->paused()},{"buffering",vlc_->buffering()},{"positionSeconds",vlc_->position()},{"durationSeconds",vlc_->duration()}};
    const double now=now_seconds(); const int shown=shown_.load();
    nlohmann::json context={{"sourceFps",fps_},{"shownFrames",shown},{"decodedFrames",decoded_.load()},
            {"droppedFrames",dropped_.load()},{"bufferedSeconds",std::max(vq_.duration(),aq_.duration())},
            {"paused",paused_.load()},{"buffering",buffering_.load()},
            {"state",int(state_.load())},{"positionSeconds",position()},{"videoPacketSeconds",vq_.duration()},{"audioPacketSeconds",aq_.duration()},
            {"queuedAudioSeconds",queued_audio_seconds()},{"videoSerial",vq_.serial()},{"displayedVideoSerial",displayed_serial_.load()},
            {"videoDropUntil",vdrop_.load()},{"audioDropUntil",adrop_.load()},
            {"eof",eof_.load()},{"audioDrained",audio_drained_.load()},{"videoDrained",video_drained_.load()},
            {"seekPending",seek_pending_.load()},{"seekRefill",seek_refill_.load()},{"reopenTarget",reopen_target_.load()}};
    if(diagnostic_time_>0 && now-diagnostic_time_>.1) context["shownFps"]=std::max(0,shown-diagnostic_shown_)/(now-diagnostic_time_);
    diagnostic_time_=now; diagnostic_shown_=shown; return context;
}

std::string Player::stats() {
	if(vlc_) return vlc_->stats();
	if (state_ != State::Playing) return "";
	char buf[256];
	snprintf(buf, sizeof(buf), "%s · %.3g fps · buffered %.1fs · dropped %d of %d%s", codec_summary().c_str(), fps_,
	         std::max(vq_.duration(), aq_.duration()), dropped_.load(), decoded_.load(),
	         paused_ ? " · paused" : buffering_ ? " · buffering" : "");
	return buf;
}
