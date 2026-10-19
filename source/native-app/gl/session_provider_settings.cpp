#include "session.hpp"
#include "providers.h"

namespace sx {
void Session::provider_options(const std::string& url) {
    if (inherit_plugins_ || watching || resuming_) return;
    auto a=find_addon(url); if(!a)return;
    std::vector<std::string> rows,ids;
    for(const auto& p:a->providers) {rows.push_back(p.name);ids.push_back(p.id);}
    open_dropdown("Provider settings",rows,0,[this,url,ids](int i){
        if(i>=0 && size_t(i)<ids.size())provider_options_load(url,ids[i]);
    });
    dropdown.message="Choose a provider to change its available options. Settings belong to the current profile.";
}
void Session::provider_options_load(const std::string& url,const std::string& id) {
    auto a=find_addon(url);if(!a || inherit_plugins_ || watching || resuming_)return;
    for(const auto& p:a->providers) if(p.id==id) {
        const auto generation=profile_generation_;
        const auto addon_generation=addons_gen_;
        const auto cancel=std::make_shared<std::atomic<bool>>(false);
        struct Result {json layout;std::string error;};
        say("Loading provider options...");
        provider_tasks_.run<Result>([a=*a,p,cancel]{Result r;r.layout=providers::settings_layout(a,p,r.error,cancel.get());return r;},
            [this,url,id,generation,addon_generation](Result& r){
                if(generation!=profile_generation_ || addon_generation!=addons_gen_ || inherit_plugins_)return;
                if(!r.error.empty()){say(r.error,8,true);return;}
                provider_options_show(url,id,r.layout);
            });
        return;
    }
}
void Session::provider_options_show(const std::string& url,const std::string& id,const json& layout) {
    if(inherit_plugins_ || watching || resuming_ || !find_addon(url))return;
    const auto generation=profile_generation_;
    std::vector<std::string> rows;std::vector<json> fields;
    if(layout.is_array())for(const auto& f:layout) {
        const auto type=jstr(f,"type"),key=jstr(f,"key"),label=jstr(f,"label");
        if(!f.is_object() || key.empty() || key.size()>128 || label.empty())continue;
        // Password entry needs a verified masked keyboard; never silently use
        // the ordinary console keyboard for a field marked as a password.
        if(jbool(f,"isPassword"))continue;
        if(type!="text" && type!="select" && type!="toggle")continue;
        rows.push_back(label);fields.push_back(f);
        if(fields.size()==64)break;
    }
    if(rows.empty()){say("This provider has no editable options available on this PS5.",7);return;}
    rows.push_back("Reset provider settings");
    open_dropdown("Provider settings",rows,0,[this,url,id,layout,fields,generation](int i){
        if(i<0 || size_t(i)>fields.size() || generation!=profile_generation_ || inherit_plugins_)return;
        auto save=[this,url,id,layout,generation](const std::string& key,const json& value){
            if(generation!=profile_generation_ || inherit_plugins_ || !find_addon(url))return;
            auto& repository=prefs_.provider_settings[url];
            if(!repository.is_object())repository=json::object();
            auto& settings=repository[id];
            if(!settings.is_object())settings=json::object();
            if(key.empty())settings=json::object();else settings[key]=value;
            for(auto& a:addons_)if(a->transport_url==url)for(auto& p:a->providers)if(p.id==id)p.settings=settings;
            save_profile_settings();say("Provider settings saved");
            provider_options_show(url,id,layout);
        };
        if(size_t(i)==fields.size()){save("",json());return;}
        const auto f=fields[i];const auto key=jstr(f,"key"),type=jstr(f,"type");
        const auto& settings=jobj(jobj(prefs_.provider_settings,url.c_str()),id.c_str());
        if(type=="toggle") {save(key,!jbool(settings,key.c_str(),jbool(f,"defaultValue")));return;}
        if(type=="text") {
            say(jstr(f,"description",jstr(f,"label")),8);
            ask_text([save,key](const std::string& text){if(text.size()<=2048)save(key,text);});
            return;
        }
        const auto& options=jobj(f,"options");
        if(!options.is_array())return;
        std::vector<std::string> labels;std::vector<json> values;
        for(const auto& o:options)if(o.is_object() && o.contains("value") && o["value"].is_primitive() && !o["value"].is_null()) {
            labels.push_back(jstr(o,"label",o["value"].dump()));values.push_back(o["value"]);
            if(values.size()==64)break;
        }
        if(labels.empty())return;
        open_dropdown(jstr(f,"label"),labels,0,[save,key,values](int choice){
            if(choice>=0 && size_t(choice)<values.size())save(key,values[choice]);
        });
    });
    dropdown.message="Provider options are saved for this profile. Password fields require a masked keyboard and are not offered here.";
}
}
