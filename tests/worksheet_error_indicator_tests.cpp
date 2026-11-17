#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/formula_error_checks.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QScrollBar>
#include <QTest>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void actualBiffSelection(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 const auto source=QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().absoluteFilePath("../tests/fixtures/biff/financial-source.xls");
 require(window.openWorkbook(source),"open the untouched real120-sheet BIFF source");
 auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* name=window.findChild<QLineEdit*>("formula.name");auto* formula=window.findChild<QLineEdit*>("formula.input");
 view->setFocus();for(int i=0;i<32;++i)QTest::keyClick(view,Qt::Key_PageDown,Qt::ControlModifier);QTest::keyClick(view,Qt::Key_F1,Qt::ControlModifier);QApplication::processEvents();
 name->setFocus();QTest::keyClick(name,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(name,"P7");QTest::keyClick(name,Qt::Key_Return);QApplication::processEvents();
 const auto before=window.currentSession().snapshot();const auto history=window.currentSession().historyBytes();const auto cell=model->index(5,18);const auto point=view->visualRect(cell).center();require(view->viewport()->rect().contains(point),"source mergedS6 remains visibly clickable");
 QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,point);QApplication::processEvents();
 require(model->data(cell,FormulaErrorCheckRole).toBool()&&name->text()=="S6"&&formula->text()=="2019","reproduce the genuine selected numeric-text warning without altering its literal contents");
 auto* button=window.findChild<QToolButton*>("worksheet.errorIndicator");
 require(button&&button->isVisible(),"a real graphical yellow error-check button is visible beside selected BIFF numeric-text S6");
 require(!button->icon().isNull()&&!button->accessibleName().isEmpty(),"the warning control exposes a graphical icon and accessible name");
 const auto image=button->icon().pixmap(16,16).toImage();int yellow=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){const auto color=image.pixelColor(x,y);if(color.alpha()>100&&color.red()>170&&color.green()>120&&color.blue()<150)++yellow;}require(yellow>=15,"the actual icon contains the source yellow warning triangle");
 const auto rect=view->visualRect(cell);require(button->size()==QSize(19,20),"the source warning control retains its19x20extent");
 if(qEnvironmentVariable("TIGERSHEETS_ERROR_INDICATOR_CASE")!="corners")require(button->mapTo(view->viewport(),QPoint())==rect.topLeft()-QPoint(1,1)+QPoint(-33,1),"the warning control is anchored to the source selection boundary, not the one-pixel-inset cell content origin");
 const auto control=button->grab().toImage();for(const auto corner:{QPoint(0,0),QPoint(control.width()-1,0),QPoint(0,control.height()-1),QPoint(control.width()-1,control.height()-1)})require(control.pixelColor(corner)==QColor("#b8b8b8"),"all four warning control corners carry the real square border without inherited theme rounding");
 QTest::mouseMove(view->viewport(),QPoint(500,400));QApplication::processEvents();require(button->isVisible(),"the selected warning control persists after the pointer leaves the year");
 require(window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history,"warning selection/painting/pointer movement changes no workbook input, type, style or history");
 if(const auto capture=qEnvironmentVariable("TIGERSHEETS_WARNING_BUTTON_CAPTURE");!capture.isEmpty()){require(window.grab().save(capture),"retain the actual warning control capture");const auto position=button->mapTo(&window,QPoint());const auto cellPosition=view->viewport()->mapTo(&window,rect.topLeft());std::cout<<"Warning button native-window bounds: "<<position.x()<<','<<position.y()<<','<<button->width()<<','<<button->height()<<"; source cell top-left: "<<cellPosition.x()<<','<<cellPosition.y()<<'\n';}
 view->setFocus();QTest::keyClick(view,Qt::Key_F2);QApplication::processEvents();
 require(view->activeCellEditor()&&!button->isVisible(),"editing the flagged text hides its selection warning control without committing the literal");
 QTest::keyClick(view->activeCellEditor(),Qt::Key_Escape);QApplication::processEvents();QApplication::processEvents();
 require(button->isVisible()&&model->raw(5,18)=="'2019","canceling the real literal editor restores the selected warning control");
 QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();auto* menu=window.findChild<QMenu*>("worksheet.errorMenu");
 require(menu&&menu->isVisible(),"clicking the actual warning icon opens an error-check menu");
 require(!menu->actions().empty()&&menu->actions().front()->text()=="Number Stored as Text"&&!menu->actions().front()->isEnabled(),"the menu truthfully identifies the actual warning");
 QTest::keyClick(menu,Qt::Key_Escape);QApplication::processEvents();
 require(!menu->isVisible()&&window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history,"dismissing error checking changes neither the imported literal nor its history");
 QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();menu=window.findChild<QMenu*>("worksheet.errorMenu");auto* edit=menu->findChild<QAction*>("worksheet.error.edit");require(edit&&edit->isEnabled(),"the warning menu offers a working formula-bar edit action");
 const auto editPoint=menu->actionGeometry(edit).center();QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,editPoint);QApplication::processEvents();require(formula->hasFocus()&&formula->cursorPosition()==formula->text().size()&&formula->text()=="2019","the real error-menu edit action focuses the actual literal bar at its end");
 QTest::keyClick(formula,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(formula,"2018");QTest::keyClick(formula,Qt::Key_Escape);QApplication::processEvents();QApplication::processEvents();require(model->raw(5,18)=="'2019"&&window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history,"canceling an actual menu-initiated literal edit preserves its original text type and history");
 require(button->isVisible(),"canceling a menu-initiated formula-bar draft restores the selected warning control");
 bool modalFound=false,visibleWithModal=true;QTimer::singleShot(0,[&]{QApplication::processEvents();if(auto* modal=qobject_cast<QDialog*>(QApplication::activeModalWidget())){modalFound=true;visibleWithModal=button->isVisible();modal->reject();}});view->setFocus();QTest::keyClick(view,Qt::Key_1,Qt::ControlModifier);QApplication::processEvents();QApplication::processEvents();
 require(modalFound&&!visibleWithModal,"the warning control is hidden while an actual Format Cells modal owns the window");
 window.activateWindow();QApplication::processEvents();QApplication::processEvents();require(button->isVisible(),"reactivating the warning workbook after canceling Format Cells restores its selected control");
 QTest::keyClick(view->verticalScrollBar(),Qt::Key_End);QApplication::processEvents();require(!button->isVisible(),"scrolling the selected warning cell outside the viewport hides its control");
 QTest::keyClick(view->verticalScrollBar(),Qt::Key_Home);QApplication::processEvents();require(button->isVisible(),"scrolling back to the visible selected warning restores the control");
 view->setCurrentIndex(model->index(6,15));QApplication::processEvents();require(!button->isVisible(),"selecting ordinary P7 hides the prior cell's warning control");
 view->setCurrentIndex(cell);QApplication::processEvents();require(button->isVisible(),"returning to the still-flagged text restores the warning control");
 {MainWindow other;other.show();other.activateWindow();QApplication::processEvents();QApplication::processEvents();require(!button->isVisible(),"a different active workbook hides the source window's error control");window.activateWindow();QApplication::processEvents();QApplication::processEvents();require(button->isVisible(),"reactivating the selected warning workbook restores its own error control");}
 require(window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history,"editing cancellation, menus, ordinary selection and workbook activation preserve the source exactly");
}
void ignoredWarnings(){
 QTemporaryDir files;auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"]=Json::array({{{"row",1},{"col",1},{"input","'2021"}},{{"row",2},{"col",2},{"input","'2020"}},{{"row",3},{"col",3},{"input","2022"}}});
 book["sheets"][0]["ignoredErrors"]=Json::array({{{"ranges",Json::array({{{"row1",1},{"col1",1},{"row2",1},{"col2",1}}})},{"checks",{{"numberStoredAsText",true}}}}});
 const auto path=files.filePath("native-ignored-warning.xlsx");writeWorkbook(path.toStdString(),book);
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();require(window.openWorkbook(path),"open native ignored-error flags from an actual workbook package");QApplication::processEvents();
 auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* button=window.findChild<QToolButton*>("worksheet.errorIndicator");const auto before=window.currentSession().snapshot();const auto history=window.currentSession().historyBytes();
 view->setCurrentIndex(model->index(0,0));QApplication::processEvents();require(!model->data(model->index(0,0),FormulaErrorCheckRole).toBool()&&!button->isVisible(),"a native explicitly ignored numeric-text cell receives no warning button");
 view->setCurrentIndex(model->index(1,1));QApplication::processEvents();require(button->isVisible(),"an adjacent unignored numeric-text warning remains available");
 view->setCurrentIndex(model->index(2,2));QApplication::processEvents();require(!button->isVisible(),"an actual numeric year receives no number-stored-as-text control");
 require(window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history,"checking native ignore flags and actual number types preserves cells, ignore metadata and history");
}
}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{actualBiffSelection();ignoredWarnings();std::cout<<"Native warning indicator passed\n";}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
