#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
QMenu* fileMenu(MainWindow& window){for(auto* action:window.menuBar()->actions())if(action->text()=="File")return action->menu();throw std::runtime_error("File menu exists");}
QMenu* recentMenu(MainWindow& window){auto* file=fileMenu(window);const auto actions=file->actions();for(int i=0;i<actions.size();++i)if(actions[i]->text()=="Open…"){require(i+1<actions.size()&&actions[i+1]->text()=="Open Recent"&&actions[i+1]->menu(),"Open Recent is a submenu directly below Open under File");return actions[i+1]->menu();}throw std::runtime_error("Open remains available under File");}
void emptyMenu(){MainWindow window;auto* recent=recentMenu(window);require(recent->actions().size()==1&&!recent->actions().front()->isEnabled()&&recent->actions().front()->text()=="No recent workbooks","an empty recent menu explains that no workbooks are available");require(!window.currentSession().canUndo(),"displaying Open Recent never edits the workbook");}
QString csv(QTemporaryDir& files,const QString& name,const QByteArray& contents){const auto path=files.filePath(name);QFile file(path);require(file.open(QIODevice::WriteOnly)&&file.write(contents)==contents.size(),"create a synthetic recent workbook");return path;}
QStringList recentPaths(MainWindow& window){auto* menu=recentMenu(window);QMetaObject::invokeMethod(menu,"aboutToShow",Qt::DirectConnection);QStringList paths;for(auto* action:menu->actions())if(action->data().isValid())paths<<action->data().toString();return paths;}
void chooseRecent(MainWindow& window,const QString& path){auto* menu=recentMenu(window);menu->popup({100,100});QApplication::processEvents();QAction* chosen=nullptr;for(auto* action:menu->actions())if(action->data().toString()==path)chosen=action;require(chosen&&chosen->isEnabled(),"recent workbook has an enabled native menu entry");require(chosen->toolTip().contains(path),"recent entry exposes the full path in its tooltip");QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(chosen).center());QApplication::processEvents();}
void remembersAndReopens(){
 QTemporaryDir files;const auto first=csv(files,"Budget & forecast.csv","11\n"),second=csv(files,"Other.csv","22\n");QString saved;
 {
  MainWindow window,observer;window.show();require(window.openWorkbook(first)&&window.openWorkbook(second),"open two synthetic workbooks successfully");
  require(recentPaths(window)==QStringList({second,first}),"successful opens appear newest first in Open Recent");
  require(recentPaths(observer)==QStringList({second,first}),"an existing second window refreshes the shared recent list");
  require(window.openWorkbook(files.path()+"/./Budget & forecast.csv")&&recentPaths(window)==QStringList({first,second}),"reopening the same file promotes one normalized entry without duplicates");
  chooseRecent(window,second);auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());
  require(window.windowFilePath()==second&&model->raw(0,0)=="22"&&!window.isWindowModified(),"choosing Open Recent loads the actual workbook into a clean current window");
  require(recentPaths(window)==QStringList({second,first}),"opening through the recent menu promotes the selected workbook");
  saved=files.filePath("Saved.xlsx");require(window.saveWorkbook(saved)&&recentPaths(window).front()==saved,"a successful Save As adds its new workbook path");
 }
 MainWindow restarted;require(recentPaths(restarted)==QStringList({saved,second,first}),"recent workbooks survive destruction and recreation of all windows");
 QStringList expected;for(int i=0;i<12;++i){const auto path=csv(files,QString("Recent%1.csv").arg(i),QByteArray::number(i)+'\n');require(restarted.openWorkbook(path),"open another recent workbook");expected.prepend(path);while(expected.size()>10)expected.removeLast();}
 require(recentPaths(restarted)==expected,"the recent list stays bounded to the ten newest distinct workbooks");
}
void preservesEditsAndHandlesErrors(){
 QTemporaryDir files;const auto first=csv(files,"Current.csv","11\n"),target=csv(files,"Target.csv","22\n"),broken=files.filePath("Broken.xlsx");
 MainWindow window;window.show();require(window.openWorkbook(target)&&window.saveWorkbook(broken)&&window.openWorkbook(first),"prepare successful recent entries before stale-file checks");
 {QFile corrupt(broken);require(corrupt.open(QIODevice::WriteOnly|QIODevice::Truncate)&&corrupt.write("not an Excel workbook")>0,"replace only a synthetic test workbook with corrupt bytes");}
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());require(model->setData(model->index(0,0),"99",Qt::EditRole),"make an unsaved current edit before opening a recent file");const auto before=window.currentSession().snapshot();
 chooseRecent(window,target);MainWindow* other=nullptr;for(auto* widget:QApplication::topLevelWidgets())if(auto* candidate=dynamic_cast<MainWindow*>(widget);candidate&&candidate!=&window&&candidate->windowFilePath()==target)other=candidate;
 require(other&&window.currentSession().snapshot()==before&&window.windowFilePath()==first&&window.isWindowModified()&&window.currentSession().canUndo(),"Open Recent preserves unsaved data, identity and Undo by opening another window");
 auto* opened=dynamic_cast<WorksheetModel*>(other->findChild<QTableView*>("worksheet.grid")->model());require(opened->raw(0,0)=="22","the separate recent window contains the requested file");require(other->close(),"close the clean temporary recent window");QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
 const auto history=recentPaths(window);QString failure;bool warned=false;QTimer dismiss;dismiss.setInterval(0);QObject::connect(&dismiss,&QTimer::timeout,[&]{if(auto* warning=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){dismiss.stop();warned=true;failure=warning->text();warning->accept();}else if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){dismiss.stop();dialog->reject();}});dismiss.start();
 chooseRecent(window,broken);require(warned&&!failure.trimmed().isEmpty()&&window.lastError()==failure,"a failed recent open from a dirty window displays the actual workbook error");
 require(window.currentSession().snapshot()==before&&window.windowFilePath()==first&&window.isWindowModified()&&recentPaths(window)==history,"a corrupt recent file preserves unsaved work and does not promote itself in history");
 require(QFile::remove(target),"remove only a synthetic recent file");recentPaths(window);QAction* missing=nullptr;for(auto* action:recentMenu(window)->actions())if(action->data().toString()==target)missing=action;require(missing&&!missing->isEnabled(),"a deleted recent workbook stays identifiable but cannot be opened");
 require(!window.openWorkbook(files.filePath("Never opened.xlsx"))&&!window.saveWorkbook(files.filePath("missing/Not saved.xlsx"))&&recentPaths(window)==history,"failed opens and saves do not add or reorder recent entries");
 bool cancelled=false;QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){cancelled=true;dialog->reject();}});require(!window.executeCommand("save.as")&&cancelled&&recentPaths(window)==history,"cancelling Save As leaves recent history unchanged");
 const auto exported=files.filePath("Export.csv");require(window.saveWorkbook(exported)&&recentPaths(window)==history&&window.windowFilePath()==first&&window.isWindowModified(),"a CSV-only export does not become the current or most recent workbook");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{const std::string mode=argc>1?argv[1]:"all";for(const auto& test:{std::pair{"empty",emptyMenu},std::pair{"history",remembersAndReopens},std::pair{"safety",preservesEditsAndHandlesErrors}})if(mode=="all"||mode==test.first){QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());test.second();}std::cout<<"recent workbook tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
