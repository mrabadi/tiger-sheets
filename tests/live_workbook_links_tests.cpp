#include "tigersheets/workbook_links.hpp"
#include "tigersheets/calculation.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json value(const Json& calculation){for(const auto& cell:calculation.at("values"))if(cell.at("sheet")=="model"&&cell.at("row")==1&&cell.at("col")==1)return cell.at("value");throw std::runtime_error("Missing linked value");}
}
int main(){try{
 WorkbookSession source({{"revision",0},{"sheets",Json::array({{{"id","ops"},{"name","operations"},{"cells",Json::array({{{"row",16},{"col",7},{"input","12112"}}})}}})}});
 const Json model={{"revision",0},{"sheets",Json::array({{{"id","model"},{"name","Model"},{"cells",Json::array({{{"row",1},{"col",1},{"input","='[Otis-10-K-Raw.xls]operations'!G16"}}})}}})},{"links",Json::array({{{"name","Otis-10-K-Raw.xls"},{"path","/tmp/unsaved-otis-source/Otis-10-K-Raw.xls"},{"enabled",true}}})}};
 WorkbookLinkOptions options;options.liveWorkbooks.push_back({"Otis-10-K-Raw.xls","/tmp/unsaved-otis-source/Otis-10-K-Raw.xls",source.captureSnapshot()});
 const auto before=prepareWorkbookLinks(model,nullptr,options);require(before.filesRead==0&&before.diagnostics.empty()&&before.links.at(0).at("state")=="open","Open source workbook resolves without disk refresh");
 require(value(CalculationEngine().calculate(before.snapshot))==12112,"External formula uses the open source workbook's unsaved value");
 std::string error;require(source.apply(0,Json::array({{{"type","set_cell"},{"sheet","ops"},{"row",16},{"col",7},{"input","13000"}}}),&error),error.c_str());
 options.liveWorkbooks[0].workbook=source.captureSnapshot();const auto after=prepareWorkbookLinks(model,nullptr,options);
 require(value(CalculationEngine().calculate(after.snapshot))==13000,"A later capture uses the edited live source revision");
 require(value(CalculationEngine().calculate(before.snapshot))==12112,"An in-flight calculation retains its immutable source capture");
 options.retainedLinks=before.snapshot.at("links");require(value(CalculationEngine().calculate(prepareWorkbookLinks(model,nullptr,options).snapshot))==13000,"Current live source overrides an older retained source cache");
 options.liveWorkbooks.push_back({"Otis-10-K-Raw.xls","/tmp/different-source/Otis-10-K-Raw.xls",std::make_shared<const Json>(WorkbookSession().snapshot())});
 require(value(CalculationEngine().calculate(prepareWorkbookLinks(model,nullptr,options).snapshot))==13000,"Exact source path disambiguates open workbooks sharing a basename");
 auto ambiguous=model;ambiguous["links"][0]["path"]="Otis-10-K-Raw.xls";const auto blocked=prepareWorkbookLinks(ambiguous,nullptr,options);require(!blocked.diagnostics.empty()&&blocked.diagnostics[0].at("code")=="external_link_ambiguous","Ambiguous basename never chooses an arbitrary open source");
 options.maxWorkbooks=0;const auto limited=prepareWorkbookLinks(model,nullptr,options);require(!limited.diagnostics.empty()&&limited.diagnostics[0].at("code")=="external_link_limit","Live source resolution retains workbook count budgets");options.maxWorkbooks=16;
 require(model.at("sheets")[0].at("cells")[0].at("input")=="='[Otis-10-K-Raw.xls]operations'!G16","Link preparation preserves the canonical external formula");
 std::cout<<"Live unsaved source-workbook links passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
