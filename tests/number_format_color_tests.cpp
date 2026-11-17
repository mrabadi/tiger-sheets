#include "tigersheets/grid.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QTabBar>
#include <QTemporaryDir>
#include <QScrollBar>
#include "tigersheets/calculation.hpp"
#include "tigersheets/formula_error_checks.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QColor>
#include <QTest>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
void sourceNegative(){
 const auto source=readWorkbook(std::string(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/financial-source.xls");
 WorkbookSession session(source.workbook);WorksheetModel model(session);model.setSheetId("sheet-33");const auto before=session.snapshot();
 for(const auto [row,col]:{std::pair{17,13},std::pair{17,19},std::pair{20,19}}){
  require(model.numericValue(row-1,col-1).value_or(0)<0,"the retained source amount is negative");
  require(QString::fromStdString(model.cellStyle(row-1,col-1).at("numberFormat").get<std::string>()).contains("[Red]",Qt::CaseInsensitive),"source XLS retains its negative-section color directive");
  require(model.data(model.index(row-1,col-1),Qt::ForegroundRole).value<QColor>()==QColor("#ff0000"),"source negative number-format section overrides the black base font with actual red ink");
 }
 require(model.data(model.index(16,6),Qt::ForegroundRole).value<QColor>()==QColor("#000000"),"the source positive year retains its black base font");
 require(session.snapshot()==before&&!session.canUndo(),"reading display colors never changes source formats, values or history");
}
void sections(){
 WorkbookSession session;WorksheetModel model(session);const auto sheet=model.sheetId().toStdString();
 const std::string format="[Blue]0;[Red](0);[Green]0";
 Json operations=Json::array();for(const auto [row,input]:{std::pair{1,"5"},std::pair{2,"-5"},std::pair{3,"0"},std::pair{4,"=-2"}})operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",row},{"col",1},{"input",input},{"style",{{"color","#ff00ff"},{"numberFormat",format}}}});
 operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",5},{"col",1},{"input","-5"},{"style",{{"color","#ff00ff"},{"numberFormat","0;(0)"}}}});
 require(model.apply(operations),"prepare ordinary values and a calculated formula with independent base font colors");
 model.setCalculated(CalculationEngine().calculate(session.snapshotView()));const auto before=session.snapshot();
 const QColor expected[]={QColor("#0000ff"),QColor("#ff0000"),QColor("#00ff00"),QColor("#ff0000"),QColor("#ff00ff")};
 for(int row=0;row<5;++row)require(model.data(model.index(row,0),Qt::ForegroundRole).value<QColor>()==expected[row],"the chosen numeric format section controls literal/formula/zero colors, while uncolored sections retain the font color");
 require(model.raw(3,0)=="=-2"&&model.numericValue(3,0)==-2&&session.snapshot()==before,"numeric display coloring preserves formula, actual result and history");
}
void requireSourceTriangle(const QImage& image,const QRect& cell){
 int pixels=0;
 for(int y=0;y<10;++y)for(int x=0;x<10;++x){
  const bool actual=image.pixelColor(cell.topLeft()+QPoint(x,y))==QColor("#008000");
  const bool expected=x<6&&y<6&&x+y<6;
  pixels+=actual;
  if(actual!=expected){std::cerr<<"Warning geometry mismatch at relative "<<x<<','<<y<<" actual="<<actual<<" expected="<<expected<<'\n';require(false,"source warning triangle has six-pixel legs at the first cell content pixel, without an inset");}
 }
 require(pixels==21,"source warning triangle consists of exactly21 native green pixels");
}
void sourceTextWarningsBeforeBackgroundCalculation(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();require(window.openWorkbook(QString::fromStdString(std::string(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/financial-source.xls")),"open the untouched120-sheet source through the real MainWindow lifecycle");auto* tabs=window.findChild<QTabBar*>("workspace.sheets");auto* view=window.findChild<WorksheetView*>("worksheet.grid");require(tabs&&view,"source window exposes real worksheet tabs and grid");tabs->setCurrentIndex(32);auto* model=dynamic_cast<WorksheetModel*>(view->model());require(model&&model->sheetId()=="sheet-33","select the actual Operations tab through the native UI");const auto before=window.currentSession().snapshot();
 for(const int column:{7,13,19})require(model->data(model->index(5,column-1),FormulaErrorCheckRole).toBool(),"actual imported text-year warnings are available immediately, before whole-book background calculation");
 QTest::keyClick(view,Qt::Key_F1,Qt::ControlModifier);
 view->verticalScrollBar()->setValue(0);view->horizontalScrollBar()->setValue(0);QApplication::processEvents();
 const auto history=window.currentSession().historyBytes();const bool undo=window.currentSession().canUndo(),redo=window.currentSession().canRedo();
 for(const bool gridLines:{true,false}){
  view->setShowGrid(gridLines);QApplication::processEvents();const auto image=view->viewport()->grab().toImage();
  for(const int column:{7,13,19}){const auto rect=view->visualRect(model->index(5,column-1));require(rect.intersects(view->viewport()->rect()),"all three actual merged source-year headings are visible");requireSourceTriangle(image,rect);}
  const auto ordinary=view->visualRect(model->index(7,6));
  for(int y=0;y<10;++y)for(int x=0;x<10;++x)require(image.pixelColor(ordinary.topLeft()+QPoint(x,y))!=QColor("#008000"),"a source numeric amount has no warning artwork");
  require(window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history&&window.currentSession().canUndo()==undo&&window.currentSession().canRedo()==redo,"warning geometry and gridline visibility preserve source data and Undo/Redo history");
 }
 view->setShowGrid(true);QApplication::processEvents();
 if(const auto* capture=std::getenv("TIGERSHEETS_RAW_WARNING_CAPTURE"))require(window.grab().save(QString::fromLocal8Bit(capture)),"save the actual source-window warning geometry capture");
 model->setCalculated({{"revision",before.at("revision")},{"status","stale"},{"values",Json::array()}});require(model->data(model->index(5,6),FormulaErrorCheckRole).toBool(),"an intrinsic input-type warning survives a pending unrelated calculation");require(!model->data(model->index(7,6),FormulaErrorCheckRole).toBool(),"ordinary source numeric amounts remain unflagged");require(window.currentSession().snapshot()==before&&!window.currentSession().canUndo(),"opening, tabbing and painting intrinsic checks change no data, type, style or history");
}
void sourceTextWarnings(){
 const auto source=readWorkbook(std::string(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/financial-source.xls");
 WorkbookSession session(source.workbook);WorksheetModel model(session);model.setSheetId("sheet-33");const auto before=session.snapshot();
 const auto calculated=CalculationEngine().calculate(session.snapshotView());model.setCalculated(calculated);
 for(const int column:{7,13,19}){
  require(model.raw(5,column-1).startsWith('\''),"retained XLS year headings preserve their actual string type");
  require(model.data(model.index(5,column-1),FormulaErrorCheckRole).toBool(),"actual source text years publish their native green warning marker");
 }
 require(!model.data(model.index(7,6),FormulaErrorCheckRole).toBool(),"ordinary raw numeric amounts are not text-number warnings");
 require(session.snapshot()==before&&!session.canUndo(),"actual source calculation/display warnings preserve all120sheets and history");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{sourceTextWarningsBeforeBackgroundCalculation();sourceNegative();sections();sourceTextWarnings();std::cout<<"Native number-format colors and raw text warnings passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
