#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/snapshot_capture.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QTabBar>
#include <QStatusBar>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QInputDialog>
#include <QTimer>
#include <QToolButton>
#include <QMenuBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QEvent>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void fileOnIdentityRow(){
 MainWindow window;window.show();QApplication::processEvents();
 auto* brand=window.findChild<QLabel*>("workspace.brand");auto* menus=window.menuBar();
 require(brand&&menus&&!menus->actions().isEmpty(),"workspace identity and File menu exist");
 const auto file=menus->actionGeometry(menus->actions().front());
 const auto logoCenter=brand->mapTo(&window,brand->rect().center());const auto menuCenter=menus->mapTo(&window,file.center());
 require(std::abs(logoCenter.y()-menuCenter.y())<=4,"File menu must share the logo and application-name row");
}
void compactWorkspace(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");require(grid!=nullptr,"worksheet exists");
 require(window.executeCommand("view.ribbon"),"collapse the source ribbon before checking compact workspace geometry");QApplication::processEvents();require(grid->mapTo(&window,QPoint(0,0)).y()==110&&grid->viewport()->mapTo(&window,QPoint(0,0)).y()==130,"collapsed chrome places worksheet headers at110 and cells at130 without a duplicate formatting row");
}
void liveCellFormula(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");grid->setFocus();grid->setCurrentIndex(grid->model()->index(0,0));QTest::keyClicks(grid,"=");QApplication::processEvents();auto* editor=grid->findChild<QLineEdit*>();require(editor!=nullptr,"typing opens the native cell editor");QTest::keyClicks(editor,"SUM(");QApplication::processEvents();
 require(formula->text()=="=SUM(","in-cell formula typing must update the formula bar immediately");
 require(window.currentSession().snapshot()["sheets"][0]["cells"].empty(),"typing must not commit a cell before acceptance");
}
void cancelCellEntry(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");grid->setFocus();QTest::keyClicks(grid,"=");QApplication::processEvents();auto* editor=grid->findChild<QLineEdit*>();require(editor!=nullptr,"cell editor opens");QTest::keyClicks(editor,"10+20");QTest::keyClick(editor,Qt::Key_Escape);QApplication::processEvents();
 require(formula->text().isEmpty()&&window.currentSession().snapshot()["sheets"][0]["cells"].empty(),"Escape must cancel in-cell entry and restore the formula bar");
}
void entryNavigation(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");formula->setFocus();QTest::keyClicks(formula,"=10+20");QTest::keyClick(formula,Qt::Key_Return);QApplication::processEvents();
 require(grid->currentIndex()==grid->model()->index(1,0)&&grid->model()->data(grid->model()->index(0,0),Qt::EditRole).toString()=="=10+20","Enter must accept formula-bar entry and move down one row");
 grid->setFocus();QTest::keyClicks(grid,"4");QApplication::processEvents();auto* editor=grid->findChild<QLineEdit*>();require(editor!=nullptr,"cell editor opens");QTest::keyClick(editor,Qt::Key_Tab);QApplication::processEvents();
 require(grid->currentIndex()==grid->model()->index(1,1)&&grid->model()->data(grid->model()->index(1,0),Qt::EditRole).toString()=="4","Tab must accept cell entry and move right");
}
void undoRestoresEditedSelection(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 auto* grid=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 require(model->setData(model->index(109,5),"=F109+F114"),"prepare the actual F110 formula before a fresh mistaken entry");
 const auto original=window.currentSession().snapshot().at("sheets");const auto first=model->index(109,5),last=model->index(109,7);
 grid->setCurrentIndex(first);grid->selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);grid->scrollTo(first);grid->setFocus();QApplication::processEvents();
 auto replace=[&](const char* text){QTest::keyClicks(grid,text);QApplication::processEvents();auto* editor=grid->activeCellEditor();require(editor,"fresh entry uses the actual native cell editor");QTest::keyClick(editor,Qt::Key_Return);QApplication::processEvents();};
 auto restored=[&]{const auto ranges=grid->selectionModel()->selection();return grid->currentIndex()==first&&ranges.size()==1&&ranges.front().topLeft()==first&&ranges.front().bottomRight()==last;};
 replace("r");require(model->raw(109,5)=="r"&&grid->currentIndex()==model->index(110,5),"mistaken literal r commits and Enter moves to F111");
 QTest::keyClick(grid,Qt::Key_Z,Qt::ControlModifier);QApplication::processEvents();
 require(window.currentSession().snapshot().at("sheets")==original,"one actual Ctrl+Z restores the prior formula and all cell metadata");
 require(restored(),"Ctrl+Z must restore the edited F110 origin and its prior F110:H110 selection");
 QTest::keyClick(grid,Qt::Key_Y,Qt::ControlModifier);QApplication::processEvents();require(model->raw(109,5)=="r"&&restored(),"actual Redo restores the literal with the corresponding edit selection");
 require(window.executeCommand("undo")&&restored(),"return to the original formula before checking a branched edit");
 const auto history=window.currentSession().historyBytes();QTest::keyClick(grid,Qt::Key_F2);QApplication::processEvents();auto* editor=grid->activeCellEditor();require(editor,"open the restored formula for the no-op control");QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);QApplication::processEvents();require(window.currentSession().historyBytes()==history,"no-op formula acceptance creates no new history entry");
 grid->setCurrentIndex(first);grid->selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);grid->setFocus();replace("7");require(!window.currentSession().canRedo(),"a new edit retires the prior redo branch");
 require(window.executeCommand("undo")&&window.currentSession().snapshot().at("sheets")==original&&restored(),"branched-history Undo restores the correct formula and selection without stale view context");
 auto fresh=window.currentSession().snapshot();fresh["revision"]=0;fresh["sheets"][0]["cells"]=Json::array();window.currentSession()=WorkbookSession(fresh);model->refresh();
 const auto other=model->index(3,2);grid->setCurrentIndex(other);grid->selectionModel()->select(other,QItemSelectionModel::ClearAndSelect);grid->setFocus();replace("9");require(window.executeCommand("undo")&&grid->currentIndex()==other&&model->raw(3,2).isEmpty(),"replacing the session cannot reuse a stale edit selection from another history");
}
void undoSelectionPruningPreservesLiveHistory(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 auto* grid=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
 const auto older=model->index(3,2),branch=model->index(0,0);
 grid->setCurrentIndex(older);grid->selectionModel()->select(older,QItemSelectionModel::ClearAndSelect);grid->setFocus();
 QTest::keyClicks(grid,"4");QApplication::processEvents();auto* editor=grid->activeCellEditor();require(editor,"the older live edit starts in the actual native editor");QTest::keyClick(editor,Qt::Key_Return);QApplication::processEvents();
 require(model->raw(3,2)=="4","the older edit is committed before any branch churn");
 grid->setCurrentIndex(branch);grid->selectionModel()->select(branch,QItemSelectionModel::ClearAndSelect);grid->setFocus();
 for(int i=0;i<1030;++i){
  require(model->setData(branch,QString::number(i+1)),"each newer branch is an actual changed model transaction");
  require(window.executeCommand("undo")&&model->raw(0,0).isEmpty(),"each actual native Undo retires only the newer branch");
 }
 require(grid->currentIndex()==branch&&model->raw(3,2)=="4","branch churn leaves the old edit live while the current selection is elsewhere");
 QTest::keyClick(grid,Qt::Key_Z,Qt::ControlModifier);QApplication::processEvents();
 require(model->raw(3,2).isEmpty()&&model->raw(0,0).isEmpty(),"ordinary content Undo remains correct after more than1024 retired branches");
 require(grid->currentIndex()==older,"Undo must retain the C4 selection for an older live history entry after more than1024 retired branches");
 require(window.executeCommand("redo")&&model->raw(3,2)=="4"&&grid->currentIndex()==older,"Redo also retains the reachable older edit context after pruning");
 std::cout<<"live history selection survived1030 retired branches\n";
}
void pointRangeWhileTyping(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");for(int row=0;row<3;++row)grid->model()->setData(grid->model()->index(row,1),QString::number((row+1)*10),Qt::EditRole);window.recalculateNow();grid->setCurrentIndex(grid->model()->index(0,3));const auto before=window.currentSession().snapshot();
 formula->setFocus();QTest::keyClicks(formula,"=SUM(");const auto start=grid->visualRect(grid->model()->index(0,1)).center(),end=grid->visualRect(grid->model()->index(2,1)).center();QTest::mousePress(grid->viewport(),Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(grid->viewport(),end);QTest::mouseRelease(grid->viewport(),Qt::LeftButton,Qt::NoModifier,end);QApplication::processEvents();
 if(formula->text()!="=SUM(B1:B3"||window.currentSession().snapshot()!=before)std::cerr<<"point diagnostic text="<<formula->text().toStdString()<<" current="<<grid->currentIndex().row()<<","<<grid->currentIndex().column()<<" revision="<<window.currentSession().snapshot()["revision"]<<" before="<<before["revision"]<<"\n";
 require(formula->text()=="=SUM(B1:B3"&&grid->currentIndex()==grid->model()->index(0,3)&&window.currentSession().snapshot()==before,"dragging worksheet cells must insert a range while leaving the formula uncommitted");
 QTest::keyClicks(formula,")");QTest::keyClick(formula,Qt::Key_Return);window.recalculateNow();require(grid->model()->data(grid->model()->index(0,3)).toString()=="60","the accepted pointed formula must calculate normally");
}
void referenceOutline(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");grid->setCurrentIndex(grid->model()->index(0,3));formula->setFocus();QTest::keyClicks(formula,"=SUM(B1:B3)");QApplication::processEvents();const auto image=grid->viewport()->grab().toImage();const auto first=grid->visualRect(grid->model()->index(0,1)),last=grid->visualRect(grid->model()->index(2,1));int colored=0;for(int y=first.top()+2;y<last.bottom()-2;++y)for(int x=first.left();x<=first.left()+2;++x){const auto c=image.pixelColor(x,y);if(c.blue()>c.red()+40&&c.blue()>c.green()+20)++colored;}
 require(colored>20,"formula references must have a visible colored range outline in the worksheet");
}
void f4PreservesEntry(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");formula->setFocus();QTest::keyClicks(formula,"=B1");QTest::keyClick(formula,Qt::Key_F4);QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualRect(grid->model()->index(0,2)).center());QApplication::processEvents();
 require(grid->model()->data(grid->model()->index(0,0),Qt::EditRole).toString()=="=$B$1","F4 reference cycling must preserve pending formula edits when focus changes");
}
void narrowRibbon(){MainWindow window;window.resize(820,650);window.show();QApplication::processEvents();int found=0;for(auto* button:window.findChildren<QToolButton*>())if(button->text()=="Fill"||button->text()=="Center"||button->text()=="Right"){++found;require(!button->icon().isNull()&&button->width()>=24,"narrow source ribbon controls retain visible graphical icons and usable24pixel targets");}require(found==3,"Home formatting controls remain available");}
void functionCompletion(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");formula->setFocus();QTest::keyClicks(formula,"=AVG");QApplication::processEvents();auto* popup=formula->findChild<QListWidget*>("formula.completions");require(popup&&popup->isVisible()&&popup->count()>0,"typing a function prefix must show available canonical worksheet functions");QTest::keyClick(formula,Qt::Key_Tab);require(formula->text()=="=AVERAGE("&&grid->currentIndex()==grid->model()->index(0,0)&&window.currentSession().snapshot()["sheets"][0]["cells"].empty(),"Tab must insert AVERAGE and its opening parenthesis without committing the formula");QTest::keyClicks(formula,"10,20)");QTest::keyClick(formula,Qt::Key_Return);window.recalculateNow();require(grid->model()->data(grid->model()->index(0,0)).toString()=="15","a completed formula must evaluate and commit through the ordinary editor");
}
void sheetSwitchReplacesPendingCalculationQuietly(){
 MainWindow window;window.show();QApplication::processEvents();require(window.executeCommand("sheet.add"),"create a second sheet for read-only navigation");auto* tabs=window.findChild<QTabBar*>("workspace.sheets");auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto book=window.currentSession().snapshot();for(int row=1;row<=1500;++row)book["sheets"][0]["cells"].push_back({{"row",row},{"col",1},{"input",std::to_string(row)}});window.currentSession()=WorkbookSession(book);model->refresh();
 QTimer* delay=nullptr;for(auto* timer:window.findChildren<QTimer*>())if(timer->isSingleShot()&&timer->interval()==35)delay=timer;SnapshotCapture* capture=nullptr;for(auto* child:window.children())if(auto* candidate=dynamic_cast<SnapshotCapture*>(child))capture=candidate;require(delay&&capture,"real background calculation timer and sliced capture are available");tabs->setCurrentIndex(0);delay->stop();capture->stop();window.statusBar()->clearMessage();const auto before=window.currentSession().snapshot();const bool undo=window.currentSession().canUndo(),redo=window.currentSession().canRedo();
 require(QMetaObject::invokeMethod(delay,"timeout",Qt::DirectConnection)&&capture->running(),"begin actual sliced native calculation before switching sheets");tabs->setCurrentIndex(1);require(QMetaObject::invokeMethod(delay,"timeout",Qt::DirectConnection),"dispatch the replacement calculation after a real sheet switch");delay->stop();require(QTest::qWaitFor([&]{return !capture->running();},3000),"replacement calculation finishes capturing inputs");
 require(!window.statusBar()->currentMessage().contains("changed while preparing the report"),"read-only sheet switching silently replaces stale background calculation without a report error banner");require(window.currentSession().snapshot()==before&&window.currentSession().canUndo()==undo&&window.currentSession().canRedo()==redo,"background calculation replacement leaves workbook values and Undo/Redo unchanged");
}
void insertFunctionButton(){MainWindow window;window.show();QApplication::processEvents();auto* button=window.findChild<QToolButton*>("formula.insertFunction");require(button,"the formula bar fx control must be an actionable Insert Function button");bool opened=false;QTimer::singleShot(0,&window,[&]{if(auto* dialog=window.findChild<QInputDialog*>()){opened=true;dialog->setTextValue("SUM");dialog->accept();}});QTest::mouseClick(button,Qt::LeftButton);auto* formula=window.findChild<QLineEdit*>("formula.input");require(opened&&formula->text()=="=SUM()"&&formula->isModified(),"Insert Function must place an editable uncommitted function draft in the formula bar");}
void savingProgressFollowsActualFileWrite(){
 for(const auto& kind:QStringList{"xlsx","xls","csv","failure"}){
  QTemporaryDir files;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
  auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());
  require(model->setData(model->index(0,0),"1"),"prepare a real workbook before the save-progress lifecycle");
  const auto identity=files.filePath(kind=="xls"?"Source.xls":"Source.xlsx");require(window.saveWorkbook(identity),"create the initial saved source before observing an overwrite");
  require(model->setData(model->index(0,0),"2"),"make a real unsaved edit before the observed save");
  const auto destination=kind=="csv"?files.filePath("Export.csv"):kind=="failure"?files.filePath("missing/Failure.xlsx"):identity;
  if(kind=="csv"){QFile old(destination);require(old.open(QIODevice::WriteOnly)&&old.write("old export\n")==11,"prepare an existing CSV destination for the actual export");}
  const bool existed=QFileInfo::exists(destination);QByteArray beforeBytes;
  if(existed){QFile old(destination);require(old.open(QIODevice::ReadOnly),"read exact bytes before the observed save");beforeBytes=old.readAll();}
  const auto before=window.currentSession().snapshot();const auto history=window.currentSession().historyBytes();
  struct SavingPaintProbe:QObject {
   QStatusBar* bar;QString destination,message;QByteArray beforeBytes;bool existed,paintedBeforeWrite=false;int savingPaints=0;
   bool eventFilter(QObject* object,QEvent* event)override{
    if(object==bar&&event->type()==QEvent::Paint&&bar->currentMessage()==message){
     ++savingPaints;QFile current(destination);const bool present=QFileInfo::exists(destination);
     paintedBeforeWrite|=present==existed&&(!present||(current.open(QIODevice::ReadOnly)&&current.readAll()==beforeBytes));
    }
    return QObject::eventFilter(object,event);
   }
  } probe;
  probe.bar=window.statusBar();probe.destination=destination;probe.message="Saving "+QFileInfo(destination).fileName()+"...";probe.beforeBytes=beforeBytes;probe.existed=existed;probe.bar->installEventFilter(&probe);
  require(probe.bar->isVisible(),"observe the real visible native status bar rather than a synthetic progress field");
  const bool saved=window.saveWorkbook(destination);
  require(saved==(kind!="failure"),"the real file operation succeeds or fails as requested");
  require(probe.savingPaints>0&&probe.paintedBeforeWrite,"the actual native Saving status must paint before destination bytes are replaced");
  require(!probe.bar->currentMessage().startsWith("Saving "),"success, failure and CSV early return all end the transient Saving state");
  require(!probe.message.contains("ESC",Qt::CaseInsensitive),"save progress does not advertise an unsupported cancellation action");
  require(window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history,"save progress and actual I/O add no workbook edit or Undo entry");
  require(window.windowFilePath()==identity,"the save lifecycle retains the correct workbook identity");
  if(saved){QFile written(destination);require(written.open(QIODevice::ReadOnly)&&written.readAll()!=beforeBytes,"successful native saving or CSV export actually replaces the destination bytes");}
  else require(!QFileInfo::exists(destination)&&window.isWindowModified()&&!window.lastError().isEmpty(),"failed saving leaves the destination absent and the edited workbook visibly unsaved");
  if(kind=="csv")require(window.isWindowModified()&&window.findChild<QLabel*>("status.saveState")->text()=="CSV exported — save XLSX to retain workbook changes","CSV cleanup preserves its truthful export guidance and unsaved source state");
  else if(saved)require(!window.isWindowModified()&&window.findChild<QLabel*>("workspace.saveState")->text()=="Saved locally","successful native save transitions from Saving to Saved locally");
  std::cout<<"native saving lifecycle "<<kind.toStdString()<<" passed\n";
 }
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto selected=argc>1?std::string(argv[1]):std::string();for(auto test:{std::pair{"file",fileOnIdentityRow},std::pair{"density",compactWorkspace},std::pair{"live",liveCellFormula},std::pair{"cancel",cancelCellEntry},std::pair{"navigation",entryNavigation},std::pair{"point",pointRangeWhileTyping},std::pair{"outline",referenceOutline},std::pair{"f4",f4PreservesEntry},std::pair{"narrow",narrowRibbon},std::pair{"completion",functionCompletion},std::pair{"fx",insertFunctionButton},std::pair{"calc_switch",sheetSwitchReplacesPendingCalculationQuietly},std::pair{"saving",savingProgressFollowsActualFileWrite},std::pair{"undo_selection",undoRestoresEditedSelection},std::pair{"undo_pruning",undoSelectionPruningPreservesLiveHistory}})if(selected.empty()||selected==test.first)test.second();std::cout<<"workspace UX tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
