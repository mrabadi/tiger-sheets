#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Surface {
 MainWindow window;WorksheetView* view;WorksheetModel* model;QLineEdit* bar;
 Surface(){window.resize(1200,720);window.show();view=window.findChild<WorksheetView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(view->model());bar=window.findChild<QLineEdit*>("formula.input");require(view&&model&&bar,"native workbook exposes worksheet and formula editor");}
 void activate(){window.activateWindow();view->setFocus();QApplication::processEvents();}
};
void stableOrigin(const Surface& surface,const QModelIndex& target,const char* stage){
 const auto selected=surface.view->selectionModel()->selection();
 std::cout<<stage<<" current="<<surface.view->currentIndex().row()+1<<','<<surface.view->currentIndex().column()+1<<" selection=";
 for(const auto& area:selected)std::cout<<area.top()+1<<','<<area.left()+1<<':'<<area.bottom()+1<<','<<area.right()+1<<';';std::cout<<'\n';
 require(surface.view->currentIndex()==target,"cross-workbook pointing must not move the formula origin's active cell before Enter");
 require(selected.size()==1&&selected.front().topLeft()==target&&selected.front().bottomRight()==target,"cross-workbook pointing must not shift the formula origin's visible selection before Enter");
}
void referenceSelection(bool inlineEntry,bool mouseActivation,bool cancel){
 QTemporaryDir files;Surface origin,source;
 const auto target=origin.model->index(7,2),neighbor=origin.model->index(7,3),point=source.model->index(3,5);
 require(source.model->setData(point,"42")&&source.window.saveWorkbook(files.filePath("Selection Source.xlsx")),"prepare a genuinely saved source workbook");
 require(origin.model->setData(neighbor,"neighbor"),"prepare untouched cell beside the formula origin");
 origin.view->setCurrentIndex(target);origin.view->scrollTo(target);source.view->setCurrentIndex(source.model->index(1,1));
 const auto before=origin.window.currentSession().snapshot(),sourceBefore=source.window.currentSession().snapshot();
 origin.activate();QLineEdit* editor=origin.bar;
 if(inlineEntry){QTest::keyClicks(origin.view,"=");QApplication::processEvents();editor=origin.view->activeCellEditor();require(editor,"typing equals starts the native inline formula editor");}
 else{origin.bar->setFocus();QTest::keyClicks(origin.bar,"=");}
 std::cout<<"case inline="<<inlineEntry<<" mouseActivation="<<mouseActivation<<" cancel="<<cancel<<'\n';
 stableOrigin(origin,target,"draft");
 if(mouseActivation)source.activate();else{QTest::keyClick(editor,Qt::Key_Tab,Qt::ControlModifier);QTest::keyRelease(source.bar,Qt::Key_Control);QApplication::processEvents();}
 require(QApplication::activeWindow()==&source.window,"the genuine source workbook becomes active");
 stableOrigin(origin,target,"source activated");
 QTest::mouseClick(source.view->viewport(),Qt::LeftButton,Qt::NoModifier,source.view->visualRect(point).center());QApplication::processEvents();
 const QString formula="='[Selection Source.xlsx]Sheet1'!$F$4";
 require(origin.bar->text()==formula,"the pointed source cell belongs to the original pending formula");
 stableOrigin(origin,target,"source pointed");
 QTest::keyClick(source.bar,Qt::Key_Tab,Qt::ControlModifier);QTest::keyRelease(origin.bar,Qt::Key_Control);QApplication::processEvents();
 require(QApplication::activeWindow()==&origin.window,"return to the origin before accepting the pending formula");
 stableOrigin(origin,target,"origin returned");
 if(inlineEntry&&mouseActivation&&!cancel){
  QTest::keyClick(origin.bar,Qt::Key_F2);QApplication::processEvents();
  require(origin.view->activeCellEditor(),"a returned external formula can resume its native inline editor");
  source.activate();stableOrigin(origin,target,"source reactivated");
  QTest::mouseClick(source.view->viewport(),Qt::LeftButton,Qt::NoModifier,source.view->visualRect(point).center());
  QTest::keyClick(source.bar,Qt::Key_Tab,Qt::ControlModifier);QTest::keyRelease(origin.bar,Qt::Key_Control);QApplication::processEvents();
  stableOrigin(origin,target,"resumed origin returned");
 }
 require(origin.window.currentSession().snapshot()==before&&source.window.currentSession().snapshot()==sourceBefore,"pointing never commits either workbook before acceptance");
 QTest::keyClick(origin.bar,cancel?Qt::Key_Escape:Qt::Key_Return);QApplication::processEvents();
 if(cancel){stableOrigin(origin,target,"cancelled");require(origin.window.currentSession().snapshot()==before,"Escape keeps the original workbook unchanged");}
 else{
  origin.window.recalculateNow();require(origin.model->raw(7,2)==formula&&origin.model->numericValue(7,2)==42&&origin.model->raw(7,3)=="neighbor","Enter commits the actual formula to C8 and leaves the right-hand cell unchanged");
  require(origin.view->currentIndex()==origin.model->index(8,2),"normal Enter moves down only after the external formula is committed");
  require(origin.window.executeCommand("undo")&&origin.window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"the formula commit is one undoable edit");
 }
 require(source.window.currentSession().snapshot()==sourceBefore,"the source workbook's data and history stay unchanged");
}
}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{for(bool inlineEntry:{false,true})for(bool mouse:{false,true})referenceSelection(inlineEntry,mouse,false);referenceSelection(true,true,true);std::cout<<"cross-workbook origin selection tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
