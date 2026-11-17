#include "tigersheets/ooxml.hpp"
#include "tigersheets/workbook_links.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/external_links_ooxml.hpp"
#include <pugixml.hpp>
#include <zip.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json calculatedValue(const Json& result){for(const auto& cell:result.at("values"))if(cell.at("row")==1&&cell.at("col")==1)return cell.at("value");throw std::runtime_error("Missing external formula result");}
}
int main(int argc,char** argv){const auto output=(std::filesystem::temp_directory_path()/("tigersheets-native-external-"+std::to_string(getpid())+".xlsx")).string();try{
 const Json cache={{"revision",0},{"sheets",Json::array({{{"id","ops"},{"name","Owner's data"},{"cells",Json::array({{{"row",1},{"col",1},{"input","'00123"}},{{"row",1},{"col",2},{"input","TRUE"}},{{"row",16},{"col",7},{"input","=6000+6112"}}})}}})}};
 const Json book={{"revision",0},{"sheets",Json::array({{{"id","model"},{"name","Model"},{"cells",Json::array({{{"row",1},{"col",1},{"input","='[Source Data.xls]Owner''s data'!$G$16*2"}},{{"row",2},{"col",1},{"input","=\"[Source Data.xls]Owner's data!G16\""}}})}}})},{"links",Json::array({{{"name","Source Data.xls"},{"path","/tmp/nonexistent-source/Source Data.xls"},{"enabled",true},{"cachedWorkbook",cache}}})}};
 const auto original=book;WorkbookPackage saved;writeWorkbook(output,book,&saved);
 require(saved.entries.contains("xl/externalLinks/tigersheetsLink1.xml"),"Native save creates an OOXML externalBook part for authored workbook links");
 pugi::xml_document workbook,relations,worksheet,external,paths;workbook.load_string(saved.entries.at("xl/workbook.xml").c_str());relations.load_string(saved.entries.at("xl/_rels/workbook.xml.rels").c_str());worksheet.load_string(saved.entries.at("xl/worksheets/sheet1.xml").c_str());external.load_string(saved.entries.at("xl/externalLinks/tigersheetsLink1.xml").c_str());paths.load_string(saved.entries.at("xl/externalLinks/_rels/tigersheetsLink1.xml.rels").c_str());
 require(workbook.child("workbook").child("externalReferences").child("externalReference"),"Native workbook declares its external reference relationship");
 require(std::string(worksheet.select_node("//c[@r='A1']/f").node().text().as_string())=="'[1]Owner''s data'!$G$16*2","Native formulas reference the externalBook index and preserve quoted sheet names");
 require(std::string(worksheet.select_node("//c[@r='A2']/f").node().text().as_string())=="\"[Source Data.xls]Owner's data!G16\"","External normalization leaves formula string literals unchanged");
 require(std::string(paths.child("Relationships").child("Relationship").attribute("Target").value())=="file:///tmp/nonexistent-source/Source%20Data.xls","Native external source relationship uses an escaped local file URI");
 require(std::string(external.select_node("//sheetName").node().attribute("val").value())=="Owner's data","Native cache stores exact source worksheet names");
 require(std::string(external.select_node("//cell[@r='G16']/v").node().text().as_string())=="12112","Native cache contains the calculated external source value");
 require(std::string(external.select_node("//cell[@r='A1']").node().attribute("t").value())=="str"&&std::string(external.select_node("//cell[@r='A1']/v").node().text().as_string())=="00123","Native external cache preserves literal numeric-looking strings");
 require(std::string(external.select_node("//cell[@r='B1']").node().attribute("t").value())=="b","Native external cache preserves booleans");
 require(book==original,"Native serialization leaves canonical filename formulas and source snapshots unchanged");
 if(argc>1&&std::string(argv[1])=="prefix"){
  external.document_element().remove_attribute("xmlns");external.document_element().append_attribute("xmlns:x")="http://schemas.openxmlformats.org/spreadsheetml/2006/main";const auto qualify=[&](const auto& self,pugi::xml_node node)->void{for(auto element:node.children())if(element.type()==pugi::node_element){element.set_name(("x:"+std::string(element.name())).c_str());self(self,element);}};qualify(qualify,external);auto nativeBook=external.document_element().first_child();auto definitions=nativeBook.insert_child_before("x:definedNames",nativeBook.last_child());auto definition=definitions.append_child("x:definedName");definition.append_attribute("name")="OriginalName";definition.append_attribute("refersTo")="'Owner''s data'!$G$16";std::ostringstream prefixed;external.save(prefixed,"",pugi::format_raw);saved.entries["xl/externalLinks/tigersheetsLink1.xml"]=prefixed.str();
  writeNativeExternalLinks(saved.entries,book,saved.sheetParts);pugi::xml_document refreshed;require(refreshed.load_string(saved.entries.at("xl/externalLinks/tigersheetsLink1.xml").c_str()),"read refreshed prefixed external source part");const auto externalBook=refreshed.document_element().first_child();require(std::string(externalBook.first_child().name())=="x:sheetNames","refreshed native externalBook retains schema order with sheetNames before preserved definedNames");require(externalBook.child("x:definedNames").child("x:definedName"),"refresh preserves native external defined names");for(const auto* name:{"x:sheetNames/x:sheetName","x:sheetDataSet/x:sheetData/x:row/x:cell/x:v"})require(externalBook.select_node(name),"refreshed native external cache descendants retain their bound spreadsheet namespace prefix");std::cout<<"Prefixed external source cache passed\n";
 }
 auto reopened=readWorkbook(output);require(reopened.workbook.at("sheets")[0].at("cells")[0].at("input")==book.at("sheets")[0].at("cells")[0].at("input"),"Reopened formula bar retains the source filename while native XML stores its numeric external index");auto prepared=prepareWorkbookLinks(reopened.workbook,&reopened);require(prepared.filesRead==0&&calculatedValue(CalculationEngine().calculate(prepared.snapshot))==24224,"Saved external cache recalculates correctly without reading the missing source file");
 if(argc>1&&std::string(argv[1])=="structure"){
  auto authored=book;authored["sheets"][0]["defaultStyle"]={{"fontFamily","Calibri"},{"fontSize",12}};authored["sheets"][0]["columnLayout"]=Json::array({{{"index",1},{"size",11.14}}});authored["sheets"][0]["cells"].push_back({{"row",5},{"col",2},{"input","=A1+1"}});
  WorkbookPackage native;writeWorkbook(output,authored,&native);native=readWorkbook(output);const auto externalPart=native.entries.at("xl/externalLinks/tigersheetsLink1.xml");
  preflightWorksheetStructure(native,"model");WorkbookSession edited(native.workbook);
  require(edited.apply(edited.snapshot().at("revision").get<std::uint64_t>(),Json::array({{{"type","insert_rows"},{"sheet","model"},{"index",1},{"count",2}},{{"type","insert_columns"},{"sheet","model"},{"index",1},{"count",1}}})),"insert local rows and columns after saving an external formula");
  require(edited.cellAt("model",3,2)->at("input")==book["sheets"][0]["cells"][0]["input"],"local structural edits preserve absolute external source coordinates");
  require(edited.cellAt("model",7,3)->at("input")=="=B3+1","local dependent formulas rebase while external source coordinates stay unchanged");
  writeWorkbook(output,edited.snapshot(),&native);const auto changed=readWorkbook(output);
  require(changed.entries.at("xl/externalLinks/tigersheetsLink1.xml")==externalPart,"saved local structural edits preserve the external workbook source cache");
  require(changed.workbook.at("sheets")[0].at("defaultStyle").at("fontSize")==12,"saved local structure edits retain the declared default font");
  auto unsupported=native;unsupported.entries["xl/externalLinks/tigersheetsLink1.xml"]="<externalLink xmlns='http://schemas.openxmlformats.org/spreadsheetml/2006/main'><ddeLink ddeService='Other' ddeTopic='Source'/></externalLink>";bool rejected=false;try{preflightWorksheetStructure(unsupported,"model");}catch(const std::exception&){rejected=true;}require(rejected,"unsupported non-workbook external link types remain protected during structural edits");
  unsupported=native;pugi::xml_document nonuniform;nonuniform.load_string(unsupported.entries.at(unsupported.sheetParts.at("model")).c_str());nonuniform.document_element().child("cols").last_child().attribute("style")=0;std::ostringstream modified;nonuniform.save(modified,"",pugi::format_raw);unsupported.entries[unsupported.sheetParts.at("model")]=modified.str();rejected=false;try{preflightWorksheetStructure(unsupported,"model");}catch(const std::exception&){rejected=true;}require(rejected,"nonuniform native column styles remain protected until their structural rebasing is supported");
  std::filesystem::remove(output);std::cout<<"External workbook local structure preservation passed\n";return 0;
 }
 int error=0;auto* archive=zip_open(output.c_str(),0,&error);require(archive,"Open saved native package for independent metadata-removal check");const auto index=zip_name_locate(archive,"tigersheets/metadata.json",0);require(index>=0&&zip_delete(archive,index)==0&&zip_close(archive)==0,"Remove optional app metadata");
 reopened=readWorkbook(output);prepared=prepareWorkbookLinks(reopened.workbook,&reopened);require(prepared.filesRead==0&&calculatedValue(CalculationEngine().calculate(prepared.snapshot))==24224,"Native external relationships/formulas/caches are sufficient after removing all app metadata");
 std::filesystem::remove(output);std::cout<<"Native external workbook authoring passed\n";
}catch(const std::exception& error){std::filesystem::remove(output);std::cerr<<error.what()<<'\n';return 1;}}
