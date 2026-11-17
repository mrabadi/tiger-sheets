#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QImage>
#include <QPointer>
#include <QTabBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
 MainWindow window;WorksheetView* grid;WorksheetModel* model;QLineEdit* bar;QTabBar* tabs;QString origin;
 Fixture(){window.resize(1400,850);window.show();window.activateWindow();QApplication::processEvents();grid=dynamic_cast<WorksheetView*>(window.findChild<QTableView*>("worksheet.grid"));model=dynamic_cast<WorksheetModel*>(grid->model());bar=window.findChild<QLineEdit*>("formula.input");tabs=window.findChild<QTabBar*>("workspace.sheets");require(grid&&model&&bar&&tabs,"cross-sheet fixture exposes the real worksheet and formula editors");origin=model->sheetId();require(model->apply(Json::array({{{"type","add_sheet"},{"id","cross-inputs"},{"name","Inputs 2026"}},{{"type","set_cell"},{"sheet",origin.toStdString()},{"row",4},{"col",3},{"input","17"}},{{"type","set_cell"},{"sheet","cross-inputs"},{"row",2},{"col",2},{"input","42"}}})),"seed an origin cell and a separate input worksheet");grid->setCurrentIndex(model->index(3,2));window.recalculateNow();QApplication::processEvents();}
 void chooseInput(){QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualRect(model->index(1,1)).center());QApplication::processEvents();}
};
void controlEnterKeepsTheOrigin(){
 for(const bool inCell:{true,false}){
  Fixture f;const auto target=f.model->index(11,2);require(f.model->setData(f.model->index(8,2),"3")&&f.model->setData(f.model->index(21,2),"11205"),"prepare independent source-style C9/C22 inputs");f.grid->setCurrentIndex(target);f.grid->scrollTo(target);const auto before=f.window.currentSession().snapshot();f.grid->setFocus();QLineEdit* editor=f.bar;if(inCell){QTest::keyClick(f.grid,Qt::Key_F2);editor=f.grid->activeCellEditor();require(editor,"open the genuine C12 inline editor");}editor->setFocus();editor->selectAll();QTest::keyClicks(editor,"=C22/C9");QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);QApplication::processEvents();f.window.recalculateNow();require(f.grid->currentIndex()==target&&f.window.findChild<QLineEdit*>("formula.name")->text()=="C12","CtrlEnter accepts the source formula while retaining C12 rather than moving down");require(!f.grid->activeCellEditor()&&f.model->raw(11,2)=="=C22/C9"&&f.model->numericValue(11,2)==3735,"CtrlEnter finishes either genuine entry surface with the exact formula and value");require(f.window.executeCommand("undo")&&f.window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"retained-cell formula acceptance is exactly one native Undo");
 }
 Fixture f;const auto before=f.window.currentSession().snapshot();f.bar->setFocus();f.bar->selectAll();QTest::keyClicks(f.bar,"=");QTest::keyClick(f.bar,Qt::Key_PageDown,Qt::ControlModifier);QApplication::processEvents();f.chooseInput();QTest::keyClick(f.bar,Qt::Key_Enter,Qt::ControlModifier);QApplication::processEvents();require(f.model->sheetId()==f.origin&&f.grid->currentIndex()==f.model->index(3,2)&&f.model->raw(3,2)=="='Inputs 2026'!B2","Ctrl keypadEnter returns to and retains the actual cross-sheet origin");require(f.window.executeCommand("undo")&&f.window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"cross-sheet CtrlEnter commits only one transaction and preserves source cells");
}
void formulaBarPointsAcrossSheets(){
 Fixture f;const auto before=f.window.currentSession().snapshot();f.bar->setFocus();f.bar->selectAll();QTest::keyClicks(f.bar,"=");
 QTest::keyClick(f.bar,Qt::Key_PageDown,Qt::ControlModifier);QApplication::processEvents();
 require(f.tabs->currentIndex()==1&&f.model->sheetId()=="cross-inputs","Ctrl+PageDown visits the input worksheet while a formula-bar draft is active");
 require(f.bar->text()=="="&&f.window.currentSession().snapshot()==before,"switching reference worksheets keeps the formula draft uncommitted");
 f.chooseInput();require(f.bar->text()=="='Inputs 2026'!B2","clicking the other worksheet inserts its quoted sheet-qualified address");
 require(f.window.currentSession().snapshot()==before,"pointing to another worksheet never commits either cell");
 QTest::keyClick(f.bar,Qt::Key_Return);QApplication::processEvents();
 require(f.tabs->currentIndex()==0&&f.model->sheetId()==f.origin&&f.model->raw(3,2)=="='Inputs 2026'!B2","Enter commits the formula to its original worksheet and returns there");
 const auto* input=f.window.currentSession().cellAt("cross-inputs",2,2);require(input&&input->at("input")=="42"&&!f.window.currentSession().cellAt("cross-inputs",4,3),"cross-sheet formula entry preserves all source cells");
 f.window.recalculateNow();require(f.model->data(f.model->index(3,2)).toString()=="42","the committed cross-sheet formula calculates its input value");
 require(f.grid->hasFocus()&&f.grid->currentIndex()==f.model->index(4,2),"Enter resumes ordinary downward navigation on the origin worksheet");
 require(f.window.executeCommand("undo")&&f.window.currentSession().snapshot().at("sheets")==before.at("sheets"),"one Undo restores the original cell without changing the source sheet");
}
void inCellDraftPointsAcrossSheets(){
 Fixture f;const auto before=f.window.currentSession().snapshot();f.grid->setFocus();QTest::keyClick(f.grid,Qt::Key_F2);QApplication::processEvents();QPointer<QLineEdit> editor=f.grid->activeCellEditor();require(editor,"F2 opens the origin cell's native editor");editor->selectAll();QTest::keyClicks(editor,"=");
 QTest::keyClick(editor,Qt::Key_PageDown,Qt::ControlModifier);QApplication::processEvents();
 require(f.tabs->currentIndex()==1&&f.model->sheetId()=="cross-inputs","Ctrl+PageDown visits the input worksheet while an in-cell draft is active");
 require(f.bar->hasFocus()&&f.bar->text()=="="&&!f.grid->activeCellEditor(),"cross-sheet cell entry continues in the visible formula bar");
 f.chooseInput();require(f.bar->text()=="='Inputs 2026'!B2"&&f.window.currentSession().snapshot()==before,"in-cell cross-sheet pointing preserves the original draft without committing");
 QTest::keyClick(f.bar,Qt::Key_Return);QApplication::processEvents();
 require(f.model->sheetId()==f.origin&&f.model->raw(3,2)=="='Inputs 2026'!B2"&&f.grid->currentIndex()==f.model->index(4,2),"Enter after in-cell pointing commits and advances on the origin worksheet");
 const auto* input=f.window.currentSession().cellAt("cross-inputs",2,2);require(input&&input->at("input")=="42"&&!f.window.currentSession().cellAt("cross-inputs",4,3),"in-cell pointing leaves the input worksheet unchanged");
}
void tabPointingAndEscapePreserveBothWorksheets(){
 for(const bool inCell:{false,true}){
  Fixture f;require(f.model->apply(Json::array({{{"type","rename_sheet"},{"sheet","cross-inputs"},{"name","Inputs'2026"}}})),"rename the input sheet with an apostrophe");const auto before=f.window.currentSession().snapshot();QLineEdit* editor=f.bar;
  if(inCell){f.grid->setFocus();QTest::keyClick(f.grid,Qt::Key_F2);QApplication::processEvents();editor=f.grid->activeCellEditor();require(editor,"open in-cell tab-pointing draft");}editor->setFocus();editor->selectAll();QTest::keyClicks(editor,"=");
  QTest::mouseClick(f.tabs,Qt::LeftButton,Qt::NoModifier,f.tabs->tabRect(1).center());QApplication::processEvents();
  require(f.tabs->currentIndex()==1&&f.bar->text()=="="&&f.window.currentSession().snapshot()==before,"clicking a worksheet tab preserves an uncommitted formula draft from either editor");
  f.chooseInput();require(f.bar->text()=="='Inputs''2026'!B2","sheet reference pointing escapes embedded apostrophes");
  f.bar->setFocus();QTest::keyClick(f.bar,Qt::Key_Escape);QApplication::processEvents();
  require(f.tabs->currentIndex()==0&&f.model->sheetId()==f.origin&&f.grid->currentIndex()==f.model->index(3,2),"Escape returns to the original worksheet and cell");
  require(f.bar->text()=="17"&&f.window.currentSession().snapshot()==before&&!f.grid->activeCellEditor(),"Escape restores the original formula bar and leaves both worksheets unchanged");
 }
}
int outlinePixels(WorksheetView& view,const QModelIndex& cell,const QColor& color){
 const auto rect=view.visualRect(cell);const auto image=view.viewport()->grab().toImage();int count=0;for(int y=std::max(0,rect.top());y<=std::min(image.height()-1,rect.bottom());++y)for(int x=std::max(0,rect.left());x<=std::min(image.width()-1,rect.left()+2);++x)if(image.pixelColor(x,y)==color)++count;return count;
}
void localReferenceOutlinesStayOnTheirOriginSheet(){
 Fixture f;const auto before=f.window.currentSession().snapshot();f.bar->setFocus();f.bar->selectAll();QTest::keyClicks(f.bar,"=A1+");QApplication::processEvents();
 require(outlinePixels(*f.grid,f.model->index(0,0),QColor("#3f6db5"))>12,"the origin worksheet visibly outlines its local A1 reference");
 QTest::keyClick(f.bar,Qt::Key_PageDown,Qt::ControlModifier);QApplication::processEvents();
 require(outlinePixels(*f.grid,f.model->index(0,0),QColor("#3f6db5"))==0,"a local origin reference is not outlined on the input worksheet");
 f.chooseInput();require(f.bar->text()=="=A1+'Inputs 2026'!B2","pointing preserves local operands alongside a cross-sheet reference");
 QTest::keyClick(f.bar,Qt::Key_PageUp,Qt::ControlModifier);QApplication::processEvents();require(outlinePixels(*f.grid,f.model->index(0,0),QColor("#3f6db5"))>12,"returning to the origin worksheet restores its local reference outline");
 QTest::keyClick(f.bar,Qt::Key_Escape);QApplication::processEvents();require(f.window.currentSession().snapshot()==before,"inspecting reference outlines across worksheets leaves the workbook unchanged");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{if(qgetenv("TIGERSHEETS_CROSS_SHEET_TEST_CASE")=="ctrl_enter"){controlEnterKeepsTheOrigin();std::cout<<"Native retained-cell CtrlEnter passed\n";return 0;}controlEnterKeepsTheOrigin();formulaBarPointsAcrossSheets();inCellDraftPointsAcrossSheets();tabPointingAndEscapePreserveBothWorksheets();localReferenceOutlinesStayOnTheirOriginSheet();std::cout<<"cross-sheet formula entry tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
