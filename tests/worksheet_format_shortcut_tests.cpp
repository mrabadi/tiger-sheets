#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void wholeDimensionSelectionShortcuts(){
 for(bool rows:{false,true}){MainWindow window;window.show();window.activateWindow();QApplication::processEvents();auto* grid=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());const auto active=model->index(7,6);require(model->setData(active,"6428",Qt::EditRole)&&model->setData(model->index(rows?10:0,rows?0:9),"neighbor",Qt::EditRole),"seed whole-dimension shortcut fixture");grid->setCurrentIndex(active);grid->setFocus();const auto before=window.currentSession().snapshot();QTest::keyClick(grid,Qt::Key_Space,rows?Qt::ShiftModifier:Qt::ControlModifier);QTest::keyRelease(grid,rows?Qt::Key_Shift:Qt::Key_Control);QApplication::processEvents();auto selection=grid->selectionModel()->selection();require(selection.size()==1&&(rows?selection.front().width()==model->columnCount()&&selection.front().top()==7&&selection.front().height()==1:selection.front().height()==model->rowCount()&&selection.front().left()==6&&selection.front().width()==1),"ShiftSpace selects the entire active row and CtrlSpace the entire active column");for(int count=0;count<2;++count)QTest::keyClick(grid,rows?Qt::Key_Down:Qt::Key_Right,Qt::ShiftModifier);QTest::keyRelease(grid,Qt::Key_Shift);selection=grid->selectionModel()->selection();require(selection.size()==1&&(rows?selection.front().height()==3&&selection.front().width()==model->columnCount():selection.front().width()==3&&selection.front().height()==model->rowCount())&&grid->currentIndex()==active,"ShiftArrows extend whole-row/column selections while retaining the original active cell");require(!grid->activeCellEditor()&&model->raw(7,6)=="6428"&&window.currentSession().snapshot()==before,"dimension selection shortcuts never insert spaces or edit the source workbook");QTest::keyClick(grid,Qt::Key_Minus,Qt::ControlModifier);QTest::keyRelease(grid,Qt::Key_Control);require(model->raw(rows?7:0,rows?0:6)=="neighbor","CtrlMinus deletes the whole dimension range selected by the real keyboard shortcuts");require(window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"one Undo restores the deleted source dimensions and values");}
}
void structuralShortcuts(){
 for(bool rows:{true,false}){
  MainWindow window;window.show();window.activateWindow();QApplication::processEvents();auto* grid=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model->setData(model->index(1,1),"42",Qt::EditRole)&&model->setData(model->index(4,3),"=B2+1",Qt::EditRole),"seed structural keyboard fixture");grid->setCurrentIndex(model->index(1,1));const int bottom=rows?2:model->rowCount()-1,right=rows?model->columnCount()-1:2;grid->selectionModel()->select(QItemSelection(model->index(rows?1:0,rows?0:1),model->index(bottom,right)),QItemSelectionModel::ClearAndSelect);grid->setFocus();const auto before=window.currentSession().snapshot();QTest::keyClick(grid,Qt::Key_Plus,Qt::ControlModifier);QApplication::processEvents();require(model->raw(rows?3:1,rows?1:3)=="42"&&model->raw(rows?6:4,rows?3:5)==(rows?"=B4+1":"=D2+1"),"CtrlPlus inserts all selected whole rows or columns and updates formulas");const auto selection=grid->selectionModel()->selection();require(selection.size()==1&&selection.front().height()==(rows?2:model->rowCount())&&selection.front().width()==(rows?model->columnCount():2)&&grid->currentIndex()==model->index(1,1),"structural shortcuts retain the active cell and complete row/column selection");QTest::keyClick(grid,Qt::Key_Minus,Qt::ControlModifier);QApplication::processEvents();auto restored=window.currentSession().snapshotView().at("sheets");for(auto& sheet:restored)sheet.erase("structuralRevision");require(restored==before.at("sheets"),"CtrlMinus deletes the selected inserted rows or columns and restores all inputs/formulas");require(window.executeCommand("undo")&&window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"each structural shortcut creates exactly one undoable edit");grid->setCurrentIndex(model->index(1,1));grid->setFocus();QTest::keyClick(grid,Qt::Key_F2);const auto editing=window.currentSession().snapshot();QTest::keyClick(grid->activeCellEditor(),Qt::Key_Plus,Qt::ControlModifier);require(window.currentSession().snapshot()==editing,"CtrlPlus inside a cell editor never inserts worksheet rows");grid->cancelEntry();
 }
}
int main(int argc,char** argv){
 QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());
 try{
  if(argc>1&&std::string(argv[1])=="whole_dimensions"){wholeDimensionSelectionShortcuts();return 0;}
  wholeDimensionSelectionShortcuts();
  if(argc>1&&std::string(argv[1])=="structural"){structuralShortcuts();std::cout<<"structural keyboard shortcuts passed\n";return 0;}
  structuralShortcuts();
  MainWindow window;window.show();window.activateWindow();QApplication::processEvents();
  auto* grid=window.findChild<QTableView*>("worksheet.grid");require(grid,"worksheet grid is available");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model&&model->setData(model->index(0,0),"1200.5",Qt::EditRole),"shortcut fixture is editable");
  grid->setCurrentIndex(model->index(0,0));grid->setFocus();QApplication::processEvents();require(grid->hasFocus(),"the worksheet owns keyboard focus before the shortcut");
  const auto before=window.currentSession().snapshot();bool opened=false;QTimer rejectDialog;rejectDialog.setSingleShot(true);
  QObject::connect(&rejectDialog,&QTimer::timeout,&window,[&]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){opened=dialog->objectName()=="formatCells.dialog"&&dialog->windowTitle()=="Format Cells";dialog->reject();}});
  rejectDialog.start(0);QTest::keyClick(grid,Qt::Key_1,Qt::ControlModifier);QApplication::processEvents();
  require(opened,"Ctrl+1 opens the native Format Cells dialog from the focused worksheet");require(window.currentSession().snapshot()==before,"cancelling keyboard-invoked Format Cells leaves the workbook and history unchanged");
  std::cout<<"worksheet Format Cells shortcut tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
