#include "i18n.hpp"
#include "settings_navigation.hpp"
#include "experience_preferences.hpp"
#include "dynamic_font.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
 assert(argc==2);using namespace sx;sx::ExperiencePreferences prefs;assert(prefs.ui_language=="en");
 for(auto bad:{nlohmann::json::object(),nlohmann::json::array(),nlohmann::json(nullptr),nlohmann::json{{"ui_language",99}},nlohmann::json{{"ui_language","../../de"}},nlohmann::json{{"ui_language",false}}}){load_experience(bad,prefs);assert(prefs.ui_language=="en");}
 nlohmann::json saved={{"audio_languages","eng"},{"subtitle_languages","fra"},{"private_other","retained"}};prefs.ui_language="de";save_experience(saved,prefs);ExperiencePreferences restored;load_experience(nlohmann::json::parse(saved.dump()),restored);assert(restored.ui_language=="de"&&saved["private_other"]=="retained"&&saved["audio_languages"]=="eng"&&saved["subtitle_languages"]=="fra");
 i18n::set_language("de");assert(std::string(i18n::tr("Settings"))=="Einstellungen");assert(std::string(i18n::tr("Import playlist from USB"))=="Wiedergabeliste von USB importieren");assert(std::string(i18n::tr("custom-provider-title"))=="custom-provider-title");
 for(size_t i=1;i<std::size(i18n::dictionary);++i)assert(i18n::dictionary[i-1].english<i18n::dictionary[i].english);
 assert(i18n::search_key("ÜBER") == i18n::search_key("über"));assert(i18n::search_key("Straße")==i18n::search_key("STRASSE"));assert(i18n::search_key("ÜBER")==i18n::search_key("U\xcc\x88" "ber"));
 SettingsContent content;SettingRow one;one.label="Übersicht";one.help="Sprache ändern";one.category="Appearance & Lighting";content.rows.push_back(one);one.label="Ton";one.category="Audio";content.rows.push_back(one);
 SettingsNavigation nav;nav.refresh(content);assert(nav.categories.front()=="Audio");assert(nav.category_row(content,1).label=="Ton");assert(SettingsNavigation::search(content,"ÜBERSICHT")==std::vector<int>{0});assert(SettingsNavigation::search(content,"übersicht")==std::vector<int>{0});assert(SettingsNavigation::search(content,"Beleuchtung")==std::vector<int>{0});nav.index=2;assert(nav.open(content)&&nav.source_row()==0);nav.refresh(content);assert(nav.source_row()==0);
 unsigned count=0;for(const auto& faces:std::vector<std::vector<FaceSpec>>{{{"Inter-Regular.ttf"},{"NotoSans-Regular.ttf"}},{{"Inter-SemiBold.ttf"},{"NotoSans-Bold.ttf"}},{{"Montserrat-Medium.ttf"},{"NotoSans-Bold.ttf"}}}){TtfChain chain;assert(chain.load(argv[1],faces));hui::gfx::GlyphBitmap g;for(char32_t cp:std::u32string(U"ÄÖÜäöüßẞ")){assert(chain.glyph(cp,g)&&!g.pixels.empty());++count;}}assert(count==24);
 i18n::set_language("xx");assert(std::string(i18n::tr("Settings"))=="Settings");
 std::cout<<"Localization: saved English/German preference, invalid settings, English fallback, German search, stable category IDs and 24 font rasters passed\n";
}
