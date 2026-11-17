#include "tigersheets/main_window.hpp"
#include <QApplication>
#include <QDialog>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int open(MainWindow& window,int next,bool accept=false){int initial=-1;window.activateWindow();auto* grid=window.findChild<QTableView*>("worksheet.grid");grid->setFocus();QApplication::processEvents();QTimer::singleShot(0,&window,[&]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){if(auto* tabs=dialog->findChild<QTabWidget*>("formatCells.tabs")){initial=tabs->currentIndex();tabs->setCurrentIndex(next);}if(accept)dialog->accept();else dialog->reject();}});QTest::keyClick(grid,Qt::Key_1,Qt::ControlModifier);return initial;}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 MainWindow first;first.show();QApplication::processEvents();const auto original=first.currentSession().snapshot();require(open(first,1)==0,"the first native Format Cells dialog starts on Number");require(open(first,3)==1,"Ctrl+1 reopens the last selected Alignment tab after Cancel");MainWindow second;second.show();QApplication::processEvents();const auto other=second.currentSession().snapshot();require(open(second,4)==0,"another workbook window starts with its own Number tab preference");require(open(first,2,true)==3,"the first window retains Border independently of another window's Fill tab");require(open(first,1)==2&&open(second,0)==4,"accepted and cancelled Format Cells dialogs both preserve each window's last selected tab");require(first.currentSession().snapshot()==original&&second.currentSession().snapshot()==other,"remembering a dialog tab creates no formatting edits or undo history");std::cout<<"Per-window Format Cells tab memory passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
