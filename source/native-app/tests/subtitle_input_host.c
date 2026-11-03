// Exercises upstream libass input hardening with a real instrumented library.
#include <ass/ass.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  ASS_Library *library = ass_library_init();
  assert(library);
  char empty = 0;
  for (unsigned i = 0; i < 100; ++i) {
    ASS_Track *track = ass_read_memory(library, &empty, 0, "UTF-8");
    if (track)
      ass_free_track(track);
  }
  assert(!ass_read_memory(library, &empty, SIZE_MAX, NULL));
  assert(!ass_read_memory(library, &empty, SIZE_MAX, "UTF-8"));
  assert(argc == 2);
  FILE *file = fopen(argv[1], "rb");
  assert(file);
  assert(fseek(file, 0, SEEK_END) == 0);
  long size = ftell(file);
  assert(size > 0 && size < 16 * 1024 * 1024);
  rewind(file);
  char *font = malloc((size_t)size);
  assert(font && fread(font, 1, (size_t)size, file) == (size_t)size);
  fclose(file);
  ass_add_font(library, "Inter-Regular.ttf", font, (int)size);
  free(font);
  ASS_Renderer *renderer = ass_renderer_init(library);
  assert(renderer);
  ass_set_frame_size(renderer, 1920, 1080);
  ass_set_fonts(renderer, NULL, "Inter", ASS_FONTPROVIDER_NONE, NULL, 1);
  uint64_t clip_digest = 0;
  for (int scale = -1; scale <= 1; ++scale) {
    char data[2048];
    int length = snprintf(
        data, sizeof(data),
        "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n"
        "[V4+ Styles]\nFormat: "
        "Name,Fontname,Fontsize,PrimaryColour,SecondaryColour,OutlineColour,"
        "BackColour,Bold,Italic,Underline,StrikeOut,ScaleX,ScaleY,Spacing,"
        "Angle,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,"
        "Encoding\n"
        "Style: "
        "Default,Inter,32,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,"
        "100,100,0,0,1,0,0,7,0,0,0,1\n"
        "[Events]\nFormat: "
        "Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text\n"
        "Dialogue: "
        "0,0:00:00.00,0:00:02.00,Default,,0,0,0,,{\\an7\\pos(10,10)\\clip(%d,m "
        "0 0 l 20 0 20 20 0 20)\\p1}m 0 0 l 50 0 50 50 0 50\n",
        scale);
    assert(length > 0 && (size_t)length < sizeof(data));
    ASS_Track *track = ass_read_memory(library, data, (size_t)length, NULL);
    assert(track);
    int changed = 0;
    ASS_Image *image = ass_render_frame(renderer, track, 1000, &changed);
    assert(image);
    uint64_t digest = UINT64_C(1469598103934665603);
    size_t visible = 0;
    for (ASS_Image *part = image; part; part = part->next) {
      digest = (digest ^ (uint64_t)part->w) * UINT64_C(1099511628211);
      digest = (digest ^ (uint64_t)part->h) * UINT64_C(1099511628211);
      for (int y = 0; y < part->h; ++y)
        for (int x = 0; x < part->w; ++x) {
          unsigned char value = part->bitmap[y * part->stride + x];
          visible += value != 0;
          digest = (digest ^ value) * UINT64_C(1099511628211);
        }
    }
    assert(visible > 0);
    if (scale == -1)
      clip_digest = digest;
    else
      assert(digest == clip_digest);
    ass_free_track(track);
  }
  ass_renderer_done(renderer);
  ass_library_done(library);
  puts("Real libass: empty/codepage, overflow-sized input and non-positive "
       "vector-clip scales passed ASan/UBSan");
}
