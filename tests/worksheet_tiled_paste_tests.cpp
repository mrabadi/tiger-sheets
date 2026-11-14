#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* name=window.findChild<QLineEdit*>("formula.name");
 require(model->setData(model->index(2,1),"0%",Qt::EditRole),"seed percentage to copy");auto select=[&](const char* address){name->setFocus();name->selectAll();QTest::keyClicks(name,address);QTest::keyClick(name,Qt::Key_Return);QApplication::processEvents();};select("B3");QTest::keyClick(grid,Qt::Key_C,Qt::ControlModifier);const auto before=window.currentSession().snapshot();select("B4:B7");QTest::keyClick(grid,Qt::Key_V,Qt::ControlModifier);QApplication::processEvents();for(int row=3;row<=6;++row)require(model->raw(row,1)=="0"&&model->cellStyle(row,1).at("numberFormat")=="0%","paste repeats the copied percentage into every selected destination cell");
 require(window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"one undo restores the entire repeated paste");for(int row=0;row<2;++row)for(int col=0;col<2;++col)require(model->setData(model->index(row,col),QString::number(row*2+col+1),Qt::EditRole),"seed rectangular source");const auto rectangle=model->copyRange(0,0,1,1);const auto boundary=window.currentSession().snapshot();require(!model->pasteRange(3,1,rectangle,"all",false,4,1)&&window.currentSession().snapshot()==boundary,"repeated paste cannot expand outside the selected destination width");std::cout<<"tiled worksheet paste tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
