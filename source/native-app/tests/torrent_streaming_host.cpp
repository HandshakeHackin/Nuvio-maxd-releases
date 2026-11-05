#include "torrent_session_boundary.hpp"
#include "../src/torrent/torrent_stream.h"
#include "../src/torrent/playback_policy.hpp"
#include <limits>
extern "C" {
#include <libavformat/avformat.h>
}
#include "../src/torrent/bencode.h"
#include "../src/torrent/engine.h"
#include "../src/trailers.h"
#include "../src/http.h"
#include "../src/util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <future>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

double real_now_seconds() asm("__real__Z11now_secondsv");
double wrapped_now_seconds() asm("__wrap__Z11now_secondsv");
static std::atomic<double> time_offset{0};
double wrapped_now_seconds(){return real_now_seconds()+time_offset.load();}

static std::atomic<bool> block_seed{false},seed_started{false};

// Minimal POSIX socket HTTP client for testing Web Drop server without libcurl-dev on host VM
std::string HttpResponse::describe() const {
    if (!error.empty()) return error;
    return "HTTP " + std::to_string(status);
}

HttpResponse http_get(const std::string& url, long /*timeout_s*/, const std::atomic<bool>* cancel,
                      const std::vector<std::string>& /*headers*/) {
    HttpResponse res;
    if(block_seed && url.find("cdn.example.org")!=std::string::npos){
        seed_started=true;while(cancel && !cancel->load())std::this_thread::sleep_for(std::chrono::milliseconds(10));
        res.error="cancelled";return res;
    }
    if (!starts_with(url, "http://")) {
        res.error = "unsupported scheme";
        return res;
    }
    std::string rest = url.substr(7);
    size_t slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
    size_t colon = hostport.find(':');
    int port = colon == std::string::npos ? 80 : std::atoi(hostport.c_str() + colon + 1);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return res;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return res;
    }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(fd, req.data(), req.size(), 0);
    std::string raw;
    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, size_t(n));
    }
    close(fd);
    size_t hdr_end = raw.find("\r\n\r\n");
    if (hdr_end != std::string::npos) {
        res.status = 200;
        res.body = raw.substr(hdr_end + 4);
    }
    return res;
}

HttpResponse http_post_json(const std::string& url, const std::string& body, long /*timeout_s*/,
                            const std::atomic<bool>* /*cancel*/, const std::vector<std::string>& /*headers*/) {
    HttpResponse res;
    if (!starts_with(url, "http://")) return res;
    std::string rest = url.substr(7);
    size_t slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
    size_t colon = hostport.find(':');
    int port = colon == std::string::npos ? 80 : std::atoi(hostport.c_str() + colon + 1);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return res;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return res;
    }
    std::string req = "POST " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: " +
                      std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    send(fd, req.data(), req.size(), 0);
    std::string raw;
    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, size_t(n));
    }
    close(fd);
    size_t hdr_end = raw.find("\r\n\r\n");
    if (hdr_end != std::string::npos) {
        res.status = raw.find("200 OK") != std::string::npos ? 200 : 400;
        res.body = raw.substr(hdr_end + 4);
    }
    return res;
}

namespace {

std::string sha1_hex(const std::string& s) {
    unsigned char d[20];
    SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), d);
    static const char* hx = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 20; ++i) {
        out += hx[d[i] >> 4];
        out += hx[d[i] & 0x0f];
    }
    return out;
}

std::string build_synthetic_multi_torrent(std::string* expected_info_hash) {
    const int64_t plen = 262144; // 256 KB
    const int64_t f0_size = 1048576; // 1 MB (4 pieces: 0..3)
    const int64_t f1_size = 4194304; // 4 MB (16 pieces: 4..19)
    const int num_pieces = 20;

    BValue info = BValue::dict();
    info.d["name"] = BValue(std::string("Sample.Show.S01"));
    info.d["piece length"] = BValue(plen);
    info.d["pieces"] = BValue(std::string(size_t(num_pieces * 20), '\x42'));

    BValue files = BValue::list();
    {
        BValue f0 = BValue::dict();
        f0.d["length"] = BValue(f0_size);
        BValue p0 = BValue::list();
        p0.l.push_back(BValue(std::string("readme.txt")));
        f0.d["path"] = p0;
        files.l.push_back(f0);
    }
    {
        BValue f1 = BValue::dict();
        f1.d["length"] = BValue(f1_size);
        BValue p1 = BValue::list();
        p1.l.push_back(BValue(std::string("Sample.Show.S01E01.1080p.mkv")));
        f1.d["path"] = p1;
        files.l.push_back(f1);
    }
    info.d["files"] = files;

    const std::string raw_info = bencode(info);
    if (expected_info_hash)
        *expected_info_hash = sha1_hex(raw_info);

    BValue root = BValue::dict();
    root.d["announce"] = BValue(std::string("udp://tracker.opentrackr.org:1337/announce"));
    BValue alist = BValue::list();
    BValue tier1 = BValue::list();
    tier1.l.push_back(BValue(std::string("udp://open.stealth.si:80/announce")));
    tier1.l.push_back(BValue(std::string("http://tracker.opentrackr.org:1337/announce")));
    alist.l.push_back(tier1);
    root.d["announce-list"] = alist;

    BValue urllist = BValue::list();
    urllist.l.push_back(BValue(std::string("https://cdn.example.org/seeds/")));
    root.d["url-list"] = urllist;
    root.d["info"] = info;
    return bencode(root);
}

} // namespace

static std::string drop_request(const std::string& host, const std::string& method,
 const std::string& path,const std::string& body,const std::string& token="",const std::string& extra="") {
 int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(uint16_t(host.find_last_of(':')==std::string::npos ? 18088 : std::atoi(host.c_str()+host.find_last_of(':')+1)));inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
 assert(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0);
 std::string req=method+" "+path+" HTTP/1.1\r\nHost: "+host+"\r\nX-Nuvio-Pairing: "+token+
 "\r\nContent-Length: "+std::to_string(body.size())+"\r\n"+extra+"\r\n"+body;
 size_t sent=0;while(sent<req.size()){auto n=send(fd,req.data()+sent,req.size()-sent,0);assert(n>0);sent+=size_t(n);}
 std::string result;char buf[8192];for(;;){auto n=recv(fd,buf,sizeof(buf),0);if(n<=0)break;result.append(buf,size_t(n));}
 close(fd);return result;
}

int main(int argc, char** argv) {
    // 0. Verify standalone torrent type never pollutes catalog history / Continue Watching
    assert(!trailers::records_history(false, "torrent"));
    assert(trailers::records_history(false, "movie"));

    // 1. Test magnet parsing (40-char hex, 32-char Base32, trackers, webseeds, so=, quotes, &amp;, double-encoded)
    {
        bt::MagnetInfo mi;
        std::string err;
        const std::string mag_hex =
            "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567"
            "&dn=Big+Buck+Bunny+4K.mkv"
            "&tr=udp%3A%2F%2Ftracker.opentrackr.org%3A1337%2Fannounce"
            "&ws=https%3A%2F%2Farchive.org%2Fdownload%2Fbbb%2F"
            "&so=2";
        assert(bt::Engine::parse_magnet(mag_hex, mi, &err));
        assert(mi.info_hash_hex == "0123456789abcdef0123456789abcdef01234567");
        assert(mi.display_name == "Big Buck Bunny 4K.mkv");
        assert(mi.trackers.size() == 1 && mi.trackers[0] == "udp://tracker.opentrackr.org:1337/announce");
        assert(mi.webseeds.size() == 1 && mi.webseeds[0] == "https://archive.org/download/bbb/");
        assert(mi.select_only_file == 2);

        // Quoted, HTML-escaped (&amp;), and percent-encoded urn%3Abtih%3A magnet link
        bt::MagnetInfo mi_quoted;
        const std::string mag_quoted =
            "  \"magnet:?xt=urn%3Abtih%3A0123456789abcdef0123456789abcdef01234567&amp;dn=Quoted+Movie\"  ";
        assert(bt::Engine::parse_magnet(mag_quoted, mi_quoted, &err));
        assert(mi_quoted.info_hash_hex == "0123456789abcdef0123456789abcdef01234567");
        assert(mi_quoted.display_name == "Quoted Movie");

        // Base32 magnet (32 chars encoding 20 bytes of 0x00)
        bt::MagnetInfo mi32;
        assert(bt::Engine::parse_magnet("magnet:?xt=urn:btih:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA&dn=ZeroTest", mi32, &err));
        assert(mi32.info_hash_hex == "0000000000000000000000000000000000000000");
        assert(mi32.display_name == "ZeroTest");

        // Bare 40-char uppercase hex hash
        bt::MagnetInfo mi_bare;
        assert(bt::Engine::parse_magnet("ABCDEF0123456789ABCDEF0123456789ABCDEF01", mi_bare, &err));
        assert(mi_bare.info_hash_hex == "abcdef0123456789abcdef0123456789abcdef01");
    }

    // Malformed and oversized file metadata must fail before arithmetic or allocation.
    {
        BValue info=BValue::dict();info.d["name"]=BValue(std::string("invalid.mkv"));
        info.d["piece length"]=BValue(int64_t(1));info.d["pieces"]=BValue(std::string(20,'x'));
        info.d["length"]=BValue(int64_t(999999999999999999LL));
        BValue root=BValue::dict();root.d["info"]=info;bt::TorrentMeta meta;std::string error;
        assert(!bt::Engine::parse_torrent_buffer(bencode(root),meta,nullptr,&error));
        info.d.erase("length");BValue files=BValue::list();
        BValue file=BValue::dict();file.d["length"]=BValue(int64_t(999999999999999999LL));
        for(int i=0;i<12;++i)files.l.push_back(file);
        info.d["files"]=files;root.d["info"]=info;
        assert(!bt::Engine::parse_torrent_buffer(bencode(root),meta,nullptr,&error));
        assert(!bt::Engine::parse_torrent_buffer(std::string((4u<<20)+1,'x'),meta,nullptr,&error));
    }

    // Strict stream addresses and container-aware startup.
    {
        std::string hash;int file=-1;
        const auto base="torrent://"+std::string(40,'a')+"/";
        assert(TorrentStream::parse_url(base+"12",&hash,&file) && file==12);
        for(const auto &bad : {"", "-1", "1junk", "999999999999999999999", "1/2", "+1"})
            assert(!TorrentStream::parse_url(base+bad,&hash,&file));
        assert(!TorrentStream::parse_url("torrent://"+std::string(40,'z')+"/0",&hash,&file));
        std::string ts(512,'\0');ts[0]=ts[188]=ts[376]=0x47;
        assert(bt::head_only_start(ts,0) && !bt::head_only_start(ts,30));
        ts.assign(512,'\0');ts[4]=ts[196]=ts[388]=0x47;
        assert(bt::head_only_start(ts,0));
        assert(bt::head_only_start(std::string("\x1a\x45\xdf\xa3",4),0));
        assert(!bt::head_only_start(std::string("\0\0\0\x18",4)+"ftypisom",0));
        assert(!bt::head_only_start("G",0));
    }

    // 2. Test .torrent bencode parsing, exact raw info-hash, file loading, header/tail prioritization, seek_reader, and read_interrupt
    const std::string test_dir = (argc > 1 && argv[1] && argv[1][0]) ? std::string(argv[1]) : std::string("test-output/torrent-streaming/bt-test-dir");
    make_dirs(test_dir + "/nested/season1");
    bt::Engine& eng = bt::Engine::get();
    eng.configure(test_dir, 6ll << 30);
    assert(eng.start_web_drop(18088).empty());
    bt::TorrentMeta disabled;std::string disabled_error;
    assert(!eng.load_torrent_buffer("x",disabled,&disabled_error));
    eng.set_enabled(true);

    std::string expected_hash;
    const std::string torrent_bytes = build_synthetic_multi_torrent(&expected_hash);
    const std::string torrent_path = test_dir + "/Sample.Show.S01.torrent";
    const std::string nested_torrent_path = test_dir + "/nested/season1/Nested.Episode.torrent";
    assert(write_file(torrent_path, torrent_bytes));
    assert(write_file(nested_torrent_path, torrent_bytes));

    {
        bt::TorrentMeta meta;
        std::string raw_info, err;
        assert(bt::Engine::parse_torrent_buffer(torrent_bytes, meta, &raw_info, &err));
        assert(meta.info_hash_hex == expected_hash);
        assert(meta.name == "Sample.Show.S01");
        assert(meta.files.size() == 2);
        assert(meta.files[1].path == "Sample.Show.S01/Sample.Show.S01E01.1080p.mkv");
        assert(meta.trackers.size() == 3);
        assert(meta.webseeds.size() == 1 && meta.webseeds[0] == "https://cdn.example.org/seeds/");

        // Load directly into Engine and verify metadata is immediately available without peer wait
        bt::TorrentMeta loaded_meta;
        assert(eng.load_torrent_file(torrent_path, loaded_meta, &err));
        assert(loaded_meta.info_hash_hex == expected_hash);

        // Verify internal .torrent backup was saved for USB unplug resilience
        assert(file_exists(test_dir + "/torrents/" + expected_hash + ".torrent"));
        assert(file_exists(test_dir + "/torrents/" + expected_hash + ".info"));

        std::vector<bt::FileInfo> files;
        assert(eng.wait_metadata(expected_hash, files, nullptr, 0.5, &err));
        assert(files.size() == 2);

        // Select the video file (index 1, pieces 4..19) and verify header + tail piece prioritization
        const int best = bt::Engine::guess_file(files, 1, 1);
        assert(best == 1);
        eng.select_file(expected_hash, best);

        const std::vector<int> wanted = eng.wanted_pieces_for_test(expected_hash);
        // File 1 spans pieces 4..19. Four playhead pieces lead, followed by the two container tail pieces.
        assert(wanted.size() == 16);
        assert(wanted[0] == 4);
        assert(wanted[1] == 5);
        assert(wanted[2] == 6);
        assert(wanted[3] == 7);
        assert(wanted[4] == 19);
        assert(wanted[5] == 18);

        // Open a reader, seek to piece 12 (offset 8 * 256 KB inside file 1), and verify immediate wanted queue shift
        int reader_id = 0;
        int64_t fsize = 0;
        auto t_handle = eng.open_reader(expected_hash, best, &reader_id, &fsize, &err);
        assert(t_handle != nullptr);
        assert(fsize == 4194304);

        eng.seek_reader(t_handle, reader_id, best, 8 * 262144);
        const std::vector<int> wanted_after_seek = eng.wanted_pieces_for_test(expected_hash);
        assert(!wanted_after_seek.empty());
        // Piece 12 (4 + 8) and 13 must now be at the very front of the queue, followed by 14/15 and pinned header/tail (4, 5, 19, 18)
        assert(wanted_after_seek[0] == 12);
        assert(wanted_after_seek[1] == 13);
        assert(wanted_after_seek[2] == 14);
        assert(wanted_after_seek[3] == 15);
        assert(wanted_after_seek[4] == 4);
        assert(wanted_after_seek[5] == 5);
        assert(wanted_after_seek[6] == 19);
        assert(wanted_after_seek[7] == 18);

        // Test dynamic cache expansion on active torrent
        eng.set_cache_bytes(12ll << 30);
        assert(eng.cache_bytes() == (12ll << 30));

        // Test read_interrupt waking up a blocked Engine::read within 100ms
        std::atomic<bool> abort_flag{false};
        std::atomic<bool> seek_interrupt{false};
        std::thread trigger([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            seek_interrupt.store(true);
        });
        const auto t0 = std::chrono::steady_clock::now();
        uint8_t dummy[64] = {};
        const int rk = eng.read(t_handle, reader_id, best, 8 * 262144, dummy, sizeof(dummy), &abort_flag, &seek_interrupt);
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        trigger.join();
        assert(rk == -1);
        assert(elapsed_ms < 500);

        seek_interrupt=false;
        const auto deadline_start=std::chrono::steady_clock::now();
        assert(eng.read(t_handle,reader_id,best,8*262144,dummy,sizeof(dummy),&abort_flag,
                        &seek_interrupt,now_seconds()+0.06)==-3);
        assert(std::chrono::steady_clock::now()-deadline_start<std::chrono::milliseconds(500));
        assert(eng.read(t_handle,reader_id,best,-1,dummy,sizeof(dummy),&abort_flag)==-4);

        eng.close_reader(t_handle, reader_id);

        // Test TorrentStream::open_avio, seek (with seek_reader), and set_read_interrupt
        std::atomic<bool> ts_abort{false};
        std::atomic<bool> ts_seek_pending{false};
        std::string ts_err;
        const std::string ts_url = TorrentStream::make_url(expected_hash, best);
        AVIOContext* pb = TorrentStream::open_avio(ts_url, &ts_abort, &ts_err);
        assert(pb != nullptr);
        assert(TorrentStream::is_ours(pb));
        TorrentStream::set_read_interrupt(pb, &ts_seek_pending);
        auto* ts = TorrentStream::of(pb);
        assert(ts != nullptr);
        assert(ts->size() == 4194304);

        // Seek via TorrentStream::seek to piece 15 (offset 11 * 256 KB inside file 1)
        const int64_t seek_res = ts->seek(11 * 262144, SEEK_SET);
        assert(seek_res == 11 * 262144);
        const std::vector<int> wanted_ts = eng.wanted_pieces_for_test(expected_hash);
        assert(!wanted_ts.empty());
        assert(wanted_ts[0] == 15);
        assert(wanted_ts[1] == 16);

        // Trigger read_interrupt while blocked in TorrentStream::read and verify AVERROR_EXIT
        std::thread ts_trigger([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            ts_seek_pending.store(true);
        });
        uint8_t ts_buf[64] = {};
        const int ts_r = ts->read(ts_buf, sizeof(ts_buf));
        ts_trigger.join();
        assert(ts_r == AVERROR_EXIT);
        ts_seek_pending=false;
        TorrentStream::set_read_deadline(pb,now_seconds()+0.06);
        assert(ts->read(ts_buf,sizeof(ts_buf))==AVERROR(ETIMEDOUT));
        TorrentStream::set_read_deadline(pb,0);
        assert(ts->seek(std::numeric_limits<int64_t>::max(),SEEK_CUR)==AVERROR(EINVAL));
        assert(ts->seek(std::numeric_limits<int64_t>::min(),SEEK_END)==AVERROR(EINVAL));
        assert(ts->seek(ts->size()+1,SEEK_SET)==AVERROR(EINVAL));
        assert(ts->seek(0,AVSEEK_SIZE)==ts->size());

        TorrentStream::close_avio(&pb);
        assert(pb == nullptr);
    }

    // 3. Test local .torrent recursive scanner & search filter (including nested subdir, excluding <40-hex>.torrent internal cache)
    {
        const auto all = bt::Engine::scan_local_torrents(test_dir);
        bool found_top = false, found_nested = false, found_internal_hash = false;
        for (const auto& f : all) {
            if (f.filename == "Sample.Show.S01.torrent") found_top = true;
            if (f.filename == "Nested.Episode.torrent") found_nested = true;
            if (f.filename == expected_hash + ".torrent") found_internal_hash = true;
        }
        assert(found_top);
        assert(found_nested);
        assert(!found_internal_hash);

        const auto filtered = bt::Engine::scan_local_torrents(test_dir, "nested.episode");
        assert(!filtered.empty());
        assert(filtered[0].filename == "Nested.Episode.torrent");
        const auto none = bt::Engine::scan_local_torrents(test_dir, "nonexistent_xyz_999");
        assert(none.empty());
    }

    // Paired HTTP transport: authorization, origin, bounds, queue, bytes and token rotation.
    {
        const std::string url=eng.start_web_drop(18088);assert(!url.empty());
        auto fragment=url.find('#');assert(fragment!=std::string::npos);
        const auto host=url.substr(7,fragment-8),token=url.substr(fragment+1);
        assert(token.size()==48);
        auto request=[&](const std::string& method,const std::string& path,const std::string& body,const std::string& key,const std::string& extra=""){
            return drop_request(host,method,path,body,key,extra);
        };
        assert(request("GET","/","","").find("200 Response")!=std::string::npos);
        const std::string magnet="magnet:?xt=urn:btih:1111111111111111111111111111111111111111&dn=WebDropMovie";
        assert(request("POST","/add",magnet,"").find("403 Response")!=std::string::npos);
        assert(request("POST","/add",magnet,token,"Origin: https://evil.example\r\n").find("403 Response")!=std::string::npos);
        assert(drop_request("evil.example","POST","/add",magnet,token).find("403 Response")!=std::string::npos);
        assert(request("POST","/add-extra",magnet,token).find("404 Response")!=std::string::npos);
        assert(request("POST","/add",magnet,token,"Content-Length: 1\r\n").find("400 Response")!=std::string::npos);
        assert(request("POST","/add",std::string(17000,'x'),token).find("413 Response")!=std::string::npos);
        assert(request("POST","/add",magnet,token).find("200 Response")!=std::string::npos);
        assert(request("POST","/add",magnet,token).find("429 Response")!=std::string::npos);
        bt::WebDropSubmission sub;assert(eng.take_web_submission(&sub));assert(sub.title=="WebDropMovie");
        assert(request("POST","/upload",torrent_bytes,token).find("200 Response")!=std::string::npos);
        assert(eng.take_web_submission(&sub));assert(sub.kind=="buffer"&&sub.value==torrent_bytes);
        const auto next=eng.start_web_drop(18088);assert(next!=url);
        assert(request("POST","/add",magnet,token).find("403 Response")!=std::string::npos);
        time_offset=601;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        assert(eng.web_drop_url().empty());
        eng.stop_web_drop();time_offset=0;assert(!eng.take_web_submission(&sub));
    }

    // 5. Test Session::torrent_* end-to-end (gl/session_torrents.cpp)
    {
        sx::Session sess;
        sess.data_dir_ = test_dir;
        sess.profile_dir_ = test_dir + "/profile1";
        make_dirs(sess.profile_dir_);
        sess.prefs_.builtin_torrents=true;
        sess.torrent_load();
        // 3 action tiles (Hub, Enter Magnet, Web Drop QR) + 2 discovered .torrent files
        assert(sess.torrents_content.items.size() == 5);
        assert(sess.torrents_content.items[0].name == "Torrents & Magnet Hub");
        assert(sess.torrents_content.items[1].name == "Enter Magnet or .torrent URL");
        assert(sess.torrents_content.items[2].name == "Phone / PC Web Drop (QR Code)");

        // Test QR code modal generation
        sess.torrent_activate(2, false);
        assert(sess.sign_in_open);
        assert(sess.sign_in.qr_size > 0);
        sess.sign_in_open = false;
        sess.torrent_poll_drop();assert(eng.web_drop_url().empty());
        // Received magnets wait for console consent and stale profile callbacks cannot open them.
        sess.torrent_show_web_drop_qr();
        auto link=sess.sign_in.link;auto marker=link.find('#');
        auto drop_host=link.substr(7,marker-8),key=link.substr(marker+1);
        const std::string incoming="magnet:?xt=urn:btih:1111111111111111111111111111111111111111";
        assert(drop_request(drop_host,"POST","/add",incoming,key).find("200 Response")!=std::string::npos);
        sess.torrent_poll_drop();assert(sess.played.empty()&&eng.web_drop_url().empty());
        assert(sess.choices.size()==2&&sess.choices.front()=="Cancel");
        auto stale=sess.pick;++sess.profile_generation_;stale(1);assert(sess.played.empty());
        sess.torrent_show_web_drop_qr();sess.prefs_.builtin_torrents=false;sess.torrent_poll_drop();
        assert(eng.web_drop_url().empty());sess.torrent_open_input(incoming);assert(sess.played.empty());
        sess.prefs_.builtin_torrents=true;

        // Test opening a local .torrent file via Session::torrent_open_input
        sess.torrent_open_input(torrent_path, false);
        assert(sess.watching);
        assert(sess.played.size() == 1);
        assert(sess.played.back().info_hash == expected_hash);
        assert(sess.played.back().file_idx == 1);
        assert(sess.w_item_.type == "torrent");
        assert(!trailers::records_history(false, sess.w_item_.type));

        // Test saving and restoring resume progress in torrents.json
        sess.player_.time = 42.0;
        sess.player_.duration_value = 120.0;
        sess.torrent_progress(false);
        assert(sess.torrent_resume(sess.w_item_.id) == 42.0);
        // Rebuild UI and verify "Resume 0:42" badge appears on the Recent stream card
        sess.torrent_build();
        assert(sess.torrents_content.items[3].version.find("Resume 0:42") != std::string::npos);

        const auto episode_id=sess.w_item_.id;
        const auto other_id="torrent:"+expected_hash+":0";
        sess.torrent_state_["resume"][other_id]={{"position",25},{"duration",120},{"updated",1}};
        sess.torrent_state_["resume"]["torrent:"+expected_hash+":2147483647"]={{"position",99},{"duration",120},{"updated",9e15}};
        sess.watching=false;
        sess.torrent_activate(3,true);
        assert(sess.choices[0].find("Resume file 2 from 0:42")!=std::string::npos);
        assert(sess.choices[1]=="Play this file from beginning");
        auto resume=sess.pick;resume(0);
        assert(sess.played.back().file_idx==1 && sess.torrent_resume(episode_id)==42);
        sess.watching=false;
        sess.torrent_activate(3,true);auto stale_resume=sess.pick;
        ++sess.profile_generation_;stale_resume(1);
        assert(sess.torrent_resume(episode_id)==42 && !sess.watching);
        sess.torrent_activate(3,true);auto restart=sess.pick;restart(1);
        assert(sess.played.back().file_idx==1 && sess.torrent_resume(episode_id)==0);
        assert(sess.torrent_resume(other_id)==25); // Other files are never erased.
        sess.player_.time=42;sess.player_.duration_value=0;
        sess.torrent_progress();assert(sess.torrent_resume(episode_id)==42);
        sess.player_.duration_value=120;
        sess.torrent_state_["resume"][episode_id]["position"]=std::numeric_limits<double>::infinity();
        assert(sess.torrent_resume(episode_id)==0);
        sess.torrent_progress();

        sess.w_item_.id="torrent:"+expected_hash+":-1";
        sess.torrent_progress();
        assert(!sess.torrent_state_["resume"].contains(sess.w_item_.id));
        assert(sess.torrent_resume(other_id)==25);
        sess.w_item_.id=episode_id;

        // Verify finishing a video (>= 95%) clears resume progress
        sess.player_.time = 118.0;
        sess.torrent_progress(true);
        assert(sess.torrent_resume(sess.w_item_.id) == 0.0);
        sess.watching = false;

        // Test Square (secondary = true) opening the multi-file picker dropdown
        sess.torrent_open_input(torrent_path, true);
        assert(!sess.choices.empty());
        assert(sess.choices[0].find("Auto-pick largest video") != std::string::npos);
        sess.pick(1); // Pick file 1
        assert(sess.watching);
        sess.watching = false;

        // Test USB unplug resilience: delete the original .torrent file and verify clicking Recent still works via cached <hash>.torrent!
        std::remove(torrent_path.c_str());
        assert(!file_exists(torrent_path));
        sess.torrent_activate(3, false); // Activate the Recent entry whose original path was deleted
        assert(sess.watching);
        assert(sess.played.back().info_hash == expected_hash);
        sess.watching = false;

        // Test global search bar intercepting magnet links and raw info-hashes
        assert(sess.torrent_try_search_input("magnet:?xt=urn:btih:2222222222222222222222222222222222222222&dn=SearchMagnet"));
        assert(sess.watching);
        assert(sess.played.back().info_hash == "2222222222222222222222222222222222222222");
        sess.watching = false;
    }

    {
        block_seed=true;seed_started=false;
        bt::TorrentMeta meta;std::string error;
        assert(eng.load_torrent_buffer(torrent_bytes,meta,&error));
        eng.select_file(meta.info_hash_hex,1);
        int reader=0;int64_t size=0;
        auto handle=eng.open_reader(meta.info_hash_hex,1,&reader,&size,&error);assert(handle);
        for(int i=0;i<100&&!seed_started;++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(seed_started);
        auto stop=std::async(std::launch::async,[&]{eng.set_enabled(false);});
        assert(stop.wait_for(std::chrono::seconds(3))==std::future_status::ready);stop.get();
        block_seed=false;
        eng.start(meta.info_hash_hex,{});assert(!eng.stats(meta.info_hash_hex).found);
    }
    eng.set_enabled(false);
    assert(eng.start_web_drop(18088).empty());
    eng.shutdown();
    std::printf("ALL TORRENT STREAMING HOST TESTS PASSED!\n");
    return 0;
}
