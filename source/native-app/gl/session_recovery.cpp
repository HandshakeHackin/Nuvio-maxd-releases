#include "session.hpp"
#include "diagnostics.h"
#include "torrent/engine.h"
#include "torrent/torrent_stream.h"
namespace sx {
void Session::watch_fail(const std::string& error) {
    const auto source=w_stream_;const auto item=w_item_;const auto video=d_video_id_;
    const auto original_item=d_item_;const auto meta=d_meta_;const bool loaded=d_meta_loaded_;
    const auto detail_before=detail;const bool trailer=w_trailer_, transcode=w_transcode_;
    const bool source_choices=!trailer && !d_streams_.empty() && item.type!="torrent" && item.type!="media-video";
    watch_stop(false);
    nuvio_diagnostics::record({{"playerFailed",true},{"errorCode",nuvio_diagnostics::classify_error(error)}});
    const bool fatal_port=error.find("Close and reopen the app")!=std::string::npos;
    std::vector<std::string> choices;std::vector<int> codes;
    const auto add=[&](const char* label,int code){choices.emplace_back(label);codes.push_back(code);};
    if(!fatal_port){add("Retry playback",0);if(item.type!="tv" && item.type!="media-video" && !trailer)add("Play from beginning",1);}
    if(!fatal_port && item.type=="torrent" && !source.info_hash.empty())add("Choose another file",2);
    if(source_choices && !fatal_port)add("Choose another source",3);
    add("Diagnostic summary",4);add("Close",5);
    open_dropdown("Playback needs attention",choices,0,
        [this,source,item,video,original_item,meta,loaded,detail_before,trailer,transcode,codes,
         generation=profile_generation_,watch_generation=w_gen_,services_generation=services_generation_,media_generation=media_generation_](int choice){
            if(choice<0 || size_t(choice)>=codes.size() || generation!=profile_generation_ ||
               watch_generation!=w_gen_ || services_generation!=services_generation_ || media_generation!=media_generation_ || watching || resuming_)return;
            const int action=codes[size_t(choice)];
            if(action==4){diagnostic_qr();return;}
            if(action==5)return;
            if(action==2){
                std::vector<bt::FileInfo> files;std::string problem;
                if(!bt::Engine::get().wait_metadata(source.info_hash,files,nullptr,0,&problem)){
                    say("The file list is not available yet. Retry playback or reopen the torrent.",8,true);return;
                }
                torrent_present_files(source.info_hash,item.name,files,source.sources,{},-1,true);return;
            }
            d_item_=original_item;d_video_id_=video;d_meta_=meta;d_meta_loaded_=loaded;detail=detail_before;
            if(action==3){d_open_=true;detail_load_streams(video,"Choose another source");changed_|=kDetail;return;}
            start_over_next_=action==1;
            play_stream(source,transcode,trailer);
        });
    dropdown.message=error+" Saved progress is retained unless you explicitly restart a file.";
}
}
