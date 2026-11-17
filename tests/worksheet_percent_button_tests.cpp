#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void keyboardFormatsSelectedPercentages(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 require(model->setData(model->index(0,0),"0.125",Qt::EditRole)&&model->setData(model->index(1,0),"0.5",Qt::EditRole),"seed numeric percentage inputs");grid->setCurrentIndex(model->index(0,0));grid->selectionModel()->select(QItemSelection(model->index(0,0),model->index(1,0)),QItemSelectionModel::ClearAndSelect);
 for(const auto key:{Qt::Key_5,Qt::Key_Percent}){
  const auto before=window.currentSession().snapshot();grid->setFocus();QTest::keyClick(grid,key,Qt::ControlModifier|Qt::ShiftModifier);QApplication::processEvents();
  require(model->cellStyle(0,0).value("numberFormat",std::string())=="0%"&&model->cellStyle(1,0).value("numberFormat",std::string())=="0%","Ctrl+Shift+5 and Ctrl+Shift+% apply whole percentages to the selected range");
  window.recalculateNow();require(model->data(model->index(0,0)).toString()=="13%"&&model->data(model->index(1,0)).toString()=="50%","percentage keyboard formatting rounds display without altering numeric inputs");
  require(model->raw(0,0)=="0.125"&&model->raw(1,0)=="0.5","percentage keyboard formatting preserves the stored inputs");
  QTest::keyClick(grid,Qt::Key_Z,Qt::ControlModifier);QApplication::processEvents();require(window.currentSession().snapshot().at("sheets")==before.at("sheets"),"one Undo restores the complete range's previous formatting");
 }
}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model->setData(model->index(12,2),"0.1",Qt::EditRole),"seed COGS assumption");grid->setCurrentIndex(model->index(12,2));auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");ribbon->setCurrentIndex(0);QToolButton* percent=nullptr;for(auto* button:ribbon->currentWidget()->findChildren<QToolButton*>())if(button->text()=="%")percent=button;require(percent,"Home ribbon provides the percent toolbar action used in the tutorial");QTest::mouseClick(percent,Qt::LeftButton);window.recalculateNow();require(model->data(model->index(12,2)).toString()=="10%"&&model->numericValue(12,2).value_or(-1)==0.1,"percent toolbar shows whole percentages without changing numeric assumptions");keyboardFormatsSelectedPercentages();std::cout<<"percent toolbar and keyboard tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
