#include "../ui/i18n.hpp"
// Profile-local source preferences, shared without UI or ranking dependencies.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "stremio.h"
#include <utility>
namespace sx::source_policy {
struct Preferences {
    bool checks=true, hide_cam=false;
    int min_height=0, max_gb=0;
    std::string hdr="any", language;
};
inline std::string language_label(const std::string& code) {
    const std::pair<const char*,const char*> names[]={{"eng","English"},{"spa",sx::i18n::tr("Spanish")},{"fra",sx::i18n::tr("French")},{"deu",sx::i18n::tr("German")},{"ita",sx::i18n::tr("Italian")},{"por",sx::i18n::tr("Portuguese")},{"jpn",sx::i18n::tr("Japanese")},{"kor",sx::i18n::tr("Korean")},{"hin",sx::i18n::tr("Hindi")}};
    for(const auto& name:names)if(code==name.first)return name.second;
    return "any";
}
inline Preferences load(const json& j) {
    Preferences p;const auto& s=jobj(j,"source_preferences");
    p.checks=jbool(s,"checks",true);p.hide_cam=jbool(s,"hide_cam",false);
    const auto h=jnum(s,"min_height"),g=jnum(s,"max_gb");
    p.min_height=(h==720||h==1080||h==2160)?int(h):0;
    p.max_gb=(g==2||g==5||g==10||g==20||g==40)?int(g):0;
    p.hdr=jstr(s,"hdr","any");if(p.hdr!="prefer"&&p.hdr!="avoid")p.hdr="any";
    p.language=jstr(s,"language");
    if(p.language!="eng"&&p.language!="spa"&&p.language!="fra"&&p.language!="deu"&&p.language!="ita"&&p.language!="por"&&p.language!="jpn"&&p.language!="kor"&&p.language!="hin")p.language.clear();
    return p;
}
inline void save(json& j,const Preferences& p) {
    j["source_preferences"]={{"checks",p.checks},{"hide_cam",p.hide_cam},{"min_height",p.min_height},{"max_gb",p.max_gb},{"hdr",p.hdr},{"language",p.language}};
}
}
