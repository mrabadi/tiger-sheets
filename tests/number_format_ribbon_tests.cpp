#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QTableView>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
QComboBox* numberFormats(MainWindow& window){for(auto* combo:window.findChildren<QComboBox*>())if(combo->findText("General")>=0&&combo->findText("Accounting")>=0&&combo->findText("Currency")>=0)return combo;throw std::runtime_error("Home number-format control exists");}
void followsCurrentCell(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* formats=numberFormats(window);const auto sheet=model->sheetId().toStdString();
 Json operations=Json::array();const std::vector<std::pair<std::string,std::string>> examples={{"$#,##0.00","Currency"},{"General","General"},{"m/d/yy","Date"},{"0%","Percent"},{"#,##0.00","Number"},{"$#,##0.00;($#,##0.00);-","Accounting"},{"0.00E+00","Custom…"},{"0.0\"%\"","Custom…"},{"0\" days\"","Custom…"},{"\"$\"#,##0","Currency"},{"[$-409]0.00","Custom…"},{"[$$-409]#,##0.00","Currency"},{"#,##0.000;(#,##0.000)","Number"},{"$#,##0;($#,##0);-","Accounting"},{"@","Text"}};
 for(int i=0;i<static_cast<int>(examples.size());++i)operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",i+1},{"col",1},{"input","42"},{"style",{{"numberFormat",examples[i].first}}}});
 operations.push_back({{"type","set_cell"},{"sheet",sheet},{"row",6},{"col",2},{"input","Gross Profit"}});require(model->apply(operations),"seed differently formatted cells");const auto before=window.currentSession().snapshot();
 for(int i=0;i<static_cast<int>(examples.size());++i){grid->setCurrentIndex(model->index(i,0));require(formats->currentText().toStdString()==examples[i].second,"selected cell's number format must show "+examples[i].second+", got "+formats->currentText().toStdString());}
 grid->setCurrentIndex(model->index(5,1));require(formats->currentText()=="General","text cell B6 must not retain the previously selected number format");require(window.currentSession().snapshot()==before,"selection updates the control without modifying workbook or history");
}
void followsFormattingAndHistory(){
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* formats=numberFormats(window);grid->setCurrentIndex(model->index(0,0));require(model->setData(model->index(0,0),"0.15",Qt::EditRole),"seed selected number");
 require(window.executeCommand("number.percent"),"percentage formatting succeeds");require(formats->currentText()=="Percent","toolbar action refreshes displayed format without changing selection");
 require(window.executeCommand("undo"),"undo percent format");require(formats->currentText()=="General","Undo refreshes selected cell format");require(window.executeCommand("redo"),"redo percent format");require(formats->currentText()=="Percent","Redo refreshes selected cell format");
 require(window.executeCommand("cells.format",{{"style",{{"numberFormat","yyyy-mm-dd"}}}}),"format through common cell command");require(formats->currentText()=="Date","Format Cells refreshes the ribbon format category");
 require(window.executeCommand("cells.clearFormats"),"clear selected cell formats");require(formats->currentText()=="General","Clear Formats resets ribbon category");
 const auto percent=formats->findText("Percent");formats->setCurrentIndex(percent);require(QMetaObject::invokeMethod(formats,"activated",Qt::DirectConnection,Q_ARG(int,percent)),"activate number-format choice");require(model->cellStyle(0,0).at("numberFormat")=="0.0%","choosing a format continues to apply it");
}
void followsWorkbookAndSheet(){
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* formats=numberFormats(window);grid->setCurrentIndex(model->index(0,0));require(window.executeCommand("cells.format",{{"style",{{"numberFormat","$#,##0.00"}}}}),"format first sheet");
 require(window.executeCommand("sheet.add"),"add second sheet");require(formats->currentText()=="General","new sheet does not retain previous sheet's currency format");auto* tabs=window.findChild<QTabBar*>("workspace.sheets");tabs->setCurrentIndex(0);require(formats->currentText()=="Currency","switching back restores the active sheet's format");
 QTemporaryDir files;const auto path=files.filePath("formats.xlsx");require(window.saveWorkbook(path),"save formatted workbook");require(window.openWorkbook(path),"reopen formatted workbook");require(formats->currentText()=="Currency","opening a workbook refreshes the selected cell format");
}
void dateEditingUsesReadableDates(){
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* formula=window.findChild<QLineEdit*>("formula.input");require(model->setData(model->index(0,0),"2025-09-30",Qt::EditRole),"enter a date in the worksheet");grid->setCurrentIndex(model->index(0,1));grid->setCurrentIndex(model->index(0,0));require(formula->text()=="2025-09-30","selecting an entered date must show a readable date in the formula bar instead of a serial");require(numberFormats(window)->currentText()=="Date","entered date is identified as Date");require(model->raw(0,0)=="45930","readable editor keeps canonical numeric storage");
}
void formattingKeepsDateEditorCurrent(){
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());auto* formula=window.findChild<QLineEdit*>("formula.input");require(model->setData(model->index(0,0),"2025-09-30",Qt::EditRole),"seed date before formatting change");grid->setCurrentIndex(model->index(0,1));grid->setCurrentIndex(model->index(0,0));require(formula->text()=="2025-09-30","date initially appears in formula bar");
 require(window.executeCommand("cells.format",{{"style",{{"numberFormat","General"}}}}),"choose General for the selected date");require(formula->text()=="45930","formatting the current date as General must refresh its formula-bar value");QTest::keyClick(formula,Qt::Key_Return);require(model->cellStyle(0,0).at("numberFormat")=="General"&&model->raw(0,0)=="45930","accepting an unchanged formula bar must preserve General and canonical value");grid->setCurrentIndex(model->index(0,0));require(window.executeCommand("undo"),"undo General formatting");require(formula->text()=="2025-09-30","Undo refreshes the same selected date's editor value");
 formula->setSelection(5,2);require(model->setData(model->index(0,1),"7",Qt::EditRole),"edit another cell while formula text is unchanged");require(formula->selectionStart()==5&&formula->selectedText()=="09","unrelated model updates preserve formula-bar cursor and selection");formula->setText("=B1+2");formula->setModified(true);require(model->setData(model->index(0,1),"8",Qt::EditRole),"update another cell while a formula draft exists");require(formula->text()=="=B1+2"&&formula->isModified(),"model updates preserve an uncommitted formula-bar draft");formula->setModified(false);
}
void numberOptionsAreAvailableFromHome(){
 MainWindow window;window.resize(1700,900);window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());grid->setCurrentIndex(model->index(0,0));require(model->setData(model->index(0,0),"-1234.567",Qt::EditRole),"seed currency value");require(window.executeCommand("cells.format",{{"style",{{"numberFormat","$#,##0.00"}}}}),"seed cents format");QToolButton* options=nullptr;for(auto* button:window.findChildren<QToolButton*>())if(button->text()=="Format…")options=button;require(options,"Home Number group exposes Format options");bool opened=false;std::string failure;
 QTimer::singleShot(0,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog){failure="native format dialog did not open";return;}opened=dialog->objectName()=="formatCells.dialog";auto* category=dialog->findChild<QComboBox*>("formatCells.category");auto* places=dialog->findChild<QSpinBox*>("formatCells.decimalPlaces");auto* negatives=dialog->findChild<QComboBox*>("formatCells.negativeStyle");if(!category||!places||!negatives){failure="Currency options are available in the native dialog";dialog->reject();return;}category->setCurrentIndex(category->findData("currency"));places->setValue(0);const auto parentheses=negatives->findData("parentheses");if(parentheses<0){failure="Currency offers parentheses for negative values";dialog->reject();return;}negatives->setCurrentIndex(parentheses);dialog->accept();});QTest::mouseClick(options,Qt::LeftButton);require(failure.empty(),failure);require(opened,"ribbon opens native Format Cells");require(model->data(model->index(0,0)).toString()=="($1,235)","currency choices show whole dollars and parenthesized negatives");require(model->numericValue(0,0).value_or(0)==-1234.567,"display rounding never changes the stored value");require(numberFormats(window)->currentText()=="Currency","ribbon tracks configured currency display");
}
void cancelledCustomFormatRestoresCurrentCategory(){
 MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formats=numberFormats(window);grid->setCurrentIndex(grid->model()->index(0,0));require(window.executeCommand("cells.format",{{"style",{{"numberFormat","$#,##0.00"}}}}),"seed currency format before cancelled dialog");const auto before=window.currentSession().snapshot();bool opened=false,nativeFormat=false;
 QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){opened=true;nativeFormat=dialog->objectName()=="formatCells.dialog";dialog->reject();}});const auto custom=formats->findText("Custom…");formats->setCurrentIndex(custom);require(QMetaObject::invokeMethod(formats,"activated",Qt::DirectConnection,Q_ARG(int,custom)),"open Custom Format through the ribbon");
 require(opened,"custom formatting dialog opened");require(nativeFormat,"Custom opens the native Format Cells options");require(window.currentSession().snapshot()==before,"cancelling custom formatting preserves workbook and history");require(formats->currentText()=="Currency","cancelling Custom Format must restore the selected cell's actual category");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto test=argc>1?std::string(argv[1]):std::string();if(test.empty()||test=="selection")followsCurrentCell();if(test.empty()||test=="history")followsFormattingAndHistory();if(test.empty()||test=="workbook")followsWorkbookAndSheet();if(test.empty()||test=="custom")cancelledCustomFormatRestoresCurrentCategory();if(test.empty()||test=="date_bar")dateEditingUsesReadableDates();if(test.empty()||test=="format_options")numberOptionsAreAvailableFromHome();if(test.empty()||test=="date_style")formattingKeepsDateEditorCurrent();std::cout<<"number-format ribbon tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
