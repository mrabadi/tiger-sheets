#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QPointer>
#include <QTabBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void dropdownWorkflow(){
 MainWindow window;window.resize(1400,850);window.show();QApplication::processEvents();
 auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 require(model->apply(Json::array({{{"type","add_sheet"},{"id","input"},{"name","Input"}},{{"type","set_cell"},{"sheet","input"},{"row",3},{"col",2},{"input","0"},{"style",{{"numberFormat","0%"}}}},{{"type","set_cell"},{"sheet","input"},{"row",4},{"col",2},{"input","10%"},{"style",{{"numberFormat","0%"}}}},{{"type","set_cell"},{"sheet","input"},{"row",5},{"col",2},{"input","15%"},{"style",{{"numberFormat","0%"}}}}})),"seed actual input worksheet");
 const auto target=model->sheetId();grid->setCurrentIndex(model->index(11,2));
 require(window.executeCommand("data.validation"),"Data Validation command opens the native list setup");
 QPointer<QDialog> dialog=window.findChild<QDialog*>("validation.dialog");require(dialog&&dialog->isVisible(),"native data validation dialog is visible");
 auto* allow=dialog->findChild<QComboBox*>("validation.allow");require(allow,"Allow control exists");allow->setCurrentIndex(allow->findText("List"));
 auto* source=dialog->findChild<QLineEdit*>("validation.source");require(source,"Source control exists");source->setText("=Input!$B$3:$B$5");
 auto* buttons=dialog->findChild<QDialogButtonBox*>();QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QApplication::processEvents();
 require(!dialog||!dialog->isVisible(),"valid range source closes the dialog");window.recalculateNow();
 auto* arrow=grid->viewport()->findChild<QToolButton*>("validation.dropdown");require(arrow&&arrow->isVisible(),"active validated cell exposes its dropdown arrow");
 QTest::mouseClick(arrow,Qt::LeftButton);QApplication::processEvents();auto* menu=grid->findChild<QMenu*>("validation.options");require(menu&&menu->isVisible(),"dropdown opens a native list");
 require(menu->actions().size()==3&&menu->actions()[1]->text()=="10%"&&menu->actions()[2]->text()=="15%","list uses percentage labels from the Input range");
 QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions()[1]).center());QApplication::processEvents();
 require(model->sheetId()==target&&model->numericValue(11,2).value_or(-1)==0.1,"choosing 10% writes a typed numeric value into the original assumption cell");
 require(window.executeCommand("undo")&&model->raw(11,2).isEmpty(),"dropdown choice is one undoable edit");
 grid->setCurrentIndex(model->index(11,2));QTest::mouseClick(arrow,Qt::LeftButton);QApplication::processEvents();QPointer<QAction> stale=menu->actions().at(1);const auto beforeStale=window.currentSession().snapshot();grid->setCurrentIndex(model->index(11,3));if(stale)stale->trigger();require(window.currentSession().snapshot()==beforeStale,"an old dropdown cannot write its previous cell after the active cell changes");
}
void canonicalNumericOption(){MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model->apply(Json::array({{{"type","add_sheet"},{"id","input"},{"name","Input"}},{{"type","set_cell"},{"sheet","input"},{"row",3},{"col",2},{"input","1"},{"style",{{"numberFormat","0%"}}}},{{"type","set_style"},{"sheet",model->sheetId().toStdString()},{"row",12},{"col",3},{"style",{{"numberFormat","0%"}}}}})),"seed a one-hundred-percent option");grid->setCurrentIndex(model->index(11,2));require(window.executeCommand("data.validation",{{"source","=Input!$B$3"}}),"create canonical numeric dropdown");auto* arrow=grid->viewport()->findChild<QToolButton*>("validation.dropdown");arrow->click();auto* menu=grid->findChild<QMenu*>("validation.options");require(menu&&menu->actions().front()->text()=="100%","list source exposes one hundred percent");menu->actions().front()->trigger();window.recalculateNow();require(model->numericValue(11,2).value_or(-1)==1&&model->data(model->index(11,2)).toString()=="100%","choosing a numeric dropdown option bypasses manual percentage-entry inference");menu->hide();}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{dropdownWorkflow();canonicalNumericOption();std::cout<<"worksheet validation UI passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
