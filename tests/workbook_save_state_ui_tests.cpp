#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/recovery.hpp"
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
QLabel* saveIndicator(MainWindow& window){return window.findChild<QLabel*>("workspace.saveState");}
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
void templateStartsWithANewDocumentIdentity(){
 QTemporaryDir files;MainWindow window;require(window.saveWorkbook(files.filePath("Existing.xlsx")),"start the example from a named saved workbook");window.loadFinancialExample();
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());require(model->raw(0,0)=="FINANCIAL MODEL","load the financial example into a fresh workbook");
 require(window.findChild<QLabel*>("workspace.documentName")->text()=="Book1.xlsx"&&window.windowTitle().contains("Book1.xlsx[*]"),"a new example must not retain the replaced workbook's filename or title");
 require(window.windowFilePath().isEmpty()&&window.isWindowModified()&&saveIndicator(window)->text()=="Unsaved changes","the example clearly needs its own first save");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 const auto test=argc>1?std::string(argv[1]):std::string("all");
 if(test=="new"||test=="all")newWorkbookIsClearlyUnsaved();
 if(test=="transitions"||test=="all")saveAndEditTransitionsStayVisible();
 if(test=="boundaries"||test=="all")failedAndCancelledSavesKeepChangesUnsaved();
 if(test=="recovery"||test=="all")recoveryRequiresANewSave();
 if(test=="save_identity"||test=="all")failedRecoveryFlushKeepsDocumentIdentity();
 if(test=="template"||test=="all")templateStartsWithANewDocumentIdentity();
 std::cout<<"workbook save state UI "<<test<<" passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
