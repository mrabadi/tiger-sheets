#include "tigersheets/formatting.hpp"
#include "tigersheets/ooxml.hpp"
#include <pugixml.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace tigersheets;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){try{
 pugi::xml_document styles;require(styles.load_file((std::string(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/accounting-formats.xml").c_str()),"independent native Accounting format fixture opens");
 std::string nativeNone,nativeDollar;for(const auto& format:styles.child("styleSheet").child("numFmts").children("numFmt")){const int id=format.attribute("numFmtId").as_int();if(id==41)nativeNone=format.attribute("formatCode").value();if(id==42)nativeDollar=format.attribute("formatCode").value();}
 const std::string none=R"(_(* #,##0_);_(* \(#,##0\);_(* "-"_);_(@_))";const std::string dollar=R"(_("$"* #,##0_);_("$"* \(#,##0\);_("$"* "-"_);_(@_))";
 require(nativeNone==none&&nativeDollar==dollar,"independent original package records retain literal escapes in Accounting negative sections");require(accountingNumberFormat(0,"")==nativeNone&&accountingNumberFormat(0)==nativeDollar,"new Accounting commands emit the exact source-native escaped negative parentheses");
 const std::string decimal=R"(_("$"* #,##0.00_);_("$"* \(#,##0.00\);_("$"* "-"??_);_(@_))";require(accountingNumberFormat(2)==decimal,"Accounting decimal precision preserves literal parentheses and matching zero padding");require(accountingNumberFormat(1,"€","407",true).find(R"(\(#,##0.0\))")!=std::string::npos,"symbol-after Accounting uses the same literal-parenthesis encoding");
 WorkbookSession session;require(session.apply(0,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","-4640"},{"style",{{"numberFormat",accountingNumberFormat(0,"")}}}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",1},{"input","0"},{"style",{{"numberFormat",accountingNumberFormat(2)}}}}})),"native Accounting source amounts");
 char temporary[]="/tmp/tigersheets-accounting-code-XXXXXX";const auto* created=mkdtemp(temporary);require(created,"temporary workbook directory");const auto path=(std::filesystem::path(created)/"accounting.xlsx").string();writeWorkbook(path,session.snapshot());const auto reopened=readWorkbook(path);const auto& cells=reopened.workbook.at("sheets").at(0).at("cells");require(cells.at(0).at("style").at("numberFormat")==nativeNone&&cells.at(1).at("style").at("numberFormat")==decimal,"native save and reopen preserve exact source Accounting codes");require(formattedValue(-4640,{{"numberFormat",nativeNone}}).find("(4,640)")!=std::string::npos&&formattedValue(0,{{"numberFormat",decimal}}).find('-')!=std::string::npos,"literal escapes preserve negative and zero display behavior");std::filesystem::remove_all(created);std::cout<<"Source-native Accounting codes passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
