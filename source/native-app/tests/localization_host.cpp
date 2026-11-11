#include "i18n.hpp"
#include "settings_navigation.hpp"
#include "experience_preferences.hpp"
#include "dynamic_font.hpp"
#include "bidi.h"
#include <cassert>
#include <iostream>
#include <set>

static std::vector<std::uint32_t> codepoints(const std::string& text){
 std::vector<std::uint32_t> out;
 for(size_t i=0;i<text.size();){
  auto c=static_cast<unsigned char>(text[i++]);unsigned cp=c;int n=c<128?0:(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:3;
  cp&=n==0?127:n==1?31:n==2?15:7;
  while(n--&&i<text.size())cp=(cp<<6)|(static_cast<unsigned char>(text[i++])&63);
  out.push_back(cp);
 }return out;
}
int main(int argc,char** argv){
 assert(argc==2);using namespace sx;ExperiencePreferences prefs;assert(prefs.ui_language=="en");
 for(auto bad:{nlohmann::json::object(),nlohmann::json::array(),nlohmann::json(nullptr),nlohmann::json{{"ui_language",99}},nlohmann::json{{"ui_language","../../de"}},nlohmann::json{{"ui_language",false}}}){load_experience(bad,prefs);assert(prefs.ui_language=="en");}
 assert(i18n::locales().size()==8);
 const std::pair<const char*,const char*> aliases[]={{"DE_de","de"},{"es-MX","es"},{"fr_CA","fr"},{"ar-EG","ar"},{"zh-CN","zh-Hans"},{"ZH_hans","zh-Hans"},{"ja-JP","ja"},{"ko-KR","ko"},{"zh-TW","en"},{"zh-Hant","en"},{"../../ar","en"},{"fr--CA","en"},{"invalid","en"}};
 for(const auto& [a,b]:aliases)assert(std::string(i18n::normalize_code(a))==b);
 for(const auto& locale:i18n::locales()){
  nlohmann::json saved={{"audio_languages","eng"},{"subtitle_languages","fra"},{"private_other","retained"}};
  prefs.ui_language=locale.code;save_experience(saved,prefs);ExperiencePreferences restored;load_experience(nlohmann::json::parse(saved.dump()),restored);
  assert(restored.ui_language==locale.code&&saved["private_other"]=="retained"&&saved["audio_languages"]=="eng"&&saved["subtitle_languages"]=="fra");
  i18n::set_language(locale.code);assert(std::string(i18n::code())==locale.code);assert(!std::string(i18n::native_name(locale.code)).empty());assert(i18n::rtl()==(std::string(locale.code)=="ar"));
  assert(std::string(i18n::tr("custom-provider-title"))=="custom-provider-title");
  const auto language=i18n::index(locale.code);
  for(const auto key:i18n::detail::core_key_indices){
   const char* english=i18n::detail::text_blob+i18n::detail::key_offsets[key];
   if(language)assert(i18n::detail::value_offsets[language][key]);
   assert(!std::string(i18n::tr(english)).empty());
  }
  SettingsContent content;SettingRow row;row.label=i18n::tr("Subtitle size");row.category="Subtitles";content.rows.push_back(row);
  assert(SettingsNavigation::search(content,i18n::tr("Subtitle size"))==std::vector<int>{0});
  SettingsNavigation nav;nav.refresh(content);assert(nav.categories==std::vector<std::string>{"Subtitles"});nav.index=1;assert(nav.open(content)&&nav.source_row()==0);
  row.category.clear();assert(SettingsNavigation::group(row)=="General");
 }
 for(size_t i=1;i<std::size(i18n::detail::key_offsets);++i)
  assert(std::string_view(i18n::detail::text_blob+i18n::detail::key_offsets[i-1])<std::string_view(i18n::detail::text_blob+i18n::detail::key_offsets[i]));
 i18n::set_language("de");assert(std::string(i18n::tr("Settings"))=="Einstellungen");assert(std::string(i18n::tr("Import playlist from USB"))=="Wiedergabeliste von USB importieren");
 i18n::set_language("fr");assert(std::string(i18n::tr("Settings"))=="Paramètres");assert(i18n::search_key("PARAMÈTRES")==i18n::search_key("parametres"));
 assert(i18n::search_key("ÉCRAN")==i18n::search_key("E\xcc\x81" "cran"));assert(i18n::search_key("TAMAÑO")==i18n::search_key("tamano"));
 assert(i18n::search_key("ÜBER")==i18n::search_key("über"));assert(i18n::search_key("Straße")==i18n::search_key("STRASSE"));
 assert(i18n::search_key("الإِعْدَادَات")==i18n::search_key("الاعدادات"));assert(i18n::search_key("设置")=="设置");assert(i18n::search_key("설정")=="설정");
 assert(std::string(i18n::tr("Choose a colour that is comfortable to read."))=="Choose a colour that is comfortable to read.");
 const auto shaped=bidi_visual("الإعدادات");assert(shaped!="الإعدادات"&&has_rtl(shaped));
 const auto mixed=bidi_visual("العربية 1080p USB");assert(mixed.find("1080p")!=std::string::npos&&mixed.find("USB")!=std::string::npos);
 std::set<std::uint32_t> needed;
 for(const auto& locale:i18n::locales()){
  i18n::set_language(locale.code);
  auto add=[&](const std::string& s){for(auto cp:codepoints(bidi_visual(s)))if(cp>32&&cp!=0xa0)needed.insert(cp);};
  add(locale.native_name);
  for(auto key:i18n::detail::core_key_indices)add(i18n::tr(i18n::detail::text_blob+i18n::detail::key_offsets[key]));
 }
 unsigned raster_count=0;
 for(int role=0;role<3;++role){
  const auto* primary=role==0?"Inter-Regular.ttf":role==1?"Inter-SemiBold.ttf":"Montserrat-Medium.ttf";
  const auto* weight=role==0?"Regular":"Bold";
  TtfChain chain;assert(chain.load(argv[1],{{primary},{std::string("NotoSans-")+weight+".ttf"},{std::string("NotoSansHebrew-")+weight+".ttf"},{std::string("NotoNaskhArabicUI-")+weight+".ttf",1.12f},{"NotoSansCJKsc-Regular.ttf"},{"NotoSansThai-Regular.ttf"}}));
  for(auto cp:needed){hui::gfx::GlyphBitmap glyph;if(!chain.glyph(cp,glyph)||glyph.pixels.empty()){std::cerr<<"Missing menu glyph U+"<<std::hex<<cp<<" in "<<primary<<"\n";return 1;}++raster_count;}
 }
 i18n::set_language("xx");assert(std::string(i18n::tr("Settings"))=="Settings");
 std::cout<<"Localization: eight saved languages, aliases, invalid settings, English fallback, stable category IDs, Latin/Arabic/CJK search, Arabic shaping and "<<raster_count<<" menu glyph rasters passed\n";
}
