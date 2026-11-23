#include "torrent_stream.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

#include <cstdlib>
#include <charconv>
#include <limits>

#include "../util.h"
#include "engine.h"

namespace {

int read_cb(void* opaque, uint8_t* buf, int n) { return static_cast<TorrentStream*>(opaque)->read(buf, n); }
int64_t seek_cb(void* opaque, int64_t offset, int whence) {
	return static_cast<TorrentStream*>(opaque)->seek(offset, whence);
}

}  // namespace

bool TorrentStream::parse_url(const std::string& url, std::string* hash, int* idx) {
    if (!hash || !idx || !is_url(url)) return false;
    const std::string rest = url.substr(10);
    if (rest.size() < 42 || rest[40] != '/') return false;
    for (size_t i=0;i<40;++i)
        if (!((rest[i]>='0' && rest[i]<='9') || (rest[i]>='a' && rest[i]<='f') || (rest[i]>='A' && rest[i]<='F'))) return false;
    int file = -1;
    const auto result=std::from_chars(rest.data()+41,rest.data()+rest.size(),file);
    if(result.ec!=std::errc{} || result.ptr!=rest.data()+rest.size() || file<0) return false;
    *hash=lower(rest.substr(0,40));*idx=file;
    return true;
}

std::string TorrentStream::make_url(const std::string& hash, int idx) {
	return "torrent://" + hash + "/" + std::to_string(idx);
}

int TorrentStream::read(uint8_t* buf, int n) {
	if (!t_) return AVERROR(EIO);
	int k = bt::Engine::get().read(t_, reader_, file_, pos_, buf, n, abort_, read_interrupt_, read_deadline_.load());
	if (k > 0) {
		pos_ += k;
		return k;
	}
	if (k == 0) return AVERROR_EOF;
	if (k == -3) return AVERROR(ETIMEDOUT);
	if (k == -4) return AVERROR(EINVAL);
	if (k == -2) failure_ = "the torrent was stopped";
	const bool interrupted = (abort_ && abort_->load()) || (read_interrupt_ && read_interrupt_->load());
	return k == -1 && interrupted ? AVERROR_EXIT : AVERROR(EIO);
}

int64_t TorrentStream::seek(int64_t offset, int whence) {
	if (whence & AVSEEK_SIZE) return size_;
	whence &= ~AVSEEK_FORCE;
    int64_t base = 0;
    if (whence == SEEK_CUR) base = pos_;
    else if (whence == SEEK_END) base = size_;
    else if (whence != SEEK_SET) return AVERROR(EINVAL);
    if ((offset > 0 && base > std::numeric_limits<int64_t>::max()-offset) ||
        (offset < 0 && offset < -base)) return AVERROR(EINVAL);
    const int64_t target=base+offset;
    if (target < 0 || target > size_) return AVERROR(EINVAL);
	pos_ = target;
	if (t_) bt::Engine::get().seek_reader(t_, reader_, file_, pos_);
	return target;
}

std::string TorrentStream::peek(size_t n) {
	std::string out(n, '\0');
	int64_t keep = pos_;
	size_t got = 0;
	while (got < n) {
		int k = bt::Engine::get().read(t_, reader_, file_, pos_ + int64_t(got),
		                               reinterpret_cast<uint8_t*>(&out[got]), int(n - got), abort_, read_interrupt_);
		if (k <= 0) break;
		got += size_t(k);
	}
	pos_ = keep;
	out.resize(got);
	return out;
}

AVIOContext* TorrentStream::open_avio(const std::string& url, const std::atomic<bool>* abort, std::string* error) {
	std::string hash;
	int idx = 0;
	if (!parse_url(url, &hash, &idx)) {
		if (error) *error = "bad torrent address";
		return nullptr;
	}
	auto* ts = new TorrentStream();
	ts->file_ = idx;
	ts->abort_ = abort;
	ts->t_ = bt::Engine::get().open_reader(hash, idx, &ts->reader_, &ts->size_, error);
	if (!ts->t_) {
		delete ts;
		return nullptr;
	}
	const int kBufSize = 256 * 1024;
	auto* buf = static_cast<uint8_t*>(av_malloc(kBufSize));
	AVIOContext* pb = buf ? avio_alloc_context(buf, kBufSize, 0, ts, read_cb, nullptr, seek_cb) : nullptr;
	if (!pb) {
		av_free(buf);
		close_avio(&pb);
		bt::Engine::get().close_reader(ts->t_, ts->reader_);
		delete ts;
		if (error) *error = "out of memory";
		return nullptr;
	}
	pb->seekable = AVIO_SEEKABLE_NORMAL;
	dlog("torrent: playing file %d of %s (%lld bytes)", idx, hash.c_str(), (long long)ts->size_);
	return pb;
}

bool TorrentStream::is_ours(AVIOContext* pb) { return pb && pb->read_packet == read_cb; }

TorrentStream* TorrentStream::of(AVIOContext* pb) {
	return is_ours(pb) ? static_cast<TorrentStream*>(pb->opaque) : nullptr;
}

void TorrentStream::close_avio(AVIOContext** pb) {
	if (!pb || !*pb) return;
	if (auto* ts = of(*pb)) {
		bt::Engine::get().close_reader(ts->t_, ts->reader_);
		delete ts;
	}
	av_freep(&(*pb)->buffer);
	avio_context_free(pb);
}
