#include "tigersheets/main_window.hpp"
#include "tigersheets/chart.hpp"
#include "tigersheets/codex.hpp"
#include "codex_preview_test.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace tigersheets;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<ChartWidget*> charts(MainWindow& window){std::vector<ChartWidget*> result;for(auto* widget:window.findChildren<QWidget*>())if(auto* chart=dynamic_cast<ChartWidget*>(widget))result.push_back(chart);return result;}
void show(MainWindow& window){window.loadFinancialExample();window.show();QApplication::processEvents();require(charts(window).size()==1,"financial example must contain its chart");}
void exclusiveSelection(){
 MainWindow window;show(window);auto book=window.currentSession().snapshot();auto figures=book["charts"];auto second=figures[0];second["id"]="second";second["anchor"]["row"]=3;second["anchor"]["col"]=6;figures.push_back(second);require(window.currentSession().apply(book["revision"],Json::array({{{"type","set_metadata"},{"key","charts"},{"value",figures}}})),"second chart setup must apply");window.recalculateNow();auto figuresUI=charts(window);require(figuresUI.size()==2,"both charts must be visible");
 QTest::mouseClick(figuresUI[0],Qt::LeftButton,Qt::NoModifier,QPoint(80,25));QTest::mouseClick(figuresUI[1],Qt::LeftButton,Qt::NoModifier,QPoint(80,25));
 require(!figuresUI[0]->isSelected()&&figuresUI[1]->isSelected(),"selecting another chart must remove the previous selection handles");
}
void gridDeselect(){
 MainWindow window;show(window);auto* chart=charts(window).front();auto* grid=window.findChild<QTableView*>("worksheet.grid");QTest::mouseClick(chart,Qt::LeftButton,Qt::NoModifier,QPoint(80,25));require(chart->isSelected(),"chart must be selected before returning to cells");QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualRect(grid->model()->index(0,0)).center());require(!chart->isSelected(),"clicking a worksheet cell must remove chart selection handles before cell commands run");
}
void cancelDialog(){
 MainWindow window;show(window);const auto before=window.currentSession().snapshot();bool visited=false;
 QTimer::singleShot(0,&window,[&]{if(auto* dialog=window.findChild<QDialog*>("chartEditor")){visited=true;dialog->findChild<QLineEdit*>("chartTitle")->setText("Canceled title");dialog->findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Cancel)->click();}});
 QTest::mouseDClick(charts(window).front(),Qt::LeftButton,Qt::NoModifier,QPoint(80,25));require(visited,"chart double click must open the native chart editor");require(window.currentSession().snapshot()==before,"canceling chart edits must preserve revision and metadata");
}
void resizeAndCancel(){
 MainWindow window;show(window);auto* chart=charts(window).front();QTest::mouseClick(chart,Qt::LeftButton,Qt::NoModifier,QPoint(80,25));const auto before=window.currentSession().snapshot();const auto geometry=chart->geometry();const QPoint corner(chart->width()-5,chart->height()-5);QTest::mousePress(chart,Qt::LeftButton,Qt::NoModifier,corner);const auto target=corner+QPoint(50,35);QMouseEvent move(QEvent::MouseMove,QPointF(target),QPointF(chart->mapToGlobal(target)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(chart,&move);require(chart->size()==geometry.size()+QSize(50,35),"chart corner must resize live");QTest::keyClick(chart,Qt::Key_Escape);QTest::mouseRelease(chart,Qt::LeftButton,Qt::NoModifier,target);require(chart->geometry()==geometry&&window.currentSession().snapshot()==before,"Escape must cancel chart resize without creating an undo transaction");
 QTest::mousePress(chart,Qt::LeftButton,Qt::NoModifier,corner);QTest::mouseRelease(chart,Qt::LeftButton,Qt::NoModifier,target);const auto after=window.currentSession().snapshot();require(after["charts"][0]["anchor"]["width"]==geometry.width()+50&&after["charts"][0]["anchor"]["height"]==geometry.height()+35,"released chart resize must persist its dimensions");require(window.executeCommand("undo")&&window.currentSession().snapshot()["charts"]==before["charts"],"resizing must be one undo transaction");
}
void typeRangeSave(){
 MainWindow window;show(window);const auto before=window.currentSession().snapshot();bool visited=false;
 QTimer::singleShot(0,&window,[&]{if(auto* dialog=window.findChild<QDialog*>("chartEditor")){visited=true;auto* type=dialog->findChild<QComboBox*>("chartType");type->setCurrentIndex(type->findData("bar"));dialog->findChild<QLineEdit*>("chartRange")->setText("A20:C24");dialog->findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Ok)->click();}});
 QTest::mouseDClick(charts(window).front(),Qt::LeftButton,Qt::NoModifier,QPoint(80,25));require(visited,"chart type/range editor must open");const auto after=window.currentSession().snapshot();require(after["charts"][0]["type"]=="bar"&&after["charts"][0]["range"]["row2"]==24&&after["sheets"]==before["sheets"],"chart type and source range must update together without modifying cells");QTemporaryDir directory;const auto path=directory.filePath("edited-chart.xlsx");require(window.saveWorkbook(path),"edited chart must save");const auto saved=readWorkbook(path.toStdString()).workbook;require(saved["charts"][0]["type"]=="bar"&&saved["charts"][0]["range"]["row2"]==24,"saved chart must retain its type and changed source range");require(window.executeCommand("undo")&&window.currentSession().snapshot()["charts"]==before["charts"],"type/range edit must be one undo transaction");
}
void previewReadonly(){
 MainWindow window;show(window);const auto before=window.currentSession().snapshot();auto proposal=before["charts"];proposal[0]["title"]="Preview chart";const auto result=test::isolatedPreview(window,{{"revision",before["revision"]},{"operations",Json::array({{{"type","set_metadata"},{"key","charts"},{"value",proposal}}})}});require(result.value("status",std::string())=="preview","chart metadata proposal must be shown");auto* chart=charts(window).front();QTest::mouseClick(chart,Qt::LeftButton,Qt::NoModifier,QPoint(80,25));QTest::keyClick(chart,Qt::Key_Delete);require(window.currentSession().snapshot()==before&&window.currentSession().hasPreview(),"preview chart Delete must not change the canonical workbook or accept the proposal");QTest::keyClick(chart,Qt::Key_F2);require(!window.findChild<QDialog*>("chartEditor"),"preview chart must not open a writable editor");const auto geometry=chart->geometry();QTest::mousePress(chart,Qt::LeftButton,Qt::NoModifier,QPoint(80,25));QTest::mouseRelease(chart,Qt::LeftButton,Qt::NoModifier,QPoint(105,45));require(window.currentSession().snapshot()==before&&chart->geometry()==geometry,"preview chart movement must not persist or displace the proposal");require(window.executeCommand("preview.discard"),"preview must remain discardable");
}
void previewReadonlyShortcut(){
 MainWindow window;show(window);const auto before=window.currentSession().snapshot();auto proposal=before["charts"];proposal[0]["title"]="Readonly chart preview";const auto result=test::isolatedPreview(window,{{"revision",before["revision"]},{"operations",Json::array({{{"type","set_metadata"},{"key","charts"},{"value",proposal}}})}});require(result.value("status",std::string())=="preview","chart proposal must be shown before readonly shortcut regression");
 auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");grid->setCurrentIndex(grid->model()->index(4,4));formula->setFocus();formula->setCursorPosition(3);const auto text=formula->text();require(formula->isReadOnly()&&text.startsWith('='),"preview formula must be readonly");QTest::keyClick(formula,Qt::Key_F4);require(formula->text()==text&&!formula->isModified(),"F4 must not modify a readonly preview formula or leave a pending edit");
 bool visited=false;QTimer::singleShot(0,&window,[&]{if(auto* dialog=window.findChild<QInputDialog*>()){visited=true;dialog->reject();}});const bool inserted=window.executeCommand("function.insert");QApplication::processEvents();require(!inserted&&!visited&&formula->text()==text&&!formula->isModified(),"Insert Function must not open a writable dialog or modify a readonly preview formula");
 QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualRect(grid->model()->index(0,0)).center());require(window.currentSession().snapshot()==before&&window.currentSession().hasPreview(),"leaving a readonly formula must preserve the workbook and active proposal");require(window.executeCommand("preview.discard"),"readonly proposal must remain discardable");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto selected=argc>1?std::string(argv[1]):std::string();for(auto test:{std::pair{"selection",exclusiveSelection},std::pair{"grid",gridDeselect},std::pair{"cancel",cancelDialog},std::pair{"resize",resizeAndCancel},std::pair{"edit",typeRangeSave},std::pair{"preview",previewReadonly},std::pair{"readonly",previewReadonlyShortcut}})if(selected.empty()||selected==test.first){test.second();std::cout<<test.first<<" passed\n";}std::cout<<"chart interaction tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
