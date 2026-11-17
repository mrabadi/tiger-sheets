#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/ribbon_page.hpp"
#include "tigersheets/recovery.hpp"
#include "tigersheets/biff.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QMenuBar>
#include <QAction>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
QLabel* saveIndicator(MainWindow& window){return window.findChild<QLabel*>("workspace.saveState");}
void legacyWorkbookIsDiscoverable(){
 MainWindow window;window.show();QApplication::processEvents();const auto before=window.currentSession().snapshot();bool discoverable=false;QTimer::singleShot(0,&window,[&]{if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){for(const auto& filter:dialog->nameFilters())if(filter.contains("*.xls ")||filter.contains("*.xls)"))discoverable=true;dialog->reject();}});require(!window.executeCommand("open")&&discoverable,"the native Open dialog discovers legacy .xls source workbooks");require(window.currentSession().snapshot()==before,"cancelling Open preserves workbook and history");
}
void newWorkbookIsClearlyUnsaved(){
 MainWindow window;window.resize(820,600);window.show();QApplication::processEvents();
 auto* indicator=saveIndicator(window);auto* name=window.findChild<QLabel*>("workspace.documentName");
 require(indicator&&indicator->isVisible(),"a new workbook needs an always-visible save status beside its name");
 require(name&&indicator->parentWidget()==name->parentWidget(),"save status belongs beside the workbook name in the header");
 require(indicator->visibleRegion().boundingRect().contains(indicator->rect()),"save status remains readable at the minimum supported window width");
 require(indicator->text()=="Not saved","a workbook that has never been saved says Not saved");
 require(indicator->accessibleName().contains("Not saved"),"save status is available without relying on color");
 require(window.isWindowModified()&&window.windowTitle().contains("[*]"),"an unsaved workbook exposes the native modified-title convention");
 require(!window.currentSession().canUndo(),"displaying save status never creates a workbook edit");
}
void saveAndEditTransitionsStayVisible(){
 QTemporaryDir files;MainWindow window;window.resize(820,600);window.show();QApplication::processEvents();
 auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* indicator=saveIndicator(window);auto* name=window.findChild<QLabel*>("workspace.documentName");
 require(model->setData(model->index(0,0),"10",Qt::EditRole),"edit a new workbook");
 require(indicator->text()=="Unsaved changes"&&window.isWindowModified(),"editing replaces Not saved with a prominent Unsaved changes state");
 QApplication::processEvents();require(indicator->visibleRegion().boundingRect().contains(indicator->rect()),"Unsaved changes remains readable at minimum window width");
 const auto path=files.filePath("Quarterly operating budget November 2026.xlsx");require(window.saveWorkbook(path)&&QFileInfo::exists(path),"save the workbook to a real local XLSX");
 require(indicator->text()=="Saved locally"&&!window.isWindowModified(),"only a successful save clears the modified indication");
 require(name->text()==QFileInfo(path).fileName()&&window.windowTitle().contains(QFileInfo(path).fileName())&&window.windowFilePath()==path,"saving updates the header name and native document identity");
 QApplication::processEvents();require(indicator->visibleRegion().boundingRect().contains(indicator->rect()),"Saved locally stays readable beside a long saved filename at minimum window width");
 const auto saved=window.currentSession().snapshot();require(model->setData(model->index(0,0),"10",Qt::EditRole)&&window.currentSession().snapshot()==saved,"accepting unchanged cell content is a no-op");
 require(indicator->text()=="Saved locally"&&!window.isWindowModified(),"a no-op edit does not mark the saved workbook modified");
 require(model->setData(model->index(0,0),"20",Qt::EditRole),"edit the saved workbook");
 require(indicator->text()=="Unsaved changes"&&window.isWindowModified(),"editing a saved workbook immediately restores Unsaved changes");
 QApplication::processEvents();require(indicator->visibleRegion().boundingRect().contains(indicator->rect()),"Unsaved changes stays readable beside a long saved filename at minimum window width");
 require(window.executeCommand("undo")&&model->raw(0,0)=="10","undo the last edit");
 require(indicator->text()=="Unsaved changes"&&window.isWindowModified(),"Undo refreshes the visible save status");
 require(window.saveWorkbook(path),"save the undone workbook");
 require(window.executeCommand("redo")&&model->raw(0,0)=="20","redo after saving");
 require(indicator->text()=="Unsaved changes"&&window.isWindowModified(),"Redo after saving marks the workbook unsaved");
 require(window.openWorkbook(path)&&model->raw(0,0)=="10","open the saved file over current edits");
 require(indicator->text()=="Saved locally"&&!window.isWindowModified(),"opening a saved workbook resets the modified state");
}
void failedAndCancelledSavesKeepChangesUnsaved(){
 QTemporaryDir files;MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* indicator=saveIndicator(window);
 require(model->setData(model->index(0,0),"123",Qt::EditRole),"seed the workbook before save failure checks");const auto path=files.filePath("Budget.xlsx");require(window.saveWorkbook(path),"save the initial workbook");
 require(model->setData(model->index(0,0),"456",Qt::EditRole),"make unsaved changes before save failure checks");const auto before=window.currentSession().snapshot();const auto title=window.windowTitle();
 require(!window.saveWorkbook(files.filePath("missing/Budget.xlsx"))&&!window.lastError().isEmpty(),"an unwritable destination reports a save failure");
 require(window.currentSession().snapshot()==before&&indicator->text()=="Unsaved changes"&&window.isWindowModified(),"a failed save leaves the data and visible unsaved state intact");
 require(window.windowFilePath()==path&&window.windowTitle()==title,"a failed save does not replace the workbook's document identity");
 bool cancelled=false;QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){cancelled=true;dialog->reject();}});
 require(!window.executeCommand("save.as")&&cancelled,"cancelling Save As returns without saving");
 require(window.currentSession().snapshot()==before&&indicator->text()=="Unsaved changes"&&window.isWindowModified(),"cancelling Save As keeps the visible unsaved state and data");
 require(window.windowFilePath()==path&&window.windowTitle()==title,"cancelling Save As preserves the workbook's document identity");
 grid->setFocus();window.activateWindow();QApplication::processEvents();
 bool f12Opened=false;QTimer::singleShot(0,&window,[&]{if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){f12Opened=dialog->acceptMode()==QFileDialog::AcceptSave;dialog->reject();}});
 QTest::keyClick(grid,Qt::Key_F12);QApplication::processEvents();
 require(f12Opened,"F12 opens the actual Save As dialog from the worksheet");
 require(window.currentSession().snapshot()==before&&indicator->text()=="Unsaved changes"&&window.isWindowModified()&&window.windowFilePath()==path&&window.windowTitle()==title,"cancelling F12 Save As preserves data, dirty state and document identity");
 const auto csv=files.filePath("Export.csv");require(window.saveWorkbook(csv)&&QFileInfo::exists(csv),"export the current sheet to CSV");
 require(indicator->text()=="Unsaved changes"&&window.isWindowModified()&&window.windowFilePath()==path,"exporting CSV does not claim the source workbook has been saved");
}
void recoveryRequiresANewSave(){
 QTemporaryDir files;MainWindow window;require(window.saveWorkbook(files.filePath("Saved.xlsx")),"start recovery from a clean saved workbook");
 auto recovered=window.currentSession().snapshot();recovered["sheets"][0]["cells"]=Json::array({{{"row",1},{"col",1},{"input","789"}}});
 {RecoveryStore store((qEnvironmentVariable("TIGERSHEETS_STATE_DIR")+"/recovery.sqlite").toStdString());store.checkpoint("save-state-recovered",recovered);}
 require(window.executeCommand("recover",{{"id","save-state-recovered"}}),"recover the checkpoint through the native workbook command");
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());require(model->raw(0,0)=="789","recovery restores the checkpoint contents");
 require(saveIndicator(window)->text()=="Unsaved changes"&&window.isWindowModified(),"a recovered workbook cannot retain the previous file's Saved locally status");
 require(window.windowFilePath().isEmpty()&&window.windowTitle().contains("Recovered workbook[*]"),"recovery clears the previous file identity and keeps the modified title");
 require(window.findChild<QLabel*>("status.saveState")->text()=="Recovered — save a new copy","recovery retains its explicit save-a-new-copy guidance");
}
void failedRecoveryFlushKeepsDocumentIdentity(){
 QTemporaryDir state;const auto previous=qgetenv("TIGERSHEETS_STATE_DIR");
 struct RestoreEnvironment {QByteArray value;~RestoreEnvironment(){qputenv("TIGERSHEETS_STATE_DIR",value);}} restore{previous};qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());
 QFile corrupt(state.filePath("recovery.sqlite"));require(corrupt.open(QIODevice::WriteOnly)&&corrupt.write("not a SQLite database")>0,"create an isolated corrupt recovery database");corrupt.close();
 MainWindow window;auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());require(model->setData(model->index(0,0),"42",Qt::EditRole),"edit before the recovery barrier failure");
 auto* name=window.findChild<QLabel*>("workspace.documentName");const auto originalName=name->text(),originalTitle=window.windowTitle();const auto path=state.filePath("Uncommitted.xlsx");
 require(!window.saveWorkbook(path)&&QFileInfo::exists(path),"the final recovery barrier can fail after the XLSX itself was written");
 require(name->text()==originalName&&window.windowTitle()==originalTitle&&window.windowFilePath().isEmpty(),"a failed save cannot partially rename or reidentify the current workbook");
 require(saveIndicator(window)->text()=="Unsaved changes"&&window.isWindowModified(),"a failed final save barrier keeps the workbook visibly unsaved");
 bool chooser=false;QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){chooser=qobject_cast<QFileDialog*>(dialog);dialog->reject();}});
 require(!window.executeCommand("save")&&chooser,"retrying a failed first save still asks where to save the workbook");
}
void recoveryContinuesAfterNativeSave(){
 for(const auto* extension:{".xlsx",".xls"}){
  QTemporaryDir files;const auto previous=qgetenv("TIGERSHEETS_STATE_DIR");
  struct RestoreEnvironment {QByteArray value;~RestoreEnvironment(){qputenv("TIGERSHEETS_STATE_DIR",value);}} restore{previous};
  qputenv("TIGERSHEETS_STATE_DIR",files.path().toUtf8());
  const auto source=files.filePath("Source.xls"),savedPath=files.filePath(QString("Saved")+extension);
  require(QFile::copy(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("fixtures/biff/independent.xls"),source),"prepare an independent native source for post-save recovery");
  QByteArray savedBytes;Json recoveredRecord;Json liveRevision;
  const auto inputAt=[](const Json& book,int row,int col){for(const auto& cell:book.at("sheets")[0].at("cells"))if(cell.at("row")==row&&cell.at("col")==col)return cell.value("input",std::string());return std::string();};
  {
   MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();require(window.openWorkbook(source),"open the real BIFF source before editing and saving");
   auto* grid=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());
   const auto typeCell=[&](int row,const QString& text){
    require(model->raw(row-1,25).isEmpty(),"the independent recovery fixture uses initially blank cells");
    grid->setCurrentIndex(model->index(row-1,25));grid->scrollTo(grid->currentIndex());grid->setFocus();QApplication::processEvents();
    QTest::keyClick(grid,Qt::Key_F2);QApplication::processEvents();auto* editor=grid->activeCellEditor();require(editor&&editor->isVisible(),"real F2 opens the native inline editor on an empty cell");
    QTest::keyClicks(editor,text);QTest::keyClick(editor,Qt::Key_Return);QApplication::processEvents();
    require(model->raw(row-1,25)==text,"the native editor commits the requested recovery content");
   };
   typeCell(50,"Saved-before-save");const auto savedRevision=window.currentSession().snapshotView().at("revision");
   require(savedRevision.get<int>()>0&&window.saveWorkbook(savedPath),"save a genuinely edited workbook through MainWindow");
   QFile saved(savedPath);require(saved.open(QIODevice::ReadOnly),"retain the exact saved source package");savedBytes=saved.readAll();saved.close();
   require(inputAt(readWorkbook(savedPath.toStdString()).workbook,50,26)=="Saved-before-save","the saved package contains the pre-save edit");
   typeCell(51,"Unsaved-after-save");liveRevision=window.currentSession().snapshotView().at("revision");
   require(liveRevision==savedRevision.get<int>()+1,"a post-save native edit advances exactly one revision");
   if(!window.executeCommand("recovery.flush"))throw std::runtime_error(std::string("post-save ")+extension+" recovery flush failed: "+window.lastError().toStdString());
   const auto records=RecoveryStore(files.filePath("recovery.sqlite").toStdString()).records();
   require(records.size()==1,"post-save unsaved changes create exactly one durable recovery record");recoveredRecord=records[0];
   require(recoveredRecord.at("workbook").at("revision")==liveRevision&&inputAt(recoveredRecord.at("workbook"),50,26)=="Saved-before-save"&&inputAt(recoveredRecord.at("workbook"),51,26)=="Unsaved-after-save","the durable journal combines the saved base and unsaved edit at the current revision");
   require(recoveredRecord.at("sourceBytes").get<std::string>()==savedBytes.toStdString()&&recoveredRecord.at("sourcePath")==savedPath.toStdString(),"recovery retains the exact post-save source bytes and identity");
  }
  const auto reopenedRecords=RecoveryStore(files.filePath("recovery.sqlite").toStdString()).records();require(reopenedRecords.size()==1&&reopenedRecords[0]==recoveredRecord,"the complete recovery record survives destruction of the original MainWindow");
  MainWindow restored;require(restored.executeCommand("recover",{{"id",recoveredRecord.at("id")}}),"a new MainWindow recovers the durable post-save journal");
  auto* restoredModel=dynamic_cast<WorksheetModel*>(restored.findChild<QTableView*>("worksheet.grid")->model());
  require(restoredModel->raw(49,25)=="Saved-before-save"&&restoredModel->raw(50,25)=="Unsaved-after-save"&&restored.currentSession().snapshotView().at("revision")==liveRevision,"native recovery restores both edits and the exact live revision");
  QFile unchanged(savedPath);require(unchanged.open(QIODevice::ReadOnly)&&unchanged.readAll()==savedBytes&&inputAt(readWorkbook(savedPath.toStdString()).workbook,51,26).empty(),"recovering an unsaved edit leaves the saved source package unchanged");
  require(restored.isWindowModified()&&restored.windowFilePath().isEmpty(),"a recovered post-save edit still requires an explicit new save");
  std::cout<<"post-save recovery "<<extension<<" durable content passed\n";
 }
}
void templateStartsWithANewDocumentIdentity(){
 QTemporaryDir files;MainWindow window;require(window.saveWorkbook(files.filePath("Existing.xlsx")),"start the example from a named saved workbook");window.loadFinancialExample();
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());require(model->raw(0,0)=="FINANCIAL MODEL","load the financial example into a fresh workbook");
 require(window.findChild<QLabel*>("workspace.documentName")->text()=="Book1.xlsx"&&window.windowTitle().contains("Book1.xlsx[*]"),"a new example must not retain the replaced workbook's filename or title");
 require(window.windowFilePath().isEmpty()&&window.isWindowModified()&&saveIndicator(window)->text()=="Unsaved changes","the example clearly needs its own first save");
}
void headerStatusFitsBesideLongFilename(){
 QTemporaryDir files;MainWindow window;window.resize(1440,960);window.show();QApplication::processEvents();auto* indicator=saveIndicator(window);auto* header=window.findChild<QWidget*>("workspace.header");auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto path=files.filePath("Otis three statement model complete tutorial replica and forecast operating assumptions November 2026.xlsx");require(window.saveWorkbook(path),"save the real workbook with a long filename before checking header layout");
 auto fits=[&]{
  QApplication::processEvents();indicator->ensurePolished();require(indicator->width()>=indicator->sizeHint().width()&&indicator->height()>=indicator->sizeHint().height(),"the save badge allocates its full styled sizeHint so status text is never clipped");require(indicator->isVisible()&&indicator->visibleRegion().boundingRect().contains(indicator->rect()),"the complete save badge stays visible beside the document name");
  const auto siblings=header->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);for(int first=0;first<siblings.size();++first){if(!siblings[first]->isVisible())continue;const QRect a(siblings[first]->mapTo(header,QPoint()),siblings[first]->size());require(header->rect().contains(a),"header controls remain inside the allocated corner");for(int second=first+1;second<siblings.size();++second){if(!siblings[second]->isVisible())continue;const QRect b(siblings[second]->mapTo(header,QPoint()),siblings[second]->size());if(a.intersects(b))std::cerr<<"header overlap at width "<<window.width()<<": "<<siblings[first]->objectName().toStdString()<<' '<<a.x()<<','<<a.width()<<" and "<<siblings[second]->objectName().toStdString()<<' '<<b.x()<<','<<b.width()<<'\n';require(!a.intersects(b),"saved and unsaved header siblings must not overlap");}}
  auto* menus=window.menuBar();const QRect corner(header->mapTo(menus,QPoint()),header->size());auto* identity=window.findChild<QWidget*>("workspace.identity");require(menus->rect().contains(corner)&&!corner.intersects(QRect(identity->mapTo(menus,QPoint()),identity->size())),"the complete header corner stays inside the menu bar and clear of the application identity");for(auto* action:menus->actions())if(action->isVisible()){const auto rect=menus->actionGeometry(action);require(!rect.isEmpty()&&!corner.intersects(rect),"header sizing leaves the File menu visible and unobstructed");}
 };
 require(indicator->text()=="Saved locally","a successful long-name save reports the full saved status");fits();require(grid->model()->setData(grid->model()->index(0,0),"42",Qt::EditRole),"edit the named workbook before checking the longer unsaved label");require(indicator->text()=="Unsaved changes","the long-name workbook reports the full unsaved status");fits();
 for(const auto& name:QStringList{"command.open","command.undo","command.redo","command.save","command.find","command.codex.toggle"}){auto* control=window.findChild<QToolButton*>(name);require(control&&control->isVisible()&&control->visibleRegion().boundingRect().contains(control->rect())&&QRect(header->mapTo(&window,QPoint()),header->size()).contains(QRect(control->mapTo(&window,QPoint()),control->size())),"header commands remain fully visible beside the filename");}
 require(!window.findChild<QWidget*>("workspace.quickAccess"),"long filenames do not require a duplicate formatting row");auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");for(int i=0;i<ribbon->count();++i)if(ribbon->tabText(i)=="Home")ribbon->setCurrentIndex(i);window.executeCommand("view.ribbon.open");auto* page=dynamic_cast<RibbonPage*>(ribbon->currentWidget());require(page,"Home uses the responsive page");
 for(const auto* name:{"tutorial.home.center","home.borders","tutorial.home.fill","tutorial.home.increaseDecimals"}){auto* control=window.findChild<QToolButton*>(name);require(control,"format command exists on Home");page->ensureWidgetVisible(control,4,0);QApplication::processEvents();require(control->isVisible()&&page->viewport()->rect().contains(QRect(control->mapTo(page->viewport(),QPoint()),control->size())),"Home overflow keeps every formatting command reachable without changing the header");}window.executeCommand("view.ribbon.close");
 window.resize(820,600);QApplication::processEvents();require(window.width()==820,"the minimum supported 820-pixel window remains usable");fits();require(window.saveWorkbook(path),"save again at minimum width");fits();
}
void legacySaveCannotOverwriteSource(){
 QTemporaryDir files;const auto fixture=QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("fixtures/biff/independent.xls");const auto source=files.filePath("Otis-10-K-Raw.xls");require(QFile::copy(fixture,source),"copy an independently generated BIFF source into an isolated directory");MainWindow window;window.show();QApplication::processEvents();require(window.openWorkbook(source),"open the actual legacy workbook through MainWindow");auto* grid=window.findChild<QTableView*>("worksheet.grid");require(grid->model()->setData(grid->model()->index(0,0),"Changed source",Qt::EditRole),"make an unsaved edit in the imported source");require(window.saveWorkbook(source),"supported BIFF Save writes actual edited values and styles to the original .xls path");QFile saved(source);require(saved.open(QIODevice::ReadOnly),"read the saved native BIFF bytes");const auto bytes=saved.readAll();saved.close();require(isCompoundWorkbook(bytes.toStdString())&&!window.isWindowModified()&&window.windowFilePath()==source,"a native legacy save retains CFB encoding and original filename identity");require(readWorkbook(source.toStdString()).workbook.at("sheets")[0].at("cells")[0].at("input")=="Changed source","reopened .xls contains the actual edited source");
 require(grid->model()->setData(grid->model()->index(0,0),"Second source edit",Qt::EditRole),"edit the source again before keyboard Save");bool chooser=false;QTimer::singleShot(0,&window,[&]{if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){chooser=true;dialog->reject();}});grid->setFocus();QTest::keyClick(grid,Qt::Key_S,Qt::ControlModifier);QApplication::processEvents();require(!chooser&&!window.isWindowModified()&&window.windowFilePath()==source,"Ctrl+S saves a supported .xls directly without a conversion chooser");
 QFile baseline(source);require(baseline.open(QIODevice::ReadOnly),"read legacy file before unsupported formula save");const auto preserved=baseline.readAll();baseline.close();require(grid->model()->setData(grid->model()->index(0,0),"=1+2",Qt::EditRole),"enter a formula that BIFF serialization does not support");require(!window.saveWorkbook(source)&&window.lastError().contains("formula",Qt::CaseInsensitive),"formula-bearing .xls export reports its unsupported partition");QFile untouched(source);require(untouched.open(QIODevice::ReadOnly)&&untouched.readAll()==preserved&&window.isWindowModified()&&window.windowFilePath()==source,"a refused unsupported BIFF save retains every original file byte, dirty state and source identity");
}
void legacySaveOffersNativeCopy(){
 QTemporaryDir files;const auto source=files.filePath("Otis-10-K-Raw.xls"),destination=files.filePath("Otis-10-K-Raw.xlsx");require(QFile::copy(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("fixtures/biff/independent.xls"),source),"prepare an isolated legacy source for Save conversion");MainWindow window;window.show();QApplication::processEvents();require(window.openWorkbook(source),"open legacy source for Save conversion");auto* grid=window.findChild<QTableView*>("worksheet.grid");require(grid->model()->setData(grid->model()->index(0,0),"Changed source",Qt::EditRole),"edit the live imported workbook");bool chooser=false,defaultName=false;QTimer::singleShot(0,[&]{auto* modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(auto* dialog=qobject_cast<QFileDialog*>(modal)){chooser=true;defaultName=dialog->selectedFiles().value(0)==destination;dialog->selectFile(destination);QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);}else if(modal)modal->reject();});require(window.executeCommand("save.as")&&chooser&&defaultName,"Save As on an imported .xls offers its matching .xlsx copy path");require(window.windowFilePath()==destination&&!window.isWindowModified()&&QFileInfo::exists(source)&&QFileInfo::exists(destination),"native conversion retains the original legacy source and adopts the saved XLSX document identity");MainWindow reopened;require(reopened.openWorkbook(destination),"reopen the converted native file");require(reopened.currentSession().snapshotView().at("sheets")[0].at("cells")[0].at("input")=="Changed source","the XLSX copy contains the actual edited source workbook");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 const auto test=argc>1?std::string(argv[1]):std::string("all");
 if(test=="new"||test=="all")newWorkbookIsClearlyUnsaved();
 if(test=="transitions"||test=="all")saveAndEditTransitionsStayVisible();
 if(test=="boundaries"||test=="all")failedAndCancelledSavesKeepChangesUnsaved();
 if(test=="recovery"||test=="all")recoveryRequiresANewSave();
 if(test=="save_identity"||test=="all")failedRecoveryFlushKeepsDocumentIdentity();
 if(test=="recovery_after_save"||test=="all")recoveryContinuesAfterNativeSave();
 if(test=="template"||test=="all")templateStartsWithANewDocumentIdentity();
 if(test=="header_fit"||test=="all")headerStatusFitsBesideLongFilename();
 if(test=="legacy_open"||test=="all")legacyWorkbookIsDiscoverable();
 if(test=="legacy"||test=="all")legacySaveCannotOverwriteSource();
 if(test=="legacy_conversion"||test=="all")legacySaveOffersNativeCopy();
 std::cout<<"workbook save state UI "<<test<<" passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
