#include "tigersheets/workbook_links.hpp"
#include "tigersheets/calculation.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){try{
 const std::string path=std::string(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/independent.xls";
 Json book={{"revision",7},{"sheets",Json::array({{{"id","model"},{"name","Model"},{"cells",Json::array({{{"row",1},{"col",1},{"input","='[independent.xls]Inputs 2026'!A2*2"}},{{"row",2},{"col",1},{"input","='[independent.xls]Owner''s data'!A1"}}})}}})},{"links",Json::array({{{"index",1},{"name","independent.xls"},{"path",path},{"enabled",true}}})}};
 const auto canonical=book;
 const auto unrefreshed=prepareWorkbookLinks(book);require(unrefreshed.filesRead==0,"BIFF links are not read without explicit refresh");
 WorkbookLinkOptions options;options.refresh=true;
 const auto prepared=prepareWorkbookLinks(book,nullptr,options);
 require(prepared.filesRead==1&&prepared.diagnostics.empty()&&prepared.links.at(0).at("state")=="refreshed","Explicit refresh reads a local BIFF source under source-byte and expanded-data budgets");
 const auto calculation=CalculationEngine().calculate(prepared.snapshot);require(calculation.at("status")=="ok","Linked BIFF workbook calculates successfully");
 bool first=false,second=false;for(const auto& value:calculation.at("values"))if(value.at("sheet")=="model"){if(value.at("row")==1)first=value.at("type")=="number"&&value.at("value")==2469;if(value.at("row")==2)second=value.at("type")=="number"&&value.at("value")==-42;}
 require(first&&second,"Cross-workbook formulas resolve BIFF source values and apostrophe worksheet names");
 require(book==canonical,"Refreshing a BIFF link leaves canonical source formulas unchanged");
 options.maxExpandedBytes=1;const auto limited=prepareWorkbookLinks(book,nullptr,options);require(limited.filesRead==0&&!limited.diagnostics.empty()&&limited.diagnostics.at(0).at("code")=="external_link_limit","BIFF import respects explicit external expanded-data budget before reading");
 std::cout<<"BIFF source-workbook formula links passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
