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
    std::cout<<"Stream result cache: expiry, profile/options isolation, signed URL exclusion, invalidation and clearing passed\n";
}
