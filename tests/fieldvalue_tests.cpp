#include "tigersheets/calculation.hpp"
#include "tigersheets/formula_editor.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/workbook_links.hpp"
#include "tigersheets/biff.hpp"
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <pugixml.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
Json valueAt(const Json& result,int row,int column=1){
 for(const auto& value:result.at("values"))if(value.at("row")==row&&value.at("col")==column)return value;
 throw std::runtime_error("Expected FIELDVALUE calculated cell is missing");
}
void nativeCompletion(){
 CalculationEngine engine;const auto names=engine.functions().at("functions").get<std::vector<std::string>>();
 require(std::find(names.begin(),names.end(),"FIELDVALUE")!=names.end(),"The real calculation engine advertises FIELDVALUE only after it is implemented");
 QFile file(QStringLiteral(TIGERSHEETS_SOURCE_DIR "/compatibility/functions.json"));require(file.open(QIODevice::ReadOnly),"Open the real frozen function catalog");
 FormulaLineEdit editor;editor.setFunctionCatalog(Json::parse(file.readAll().toStdString()),engine.functions());editor.resize(620,30);editor.show();editor.activateWindow();editor.setFocus();QApplication::processEvents();
 QSignalSpy committed(&editor,&QLineEdit::returnPressed);QTest::keyClicks(&editor,"=F");auto* popup=editor.completionPopup();
 require(popup->isVisible(),"Typing a real F prefix opens native function suggestions");
 int row=-1;for(int i=0;i<popup->count();++i)if(popup->item(i)->data(Qt::UserRole)=="FIELDVALUE")row=i;
 require(row>=0,"Source-observed FIELDVALUE is a real available native completion row");
 require(!popup->item(row)->icon().isNull(),"FIELDVALUE has the real graphical function icon");
 const auto capture=qEnvironmentVariable("TIGERSHEETS_FIELDVALUE_CAPTURE");
 if(!capture.isEmpty()){
  require(QDir().mkpath(capture),"Create isolated native function capture directory");
  require(popup->grab().save(capture+"/f-function-list.png"),"Retain actual F completion list containing FIELDVALUE");
 }
 for(int attempt=0;popup->currentRow()!=row&&attempt<popup->count();++attempt)QTest::keyClick(&editor,Qt::Key_Down);
 require(popup->currentRow()==row,"Native arrow keys navigate to FIELDVALUE without editing text");
 QTest::keyClick(&editor,Qt::Key_Tab);
 require(editor.text()=="=FIELDVALUE("&&committed.empty(),"Native completion inserts the recognized function without committing a cell");
 require(editor.argumentHelpText().contains("value")&&editor.argumentHelpText().contains("field_name"),"FIELDVALUE help has its actual two-argument signature");
 require(editor.argumentHelpText().contains("#FIELD!")&&editor.argumentHelpText().contains("not supported"),"The bounded linked-data limitation is explicit in native help");
 if(!capture.isEmpty()){
  require(editor.grab().save(capture+"/fieldvalue-inserted-editor.png"),"Retain actual inserted FIELDVALUE editor");
  if(auto* help=editor.findChild<QLabel*>("formula.argumentHelp"))require(help->grab().save(capture+"/fieldvalue-argument-help.png"),"Retain actual native FIELDVALUE signature");
 }
 QTest::keyClicks(&editor,"42,\"Name\")");
 const Json snapshot={{"revision",1},{"sheets",Json::array({{{"id","s"},{"name","Sheet1"},{"cells",Json::array({{{"row",1},{"col",1},{"input",editor.text().toStdString()}}})}}})}};
 const auto result=valueAt(engine.calculate(snapshot),1);require(result.at("type")=="error"&&result.at("value")=="#FIELD!","A completed native suggestion executes the actual typed FIELDVALUE function");
}
void nativeXlsxErrors(){
 WorkbookSession session;const auto sheet=session.snapshotView().at("sheets").at(0).at("id");
 require(session.apply(0,Json::array({
  {{"type","set_cell"},{"sheet",sheet},{"row",1},{"col",1},{"input","#FIELD!"}},
  {{"type","set_cell"},{"sheet",sheet},{"row",2},{"col",1},{"input","'#FIELD!"}},
  {{"type","set_cell"},{"sheet",sheet},{"row",3},{"col",1},{"input","=_xlfn.FIELDVALUE(42,\"Name\")"}},
  {{"type","set_cell"},{"sheet",sheet},{"row",4},{"col",1},{"input","=IFERROR(A3,\"none\")"}},
  {{"type","set_cell"},{"sheet",sheet},{"row",5},{"col",1},{"input","=ERROR.TYPE(A1)"}}
 })),"Prepare native error, deliberately quoted text, and a real FIELDVALUE formula");
 const auto before=session.snapshot();const auto calculated=CalculationEngine().calculate(before);
 QTemporaryDir files;require(files.isValid(),"Create isolated native XLSX fixture directory");const auto path=files.filePath("fieldvalue.xlsx").toStdString();writeWorkbookCalculated(path,before,calculated);
 const auto package=readWorkbook(path);pugi::xml_document xml;require(xml.load_string(package.entries.at("xl/worksheets/sheet1.xml").c_str()),"Read actual native SpreadsheetML error records");
 auto native=[&](const char* address){for(auto row:xml.child("worksheet").child("sheetData").children("row"))for(auto cell:row.children("c"))if(std::string(cell.attribute("r").value())==address)return cell;return pugi::xml_node();};
 require(std::string(native("A1").attribute("t").value())=="e"&&std::string(native("A1").child("v").text().get())=="#FIELD!","An authored FIELD error is a real native error cell, never inline text");
 require(std::string(native("A2").attribute("t").value())=="inlineStr","An explicitly quoted FIELD-looking string remains native text");
 require(std::string(native("A3").attribute("t").value())=="e"&&std::string(native("A3").child("v").text().get())=="#FIELD!","A native FIELDVALUE formula retains a typed error cache");
 const auto again=CalculationEngine().calculate(package.workbook);
 require(valueAt(again,1).at("type")=="error"&&valueAt(again,1).at("value")=="#FIELD!","Reopened literal FIELD error retains error identity");
 require(valueAt(again,2).at("type")=="text"&&valueAt(again,2).at("value")=="#FIELD!","Reopened deliberately quoted error-looking text retains text identity");
 require(valueAt(again,3).at("value")=="#FIELD!"&&valueAt(again,4).at("value")=="none"&&valueAt(again,5).at("value")==13,"Reopened formula, IFERROR and ERROR.TYPE preserve real error semantics");
 require(session.snapshot()==before,"Native error persistence does not mutate the authored session or history");
}
void nativeExternalError(){
 const Json source={{"revision",0},{"sheets",Json::array({{{"id","source"},{"name","Data"},{"cells",Json::array({{{"row",1},{"col",1},{"input","=FIELDVALUE(42,\"Name\")"}}})}}})}};
 const Json model={{"revision",0},{"sheets",Json::array({{{"id","model"},{"name","Model"},{"cells",Json::array({{{"row",1},{"col",1},{"input","='[Missing.xlsx]Data'!A1"}},{{"row",2},{"col",1},{"input","=IFERROR(A1,\"missing field\")"}}})}}})},{"links",Json::array({{{"name","Missing.xlsx"},{"path","/nonexistent-fieldvalue-source/Missing.xlsx"},{"enabled",true},{"cachedWorkbook",source}}})}};
 QTemporaryDir files;require(files.isValid(),"Create isolated native linked error fixture directory");
 WorkbookPackage package;writeWorkbook(files.filePath("linked-fieldvalue.xlsx").toStdString(),model,&package);
 pugi::xml_document external;require(external.load_string(package.entries.at("xl/externalLinks/tigersheetsLink1.xml").c_str()),"Read actual native external error cache");
 const auto error=external.select_node("//cell[@r='A1']").node();
 require(std::string(error.attribute("t").value())=="e"&&std::string(error.child("v").text().get())=="#FIELD!","Native source cache contains the calculated typed FIELD error");
 auto nativeOnly=model;nativeOnly.erase("links");
 const auto prepared=prepareWorkbookLinks(nativeOnly,&package);
 const auto calculated=CalculationEngine().calculate(prepared.snapshot);
 require(prepared.filesRead==0,"Native cached FIELD errors require no access to the absent linked workbook");
 require(valueAt(calculated,1).at("type")=="error"&&valueAt(calculated,1).at("value")=="#FIELD!","Native-only linked caches preserve FIELD error identity instead of rejecting the source");
 require(valueAt(calculated,2).at("value")=="missing field","IFERROR handles a FIELD error loaded from native external cache");
 require(nativeOnly.at("sheets")==model.at("sheets"),"Native external cache resolution leaves authored formula cells unchanged");
}
void nativeBiffText(){
 auto book=WorkbookSession().snapshot();auto& cells=book["sheets"][0]["cells"];
 const std::vector<std::string> literals={"#NULL!","#DIV/0!","#VALUE!","#REF!","#NAME?","#NUM!","#N/A","#SPILL!","#CALC!","#FIELD!","#field!","#FiElD!","#spill!","#CaLc!","#n/a"};
 for(const auto& value:literals)for(const bool prefix:{false,true})cells.push_back({{"row",cells.size()+1},{"col",1},{"input","'"+value},{"style",{{"quotePrefix",prefix}}}});
 cells.push_back({{"row",cells.size()+1},{"col",1},{"input","''#FIELD!"},{"style",{{"quotePrefix",false}}}});
 QTemporaryDir files;require(files.isValid(),"Create native BIFF ordinary-text fixture directory");const auto path=files.filePath("error-looking-text.xls").toStdString();
 writeBiffWorkbook(path,book);const auto reopened=readWorkbook(path);const auto calculated=CalculationEngine().calculate(reopened.workbook);
 for(const auto& cell:cells){const int row=cell.at("row");const auto expected=cell.at("input").get<std::string>();const auto actual=std::find_if(reopened.workbook.at("sheets")[0].at("cells").begin(),reopened.workbook.at("sheets")[0].at("cells").end(),[&](const Json& item){return item.at("row")==row&&item.at("col")==1;});
  require(actual!=reopened.workbook.at("sheets")[0].at("cells").end()&&actual->at("input")==expected,"BIFF ordinary error-looking text retains its canonical escape and exact apostrophe content");
  require(actual->at("style").value("quotePrefix",true)==cell.at("style").at("quotePrefix"),"BIFF ordinary text preserves both actual native quote-prefix flags, including the existing canonical true default");
  require(valueAt(calculated,row).at("type")=="text"&&valueAt(calculated,row).at("value")==expected.substr(1),"BIFF ordinary error-looking text remains text rather than becoming a typed error");
 }
 require(book.at("sheets")[0].at("cells")==cells,"BIFF ordinary-text serialization leaves authored values and prefix flags unchanged");
}
void nativeBiffUnsupportedError(){
 auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"].push_back({{"row",1},{"col",1},{"input","7"}});
 QTemporaryDir files;require(files.isValid(),"Create native BIFF unsupported-error save fixture");const auto path=files.filePath("existing.xls");writeBiffWorkbook(path.toStdString(),book);
 auto package=readWorkbook(path.toStdString());QFile file(path);require(file.open(QIODevice::ReadOnly),"Read pre-existing BIFF file bytes");const auto bytes=file.readAll();file.close();
 for(const auto* error:{"#FIELD!","#SPILL!","#CALC!"}){auto unsupported=book;unsupported["sheets"][0]["cells"][0]["input"]=error;const auto before=unsupported;const auto sourceBefore=package.workbook;const auto sourceHash=package.sourceHash;bool rejected=false;
  try{writeBiffWorkbook(path.toStdString(),unsupported,&package);}catch(const std::exception&){rejected=true;}
  require(rejected,"A real modern error unsupported by native BIFF codes is explicitly rejected instead of silently saved as text");
  require(file.open(QIODevice::ReadOnly),"Reopen BIFF file after rejected save");const auto after=file.readAll();file.close();
  require(after==bytes&&unsupported==before&&package.workbook==sourceBefore&&package.sourceHash==sourceHash,"Unsupported modern BIFF error save preserves existing file bytes, authored input and source package atomically");
 }
 const std::vector<std::string> legacy={"#NULL!","#DIV/0!","#VALUE!","#REF!","#NAME?","#NUM!","#N/A"};auto supported=book;supported["sheets"][0]["cells"]=Json::array();
 for(const auto& error:legacy)supported["sheets"][0]["cells"].push_back({{"row",supported["sheets"][0]["cells"].size()+1},{"col",1},{"input",error}});
 writeBiffWorkbook(path.toStdString(),supported);const auto calculated=CalculationEngine().calculate(readWorkbook(path.toStdString()).workbook);
 for(std::size_t index=0;index<legacy.size();++index)require(valueAt(calculated,index+1).at("type")=="error"&&valueAt(calculated,index+1).at("value")==legacy[index],"All seven native legacy BIFF error codes remain real errors");
}
void nativeCompatibilityRanking(){
 CalculationEngine engine;QFile file(QStringLiteral(TIGERSHEETS_SOURCE_DIR "/compatibility/functions.json"));require(file.open(QIODevice::ReadOnly),"Read actual native compatibility categories");
 FormulaLineEdit editor;editor.setFunctionCatalog(Json::parse(file.readAll().toStdString()),engine.functions());editor.resize(620,30);editor.show();editor.activateWindow();editor.setFocus();QApplication::processEvents();QSignalSpy committed(&editor,&QLineEdit::returnPressed);
 auto names=[&]{QStringList result;auto* popup=editor.completionPopup();for(int i=0;i<popup->count();++i)result<<popup->item(i)->data(Qt::UserRole).toString();return result;};
 QTest::keyClicks(&editor,"=F");const QStringList source={"F.DIST","F.DIST.RT","F.INV","F.INV.RT","F.TEST","FACT","FACTDOUBLE","FALSE","FIELDVALUE"};
 require(names().mid(0,9)==source,"The real source F list retains its first nine modern functions, including FIELDVALUE ninth");
 const auto all=names();require(all.contains("FDIST"),"Legacy FDIST remains in broad completion rather than being hidden");bool contains=false;for(const auto& name:all){if(!name.startsWith('F'))contains=true;else require(!contains,"Every actual prefix match remains before all contains matches despite compatibility ranking");}
 editor.setText("=PERCENTIL");require(names().mid(0,3)==QStringList{"PERCENTILE.EXC","PERCENTILE.INC","PERCENTILE"},"Modern PERCENTILE names precede their retained Compatibility-category predecessor");
 for(const auto* name:{"PERCENTILE","RANK","STDEV"}){editor.setText(QString("=")+name);QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()==QString("=")+name+"("&&committed.empty(),"A fully typed legacy name accepts itself rather than a longer modern name sharing its prefix");}
 editor.setText("=FDIST");require(editor.completionPopup()->isVisible()&&editor.completionPopup()->currentItem()->data(Qt::UserRole)=="FDIST","Typing an exact legacy function still selects its real completion");QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()=="=FDIST("&&committed.empty(),"Legacy completion remains keyboard-insertable without committing");QTest::keyClicks(&editor,"1,1,1)");
 auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"].push_back({{"row",1},{"col",1},{"input",editor.text().toStdString()}});require(std::abs(valueAt(engine.calculate(book),1).at("value").get<double>()-0.5)<1e-10,"The accepted legacy FDIST remains calculable");
}
void nativeXlsxErrorCase(bool literalOnly){
 auto book=WorkbookSession().snapshot();auto& cells=book["sheets"][0]["cells"];
 for(const auto& text:{"#field!","#FiElD!","#spill!","#CaLc!","#n/a"})for(const bool prefix:{false,true})cells.push_back({{"row",cells.size()+1},{"col",1},{"input",std::string("'")+text},{"style",{{"quotePrefix",prefix}}}});
 const int errorRow=cells.size()+1;cells.push_back({{"row",errorRow},{"col",1},{"input","#field!"}});
 QTemporaryDir files;require(files.isValid(),"Create native XLSX mixed-case error-looking input fixture");const auto path=files.filePath("mixed-case.xlsx").toStdString();writeWorkbook(path,book);const auto reopened=readWorkbook(path);const auto calculated=CalculationEngine().calculate(reopened.workbook);
 if(literalOnly){pugi::xml_document xml;require(xml.load_string(reopened.entries.at("xl/worksheets/sheet1.xml").c_str()),"Read native mixed-case literal error XML");const auto reference=std::string("A")+std::to_string(errorRow);const auto node=xml.select_node(("//c[@r='"+reference+"']").c_str()).node();require(std::string(node.attribute("t").value())=="e"&&std::string(node.child("v").text().get())=="#FIELD!","An unquoted case-insensitive native error is stored with a real normalized error code");require(valueAt(calculated,errorRow).at("type")=="error"&&valueAt(calculated,errorRow).at("value")=="#FIELD!","A reopened mixed-case actual error preserves typed error identity");return;}
 for(int row=1;row<errorRow;++row){const auto text=cells[row-1].at("input").get<std::string>().substr(1);require(valueAt(calculated,row).at("type")=="text"&&valueAt(calculated,row).at("value")==text,"Quoted mixed-case XLSX error-looking text retains its exact case/content and ordinary text identity");}
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{const std::string mode=argc>1?argv[1]:"all";if(mode=="all"||mode=="completion")nativeCompletion();if(mode=="all"||mode=="xlsx")nativeXlsxErrors();if(mode=="all"||mode=="external")nativeExternalError();if(mode=="all"||mode=="biff")nativeBiffText();if(mode=="all"||mode=="biff_reject")nativeBiffUnsupportedError();if(mode=="all"||mode=="ranking")nativeCompatibilityRanking();if(mode=="all"||mode=="case_text")nativeXlsxErrorCase(false);if(mode=="all"||mode=="case_error")nativeXlsxErrorCase(true);std::cout<<"FIELDVALUE "<<mode<<" passed\n";return 0;}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
