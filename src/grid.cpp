#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/formatting.hpp"
#include "tigersheets/selection_statistics.hpp"
#include "tigersheets/formula_error_checks.hpp"
#include "tigersheets/font_presentation.hpp"
#include <QColor>
#include <QFont>
#include <QLocale>
#include <QRegularExpression>
#include <QDate>
#include <QTime>
#include <QUuid>
#include <cmath>
#include <algorithm>
#include <charconv>
#include <set>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
namespace tigersheets {
namespace {
QString numberText(double value,const Json& style,bool date1904){
 // The formatter uses a fixed December 30 origin; Excel serials before its
 // fictitious February 29 require one extra day only when rendering a date.
 if(!date1904&&value>0&&value<60&&dateNumberFormat(style.value("numberFormat",std::string())))value+=1;
 return QString::fromStdString(formattedValue(value,style,date1904));
}
std::optional<double> numericLiteral(const std::string& input){
 if(input.empty()||input[0]=='\''||input[0]=='=')return std::nullopt;double number=0;const auto parsed=std::from_chars(input.data(),input.data()+input.size(),number);bool okay=parsed.ec==std::errc()&&parsed.ptr==input.data()+input.size();if(!okay)number=QString::fromStdString(input).toDouble(&okay);return okay&&std::isfinite(number)?std::optional<double>(number):std::nullopt;
}
bool percentageFormat(std::string_view format){
 bool quoted=false;for(std::size_t i=0;i<format.size();++i){const char ch=format[i];if(ch=='"'){quoted=!quoted;continue;}if(quoted)continue;if(ch=='\\'||ch=='_'||ch=='*'){if(i+1<format.size())++i;continue;}if(ch=='['){while(i+1<format.size()&&format[i+1]!=']')++i;if(i+1<format.size())++i;continue;}if(ch=='%')return true;}return false;
}
QString inferredDateFormulaFormat(const QString& input){
 if(!input.startsWith('='))return {};static const QRegularExpression expression("^=\\s*(EDATE|DATE|TODAY|NOW)\\s*\\(",QRegularExpression::CaseInsensitiveOption);const auto head=expression.match(input);if(!head.hasMatch())return {};
 int depth=1;QChar quote;
 for(int pos=head.capturedLength();pos<input.size();++pos){const auto ch=input.at(pos);if(!quote.isNull()){if(ch==quote){if(pos+1<input.size()&&input.at(pos+1)==quote)++pos;else quote=QChar();}continue;}if(ch=='\"'||ch=='\''){quote=ch;continue;}if(ch=='(')++depth;else if(ch==')'&&--depth==0)return input.mid(pos+1).trimmed().isEmpty()?(head.captured(1).compare("NOW",Qt::CaseInsensitive)==0?QString("m/d/yyyy h:mm"):QString("m/d/yyyy")):QString();}
 return {};
}
struct DateLiteral {QDate date;QString format;};
std::optional<DateLiteral> dateLiteral(const QString& input){
 static const QRegularExpression iso("^([0-9]{4})([-/])([0-9]{1,2})\\2([0-9]{1,2})$");
 static const QRegularExpression localized("^([0-9]{1,2})([-/])([0-9]{1,2})\\2([0-9]{2}|[0-9]{4})$");
 const auto token=[](const QString& input,QChar symbol){return QString(input.size()==2?2:1,symbol);};
 const auto standard=iso.match(input);if(standard.hasMatch()){
  const int year=standard.captured(1).toInt();const QDate date(year,standard.captured(3).toInt(),standard.captured(4).toInt());const auto separator=standard.captured(2);
  return year>=1900&&date.isValid()?std::optional<DateLiteral>({date,"yyyy"+separator+token(standard.captured(3),'m')+separator+token(standard.captured(4),'d')}):std::nullopt;
 }
 const auto separated=localized.match(input);if(!separated.hasMatch())return std::nullopt;
 const int first=separated.captured(1).toInt(),second=separated.captured(3).toInt();int year=separated.captured(4).toInt();if(separated.captured(4).size()==2)year+=year<30?2000:1900;if(year<1900||first<1||second<1)return std::nullopt;
 const bool dayFirst=first>12;const QDate date(year,dayFirst?second:first,dayFirst?first:second);
 const auto separator=separated.captured(2),format=token(separated.captured(1),dayFirst?'d':'m')+separator+token(separated.captured(3),dayFirst?'m':'d')+separator+(separated.captured(4).size()==2?"yy":"yyyy");
 return date.isValid()?std::optional<DateLiteral>({date,format}):std::nullopt;
}
QString numericEditText(const QString& input,const Json& style,bool date1904){
 const auto number=numericLiteral(input.toStdString());const auto format=style.value("numberFormat",std::string());
 if(number&&percentageFormat(format)){
  // Shift the shortest round-trip decimal, avoiding binary multiplication
  // artifacts such as 0.07 becoming 7.000000000000001 in the formula bar.
  char buffer[128];const auto converted=std::to_chars(buffer,buffer+sizeof(buffer),*number);if(converted.ec!=std::errc())return input;
  auto digits=QString::fromLatin1(buffer,converted.ptr-buffer);const bool negative=digits.startsWith('-');if(negative)digits.remove(0,1);
  int exponent=0;const int e=digits.indexOf('e');if(e>=0){exponent=digits.mid(e+1).toInt();digits.truncate(e);}
  int point=digits.indexOf('.');if(point<0)point=digits.size();else digits.remove(point,1);point+=exponent+2;
  if(point<=0)digits="0."+QString(-point,'0')+digits;else if(point>=digits.size())digits+=QString(point-digits.size(),'0');else digits.insert(point,'.');
  while(digits.size()>1&&digits[0]=='0'&&digits[1]!='.')digits.remove(0,1);
  if(digits.contains('.')){while(digits.endsWith('0'))digits.chop(1);if(digits.endsWith('.'))digits.chop(1);}
  return (negative?"-":"")+digits+"%";
 }
 if(!number||std::trunc(*number)!=*number||!dateNumberFormat(format))return input;
 // Keep fractional times, out-of-range values and Excel's fictitious leap day
 // as serials so accepting an unchanged editor cannot alter their value.
 if(*number<-1460||*number>2958465||(!date1904&&*number==60))return input;
 const auto serial=static_cast<qint64>(*number);const auto date=date1904?QDate(1904,1,1).addDays(serial):QDate(1899,12,31).addDays(serial-(serial>60?1:0));
 if(!date.isValid()||date.year()<1900||date.year()>9999)return input;
 static const QRegularExpression yearFirst("^\\s*(?:\\[[^\\]]*\\]\\s*)*yyyy[-/]m{1,2}[-/]d{1,2}(?:\\s|;|$)",QRegularExpression::CaseInsensitiveOption);
 return yearFirst.match(QString::fromStdString(format)).hasMatch()?date.toString(Qt::ISODate):date.toString("M/d/yyyy");
}
const Json* previewCell(const Json& book,const QString& sheet,int row,int col){
 const auto id=sheet.toStdString();for(const auto& entry:book.at("sheets"))if(entry.at("id")==id){const auto& cells=entry.at("cells").get_ref<const Json::array_t&>();const auto coordinate=std::pair{row+1,col+1};const auto found=std::lower_bound(cells.begin(),cells.end(),coordinate,[](const Json& cell,const auto& point){return std::pair{cell.at("row").get<int>(),cell.at("col").get<int>()}<point;});return found!=cells.end()&&found->at("row")==row+1&&found->at("col")==col+1?&*found:nullptr;}return nullptr;
}
const Json* mergedOverlap(const Json& book,const std::string& id,int row1,int col1,int row2,int col2){for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==id&&sheet.contains("mergedRanges"))for(const auto& merge:sheet.at("mergedRanges"))if(merge.at("row1")<=row2&&merge.at("row2")>=row1&&merge.at("col1")<=col2&&merge.at("col2")>=col1)return &merge;return nullptr;}
std::pair<int,int> sparseDimensions(const Json& clip){
 const int rows=clip.at("rows"),columns=clip.at("columns"),sr=clip.at("sourceRow"),sc=clip.at("sourceCol");if(rows<1||columns<1||rows>MaxRows||columns>MaxColumns||sr<0||sc<0||rows>MaxRows-sr||columns>MaxColumns-sc||!((sr==0&&rows==MaxRows)||(sc==0&&columns==MaxColumns)))throw std::runtime_error("Invalid sparse whole-row/column clipboard bounds");
 const auto& entries=clip.at("entries");if(!entries.is_array()||entries.size()>1000000||!clip.at("cells").empty()||!clip.value("defaultStyle",Json::object()).is_object())throw std::runtime_error("Invalid sparse clipboard cells");std::set<std::pair<int,int>> seen;for(const auto& entry:entries){const int row=entry.at("row"),col=entry.at("col");if(row<0||col<0||row>=rows||col>=columns||!entry.at("input").is_string()||!entry.value("style",Json::object()).is_object()||!seen.emplace(row,col).second)throw std::runtime_error("Invalid sparse clipboard coordinate");}return {rows,columns};
}
}
WorksheetModel::WorksheetModel(WorkbookSession& s,QObject*p):QAbstractTableModel(p),session_(s),clipboardOwner_(QUuid::createUuid().toString()){refresh();}
WorksheetModel::~WorksheetModel(){previewView_=nullptr;}
int WorksheetModel::rowCount(const QModelIndex& p)const{return p.isValid()?0:MaxRows;}
int WorksheetModel::columnCount(const QModelIndex& p)const{return p.isValid()?0:MaxColumns;}
QString WorksheetModel::sheetId()const{return sheet_;}
void WorksheetModel::setSheetId(QString id){if(id.isEmpty())clipboardOwner_=QUuid::createUuid().toString();sheet_=id;if(previewView_){readOnly_=true;loadSnapshot(*previewView_);}else refresh();}
void WorksheetModel::refresh(){
 previewView_=nullptr;preview_.reset();readOnly_=false;
 // Explicit workbook/context loads end transient interactions. Ordinary edit
 // publication below must retain a physically held selection.
 setProperty("worksheet.contextGeneration",QVariant::fromValue<qulonglong>(property("worksheet.contextGeneration").toULongLong()+1));
 loadSnapshot(session_.snapshotView());
}
void WorksheetModel::showPreview(const Json& candidate){previewView_=nullptr;preview_=candidate;previewView_=&*preview_;readOnly_=true;loadSnapshot(*previewView_);}
void WorksheetModel::showPreviewView(const Json& candidate){const bool owned=preview_&&&candidate==&*preview_;previewView_=nullptr;if(!owned)preview_.reset();previewView_=&candidate;readOnly_=true;loadSnapshot(candidate);}
void WorksheetModel::loadSnapshot(const Json& book){
 setProperty("formula.errorChecks",QVariantMap());
 const auto settings=book.value("settings",Json::object());date1904_=settings.is_object()&&settings.contains("date1904")&&settings["date1904"].is_boolean()&&settings["date1904"].get<bool>();cells_.clear();values_.clear();valueTypes_.clear();valuesCurrent_=false;displayRevision_=book.value("revision",std::uint64_t{});
 const auto id=sheet_.toStdString();const bool present=std::any_of(book.at("sheets").begin(),book.at("sheets").end(),[&](const Json& sheet){return sheet.at("id")==id;});if(!present)sheet_=book.at("sheets").empty()?QString():QString::fromStdString(book.at("sheets")[0].at("id").get<std::string>());
 defaultStyle_=Json::object();for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==sheet_.toStdString()){defaultStyle_=sheet.value("defaultStyle",Json::object());break;}
 emit dataChanged(index(0,0),index(MaxRows-1,MaxColumns-1));
}
QString WorksheetModel::raw(int row,int col)const{const auto* cell=previewView_?previewCell(*previewView_,sheet_,row,col):session_.cellAt(sheet_.toStdString(),row+1,col+1);return cell?QString::fromStdString(cell->value("input",std::string())):QString();}
std::optional<double> WorksheetModel::numericValue(int row,int col)const{
 if(row<0||col<0||row>=MaxRows||col>=MaxColumns)return std::nullopt;
 if(valuesCurrent_){const auto cached=values_.find({row,col});if(cached!=values_.end()){const auto type=valueTypes_.find({row,col});if(!cached->second.is_number()||(type!=valueTypes_.end()&&type->second=="error"))return std::nullopt;const auto number=cached->second.get<double>();return std::isfinite(number)?std::optional<double>(number):std::nullopt;}}
 const auto* cell=previewView_?previewCell(*previewView_,sheet_,row,col):session_.cellAt(sheet_.toStdString(),row+1,col+1);return cell?numericLiteral(cell->at("input").get_ref<const std::string&>()):std::nullopt;
}
void WorksheetModel::setCalculated(const Json& result){
 if(result.value("revision",std::uint64_t{})!=displayRevision_)return;
 int firstRow=MaxRows,lastRow=-1,firstCol=MaxColumns,lastCol=-1;auto affected=[&](const auto& key){firstRow=std::min(firstRow,key.first);lastRow=std::max(lastRow,key.first);firstCol=std::min(firstCol,key.second);lastCol=std::max(lastCol,key.second);};for(const auto& [key,value]:values_){static_cast<void>(value);affected(key);}
 const auto previousChecks=property("formula.errorChecks").toMap();for(auto it=previousChecks.begin();it!=previousChecks.end();++it){const auto coordinate=it.key().split(',');if(coordinate.size()==2)affected(std::pair{coordinate[0].toInt(),coordinate[1].toInt()});}
 const auto iteration=result.find("iteration");const bool converged=iteration==result.end()||!iteration->is_object()||iteration->value("converged",true);valuesCurrent_=result.value("status",std::string("ok"))=="ok"&&converged;values_.clear();valueTypes_.clear();
 const auto cache=result.find(result.contains("displayValues")?"displayValues":"values");if(valuesCurrent_&&cache!=result.end())for(const auto& value:*cache)if(value.value("sheet",std::string())==sheet_.toStdString()){const auto key=std::pair{value.at("row").get<int>()-1,value.at("col").get<int>()-1};values_[key]=value.at("value");valueTypes_[key]=value.value("type",std::string());affected(key);}
 QVariantMap checks;if(valuesCurrent_)for(const auto& check:result.value("errorChecks",Json::array()))if(check.value("sheet",std::string())==sheet_.toStdString()&&(check.value("code",std::string())=="formulaRange"||check.value("code",std::string())=="numberStoredAsText")){const int row=check.value("row",0)-1,col=check.value("col",0)-1;if(row>=0&&row<MaxRows&&col>=0&&col<MaxColumns){checks[QString::number(row)+","+QString::number(col)]=true;affected(std::pair{row,col});}}setProperty("formula.errorChecks",checks);
 if(lastRow>=firstRow)emit dataChanged(index(firstRow,firstCol),index(lastRow,lastCol));
}
QVariant WorksheetModel::data(const QModelIndex&i,int role)const{
 if(!i.isValid())return {};
 if(role==FormulaErrorCheckRole){
  const auto input=raw(i.row(),i.column()).toStdString();
  if(input.starts_with('\''))for(const auto& sheet:workbookView().at("sheets"))if(sheet.at("id")==sheet_.toStdString())return numberStoredAsTextWarning(sheet,i.row()+1,i.column()+1,input);
  return property("formula.errorChecks").toMap().contains(QString::number(i.row())+","+QString::number(i.column()));
 }
 if(role==Qt::EditRole){const auto input=raw(i.row(),i.column());const auto style=cellStyle(i.row(),i.column());if(input.startsWith('\'')&&style.contains("quotePrefix")&&!style.at("quotePrefix").get<bool>())return input.mid(1);return numericEditText(input,style,date1904_);}
 if(role==Qt::DisplayRole){
  if(property("showFormulas").toBool()){const auto input=raw(i.row(),i.column());if(input.startsWith('='))return input;}
  auto style=cellStyle(i.row(),i.column());
  auto v=values_.find({i.row(),i.column()});
  if(v!=values_.end()){if(v->second.is_number())return numberText(v->second.get<double>(),style,date1904_);if(v->second.is_boolean())return v->second.get<bool>()?QString("TRUE"):QString("FALSE");if(v->second.is_string())return QString::fromStdString(v->second.get<std::string>());}
  auto text=raw(i.row(),i.column());if(text.startsWith("'")){text.remove(0,1);return text;}bool ok=false;double number=text.toDouble(&ok);if(ok)return numberText(number,style,date1904_);return text;
 }
 auto style=cellStyle(i.row(),i.column());
 if(role==Qt::BackgroundRole&&style.contains("fill")&&!style.at("fill").get_ref<const std::string&>().empty())return QColor(QString::fromStdString(style["fill"].get<std::string>()));
 if(role==Qt::ForegroundRole){
  const auto format=style.value("numberFormat",std::string("General"));
  if(format.find('[')!=std::string::npos)if(const auto value=numericValue(i.row(),i.column()))try{
   const auto formatted=formatCellNumber(*value,format);
   constexpr const char* namedColors[]={"#000000","#ffffff","#ff0000","#00ff00","#0000ff","#ffff00","#ff00ff","#00ffff"};
   if(formatted.colorIndex&&*formatted.colorIndex>=1&&*formatted.colorIndex<=std::size(namedColors))return QColor(namedColors[*formatted.colorIndex-1]);
  }catch(const std::exception&){}
  if(style.contains("color")&&!style.at("color").get_ref<const std::string&>().empty())return QColor(QString::fromStdString(style["color"].get<std::string>()));
 }
 if(role==Qt::FontRole){QFont f(QString::fromStdString(style.value("fontFamily",std::string("Calibri"))),style.value("fontSize",11));f.setBold(style.value("bold",false));f.setItalic(style.value("italic",false));f.setUnderline(style.value("underline",false));return worksheetPresentationFont(f);}
 if(role==Qt::TextAlignmentRole){const auto vertical=style.value("verticalAlignment",std::string("bottom"));const auto verticalFlag=vertical=="top"?Qt::AlignTop:vertical=="center"?Qt::AlignVCenter:Qt::AlignBottom;auto alignment=style.value("alignment",std::string());if(alignment=="left")return int(Qt::AlignLeft|verticalFlag);if(alignment=="center")return int(Qt::AlignHCenter|verticalFlag);if(alignment=="right")return int(Qt::AlignRight|verticalFlag);bool ok=false;raw(i.row(),i.column()).toDouble(&ok);auto v=values_.find({i.row(),i.column()});return int((ok||(v!=values_.end()&&v->second.is_number())?Qt::AlignRight:Qt::AlignLeft)|verticalFlag);}
 return {};
}
QVariant WorksheetModel::headerData(int s,Qt::Orientation o,int role)const{if(role!=Qt::DisplayRole)return {};return o==Qt::Horizontal?QVariant(QString::fromStdString(columnName(s+1))):QVariant(s+1);}
Qt::ItemFlags WorksheetModel::flags(const QModelIndex&i)const{return i.isValid()?Qt::ItemIsEnabled|Qt::ItemIsSelectable|(readOnly_?Qt::NoItemFlags:Qt::ItemIsEditable):Qt::NoItemFlags;}
Json WorksheetModel::cellStyle(int row,int col)const{
 const auto* cell=previewView_?previewCell(*previewView_,sheet_,row,col):session_.cellAt(sheet_.toStdString(),row+1,col+1);
 // Native XFs describe a complete style, including absent emphasis and fill.
 if(cell&&cell->contains("style")&&cell->at("style").contains("ooxmlStyleId"))return cell->at("style");
 auto style=defaultStyle_;if(cell&&cell->contains("style"))style.update(cell->at("style"));return style;
}
Json WorksheetModel::copyRange(int r1,int c1,int r2,int c2)const{
 if(r1<0||c1<0||r2<r1||c2<c1||r2>=MaxRows||c2>=MaxColumns)return nullptr;
 Json result={{"sourceRow",r1},{"sourceCol",c1},{"sourceHasMerges",mergedOverlap(workbookView(),sheet_.toStdString(),r1+1,c1+1,r2+1,c2+1)!=nullptr},{"cells",Json::array()}};
 if(static_cast<long long>(r2-r1+1)*(c2-c1+1)>1000000){if(!((r1==0&&r2==MaxRows-1)||(c1==0&&c2==MaxColumns-1)))return nullptr;result["sparse"]=true;result["rows"]=r2-r1+1;result["columns"]=c2-c1+1;result["entries"]=Json::array();result["defaultStyle"]=defaultStyle_;for(const auto& sheet:workbookView().at("sheets"))if(sheet.at("id")==sheet_.toStdString())for(const auto& stored:sheet.at("cells")){const int r=stored.at("row").get<int>()-1,c=stored.at("col").get<int>()-1;if(r<r1||r>r2||c<c1||c>c2)continue;Json entry={{"row",r-r1},{"col",c-c1},{"input",stored.at("input")},{"style",cellStyle(r,c)}};if(const auto found=values_.find({r,c});valuesCurrent_&&found!=values_.end()){entry["value"]=found->second;entry["valueType"]=valueTypes_.at({r,c});}result["entries"].push_back(std::move(entry));}if(result.at("entries").size()>1000000)return nullptr;return result;}
 for(int r=r1;r<=r2;++r){Json row=Json::array();for(int c=c1;c<=c2;++c){Json cell={{"input",raw(r,c).toStdString()},{"style",cellStyle(r,c)}};auto value=values_.find({r,c});if(valuesCurrent_&&value!=values_.end()){cell["value"]=value->second;cell["valueType"]=valueTypes_.at({r,c});}row.push_back(cell);}result["cells"].push_back(row);}
 return result;
}
Json WorksheetModel::cutRange(int r1,int c1,int r2,int c2)const{auto clip=copyRange(r1,c1,r2,c2);if(!clip.is_null()){clip["cut"]=true;clip["sourceSheet"]=sheet_.toStdString();clip["sourceOwner"]=clipboardOwner_.toStdString();clip["sourceRevision"]=displayRevision_;}return clip;}
bool WorksheetModel::pasteRange(int row,int col,const Json& clip,const std::string& requestedMode,bool skipBlanks,int targetRows,int targetColumns){
 error_.clear();
 const bool transpose=requestedMode=="transpose"||requestedMode.ends_with(".transpose");
 const auto mode=requestedMode=="transpose"?std::string("all"):transpose?requestedMode.substr(0,requestedMode.size()-10):requestedMode;
 if(!clip.is_object()||!clip.contains("cells")||!clip["cells"].is_array())return false;
 if(mode!="all"&&mode!="formulas"&&mode!="values"&&mode!="formats"){error_="This Paste Special mode is not supported.";return false;}
 if(clip.value("cut",false)){if(mode!="all"||transpose||skipBlanks){error_="Use ordinary Paste to move cut cells, or Copy before using Paste Special.";return false;}return moveRange(row,col,clip);}
 if(clip.value("sparse",false)){try{
  if(transpose)throw std::runtime_error("A whole-row or whole-column range cannot be transposed within worksheet bounds");
  const auto [sourceRows,sourceColumns]=sparseDimensions(clip);if(mode!="all"&&mode!="formulas"&&mode!="values"&&mode!="formats")throw std::runtime_error("This sparse whole-row/column paste mode is not supported");const int height=std::max(sourceRows,targetRows),width=std::max(sourceColumns,targetColumns);if(row<0||col<0||row>=MaxRows||col>=MaxColumns||height>MaxRows-row||width>MaxColumns-col||height%sourceRows||width%sourceColumns||(targetRows>sourceRows&&targetColumns<sourceColumns)||(targetColumns>sourceColumns&&targetRows<sourceRows))throw std::runtime_error("The sparse destination must contain whole source ranges within worksheet bounds");
  const auto copies=static_cast<std::uint64_t>(height/sourceRows)*(width/sourceColumns);if(copies*clip.at("entries").size()>1000000)throw std::runtime_error("Sparse paste exceeds one million stored cells");if(clip.value("sourceHasMerges",false)&&mode!="values"&&mode!="formats")throw std::runtime_error("Unmerge the copied source cells before ordinary or formula paste");if(mode!="formats"&&mergedOverlap(workbookView(),sheet_.toStdString(),row+1,col+1,row+height,col+width))throw std::runtime_error("Unmerge the destination cells before pasting a whole row or column");
  if((mode=="all"||mode=="formats")&&clip.value("defaultStyle",Json::object())!=defaultStyle_)throw std::runtime_error("Sparse formatting paste between differing worksheet default styles is not supported");Json ops=Json::array();if(!skipBlanks)for(const auto& sheet:workbookView().at("sheets"))if(sheet.at("id")==sheet_.toStdString())for(const auto& cell:sheet.at("cells")){const int r=cell.at("row").get<int>()-1,c=cell.at("col").get<int>()-1;if(r<row||r>=row+height||c<col||c>=col+width)continue;Json op={{"type",mode=="formats"?"set_style":"set_cell"},{"sheet",sheet_.toStdString()},{"row",r+1},{"col",c+1}};if(mode!="formats")op["input"]="";if(mode=="all"||mode=="formats"){op["style"]=clip.value("defaultStyle",Json::object());op["replaceStyle"]=true;}ops.push_back(std::move(op));}
  for(int dy=0;dy<height;dy+=sourceRows)for(int dx=0;dx<width;dx+=sourceColumns)for(const auto& entry:clip.at("entries")){const int r=entry.at("row"),c=entry.at("col"),tr=row+dy+r,tc=col+dx+c;auto input=entry.at("input").get<std::string>();if(skipBlanks&&input.empty())continue;if(mode=="values"&&!entry.contains("value")&&input.starts_with('='))throw std::runtime_error("Calculate the source cells before using Paste Values");if(mode=="values"&&entry.contains("value")){const auto& v=entry.at("value");input=v.is_null()?std::string():v.is_string()?(entry.value("valueType",std::string())=="error"?v.get<std::string>():"'"+v.get<std::string>()):v.is_boolean()?(v.get<bool>()?"TRUE":"FALSE"):v.dump();}else if(input.starts_with('='))input=shiftFormula(input,tr-clip.at("sourceRow").get<int>()-r,tc-clip.at("sourceCol").get<int>()-c);Json op={{"type",mode=="formats"?"set_style":"set_cell"},{"sheet",sheet_.toStdString()},{"row",tr+1},{"col",tc+1}};if(mode!="formats")op["input"]=input;if(mode=="all"||mode=="formats"){op["style"]=entry.value("style",Json::object());op["replaceStyle"]=true;}ops.push_back(std::move(op));}return apply(ops);
 }catch(const std::exception& error){error_=QString::fromUtf8(error.what());return false;}}
 const int sourceRows=static_cast<int>(clip.at("cells").size());int sourceColumns=0;for(const auto& line:clip.at("cells")){if(!line.is_array())return false;sourceColumns=std::max(sourceColumns,static_cast<int>(line.size()));}
 const int requestedRows=transpose?targetColumns:targetRows,requestedColumns=transpose?targetRows:targetColumns;
 if(sourceRows>0&&sourceColumns>0&&(requestedRows>sourceRows||requestedColumns>sourceColumns)){
  const int rows=std::max(sourceRows,requestedRows),columns=std::max(sourceColumns,requestedColumns);
  if(requestedRows<sourceRows||requestedColumns<sourceColumns||rows%sourceRows||columns%sourceColumns||static_cast<std::int64_t>(rows)*columns>1000000){error_="The destination must contain whole copies of the source range (at most 1,000,000 cells).";return false;}
  for(const auto& line:clip.at("cells"))if(line.size()!=static_cast<std::size_t>(sourceColumns)){error_="Select a rectangular clipboard range before repeating Paste.";return false;}
  Json expanded=clip;expanded["cells"]=Json::array();for(int r=0;r<rows;++r){Json line=Json::array();for(int c=0;c<columns;++c){auto cell=clip.at("cells")[r%sourceRows][c%sourceColumns];const auto input=cell.value("input",std::string());if(input.starts_with('='))cell["input"]=shiftFormula(input,r-r%sourceRows,c-c%sourceColumns);line.push_back(std::move(cell));}expanded["cells"].push_back(std::move(line));}
  return pasteRange(row,col,expanded,requestedMode,skipBlanks);
 }
 if(clip.value("sourceHasMerges",false)&&mode!="values"&&mode!="formats"){error_="Unmerge the copied source cells before ordinary, formula or transpose paste. Paste Values or Formats can be used instead.";return false;}
 std::size_t targetCells=0,sourceWidth=0;for(const auto& line:clip.at("cells")){if(!line.is_array())return false;targetCells+=line.size();sourceWidth=std::max(sourceWidth,line.size());}
 if(targetCells&&mode!="formats"){
  const auto height=transpose?sourceWidth:clip.at("cells").size(),width=transpose?clip.at("cells").size():sourceWidth;
  if(row<0||row>=MaxRows||col<0||col>=MaxColumns||height>static_cast<std::size_t>(MaxRows-row)||width>static_cast<std::size_t>(MaxColumns-col))return false;
  if(const auto* merge=mergedOverlap(workbookView(),sheet_.toStdString(),row+1,col+1,row+static_cast<int>(height),col+static_cast<int>(width));merge&&(targetCells!=1||height!=1||width!=1||merge->at("row1")!=row+1||merge->at("col1")!=col+1)){error_="Unmerge the destination cells before pasting multiple values or writing inside a merged area. A single cell can be pasted into the upper-left merged cell.";return false;}
 }
 Json ops=Json::array();
 for(size_t r=0;r<clip["cells"].size();++r){const auto& source=clip["cells"][r];if(!source.is_array())return false;
  for(size_t c=0;c<source.size();++c){const auto& cell=source[c];auto input=cell.value("input",std::string());if(skipBlanks&&input.empty())continue;
   const int tr=row+static_cast<int>(transpose?c:r),tc=col+static_cast<int>(transpose?r:c);
   if(tr<0||tr>=MaxRows||tc<0||tc>=MaxColumns)return false;
   if(mode=="formats"){ops.push_back({{"type","set_style"},{"sheet",sheet_.toStdString()},{"row",tr+1},{"col",tc+1},{"style",cell.value("style",Json::object())},{"replace",true}});continue;}
   if(mode=="values"&&!cell.contains("value")&&!input.empty()&&input[0]=='='){error_="Calculate the source cells before using Paste Values.";return false;}
   if(mode=="values"&&cell.contains("value")){const auto& v=cell["value"];input=v.is_null()?std::string():v.is_string()?(cell.value("valueType",std::string())=="error"?v.get<std::string>():"'"+v.get<std::string>()):v.is_boolean()?(v.get<bool>()?"TRUE":"FALSE"):v.dump();}
   else if(!input.empty()&&input[0]=='=')input=shiftFormula(input,tr-clip.value("sourceRow",row)-static_cast<int>(r),tc-clip.value("sourceCol",col)-static_cast<int>(c));
   Json op={{"type","set_cell"},{"sheet",sheet_.toStdString()},{"row",tr+1},{"col",tc+1},{"input",input}};
   if(mode=="all"){op["style"]=cell.value("style",Json::object());op["replaceStyle"]=true;}
   ops.push_back(op);
  }
 }
 return apply(ops);
}
bool WorksheetModel::moveRange(int row,int col,const Json& clip){
 try{
  const auto book=session_.snapshot();if(clip.value("sourceOwner",std::string())!=clipboardOwner_.toStdString()||clip.value("sourceRevision",std::uint64_t{})!=book.at("revision").get<std::uint64_t>())throw std::runtime_error("The cut source changed or belongs to another workbook. Cut the cells again.");
  const auto sourceId=clip.at("sourceSheet").get<std::string>(),targetId=sheet_.toStdString();std::string sourceName,targetName;
  for(const auto& sheet:book.at("sheets")){if(sheet.at("id")==sourceId)sourceName=sheet.at("name").get<std::string>();if(sheet.at("id")==targetId)targetName=sheet.at("name").get<std::string>();}
  if(sourceName.empty()||targetName.empty())throw std::runtime_error("The cut source or destination worksheet is unavailable.");
  const bool sparse=clip.value("sparse",false);const auto& cells=clip.at("cells");if(!sparse&&(cells.empty()||!cells[0].is_array()||cells[0].empty()))throw std::runtime_error("The cut range is empty.");const auto dimensions=sparse?sparseDimensions(clip):std::pair{static_cast<int>(cells.size()),static_cast<int>(cells[0].size())};
  const int height=dimensions.first,width=dimensions.second,sr=clip.at("sourceRow"),sc=clip.at("sourceCol");if(sparse&&clip.value("defaultStyle",Json::object())!=defaultStyle_)throw std::runtime_error("Sparse column moves between differing worksheet default styles are not supported");
  if(row<0||col<0||row+height>MaxRows||col+width>MaxColumns||sr<0||sc<0||sr+height>MaxRows||sc+width>MaxColumns)throw std::runtime_error("The moved range exceeds worksheet bounds.");
  if(mergedOverlap(book,sourceId,sr+1,sc+1,sr+height,sc+width))throw std::runtime_error("Unmerge the source cells before moving them; merged-cell moves are not supported.");
  if(const auto* merge=mergedOverlap(book,targetId,row+1,col+1,row+height,col+width);merge&&(height!=1||width!=1||merge->at("row1")!=row+1||merge->at("col1")!=col+1))throw std::runtime_error("Unmerge the destination cells before moving multiple values or writing inside a merged area. A single cell can be moved into the upper-left merged cell.");
  auto inside=[](int r,int c,int top,int left,int h,int w){return r>=top&&r<top+h&&c>=left&&c<left+w;};
  auto protectedRange=[&](const Json& range,const std::string& defaultSheet){if(!range.is_object()||!range.contains("row1")||!range.contains("row2")||!range.contains("col1")||!range.contains("col2"))return false;const auto id=range.value("sheet",defaultSheet);const int r1=range.at("row1"),r2=range.at("row2"),c1=range.at("col1"),c2=range.at("col2");return (id==sourceId&&r1<=sr+height&&r2>=sr+1&&c1<=sc+width&&c2>=sc+1)||(id==targetId&&r1<=row+height&&r2>=row+1&&c1<=col+width&&c2>=col+1);};
  for(const auto& chart:book.value("charts",Json::array()))if(protectedRange(chart.value("range",Json::object()),chart.value("sheet",std::string())))throw std::runtime_error("The move intersects a chart data range. Adjust the chart source before moving these cells.");
  for(const auto& pivot:book.value("pivots",Json::array()))if(pivot.value("version",0)==1&&(protectedRange(pivot.value("source",Json::object()),std::string())||protectedRange(pivot.value("output",Json::object()),std::string())))throw std::runtime_error("The move intersects a PivotTable source or report. Adjust the report or source before moving these cells.");
  auto rewrite=[&](const std::string& formula,const std::string& sheet,bool moved){return moveFormulaReferences(formula,sheet,sourceName,targetName,sr+1,sc+1,sr+height,sc+width,row-sr,col-sc,moved);};
  Json ops=Json::array();
  if(sparse){for(const auto& sheet:book.at("sheets")){const auto id=sheet.at("id").get<std::string>();for(const auto& cell:sheet.at("cells")){const int r=cell.at("row").get<int>()-1,c=cell.at("col").get<int>()-1;if((id==sourceId&&inside(r,c,sr,sc,height,width))||(id==targetId&&inside(r,c,row,col,height,width)))ops.push_back({{"type","set_cell"},{"sheet",id},{"row",r+1},{"col",c+1},{"input",""},{"style",Json::object()},{"replaceStyle",true}});}}for(const auto& cell:clip.at("entries")){auto input=cell.at("input").get<std::string>();if(input.starts_with('='))input=rewrite(input,sourceName,true);ops.push_back({{"type","set_cell"},{"sheet",targetId},{"row",row+cell.at("row").get<int>()+1},{"col",col+cell.at("col").get<int>()+1},{"input",input},{"style",cell.value("style",Json::object())},{"replaceStyle",true}});}}
  else{for(int r=0;r<height;++r){if(!cells[r].is_array()||static_cast<int>(cells[r].size())!=width)throw std::runtime_error("The cut clipboard is not rectangular.");for(int c=0;c<width;++c)ops.push_back({{"type","set_cell"},{"sheet",sourceId},{"row",sr+r+1},{"col",sc+c+1},{"input",""},{"style",Json::object()},{"replaceStyle",true}});}
   for(int r=0;r<height;++r)for(int c=0;c<width;++c){const auto& cell=cells[r][c];auto input=cell.value("input",std::string());if(!input.empty()&&input[0]=='=')input=rewrite(input,sourceName,true);ops.push_back({{"type","set_cell"},{"sheet",targetId},{"row",row+r+1},{"col",col+c+1},{"input",input},{"style",cell.value("style",Json::object())},{"replaceStyle",true}});}}
  for(const auto& sheet:book.at("sheets")){const auto id=sheet.at("id").get<std::string>(),name=sheet.at("name").get<std::string>();for(const auto& cell:sheet.at("cells")){const int r=cell.at("row").get<int>()-1,c=cell.at("col").get<int>()-1;if((id==sourceId&&inside(r,c,sr,sc,height,width))||(id==targetId&&inside(r,c,row,col,height,width)))continue;const auto input=cell.value("input",std::string());if(!input.empty()&&input[0]=='='){auto revised=rewrite(input,name,false);if(revised!=input)ops.push_back({{"type","set_cell"},{"sheet",id},{"row",r+1},{"col",c+1},{"input",revised}});}}}
  auto names=book.value("names",Json::object());const auto originalNames=names;for(auto& name:names)if(name.is_string())name=rewrite(name.get<std::string>(),"",false);if(names!=originalNames)ops.push_back({{"type","set_metadata"},{"key","names"},{"value",names}});
  return apply(ops);
 }catch(const std::exception& error){error_=QString::fromUtf8(error.what());return false;}
}
QString WorksheetModel::lastError()const{return error_;}
const Json& WorksheetModel::workbookView()const noexcept{return previewView_?*previewView_:session_.snapshotView();}
Json WorksheetModel::calculatedSnapshot()const{Json result={{"revision",displayRevision_},{"status",valuesCurrent_?"ok":"stale"},{"values",Json::array()}};if(valuesCurrent_)for(const auto& [key,value]:values_)result["values"].push_back({{"sheet",sheet_.toStdString()},{"row",key.first+1},{"col",key.second+1},{"type",valueTypes_.at(key)},{"value",value}});return result;}
bool WorksheetModel::sortRange(int r1,int c1,int r2,int c2,bool ascending){
 error_.clear();const auto clip=copyRange(r1,c1,r2,c2);if(clip.is_null()||clip.value("sparse",false)||r1>=r2){error_="Select a bounded rectangular range with at least two data rows.";return false;}
 if(clip.value("sourceHasMerges",false)){error_="Unmerge the selected cells before sorting; merged ranges cannot be sorted safely.";return false;}
 struct Row {int source;Json cells,key;int kind;};std::vector<Row> rows;
 for(int r=r1;r<=r2;++r){auto cells=clip["cells"][r-r1];const auto& cell=cells[0];const auto input=cell["input"].get<std::string>();Json key;
  if(cell.contains("value"))key=cell["value"];
  else if(!input.empty()&&input[0]=='='){error_="Calculate the sort column before sorting.";return false;}
  else if(input.empty())key=nullptr;
  else if(input[0]=='\'')key=input.substr(1);
  else{bool ok=false;const double number=QString::fromStdString(input).toDouble(&ok);if(ok)key=number;else if(QString::fromStdString(input).compare("TRUE",Qt::CaseInsensitive)==0)key=true;else if(QString::fromStdString(input).compare("FALSE",Qt::CaseInsensitive)==0)key=false;else key=input;}
  const int kind=key.is_null()||(key.is_string()&&key=="")?4:cell.value("valueType",std::string())=="error"?3:key.is_number()?0:key.is_string()?1:2;rows.push_back({r,std::move(cells),std::move(key),kind});
 }
 std::stable_sort(rows.begin(),rows.end(),[ascending](const Row& a,const Row& b){if(a.kind==4||b.kind==4)return a.kind<b.kind;int order=0;if(a.kind!=b.kind)order=a.kind<b.kind?-1:1;else if(a.key.is_string())order=QString::compare(QString::fromStdString(a.key.get<std::string>()),QString::fromStdString(b.key.get<std::string>()),Qt::CaseInsensitive);else order=a.key<b.key?-1:a.key>b.key?1:0;return ascending?order<0:order>0;});
 Json ops=Json::array();for(std::size_t r=0;r<rows.size();++r)for(int c=c1;c<=c2;++c){const auto& source=rows[r];const auto& cell=source.cells[c-c1];auto input=cell["input"].get<std::string>();if(!input.empty()&&input[0]=='=')input=shiftFormula(input,r1+static_cast<int>(r)-source.source,0);ops.push_back({{"type","set_cell"},{"sheet",sheet_.toStdString()},{"row",r1+static_cast<int>(r)+1},{"col",c+1},{"input",input},{"style",cell["style"]},{"replaceStyle",true}});}
 return apply(ops);
}
bool WorksheetModel::apply(const Json& ops){
 error_.clear();if(readOnly_){error_="Accept or discard the preview before editing cells.";return false;}const auto revision=session_.snapshotView().at("revision").get<std::uint64_t>();std::string error;
 if(!session_.apply(revision,ops,&error)){error_=QString::fromStdString(error);return false;}const auto& book=session_.snapshotView();if(book.at("revision")==revision)return true;
 refreshAfterChange();if(onChanged)onChanged();return true;
}
void WorksheetModel::refreshAfterChange(){
 if(previewView_||readOnly_){refresh();return;}
 const auto& book=session_.snapshotView();const auto revision=book.at("revision").get<std::uint64_t>();const auto& change=session_.lastChange();const bool preserveCalculated=valuesCurrent_&&revision>0&&displayRevision_==revision-1&&!change.value("structure",true)&&!change.value("metadata",false)&&!change.value("calculationInputsChanged",true);
 if(!preserveCalculated){const auto previousChecks=property("formula.errorChecks").toMap();setProperty("formula.errorChecks",QVariantMap());for(auto it=previousChecks.begin();it!=previousChecks.end();++it){const auto coordinate=it.key().split(',');if(coordinate.size()==2){const auto cell=index(coordinate[0].toInt(),coordinate[1].toInt());emit dataChanged(cell,cell,{FormulaErrorCheckRole});}}}
 if(change.value("structure",true))loadSnapshot(book);else{displayRevision_=book.at("revision");valuesCurrent_=preserveCalculated;const auto settings=book.value("settings",Json::object());date1904_=settings.is_object()&&settings.contains("date1904")&&settings["date1904"].is_boolean()&&settings["date1904"].get<bool>();
  const auto layouts=change.value("layoutSheets",Json::array());const bool layoutChanged=std::find(layouts.begin(),layouts.end(),sheet_.toStdString())!=layouts.end();
  if(layoutChanged)for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==sheet_.toStdString()){defaultStyle_=sheet.value("defaultStyle",Json::object());break;}
  for(const auto& range:change.at("sheets"))if(range.at("sheet")==sheet_.toStdString()){const int r1=range.at("row1").get<int>()-1,r2=range.at("row2").get<int>()-1,c1=range.at("col1").get<int>()-1,c2=range.at("col2").get<int>()-1;if(!preserveCalculated){auto value=values_.lower_bound({r1,0});while(value!=values_.end()&&value->first.first<=r2){if(value->first.second>=c1&&value->first.second<=c2){valueTypes_.erase(value->first);value=values_.erase(value);}else ++value;}}if(!layoutChanged)emit dataChanged(index(r1,c1),index(r2,c2));}
  if(layoutChanged)emit dataChanged(index(0,0),index(rowCount()-1,columnCount()-1));
 }
}
std::optional<std::string> WorksheetModel::inferredFormulaNumberFormat(const QModelIndex& target,const QString& input)const{
 if(!target.isValid()||target.model()!=this||!input.startsWith('='))return std::nullopt;
 const auto style=cellStyle(target.row(),target.column());std::optional<std::string> inferred;
 if(!style.contains("numberFormat")){const auto date=inferredDateFormulaFormat(input);if(!date.isEmpty())inferred=date.toStdString();}
 const auto targetFormat=style.value("numberFormat",std::string("General"));if(!targetFormat.empty()&&targetFormat!="General")return inferred;
 if(raw(target.row(),target.column()).isEmpty()&&target.column()>=3&&!inferred){
  const auto candidate=cellStyle(target.row(),target.column()-1).value("numberFormat",std::string("General"));
  if(!candidate.empty()&&candidate!="General"&&candidate!="@"){
   bool consistent=true;for(int offset=1;offset<=3;++offset){const auto neighbor=raw(target.row(),target.column()-offset);if((!neighbor.startsWith('=')&&!numericLiteral(neighbor.toStdString()))||cellStyle(target.row(),target.column()-offset).value("numberFormat",std::string("General"))!=candidate){consistent=false;break;}}
   if(consistent)return candidate;
  }
 }
 static const QRegularExpression functionCall("[A-Za-z_][A-Za-z0-9_.]*\\s*\\(");
 static const QRegularExpression averageCall("^\\s*=\\s*AVERAGE\\s*\\(",QRegularExpression::CaseInsensitiveOption);
 if(!functionCall.match(input).hasMatch()||averageCall.match(input).hasMatch()){
  const auto references=formulaReferences(input.toStdString());if(!references.empty()&&!references.front().external){const auto& reference=references.front();bool local=reference.sheet.empty();
   if(!local)for(const auto& sheet:workbookView().at("sheets"))if(sheet.at("id")==sheet_.toStdString()&&sheet.at("name")==reference.sheet)local=true;
   if(local){const auto source=cellStyle(reference.row1-1,reference.col1-1);const auto format=source.value("numberFormat",std::string("General"));if(!format.empty()&&format!="General")return format;}
  }
 }
 return inferred;
}
bool WorksheetModel::setData(const QModelIndex&i,const QVariant&v,int role){
 const bool replacement=role==ReplacementEditRole;
 if(!i.isValid()||(role!=Qt::EditRole&&!replacement))return false;
 const auto text=v.toString();const auto style=cellStyle(i.row(),i.column());Json operation={{"type","set_cell"},{"sheet",sheet_.toStdString()},{"row",i.row()+1},{"col",i.column()+1},{"input",text.toStdString()}};
 const auto previous=raw(i.row(),i.column()),edit=data(i,Qt::EditRole).toString();if(!replacement&&edit!=previous&&text==edit){operation["input"]=previous.toStdString();return apply(Json::array({std::move(operation)}));}
 if(text.startsWith('\'')&&(replacement||!previous.startsWith('\''))&&style.contains("quotePrefix")&&!style.at("quotePrefix").get<bool>())operation["style"]={{"quotePrefix",true}};
 if(replacement&&!text.startsWith('\'')&&style.value("quotePrefix",false))operation["style"]={{"quotePrefix",false}};
 if(!replacement&&previous.startsWith('\'')&&style.contains("quotePrefix")&&!style.at("quotePrefix").get<bool>()){
  if(text.startsWith('\'')&&!edit.startsWith('\''))operation["style"]={{"quotePrefix",true}};
  else{auto literal=text.toStdString(),upper=literal;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});if(!literal.empty()&&(upper=="TRUE"||upper=="FALSE"||std::isdigit(static_cast<unsigned char>(literal.front()))||literal.find_first_of("'=+-.@$\xC2\xE2\xC2")==0))literal="'"+literal;operation["input"]=std::move(literal);}
  return apply(Json::array({std::move(operation)}));
 }
 if(style.value("numberFormat",std::string())=="@"){
  if(text!=previous&&!text.isEmpty()&&!text.startsWith('\''))operation["input"]="'"+text.toStdString();
  return apply(Json::array({std::move(operation)}));
 }
 if(const auto format=inferredFormulaNumberFormat(i,text))operation["style"]["numberFormat"]=*format;
 const auto trimmed=text.trimmed();if(trimmed.endsWith('%')){
  bool okay=false;const double number=trimmed.chopped(1).trimmed().toDouble(&okay)/100.0;
  if(okay&&std::isfinite(number)){
   char buffer[128];const auto converted=std::to_chars(buffer,buffer+sizeof(buffer),number);if(converted.ec!=std::errc())return false;operation["input"]=std::string(buffer,converted.ptr);
   const auto format=style.value("numberFormat",std::string("General"));
   if(!percentageFormat(format)){const int decimal=static_cast<int>(trimmed.indexOf('.'));const auto digits=decimal<0?0:std::min(12,static_cast<int>(trimmed.size())-decimal-2);operation["style"]["numberFormat"]=digits>0?"0."+std::string(digits,'0')+"%":"0%";}
  }
 }else if(percentageFormat(style.value("numberFormat",std::string()))){
  bool okay=false;const double number=trimmed.toDouble(&okay);if(okay&&std::isfinite(number)&&std::abs(number)>=1.0){char buffer[128];const auto converted=std::to_chars(buffer,buffer+sizeof(buffer),number/100.0);if(converted.ec!=std::errc())return false;operation["input"]=std::string(buffer,converted.ptr);}
 }
 if(const auto parsed=dateLiteral(trimmed)){
  const auto serial=date1904_?QDate(1904,1,1).daysTo(parsed->date):QDate(1899,12,31).daysTo(parsed->date)+(parsed->date>=QDate(1900,3,1)?1:0);
  operation["input"]=QString::number(serial).toStdString();if(!style.contains("numberFormat")||style.value("numberFormat",std::string())=="General")operation["style"]["numberFormat"]=parsed->format.toStdString();
 }
 return apply(Json::array({std::move(operation)}));
}
struct SelectionStatistics::Job {
 QPointer<WorksheetModel> model;
 const Json::array_t* cells=nullptr;
 std::size_t cursor=0;
 std::map<std::pair<int,int>,Json>::const_iterator value;
 std::uint64_t revision=0;
 QString sheet;
 int top,left,bottom,right;
 SelectionTotals totals;
 std::function<bool()> current;
 std::function<void(SelectionTotals)> complete;
 QMetaObject::Connection changed,destroyed;
};
namespace {
void addSelectionNumber(SelectionTotals& totals,const WorksheetModel& model,int row,int col,double value){const auto format=model.cellStyle(row,col).value("numberFormat",std::string("General"));if(totals.count==0)totals.numberFormat=format;else if(totals.numberFormat!=format)totals.numberFormat="General";totals.sum+=value;++totals.count;}
}
SelectionStatistics::SelectionStatistics(QObject* parent):QObject(parent){}
SelectionStatistics::~SelectionStatistics(){cancel();}
void SelectionStatistics::cancel(){if(job_){QObject::disconnect(job_->changed);QObject::disconnect(job_->destroyed);job_.reset();}}
void SelectionStatistics::start(WorksheetModel& model,int top,int left,int bottom,int right,std::function<bool()> current,std::function<void(SelectionTotals)> complete){
 cancel();if(top<0||left<0||bottom<top||right<left||bottom>=MaxRows||right>=MaxColumns)throw std::invalid_argument("Invalid selection statistics range");if(current&&!current())return;
 if(static_cast<std::uint64_t>(bottom-top+1)*(right-left+1)<=4096){SelectionTotals totals;for(int row=top;row<=bottom;++row)for(int col=left;col<=right;++col){const auto value=model.values_.find({row,col});if(!model.raw(row,col).isEmpty()||(model.valuesCurrent_&&value!=model.values_.end()&&!value->second.is_null()))++totals.populated;if(const auto number=model.numericValue(row,col))addSelectionNumber(totals,model,row,col,*number);}if((!current||current())&&complete)complete(totals);return;}
 auto job=std::make_shared<Job>();job->model=&model;job->revision=model.workbookView().at("revision");job->sheet=model.sheet_;job->top=top;job->left=left;job->bottom=bottom;job->right=right;job->current=std::move(current);job->complete=std::move(complete);job->value=model.values_.cbegin();for(const auto& sheet:model.workbookView().at("sheets"))if(sheet.at("id")==job->sheet.toStdString()){job->cells=&sheet.at("cells").get_ref<const Json::array_t&>();break;}
 job_=job;job->changed=connect(&model,&QAbstractItemModel::dataChanged,this,[this]{cancel();});job->destroyed=connect(&model,&QObject::destroyed,this,[this]{cancel();});QTimer::singleShot(0,this,[this,job]{advance(job);});
}
void SelectionStatistics::advance(const std::shared_ptr<Job>& job){
 if(job_!=job)return;auto* model=job->model.data();if(!model||model->sheet_!=job->sheet||model->workbookView().at("revision")!=job->revision||(job->current&&!job->current())){cancel();return;}
 QElapsedTimer elapsed;elapsed.start();std::size_t visited=0;const auto valueEnd=model->values_.cend();
 while((job->cells&&job->cursor<job->cells->size())||job->value!=valueEnd){
  const auto source=job->cells&&job->cursor<job->cells->size()?&job->cells->at(job->cursor):nullptr;const auto sourcePoint=source?std::pair{source->at("row").get<int>()-1,source->at("col").get<int>()-1}:std::pair{MaxRows,MaxColumns};const auto valuePoint=job->value!=valueEnd?job->value->first:std::pair{MaxRows,MaxColumns};const auto point=std::min(sourcePoint,valuePoint);
  if(point.first>=job->top&&point.first<=job->bottom&&point.second>=job->left&&point.second<=job->right){if((point==sourcePoint&&!source->at("input").get_ref<const std::string&>().empty())||(model->valuesCurrent_&&point==valuePoint&&!job->value->second.is_null()))++job->totals.populated;std::optional<double> number;if(model->valuesCurrent_&&point==valuePoint){const auto type=model->valueTypes_.find(point);if(job->value->second.is_number()&&(type==model->valueTypes_.end()||type->second!="error")){const auto value=job->value->second.get<double>();if(std::isfinite(value))number=value;}}else if(point==sourcePoint)number=numericLiteral(source->at("input").get_ref<const std::string&>());if(number)addSelectionNumber(job->totals,*model,point.first,point.second,*number);}
  if(point==sourcePoint)++job->cursor;if(point==valuePoint)++job->value;
  if(++visited>=512||elapsed.elapsed()>=4){QTimer::singleShot(0,this,[this,job]{advance(job);});return;}
 }
 auto complete=std::move(job->complete);const auto totals=job->totals;cancel();if(complete)complete(totals);
}
}
