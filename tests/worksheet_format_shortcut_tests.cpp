#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QDialog>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
 QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());
 try{
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
