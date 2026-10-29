#pragma once
#include <string>
#include <vector>
namespace sx {
// Reopens can renumber tracks. Prefer the same complete metadata; use a unique
// language match only when there is no exact match, never an arbitrary ordinal.
template<class Track> int restored_subtitle(const Track& selected,const std::vector<Track>& tracks) {
    int exact=-1, exacts=0, language=-1, languages=0;
    for(size_t i=0;i<tracks.size();++i) {
        const auto& t=tracks[i];
        if(t.embedded<0 || t.lang!=selected.lang)continue;
        if(t.label==selected.label && t.forced==selected.forced && t.hearing_impaired==selected.hearing_impaired) {
            if(t.embedded==selected.embedded)return int(i);
            exact=int(i);++exacts;
        }
        if(!selected.lang.empty() && t.forced==selected.forced && t.hearing_impaired==selected.hearing_impaired) {language=int(i);++languages;}
    }
    return exacts==1?exact:exacts>1?-1:languages==1?language:-1;
}
}
