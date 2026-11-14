#include "tigersheets/printing.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QFile>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QProcess>
#include <QPrintDialog>
#include <QRegularExpression>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool result,const char* message){if(!result)throw std::runtime_error(message);}
QString pdfText(const QString& path,const QStringList& options={}){
 QProcess process;QStringList arguments=options;arguments<<path<<"-";process.start("/usr/bin/pdftotext",arguments);
 require(process.waitForStarted(3000)&&process.waitForFinished(10000)&&process.exitCode()==0,"independent PDF text extraction must succeed");return QString::fromUtf8(process.readAllStandardOutput());
}
void select(QTableView& view,int bottom,int right){const auto first=view.model()->index(0,0),last=view.model()->index(bottom,right);view.setCurrentIndex(first);view.selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);}
void selectedDisplayValues(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);
 require(model.apply(Json::array({
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","Quarter"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",2},{"input","AnnualValue"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",1},{"input","KnownRow"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",2},{"input","=2+3"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",3},{"input","OutsideSelection"}}
 })),"create print fixture");model.setCalculated(CalculationEngine().calculate(session.snapshot()));
 view.setColumnWidth(0,120);view.setColumnWidth(1,280);select(view,1,1);const auto original=session.snapshot();QTemporaryDir files;const auto path=files.filePath("worksheet.pdf");
 require(printWorksheet(nullptr,model,view,path),"PDF export must report success");QFile file(path);require(file.open(QIODevice::ReadOnly)&&file.read(5)=="%PDF-","actual Qt printer must write a PDF document");
 const auto text=pdfText(path);require(text.contains("Quarter")&&text.contains("AnnualValue")&&text.contains("KnownRow")&&text.contains('5'),"selected display content and calculated formula value must be printable");
 require(!text.contains("=2+3")&&!text.contains("OutsideSelection"),"print selected displayed cells only");require(session.snapshot()==original,"printing must not mutate the workbook");
}
struct WordBox{double x,y,width,height;};
WordBox wordBox(const QString& xml,const QString& word){
 const QRegularExpression expression("<word xMin=\"([0-9.]+)\" yMin=\"([0-9.]+)\" xMax=\"([0-9.]+)\" yMax=\"([0-9.]+)\">"+QRegularExpression::escape(word)+"</word>");
 const auto match=expression.match(xml);require(match.hasMatch(),"expected complete word must survive PDF text clipping");return {match.captured(1).toDouble(),match.captured(2).toDouble(),match.captured(3).toDouble()-match.captured(1).toDouble(),match.captured(4).toDouble()-match.captured(2).toDouble()};
}
void visibleLayout(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);
 require(model.apply(Json::array({
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","LEFT"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",2},{"input","RIGHT"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",1},{"input","small"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",2},{"input","LargeHeadingComplete"},{"style",{{"fontFamily","DejaVu Sans"},{"fontSize",24},{"bold",true},{"color","#a83224"},{"fill","#fff2cc"},{"alignment","center"}}}}
 })),"create styled layout fixture");view.setColumnWidth(0,80);view.setColumnWidth(1,480);view.setRowHeight(0,30);view.setRowHeight(1,80);select(view,1,1);
 QTemporaryDir files;const auto path=files.filePath("layout.pdf");require(printWorksheet(nullptr,model,view,path),"styled worksheet PDF export");const auto boxes=pdfText(path,{"-bbox"});const auto left=wordBox(boxes,"LEFT"),right=wordBox(boxes,"RIGHT"),small=wordBox(boxes,"small"),large=wordBox(boxes,"LargeHeadingComplete");
 require((right.x-left.x)/small.height>3&&(right.x-left.x)/small.height<8,"printed columns must retain the grid's 80:480 proportions");
 require(large.height/small.height>1.8&&large.height/small.height<2.6,"styled 24pt text must retain its size relative to the 11pt grid font");
}
void rowPagination(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);
 require(model.apply(Json::array({
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","PAGEFIRST"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",70},{"col",1},{"input","PAGEMIDDLE"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",150},{"col",1},{"input","PAGELAST"}}
 })),"create pagination fixture");view.verticalHeader()->setDefaultSectionSize(30);view.setColumnWidth(0,160);view.setColumnWidth(1,160);select(view,149,1);
 QTemporaryDir files;const auto path=files.filePath("pages.pdf");require(printWorksheet(nullptr,model,view,path),"multi-page PDF export");const auto text=pdfText(path);
 require(text.count("PAGEFIRST")==1&&text.count("PAGEMIDDLE")==1&&text.count("PAGELAST")==1,"pagination must print every selected row exactly once");
 require(text.count(QChar('\f'))>=3&&text.lastIndexOf(QChar('\f'),text.indexOf("PAGELAST"))>=0,"late rows must continue on subsequent PDF pages");
}
void savedAreaAndHeaders(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);view.verticalHeader()->setDefaultSectionSize(30);view.setColumnWidth(0,180);view.setColumnWidth(1,180);
 require(model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","REPEATHEADER"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",100},{"col",2},{"input","AREAEND"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",3},{"input","OUTSIDEAREA"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",130},{"col",1},{"input","OUTSIDEAREA"}},{{"type","set_metadata"},{"key","settings"},{"value",{{"printSheets",{{"sheet-1",{{"area",{{"row1",1},{"col1",1},{"row2",100},{"col2",2}}},{"repeatHeaderRows",{{"row1",1},{"row2",1}}},{"fitWidth",true}}}}}}}}})),"saved print settings fixture");select(view,0,0);QTemporaryDir files;const auto path=files.filePath("saved-area.pdf");require(printWorksheet(nullptr,model,view,path),"print saved report area");const auto text=pdfText(path);require(text.contains("AREAEND")&&!text.contains("OUTSIDEAREA")&&text.count("REPEATHEADER")>=2&&text.count("REPEATHEADER")==text.count(QChar('\f')),"saved area must exclude outside cells and repeat its header on every PDF page");
}
void horizontalPagination(){WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);for(int c=0;c<4;++c)view.setColumnWidth(c,300);require(model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","WIDELEFT"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",4},{"input","WIDERIGHT"}},{{"type","set_metadata"},{"key","settings"},{"value",{{"printSheets",{{"sheet-1",{{"area",{{"row1",1},{"col1",1},{"row2",2},{"col2",4}}},{"fitWidth",false}}}}}}}}})),"actual-size print fixture");QTemporaryDir files;const auto path=files.filePath("wide.pdf");require(printWorksheet(nullptr,model,view,path),"actual-size printing must paginate across columns");const auto text=pdfText(path);require(text.contains("WIDELEFT")&&text.contains("WIDERIGHT")&&text.count(QChar('\f'))>=2,"horizontal pagination must retain both sides of the worksheet");}
void chartGraphics(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);view.horizontalHeader()->setDefaultSectionSize(100);view.verticalHeader()->setDefaultSectionSize(30);
 require(model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","Quarter"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",2},{"input","Revenue"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",1},{"input","Q1"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",2},{"col",2},{"input","25"}}})),"chart print data");
 Json chart={{"id","print-chart"},{"sheet","sheet-1"},{"title","NATIVECHARTPDF"},{"type","column"},{"range",{{"row1",1},{"col1",1},{"row2",2},{"col2",2}}},{"anchor",{{"row",5},{"col",1},{"width",480},{"height",280}}}};
 require(model.apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",Json::array({chart})}}})),"print chart fixture");auto calculated=CalculationEngine().calculate(session.snapshot());model.setCalculated(calculated);select(view,0,0);QTemporaryDir files;const auto path=files.filePath("chart.pdf");require(printWorksheet(nullptr,model,view,path,session.snapshotView(),calculated),"print chart PDF");require(pdfText(path).contains("NATIVECHARTPDF"),"default used print area must include chart graphics beyond the cell data");
 QProcess raster;raster.start("/usr/bin/pdftoppm",{"-scale-to","900","-singlefile","-png",path,files.filePath("chart")});require(raster.waitForStarted(3000)&&raster.waitForFinished(10000)&&raster.exitCode()==0,"PDF chart rasterization");QImage image(files.filePath("chart.png"));int green=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto color=image.pixelColor(x,y);if(color.green()>color.red()+20&&color.green()>color.blue()+5&&color.green()<140)++green;}require(green>100,"PDF must contain actual colored series graphics");
}
void rangeLimits(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);QTemporaryDir files;
 select(view,1,199);require(printWorksheet(nullptr,model,view,files.filePath("supported-200-columns.pdf")),"report printing must support the full 200-column output capacity");
 for(const auto& [bottom,right]:{std::pair{1,200},std::pair{10000,1},std::pair{1999,199}}){select(view,bottom,right);const auto path=files.filePath("oversized.pdf");bool rejected=false;try{printWorksheet(nullptr,model,view,path);}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("10000 rows, 200 columns or 250000 cells")!=std::string::npos;}require(rejected,"oversized selections must fail before opening the PDF output");require(!QFile::exists(path),"rejected print range must not create a partial PDF");}
}
void oversizedRow(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);view.setRowHeight(0,6000);select(view,1,1);QTemporaryDir files;const auto path=files.filePath("tall.pdf");bool rejected=false;try{printWorksheet(nullptr,model,view,path);}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("row is taller than the printable page")!=std::string::npos;}
 require(rejected&&!QFile::exists(path),"a row too tall to paginate must be diagnosed before PDF creation");
}
void defaultRangeAndCancel(){
 WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);require(model.apply(Json::array({
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","DEFAULTFIRST"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",40},{"col",9},{"input","DEFAULTLAST"}},
  {{"type","set_cell"},{"sheet","sheet-1"},{"row",41},{"col",10},{"input","OUTSIDEDEFAULT"}}
 })),"create default print fixture");view.horizontalHeader()->setDefaultSectionSize(160);select(view,0,0);QTemporaryDir files;const auto path=files.filePath("default.pdf");require(printWorksheet(nullptr,model,view,path),"single current cell prints the actual used worksheet area");const auto text=pdfText(path);require(text.contains("DEFAULTFIRST")&&text.contains("DEFAULTLAST")&&text.contains("OUTSIDEDEFAULT"),"default printing must include used cells beyond the former A1:I40 placeholder");
 const auto original=session.snapshot();bool rejectedDialog=false;QTimer timer;timer.setSingleShot(true);QObject::connect(&timer,&QTimer::timeout,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QPrintDialog*>(widget)){rejectedDialog=true;dialog->reject();}});timer.start(0);require(!printWorksheet(nullptr,model,view),"canceling native printer dialog returns false");require(rejectedDialog&&session.snapshot()==original,"canceling native print dialog must leave workbook unchanged");
 bool rejectedPath=false;try{printWorksheet(nullptr,model,view,files.filePath("missing/output.pdf"));}catch(const std::runtime_error& error){rejectedPath=std::string(error.what()).find("printer")!=std::string::npos;}require(rejectedPath,"unwritable PDF destination must report a printer error");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{selectedDisplayValues();visibleLayout();rowPagination();savedAreaAndHeaders();horizontalPagination();chartGraphics();rangeLimits();oversizedRow();defaultRangeAndCancel();std::cout<<"printing tests passed\n";}catch(const std::exception& error){std::cerr<<"printing test failed: "<<error.what()<<'\n';return 1;}}
