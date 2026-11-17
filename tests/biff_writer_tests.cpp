#include "tigersheets/biff.hpp"
#include "tigersheets/ooxml.hpp"
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
const Json& cell(const Json& sheet,int row,int col){for(const auto& value:sheet.at("cells"))if(value.at("row")==row&&value.at("col")==col)return value;throw std::runtime_error("Missing BIFF roundtrip cell");}
std::string bytes(const std::string& path){std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};}
}
int main(int argc,char** argv){const auto path=(std::filesystem::temp_directory_path()/("tigersheets-biff-writer-"+std::to_string(getpid())+".xls")).string();try{
 const std::string continued(9000,'x');
 const Json book={{"revision",0},{"sheets",Json::array({{{"id","a"},{"name","Café 東京"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Unicode Ω 東京"}},{{"row",1},{"col",2},{"input","1234.5"}},{{"row",2},{"col",1},{"input","TRUE"}},{{"row",2},{"col",2},{"input","FALSE"}},{{"row",3},{"col",1},{"input","'00123"}},{{"row",3},{"col",2},{"input","'=literal"}},{{"row",4},{"col",1},{"input",continued}}})}},{{"id","b"},{"name","Owner's data"},{"cells",Json::array({{{"row",1},{"col",1},{"input","-42"}}})}}})}};
 writeBiffWorkbook(path,book);require(isCompoundWorkbook(bytes(path)),"Legacy save creates a real CFB/BIFF8 file rather than a ZIP with an XLS extension");
 const auto reopened=readWorkbook(path).workbook;require(reopened.at("sheets").size()==2,"BIFF writer retains worksheet count");
 for(std::size_t i=0;i<book.at("sheets").size();++i){const auto& before=book.at("sheets")[i];const auto& after=reopened.at("sheets")[i];require(before.at("name")==after.at("name"),"BIFF writer preserves Unicode and apostrophe worksheet names");for(const auto& expected:before.at("cells"))require(cell(after,expected.at("row"),expected.at("col")).at("input")==expected.at("input"),"BIFF writer preserves typed numeric/boolean/text cells and continued long SST strings");}
 if(argc>1&&std::string(argv[1])=="styles"){
  auto styled=readWorkbook(std::string(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/independent.xls").workbook;styled["sheets"][1]["defaultStyle"]["fontFamily"]="DejaVu Sans";styled["sheets"][1]["defaultStyle"]["fontSize"]=17;
  styled["sheets"][1]["cells"].push_back({{"row",3},{"col",3},{"input","9.125"},{"style",{{"numberFormat","0.0"}}}});
  writeBiffWorkbook(path,styled);const auto roundtrip=readWorkbook(path).workbook;
  for(std::size_t i=0;i<styled.at("sheets").size();++i){const auto& before=styled.at("sheets")[i];const auto& after=roundtrip.at("sheets")[i];require(after.at("defaultStyle")==before.at("defaultStyle"),"BIFF saves each worksheet's own sparse default font/style");
   for(const auto& expected:before.at("cells")){const auto& actual=cell(after,expected.at("row"),expected.at("col"));require(actual.at("input")==expected.at("input"),"Styled BIFF cells retain inputs");auto effective=before.at("defaultStyle");if(expected.contains("style"))effective.update(expected.at("style"));require(actual.at("style")==effective,"BIFF preserves effective fonts, palette RGB, fills, number formats, alignment, wrap, indent, and borders for populated/formattedblank cells");}
   for(const auto* key:{"rowLayout","columnLayout"})for(const auto& entry:before.value(key,Json::array())){bool found=false;for(const auto& candidate:after.value(key,Json::array()))if(candidate.at("index")==entry.at("index")){found=true;for(const auto* field:{"size","hidden"})if(entry.contains(field))require(candidate.value(field,field==std::string("hidden")?Json(false):Json())==entry.at(field),"BIFF preserves explicit row/column dimensions and hidden flags");}require(found,"BIFF preserves every explicit dimension");}
   require(after.value("mergedRanges",Json::array())==before.value("mergedRanges",Json::array())&&after.value("showGridLines",true)==before.value("showGridLines",true),"BIFF preserves merges and gridline visibility");
  }
  std::cout<<"Native BIFF8 style/layout writer passed\n";
 }
 if(argc>1&&std::string(argv[1])=="source"){
  auto package=readWorkbook(std::string(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/financial-source.xls");auto edited=package.workbook;edited["sheets"][32]["name"]="operations";for(auto& entry:edited["sheets"][32]["cells"])if(entry.at("row")==16&&entry.at("col")==7)entry["input"]="12112";
  writeBiffWorkbook(path,edited,&package);const auto reopened=readWorkbook(path);require(package.sourcePath==reopened.sourcePath&&package.sourceHash==reopened.sourceHash,"Legacy save refreshes its source package identity");require(reopened.workbook.at("sheets").size()==120,"Native source save preserves all 120 worksheets");
  for(std::size_t i=0;i<120;++i){const auto& before=edited.at("sheets")[i];const auto& after=reopened.workbook.at("sheets")[i];require(before.at("name")==after.at("name")&&before.at("defaultStyle")==after.at("defaultStyle"),"Native source save preserves every sheet name and default style");require(before.at("cells").size()==after.at("cells").size(),"Native source save retains all populated and formatted blank cells");for(const auto& expected:before.at("cells")){const auto& actual=cell(after,expected.at("row"),expected.at("col"));require(actual.at("input")==expected.at("input")&&actual.at("style")==expected.at("style"),"Native source save preserves every input and effective cell style");}for(const auto* key:{"rowLayout","columnLayout","mergedRanges","hiddenRows","defaultRowHeight","showGridLines"})require(before.value(key,Json())==after.value(key,Json()),"Native source save preserves row/column/merge/view layout");}
  if(const auto* retained=std::getenv("TIGERSHEETS_BIFF_SOURCE_CAPTURE"))std::filesystem::copy_file(path,retained,std::filesystem::copy_options::overwrite_existing);std::cout<<"Native 120-sheet BIFF8 source save passed\n";
 }
 const auto original=bytes(path);auto formula=book;formula["sheets"][0]["cells"][0]["input"]="=1+2";bool refused=false;try{writeBiffWorkbook(path,formula);}catch(const std::runtime_error&){refused=true;}require(refused&&bytes(path)==original,"Unsupported BIFF formula serialization fails before touching an existing file");
 if(argc>1&&std::string(argv[1])=="retain"){const auto retained=std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/biff/writer-values.xls";std::filesystem::copy_file(path,retained,std::filesystem::copy_options::overwrite_existing);}std::filesystem::remove(path);std::cout<<"Native BIFF8 writer values passed\n";
}catch(const std::exception& error){std::filesystem::remove(path);std::cerr<<error.what()<<'\n';return 1;}}
