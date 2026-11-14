#include "tigersheets/references.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <stdexcept>
namespace tigersheets {
std::string columnName(int c){std::string s;while(c>0){--c;s+=char('A'+c%26);c/=26;}std::reverse(s.begin(),s.end());return s;}
int columnNumber(std::string_view s){int n=0;for(unsigned char c:s){if(!std::isalpha(c))return 0;n=n*26+std::toupper(c)-'A'+1;if(n>MaxColumns)return 0;}return n;}
namespace {
bool asciiLetter(char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');}
bool asciiDigit(char c){return c>='0'&&c<='9';}
bool formulaIdentifier(unsigned char c){return asciiLetter(static_cast<char>(c))||asciiDigit(static_cast<char>(c))||c=='_'||c=='.'||c>=128;}
std::size_t quotedEnd(std::string_view formula,std::size_t start,char quote){
 auto end=start+1;
 while(end<formula.size()){if(formula[end++]==quote){if(end<formula.size()&&formula[end]==quote)++end;else break;}}
 return end;
}
std::size_t bracketEnd(std::string_view formula,std::size_t start){
 int depth=0;auto end=start;
 do{if(formula[end]=='[')++depth;else if(formula[end]==']')--depth;++end;}while(end<formula.size()&&depth);
 return end;
}
struct ReferenceCell {
 std::size_t start=0,end=0,letters=0,endLetters=0,digits=0;
 int row=0,col=0;
 bool colLock=false,rowLock=false;
 char kind='a';
};
std::optional<ReferenceCell> referenceCell(std::string_view formula,std::size_t start){
 if(start>=formula.size())return std::nullopt;
 ReferenceCell cell;cell.start=start;auto end=start;
 if(formula[end]=='$'){cell.colLock=true;++end;}
 cell.letters=end;while(end<formula.size()&&asciiLetter(formula[end]))++end;cell.endLetters=end;
 if(end<formula.size()&&formula[end]=='$'){cell.rowLock=true;++end;}
 cell.digits=end;while(end<formula.size()&&asciiDigit(formula[end]))++end;cell.end=end;
 cell.col=columnNumber(formula.substr(cell.letters,cell.endLetters-cell.letters));
 const auto parsed=std::from_chars(formula.data()+cell.digits,formula.data()+end,cell.row);
 if(!cell.col||cell.digits==end||parsed.ec!=std::errc{}||cell.row<1||cell.row>MaxRows)return std::nullopt;
 return cell;
}
std::optional<ReferenceCell> referenceEndpoint(std::string_view formula,std::size_t start){
 if(auto cell=referenceCell(formula,start))return cell;
 if(start>=formula.size())return std::nullopt;
 ReferenceCell axis;axis.start=start;auto end=start;bool locked=false;if(formula[end]=='$'){locked=true;++end;}
 axis.letters=end;while(end<formula.size()&&asciiLetter(formula[end]))++end;axis.endLetters=end;
 if(end>axis.letters){
  axis.col=columnNumber(formula.substr(axis.letters,end-axis.letters));if(!axis.col)return std::nullopt;
  axis.row=1;axis.colLock=locked;axis.kind='c';axis.end=end;return axis;
 }
 axis.digits=end;while(end<formula.size()&&asciiDigit(formula[end]))++end;
 const auto parsed=std::from_chars(formula.data()+axis.digits,formula.data()+end,axis.row);
 if(end==axis.digits||parsed.ec!=std::errc{}||axis.row<1||axis.row>MaxRows)return std::nullopt;
 axis.col=1;axis.rowLock=locked;axis.kind='r';axis.end=end;return axis;
}
struct ReferenceQualifier {std::size_t end=0;std::string sheet;bool external=false;};
std::optional<ReferenceQualifier> referenceQualifier(std::string_view formula,std::size_t start){
 auto end=start;std::string decoded;
 if(formula[start]=='\''){
  ++end;bool closed=false;
  while(end<formula.size()){
   const char c=formula[end++];if(c=='\''){if(end<formula.size()&&formula[end]=='\''){decoded+='\'';++end;}else{closed=true;break;}}else decoded+=c;
  }
  if(!closed)return std::nullopt;
 }else{
  if(formula[end]=='['){const auto bracket=bracketEnd(formula,end);if(bracket==end||formula[bracket-1]!=']')return std::nullopt;decoded+=formula.substr(end,bracket-end);end=bracket;}
  const auto name=end;while(end<formula.size()&&formulaIdentifier(static_cast<unsigned char>(formula[end])))++end;
  if(end==name)return std::nullopt;
  decoded+=formula.substr(name,end-name);
 }
 if(end>=formula.size()||formula[end]!='!')return std::nullopt;
 ReferenceQualifier result;result.end=end+1;result.sheet=std::move(decoded);
 const auto closing=result.sheet.rfind(']');if(result.sheet.find('[')!=std::string::npos&&closing!=std::string::npos){result.external=true;result.sheet.erase(0,closing+1);}
 if(result.sheet.empty()||result.sheet.find(':')!=std::string::npos)return std::nullopt;
 return result;
}
bool referenceBefore(std::string_view formula,std::size_t start){
 auto previous=start;while(previous>0&&std::isspace(static_cast<unsigned char>(formula[previous-1])))--previous;
 if(previous>0&&formula[previous-1]==':')return false;
 return start==0||(!formulaIdentifier(static_cast<unsigned char>(formula[start-1]))&&formula[start-1]!='$'&&formula[start-1]!=']'&&formula[start-1]!='!'&&formula[start-1]!='#');
}
bool referenceAfter(std::string_view formula,std::size_t end){return end==formula.size()||(!formulaIdentifier(static_cast<unsigned char>(formula[end]))&&formula[end]!='$'&&formula[end]!='('&&formula[end]!='!'&&formula[end]!='['&&formula[end]!=':');}
struct ParsedReference {FormulaReference value;ReferenceCell first,last;bool range=false;};
std::vector<ParsedReference> parsedReferences(std::string_view formula){
 std::vector<ParsedReference> parsed;
 for(std::size_t start=0;start<formula.size();){
  if(formula[start]=='"'){start=quotedEnd(formula,start,'"');continue;}
  const auto qualifier=referenceBefore(formula,start)?referenceQualifier(formula,start):std::nullopt;
  if(!qualifier&&formula[start]=='\''){start=quotedEnd(formula,start,'\'');continue;}
  if(!qualifier&&formula[start]=='['){start=bracketEnd(formula,start);continue;}
  const auto first=referenceBefore(formula,start)?referenceEndpoint(formula,qualifier?qualifier->end:start):std::nullopt;
  if(first){
   auto last=*first;bool range=false,rangeCandidate=false;auto separator=first->end;while(separator<formula.size()&&std::isspace(static_cast<unsigned char>(formula[separator])))++separator;
   if(separator<formula.size()&&formula[separator]==':'){
    rangeCandidate=true;
    auto next=separator+1;while(next<formula.size()&&std::isspace(static_cast<unsigned char>(formula[next])))++next;
    const auto second=referenceEndpoint(formula,next);if(second&&second->kind==first->kind){last=*second;range=true;}
   }
   if((!rangeCandidate||range)&&(range||first->kind=='a')&&referenceAfter(formula,last.end)){
    FormulaReference value;if(qualifier){value.sheet=qualifier->sheet;value.external=qualifier->external;}value.start=start;value.length=last.end-start;value.row1=std::min(first->row,last.row);value.row2=std::max(first->row,last.row);value.col1=std::min(first->col,last.col);value.col2=std::max(first->col,last.col);
    if(first->kind=='c'){value.row1=1;value.row2=MaxRows;}
    if(first->kind=='r'){value.col1=1;value.col2=MaxColumns;}
    parsed.push_back({value,*first,last,range});start=last.end;continue;
   }
  }
  if(qualifier){start=qualifier->end;continue;}
  if(formulaIdentifier(static_cast<unsigned char>(formula[start]))||formula[start]=='$'){do{++start;}while(start<formula.size()&&(formulaIdentifier(static_cast<unsigned char>(formula[start]))||formula[start]=='$'));}
  else ++start;
 }
 return parsed;
}
}
std::vector<FormulaReference> formulaReferences(std::string_view formula){
 std::vector<FormulaReference> result;for(const auto& parsed:parsedReferences(formula))result.push_back(parsed.value);return result;
}
std::string shiftFormula(std::string_view formula,int dr,int dc){
 std::string out;std::size_t previous=0;
 for(const auto& reference:parsedReferences(formula)){
  auto shifted=[&](const ReferenceCell& cell)->std::optional<std::string>{
   const long long row=static_cast<long long>(cell.row)+(cell.kind=='c'||cell.rowLock?0:dr),col=static_cast<long long>(cell.col)+(cell.kind=='r'||cell.colLock?0:dc);
   if(row<1||row>MaxRows||col<1||col>MaxColumns)return std::nullopt;
   std::string text;if(cell.kind!='r')text=(cell.colLock?"$":"")+columnName(static_cast<int>(col));
   if(cell.kind!='c')text+=(cell.rowLock?"$":"")+std::to_string(row);return text;
  };
  const auto first=shifted(reference.first),last=shifted(reference.last);
  out+=formula.substr(previous,reference.value.start-previous);
  out+=formula.substr(reference.value.start,reference.first.start-reference.value.start);
  if(!first||!last)out+="#REF!";
  else{out+=*first;if(reference.range){out+=formula.substr(reference.first.end,reference.last.start-reference.first.end);out+=*last;}}
  previous=reference.last.end;
 }
 out+=formula.substr(previous);return out;
}
std::string editFormulaReferences(std::string_view formula,std::string_view formulaSheet,std::string_view editedSheet,bool rows,int index,int count,bool remove){
 const int limit=rows?MaxRows:MaxColumns;if(index<1||index>limit||count<1||count>limit-index+1)throw std::runtime_error("Invalid structural reference edit bounds");
 auto equal=[](std::string_view a,std::string_view b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](unsigned char x,unsigned char y){return std::tolower(x)==std::tolower(y);});};
 const auto references=parsedReferences(formula);
 const auto supportedGap=[](std::string_view gap){for(std::size_t at=0;at<gap.size();){const char c=gap[at];if(c=='"'||c=='\''){at=quotedEnd(gap,at,c);continue;}if(c=='#'){++at;while(at<gap.size()&&(formulaIdentifier(static_cast<unsigned char>(gap[at]))||gap[at]=='/'||gap[at]=='?'||gap[at]=='!'))++at;continue;}if(c=='['||c==']'||c==':'||c=='!')throw std::runtime_error("Structural editing cannot safely rebase 3D, structured or unsupported references; replace them with supported A1 references first");++at;}};
 std::size_t checked=0;for(const auto& reference:references){supportedGap(formula.substr(checked,reference.value.start-checked));checked=reference.last.end;}supportedGap(formula.substr(checked));
 std::string out;std::size_t previous=0;
 for(const auto& reference:references){
  const auto& span=reference.value;const auto sheet=span.sheet.empty()?formulaSheet:std::string_view(span.sheet);
  out+=formula.substr(previous,span.start-previous);previous=reference.last.end;
  if(span.external||!equal(sheet,editedSheet)||(rows&&reference.first.kind=='c')||(!rows&&reference.first.kind=='r')){out+=formula.substr(span.start,span.length);continue;}
  const int first=rows?reference.first.row:reference.first.col,last=rows?reference.last.row:reference.last.col;
  const int low=std::min(first,last),high=std::max(first,last);int newLow=low,newHigh=high;bool invalid=false;
  if(!remove){if(low>=index)newLow+=count;if(high>=index)newHigh+=count;invalid=newHigh>limit;}
  else{
   const int end=index+count-1;
   if(low>=index&&high<=end)invalid=true;
   else{newLow=low<index?low:low>end?low-count:index;newHigh=high<index?high:high>end?high-count:index-1;}
  }
  out+=formula.substr(span.start,reference.first.start-span.start);
  if(invalid){out+="#REF!";continue;}
  auto endpoint=[&](const ReferenceCell& cell,int value){const int row=rows?value:cell.row,col=rows?cell.col:value;std::string text;if(cell.kind!='r')text=(cell.colLock?"$":"")+columnName(col);if(cell.kind!='c')text+=(cell.rowLock?"$":"")+std::to_string(row);return text;};
  const bool reversed=first>last;out+=endpoint(reference.first,reversed?newHigh:newLow);
  if(reference.range){out+=formula.substr(reference.first.end,reference.last.start-reference.first.end);out+=endpoint(reference.last,reversed?newLow:newHigh);}
 }
 out+=formula.substr(previous);return out;
}
std::string moveFormulaReferences(std::string_view formula,std::string_view formulaSheet,std::string_view sourceSheet,std::string_view destinationSheet,int row1,int col1,int row2,int col2,int dr,int dc,bool formulaMoved){
 auto equal=[](std::string_view a,std::string_view b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](unsigned char x,unsigned char y){return std::tolower(x)==std::tolower(y);});};
 auto quoted=[](std::string_view name){std::string text="'";for(char c:name){text+=c;if(c=='\'')text+='\'';}return text+"'!";};
 std::string out;std::size_t previous=0;
 for(const auto& reference:parsedReferences(formula)){
  const auto& span=reference.value;const auto sheet=span.sheet.empty()?formulaSheet:std::string_view(span.sheet);
  const bool intersects=!span.external&&equal(sheet,sourceSheet)&&span.row1<=row2&&span.row2>=row1&&span.col1<=col2&&span.col2>=col1;
  const bool contained=intersects&&span.row1>=row1&&span.row2<=row2&&span.col1>=col1&&span.col2<=col2;
  if(intersects&&!contained)throw std::runtime_error("The move partially intersects a formula range. Move the complete referenced range or adjust the formula first.");
  out+=formula.substr(previous,span.start-previous);
  if(contained){
   if(!span.sheet.empty()||!equal(formulaMoved?destinationSheet:formulaSheet,destinationSheet))out+=quoted(destinationSheet);
   auto moved=[&](const ReferenceCell& cell){std::string text;if(cell.kind!='r')text=(cell.colLock?"$":"")+columnName(cell.col+dc);if(cell.kind!='c')text+=(cell.rowLock?"$":"")+std::to_string(cell.row+dr);return text;};
   out+=moved(reference.first);if(reference.range){out+=formula.substr(reference.first.end,reference.last.start-reference.first.end);out+=moved(reference.last);}
  }else if(formulaMoved&&span.sheet.empty()&&!span.external&&!equal(sourceSheet,destinationSheet)){
   out+=quoted(sourceSheet);out+=formula.substr(reference.first.start,reference.last.end-reference.first.start);
  }else out+=formula.substr(span.start,span.length);
  previous=reference.last.end;
 }
 out+=formula.substr(previous);return out;
}
std::string cycleReference(std::string_view formula,std::size_t caret){
 for(const auto& reference:parsedReferences(formula)){
  if(reference.first.kind!='a'||caret<reference.first.start||caret>reference.last.end)continue;
  bool colLock=reference.first.colLock,rowLock=reference.first.rowLock;
  if(!colLock&&!rowLock){colLock=rowLock=true;}
  else if(colLock&&rowLock)colLock=false;
  else if(rowLock){rowLock=false;colLock=true;}
  else colLock=false;
  auto locked=[&](const ReferenceCell& cell){
   return (colLock?std::string("$"):std::string())+std::string(formula.substr(cell.letters,cell.endLetters-cell.letters))+(rowLock?"$":"")+std::string(formula.substr(cell.digits,cell.end-cell.digits));
  };
  auto result=std::string(formula.substr(0,reference.first.start))+locked(reference.first);
  if(reference.range)result+=std::string(formula.substr(reference.first.end,reference.last.start-reference.first.end))+locked(reference.last);
  result+=formula.substr(reference.last.end);return result;
 }
 return std::string(formula);
}
std::string renameSheetReferences(std::string_view formula,std::string_view oldName,std::string_view newName){
 auto equal=[](std::string_view a,std::string_view b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](unsigned char x,unsigned char y){return std::tolower(x)==std::tolower(y);});};
 std::string quoted="'";for(char c:newName){quoted+=c;if(c=='\'')quoted+='\'';}quoted+='\'';
 std::string out;
 for(std::size_t i=0;i<formula.size();){
  const auto start=i;
  if(formula[i]=='"'){out+=formula[i++];while(i<formula.size()){char c=formula[i++];out+=c;if(c=='"'){if(i<formula.size()&&formula[i]=='"')out+=formula[i++];else break;}}continue;}
  if(formula[i]=='['){int depth=0;do{if(formula[i]=='[')++depth;else if(formula[i]==']')--depth;out+=formula[i++];}while(i<formula.size()&&depth);continue;}
  if(formula[i]=='\''){std::string decoded;++i;while(i<formula.size()){char c=formula[i++];if(c=='\''){if(i<formula.size()&&formula[i]=='\''){decoded+='\'';++i;}else break;}else decoded+=c;}if(i<formula.size()&&formula[i]=='!'&&equal(decoded,oldName))out+=quoted;else out+=formula.substr(start,i-start);continue;}
  auto identifier=[](unsigned char c){return std::isalnum(c)||c=='_'||c=='.'||c>=128;};
  if(identifier(static_cast<unsigned char>(formula[i]))){while(i<formula.size()&&identifier(static_cast<unsigned char>(formula[i])))++i;auto token=formula.substr(start,i-start);if(i<formula.size()&&formula[i]=='!'&&(start==0||formula[start-1]!=']')&&equal(token,oldName))out+=quoted;else out+=token;continue;}
  out+=formula[i++];
 }
 return out;
}
}
