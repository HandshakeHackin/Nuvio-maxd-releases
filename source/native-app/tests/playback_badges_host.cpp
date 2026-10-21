#include "playback_badges.hpp"
#include <cassert>
#include <iostream>
using sx::playback_badges::build;
bool has(const std::vector<std::string>& badges,const std::string& value) {
    return std::find(badges.begin(),badges.end(),value)!=badges.end();
}
int main() {
    for(auto label:{"240p","360p","480p","540p","576p","720p","1080p","1440p"})assert(has(build(std::string("Film.")+label+".WEB-DL"),label));
    assert(has(build("Film.2160p"),"4K") && has(build("Film.4320p"),"8K"));
    assert(has(build("2160p HDR10 HEVC DDP7.1",1920,800,true,false,false,"English · AAC stereo","H264 1920x800 (hardware)"),"1080p"));
    const auto actual=build("2160p HDR10 HEVC DDP7.1 Atmos WEB-DL",1920,800,true,false,false,"English · AAC stereo","H264 1920x800 (hardware)");
    for(auto value:{"1080p","SDR","2.0","WEB-DL","H.264","AAC"})assert(has(actual,value));
    for(auto value:{"4K","HDR10","7.1","HEVC","DD+","Atmos"})assert(!has(actual,value));
    assert(has(build("",3840,1600,true,true,true,"EAC3 5.1(side)"),"4K"));
    assert(has(build("",3840,1600,true,true,true,"EAC3 5.1(side)"),"HDR10"));
    assert(has(build("",3840,1600,true,true,true,"EAC3 5.1(side)"),"10-bit"));
    for(auto value:{"1.0","2.0","2.1","3.1","4.0","5.1","6.1","7.1"})assert(has(build("",0,0,false,false,false,std::string("Audio · ")+value),value));
    assert(has(build("DDP5.1 WEB-DL"),"5.1") && has(build("DDP5.1 WEB-DL"),"DD+"));
    assert(has(build("AAC2.0"),"2.0") && has(build("AAC2.0"),"AAC"));
    assert(has(build("7.1",0,0,false,false,false,"English · 6 ch"),"6 ch"));
    assert(!has(build("7.1",0,0,false,false,false,"English · 6 ch"),"7.1"));
    assert(has(build("",0,0,false,false,false,"7.1 track · AAC stereo"),"2.0"));
    assert(!has(build("",0,0,false,false,false,"7.1 track · AAC stereo"),"7.1"));
    const std::vector<std::pair<std::string,std::string>> sources={{"WEB-DL","WEB-DL"},{"web_dl","WEB-DL"},{"WEBRip","WEBRip"},{"Blu-ray","Blu-ray"},{"BluRay.REMUX","Blu-ray"},{"BDRip","BDRip"},{"BRRip","BRRip"},{"DVD-rip","DVD-Rip"},{"DVDRip","DVD-Rip"},{"DVDSCR","DVD-SCR"},{"HDTV","HDTV"},{"PDTV","PDTV"},{"HDCAM","CAM"},{"Telesync","TS"},{"Telecine","TC"}};
    for(const auto& [input,label]:sources)assert(has(build("Title."+input+".x265"),label));
    assert(has(build("BluRay.REMUX"),"REMUX"));
    assert(has(build("HDR10+ Dolby Vision"),"HDR10+"));
    assert(has(build("HDR10+ Dolby Vision",3840,2160,true,true),"HDR10+ source"));
    assert(!has(build("HDR10+ Dolby Vision",1920,1080,true,false),"Dolby Vision source"));
    assert(build("Movie.DVD-Rip").size()==1); // DV must not match DVD.
    assert(build("The.Cambridge.Tsunami.Hdrama.Film.2025").empty());
    assert(build("").empty());
    assert(!has(build("File size 5.1 GB"),"5.1"));
    assert(!has(build("7.1 GiB"),"7.1"));
    assert(has(build("",1080,1920,true,false),"1080×1920"));
    assert(build(std::string(9000,'x')+" HDR10 7.1").empty());
    std::cout<<"Playback badges: resolutions/crops, selected audio, HDR source vs SDR, codec overrides, release types, boundaries and bounded input passed\n";
}
