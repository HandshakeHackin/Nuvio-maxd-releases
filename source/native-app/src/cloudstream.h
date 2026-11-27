#pragma once
#include "util.h"
#include <set>

// Explicit native ports, not an Android/Dalvik runtime. Never download .cs3
// executables or redirect a repository to another author's server.
namespace cloudstream {
struct Port { const char *module, *id, *name, *sha256; unsigned types; };
#include "cloudstream_ports.inc"
inline constexpr const char* source_url = "https://raw.githubusercontent.com/Bnyro/GermanProviders/master/repo.json";
inline constexpr const char* list_url = "https://raw.githubusercontent.com/bnyro/GermanProviders/builds/plugins.json";
inline bool known_source(const std::string& url) {
    const auto u=lower(url);
    for(const auto* branch:{"master","main","refs/heads/master","refs/heads/main"})
        if(u==std::string("https://raw.githubusercontent.com/bnyro/germanproviders/")+branch+"/repo.json") return true;
    return false;
}
inline const Port* port(const std::string& id) {
    for(const auto& p:ports)if(id==p.id)return &p;
    return nullptr;
}
// Canonicalize only the known public repository aliases, within the active
// profile. Existing canonical choices take precedence over an older alias.
inline bool migrate_aliases(std::vector<std::string>& extras,json& enabled,json& settings) {
    bool changed=false;std::set<std::string> seen;std::vector<std::string> unique;
    for(const auto& value:extras) {
        const auto url=known_source(value)?std::string(source_url):value;
        if(url!=value)changed=true;
        if(seen.insert(url).second)unique.push_back(url);else changed=true;
    }
    extras=std::move(unique);
    for(auto* map:{&enabled,&settings}) {
        if(!map->is_object())continue;
        std::vector<std::string> aliases;
        for(auto it=map->begin();it!=map->end();++it)
            if(it.key()!=source_url && known_source(it.key()))aliases.push_back(it.key());
        for(const auto& alias:aliases) {
            if((*map)[alias].is_object()) {
                auto& target=(*map)[source_url];if(!target.is_object())target=json::object();
                for(auto it=(*map)[alias].begin();it!=(*map)[alias].end();++it)
                    if(!target.contains(it.key()))target[it.key()]=it.value();
            }
            map->erase(alias);changed=true;
        }
    }
    return changed;
}
inline bool repository(const json& manifest) {
    return manifest.is_object() && jobj(manifest,"pluginLists").is_array();
}
inline bool valid_list(const json& manifest) {
    const auto& lists=jobj(manifest,"pluginLists");
    return jnum(manifest,"manifestVersion")==1 && lists.is_array() && lists.size()==1 &&
           lists[0].is_string() && lower(lists[0].get<std::string>())==lower(list_url);
}
inline bool adapt(const json& repo,const json& plugins,json& out,std::string& error) {
    if(!valid_list(repo) || !plugins.is_array() || plugins.empty() || plugins.size()>256) {
        error="Invalid or unsupported CloudStream provider list";return false;
    }
    json scripts=json::array(),unsupported=json::array();std::set<std::string> seen;
    for(const auto& entry:plugins) {
        const auto module=lower(jstr(entry,"internalName"));
        if(module.empty() || module.size()>80 || !seen.insert(module).second) {
            error="Invalid or duplicate CloudStream provider ID";return false;
        }
        const auto name=jstr(entry,"name",jstr(entry,"internalName"));
        if(name.empty() || name.size()>160) {error="Invalid CloudStream provider name";return false;}
        const auto expected="https://raw.githubusercontent.com/bnyro/germanproviders/builds/"+module+".cs3";
        const bool compatible=jnum(entry,"apiVersion")==1 && jnum(entry,"status")==1 &&
            lower(jstr(entry,"repositoryUrl"))=="https://github.com/bnyro/germanproviders" &&
            lower(jstr(entry,"url"))==expected;
        bool found=false;
        if(compatible)for(const auto& p:ports)if(module==p.module) {
            json types=json::array();if(p.types&1)types.push_back("movie");if(p.types&2)types.push_back("tv");
            scripts.push_back({{"id",p.id},{"name",p.name},{"filename",std::string("builtin:cloudstream/")+p.id},
                {"nativePort",p.id},{"version","1.0.16"},{"supportedTypes",types}});
            found=true;
        }
        if(!found)unsupported.push_back(name);
    }
    out={{"id","cloudstream:bnyro-german-ps5"},{"name",jstr(repo,"name","German Providers")},
        {"version","1.0.16"},{"description","Standalone PS5 ports · Bnyro / dw98-git · GPL-3.0"},
        {"scrapers",scripts},{"_nuvioCloudstreamPorts",1},{"unsupportedProviders",unsupported}};
    return true;
}
}
