// Production row builder with real app models; no rendering or account mocks.
#include "continue_watching.hpp"
#include <cassert>
#include <iostream>
int main(){
 std::map<std::string,json> library;
 std::map<std::string,sx::Progress> progress;
 for(int i=0;i<120;++i){auto id="tt"+std::to_string(1000+i);
  library[id]={{"type","movie"},{"name","Untitled"},{"state",{{"timeOffset",60000},{"duration",3600000},{"video_id",id},{"lastWatchedMs",1000+i}}}};
  sx::Progress p;p.name="Local";p.time=60;p.duration=3600;p.updated=1000+i;progress[id]=p;
 }
 library["removed"]={{"removed",true},{"state",{{"timeOffset",60000}}}};
 library["completed"]={{"state",{{"timeOffset",0},{"nuvioCompleted",true}}}};
 auto meta=[](sx::Item& item){if(item.name=="Untitled")item.name="Metadata title";};
 auto rows=sx::make_continue_watching(library,progress,true,meta);
 assert(rows.size()==120 && "Continue Watching silently truncated to catalog budget");
 assert(rows.front().id=="tt1119" && rows.back().id=="tt1000" && rows.front().name=="Metadata title");
 rows=sx::make_continue_watching(library,progress,false,meta);assert(rows.size()==120 && rows.front().name=="Local");
 progress["tt1119"].up_next="tt1119:2:1";
 rows=sx::make_continue_watching(library,progress,true,meta);assert(rows.size()==120); // no duplicate Next Up
 progress["tt1119"].time=0;library["tt1119"]["state"]["timeOffset"]=0;
 rows=sx::make_continue_watching(library,progress,true,meta);assert(rows.size()==120);
 assert(rows.front().up_next && rows.front().video_id=="tt1119:2:1");
 std::cout<<"Continue Watching: full history, ordering, metadata, guest history, removals and Next Up deduplication passed\n";
}
