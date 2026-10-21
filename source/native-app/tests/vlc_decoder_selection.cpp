// A rejecting decoder records selection attempts; real FFmpeg still decodes.
#define MODULE_NAME nuviotestdecoder
#define MODULE_STRING "nuviotestdecoder"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <cstdio>
#include <cstdlib>
static int Open(vlc_object_t* obj) {
 if (!var_InheritBool(obj,"ps5vdec")) return VLC_EGENERIC;
 if(const char* path=std::getenv("NUVIO_DECODER_ATTEMPTS"))
  if(FILE* f=std::fopen(path,"a")){std::fputs("attempt\n",f);std::fclose(f);}
 return VLC_EGENERIC;
}
vlc_module_begin()
 set_shortname("Nuvio test decoder") set_description("Records decoder selection without decoding")
 set_capability("video decoder",10000) add_shortcut("ps5vdec")
 add_bool("ps5vdec",true,"Test decoder","Test decoder",false)
 set_callbacks(Open,nullptr)
vlc_module_end()
