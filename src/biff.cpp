#include "tigersheets/biff.hpp"
#include "tigersheets/references.hpp"
#include <xls.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
namespace tigersheets {
namespace {
std::string number(double value){
 if(!std::isfinite(value))throw std::runtime_error("BIFF workbook contains a nonfinite number");
 char buffer[64];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);
 if(result.ec!=std::errc())throw std::runtime_error("Cannot decode BIFF numeric value");return {buffer,result.ptr};
}
std::string text(std::string value){
 auto upper=value;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
 static const std::set<std::string> errors{"#NULL!","#DIV/0!","#VALUE!","#REF!","#NAME?","#NUM!","#N/A","#SPILL!","#CALC!","#FIELD!"};
 if(value.empty()||upper=="TRUE"||upper=="FALSE"||errors.contains(upper)||std::isdigit(static_cast<unsigned char>(value[0]))||value.find_first_of("'=+-.@$\xC2\xE2\xC2")==0)return "'"+value;return value;
}
std::string errorValue(int code){switch(code){case 0:return "#NULL!";case 7:return "#DIV/0!";case 15:return "#VALUE!";case 23:return "#REF!";case 29:return "#NAME?";case 36:return "#NUM!";case 42:return "#N/A";default:throw std::runtime_error("BIFF workbook contains an invalid error value");}}
unsigned word(std::string_view bytes,std::size_t offset){if(offset+2>bytes.size())throw std::runtime_error("Truncated BIFF record");return static_cast<unsigned char>(bytes[offset])|(static_cast<unsigned char>(bytes[offset+1])<<8U);}
void records(std::string_view stream,std::size_t offset,const std::function<void(unsigned,std::string_view)>& visit){
 unsigned depth=0;while(offset+4<=stream.size()){
  const auto id=word(stream,offset),size=word(stream,offset+2);offset+=4;if(size>stream.size()-offset)throw std::runtime_error("Truncated BIFF stream");
  const auto record=stream.substr(offset,size);offset+=size;
  if(id==0x0809||id==0x0409||id==0x0209||id==0x0009)++depth;
  if(depth==1)visit(id,record);
  if(id==XLS_RECORD_EOF){if(depth==0)throw std::runtime_error("Invalid BIFF stream nesting");if(--depth==0)return;}
 }throw std::runtime_error("BIFF stream has no end record");
}
std::string workbookStream(xls::xlsWorkBook& book){
 if(!book.olestr||book.olestr->size>128ULL*1024*1024)throw std::runtime_error("Legacy workbook stream exceeds 128 MiB import limit");
 std::string stream(book.olestr->size,'\0');if(xls::ole2_seek(book.olestr,0)!=0||xls::ole2_read(stream.data(),1,stream.size(),book.olestr)!=static_cast<ssize_t>(stream.size()))throw std::runtime_error("Cannot read legacy workbook stream");return stream;
}
std::string rgb(unsigned red,unsigned green,unsigned blue){constexpr char hex[]="0123456789ABCDEF";std::string out="#";for(auto channel:{red,green,blue}){out+=hex[(channel>>4U)&15U];out+=hex[channel&15U];}return out;}
std::vector<std::string> palette(std::string_view stream){
 // Excel's BIFF8 indexed palette; indexes 0..7 are invariant system colors.
 constexpr std::array<unsigned,56> colors{0x000000,0xFFFFFF,0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,0x800000,0x008000,0x000080,0x808000,0x800080,0x008080,0xC0C0C0,0x808080,0x9999FF,0x993366,0xFFFFCC,0xCCFFFF,0x660066,0xFF8080,0x0066CC,0xCCCCFF,0x000080,0xFF00FF,0xFFFF00,0x00FFFF,0x800080,0x800000,0x008080,0x0000FF,0x00CCFF,0xCCFFFF,0xCCFFCC,0xFFFF99,0x99CCFF,0xFF99CC,0xCC99FF,0xFFCC99,0x3366FF,0x33CCCC,0x99CC00,0xFFCC00,0xFF9900,0xFF6600,0x666699,0x969696,0x003366,0x339966,0x003300,0x333300,0x993300,0x993366,0x333399,0x333333};
 std::vector<std::string> result;for(unsigned i=0;i<64;++i){const auto color=colors[i<8?i:i-8];result.push_back(rgb(color>>16U,(color>>8U)&255U,color&255U));}
 records(stream,0,[&](unsigned id,std::string_view data){if(id!=XLS_RECORD_PALETTE)return;const auto count=word(data,0);if(count>56||2+count*4>data.size())throw std::runtime_error("Invalid BIFF color palette");for(unsigned i=0;i<count;++i)result[i+8]=rgb(static_cast<unsigned char>(data[2+i*4]),static_cast<unsigned char>(data[3+i*4]),static_cast<unsigned char>(data[4+i*4]));});return result;
}
std::vector<Json> styles(const xls::xlsWorkBook& book,const std::vector<std::string>& colors){
 std::map<unsigned,std::string> formats{{0,"General"},{1,"0"},{2,"0.00"},{3,"#,##0"},{4,"#,##0.00"},{5,"\"$\"#,##0_);(\"$\"#,##0)"},{6,"\"$\"#,##0_);[Red](\"$\"#,##0)"},{7,"\"$\"#,##0.00_);(\"$\"#,##0.00)"},{8,"\"$\"#,##0.00_);[Red](\"$\"#,##0.00)"},{9,"0%"},{10,"0.00%"},{11,"0.00E+00"},{12,"# ?/?"},{13,"# ??/??"},{14,"mm-dd-yy"},{15,"d-mmm-yy"},{16,"d-mmm"},{17,"mmm-yy"},{18,"h:mm AM/PM"},{19,"h:mm:ss AM/PM"},{20,"h:mm"},{21,"h:mm:ss"},{22,"m/d/yy h:mm"},{37,"#,##0 ;(#,##0)"},{38,"#,##0 ;[Red](#,##0)"},{39,"#,##0.00;(#,##0.00)"},{40,"#,##0.00;[Red](#,##0.00)"},{41,"_(* #,##0_);_(* (#,##0);_(* \"-\"_);_(@_)"},{42,"_(\"$\"* #,##0_);_(\"$\"* (#,##0);_(\"$\"* \"-\"_);_(@_)"},{43,"_(* #,##0.00_);_(* (#,##0.00);_(* \"-\"??_);_(@_)"},{44,"_(\"$\"* #,##0.00_);_(\"$\"* (#,##0.00);_(\"$\"* \"-\"??_);_(@_)"},{45,"mm:ss"},{46,"[h]:mm:ss"},{47,"mmss.0"},{48,"##0.0E+0"},{49,"@"}};
 for(unsigned i=0;i<book.formats.count;++i)formats[book.formats.format[i].index]=book.formats.format[i].value?book.formats.format[i].value:"General";
 const auto color=[&](unsigned index){return index<colors.size()?colors[index]:std::string("#000000");};
 constexpr std::array<const char*,8> horizontal{"general","left","center","right","fill","justify","centerContinuous","distributed"};
 constexpr std::array<const char*,5> vertical{"top","center","bottom","justify","distributed"};
 constexpr std::array<const char*,14> borderNames{"none","thin","medium","dashed","dotted","thick","double","hair","mediumDashed","dashDot","mediumDashDot","dashDotDot","mediumDashDotDot","slantDashDot"};
 std::vector<Json> result;for(unsigned i=0;i<book.xfs.count;++i){
  const auto& xf=book.xfs.xf[i];const auto fontIndex=xf.font>=4?xf.font-1:xf.font;if(fontIndex>=book.fonts.count)throw std::runtime_error("BIFF cell refers to a missing font");const auto& font=book.fonts.font[fontIndex];
  if(!formats.contains(xf.format))throw std::runtime_error("BIFF workbook uses an unsupported locale number format");
  const auto verticalIndex=(xf.align>>4U)&7U;if(verticalIndex>=vertical.size())throw std::runtime_error("Invalid BIFF vertical alignment");
  Json style={{"fontFamily",font.name?font.name:"Arial"},{"fontSize",font.height/20.0},{"bold",font.bold>=700},{"italic",(font.flag&2)!=0},{"underline",font.underline!=0},{"color",color(font.color)},{"fill",((xf.linecolor>>26U)&63U)==1?color(xf.groundcolor&127U):std::string()},{"numberFormat",formats.at(xf.format)},{"alignment",horizontal[xf.align&7U]},{"verticalAlignment",vertical[verticalIndex]},{"wrap",(xf.align&8)!=0},{"justifyLastLine",(xf.align&128)!=0},{"shrinkToFit",(xf.ident&16)!=0},{"readingOrder",(xf.ident>>6)&3},{"textRotation",xf.rotation},{"locked",(xf.type&1)!=0},{"hidden",(xf.type&2)!=0},{"indent",xf.ident&15},{"border",Json::object()}};
  const std::array<unsigned,4> borderColors{(xf.linestyle>>16U)&127U,(xf.linestyle>>23U)&127U,xf.linecolor&127U,(xf.linecolor>>7U)&127U};
  const std::array<const char*,4> sides{"left","right","top","bottom"};for(unsigned edge=0;edge<4;++edge){const auto kind=(xf.linestyle>>(edge*4U))&15U;if(kind>=borderNames.size())throw std::runtime_error("Invalid BIFF border style");if(kind)style["border"][sides[edge]]={{"style",borderNames[kind]},{"color",color(borderColors[edge])}};}
  const auto diagonalFlags=(xf.linestyle>>30U)&3U,diagonalKind=(xf.linecolor>>21U)&15U;if(diagonalFlags){if(diagonalKind>=borderNames.size())throw std::runtime_error("Invalid BIFF diagonal border style");if(diagonalKind){const Json diagonal={{"style",borderNames[diagonalKind]},{"color",color((xf.linecolor>>14U)&127U)}};if(diagonalFlags&1U)style["border"]["diagonalDown"]=diagonal;if(diagonalFlags&2U)style["border"]["diagonalUp"]=diagonal;}}
  result.push_back(std::move(style));
 }return result;
}
void layout(std::string_view stream,std::size_t offset,Json& model,const std::vector<Json>& styles){
 Json rows=Json::array(),columns=Json::array(),merges=Json::array(),hidden=Json::array();std::map<unsigned,Json> colEntries;std::map<unsigned,unsigned> colStyles;
 Json view=Json::object();unsigned windows=0;int selectionPane=-1;std::string selection;const auto address=[](unsigned row,unsigned col){return columnName(static_cast<int>(col)+1)+std::to_string(row+1);};
 records(stream,offset,[&](unsigned id,std::string_view data){
  if(id==XLS_RECORD_WINDOW2){if(++windows==1){model["showGridLines"]=(word(data,0)&2)!=0;const auto row=word(data,2),col=word(data,4);if(col>255)throw std::runtime_error("Invalid BIFF worksheet view column");view["topLeftCell"]=address(row,col);if(data.size()>=14){const auto zoom=word(data,12);if(zoom>400||(zoom>0&&zoom<10))throw std::runtime_error("Invalid BIFF worksheet zoom");view["zoomScale"]=zoom?zoom:100;}}}
  else if(id==0x00A0&&windows==1){const auto numerator=word(data,0),denominator=word(data,2);if(numerator<1||numerator>32767||denominator<1||denominator>32767||numerator*10<denominator||numerator>denominator*4)throw std::runtime_error("Invalid BIFF worksheet zoom fraction");view["zoomScale"]=static_cast<int>(std::round(numerator*100.0/denominator));}
  else if(id==0x001D&&windows==1){if(data.size()<9)throw std::runtime_error("Truncated BIFF worksheet selection");const int pane=static_cast<unsigned char>(data[0]);const auto count=word(data,7);if(pane>3||count<1||9+count*6>data.size()||word(data,3)>255)throw std::runtime_error("Invalid BIFF worksheet selection");if(selectionPane==-1||(pane==3&&selectionPane!=3)){selection.clear();selectionPane=pane;}if(pane==selectionPane){view["activeCell"]=address(word(data,1),word(data,3));for(unsigned index=0;index<count;++index){const auto start=9+index*6;const auto row1=word(data,start),row2=word(data,start+2);const unsigned col1=static_cast<unsigned char>(data[start+4]),col2=static_cast<unsigned char>(data[start+5]);if(row1>row2||col1>col2)throw std::runtime_error("Invalid BIFF worksheet selected range");if(!selection.empty())selection+=' ';const auto first=address(row1,col1),last=address(row2,col2);selection+=first==last?first:first+":"+last;}}}
  else if(id==XLS_RECORD_DEFAULTROWHEIGHT){const auto height=word(data,data.size()==2?0:2)&0x7FFFU;if(height)model["defaultRowHeight"]=height/20.0;}
  else if(id==XLS_RECORD_ROW){const auto index=word(data,0)+1,height=word(data,6)&0x7FFFU,flags=word(data,12);Json entry={{"index",index}};if(height)entry["size"]=height/20.0;if(flags&0x20U){entry["hidden"]=true;hidden.push_back(index);}if(entry.size()>1)rows.push_back(std::move(entry));}
  else if(id==XLS_RECORD_COLINFO){const auto first=word(data,0),last=word(data,2),width=word(data,4),flags=word(data,8);if(first>last||first>255||last>256)throw std::runtime_error("Invalid BIFF column range");
   // Some Excel-compatible exporters use 256 as the all-column endpoint.
   const auto xf=word(data,6);if(xf>=styles.size())throw std::runtime_error("BIFF column refers to a missing style");for(unsigned index=first;index<=std::min(last,255U);++index){Json entry={{"index",index+1}};if(width)entry["size"]=width/256.0;if((flags&1U)||width==0)entry["hidden"]=true;if(flags&4U)entry["bestFit"]=true;colEntries[index]=std::move(entry);colStyles[index]=xf;}}
  else if(id==XLS_RECORD_MERGEDCELLS){const auto count=word(data,0);if(2+count*8>data.size())throw std::runtime_error("Invalid BIFF merged ranges");for(unsigned i=0;i<count;++i){const auto base=2+i*8;merges.push_back({{"row1",word(data,base)+1},{"row2",word(data,base+2)+1},{"col1",word(data,base+4)+1},{"col2",word(data,base+6)+1}});}}
 });
 if(!selection.empty())view["selection"]=std::move(selection);if(!view.empty())model["nativeView"]=std::move(view);
 if(colStyles.size()==256&&std::all_of(colStyles.begin(),colStyles.end(),[&](const auto& entry){return entry.second==colStyles.begin()->second;}))model["defaultStyle"]=styles.at(colStyles.begin()->second);
 for(auto& [index,entry]:colEntries)columns.push_back(std::move(entry));if(!rows.empty())model["rowLayout"]=std::move(rows);if(!columns.empty())model["columnLayout"]=std::move(columns);if(!merges.empty())model["mergedRanges"]=std::move(merges);if(!hidden.empty())model["hiddenRows"]=std::move(hidden);
}
void preflightAllocation(std::string_view stream,std::size_t offset){
 // libxls materializes a rectangular working table even for sparse BIFF data.
 // Bound its allocation before calling the native worksheet parser.
 std::size_t rows=1,columns=1;
 records(stream,offset,[&](unsigned id,std::string_view data){
  if(id==XLS_RECORD_ROW){rows=std::max(rows,static_cast<std::size_t>(word(data,0))+1);columns=std::max(columns,static_cast<std::size_t>(word(data,4)));}
  else if(id==XLS_RECORD_MULRK||id==XLS_RECORD_MULBLANK){rows=std::max(rows,static_cast<std::size_t>(word(data,0))+1);columns=std::max(columns,static_cast<std::size_t>(word(data,data.size()-2))+1);}
  else if(id==XLS_RECORD_NUMBER||id==XLS_RECORD_RK||id==XLS_RECORD_LABELSST||id==XLS_RECORD_BLANK||id==XLS_RECORD_LABEL||id==XLS_RECORD_RSTRING||id==XLS_RECORD_FORMULA||id==XLS_RECORD_FORMULA_ALT||id==XLS_RECORD_BOOLERR){rows=std::max(rows,static_cast<std::size_t>(word(data,0))+1);columns=std::max(columns,static_cast<std::size_t>(word(data,2))+1);}
  if(columns>256||rows*columns>1048576)throw std::runtime_error("Legacy worksheet exceeds the native parser allocation budget (1048576 cells)");
 });
}
}
bool isCompoundWorkbook(std::string_view bytes) noexcept{return bytes.size()>=8&&bytes.substr(0,8)==std::string_view("\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1",8);}
Json readBiffWorkbook(std::string_view bytes){
 if(bytes.size()>256ULL*1024*1024)throw std::runtime_error("Legacy workbook exceeds 256 MiB import limit");
 xls::xls_error_t error=xls::LIBXLS_OK;
 std::unique_ptr<xls::xlsWorkBook,decltype(&xls::xls_close_WB)> book(xls::xls_open_buffer(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),"UTF-8",&error),xls::xls_close_WB);
 if(!book)throw std::runtime_error(std::string("Cannot read legacy Excel workbook: ")+xls::xls_getError(error));
 const auto stream=workbookStream(*book);const auto cellStyles=styles(*book,palette(stream));
 Json result={{"revision",0},{"sheets",Json::array()},{"settings",{{"date1904",book->is1904!=0}}}};
 if(book->sheets.count==0||book->sheets.count>4096)throw std::runtime_error("Legacy workbook has an invalid worksheet count");
 bool primaryWindow=false;records(stream,0,[&](unsigned id,std::string_view data){if(id==XLS_RECORD_WINDOW1&&!primaryWindow){const auto active=word(data,10);if(active>=book->sheets.count)throw std::runtime_error("Invalid BIFF active worksheet");result["activeSheetIndex"]=active;primaryWindow=true;}});
 for(unsigned int index=0;index<book->sheets.count;++index){
  if(book->sheets.sheet[index].type!=0)throw std::runtime_error("Legacy workbook contains an unsupported non-worksheet sheet");
  preflightAllocation(stream,book->sheets.sheet[index].filepos);
  std::unique_ptr<xls::xlsWorkSheet,decltype(&xls::xls_close_WS)> sheet(xls::xls_getWorkSheet(book.get(),static_cast<int>(index)),xls::xls_close_WS);
  if(!sheet)throw std::runtime_error("Cannot allocate legacy worksheet");
  error=xls::xls_parseWorkSheet(sheet.get());if(error!=xls::LIBXLS_OK)throw std::runtime_error(std::string("Cannot read legacy worksheet: ")+xls::xls_getError(error));
  Json model={{"id","sheet-"+std::to_string(index+1)},{"name",book->sheets.sheet[index].name},{"cells",Json::array()}};
  if(!cellStyles.empty())model["defaultStyle"]=cellStyles.at(cellStyles.size()>15?15:0);
  layout(stream,book->sheets.sheet[index].filepos,model,cellStyles);
  for(unsigned int row=0;row<=sheet->rows.lastrow;++row)for(unsigned int col=0;col<sheet->rows.row[row].cells.count;++col){
   const auto& value=sheet->rows.row[row].cells.cell[col];std::string input;
   switch(value.id){
    case XLS_RECORD_NUMBER:case XLS_RECORD_RK:input=number(value.d);break;
    case XLS_RECORD_LABELSST:case XLS_RECORD_LABEL:case XLS_RECORD_RSTRING:input=text(value.str?value.str:"");break;
    case XLS_RECORD_BOOLERR:input=value.str&&std::string_view(value.str)=="error"?errorValue(static_cast<int>(value.d)):(value.d==0?"FALSE":"TRUE");break;
    case XLS_RECORD_BLANK:if(value.xf==0)continue;break;
    case XLS_RECORD_FORMULA:case XLS_RECORD_FORMULA_ALT:throw std::runtime_error("Legacy workbook contains formula tokens not yet supported by this importer");
    default:continue;
   }
   if(value.xf>=cellStyles.size())throw std::runtime_error("BIFF cell refers to a missing style");
   auto style=cellStyles.at(value.xf);if(value.id==XLS_RECORD_LABELSST||value.id==XLS_RECORD_LABEL||value.id==XLS_RECORD_RSTRING){if((book->xfs.xf[value.xf].type&8U)!=0){if(input.empty()||input.front()!='\'')input="'"+input;}else if(!input.empty()&&input.front()=='\'')style["quotePrefix"]=false;}
   model["cells"].push_back({{"row",row+1},{"col",col+1},{"input",input},{"style",std::move(style)}});
  }
  result["sheets"].push_back(std::move(model));
 }
 WorkbookSession validated(result);return validated.snapshot();
}
}
