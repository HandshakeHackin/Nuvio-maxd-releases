#include "addon_sources_boundary.hpp"
#include "cloudstream.h"
#include <cassert>
#include <fstream>
#include <iostream>
static sx::Session session(const Addon& a) {
    sx::Session s;s.addons_.push_back(std::make_unique<Addon>(a));return s;
}
static json scene(const sx::Session& s){return {{"title",s.title},{"message",s.dropdown.message},{"options",s.dropdown.options},{"danger",s.dropdown.danger}};}
int main(int argc,char**argv) {
    assert(argc==3);json plugins;assert(load_json(argv[1],plugins));
    json repo{{"name","German Providers"},{"manifestVersion",1},{"pluginLists",json::array({cloudstream::list_url})}},manifest;
    std::string error;assert(cloudstream::adapt(repo,plugins,manifest,error));
    Addon a;assert(parse_addon(manifest,cloudstream::source_url,a));a.from_account=true;
    auto s=session(a);s.addon_activate(0);assert(s.dropdown.options.size()==30);
    const int keller=int(std::find_if(a.providers.begin(),a.providers.end(),[](const auto& p){return p.id=="kellerkino";})-a.providers.begin());
    assert(keller<20);s.choose(keller);assert(s.addons_[0]->providers[keller].enabled && s.prefs_.provider_enabled[a.transport_url]["kellerkino"]==true);
    const auto enabled=s.prefs_.provider_enabled;s.choose(20);assert(s.prefs_.provider_enabled==enabled);
    assert(s.last_message==sx::i18n::tr("No PS5 port is available for this provider."));
    s.addon_activate(0);s.choose(27);assert(s.settings==1 && s.syncs.empty());
    auto inherited=session(a);inherited.inherit_plugins_=true;inherited.addon_activate(0);assert(!inherited.callback);
    Addon missing;missing.name="Unavailable repository";missing.transport_url="https://fixture.invalid/manifest.json";
    missing.from_account=true;missing.provider_repository=true;missing.unavailable=true;missing.load_error="HTTP 404";
    auto failed=session(missing);failed.addon_activate(0);failed.choose(0);assert(failed.retries==1 && failed.syncs.empty());
    failed.addon_activate(0);failed.choose(1);assert(failed.syncs.size()==1 && std::get<1>(failed.syncs[0]) && std::get<2>(failed.syncs[0]));
    assert(failed.reloads==1);
    auto separate=session(a);separate.addon_activate(0);assert(!separate.addons_[0]->providers[keller].enabled);
    json scenes;
    for(const auto& locale:sx::i18n::locales()) {
        sx::i18n::set_language(locale.code);auto menu=session(a);menu.addons_.push_back(std::make_unique<Addon>(missing));
        menu.refresh_addons();assert(menu.addons.items.size()==2);
        assert(menu.addons.items[0].description.find("0/20")!=std::string::npos);
        assert(menu.addons.items[1].description.find("HTTP 404")!=std::string::npos);
        menu.addon_activate(0);scenes[locale.code]["ports"]=scene(menu);
        menu.addon_activate(1);scenes[locale.code]["unavailable"]=scene(menu);
        for(const auto& row:menu.addons.items)scenes[locale.code]["sources"].push_back({{"name",row.name},{"version",row.version},{"description",row.description},{"types",row.types},{"initial",row.initial}});
    }
    std::ofstream(argv[2])<<scenes.dump(2);sx::i18n::set_language("en");
    std::cout<<"Production source menus: port toggles, unsupported entries, settings/removal indices, inherited sources, retry/removal, profile separation and eight localized scenes passed\n";
}
