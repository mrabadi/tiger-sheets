#include "tigersheets/ooxml.hpp"
#include "tigersheets/workbook_links.hpp"
#include "tigersheets/calculation.hpp"
#include <pugixml.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Directory {std::filesystem::path path;Directory(){char pattern[]="/tmp/tigersheets-link-portability-XXXXXX";auto* created=mkdtemp(pattern);if(!created)throw std::runtime_error("Cannot create portable-link fixture");path=created;}~Directory(){std::error_code error;std::filesystem::remove_all(path,error);}};
Json sourceBook(int number){return {{"revision",0},{"sheets",Json::array({{{"id","source"},{"name","Data"},{"cells",Json::array({{{"row",1},{"col",1},{"input",std::to_string(number)}}})}}})}};}
std::string target(const WorkbookPackage& package){for(const auto& [name,bytes]:package.entries)if(name.starts_with("xl/externalLinks/_rels/")&&name.ends_with(".rels")){pugi::xml_document xml;require(xml.load_string(bytes.c_str()),"read external path XML");return xml.document_element().first_child().attribute("Target").value();}throw std::runtime_error("Missing native external path");}
Json firstValue(const Json& result){for(const auto& value:result.at("values"))if(value.at("row")==1&&value.at("col")==1)return value.at("value");throw std::runtime_error("Missing calculation result");}
}
int main(){try{
 Directory fixture;const auto pair=fixture.path/"original pair",moved=fixture.path/"relocated pair",outside=fixture.path/"model elsewhere";for(const auto& dir:{pair,moved,outside})std::filesystem::create_directories(dir);
 const auto source=pair/"Source Data.xlsx",model=pair/"Model.xlsx";writeWorkbook(source.string(),sourceBook(21));
 const Json book={{"revision",0},{"sheets",Json::array({{{"id","model"},{"name","Model"},{"cells",Json::array({{{"row",1},{"col",1},{"input","='[Source Data.xlsx]Data'!A1*2"}}})}}})},{"links",Json::array({{{"name","Source Data.xlsx"},{"path",source.string()},{"enabled",true},{"cachedWorkbook",sourceBook(21)}}})}};
 const auto unchanged=book;WorkbookPackage saved;writeWorkbook(model.string(),book,&saved);
 require(target(saved)=="Source%20Data.xlsx","same-folder native source paths are relative escaped filenames so the pair can move together");
 auto original=readWorkbook(model.string());require(original.workbook.at("links")[0].at("path")=="Source Data.xlsx"&&!original.workbook.at("links")[0].value("uriEncoded",true),"optional app metadata preserves a relative unescaped source path rather than overriding the portable native relationship");
 require(book==unchanged,"native path normalization never changes the caller's formula or semantic workbook");
 for(const auto& name:{"Model.xlsx","Source Data.xlsx"})std::filesystem::copy_file(pair/name,moved/name);
 writeWorkbook(source.string(),sourceBook(99));auto relocated=readWorkbook((moved/"Model.xlsx").string());WorkbookLinkOptions refresh;refresh.refresh=true;const auto resolved=prepareWorkbookLinks(relocated.workbook,&relocated,refresh);
 require(resolved.filesRead==1&&firstValue(CalculationEngine().calculate(resolved.snapshot))==42,"a copied pair refreshes the relocated real source, not the original absolute source or only its saved cache");
 const auto elsewhere=outside/"Model.xlsx";writeWorkbook(elsewhere.string(),relocated.workbook,&relocated);
 const auto expected="file://"+(moved/"Source Data.xlsx").string();std::string escaped;for(char c:expected)escaped+=c==' '?"%20":std::string(1,c);
 require(target(relocated)==escaped,"Save As to another folder retains the actual source with an absolute escaped URI");
 const auto copied=readWorkbook(elsewhere.string());const auto copiedResult=prepareWorkbookLinks(copied.workbook,&copied,refresh);require(copiedResult.filesRead==1&&firstValue(CalculationEngine().calculate(copiedResult.snapshot))==42,"unchanged native Save As does not silently retarget a relative source into the destination folder");
 std::cout<<"Native external workbook portability passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
