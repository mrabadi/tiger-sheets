#include "tigersheets/ooxml.hpp"
#include "tigersheets/financial_models.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 char pattern[]="/tmp/tigersheets-financial-persistence-XXXXXX";const auto* temporary=mkdtemp(pattern);require(temporary,"private fixture directory");const std::filesystem::path directory(temporary);
 auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"].push_back({{"row",1},{"col",1},{"input","0.1"}});
 const Json definition={{"version",1},{"id","base-assumptions"},{"name","Base assumptions"},{"kind","assumption_set"},{"cells",Json::array({{{"sheet","sheet-1"},{"row",1},{"col",1},{"type","number"},{"value",0.1}}})}};
 book["financialModels"]=Json::array({definition});const auto first=(directory/"assumptions.xlsx").string();writeWorkbook(first,book);
 auto reopened=readWorkbook(first);require(reopened.workbook.contains("financialModels"),"saved financial assumption sets must reopen as editable definitions");require(reopened.workbook.at("financialModels")==book.at("financialModels"),"assumption addresses and typed values survive first reopen");
 const auto second=(directory/"assumptions-resaved.xlsx").string();writeWorkbook(second,reopened.workbook,&reopened);const auto again=readWorkbook(second);require(again.workbook.at("financialModels")==book.at("financialModels"),"saved financial definitions survive two native save/reopen cycles");require(again.workbook.at("sheets")[0].at("cells")[0].at("input")=="0.1","saved configuration does not change source inputs");
 const Json forecast={{"version",1},{"id","forecast"},{"name","Revenue forecast"},{"kind","forecast"},{"periods",3},{"drivers",Json::array({{{"name","Revenue"},{"base",100},{"growth",0.1}}})},{"output",{{"sheet","sheet-1"},{"row1",4},{"col1",1}}}};
 CalculationContext context;context.snapshot=book;context.generation=3;const auto generated=generateFinancialModel(book,forecast,context);WorkbookSession session(book);std::string error;const bool applied=session.apply(0,financialModelOperations(book,generated),&error);require(applied,error.c_str());const auto reportPath=(directory/"forecast.xlsx").string();writeWorkbook(reportPath,session.snapshot());auto report=readWorkbook(reportPath);WorkbookSession reportSession(report.workbook);require(reportSession.cellAt("sheet-1",5,4)->value("financialModelOwner",std::string())=="forecast","reopening generated financial reports restores managed output ownership");require(!reportSession.apply(0,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",5},{"col",4},{"input","999"}}}),&error),"reopened financial result values remain protected from generic editing");
 auto custom=book;custom["sheets"][0]["id"]="custom-inputs";custom["financialModels"][0]["cells"][0]["sheet"]="custom-inputs";const auto customPath=(directory/"custom-identities.xlsx").string();writeWorkbook(customPath,custom);const auto customReopened=readWorkbook(customPath);require(customReopened.workbook.at("sheets")[0].at("id")=="custom-inputs"&&customReopened.workbook.at("financialModels")[0].at("cells")[0].at("sheet")=="custom-inputs","saved financial definitions retain stable worksheet identities through native XLSX roundtrip");
 std::filesystem::remove_all(directory);std::cout<<"financial model persistence tests passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
