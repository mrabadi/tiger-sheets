#include "tigersheets/ooxml.hpp"
#include "tigersheets/calculation.hpp"
#include <pugixml.hpp>
#include <zip.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
std::string bytes(const std::string& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
struct Temporary {std::filesystem::path path;Temporary(){char pattern[]="/tmp/tigersheets-calculated-cache-XXXXXX";const auto* created=mkdtemp(pattern);require(created,"temporary directory");path=created;}~Temporary(){std::filesystem::remove_all(path);}};
Json fixture(){auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"]=Json::array({{{"row",1},{"col",1},{"input","21"}},{{"row",1},{"col",2},{"input","=A1*2"},{"style",{{"bold",true},{"numberFormat","#,##0"}}}},{{"row",1},{"col",3},{"input","=\"Report & review\""}},{{"row",1},{"col",4},{"input","=A1>0"}},{{"row",1},{"col",5},{"input","=1/0"}},{{"row",1},{"col",6},{"input","'=A1*3"}}});return book;}
pugi::xml_node nativeCell(pugi::xml_document& doc,const char* ref){for(auto row:doc.child("worksheet").child("sheetData").children())for(auto cell:row.children())if(std::string(cell.attribute("r").value())==ref)return cell;throw std::runtime_error("native cell absent");}
void typedValues(){Temporary temporary;const auto path=(temporary.path/"typed.xlsx").string();auto book=fixture();const auto calculation=CalculationEngine().calculate(book);require(calculation.at("status")=="ok","real engine calculation completed");writeWorkbookCalculated(path,book,calculation,nullptr);const auto result=readWorkbook(path);pugi::xml_document doc;require(doc.load_string(result.entries.at("xl/worksheets/sheet1.xml").c_str()),"cached worksheet XML");
 const auto number=nativeCell(doc,"B1"),text=nativeCell(doc,"C1"),boolean=nativeCell(doc,"D1"),error=nativeCell(doc,"E1"),literal=nativeCell(doc,"F1");
 require(number.child("f")&&number.child("v")&&number.child("v").text().as_double()==42&&std::string(number.attribute("t").value())!="str","numeric formula result has a native numeric cache");
 require(std::string(text.attribute("t").value())=="str"&&std::string(text.child("v").text().get())=="Report & review","formula text uses native str cache without changing formula");
 require(std::string(boolean.attribute("t").value())=="b"&&std::string(boolean.child("v").text().get())=="1","formula boolean uses native b cache");
 require(std::string(error.attribute("t").value())=="e"&&std::string(error.child("v").text().get())=="#DIV/0!","formula error uses native e cache");
 require(!literal.child("f")&&std::string(literal.attribute("t").value())=="inlineStr","formula-looking literal remains text");require(result.workbook.at("sheets")[0].at("cells")[1].at("input")=="=A1*2"&&result.workbook.at("sheets")[0].at("cells")[1].at("style").at("bold")==true,"cached export preserves formula inputs and styles");
}
void staleResults(){Temporary temporary;const auto path=(temporary.path/"protected.xlsx").string();const auto book=fixture();writeWorkbook(path,book);auto source=readWorkbook(path);const auto original=bytes(path),hash=source.sourceHash;const auto baseline=source.baseline;const auto calculated=CalculationEngine().calculate(book);
 for(int mode=0;mode<5;++mode){auto invalid=calculated;if(mode==0)invalid["revision"]=book.at("revision").get<int>()+1;else if(mode==1)invalid["status"]="cancelled";else if(mode==2){auto& values=invalid["values"];values.erase(std::remove_if(values.begin(),values.end(),[](const Json& value){return value.at("row")==1&&value.at("col")==2;}),values.end());}else if(mode==3){for(auto& value:invalid["values"])if(value.at("col")==2)value["value"]=std::numeric_limits<double>::infinity();}else{for(auto& value:invalid["values"])if(value.at("col")==2)value["value"]="42";}
  bool rejected=false;try{writeWorkbookCalculated(path,book,invalid,&source);}catch(const std::exception&){rejected=true;}require(rejected&&bytes(path)==original&&source.sourceHash==hash&&source.baseline==baseline,"stale incomplete failed or invalid caches reject without changing file or package baseline");
 }
}
void nonConverged(){Temporary temporary;const auto path=(temporary.path/"provisional.xlsx").string();const auto book=fixture();writeWorkbook(path,book);auto source=readWorkbook(path);const auto original=bytes(path),hash=source.sourceHash;const auto baseline=source.baseline;auto provisional=CalculationEngine().calculate(book);provisional["iteration"]={{"converged",false}};require(provisional.at("status")=="ok","provisional fixture isolates convergence from status");bool rejected=false;try{writeWorkbookCalculated(path,book,provisional,&source);}catch(const std::exception&){rejected=true;}require(rejected&&bytes(path)==original&&source.sourceHash==hash&&source.baseline==baseline,"Nonconverged provisional values must reject despite status ok and leave file and source unchanged");}
void preservedFormula(){Temporary temporary;const auto path=(temporary.path/"preserved.xlsx").string();auto book=fixture();writeWorkbook(path,book);auto source=readWorkbook(path);pugi::xml_document doc;require(doc.load_string(source.entries.at("xl/worksheets/sheet1.xml").c_str()),"source XML");auto formula=nativeCell(doc,"B1").child("f");formula.append_attribute("t")="shared";formula.append_attribute("si")="3";formula.append_attribute("ref")="B1:B1";formula.append_attribute("ca")=1;std::ostringstream serialized;doc.save(serialized);const auto xml=serialized.str();int detail=0;auto* archive=zip_open(path.c_str(),0,&detail);require(archive,"fixture archive");auto* member=zip_source_buffer(archive,xml.data(),xml.size(),0);require(member&&zip_file_add(archive,"xl/worksheets/sheet1.xml",member,ZIP_FL_OVERWRITE)>=0&&zip_close(archive)==0,"shared formula fixture");source=readWorkbook(path);book=source.workbook;
 writeWorkbookCalculated(path,book,CalculationEngine().calculate(book),&source);auto saved=readWorkbook(path);pugi::xml_document first;require(first.load_string(saved.entries.at("xl/worksheets/sheet1.xml").c_str()),"unchanged calculated source save");const auto firstCell=nativeCell(first,"B1");require(firstCell.child("v").text().as_double()==42&&std::string(firstCell.child("f").attribute("t").value())=="shared"&&firstCell.child("f").attribute("si").as_int()==3&&firstCell.child("f").attribute("ca").as_bool(),"cache refresh bypasses byte-copy shortcut and preserves shared formula attributes");
 auto edited=source.workbook;edited["revision"]=1;edited["sheets"][0]["cells"][0]["input"]="25";writeWorkbookCalculated(path,edited,CalculationEngine().calculate(edited),&source);saved=readWorkbook(path);pugi::xml_document second;require(second.load_string(saved.entries.at("xl/worksheets/sheet1.xml").c_str()),"edited calculated source save");require(nativeCell(second,"B1").child("v").text().as_double()==50&&source.baseline==edited,"edited save updates caches and package baseline together");
}
}
int main(int argc,char** argv){try{if(argc>1&&std::string(argv[1])=="nonconverged")nonConverged();else{typedValues();staleResults();nonConverged();preservedFormula();}std::cout<<"Calculated XLSX typed caches, rejection atomicity and preserved formula attributes passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
