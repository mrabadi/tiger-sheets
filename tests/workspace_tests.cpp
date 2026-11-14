#include "tigersheets/main_window.hpp"
#include "tigersheets/recovery.hpp"
#include <QApplication>
#include <QTableView>
#include <QLineEdit>
#include <QTabWidget>
#include <QTest>
#include <QTemporaryDir>
#include <QDialog>
#include <QPointer>
#include <QFile>
#include <QPushButton>
#include "tigersheets/ooxml.hpp"
#include "tigersheets/csv.hpp"
#include <iostream>
int main(int argc,char**argv){
 QApplication app(argc,argv);
 QTemporaryDir stateDirectory;qputenv("TIGERSHEETS_STATE_DIR",stateDirectory.path().toUtf8());
 tigersheets::MainWindow window;
 auto* grid=window.findChild<QTableView*>("worksheet.grid");
 auto* formula=window.findChild<QLineEdit*>("formula.input");
 auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");
 if(!grid||!formula||!ribbon||ribbon->tabText(0)!="Home"){std::cerr<<"FAILED: Excel familiar native workspace\n";return 1;}
 grid->setCurrentIndex(grid->model()->index(0,0));formula->setText("=10+20");QTest::keyClick(formula,Qt::Key_Return);
 if(window.currentSession().snapshot()["sheets"][0]["cells"][0]["input"]!="=10+20"){std::cerr<<"FAILED: formula bar edits active cell\n";return 1;}
 if(!grid->currentIndex().isValid()){std::cerr<<"FAILED: editing preserves active-cell navigation\n";return 1;}
 window.recalculateNow();
 if(grid->model()->data(grid->model()->index(0,0)).toString()!="30"){std::cerr<<"FAILED: entered formula calculates through actual engine\n";return 1;}
 grid->setCurrentIndex(grid->model()->index(0,0));
 grid->selectionModel()->select(QItemSelection(grid->model()->index(0,0),grid->model()->index(2,0)),QItemSelectionModel::ClearAndSelect);
 if(!window.executeCommand("fill.down")||window.currentSession().snapshot()["sheets"][0]["cells"].size()!=3){std::cerr<<"FAILED: Fill Down copies selected formulas as one undo transaction\n";return 1;}
 QTemporaryDir folder;auto path=folder.filePath("model.xlsx");
 if(!window.saveWorkbook(path)||!window.openWorkbook(path)||window.currentSession().snapshot()["sheets"][0]["cells"].size()!=3){std::cerr<<"FAILED: workspace native file roundtrip\n";return 1;}
 grid->setCurrentIndex(grid->model()->index(0,0));QTest::keyClick(grid,Qt::Key_End,Qt::ControlModifier);
 if(grid->currentIndex().row()!=2||grid->currentIndex().column()!=0){std::cerr<<"FAILED: Ctrl+End navigates to the used range\n";return 1;}
 QTest::keyClick(grid,Qt::Key_Home,Qt::ControlModifier);QTest::keyClick(grid,Qt::Key_Down,Qt::ControlModifier);
 if(grid->currentIndex().row()!=2){std::cerr<<"FAILED: Ctrl+Down navigates to the edge of the current data region\n";return 1;}
 formula->setText("=A1");formula->setCursorPosition(3);QTest::keyClick(formula,Qt::Key_F4);
 if(formula->text()!="=$A$1"){std::cerr<<"FAILED: F4 cycles references while editing formula\n";return 1;}
 if(!window.executeCommand("vba.editor")){std::cerr<<"FAILED: Developer command opens a working VBA editor\n";return 1;}
 QPointer<QDialog> editor=window.findChild<QDialog*>("vba.editor");
 if(!window.executeCommand("vba.record"))return 1;QPointer<QDialog> recorder=window.findChild<QDialog*>("vba.recorder");if(!recorder)return 1;recorder->findChild<QPushButton*>("vba.record.start")->click();
 if(!editor||!window.openWorkbook(path)||editor||recorder){std::cerr<<"FAILED: changing workbooks destroys the previous workbook macro dialogs\n";return 1;}
 formula->setText("=42");QTest::keyClick(formula,Qt::Key_Return);
 if(!window.executeCommand("recovery.flush")){std::cerr<<"FAILED: recovery flush barrier\n";return 1;}
 tigersheets::RecoveryStore recovery(stateDirectory.filePath("recovery.sqlite").toStdString());auto records=recovery.records();QFile saved(path);saved.open(QIODevice::ReadOnly);
 // Replacement retains unsaved editor/macro state under the previous identity.
 // Select the current workbook rather than assuming it is the only recovery.
 const auto savedBytes=saved.readAll().toStdString();tigersheets::Json currentRecovery;
 for(const auto& record:records)if(record.at("workbook")==window.currentSession().snapshotView()&&record.at("sourceBytes")==savedBytes)currentRecovery=record;
 if(currentRecovery.is_null()){std::cerr<<"FAILED: recovery retains original package bytes with current unsaved edits\n";return 1;}
 saved.close();QFile::remove(path);tigersheets::MainWindow restored;
 if(!restored.executeCommand("recover",{{"id",currentRecovery.at("id")}})||restored.currentSession().snapshot()["sheets"][0]["cells"][0]["input"]!="=42"||!restored.saveWorkbook(folder.filePath("recovered.xlsx"))){std::cerr<<"FAILED: recovery restores unsaved edits and original package after source deletion\n";return 1;}
 auto book=window.currentSession().snapshot();window.currentSession().preview(book["revision"],tigersheets::Json::array({{{"type","set_cell"},{"sheet",book["sheets"][0]["id"]},{"row",1},{"col",1},{"input","=43"}}}));
 if(!window.executeCommand("preview.accept")||!window.executeCommand("recovery.flush")){std::cerr<<"FAILED: accepted Codex proposals create a recovery checkpoint\n";return 1;}
 bool acceptedRecovered=false;for(const auto& record:recovery.records())if(record.at("id")==currentRecovery.at("id")&&record.at("workbook")==window.currentSession().snapshotView())acceptedRecovered=true;
 if(!acceptedRecovered){std::cerr<<"FAILED: accepted Codex proposal recreates its own current recovery after another window saves it\n";return 1;}
 tigersheets::MainWindow pending;pending.show();app.processEvents();auto* pendingGrid=pending.findChild<QTableView*>("worksheet.grid");auto active=pendingGrid->model()->index(0,0);pendingGrid->setCurrentIndex(active);pendingGrid->edit(active);app.processEvents();auto* cellEditor=pendingGrid->findChild<QLineEdit*>();if(!cellEditor)return 1;cellEditor->setText("77");cellEditor->setFocus();auto pendingPath=folder.filePath("pending.xlsx");
 if(!pending.saveWorkbook(pendingPath)||tigersheets::readWorkbook(pendingPath.toStdString()).workbook["sheets"][0]["cells"].empty()||tigersheets::readWorkbook(pendingPath.toStdString()).workbook["sheets"][0]["cells"][0]["input"]!="77"){std::cerr<<"FAILED: saving commits the active cell editor\n";return 1;}
 auto* pendingFormula=pending.findChild<QLineEdit*>("formula.input");pendingFormula->setFocus();pendingFormula->selectAll();QTest::keyClicks(pendingFormula,"=6*7");if(!pending.saveWorkbook(pendingPath)||tigersheets::readWorkbook(pendingPath.toStdString()).workbook["sheets"][0]["cells"][0]["input"]!="=6*7"){std::cerr<<"FAILED: saving commits the formula bar without Return\n";return 1;}
 pendingFormula->setFocus();pendingFormula->selectAll();QTest::keyClicks(pendingFormula,"=6*8");QTest::keyClick(pendingFormula,Qt::Key_Return);const auto title=pending.windowTitle(),csvPath=folder.filePath("export.csv");const auto original=pending.currentSession().snapshot();
 if(!pending.saveWorkbook(csvPath)||tigersheets::readCsv(csvPath.toStdString())["sheets"][0]["cells"][0]["input"]!="48"){std::cerr<<"FAILED: CSV export writes calculated values rather than formula source\n";return 1;}
 if(pending.windowTitle()!=title||pending.currentSession().snapshot()!=original){std::cerr<<"FAILED: CSV export preserves the workbook identity and formula model\n";return 1;}
 if(!pending.executeCommand("recovery.flush")){std::cerr<<"FAILED: CSV recovery flush barrier\n";return 1;}
 bool retained=false;for(const auto& record:recovery.records())if(record["sourcePath"]==pendingPath.toStdString())retained=true;if(!retained){std::cerr<<"FAILED: CSV export retains unsaved workbook recovery\n";return 1;}
 tigersheets::MainWindow csvWindow;if(!csvWindow.openWorkbook(csvPath))return 1;auto* csvGrid=csvWindow.findChild<QTableView*>("worksheet.grid");csvGrid->model()->setData(csvGrid->model()->index(0,0),"=9*9",Qt::EditRole);
 if(!csvWindow.saveWorkbook(csvPath)||!csvWindow.saveWorkbook(csvPath)){std::cerr<<"FAILED: repeated CSV exports track their own file changes\n";return 1;}
}
