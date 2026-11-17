#include "tigersheets/live_workbooks.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}}
int main(){try{
 WorkbookSession first,second;int firstChanges=0,secondChanges=0;
 auto firstSource=registerLiveWorkbook([&]{return LiveWorkbookSnapshot{"First.xlsx","/tmp/First.xlsx",first.captureSnapshot()};},[&]{++firstChanges;});
 auto secondSource=registerLiveWorkbook([&]{return LiveWorkbookSnapshot{"Second.xls","/tmp/Second.xls",second.captureSnapshot()};},[&]{++secondChanges;});
 auto sources=captureLiveWorkbooks(firstSource.get());require(sources.size()==1&&sources[0].name=="Second.xls","Registry captures other open workbooks while excluding the requesting window");
 const auto captured=sources[0].workbook;const auto before=firstChanges;
 std::string error;require(second.apply(0,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","42"}}}),&error),error.c_str());
 notifyLiveWorkbookChanged(secondSource.get());require(firstChanges==before+1&&secondChanges==0,"An edit invalidates other registered hosts without notifying its own host");
 require(captured->at("revision")==0&&captureLiveWorkbooks(firstSource.get())[0].workbook->at("revision")==1,"Registry captures retain immutable revisions for concurrent calculations");
 std::weak_ptr<LiveWorkbookRegistration> weak=secondSource;const auto beforeClose=firstChanges;secondSource.reset();
 require(weak.expired()&&captureLiveWorkbooks(firstSource.get()).empty(),"Registry retains no source or window after its registration is released");
 require(firstChanges==beforeClose+1,"Closing a source invalidates dependent calculation contexts");
 firstSource.reset();require(captureLiveWorkbooks().empty(),"All closed workbook registrations are pruned");
 std::cout<<"Live workbook registry lifetime tests passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
