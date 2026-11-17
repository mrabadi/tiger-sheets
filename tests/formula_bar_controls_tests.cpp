#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QTabBar>
#include <QTemporaryDir>
#include <QToolButton>
#include <QMenu>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct Surface {
 MainWindow window;WorksheetView* view;WorksheetModel* model;QLineEdit* bar;QLineEdit* name;
 Surface(){window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();view=window.findChild<WorksheetView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(view->model());bar=window.findChild<QLineEdit*>("formula.input");name=window.findChild<QLineEdit*>("formula.name");window.executeCommand("view.ribbon");QApplication::processEvents();}
 QToolButton* button(const char* key){auto* value=window.findChild<QToolButton*>(key);require(value,"source formula bar graphical Cancel and Enter controls exist");return value;}
 void draft(const QString& text){window.activateWindow();bar->setFocus();bar->selectAll();QTest::keyClicks(bar,text);QApplication::processEvents();}
};
void sourceGeometryAndLocalButtons(){
 Surface f;auto* cancel=f.button("formula.cancel");auto* accept=f.button("formula.accept");auto* fx=f.button("formula.insertFunction");auto position=[&](QWidget* widget){return widget->mapTo(&f.window,QPoint());};require(position(f.name)==QPoint(7,74)&&f.name->size()==QSize(102,26)&&position(f.bar)==QPoint(253,74),"Name Box and formula field retain x7/253 and recover36pixels after the duplicate row is removed");require(!cancel->icon().isNull()&&!accept->icon().isNull()&&!fx->icon().isNull()&&!cancel->isEnabled()&&!accept->isEnabled(),"source formula actions have icons and idle Cancel/Enter are disabled");require(f.bar->placeholderText().isEmpty(),"source blank formula bar has no placeholder text");const auto anchor=f.model->index(1,1);f.view->setCurrentIndex(anchor);const auto before=f.window.currentSession().snapshot();f.draft("=1+2");require(cancel->isEnabled()&&accept->isEnabled(),"typing a draft enables the formula actions");QTest::mouseClick(cancel,Qt::LeftButton);QApplication::processEvents();require(f.window.currentSession().snapshot()==before&&f.bar->text().isEmpty()&&f.view->currentIndex()==anchor&&!cancel->isEnabled(),"Cancel restores the original cell without accepting its draft or moving its anchor");f.draft("=1+2");QTest::mouseClick(accept,Qt::LeftButton);QApplication::processEvents();f.window.recalculateNow();require(f.model->raw(1,1)=="=1+2"&&f.model->data(anchor).toString()=="3"&&f.view->currentIndex()==anchor&&f.view->hasFocus(),"formula-bar Enter accepts the draft and retains B2");require(f.window.executeCommand("undo")&&f.window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"the Enter button creates exactly one undoable cell edit");
 // The actual inline delegate must keep its draft through a NoFocus button click.
 f.view->setFocus();QTest::keyClicks(f.view,"7");QApplication::processEvents();require(f.view->activeCellEditor()&&cancel->isEnabled(),"an inline cell draft enables the same controls");QTest::mouseClick(cancel,Qt::LeftButton);QApplication::processEvents();require(f.model->raw(1,1).isEmpty()&&!f.view->activeCellEditor(),"Cancel discards an actual inline editor without a focus-out commit");
}
void nameBoxList(){
 Surface f;auto* arrow=f.window.findChild<QToolButton*>("formula.nameList");require(arrow&&!arrow->icon().isNull()&&arrow->isVisible(),"the source Name Box has a real graphical dropdown arrow");require(arrow->parentWidget()==f.name&&arrow->focusPolicy()==Qt::NoFocus,"the source dropdown remains within the existing Name Box without moving keyboard focus");
 const auto id=f.model->sheetId().toStdString();require(f.model->apply(Json::array({{{"type","set_metadata"},{"key","names"},{"value",{{"SourceRange","'Sheet1'!$C$7:$E$7"}}}}})),"native named-range fixture");const auto before=f.window.currentSession().snapshot();f.view->setFocus();QTest::mouseClick(arrow,Qt::LeftButton);QApplication::processEvents();auto* menu=f.window.findChild<QMenu*>("formula.nameList.menu");require(menu&&menu->isVisible()&&menu->actions().size()==1&&menu->actions().front()->text()=="SourceRange","the Name Box dropdown reads the actual workbook names");QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions().front()).center());QApplication::processEvents();const auto ranges=f.view->selectionModel()->selection();require(f.view->currentIndex()==f.model->index(6,2)&&ranges.size()==1&&ranges.front().bottomRight()==f.model->index(6,4)&&f.view->hasFocus(),"choosing the actual named range navigates and selects C7:E7");require(f.window.currentSession().snapshot()==before,"Name Box navigation leaves workbook inputs and history unchanged");
}
void externalButtons(){
 QTemporaryDir files;Surface origin,source;require(source.model->setData(source.model->index(1,1),"42",Qt::EditRole)&&source.window.saveWorkbook(files.filePath("Control Source.xlsx")),"name the actual external source");const auto originBefore=origin.window.currentSession().snapshot();const auto sourceBefore=source.window.currentSession().snapshot();
 auto point=[&]{origin.draft("=");QTest::keyClick(origin.bar,Qt::Key_Tab,Qt::ControlModifier);QTest::keyRelease(source.bar,Qt::Key_Control);QApplication::processEvents();QTest::mouseClick(source.view->viewport(),Qt::LeftButton,Qt::NoModifier,source.view->visualRect(source.model->index(1,1)).center());QApplication::processEvents();};
 point();QTest::mouseClick(source.button("formula.cancel"),Qt::LeftButton);QApplication::processEvents();require(origin.window.currentSession().snapshot()==originBefore&&source.window.currentSession().snapshot()==sourceBefore&&origin.bar->text().isEmpty(),"Cancel from the pointed source restores both documents without a link edit");point();QTest::mouseClick(source.button("formula.accept"),Qt::LeftButton);QApplication::processEvents();origin.window.recalculateNow();require(origin.model->data(origin.model->index(0,0)).toString()=="42"&&origin.view->currentIndex()==origin.model->index(0,0)&&source.window.currentSession().snapshot()==sourceBefore,"Enter from the external source commits to the original A1, retains its anchor and preserves source data");require(origin.window.executeCommand("undo")&&origin.window.currentSession().snapshotView().at("sheets")==originBefore.at("sheets"),"external formula and link metadata remain one native Undo");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{sourceGeometryAndLocalButtons();nameBoxList();externalButtons();std::cout<<"Source formula bar controls passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
