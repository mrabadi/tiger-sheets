#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <QFontMetrics>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());model->setData(model->index(1,1),"Keep",Qt::EditRole);grid->setCurrentIndex(model->index(1,1));grid->selectionModel()->select(QItemSelection(model->index(1,1),model->index(2,2)),QItemSelectionModel::ClearAndSelect);const auto before=window.currentSession().snapshot();
 require(window.executeCommand("rows.size",{{"size",12.0}}),"Row Height edits the selected rows");require(grid->rowHeight(1)==qRound(12.0*grid->logicalDpiY()/72.0)&&grid->rowHeight(2)==grid->rowHeight(1),"row dimensions use points and the selected row count");
 require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==before.at("sheets"),"row dimensions are one undoable transaction");require(window.executeCommand("redo"),"row height redo");
 require(window.executeCommand("rows.hide")&&grid->isRowHidden(1)&&grid->isRowHidden(2),"Hide applies to every selected row");require(window.executeCommand("rows.unhide")&&!grid->isRowHidden(1)&&!grid->isRowHidden(2),"Unhide restores the selected rows");
 const QFontMetrics normal(QFont("Calibri",11),grid->viewport());int digit=1;for(char value='0';value<='9';++value)digit=std::max(digit,normal.horizontalAdvance(QChar(value)));
 require(window.executeCommand("columns.size",{{"size",2.0}})&&grid->columnWidth(1)==2*digit+5,"Column Width uses Excel character units");require(window.executeCommand("columns.hide")&&grid->isColumnHidden(1)&&grid->isColumnHidden(2),"Hide applies to selected columns");require(window.executeCommand("columns.unhide")&&!grid->isColumnHidden(1)&&!grid->isColumnHidden(2)&&model->raw(1,1)=="Keep","Unhide preserves cell values");
 const auto width=grid->columnWidth(1);for(int cycle=0;cycle<2;++cycle){const auto path=state.filePath(QString("dimensions-%1.xlsx").arg(cycle));require(window.saveWorkbook(path)&&window.openWorkbook(path),"dimensions save and reopen");grid=window.findChild<QTableView*>("worksheet.grid");require(grid->columnWidth(1)==width,"character-width authoring retains its pixel width across two native save/reopen cycles");}
 std::cout<<"worksheet dimension action tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
