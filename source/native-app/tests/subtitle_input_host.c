// Exercises upstream libass input hardening with a real instrumented library.
#include <ass/ass.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    ASS_Library* library=ass_library_init();assert(library);
    char empty=0;
    for(unsigned i=0;i<100;++i){
        ASS_Track* track=ass_read_memory(library,&empty,0,"UTF-8");
        if(track)ass_free_track(track);
    }
    assert(!ass_read_memory(library,&empty,SIZE_MAX,NULL));
    assert(!ass_read_memory(library,&empty,SIZE_MAX,"UTF-8"));
    ass_library_done(library);
    puts("Real libass: empty/codepage and overflow-sized memory inputs rejected safely");
}
