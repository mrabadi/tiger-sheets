#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void formatCells(){
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 require(model->setData(model->index(0,0),"1200.5",Qt::EditRole),"format fixture");grid->setCurrentIndex(model->index(0,0));grid->selectionModel()->select(QItemSelection(model->index(0,0),model->index(1,1)),QItemSelectionModel::ClearAndSelect);
 const auto before=window.currentSession().snapshot();require(window.executeCommand("cells.format",{{"style",{{"numberFormat","$#,##0.00"},{"bold",true},{"alignment","right"}}}}),"Format Cells applies number and font options to the selected range");
 require(model->raw(0,0)=="1200.5"&&model->cellStyle(1,1).at("bold")==true&&model->cellStyle(0,0).at("numberFormat")=="$#,##0.00","formatting preserves values and formats blank selected cells");
 require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==before.at("sheets"),"Format Cells is one undo transaction");
}
void structuralCommands(){
 const Json cases=Json::array({
  {{"command","rows.insertAbove"},{"cells",Json::array({Json::array({4,1,"selected"}),Json::array({6,1,"after"}),Json::array({1,3,"=A6"})})}},
  {{"command","rows.insertBelow"},{"cells",Json::array({Json::array({2,1,"selected"}),Json::array({4,1,""}),Json::array({6,1,"after"})})}},
  {{"command","columns.insertLeft"},{"cells",Json::array({Json::array({2,3,"selected"}),Json::array({4,3,"after"}),Json::array({1,5,"=C4"})})}},
  {{"command","columns.insertRight"},{"cells",Json::array({Json::array({2,1,"selected"}),Json::array({2,3,""}),Json::array({1,5,"=A4"})})}},
  {{"command","rows.delete"},{"cells",Json::array({Json::array({2,1,"after"}),Json::array({1,3,"=A2"})})}},
  {{"command","columns.delete"},{"cells",Json::array({Json::array({1,1,"=#REF!"}),Json::array({2,1,""})})}}
 });
 for(const auto& entry:cases){MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
  model->setData(model->index(0,0),"before",Qt::EditRole);model->setData(model->index(1,0),"selected",Qt::EditRole);model->setData(model->index(2,0),"second",Qt::EditRole);model->setData(model->index(3,0),"after",Qt::EditRole);model->setData(model->index(0,2),"=A4",Qt::EditRole);model->setData(model->index(1,1),"=A2",Qt::EditRole);
  grid->setCurrentIndex(model->index(1,0));grid->selectionModel()->select(QItemSelection(model->index(1,0),model->index(2,1)),QItemSelectionModel::ClearAndSelect);const auto before=window.currentSession().snapshot();
  if(!window.executeCommand(QString::fromStdString(entry.at("command"))))throw std::runtime_error("context structural command must execute: "+entry.at("command").get<std::string>()+" "+window.lastError().toStdString());
  for(const auto& cell:entry.at("cells"))require(model->raw(cell[0].get<int>()-1,cell[1].get<int>()-1).toStdString()==cell[2].get<std::string>(),"insert/delete honors selected row/column counts, direction and dependent formulas");
  require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==before.at("sheets"),"structural context command is exactly one undo transaction");
 }
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto selected=argc>1?std::string(argv[1]):std::string();if(selected.empty()||selected=="format")formatCells();if(selected.empty()||selected=="structure")structuralCommands();std::cout<<"worksheet action UI tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
