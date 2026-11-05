#include "hdr_fallback.hpp"
#include "session.hpp"
#include <cassert>
#include <cstdio>
int main() {
 Session s;int restores=0;
 auto restore=[&]{++restores;return true;};auto convert=[&]{s.fallback_to_sdr();};
 s.prefs_.auto_hdr_fallback=false;
 assert(!sx::try_hdr_fallback(s.allow_hdr_fallback(),restore,convert));assert(restores==0&&!s.uploaded);
 s.prefs_.auto_hdr_fallback=true;s.prefs_.native_hdr=false;
 assert(!sx::try_hdr_fallback(s.allow_hdr_fallback(),restore,convert));assert(restores==0);
 s.prefs_.native_hdr=true;
 assert(!sx::try_hdr_fallback(s.allow_hdr_fallback(),[&]{++restores;return false;},convert));
 assert(!s.hdr_fallback_session_&&!s.uploaded);
 assert(sx::try_hdr_fallback(s.allow_hdr_fallback(),restore,convert));
 assert(s.hdr_fallback_session_&&s.uploaded&&s.messages==1);
 assert(s.prefs_.native_hdr&&s.prefs_.auto_hdr_fallback); // Global preference retained.
 assert(!sx::try_hdr_fallback(s.allow_hdr_fallback(),restore,convert));assert(restores==2);
 s.hdr_fallback_session_=false;assert(s.allow_hdr_fallback());
 std::puts("HDR fallback: strict/SDR modes, failed restoration, retained picture, session reset and unchanged preferences passed");
}
