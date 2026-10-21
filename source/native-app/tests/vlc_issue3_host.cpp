#define main existing_vlc_test_main
#include "vlc_host.cpp"
#undef main
#include "yuv_convert.h"
#include <fstream>
#include <iterator>
static std::string attempts(){std::ifstream f(std::getenv("NUVIO_DECODER_ATTEMPTS"));return {std::istreambuf_iterator<char>(f),{}};}
static void colour(VlcBackend& b,int component,bool ten){
 const double until=now_seconds()+10;bool found=false;
 while(now_seconds()<until){
  if(b.state()==4)throw std::runtime_error(b.error());
  if(const auto* p=b.present()){
   require(p->format==(ten?VideoFormat::Nv12_10:VideoFormat::Nv12),"bit depth changed");
   require(p->shift==0,"planar samples were treated as high-aligned P010");
   const int bytes=ten?2:1;
   YuvPicture y{p->y,p->uv,nullptr,p->w*bytes,((p->w+1)/2)*2*bytes,0,true,ten,p->shift,p->w,p->h};
   std::vector<uint8_t> rgba(size_t(p->w)*p->h*4);yuv_to_rgba(y,p->colors,1,rgba.data(),p->w,0,p->h);
   const size_t at=(size_t(p->h/2)*p->w+p->w/2)*4;
   require(rgba[at+component]>190 && rgba[at+(component+1)%3]<60 && rgba[at+(component+2)%3]<60,"decoded colour planes corrupted");
   found=true;break;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
 }
 require(found,"no test colour picture");
}
int main(int argc,char**argv)try{
 require(argc==5,"four colour fixtures required");
 {VlcBackend b;VlcBackend::Options o;o.url=std::string("file://")+argv[1];require(b.open(o),"control open failed");colour(b,0,false);b.close();}
 require(!attempts().empty(),"test decoder was not eligible before forced fallback");
 const auto before=attempts();
 for(int i=1;i<argc;i++){
  VlcBackend b;VlcBackend::Options o;o.url=std::string("file://")+argv[i];o.software=true;
  require(b.open(o),"software open failed");colour(b,i==4?2:i-1,i==4);b.close();
  require(attempts()==before,"software fallback reselected the PS5 decoder");
 }
 std::cout<<"VLC issue 3: real software decoder selection, RGB primary pixels, 8/10-bit planar packing passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
