#include "tigersheets/ooxml.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
const Json& cell(const Json& sheet,int row,int col){for(const auto& value:sheet.at("cells"))if(value.at("row")==row&&value.at("col")==col)return value;throw std::runtime_error("Missing imported cell");}
}
int main(int argc,char** argv){try{
 const std::string source=TIGERSHEETS_SOURCE_DIR;
 const auto package=readWorkbook(source+"/tests/fixtures/biff/independent.xls");
 const auto& book=package.workbook;require(book.at("sheets").size()==2,"BIFF preserves both worksheets");
 const auto& first=book.at("sheets")[0];const auto& second=book.at("sheets")[1];
 require(first.at("name")=="Inputs 2026"&&second.at("name")=="Owner's data","BIFF preserves Unicode and quoted sheet names");
 require(cell(first,1,1).at("input")=="Café 東京","BIFF UTF-16 shared string is decoded to UTF-8");
 require(std::stod(cell(first,2,1).at("input").get<std::string>())==1234.5,"BIFF numeric value retains precision");
 require(cell(first,3,1).at("input")=="TRUE"&&cell(second,1,2).at("input")=="FALSE","BIFF booleans retain type");
 require(cell(first,4,1).at("input")=="'00123"&&cell(first,5,1).at("input")=="'=literal","BIFF literal text does not become a number or formula");
 require(cell(second,1,1).at("input")=="-42","BIFF negative numeric value");
 require(package.baseline==book&&!package.sourceHash.empty()&&package.entries.empty(),"BIFF import establishes a semantic baseline without pretending to be OOXML");
 WorkbookSession session(book);require(session.snapshot().at("sheets").size()==2,"BIFF result is a valid editable workbook");
 if(argc>1&&std::string(argv[1])=="budget"){
  bool rejected=false;try{readWorkbook(source+"/tests/fixtures/biff/allocation-budget.xls");}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("allocation budget")!=std::string::npos;}
  require(rejected,"BIFF bounded import refuses excessive dense parser allocation before parsing a sparse distantcorner worksheet");
  std::cout<<"BIFF allocation budget passed\n";
 }
 if(argc>1&&std::string(argv[1])=="raw"){
  std::ifstream referenceFile(source+"/tests/fixtures/biff/financial-source-reference.json");const auto reference=Json::parse(referenceFile);
  const auto raw=readWorkbook(source+"/tests/fixtures/biff/financial-source.xls").workbook;
  require(raw.at("sheets").size()==reference.at("sheetCount"),"Raw Otis BIFF source preserves all120 worksheets");std::size_t count=0,blanks=0;
  for(std::size_t i=0;i<raw.at("sheets").size();++i){const auto& actual=raw.at("sheets")[i];const auto& expected=reference.at("sheets")[i];
   require(actual.at("name")==expected.at("name"),"Raw source sheet ordering and names match independent xlrd");
   for(const auto& value:expected.at("cells")){const auto& imported=cell(actual,value.at("row"),value.at("col"));auto input=imported.at("input").get<std::string>();
    if(value.at("type")==1){if(input.starts_with('\''))input.erase(0,1);require(input==value.at("value"),"Raw source text matches independent xlrd");}else require(std::stod(input)==value.at("value").get<double>(),"Raw source numbers match independent xlrd");
    const auto& expectedStyle=reference.at("styles").at(std::to_string(value.at("xf").get<int>()));for(auto [key,entry]:expectedStyle.items())if(imported.at("style").at(key)!=entry)throw std::runtime_error("Raw style mismatch: "+actual.at("name").get<std::string>()+" row "+value.at("row").dump()+" col "+value.at("col").dump()+" "+key+" expected "+entry.dump()+" actual "+imported.at("style").at(key).dump());++count;
   }
   for(const auto& value:expected.at("formattedBlanks")){const auto& imported=cell(actual,value[0],value[1]);require(imported.at("input")=="","Raw source formattedblank is retained");++blanks;}
   require(actual.value("mergedRanges",Json::array())==expected.at("merges"),"Raw source merged ranges match independent xlrd");require(actual.at("showGridLines")==expected.at("showGridLines"),"Raw source gridline visibility matches independent xlrd");
  }
  require(count==3440&&blanks==15527,"Raw source all3440 typed cells and15527formattedblanks independently audited");
  const auto output=std::filesystem::temp_directory_path()/"tigersheets-otis-raw-native-roundtrip.xlsx";writeWorkbook(output.string(),raw);const auto reopened=readWorkbook(output.string()).workbook;std::filesystem::remove(output);
  require(reopened.at("sheets").size()==120,"Raw source native save/reopen retains all120worksheets");
  for(std::size_t i=0;i<raw.at("sheets").size();++i){const auto& before=raw.at("sheets")[i];const auto& after=reopened.at("sheets")[i];require(before.at("name")==after.at("name")&&before.at("cells").size()==after.at("cells").size(),"Raw source native save/reopen retains names and allpopulated/formattedcells");
   for(std::size_t c=0;c<before.at("cells").size();++c){const auto& a=before.at("cells")[c];const auto& b=after.at("cells")[c];require(a.at("row")==b.at("row")&&a.at("col")==b.at("col")&&a.at("input")==b.at("input"),"Raw source everycell coordinate/value survives native save/reopen");for(const auto* key:{"fontFamily","fontSize","numberFormat","color"})require(a.at("style").at(key)==b.at("style").at(key),"Raw source cellfonts/colors/numberformats survive native save/reopen");for(const auto* key:{"bold","italic","underline"})require(a.at("style").at(key)==b.at("style").value(key,false),"Raw source true/falsefontflags survive native save/reopen");}
   for(const auto* key:{"rowLayout","columnLayout","mergedRanges","showGridLines","defaultRowHeight"})require(before.value(key,Json())==after.value(key,Json()),"Raw source layout survives native save/reopen");
  }
  std::cout<<"Raw BIFF source independently audited:120sheets3440values15527formattedblanks\n";
 }
 if(argc>1&&std::string(argv[1])=="styles"){
  const auto& style=cell(first,2,1).at("style");
  require(style.at("fontFamily")=="Arial"&&style.at("fontSize")==14&&style.at("bold")==true&&style.at("italic")==true&&style.at("underline")==true,"BIFF font attributes survive import");
  require(style.at("color")=="#123456"&&style.at("fill")=="#FFFF00","BIFF custom palette and solid fill survive import");
  require(style.at("numberFormat")=="#,##0.00"&&style.at("alignment")=="right"&&style.at("verticalAlignment")=="top"&&style.at("wrap")==true,"BIFF number format and alignment survive import");
  require(style.at("border").at("bottom")==Json({{"style","double"},{"color","#FF0000"}}),"BIFF border style and color survive import");
  require(first.at("showGridLines")==false,"BIFF worksheet gridline visibility survives import");
  bool rowHeight=false,hiddenRow=false,columnWidth=false,hiddenColumn=false;
  for(const auto& entry:first.at("rowLayout")){if(entry.at("index")==2)rowHeight=entry.at("size")==30;if(entry.at("index")==8)hiddenRow=entry.value("hidden",false);}
  for(const auto& entry:first.at("columnLayout")){if(entry.at("index")==1)columnWidth=entry.at("size")==18;if(entry.at("index")==3)hiddenColumn=entry.value("hidden",false);}
  require(rowHeight&&hiddenRow&&columnWidth&&hiddenColumn,"BIFF row heights, column widths, and hidden dimensions survive import");
  require(first.at("mergedRanges")==Json::array({{{"row1",10},{"col1",1},{"row2",11},{"col2",3}}}),"BIFF merged range survives import");
  const auto output=std::filesystem::temp_directory_path()/"tigersheets-biff-roundtrip.xlsx";writeWorkbook(output.string(),book);const auto reopened=readWorkbook(output.string()).workbook;std::filesystem::remove(output);
  const auto& reopenedFirst=reopened.at("sheets")[0];auto reopenedStyle=cell(reopenedFirst,2,1).at("style");reopenedStyle.erase("ooxmlStyleId");
  require(reopenedStyle==style,"Imported BIFF font/number/fill/border persist through native XLSX save/reopen");
  require(reopenedFirst.at("rowLayout")==first.at("rowLayout")&&reopenedFirst.at("columnLayout")==first.at("columnLayout")&&reopenedFirst.at("mergedRanges")==first.at("mergedRanges"),"Imported BIFF layout persists through native XLSX save/reopen");
  std::cout<<"BIFF style/layout import and XLSX roundtrip passed\n";
 }
 std::cout<<"BIFF values import passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
