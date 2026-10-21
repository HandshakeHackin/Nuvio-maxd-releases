#include "stream_result_cache.hpp"
#include <cassert>
#include <iostream>
int main() {
    sx::StreamResultCache cache;
    Stream s;s.url="https://example.com/video.m3u8";s.addon="Repo / Provider";s.request_headers={"Referer: https://example.com/"};
    cache.put("profile1/movie1/options1",{s},100);
    assert(cache.get("profile1/movie1/options1",101).size()==1);
    assert(cache.get("profile2/movie1/options1",101).empty());
    assert(cache.get("profile1/movie1/options2",101).empty());
    assert(cache.get("profile1/movie1/options1",160).empty());
    s.url+="?token=private";cache.put("signed",{s},100);assert(cache.get("signed",101).empty());
    s.url="https://example.com/video.mp4";cache.put("good",{s},100);
    cache.put("good",{},101);assert(cache.get("good",102).empty());
    cache.put("good",{s},100);cache.clear();assert(cache.get("good",101).empty());
    Stream torrent;torrent.info_hash=std::string(40,'a');torrent.addon="Torrentio";torrent.file_idx=3;
    torrent.sources={"tracker:https://example.com/announce"};torrent.debrid_resolvable=true;torrent.debrid_service="old";
    cache.put("mixed",{s,torrent},100);
    auto mixed=cache.get("mixed",101);assert(mixed.size()==2&&mixed[1].file_idx==3&&mixed[1].sources==torrent.sources);
    assert(!mixed[1].debrid_resolvable&&mixed[1].debrid_service.empty());
    mixed=cache.get("mixed",160);assert(mixed.size()==1&&mixed[0].info_hash==torrent.info_hash);
    assert(cache.get("mixed",399).size()==1);assert(cache.get("mixed",400).empty()); // reading never extends lifetime
    torrent.info_hash="not-a-hash";cache.put("invalid",{torrent},100);assert(cache.get("invalid",101).empty());
    Stream resolved;resolved.client_resolve=json{{"service","example"}};cache.put("private-resolver",{resolved},100);assert(cache.get("private-resolver",101).empty());
    cache.put("reload",{s},100);cache.erase("reload");assert(cache.get("reload",101).empty());
    cache.put("clock",{s},100);assert(cache.get("clock",99).empty());
    for(int i=0;i<8;++i)cache.put("lru"+std::to_string(i),{s},100+i);
    assert(cache.get("lru0",109).size()==1);cache.put("lru8",{s},110);
    assert(cache.get("lru1",111).empty()&&cache.get("lru0",111).size()==1);
    Stream large=s;large.description.assign(1024*1024,'x');cache.put("oversize",{large},100);assert(cache.get("oversize",101).empty());
    std::vector<Stream> many(300,s);cache.put("bounded",many,100);assert(cache.get("bounded",101).size()==256);
    std::cout<<"Stream result cache: HTTP/torrent expiry, stable torrent metadata, profile/options isolation, signed URL exclusion, memory/count/LRU bounds, reload and clearing passed\n";
}
