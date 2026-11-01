#include "live_guide.h"
#include "airtime.hpp"
#include "util.h"
#include <cassert>
#include <iostream>
const char* kUserAgent="Maxd-Guide-Fixture";
void http_setup_handle(void*){}
const std::string& http_ca_bundle(){static std::string empty;return empty;}
int main(int argc,char** argv) {
    assert(argc==2);const std::string base=argv[1];const auto now=*airtime::iso("2026-11-01T12:30:00Z");
    assert(airtime::xmltv("20261101143000 +0200")==now);
    assert(airtime::iso("2026-11-01T07:30:00-05:00")==now);
    assert(!airtime::iso("2026-02-30T10:00:00Z")&&!airtime::xmltv("20261101126000 +0000"));
    std::string xml="<?xml version='1.0'?><tv><programme channel='one' start='20261101120000 +0000' stop='20261101130000 +0000'><title>News &amp; weather</title></programme><programme channel='one' start='20261101130000 +0000' stop='20261101140000 +0000'><title><![CDATA[Next <show>]]></title></programme><programme channel='other' start='20261101120000 +0000' stop='20261101130000 +0000'><title>Wrong channel</title></programme></tv>";
    auto r=live_guide::parse_xmltv(xml,{"one"},now);assert(r.error.empty()&&r.channels.size()==1&&r.channels["one"].size()==2);
    auto pair=live_guide::current(r.channels["one"],now);assert(pair.now&&pair.now->title=="News & weather"&&pair.next&&pair.next->title=="Next <show>");
    pair=live_guide::current(r.channels["one"],*airtime::iso("2026-11-01T13:00:00Z"));assert(pair.now&&pair.now->title=="Next <show>"&&!pair.next);
    assert(live_guide::parse_xmltv(xml,{"missing"},now).channels.empty());
    assert(!live_guide::parse_xmltv("<html/>",{"one"},now).error.empty());
    assert(!live_guide::parse_xmltv("<tv>",{"one"},now).error.empty());
    assert(!live_guide::parse_xmltv("<!DOCTYPE tv [<!ENTITY x SYSTEM 'file:///etc/passwd'>]><tv>&x;</tv>",{"one"},now).error.empty());
    assert(live_guide::parse_xmltv("<!DOCTYPE tv SYSTEM 'xmltv.dtd'><tv/>",{},now).error.empty());
    std::atomic<bool> cancel=true;assert(!live_guide::parse_xmltv(xml,{"one"},now,&cancel).error.empty());
    assert(!live_guide::parse_xmltv(std::string(live_guide::max_bytes+1,' '),{},now).error.empty());
    auto playlist=live_tv::parse("#EXTM3U x-tvg-url=\"../guide.xml.gz\"\n#EXTINF:-1 tvg-id=\"one\",One\nhttps://video.example/live.ts\n",base+"/lists/main.m3u");
    assert(playlist.error.empty()&&playlist.guide_url==base+"/guide.xml.gz"&&playlist.channels[0].epg_id=="one");
    r=live_guide::fetch_xmltv(base+"/guide.xml.gz",{"one"},now,nullptr);assert(r.error.empty()&&r.channels["one"].size()==2);
    assert(!live_guide::fetch_xmltv(base+"/bomb.gz",{},now,nullptr).error.empty());
    r=live_guide::fetch_xtream(base,"fixture user","fixture&pass","42",now,nullptr);
    pair=live_guide::current(r.channels["42"],now);assert(r.error.empty()&&pair.now&&pair.now->title=="News & weather"&&pair.next&&pair.next->title=="Test");
    assert(!live_guide::fetch_xtream(base,"u","p","42&action=bad",now,nullptr).error.empty());
    assert(!live_guide::parse_xtream("not JSON","42",now).error.empty());
    assert(live_guide::parse_xtream("{\"epg_listings\":[]}","42",now).error.empty());
    // Guide failure leaves the channel playlist and playback URL intact.
    assert(playlist.channels.size()==1&&playlist.channels[0].url=="https://video.example/live.ts");
    std::cout<<"PASS: provider now/next, XMLTV timezone/entities/cancellation, gzip limits, Xtream contract and playback independence\n";
}
