#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct OtherWindows {
 MainWindow& original;
 ~OtherWindows(){QList<QPointer<MainWindow>> windows;for(auto* widget:QApplication::topLevelWidgets())if(auto* window=dynamic_cast<MainWindow*>(widget);window&&window!=&original)windows.push_back(window);for(auto window:windows)if(window)delete window;}
};
void fileNewOpensAnIndependentForegroundWorkbook(){
 MainWindow original;OtherWindows cleanup{original};original.resize(1280,720);original.show();original.activateWindow();
 auto* originalGrid=original.findChild<QTableView*>("worksheet.grid");auto* originalModel=dynamic_cast<WorksheetModel*>(originalGrid->model());
 require(originalModel->setData(originalModel->index(3,2),"Keep this unsaved workbook",Qt::EditRole),"prepare a genuine unsaved original workbook");
 originalGrid->setCurrentIndex(originalModel->index(3,2));originalGrid->selectionModel()->select(QItemSelection(originalModel->index(3,2),originalModel->index(4,3)),QItemSelectionModel::ClearAndSelect);originalGrid->setFocus();QApplication::processEvents();
 const auto before=original.currentSession().snapshot();const auto selected=originalGrid->selectionModel()->selection();const auto current=originalGrid->currentIndex();
 QAction* fileAction=nullptr;for(auto* action:original.menuBar()->actions())if(action->text()=="File")fileAction=action;require(fileAction&&fileAction->menu(),"File is a native menu");
 QTest::mouseClick(original.menuBar(),Qt::LeftButton,Qt::NoModifier,original.menuBar()->actionGeometry(fileAction).center());QApplication::processEvents();auto* menu=fileAction->menu();require(menu->isVisible(),"a genuine File menu click opens the native menu");
 QAction* create=nullptr;for(auto* action:menu->actions())if(action->text()=="New")create=action;require(create&&create->isEnabled(),"File offers an enabled New command");
 QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(create).center());QApplication::processEvents();
 QPointer<MainWindow> opened;int count=0;for(auto* widget:QApplication::topLevelWidgets())if(auto* candidate=dynamic_cast<MainWindow*>(widget);candidate&&candidate!=&original){opened=candidate;++count;}
 require(count==1&&opened&&opened->isVisible()&&!opened->isMinimized(),"File New creates exactly one visible independent workbook window");
 auto* grid=opened->findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 std::cout<<"File New active="<<(QApplication::activeWindow()==opened)<<" focus="<<(QApplication::focusWidget()?QApplication::focusWidget()->objectName().toStdString():"<none>")<<" gridFocus="<<grid->hasFocus()<<'\n';
 require(QTest::qWaitForWindowActive(opened,1000),"File New activates the newly opened workbook instead of leaving it behind the original");
 require(grid&&model&&grid->hasFocus(),"File New puts keyboard focus in the new worksheet, ready for typing");
 require(opened->windowFilePath().isEmpty()&&!opened->currentSession().canUndo()&&opened->currentSession().snapshotView().at("sheets")[0].at("cells").empty(),"File New starts a separate blank unsaved workbook without history");
 require(original.currentSession().snapshot()==before&&originalGrid->selectionModel()->selection()==selected&&originalGrid->currentIndex()==current&&original.isWindowModified()&&original.currentSession().canUndo(),"File New preserves the original workbook's data, selection, current cell and Undo");
 QTest::keyClick(grid,Qt::Key_4);QApplication::processEvents();auto* editor=QApplication::focusWidget();require(editor&&grid->isAncestorOf(editor),"typing starts the new worksheet's real inline editor");QTest::keyClick(editor,Qt::Key_2);QTest::keyClick(editor,Qt::Key_Return);QApplication::processEvents();
 require(model->raw(0,0)=="42"&&original.currentSession().snapshot()==before,"typing after File New edits only the newly opened worksheet");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());std::cout<<std::unitbuf;try{fileNewOpensAnIndependentForegroundWorkbook();std::cout<<"File New native workflow passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
