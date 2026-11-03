#include <expat.h>
#include <ft2build.h>
#include <png.h>
#include FT_FREETYPE_H
#include <cassert>
#include <cstring>
#include <fribidi.h>
#include <hb-ft.h>
#include <iostream>
#include <string>
#include <vector>
int main(int argc, char **argv) {
  assert(argc == 2);
  assert(std::string(XML_ExpatVersion()) == "expat_2.10.0");
  assert(std::string(png_get_libpng_ver(nullptr)) == "1.6.59");
  assert(std::string(hb_version_string()) == "15.0.0");
  assert(std::strstr(fribidi_version_info, "1.0.18"));
  FT_Library library = nullptr;
  assert(FT_Init_FreeType(&library) == 0);
  int major, minor, patch;
  FT_Library_Version(library, &major, &minor, &patch);
  assert(major == 2 && minor == 15 && patch == 0);
  struct Sample {
    const char *font;
    const char *text;
  };
  for (auto sample :
       std::vector<Sample>{{"Inter-Regular.ttf", "Movie 123"},
                           {"NotoSansHebrew-Regular.ttf", "שלום"},
                           {"NotoNaskhArabicUI-Regular.ttf", "مرحبا"},
                           {"NotoSansThai-Regular.ttf", "ภาษาไทย"},
                           {"NotoSansCJKsc-Regular.ttf", "电影"}}) {
    FT_Face face = nullptr;
    auto path = std::string(argv[1]) + "/" + sample.font;
    assert(FT_New_Face(library, path.c_str(), 0, &face) == 0);
    assert(FT_Set_Pixel_Sizes(face, 0, 32) == 0);
    hb_font_t *font = hb_ft_font_create_referenced(face);
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, sample.text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font, buffer, nullptr, 0);
    unsigned count = 0;
    auto glyphs = hb_buffer_get_glyph_infos(buffer, &count);
    assert(count > 0);
    for (unsigned i = 0; i < count; ++i) {
      assert(glyphs[i].codepoint != 0);
      assert(FT_Load_Glyph(face, glyphs[i].codepoint, FT_LOAD_RENDER) == 0);
    }
    hb_buffer_destroy(buffer);
    hb_font_destroy(font);
    FT_Done_Face(face);
  }
  FT_Done_FreeType(library);
  auto parser = XML_ParserCreate(nullptr);
  const char bad[] = "<tv><programme></tv>";
  assert(XML_Parse(parser, bad, sizeof(bad) - 1, XML_TRUE) == XML_STATUS_ERROR);
  XML_ParserFree(parser);
  png_image image{};
  image.version = PNG_IMAGE_VERSION;
  const unsigned char invalid[] = {137, 80, 78, 71, 13, 10, 26, 10, 0};
  assert(!png_image_begin_read_from_memory(&image, invalid, sizeof(invalid)));
  png_image_free(&image);
  std::cout
      << "Pinned XML/PNG versions, malformed input rejection and "
         "Latin/Hebrew/Arabic/Thai/CJK shaping and rasterization passed\n";
}
