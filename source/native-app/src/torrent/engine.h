// The built-in torrent engine: plays torrents without a Stremio streaming
// server. A small BitTorrent client made for streaming:
//
//   - finds peers through the addon's trackers (HTTP and UDP), the DHT
//     (Juliusz Chroboczek's dht library) and peer exchange;
//   - fetches the torrent's metadata from the peers (BEP 9) when the addon
//     only gives the info hash, or loads it directly from a .torrent file;
//   - prioritizes container header and tail pieces (first 2 and last 2 pieces
//     of the selected file) and pins them in the rolling cache so MP4 moov
//     atoms and MKV Cues demux without delay;
//   - downloads the pieces just ahead of where the player reads, in order,
//     asking several peers (and optional HTTP webseeds, BEP 19) for the next
//     blocks when they're late;
//   - keeps pieces in a rolling cache file of a configurable size in the app's
//     storage, so a big file streams without filling the disk.
//
// It only downloads: it never accepts connections for uploading.
//
// One network thread does all the peer and DHT work; the player's demuxer
// thread reads through TorrentStream (torrent_stream.h), which blocks until
// the pieces it needs are here.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bt {

struct FileInfo {
	std::string path;     // inside the torrent, "/"-separated
	int64_t size = 0;
	int64_t offset = 0;   // in the torrent's byte stream
};

struct MagnetInfo {
	std::string info_hash_hex;  // 40-char lowercase hex
	std::string display_name;
	std::vector<std::string> trackers;
	std::vector<std::string> webseeds;
	int select_only_file = -1;  // BEP 53 so=<idx> when present
};

struct TorrentMeta {
	std::string info_hash_hex;  // 40-char lowercase hex
	std::string name;
	int64_t total_size = 0;
	int64_t piece_length = 0;
	int num_pieces = 0;
	bool single_file_mode = false;
	std::vector<FileInfo> files;
	std::vector<std::string> trackers;
	std::vector<std::string> webseeds;
};

struct LocalTorrentFile {
	std::string path;          // full filesystem path
	std::string filename;      // basename
	std::string source_label;  // e.g. "USB 0", "PS5 /data/torrents"
	int64_t file_size = 0;     // .torrent file size in bytes
};

struct WebDropSubmission {
	std::string kind;   // "magnet" or "file"
	std::string value;  // magnet URI, .torrent URL, or local .torrent path
	std::string title;  // display name or filename
};

struct Stats {
	bool found = false;           // the torrent is running
	bool has_metadata = false;
	int peers = 0;                // connected
	int known_peers = 0;          // addresses found so far
	bool incoming = false;        // the router forwards our port: peers can connect to us
	double download_rate = 0;     // bytes per second
	double file_progress = 0;     // 0..1 of the selected file downloaded so far
};

struct Torrent;

class Engine {
public:
	static Engine& get();

	// Where the cache file, the DHT's node list and saved metadata go.
	// Call once before start(); cache_bytes is the rolling cache's size.
	void configure(const std::string& data_dir, int64_t cache_bytes);
	void set_cache_bytes(int64_t cache_bytes);
	int64_t cache_bytes() const;
	void shutdown();
	void set_enabled(bool enabled);
	void stop_web_drop();

	// Starts downloading the torrent (or keeps it going); any other torrent
	// stops. trackers: "udp://...", "http(s)://..." (addon sources, with or
	// without their "tracker:" prefix; "dht:" entries are ignored).
	void start(const std::string& info_hash_hex, const std::vector<std::string>& trackers);
	void start_with_webseeds(const std::string& info_hash_hex, const std::vector<std::string>& trackers,
	                         const std::vector<std::string>& webseeds);

	// Parses a magnet:? URI, 40-char hex info-hash, or 32-char Base32 info-hash.
	static bool parse_magnet(const std::string& uri_or_hash, MagnetInfo& out, std::string* error = nullptr);

	// Parses a raw .torrent buffer and extracts its exact info-hash, file list, trackers and webseeds.
	static bool parse_torrent_buffer(const std::string& data, TorrentMeta& out, std::string* raw_info_out = nullptr,
	                                 std::string* error = nullptr);

	// Starts a torrent from a magnet link or raw info-hash.
	bool start_magnet(const std::string& uri_or_hash, MagnetInfo* out = nullptr, std::string* error = nullptr);

	// Loads a .torrent buffer or local .torrent file directly into the engine with immediate metadata
	// (no BEP 9 peer metadata wait required).
	bool load_torrent_buffer(const std::string& data, TorrentMeta& out, std::string* error = nullptr);
	bool load_torrent_file(const std::string& path, TorrentMeta& out, std::string* error = nullptr);

	// Scans USB drives (/mnt/usb0..7) and PS5 storage (/data/torrents, /data/homebrew, /data) for .torrent files.
	static std::vector<LocalTorrentFile> scan_local_torrents(const std::string& extra_dir = "",
	                                                         const std::string& query = "");

	// Lightweight LAN HTTP server so a phone or PC browser can paste a magnet link or upload a .torrent file.
	std::string start_web_drop(int preferred_port = 8088);
	std::string web_drop_url() const;
	bool take_web_submission(WebDropSubmission* out);

	// Waits for the torrent's file list. False on timeout, cancel or error
	// (with *error set: "cancelled" when cancelled).
	bool wait_metadata(const std::string& info_hash_hex, std::vector<FileInfo>& files,
	                   const std::atomic<bool>* cancel, double timeout_s, std::string* error);

	// Which file will be played: its pieces come first.
	void select_file(const std::string& info_hash_hex, int file_idx);

	Stats stats(const std::string& info_hash_hex);

	// For TorrentStream. A reader holds the torrent; while any reader is
	// open the engine downloads ahead of the readers' positions.
	std::shared_ptr<Torrent> open_reader(const std::string& info_hash_hex, int file_idx, int* reader_id,
	                                     int64_t* file_size, std::string* error);
	void seek_reader(const std::shared_ptr<Torrent>& t, int reader_id, int file_idx, int64_t pos);
	void close_reader(const std::shared_ptr<Torrent>& t, int reader_id);
	// Reads up to n bytes of the file at pos, waiting for them to arrive.
	// Returns the count, 0 at the end of the file, -1 on abort/interrupt, -2 when the
	// torrent was stopped.
	int read(const std::shared_ptr<Torrent>& t, int reader_id, int file_idx, int64_t pos, uint8_t* buf, int n,
	         const std::atomic<bool>* abort, const std::atomic<bool>* read_interrupt = nullptr);

	// Picks the file to play: by season/episode in the name for a series,
	// else the largest video. -1 when there's no file.
	static int guess_file(const std::vector<FileInfo>& files, int season, int episode);

	// Returns the current wanted piece queue for a running torrent (used by host tests).
	std::vector<int> wanted_pieces_for_test(const std::string& info_hash_hex);
};

}  // namespace bt
