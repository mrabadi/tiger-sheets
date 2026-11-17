#include "tigersheets/session.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/biff.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
const Json& cell(const Json& book,int row,int col){for(const auto& entry:book.at("sheets")[0].at("cells"))if(entry.at("row")==row&&entry.at("col")==col)return entry;throw std::runtime_error("Missing inherited blank cell style");}
double width(const Json& book,int column){for(const auto& entry:book.at("sheets")[0].at("columnLayout"))if(entry.at("index")==column)return entry.at("size");return 8.43;}
}
int main(){const auto path=(std::filesystem::temp_directory_path()/("tigersheets-insert-inherit-"+std::to_string(getpid())+".xls")).string();try{
 const Json book={{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","cash flows"},{"defaultStyle",{{"fontFamily","Arial"},{"fontSize",12}}},{"columnLayout",Json::array({{{"index",1},{"size",70.5}},{{"index",2},{"size",16.75}}})},{"cells",Json::array({{{"row",1},{"col",1},{"input","Cash flow statement"},{"style",{{"bold",true},{"fontSize",14},{"fill","#334455"}}}},{{"row",2},{"col",1},{"input","Revenue"},{"style",{{"alignment","left"},{"numberFormat","0.0"}}}},{{"row",1},{"col",2},{"input","2019"}},{{"row",1},{"col",3},{"input","2020"}},{{"row",1},{"col",4},{"input","2021"}}})}}})}};
 WorkbookSession session(book);const auto baseline=session.snapshot();require(session.apply(0,Json::array({{{"type","insert_columns"},{"sheet","s"},{"index",2},{"count",1}}})),"insert whole column B through the real structural transaction");const auto inserted=session.snapshot();require(width(inserted,2)==70.5&&width(inserted,3)==16.75,"a newly inserted B inherits the wide left A column while existing B width moves to C");require(cell(inserted,1,2).at("input")==""&&cell(inserted,1,2).at("style")==cell(book,1,1).at("style")&&cell(inserted,2,2).at("style")==cell(book,2,1).at("style"),"blank inserted cells inherit left-column formatting without copying values");require(cell(inserted,1,3).at("input")=="2019"&&cell(inserted,1,4).at("input")=="2020"&&cell(inserted,1,5).at("input")=="2021","the original three historical-year columns shift together");
 writeBiffWorkbook(path,inserted);const auto reopened=readWorkbook(path).workbook;require(width(reopened,2)==70.5&&cell(reopened,1,2).at("input")==""&&cell(reopened,1,2).at("style").at("bold")==true&&cell(reopened,1,2).at("style").at("fill")=="#334455","native BIFF save/reopen retains inherited column width and blank-cell formats");require(session.undo()&&session.snapshotView().at("sheets")==baseline.at("sheets")&&!session.canUndo(),"one Undo restores all inherited and shifted formatting, dimensions and values");
 require(session.apply(session.snapshotView().at("revision"),Json::array({{{"type","insert_columns"},{"sheet","s"},{"index",1},{"count",2}}})),"insert two columns at the left boundary");const auto left=session.snapshot();require(width(left,1)==70.5&&width(left,2)==70.5&&cell(left,1,1).at("input")==""&&cell(left,1,2).at("style")==cell(book,1,1).at("style")&&cell(left,1,3).at("input")=="Cash flow statement","insertion at A inherits the original right column's width and formats for each new blank column");std::filesystem::remove(path);std::cout<<"Column insertion inheritance passed\n";
 }catch(const std::exception& error){std::filesystem::remove(path);std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
