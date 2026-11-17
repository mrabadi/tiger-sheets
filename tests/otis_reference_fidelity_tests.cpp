#include "tigersheets/ooxml.hpp"
#include "tigersheets/calculation.hpp"
#include <cmath>
#include <bit>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>
#include <zip.h>
#include <pugixml.hpp>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
const Json& cell(const Json& sheet,int row,int col){for(const auto& item:sheet.at("cells"))if(item.at("row")==row&&item.at("col")==col)return item;throw std::runtime_error("Reference cell missing");}
WorkbookPackage reference(){return readWorkbook(std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-90-Minute-Case-Study-Otis-Complete.xlsx");}
void themeColors(){
 const auto package=reference();const auto& sheet=package.workbook.at("sheets")[0];
 require(cell(sheet,2,2).at("style").value("fill",std::string())=="#D9D9D9","Otis header theme white with -15% tint must display #D9D9D9");
 require(cell(sheet,2,2).at("style").value("color",std::string())=="#000000","Otis automatic theme font must display black");
 require(cell(sheet,54,3).at("style").value("color",std::string())=="#0000FF","Otis historical numeric inputs retain their explicit blue font");
}
void borders(){
 auto package=reference();const auto& sheet=package.workbook.at("sheets")[0];const Json expected={{"style","thin"},{"color","#000000"}};
 require(cell(sheet,56,2).at("style").value("border",Json::object()).value("top",Json::object())==expected,"Otis total revenue must retain its thin black top border");
 require(cell(sheet,2,2).at("style").value("border",Json::object()).value("bottom",Json::object())==expected,"Otis section header must retain its thin black bottom border");
 const auto path=std::filesystem::temp_directory_path()/("tigersheets-otis-border-"+std::to_string(getpid())+".xlsx");writeWorkbook(path.string(),package.workbook);const auto reopened=readWorkbook(path.string());std::filesystem::remove(path);
 require(cell(reopened.workbook.at("sheets")[0],56,2).at("style").at("border").at("top")==expected,"Fresh native export must author subtotal borders without the source package");
}
void indentation(){
 auto package=reference();const auto& sheet=package.workbook.at("sheets")[0];
 require(cell(sheet,54,2).at("style").value("indent",0)==1,"Otis product label must retain one indent level");
 require(cell(sheet,105,2).at("style").value("indent",0)==2,"Otis cash flow adjustment label must retain two indent levels");
 const auto path=std::filesystem::temp_directory_path()/("tigersheets-otis-indent-"+std::to_string(getpid())+".xlsx");writeWorkbook(path.string(),package.workbook);const auto reopened=readWorkbook(path.string());std::filesystem::remove(path);
 require(cell(reopened.workbook.at("sheets")[0],105,2).at("style").value("indent",0)==2,"Fresh native export must retain cash flow label indentation");
}
void accountingFormats(){
 const auto package=reference();const auto& sheet=package.workbook.at("sheets")[0];
 const auto currency=cell(sheet,54,3).at("style").value("numberFormat",std::string());
 const auto integer=cell(sheet,55,3).at("style").value("numberFormat",std::string());
 require(currency.find('#')!=std::string::npos&&currency.find('$')!=std::string::npos&&currency.find('(')!=std::string::npos,"Otis first financial row must import built-in dollar accounting format 42");
 require(integer.find('#')!=std::string::npos&&integer.find('$')==std::string::npos&&integer.find('(')!=std::string::npos,"Otis remaining financial rows must import built-in accounting format 41");
}
void sheetDefaults(){
 const auto package=reference();const auto& sheet=package.workbook.at("sheets")[0];
 require(sheet.value("defaultRowHeight",0.0)==15.75,"Otis default row height must import as 15.75 points");
 require(!sheet.value("showGridLines",true),"Otis gridlines must be hidden");
 require(sheet.value("defaultStyle",Json::object()).value("fontSize",0.0)==12.0,"Otis whole-sheet column font must import as Calibri 12");
 require(sheet.at("defaultStyle").value("fontFamily",std::string())=="Calibri","Otis default font family must import");
 const auto path=std::filesystem::temp_directory_path()/("tigersheets-otis-defaults-"+std::to_string(getpid())+".xlsx");writeWorkbook(path.string(),package.workbook);const auto reopened=readWorkbook(path.string());std::filesystem::remove(path);const auto& restored=reopened.workbook.at("sheets")[0];
 if(restored.value("defaultRowHeight",0.0)!=15.75||restored.value("showGridLines",true)||restored.value("defaultStyle",Json::object()).value("fontSize",0.0)!=12.0)std::cerr<<Json{{"defaultRowHeight",restored.value("defaultRowHeight",Json())},{"showGridLines",restored.value("showGridLines",Json())},{"defaultStyle",restored.value("defaultStyle",Json())}}.dump()<<'\n';
 require(restored.value("defaultRowHeight",0.0)==15.75&&!restored.value("showGridLines",true)&&restored.value("defaultStyle",Json::object()).value("fontSize",0.0)==12.0,"Fresh native export must retain all Otis sparse sheet defaults");
}
void authorDefaults(){
 WorkbookSession session;const auto original=session.snapshot();const auto id=original.at("sheets")[0].at("id");std::string error;
 const Json operation={{"type","set_sheet_layout"},{"sheet",id},{"defaultRowHeight",15.75},{"showGridLines",false},{"defaultStyle",{{"fontFamily","Calibri"},{"fontSize",12}}}};
 const bool applied=session.apply(0,Json::array({operation}),&error);require(applied,("Sparse whole-sheet formatting must be an ordinary layout command: "+error).c_str());const auto expected=session.snapshot().at("sheets");
 require(expected[0].at("cells").empty()&&expected[0].at("defaultStyle").at("fontSize")==12,"Whole-sheet defaults must not allocate blank cells");
 require(session.undo()&&session.snapshot().at("sheets")==original.at("sheets"),"Whole-sheet defaults must undo in one transaction");
 require(session.redo()&&session.snapshot().at("sheets")==expected,"Whole-sheet defaults must redo in one transaction");
 const auto before=session.snapshot();for(const auto& patch:Json::array({Json{{"defaultRowHeight",0}},Json{{"defaultRowHeight",500}},Json{{"showGridLines","no"}},Json{{"defaultStyle",Json::array()}},Json{{"defaultStyle",{{"fontSize",0}}}}})){auto invalid=patch;invalid["type"]="set_sheet_layout";invalid["sheet"]=id;require(!session.apply(before.at("revision"),Json::array({invalid}),&error)&&session.snapshot()==before,"Invalid sheet defaults must reject atomically");}
 const auto path=std::filesystem::temp_directory_path()/("tigersheets-otis-authored-defaults-"+std::to_string(getpid())+".xlsx");writeWorkbook(path.string(),session.snapshot());const auto reopened=readWorkbook(path.string());std::filesystem::remove(path);const auto& sheet=reopened.workbook.at("sheets")[0];require(sheet.at("cells").empty()&&sheet.at("defaultStyle").at("fontSize")==12&&!sheet.at("showGridLines").get<bool>(),"Sparse formatting on an empty worksheet must persist in native XLSX");
}
void preserveColumnStyles(){
 const auto path=(std::filesystem::temp_directory_path()/("tigersheets-otis-column-styles-"+std::to_string(getpid())+".xlsx")).string();struct Cleanup{std::string path;~Cleanup(){std::filesystem::remove(path);}}cleanup{path};
 auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"]=Json::array({{{"row",1},{"col",1},{"input","1"},{"style",{{"fill","#ffff00"},{"numberFormat","0.00"}}}},{{"row",1},{"col",2},{"input","2"},{"style",{{"fill","#00ff00"},{"numberFormat","0%"}}}}});writeWorkbook(path,book);auto initial=readWorkbook(path);
 pugi::xml_document worksheet,styles;require(worksheet.load_string(initial.entries.at("xl/worksheets/sheet1.xml").c_str())&&styles.load_string(initial.entries.at("xl/styles.xml").c_str()),"native column-style fixture parses");const auto data=worksheet.child("worksheet").child("sheetData");const int firstStyle=data.child("row").child("c").attribute("s").as_int(),secondStyle=data.child("row").child("c").next_sibling("c").attribute("s").as_int();auto cols=worksheet.child("worksheet").insert_child_before("cols",data);auto first=cols.append_child("col");first.append_attribute("min")=1;first.append_attribute("max")=1;first.append_attribute("style")=firstStyle;auto second=cols.append_child("col");second.append_attribute("min")=2;second.append_attribute("max")=16384;second.append_attribute("style")=secondStyle;
 auto nth=[](pugi::xml_node parent,int index){auto node=parent.first_child();for(int current=0;node&&current<index;++current)node=node.next_sibling();return node;};nth(styles.child("styleSheet").child("cellXfs"),firstStyle).append_child("alignment").append_attribute("textRotation")=45;
 auto member=[&](const char* name,const pugi::xml_document& doc){std::ostringstream bytes;doc.save(bytes);const auto value=bytes.str();int error=0;auto* archive=zip_open(path.c_str(),0,&error);require(archive,"native column fixture ZIP opens");auto* source=zip_source_buffer(archive,value.data(),value.size(),0);require(source&&zip_file_add(archive,name,source,ZIP_FL_OVERWRITE)>=0&&zip_close(archive)==0,"native column fixture ZIP changes");};member("xl/worksheets/sheet1.xml",worksheet);member("xl/styles.xml",styles);
 auto package=readWorkbook(path);require(!package.workbook.at("sheets")[0].contains("defaultStyle"),"differing native column styles are not flattened into one imported default");auto changed=package.workbook;changed["sheets"][0]["defaultStyle"]={{"fontSize",24}};for(auto& item:changed["sheets"][0]["cells"])item["style"]["fontSize"]=24;writeWorkbook(path,changed,&package);const auto reopened=readWorkbook(path);require(worksheet.load_string(reopened.entries.at("xl/worksheets/sheet1.xml").c_str())&&styles.load_string(reopened.entries.at("xl/styles.xml").c_str()),"saved native column style package parses");
 for(auto column:worksheet.child("worksheet").child("cols").children("col")){const bool a=column.attribute("min").as_int()==1;const auto root=styles.child("styleSheet"),xf=nth(root.child("cellXfs"),column.attribute("style").as_int());const auto font=nth(root.child("fonts"),xf.attribute("fontId").as_int()),fill=nth(root.child("fills"),xf.attribute("fillId").as_int());require(font.child("sz").attribute("val").as_double()==24,"whole-sheet partial font change applies to each native column style");auto rgb=std::string(fill.child("patternFill").child("fgColor").attribute("rgb").value());std::transform(rgb.begin(),rgb.end(),rgb.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});require(rgb==(a?"FFFFFF00":"FF00FF00"),"whole-sheet partial font change preserves distinct native column fills for unstored cells");std::string number;for(auto format:root.child("numFmts").children("numFmt"))if(format.attribute("numFmtId").as_int()==xf.attribute("numFmtId").as_int())number=format.attribute("formatCode").value();require(number==(a?"0.00":"0%"),"whole-sheet partial font change preserves distinct native column number formats");if(a)require(xf.child("alignment").attribute("textRotation").as_int()==45,"whole-sheet partial font change preserves unmodeled native column alignment attributes");}
 require(reopened.workbook.at("sheets")[0].at("cells").size()==2,"preserving native column defaults does not allocate unstored cells");
}
void uniformPartialDefaultColumns(){
 auto book=WorkbookSession().snapshot();book["sheets"][0]["defaultStyle"]={{"fontSize",24}};book["sheets"][0]["columnLayout"]=Json::array({{{"index",2},{"size",18}}});const auto path=(std::filesystem::temp_directory_path()/("tigersheets-otis-uniform-column-font-"+std::to_string(getpid())+".xlsx")).string();struct Cleanup{std::string path;~Cleanup(){std::filesystem::remove(path);}}cleanup{path};writeWorkbook(path,book);const auto reopened=readWorkbook(path);const auto& sheet=reopened.workbook.at("sheets")[0];require(sheet.value("defaultStyle",Json::object()).value("fontSize",0.0)==24,"width-only native columns and gap columns share one uniform partial font default");require(sheet.at("cells").empty(),"uniform partial font defaults remain sparse with native column widths");
}
void calculatedReference(){
 const auto package=reference();const auto result=CalculationEngine().calculate(package.workbook);require(result.at("diagnostics").empty(),"Published Otis formulas must calculate without engine errors");
 std::ifstream input(std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/completed-workbook.snapshot.json");Json expected;input>>expected;std::size_t checked=0;
 for(const auto& original:expected.at("sheets")[0].at("cells")){if(!original.contains("formula"))continue;const int row=original.at("row"),col=original.at("column");const auto expectedValue=std::stod(original.at("raw_value").get<std::string>());const Json* actual=nullptr;for(const auto& value:result.at("values"))if(value.at("row")==row&&value.at("col")==col){actual=&value;break;}
  require(actual&&actual->at("type")=="number",("Otis formula must calculate a number: "+original.at("address").get<std::string>()).c_str());const auto value=actual->at("value").get<double>();require(std::abs(value-expectedValue)<=std::max(1e-8,std::abs(expectedValue)*1e-10),("Otis result differs from published Excel cache: "+original.at("address").get<std::string>()).c_str());if(row==100&&col>=6&&col<=10)require(value==0.0,("Otis balance check must be exactly zero and display the accounting zero section: "+original.at("address").get<std::string>()).c_str());if(row==126&&col>=6&&col<=10)require(std::bit_cast<std::uint64_t>(value)==std::bit_cast<std::uint64_t>(expectedValue),("Otis cash-flow residual must retain exact published bits: "+original.at("address").get<std::string>()).c_str());++checked;
 }
 require(checked==515,"Every published Otis formula cache must be checked");std::cout<<"Compared all "<<checked<<" Otis formula results with original Excel caches\n";
}
Json semanticCells(const Json& sheet){auto cells=sheet.at("cells");for(auto& item:cells)if(item.contains("style"))item["style"].erase("ooxmlStyleId");return cells;}
void completeNativeRoundtrip(){
 auto package=reference();const auto expected=semanticCells(package.workbook.at("sheets")[0]);
 const auto path=std::filesystem::temp_directory_path()/("tigersheets-otis-all-cells-"+std::to_string(getpid())+".xlsx");writeWorkbook(path.string(),package.workbook);const auto fresh=readWorkbook(path.string());
 require(semanticCells(fresh.workbook.at("sheets")[0])==expected,"All 777 Otis cells must retain exact inputs and resolved style semantics in a fresh native export");
 auto edited=package.workbook;for(auto& item:edited["sheets"][0]["cells"])if(item.at("row")==54&&item.at("col")==3)item["input"]="5649";writeWorkbook(path.string(),edited,&package);const auto restored=readWorkbook(path.string());std::filesystem::remove(path);
 require(semanticCells(restored.workbook.at("sheets")[0])==semanticCells(edited.at("sheets")[0]),"An ordinary Otis cell edit must preserve every original cell style and all other inputs on native save");
}
}
int main(int argc,char** argv){try{const std::string test=argc>1?argv[1]:"all";if(test=="theme")themeColors();else if(test=="border")borders();else if(test=="indent")indentation();else if(test=="accounting")accountingFormats();else if(test=="defaults")sheetDefaults();else if(test=="author_defaults")authorDefaults();else if(test=="uniform_column_default")uniformPartialDefaultColumns();else if(test=="column_styles")preserveColumnStyles();else if(test=="calculation")calculatedReference();else if(test=="roundtrip")completeNativeRoundtrip();else if(test=="all"){themeColors();borders();indentation();accountingFormats();sheetDefaults();authorDefaults();preserveColumnStyles();uniformPartialDefaultColumns();calculatedReference();completeNativeRoundtrip();}else throw std::runtime_error("Unknown fidelity test");std::cout<<"Otis reference "<<test<<" passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
