#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 MainWindow window;window.show();app.processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 require(model->setData(model->index(0,0),"10",Qt::EditRole)&&model->setData(model->index(0,1),"=A1*2",Qt::EditRole),"clipboard UI setup");grid->setCurrentIndex(model->index(0,1));const auto before=window.currentSession().snapshot();
 require(window.executeCommand("cut")&&window.currentSession().snapshot()==before,"native Cut command defers mutation");grid->setFocus();QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(grid,&escape);
 require(QApplication::clipboard()->text().isEmpty(),"Escape on the worksheet cancels and clears the pending cut clipboard");require(window.currentSession().snapshot()==before,"cancelling Cut leaves source and undo history untouched");
 require(window.executeCommand("cut"),"cut again after cancel");grid->setCurrentIndex(model->index(1,1));require(window.executeCommand("paste"),"native Paste moves cut source");require(model->raw(0,1).isEmpty()&&model->raw(1,1)=="=A1*2","native Cut/Paste uses move reference semantics");require(QApplication::clipboard()->text().isEmpty(),"successful move consumes the cut clipboard");require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==before.at("sheets"),"native move undo restores the whole source/destination change");
 std::cout<<"worksheet clipboard UI passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
