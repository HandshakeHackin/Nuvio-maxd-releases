#pragma once
#include "content.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <string_view>
#include <string>
#include <vector>
namespace sx {
// View indices are never passed to Session: activation always uses a source row ID.
class SettingsNavigation {
public:
    static constexpr int search_action=-1000;
    static std::vector<int> search(const SettingsContent& content,const std::string& query) {
        const auto fold=[](std::string text) {
            for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return text;
        };
        std::istringstream input(fold(query));
        std::vector<std::string> words;std::string word;
        while(input>>word)words.push_back(word);
        std::vector<int> matches;
        if(words.empty())return matches;
        for(int i=0;i<int(content.rows.size());++i) {
            const auto& row=content.rows[i];
            const auto text=fold(row.label+" "+row.help+" "+group(row));
            if(std::all_of(words.begin(),words.end(),[&](const auto& w){return text.find(w)!=std::string::npos;}))matches.push_back(i);
        }
        return matches;
    }
    std::vector<std::string> categories;
    std::vector<int> rows;
    std::string category;
    int index=0, parent_index=0;
    bool root() const { return category.empty(); }
    int count() const { return int(root()?(categories.empty()?0:categories.size()+1):rows.size()); }
    int source_row() const { return !root() && index>=0 && index<int(rows.size()) ? rows[index] : -1; }
    static std::string group(const SettingRow& row) { return row.category.empty()?"General":row.category; }
    void refresh(const SettingsContent& content) {
        const int selected=source_row();
        categories.clear(); rows.clear();
        for(const auto& row:content.rows) {
            const auto name=group(row);
            if(std::find(categories.begin(),categories.end(),name)==categories.end()) categories.push_back(name);
        }
        constexpr std::array<std::string_view,8> order={"Video","Audio","Subtitles","Playback","Appearance & Lighting","Account & Services","Addons","App & Updates"};
        const auto rank=[&](const std::string& name) {return std::find(order.begin(),order.end(),name)-order.begin();};
        std::stable_sort(categories.begin(),categories.end(),[&](const auto& a,const auto& b){return rank(a)<rank(b);});
        if(!root() && std::find(categories.begin(),categories.end(),category)==categories.end()) category.clear();
        if(!root()) {
            for(int i=0;i<int(content.rows.size());++i) if(group(content.rows[i])==category) rows.push_back(i);
            const auto found=std::find(rows.begin(),rows.end(),selected);
            if(found!=rows.end()) index=int(found-rows.begin());
        }
        index=std::clamp(index,0,std::max(0,count()-1));
    }
    bool open(const SettingsContent& content) {
        if(!root() || index<0 || index==0 || index>int(categories.size())) return false;
        parent_index=index; category=categories[index-1]; index=0; refresh(content); return true;
    }
    bool back(const SettingsContent& content) {
        if(root()) return false;
        category.clear(); rows.clear(); index=parent_index; refresh(content); return true;
    }
    SettingRow category_row(const SettingsContent& content,int at) const {
        SettingRow row; row.kind=SettingRow::Kind::action;
        if(at==0) {
            row.label="Search settings";
            row.help="Find a setting by its name, description or category. Select a result to open or change it.";
            return row;
        }
        row.label=categories.at(at-1); int n=0;
        for(const auto& item:content.rows) if(group(item)==row.label) {
            if(n<4) { if(n) row.help+=" · "; row.help+=item.label; } ++n;
        }
        if(n>4) row.help+=" · more";
        row.value=std::to_string(n)+(n==1?" setting":" settings");
        return row;
    }
};
}
