// Actual Native player/demux against external audio rendition groups.
#define NUVIO_TEST_REAL_NETWORK 1
#define main lifecycle_test_main
#include "player_lifecycle_host.cpp"
#undef main
#include "player_streams.hpp"
extern "C" int console_curl_nonblocking(int){return 0;}
int main(int argc,char** argv) try {
 require(argc==2,"master URL missing");
 {
  AVFormatContext* f=avformat_alloc_context();AVInputFormat hls{};hls.name="hls";f->iformat=&hls;
  for(int i=0;i<4;++i)avformat_new_stream(f,nullptr);
  av_new_program(f,0);av_new_program(f,1);
  av_program_add_stream_index(f,0,0);av_program_add_stream_index(f,0,2);
  av_program_add_stream_index(f,1,1);av_program_add_stream_index(f,1,3);
  require(playback_streams::compatible_audio(f,2,0) && !playback_streams::compatible_audio(f,2,1),"program isolation failed");
  av_program_add_stream_index(f,0,1);
  require(playback_streams::compatible_audio(f,2,1),"shared audio group rejected");
  require(playback_streams::compatible_audio(f,-1,1),"audio-only input rejected");
  hls.name="matroska";require(playback_streams::compatible_audio(f,3,0),"file audio switching restricted");
  avformat_free_context(f);
 }
 http_init("");
 Player p;Player::Options o;o.url=argv[1];o.audio_langs={"heb"};p.open(o);
 run_until(p,[&]{return p.state()==Player::State::Playing;});
 // The selected low rendition belongs to English group 0. Hebrew audio 1
 // belongs to the other video variant and must not override that pairing.
 require(p.audio_stream()==0,"preferred-language audio crossed HLS variant groups");
 const auto tracks=p.audio_tracks();
 require(tracks.size()==1 && tracks[0].lang=="eng","incompatible audio rendition exposed as switchable");
 // Debug builds initialize the 256 MiB packet pool on the first packet.
 // Leave time for that under CI load; the assertion still requires a real picture.
 run_until(p,[&]{return !p.buffering() && p.position()>.2 && p.picture_serial()==p.video_serial();},30);
 require(p.picture_pts()<3,"HLS startup consumed the wrong timeline");
 p.select_audio(1); // stale saved/UI track must not re-enable an incompatible playlist
 run_until(p,[&]{return p.position()>.4;});
 require(p.audio_stream()==0,"incompatible HLS switch was accepted");
 p.close();std::cout<<"HLS variants: compatible initial audio, first picture, track filtering and stale switch rejection passed\n";return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
