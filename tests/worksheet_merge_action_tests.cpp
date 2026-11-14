#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 model->setData(model->index(0,0),"Revenue",Qt::EditRole);model->setData(model->index(0,1),"Other value",Qt::EditRole);model->setData(model->index(1,1),"99",Qt::EditRole);grid->setCurrentIndex(model->index(0,0));grid->selectionModel()->select(QItemSelection(model->index(0,0),model->index(1,1)),QItemSelectionModel::ClearAndSelect);const auto before=window.currentSession().snapshot();
 require(window.executeCommand("cells.mergeCenter",{{"discardOtherValues",true}}),"Merge and Center executes on the selected rectangle");
 require(model->raw(0,0)=="Revenue"&&model->raw(0,1).isEmpty()&&model->raw(1,1).isEmpty()&&model->cellStyle(0,0).at("alignment")=="center","merge preserves only the upper-left value after explicit confirmation");
 require(grid->rowSpan(0,0)==2&&grid->columnSpan(0,0)==2,"merge is visible immediately");
 require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==before.at("sheets"),"one Undo restores all values, formatting and merge state");
 require(window.executeCommand("redo"),"merge redo");grid->setCurrentIndex(model->index(0,0));require(window.executeCommand("cells.unmerge")&&grid->rowSpan(0,0)==1&&grid->columnSpan(0,0)==1&&model->raw(0,0)=="Revenue","Unmerge restores ordinary cells and retains the merged value");
 model->setData(model->index(0,1),"Retain me",Qt::EditRole);grid->setCurrentIndex(model->index(0,0));grid->selectionModel()->select(QItemSelection(model->index(0,0),model->index(0,1)),QItemSelectionModel::ClearAndSelect);const auto cancelled=window.currentSession().snapshot();bool warning=false;
 QTimer::singleShot(0,[&]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){warning=box->text().contains("upper-left");box->done(QMessageBox::No);}});
 require(!window.executeCommand("cells.merge")&&warning&&window.currentSession().snapshot()==cancelled,"declining the native merge warning preserves all values and undo state");
 require(window.executeCommand("cells.merge",{{"discardOtherValues",true}}),"merge cells without centering");grid->setCurrentIndex(model->index(0,1));grid->selectionModel()->select(QItemSelection(model->index(0,1),model->index(0,2)),QItemSelectionModel::ClearAndSelect);const auto partial=window.currentSession().snapshot();
 require(!window.executeCommand("cells.merge",{{"discardOtherValues",true}})&&window.currentSession().snapshot()==partial&&window.lastError().contains("whole existing merged cell"),"partially intersecting merged ranges are refused before losing values");
 std::cout<<"worksheet merge action tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
