#include "tigersheets/csv.hpp"
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <unistd.h>
#include <limits>
#include <map>
#include <charconv>
#include <cmath>
#include <set>
namespace tigersheets {
CsvRows parseCsv(std::string_view text){
 if(text.size()>=3&&text.substr(0,3)=="\xef\xbb\xbf")text.remove_prefix(3);
 CsvRows result;std::vector<std::string> row;std::string field;bool quoted=false,afterQuote=false;
 for(std::size_t i=0;i<text.size();++i){const auto c=text[i];
  if(quoted){if(c=='"'){if(i+1<text.size()&&text[i+1]=='"'){field+='"';++i;}else{quoted=false;afterQuote=true;}}else field+=c;continue;}
  if(c==','||c=='\r'||c=='\n'){row.push_back(field);field.clear();afterQuote=false;if(c!=','){result.push_back(row);row.clear();if(c=='\r'&&i+1<text.size()&&text[i+1]=='\n')++i;}continue;}
  if(afterQuote)throw std::runtime_error("Unexpected data after closing CSV quote");
  if(c=='"'){if(!field.empty())throw std::runtime_error("CSV quote must begin a field");quoted=true;}else field+=c;
 }
 if(quoted)throw std::runtime_error("Unterminated CSV quoted field");
 if(!row.empty()||!field.empty()||afterQuote||(!text.empty()&&text.back()==',')){row.push_back(field);result.push_back(row);}
 return result;
}
std::string formatCsv(const CsvRows& rows){std::string result;for(const auto& row:rows){bool first=true;for(const auto& field:row){if(!first)result+=',';first=false;if(field.find_first_of(",\"\r\n")!=std::string::npos){result+='"';for(const auto c:field){if(c=='"')result+='"';result+=c;}result+='"';}else result+=field;}result+="\r\n";}return result;}
Json readCsv(const std::string& path){
 std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot open CSV file");
 file.seekg(0,std::ios::end);const auto size=file.tellg();if(size<0||size>256*1024*1024)throw std::runtime_error("CSV exceeds 256 MiB limit");file.seekg(0);std::string bytes(static_cast<std::size_t>(size),'\0');if(!bytes.empty()&&!file.read(bytes.data(),size))throw std::runtime_error("Cannot read CSV file");
 const auto rows=parseCsv(bytes);if(rows.size()>1048576)throw std::runtime_error("CSV exceeds worksheet row limit");
 Json cells=Json::array();for(std::size_t r=0;r<rows.size();++r){if(rows[r].size()>16384)throw std::runtime_error("CSV exceeds worksheet column limit");for(std::size_t c=0;c<rows[r].size();++c)if(!rows[r][c].empty())cells.push_back({{"row",r+1},{"col",c+1},{"input",rows[r][c]}});}
 return {{"revision",0},{"sheets",Json::array({{{"id","sheet-1"},{"name","Sheet1"},{"cells",std::move(cells)}}})}};
}
Json csvValuesSnapshot(const Json& workbook,const Json& calculated,const std::string& sheetId){
 const auto validRevision=[](const Json& value){return value.is_number_integer()&&(value.is_number_unsigned()?value.get<std::uint64_t>()<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()):value.get<std::int64_t>()>=0);};
 if(!workbook.contains("revision")||!validRevision(workbook["revision"])||!calculated.contains("revision")||!validRevision(calculated["revision"])||calculated["revision"]!=workbook["revision"])throw std::runtime_error("CSV export requires calculated values for the current workbook revision");
 if(!calculated.contains("status")||calculated["status"]!="ok")throw std::runtime_error("CSV export requires a completed successful calculation");
 if(!calculated.contains("values")||!calculated["values"].is_array())throw std::runtime_error("CSV calculation values must be an array");
 auto output=workbook;Json* selected=nullptr;for(auto& sheet:output.at("sheets"))if(sheetId.empty()||sheet.at("id")==sheetId){selected=&sheet;break;}if(!selected)throw std::runtime_error("CSV worksheet was not found");
 const auto coordinatePart=[](const Json& value,int maximum){
  if(!value.is_number_integer())throw std::runtime_error("CSV calculated coordinates must be integers");
  if(value.is_number_unsigned()){const auto number=value.get<std::uint64_t>();if(number<1||number>static_cast<std::uint64_t>(maximum))throw std::runtime_error("CSV calculated coordinate is out of bounds");return static_cast<int>(number);}
  const auto number=value.get<std::int64_t>();if(number<1||number>maximum)throw std::runtime_error("CSV calculated coordinate is out of bounds");return static_cast<int>(number);
 };
 using Coordinate=std::pair<int,int>;std::map<Coordinate,Json> cells;std::set<Coordinate> formulas,seen;
 for(auto cell:selected->at("cells")){
  const auto coordinate=Coordinate{coordinatePart(cell.at("row"),1048576),coordinatePart(cell.at("col"),16384)};const auto input=cell.at("input").get<std::string>();
  if(input.starts_with('='))formulas.insert(coordinate);else if(input.starts_with('\''))cell["input"]=input.substr(1);
  cells[coordinate]=std::move(cell);
 }
 for(const auto& value:calculated.at("values"))if(value.at("sheet")==selected->at("id")){
  const auto row=coordinatePart(value.at("row"),1048576),col=coordinatePart(value.at("col"),16384);const Coordinate coordinate{row,col};
  if(!seen.insert(coordinate).second)throw std::runtime_error("CSV calculation contains duplicate cell results");
  const auto type=value.at("type").get<std::string>();const auto& payload=value.at("value");std::string input;
  if(type=="number"){
   if(!payload.is_number()||!std::isfinite(payload.get<double>()))throw std::runtime_error("CSV calculated number must be finite and numeric");
   if(payload.is_number_integer())input=payload.dump();else{char buffer[128];const auto text=std::to_chars(buffer,buffer+sizeof buffer,payload.get<double>(),std::chars_format::general);if(text.ec!=std::errc())throw std::runtime_error("Cannot format a CSV calculated number");input=std::string(buffer,text.ptr);}
  }else if(type=="text"||type=="error"){if(!payload.is_string())throw std::runtime_error("CSV text and error results must contain text");input=payload.get<std::string>();}
  else if(type=="boolean"){if(!payload.is_boolean())throw std::runtime_error("CSV boolean result must contain a boolean");input=payload.get<bool>()?"TRUE":"FALSE";}
  else if(type=="empty"){if(!payload.is_null())throw std::runtime_error("CSV empty result must contain null");}
  else throw std::runtime_error("Unsupported CSV calculated cell type: "+type);
  if(!cells.contains(coordinate))cells[coordinate]={{"row",row},{"col",col}};
  cells[coordinate]["input"]=std::move(input);
 }
 for(const auto& coordinate:formulas)if(!seen.contains(coordinate))throw std::runtime_error("CSV formula result is missing for "+selected->at("id").get<std::string>()+"!R"+std::to_string(coordinate.first)+"C"+std::to_string(coordinate.second));
 (*selected)["cells"]=Json::array();for(const auto& [coordinate,cell]:cells){(void)coordinate;(*selected)["cells"].push_back(cell);}
 return output;
}
void writeCsv(const std::string& path,const Json& workbook,const std::string& sheetId){
 const auto& sheets=workbook.at("sheets");if(sheets.empty())throw std::runtime_error("Workbook has no worksheet");
 const Json* selected=nullptr;for(const auto& sheet:sheets)if(sheetId.empty()||sheet.at("id")==sheetId){selected=&sheet;break;}if(!selected)throw std::runtime_error("CSV worksheet was not found");
 int lastRow=0,lastCol=0;std::map<std::pair<int,int>,std::string> values;for(const auto& cell:selected->at("cells")){const auto input=cell.at("input").get<std::string>();if(input.empty())continue;const auto r=cell.at("row").get<int>(),c=cell.at("col").get<int>();if(r<1||r>1048576||c<1||c>16384)throw std::runtime_error("CSV cell address is out of bounds");lastRow=std::max(lastRow,r);lastCol=std::max(lastCol,c);values[{r,c}]=input;}
 if(static_cast<std::uint64_t>(lastRow)*static_cast<std::uint64_t>(lastCol)>10000000)throw std::runtime_error("CSV rectangle exceeds 10 million fields");
 auto temporary=path+".tmp-XXXXXX";std::vector<char> name(temporary.begin(),temporary.end());name.push_back('\0');const auto descriptor=mkstemp(name.data());if(descriptor<0)throw std::runtime_error("Cannot create temporary CSV file");close(descriptor);
 try{std::ofstream file(name.data(),std::ios::binary|std::ios::trunc);if(!file)throw std::runtime_error("Cannot write temporary CSV file");for(int r=1;r<=lastRow;++r){std::vector<std::string> row;row.reserve(static_cast<std::size_t>(lastCol));for(int c=1;c<=lastCol;++c){const auto it=values.find({r,c});row.push_back(it==values.end()?"":it->second);}file<<formatCsv({row});}file.close();if(!file)throw std::runtime_error("Cannot finish CSV file");std::filesystem::rename(name.data(),path);}catch(...){std::filesystem::remove(name.data());throw;}
}

}
