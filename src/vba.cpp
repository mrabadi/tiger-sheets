#include "tigersheets/vba.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <cctype>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
namespace tigersheets {
namespace {
std::string trim(std::string text){const auto first=text.find_first_not_of(" \t\r\n");if(first==std::string::npos)return {};return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);}
std::string lower(std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return text;}
std::string asText(const Json& value){if(value.is_string())return value.get<std::string>();if(value.is_null())return {};if(value.is_boolean())return value.get<bool>()?"True":"False";std::ostringstream out;out<<std::setprecision(15)<<value.get<double>();return out.str();}
double asNumber(const Json& value){if(value.is_null())return 0;if(value.is_boolean())return value.get<bool>()?-1:0;if(value.is_number())return value.get<double>();const auto text=value.get<std::string>();std::size_t consumed=0;const auto number=std::stod(text,&consumed);if(consumed!=text.size())throw std::runtime_error("VBA type mismatch");return number;}
struct Runtime;
class Expression {
public:
 Expression(std::string text,Runtime& runtime):text_(std::move(text)),runtime_(runtime){}
 Json evaluate(){auto value=binary(0);spaces();if(position_!=text_.size())throw std::runtime_error("Unsupported VBA expression near "+text_.substr(position_));return value;}
private:
 std::string text_;Runtime& runtime_;std::size_t position_{};
 void spaces(){while(position_<text_.size()&&std::isspace(static_cast<unsigned char>(text_[position_])))++position_;}
 Json atom();Json binary(int minimum);
};
struct Runtime {
 WorkbookSession session;Json original;std::map<std::string,Json> variables;std::function<bool()> cancelled;std::size_t steps{};std::string active;bool readOnly=false;
 Runtime(const Json& book,std::function<bool()> stop):session(book),original(book),cancelled(std::move(stop)){active=book.value("activeSheet",book.at("sheets")[0].at("id").get<std::string>());}
 void step(){if(cancelled&&cancelled())throw std::runtime_error("__cancelled__");if(++steps>100000)throw std::runtime_error("Macro exceeded 100,000 execution steps");}
 Json expression(const std::string& text){return Expression(text,*this).evaluate();}
 std::string sheetId(const std::string& name){const auto book=session.snapshot();for(const auto& sheet:book["sheets"])if(lower(sheet.at("name").get<std::string>())==lower(name)||sheet.at("id")==name)return sheet.at("id").get<std::string>();throw std::runtime_error("VBA worksheet was not found: "+name);}
 Json read(int row,int col,const std::string& sheet=""){const auto id=sheet.empty()?active:sheet;const auto book=session.snapshot();for(const auto& item:book["sheets"])if(item.at("id")==id)for(const auto& cell:item.at("cells"))if(cell["row"]==row&&cell["col"]==col){const auto input=cell.at("input").get<std::string>();double number=0;const auto parsed=std::from_chars(input.data(),input.data()+input.size(),number);if(parsed.ec==std::errc()&&parsed.ptr==input.data()+input.size())return number;if(input.starts_with('='))throw std::runtime_error("VBA reading calculated formula cells requires calculation-host support");return input;}return nullptr;}
 static std::pair<int,int> address(std::string ref){int col=0,row=0;std::size_t i=0;ref=trim(ref);while(i<ref.size()&&(ref[i]=='$'||std::isalpha(static_cast<unsigned char>(ref[i])))){if(ref[i]!='$'){const auto ch=static_cast<char>(std::toupper(static_cast<unsigned char>(ref[i])));col=col*26+ch-'A'+1;}++i;}if(i<ref.size()&&ref[i]=='$')++i;const auto parsed=std::from_chars(ref.data()+i,ref.data()+ref.size(),row);if(parsed.ec!=std::errc()||parsed.ptr!=ref.data()+ref.size()||row<1||row>1048576||col<1||col>16384)throw std::runtime_error("Invalid VBA range address");return {row,col};}
 void write(const std::string& left,const Json& value){
  const std::regex target(R"vba(^(?:(?:Sheets|Worksheets)\("((?:""|[^"])*)"\)\.)?(?:Range\("([^"]+)"\)|Cells\((.+),(.+)\))\.(Value2?|Formula|NumberFormat|HorizontalAlignment|WrapText|Font\.(?:Bold|Italic|Underline|Name|Size|Color|ColorIndex)|Interior\.(?:Color|ColorIndex))$)vba",std::regex::icase);std::smatch match;
  if(!std::regex_match(left,match,target)){if(std::regex_match(left,std::regex(R"(^[A-Za-z_][A-Za-z0-9_]*$)"))){variables[lower(left)]=value;return;}throw std::runtime_error("Unsupported VBA assignment target: "+left);}
  if(readOnly)throw std::runtime_error("Worksheet VBA functions cannot change the workbook");
  auto sheetName=match[1].str();for(std::size_t pos=0;(pos=sheetName.find("\"\"",pos))!=std::string::npos;++pos)sheetName.replace(pos,2,"\"");const auto sheet=match[1].matched?sheetId(sheetName):active;int r1=0,c1=0,r2=0,c2=0;if(match[2].matched){const auto ref=match[2].str();const auto colon=ref.find(':');std::tie(r1,c1)=address(ref.substr(0,colon));std::tie(r2,c2)=colon==std::string::npos?std::pair{r1,c1}:address(ref.substr(colon+1));}else{r1=r2=static_cast<int>(asNumber(expression(match[3].str())));c1=c2=static_cast<int>(asNumber(expression(match[4].str())));}
  if(r2<r1||c2<c1||static_cast<std::uint64_t>(r2-r1+1)*static_cast<std::uint64_t>(c2-c1+1)>100000)throw std::runtime_error("VBA range is reversed or exceeds 100,000 cells");
  Json operations=Json::array();const auto property=lower(match[5].str());for(int row=r1;row<=r2;++row)for(int col=c1;col<=c2;++col){Json op{{"sheet",sheet},{"row",row},{"col",col}};if(property=="value"||property=="value2"||property=="formula"){op["type"]="set_cell";op["input"]=asText(value);}else{op["type"]="set_style";Json style;if(property=="font.colorindex"||property=="interior.colorindex"){const auto expected=property=="font.colorindex"?-4105:-4142;if(asNumber(value)!=expected)throw std::runtime_error("Only automatic font color or no-fill ColorIndex is supported");style=Json::object();const auto book=session.snapshot();for(const auto& item:book["sheets"])if(item["id"]==sheet)for(const auto& cell:item["cells"])if(cell["row"]==row&&cell["col"]==col)style=cell.value("style",Json::object());style.erase(property=="font.colorindex"?"color":"fill");op["replace"]=true;}else if(property=="font.bold")style["bold"]=asNumber(value)!=0;else if(property=="font.italic")style["italic"]=asNumber(value)!=0;else if(property=="font.underline"){const auto code=asNumber(value);if(code!=2&&code!=-4142)throw std::runtime_error("Supported underline values are single or none");style["underline"]=code==2;}else if(property=="wraptext")style["wrap"]=asNumber(value)!=0;else if(property=="horizontalalignment"){const std::map<int,std::string> alignments{{-4131,"left"},{-4108,"center"},{-4152,"right"},{1,"general"},{-4130,"justify"},{5,"fill"},{7,"centerContinuous"},{-4117,"distributed"}};const auto code=static_cast<int>(asNumber(value));if(!alignments.contains(code))throw std::runtime_error("Unsupported VBA horizontal alignment");style["alignment"]=alignments.at(code);}else if(property=="font.name")style["fontFamily"]=asText(value);else if(property=="font.size")style["fontSize"]=asNumber(value);else if(property=="numberformat")style["numberFormat"]=asText(value);else{const auto bgr=static_cast<unsigned>(asNumber(value));std::ostringstream rgb;rgb<<'#'<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(2)<<(bgr&255U)<<std::setw(2)<<((bgr>>8)&255U)<<std::setw(2)<<((bgr>>16)&255U);style[property=="font.color"?"color":"fill"]=rgb.str();}op["style"]=style;}operations.push_back(std::move(op));}
  std::string error;if(!session.apply(session.snapshot().at("revision").get<std::uint64_t>(),operations,&error))throw std::runtime_error(error);
 }
};
Json Expression::atom(){spaces();if(position_>=text_.size())throw std::runtime_error("Missing VBA expression");const auto c=text_[position_];if(c=='+'||c=='-'){++position_;const auto value=asNumber(binary(5));return c=='-'?-value:value;}if(c=='('){++position_;auto value=binary(0);spaces();if(position_>=text_.size()||text_[position_++]!=')')throw std::runtime_error("VBA expression missing closing parenthesis");return value;}
 if(c=='"'){++position_;std::string value;while(position_<text_.size()){const auto next=text_[position_++];if(next=='"'){if(position_<text_.size()&&text_[position_]=='"'){value+='"';++position_;}else return value;}else value+=next;}throw std::runtime_error("Unterminated VBA string");}
 if(std::isdigit(static_cast<unsigned char>(c))||c=='.'){const auto begin=position_;double value=0;const auto parsed=std::from_chars(text_.data()+begin,text_.data()+text_.size(),value);if(parsed.ec!=std::errc())throw std::runtime_error("Invalid VBA number");position_=static_cast<std::size_t>(parsed.ptr-text_.data());return value;}
 const auto begin=position_;while(position_<text_.size()&&(std::isalnum(static_cast<unsigned char>(text_[position_]))||text_[position_]=='_'))++position_;if(begin==position_)throw std::runtime_error("Unsupported VBA token");const auto name=lower(text_.substr(begin,position_-begin));spaces();
 if(position_<text_.size()&&text_[position_]=='('){++position_;std::vector<Json> args;spaces();if(position_<text_.size()&&text_[position_]!=')')for(;;){args.push_back(binary(0));spaces();if(position_<text_.size()&&text_[position_]==','){++position_;continue;}break;}if(position_>=text_.size()||text_[position_++]!=')')throw std::runtime_error("VBA function missing closing parenthesis");
  if(name=="range"||name=="cells"){int row=0,col=0;if(name=="range"){if(args.size()!=1)throw std::runtime_error("Range requires one address");std::tie(row,col)=Runtime::address(asText(args[0]));}else{if(args.size()!=2)throw std::runtime_error("Cells requires row and column");row=static_cast<int>(asNumber(args[0]));col=static_cast<int>(asNumber(args[1]));}spaces();if(position_<text_.size()&&text_[position_]=='.'){++position_;const auto property=position_;while(position_<text_.size()&&std::isalnum(static_cast<unsigned char>(text_[position_])))++position_;const auto p=lower(text_.substr(property,position_-property));if(p!="value"&&p!="value2")throw std::runtime_error("Unsupported VBA read property");}return runtime_.read(row,col);}
  if(name=="rgb"&&args.size()==3)return asNumber(args[0])+asNumber(args[1])*256+asNumber(args[2])*65536;
  if(args.size()!=1)throw std::runtime_error("Unsupported VBA function arguments");
  if(name=="chr"){const auto code=asNumber(args[0]);if(code<0||code>127||std::floor(code)!=code)throw std::runtime_error("Supported Chr codes are ASCII 0 through 127");return std::string(1,static_cast<char>(code));}
  if(name=="abs")return std::abs(asNumber(args[0]));
  if(name=="sqr")return std::sqrt(asNumber(args[0]));
  if(name=="int")return std::floor(asNumber(args[0]));
  if(name=="cdbl")return asNumber(args[0]);
  if(name=="cint"){const auto number=asNumber(args[0]);if(!std::isfinite(number))throw std::runtime_error("VBA CInt overflow");const auto floor=std::floor(number),fraction=number-floor;const auto rounded=fraction<0.5?floor:fraction>0.5?floor+1:std::fmod(std::abs(floor),2)==0?floor:floor+1;if(rounded<-32768||rounded>32767)throw std::runtime_error("VBA CInt overflow");return static_cast<int>(rounded);}
  if(name=="cstr")return asText(args[0]);
  if(name=="len")return asText(args[0]).size();
  throw std::runtime_error("Unsupported VBA function: "+name);
 }
 if(name=="true")return true;
 if(name=="false")return false;
 if(name=="empty"||name=="nothing")return nullptr;
 const auto variable=runtime_.variables.find(name);if(variable==runtime_.variables.end())throw std::runtime_error("Unknown VBA variable: "+name);return variable->second;
}
Json Expression::binary(int minimum){auto left=atom();for(;;){spaces();const auto saved=position_;std::string op;for(const auto* candidate:{"<=",">=","<>","=","<",">","&","+","-","*","/","^"})if(text_.compare(position_,std::strlen(candidate),candidate)==0){op=candidate;break;}if(op.empty())return left;const auto priority=(op=="="||op=="<"||op==">"||op=="<="||op==">="||op=="<>")?1:op=="&"?2:(op=="+"||op=="-")?3:(op=="*"||op=="/")?4:5;if(priority<minimum)return left;position_+=op.size();const auto right=binary(priority+(op=="^"?0:1));if(op=="&")left=asText(left)+asText(right);else if(op=="=")left=left==right;else if(op=="<>")left=left!=right;else{const auto a=asNumber(left),b=asNumber(right);if(op=="+")left=a+b;else if(op=="-")left=a-b;else if(op=="*")left=a*b;else if(op=="/"){if(b==0)throw std::runtime_error("VBA division by zero");left=a/b;}else if(op=="^")left=std::pow(a,b);else if(op=="<")left=a<b;else if(op==">")left=a>b;else if(op=="<=")left=a<=b;else if(op==">=")left=a>=b;}if(position_==saved)throw std::runtime_error("VBA parser did not advance");}}
std::vector<std::string> lines(const std::string& source){std::vector<std::string> result;std::istringstream input(source);std::string line;while(std::getline(input,line)){bool quoted=false;std::string clean;for(std::size_t i=0;i<line.size();++i){const auto c=line[i];if(c=='"')quoted=!quoted;if(c=='\''&&!quoted)break;clean+=c;}clean=trim(clean);if(!clean.empty()&&!lower(clean).starts_with("rem "))result.push_back(clean);}return result;}
void statement(Runtime& runtime,std::string line){runtime.step();const auto lowered=lower(line);if(lowered.starts_with("dim ")){std::string declaration=line.substr(4);const auto as=lower(declaration).find(" as ");const auto name=trim(declaration.substr(0,as));if(name.find_first_of(" ,()")!=std::string::npos)throw std::runtime_error("Unsupported VBA declaration");runtime.variables[lower(name)]=as!=std::string::npos&&lower(declaration.substr(as+4))=="string"?Json(""):Json(0);return;}if(lowered.starts_with("let "))line=trim(line.substr(4));const auto equals=line.find('=');if(equals==std::string::npos)throw std::runtime_error("Unsupported VBA statement: "+line);runtime.write(trim(line.substr(0,equals)),runtime.expression(trim(line.substr(equals+1))));}
bool execute(Runtime& runtime,const std::vector<std::string>& program,std::size_t begin,std::size_t end){
 const std::regex forStart(R"(^For\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+?)\s+To\s+(.+?)(?:\s+Step\s+(.+))?$)",std::regex::icase),ifStart(R"(^If\s+(.+)\s+Then(?:\s+(.*))?$)",std::regex::icase);
 for(auto i=begin;i<end;++i){runtime.step();const auto& line=program[i];const auto lowered=lower(line);std::smatch match;
  if(std::regex_match(line,match,forStart)){
   std::size_t finish=i+1;int depth=1;for(;finish<end;++finish){const auto text=lower(program[finish]);if(text.starts_with("for "))++depth;else if(text=="next"||text.starts_with("next ")){if(--depth==0)break;}}if(finish==end)throw std::runtime_error("VBA For has no Next");
   const auto name=lower(match[1].str());const auto start=asNumber(runtime.expression(match[2].str())),limit=asNumber(runtime.expression(match[3].str())),increment=match[4].matched?asNumber(runtime.expression(match[4].str())):1;if(increment==0||!std::isfinite(start)||!std::isfinite(limit)||!std::isfinite(increment))throw std::runtime_error("Invalid VBA For step or bounds");double value=start;
   for(;increment>0?value<=limit:value>=limit;value+=increment){runtime.step();runtime.variables[name]=value;if(execute(runtime,program,i+1,finish))return true;}runtime.variables[name]=value;i=finish;continue;
  }
  if(std::regex_match(line,match,ifStart)){
   const auto condition=asNumber(runtime.expression(match[1].str()))!=0;if(match[2].matched&&!match[2].str().empty()){if(condition)statement(runtime,match[2].str());continue;}
   std::size_t finish=i+1,alternative=end;int depth=1;for(;finish<end;++finish){const auto text=lower(program[finish]);std::smatch nested;if(std::regex_match(program[finish],nested,ifStart)&&!nested[2].matched)++depth;else if(text=="end if"||text=="endif"){if(--depth==0)break;}else if(text=="else"&&depth==1)alternative=finish;}if(finish==end)throw std::runtime_error("VBA If has no End If");if(condition){if(execute(runtime,program,i+1,std::min(alternative,finish)))return true;}else if(alternative<finish&&execute(runtime,program,alternative+1,finish))return true;i=finish;continue;
  }
  if(lowered=="exit sub")return true;
  statement(runtime,line);
 }return false;
}

}
Json runMacro(const Json& snapshot,std::string source,std::string procedure,bool enabled,std::function<bool()> cancelled){
 Json result{{"status","disabled"},{"workbook",snapshot},{"diagnostics",Json::array()},{"variables",Json::object()}};if(!enabled){result["diagnostics"].push_back("Macros are disabled for this workbook");return result;}
 try{if(!std::regex_match(procedure,std::regex("[A-Za-z_][A-Za-z0-9_]*")))throw std::runtime_error("Invalid VBA procedure name");if(source.size()>1024*1024)throw std::runtime_error("VBA module exceeds 1 MiB limit");Runtime runtime(snapshot,std::move(cancelled));const auto program=lines(source);bool found=false;const std::regex start("^(?:(?:Public|Private)\\s+)?Sub\\s+"+procedure+R"(\s*\(\s*\)\s*$)",std::regex::icase);for(std::size_t i=0;i<program.size();++i)if(std::regex_match(program[i],start)){found=true;const auto begin=i+1;for(++i;i<program.size()&&lower(program[i])!="end sub";++i){}if(i==program.size())throw std::runtime_error("VBA procedure has no End Sub");execute(runtime,program,begin,i);break;}if(!found)throw std::runtime_error("VBA Sub was not found: "+procedure);auto book=runtime.session.snapshot();book["revision"]=snapshot.value("revision",std::uint64_t{0})+(book==snapshot?0:1);result["workbook"]=std::move(book);result["variables"]=runtime.variables;result["status"]="completed";}catch(const std::exception& ex){result["status"]=std::string(ex.what())=="__cancelled__"?"cancelled":"error";result["diagnostics"].push_back(ex.what());}return result;
}
Json evaluateVbaFunction(const Json& snapshot,std::string source,std::string functionName,const Json& args,std::function<bool()> cancelled){
 Json result{{"status","error"},{"value",nullptr},{"diagnostics",Json::array()}};
 try{
  if(!std::regex_match(functionName,std::regex("[A-Za-z_][A-Za-z0-9_]*")))throw std::runtime_error("Invalid VBA function name");
  if(!args.is_array())throw std::runtime_error("VBA function arguments must be an array");
  if(source.size()>1024*1024)throw std::runtime_error("VBA module exceeds 1 MiB limit");
  Runtime runtime(snapshot,std::move(cancelled));runtime.readOnly=true;const auto program=lines(source);const std::regex start("^(?:(?:Public|Private)\\s+)?Function\\s+"+functionName+R"(\s*\((.*)\)(?:\s+As\s+\w+)?\s*$)",std::regex::icase);bool found=false;
  for(std::size_t i=0;i<program.size();++i){std::smatch declaration;if(!std::regex_match(program[i],declaration,start))continue;found=true;std::istringstream parameters(declaration[1].str());std::string parameter;std::size_t index=0;while(std::getline(parameters,parameter,',')){parameter=trim(parameter);if(parameter.empty())continue;const auto lowered=lower(parameter);if(lowered.starts_with("byval ")||lowered.starts_with("byref "))parameter=trim(parameter.substr(6));const auto as=lower(parameter).find(" as ");const auto name=trim(parameter.substr(0,as));if(!std::regex_match(name,std::regex("[A-Za-z_][A-Za-z0-9_]*")))throw std::runtime_error("Unsupported VBA function parameter");if(index>=args.size())throw std::runtime_error("VBA function argument count mismatch");runtime.variables[lower(name)]=args[index++];}if(index!=args.size())throw std::runtime_error("VBA function argument count mismatch");runtime.variables[lower(functionName)]=0;const auto begin=i+1;for(++i;i<program.size()&&lower(program[i])!="end function";++i){}if(i==program.size())throw std::runtime_error("VBA Function has no End Function");execute(runtime,program,begin,i);result["value"]=runtime.variables.at(lower(functionName));break;}
  if(!found)throw std::runtime_error("VBA Function was not found: "+functionName);
  result["status"]="completed";
 }catch(const std::exception& ex){result["diagnostics"].push_back(ex.what());if(std::string(ex.what())=="__cancelled__")result["status"]="cancelled";}
 return result;
}

}
