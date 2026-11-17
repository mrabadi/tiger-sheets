#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/workbook_links.hpp"
#include "tigersheets/calculation.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void liveSourceEditsInvalidateOtherWindows(){
 MainWindow source,model;source.show();model.show();QApplication::processEvents();
 const auto path=QString::fromUtf8(TIGERSHEETS_SOURCE_DIR)+"/tests/fixtures/biff/independent.xls";require(source.openWorkbook(path),"Open genuine BIFF source in its own native window");
 auto* sourceGrid=dynamic_cast<WorksheetView*>(source.findChild<QTableView*>("worksheet.grid"));auto* sourceModel=dynamic_cast<WorksheetModel*>(sourceGrid->model());auto* modelGrid=model.findChild<QTableView*>("worksheet.grid");auto* modelModel=dynamic_cast<WorksheetModel*>(modelGrid->model());
 const auto id=modelModel->sheetId().toStdString();require(modelModel->apply(Json::array({{{"type","set_metadata"},{"key","links"},{"value",Json::array({{{"name","independent.xls"},{"path",path.toStdString()},{"enabled",true}}})}},{{"type","set_cell"},{"sheet",id},{"row",1},{"col",1},{"input","='[independent.xls]Inputs 2026'!A2*2"}}})),"Create an external formula linked to the open source");
 model.recalculateNow();require(modelModel->data(modelModel->index(0,0)).toString()=="2469","Native model window calculates from its open source without refresh");
 require(source.property("workbook.path").toString()==path,"External pointing can discover the source's actual file path");
 source.activateWindow();sourceGrid->setCurrentIndex(sourceModel->index(1,0));sourceGrid->setFocus();QTest::keyClick(sourceGrid,Qt::Key_F2);QApplication::processEvents();auto* editor=sourceGrid->activeCellEditor();require(editor,"Edit source cell with the actual F2 editor");editor->selectAll();QTest::keyClicks(editor,"1800");QTest::keyClick(editor,Qt::Key_Return);QApplication::processEvents();
 QElapsedTimer timer;timer.start();while(modelModel->data(modelModel->index(0,0)).toString()!="3600"&&timer.elapsed()<5000){QApplication::processEvents();QTest::qWait(10);}
 require(modelModel->data(modelModel->index(0,0)).toString()=="3600","Unsaved source edit asynchronously invalidates and recalculates dependent windows");
 require(source.currentSession().cellAt(sourceModel->sheetId().toStdString(),2,1)->at("input")=="1800","Source edit remains unsaved and canonical");
 QTemporaryDir saved;const auto savedPath=saved.filePath("linked-model.xlsx");require(model.saveWorkbook(savedPath),"Save model with its live unsaved external source");const auto reopened=readWorkbook(savedPath.toStdString());const auto prepared=prepareWorkbookLinks(reopened.workbook,&reopened);const auto recalculated=CalculationEngine().calculate(prepared.snapshot);bool nativeCacheFresh=false;for(const auto& value:recalculated.at("values"))if(value.at("sheet")==id&&value.at("row")==1&&value.at("col")==1)nativeCacheFresh=value.at("type")=="number"&&value.at("value")==3600;
 require(prepared.filesRead==0&&nativeCacheFresh,"Saving preserves the unsaved source's current values in native externalBook caches without reading disk");
 require(source.openWorkbook(path),"Reopen the original source at the same file path");timer.restart();while(modelModel->data(modelModel->index(0,0)).toString()!="2469"&&timer.elapsed()<5000){QApplication::processEvents();QTest::qWait(10);}
 require(modelModel->data(modelModel->index(0,0)).toString()=="2469","Reloading the same source path invalidates dependent windows even when its identity is unchanged");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{liveSourceEditsInvalidateOtherWindows();std::cout<<"Live workbook native UI integration passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
