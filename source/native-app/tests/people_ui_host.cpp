#include "person.hpp"
#include "detail.hpp"
#include "browse_history.hpp"
#include <cassert>
#include <iostream>
int main(){
 hui::ui::Fonts fonts;sx::Icons icons;sx::Env env{fonts,icons};sx::PersonContent c;c.data.name="Person";c.data.movies={{"1","movie","One"},{"2","movie","Two"}};c.data.series={{"3","series","Show"}};
 sx::PersonPage page(env);page.set_content(&c);assert(page.selected()->id=="1");hui::InputFrame input;input.nav=hui::Direction::right;page.update(input,.1f);assert(page.selected()->id=="2");auto focus=page.focus();input.nav=hui::Direction::down;page.update(input,.1f);assert(page.selected()->id=="3");page.restore(focus);assert(page.selected()->id=="2");c.resolving=true;assert(!page.selected());c.resolving=false;c.loading=true;assert(!page.selected());c.loading=false;c.data.movies.clear();page.refresh();assert(page.selected()->id=="3");
 sx::DetailContent d;d.people={{"1","Actor","Role","",0,false},{"2","Director","Director","",0,true}};sx::Detail detail(env);detail.set_content(&d);detail.toggle_cast();assert(detail.mode()==sx::Detail::Mode::people);input.nav=hui::Direction::right;detail.update(input,.1f);assert(detail.cast_index()==1);auto df=detail.focus();detail.set_content(&d);detail.restore(df);assert(detail.mode()==sx::Detail::Mode::people&&detail.cast_index()==1);detail.toggle_cast();assert(detail.mode()==sx::Detail::Mode::streams);
 sx::BrowseHistory<int> history;history.scope(1);for(int i=0;i<8;++i)assert(history.push(i));assert(history.full()&&!history.push(8));for(int i=7;i>=0;--i)assert(history.pop()==i);assert(!history.pop());history.push(4);history.scope(2);assert(history.size()==0);history.push(3);history.scope(2);assert(history.pop()==3);
 std::cout<<"People UI: cast/person focus, movie/TV navigation, loading guards, bounded back history and profile reset passed\n";
}
