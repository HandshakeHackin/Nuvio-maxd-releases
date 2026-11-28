#include "stremio.h"
#include "http.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3)return 2;http_init(argv[1]);
    Addon a;std::string error;
    const bool ok=fetch_addon(normalize_addon_url(argv[2]),a,error);
    std::cout<<json{{"ok",ok},{"nativePorts",a.cloudstream_repository},{"providers",a.providers.size()},
        {"unavailableEntries",a.unsupported_providers.size()},{"error",error}}.dump()<<"\n";
    return ok && a.cloudstream_repository && a.providers.size()==20 ? 0:1;
}
