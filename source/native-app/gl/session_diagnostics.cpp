#include "session.hpp"
#include "diagnostics.h"
#include "torrent/engine.h"
#include "qrcodegen.h"
#include "../update/core.hpp"
namespace sx {
void Session::diagnostic_qr() {
    if(torrent_drop_open_){bt::Engine::get().stop_web_drop();torrent_drop_open_=false;}
    sign_in=SignInState{};sign_in.link_only=true;sign_in.text_payload=true;
    sign_in.title="Copy diagnostic summary";sign_in.cancel_label="Close";
    sign_in.link=nuvio_diagnostics::support_summary(nuvio_update::current_version);
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX],qr[qrcodegen_BUFFER_LEN_MAX];
    if(qrcodegen_encodeText(sign_in.link.c_str(),temp,qr,qrcodegen_Ecc_MEDIUM,1,20,qrcodegen_Mask_AUTO,true)) {
        sign_in.qr_size=qrcodegen_getSize(qr);sign_in.qr_modules.resize(sign_in.qr_size*sign_in.qr_size);
        for(int y=0;y<sign_in.qr_size;++y)for(int x=0;x<sign_in.qr_size;++x)
            sign_in.qr_modules[y*sign_in.qr_size+x]=qrcodegen_getModule(qr,x,y);
    } else {say("Could not create the summary QR. Save the text report instead.",7,true);return;}
    sign_in_open=true;
}
void Session::diagnostic_menu() {
    open_dropdown("Playback diagnostics",{"Show summary / QR to copy","Save privacy-safe text report","Detailed log location"},0,
        [this,generation=profile_generation_](int pick){
            if(generation!=profile_generation_)return;
            if(pick==0)diagnostic_qr();
            else if(pick==1){const bool ok=write_file(data_dir_+"/diagnostic-summary.txt",nuvio_diagnostics::support_summary(nuvio_update::current_version));say(ok?"Saved diagnostic-summary.txt in app storage. It excludes account details, titles, URLs and tokens.":"Could not save the diagnostic summary.",9,!ok);}
            else if(pick==2)say("Private local logs: "+data_dir_+"/runtime.json, boot-current.txt and crash-current.txt. Review files before sharing.",12);
        });
}
}
