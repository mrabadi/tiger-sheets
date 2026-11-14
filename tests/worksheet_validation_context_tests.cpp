#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json workbook(int option){
 auto book=WorkbookSession().snapshot();
 book["sheets"][0]["dataValidations"]=Json::array({{{"type","list"},{"range",{{"row1",3},{"col1",3},{"row2",3},{"col2",3}}},{"formula1","=Input!$B$3"},{"allowBlank",true}}});
 book["sheets"].push_back({{"id","input"},{"name","Input"},{"cells",Json::array({{{"row",3},{"col",2},{"input","="+std::to_string(option)}}})}});
 return book;
}
void reopenedWorkbookDoesNotReuseAnotherWorkbooksOptions(){
 QTemporaryDir directory;require(directory.isValid(),"create isolated workbook fixtures");const auto firstPath=directory.filePath("first.xlsx"),secondPath=directory.filePath("second.xlsx");
 writeWorkbook(firstPath.toStdString(),workbook(1));writeWorkbook(secondPath.toStdString(),workbook(2));
 MainWindow window;window.resize(1100,700);window.show();QApplication::processEvents();
 auto* grid=window.findChild<QTableView*>("worksheet.grid");require(grid!=nullptr,"worksheet grid exists");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model!=nullptr,"worksheet model exists");
 auto* arrow=grid->viewport()->findChild<QToolButton*>("validation.dropdown");auto* menu=grid->findChild<QMenu*>("validation.options");require(arrow&&menu,"validation uses native dropdown controls");
 require(window.openWorkbook(firstPath),"open first workbook");window.recalculateNow();grid->setCurrentIndex(model->index(2,2));require(arrow->isVisible(),"first validated target exposes its dropdown");arrow->click();
 require(menu->actions().size()==1&&menu->actions().front()->isEnabled()&&menu->actions().front()->text()=="1","first workbook supplies its current calculated formula option");menu->hide();
 const auto first=window.currentSession().snapshot();require(window.openWorkbook(secondPath),"open second workbook");const auto before=window.currentSession().snapshot();
 require(before.at("revision")==first.at("revision")&&before.at("sheets")[0].at("id")==first.at("sheets")[0].at("id")&&before.at("sheets")[1].at("id")==first.at("sheets")[1].at("id"),"fixtures reuse revision and worksheet identities across workbook contexts");
 // Keep this synchronous: the pending calculation timer cannot replace the
 // first workbook's cached result before the new menu is constructed.
 grid->setCurrentIndex(model->index(2,2));require(arrow->isVisible(),"new workbook's validated target exposes its dropdown");arrow->click();
 require(menu->actions().size()==1&&!menu->actions().front()->isEnabled()&&menu->actions().front()->text().contains("Calculate",Qt::CaseInsensitive),"a newly opened workbook must request calculation instead of offering a previous workbook's cached option");
 require(window.currentSession().snapshot()==before,"opening the pending-calculation dropdown preserves the new workbook");menu->hide();window.recalculateNow();arrow->click();
 require(menu->actions().size()==1&&menu->actions().front()->isEnabled()&&menu->actions().front()->text()=="2","fresh calculation enables only the second workbook's formula option");menu->hide();
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{reopenedWorkbookDoesNotReuseAnotherWorkbooksOptions();std::cout<<"Worksheet validation context tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
