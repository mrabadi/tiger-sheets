#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(int argc,char** argv){QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 MainWindow original;original.show();QApplication::processEvents();const auto originalBook=original.currentSession().snapshot();MainWindow inspected;inspected.show();inspected.activateWindow();auto* grid=inspected.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model->setData(model->index(0,0),"inspection",Qt::EditRole),"seed inspected workbook");const auto path=state.filePath("Inspected.xlsx");require(inspected.saveWorkbook(path),"make the inspected native workbook clean");grid->setFocus();QApplication::processEvents();QTest::keyClick(grid,Qt::Key_W,Qt::ControlModifier);QApplication::processEvents();require(!inspected.isVisible()&&original.isVisible()&&!QApplication::activeModalWidget()&&original.currentSession().snapshot()==originalBook,"Ctrl+W closes only the inspected clean workbook and leaves the existing original window unchanged");
 MainWindow dirty;dirty.show();dirty.activateWindow();auto* cells=dirty.findChild<QTableView*>("worksheet.grid");auto* edits=dynamic_cast<WorksheetModel*>(cells->model());require(edits->setData(edits->index(2,2),"unsaved",Qt::EditRole),"seed an unsaved workbook");const auto before=dirty.currentSession().snapshot();cells->setFocus();QApplication::processEvents();bool cancelled=false;QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){cancelled=dialog->windowTitle()=="Save workbook?";dialog->button(QMessageBox::Cancel)->click();}});QTest::keyClick(cells,Qt::Key_W,Qt::ControlModifier);QApplication::processEvents();require(cancelled&&dirty.isVisible()&&dirty.currentSession().snapshot()==before,"Ctrl+W uses the existing save-changes prompt and Cancel preserves data and history");dirty.activateWindow();cells->setFocus();QApplication::processEvents();bool discarded=false;QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){discarded=true;dialog->button(QMessageBox::Discard)->click();}});QTest::keyClick(cells,Qt::Key_W,Qt::ControlModifier);QApplication::processEvents();require(discarded&&!dirty.isVisible()&&original.isVisible()&&original.currentSession().snapshot()==originalBook,"Discard closes only the unsaved workbook through Ctrl+W without altering another workbook");std::cout<<"Native close shortcut passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
