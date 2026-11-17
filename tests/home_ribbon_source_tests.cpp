#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ribbon_page.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFontComboBox>
#include <QLabel>
#include <QPainter>
#include <QMenu>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableView>
#include <QTabWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct Fixture{MainWindow window;QTableView* grid;WorksheetModel* model;QTabWidget* ribbon;Fixture(){window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();grid=window.findChild<QTableView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(grid->model());ribbon=window.findChild<QTabWidget*>("workspace.ribbon");for(int c=2;c<=4;++c)require(model->apply(Json::array({{{"type","set_cell"},{"sheet",model->sheetId().toStdString()},{"row",7},{"col",c+1},{"input",std::to_string(.14+c*.01)},{"style",{{"numberFormat","0%"},{"color","#0000ff"},{"fontSize",12}}}}})),"prepare source percentage selection");grid->setCurrentIndex(model->index(6,2));grid->selectionModel()->select(QItemSelection(model->index(6,2),model->index(6,4)),QItemSelectionModel::ClearAndSelect);window.executeCommand("view.ribbon");grid->setFocus();QApplication::processEvents();}void open(){grid->setFocus();QTest::keyClick(grid,Qt::Key_Alt);QTest::keyClick(grid,Qt::Key_H);QApplication::processEvents();}QStackedWidget* stack(){return ribbon->findChild<QStackedWidget*>();}};
void requireTipAtControl(Fixture& f,const char* key){
 auto* tip=f.window.findChild<QLabel*>(QString("tutorial.keyTip.")+key);QWidget* control=nullptr;
 for(auto* candidate:f.ribbon->currentWidget()->findChildren<QWidget*>())if(candidate->property("ribbon.keyTip").toString()==key){control=candidate;break;}
 require(tip&&tip->isVisible()&&control,"compact Home retains each native command keytip");
 require(!control->property("ribbon.keyTipPoint").isValid(),"compact keytips do not retain old multi-row pixel offsets");
 require(tip->mapToGlobal(QPoint()).y()==control->mapToGlobal(QPoint(0,control->height()-4)).y(),"Home keytips follow the bottom of their actual compact controls");
 require(tip->visibleRegion().boundingRect()==tip->rect(),"compact Home keytips remain fully visible");
}
void geometry(){
 Fixture f;f.window.resize(2100,720);QApplication::processEvents();const auto before=f.window.currentSession().snapshot();const auto selection=f.grid->selectionModel()->selection();const auto body=f.grid->viewport()->mapTo(&f.window,QPoint());f.open();
 const auto tabBottom=f.ribbon->tabBar()->mapToGlobal(QPoint(0,f.ribbon->tabBar()->height()));
 require(f.stack()->height()==36&&f.stack()->mapToGlobal(QPoint()).y()==tabBottom.y()-2,"Home opens one compact command row directly below its tabs");
 const char* groups[]={"clipboard","font","alignment","number","styles","cells","editing","analysis"};int previousRight=-1,rowTop=-1;
 for(const auto* id:groups){
  auto* panel=f.window.findChild<QWidget*>(QString("home.group.")+id);require(panel&&panel->isVisible(),"compact Home retains all eight native command groups");
  const auto at=panel->geometry();if(rowTop<0)rowTop=at.top();require(at.top()==rowTop&&at.left()>previousRight&&at.height()==32,"Home groups share a single nonoverlapping compact row");previousRight=at.right();
  for(auto* caption:panel->findChildren<QLabel*>())require(!caption->isVisible(),"Home groups do not add a visible caption row");
 }
 for(const auto* name:{"formatPainter","growFont","shrinkFont","borders","verticalTop","verticalCenter","verticalBottom","orientation","mergeCenter","accounting","comma","conditionalFormatting","formatAsTable","cellStyles","insert","delete","format","clear","sortFilter","findSelect","analyzeData"}){
  auto* button=f.window.findChild<QToolButton*>(QString("home.")+name);require(button&&button->isVisible()&&!button->icon().isNull(),"every Home group retains graphical native controls");
  require(button->toolButtonStyle()==Qt::ToolButtonIconOnly&&!button->toolTip().isEmpty(),"Home commands use icon-only controls with hover descriptions");
 }
 for(const auto* key:{"FF","FS","FG","FK","1","2","3","B","H","FC","AT","AM","AB","FQ","W","AL","AC","AR","5","6","M","AN","P","K","0","9","L","T","J","I","D","O","U","FI","E","S","FD"}){
  auto* badge=f.window.findChild<QLabel*>(QString("tutorial.keyTip.")+key);require(badge&&badge->isVisible()&&badge->styleSheet().contains("#5b5b5b"),"compact Home keeps the complete native control keytips including 0 and 9");
 }
 requireTipAtControl(f,"FF");requireTipAtControl(f,"0");
 if(const auto path=qEnvironmentVariable("TIGERSHEETS_HOME_SOURCE_CAPTURE");!path.isEmpty()){
  auto frame=f.window.grab();QPainter painter(&frame);painter.drawPixmap(f.stack()->mapToGlobal(QPoint())-f.window.mapToGlobal(QPoint()),f.stack()->grab());for(auto* tip:f.window.findChildren<QLabel*>())if(tip->isWindow()&&tip->isVisible()&&tip->objectName().startsWith("tutorial.keyTip."))painter.drawPixmap(tip->mapToGlobal(QPoint())-f.window.mapToGlobal(QPoint()),tip->grab());painter.end();require(frame.save(path),"retain actual compact Home capture");
 }
 QTest::keyClick(f.grid,Qt::Key_Escape);QApplication::processEvents();require(f.window.currentSession().snapshot()==before&&f.grid->selectionModel()->selection()==selection&&f.grid->viewport()->mapTo(&f.window,QPoint())==body,"Home overlay closes without moving the worksheet or changing selection/history");
}
void tips(){Fixture f;f.window.resize(2100,720);QApplication::processEvents();f.open();for(const auto* key:{"FF","0","FN","FA","FM","FO"})requireTipAtControl(f,key);}
void borders(){
 Fixture f;const auto before=f.window.currentSession().snapshot();const auto selection=f.grid->selectionModel()->selection();f.open();
 auto* button=f.window.findChild<QToolButton*>("home.borders");require(button&&button->isVisible()&&button->isEnabled(),"Home Borders is an enabled command rather than a disabled placeholder");
 QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();auto* menu=f.window.findChild<QMenu*>("tutorial.borderMenu");require(menu&&menu->isVisible()&&QApplication::activePopupWidget()==menu,"clicking Home Borders opens the actual border command menu");
 QAction* all=nullptr;for(auto* action:menu->actions())if(QString(action->text()).remove('&').trimmed()=="All Borders")all=action;require(all,"the Home border menu exposes All Borders");
 QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(all).center());QApplication::processEvents();
 for(int column=2;column<=4;++column){const auto style=f.model->cellStyle(6,column);require(style.contains("border"),"the Home menu formats every selected cell");for(const auto* edge:{"top","bottom","left","right"})require(style.at("border").contains(edge),"All Borders applies each edge through the real Home menu");}
 require(f.grid->selectionModel()->selection()==selection&&f.window.executeCommand("undo")&&f.window.currentSession().snapshot().at("sheets")==before.at("sheets"),"Home Borders is one undoable edit and preserves the original selection");
 f.open();QTest::keyClick(f.grid,Qt::Key_B);QApplication::processEvents();require(menu->isVisible(),"Alt H B opens the same working Home border menu");QTest::keyClick(menu,Qt::Key_Escape);QApplication::processEvents();require(f.window.currentSession().snapshot().at("sheets")==before.at("sheets"),"dismissing the Home border menu never edits the workbook");
}
void label(){Fixture f;auto* format=f.window.findChild<QComboBox*>("ribbon.numberFormat");require(format&&format->currentText()=="Percentage","selected percentage cells display the exact source Percentage category");}
void accounting(){Fixture f;const auto before=f.window.currentSession().snapshot();f.open();auto* button=f.window.findChild<QToolButton*>("home.accounting");require(button&&button->isEnabled(),"source Home Accounting graphical control uses the real existing format command");QTest::mouseClick(button,Qt::LeftButton);QApplication::processEvents();require(f.model->cellStyle(6,2).value("numberFormat",std::string()).find("* #,##0.00")!=std::string::npos&&f.model->cellStyle(6,4).value("color",std::string())=="#0000ff","Home Accounting formats the selected range and preserves the font");require(f.window.executeCommand("undo")&&f.window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"Home Accounting remains one native Undo");}
void workflows(){Fixture f;const auto selection=f.grid->selectionModel()->selection();f.open();QTest::keyClick(f.grid,Qt::Key_0);QApplication::processEvents();require(f.model->cellStyle(6,2).value("numberFormat",std::string())=="0.0%"&&f.model->cellStyle(6,4).value("numberFormat",std::string())=="0.0%","Alt H0 still changes native precision for the whole source selection");f.open();QTest::keyClick(f.grid,Qt::Key_9);QApplication::processEvents();require(f.model->cellStyle(6,2).value("numberFormat",std::string())=="0%"&&f.grid->selectionModel()->selection()==selection,"Alt H9 still reverses precision and preserves selection");f.open();auto* family=f.window.findChild<QFontComboBox*>("ribbon.fontFamily");auto* size=f.window.findChild<QSpinBox*>("ribbon.fontSize");require(family&&size,"existing native font controls remain present");size->setValue(14);QApplication::processEvents();require(f.model->cellStyle(6,2).value("fontSize",0)==14&&f.model->cellStyle(6,4).value("fontSize",0)==14,"rearranged font size preserves the original selection callback");}
void launcher(){Fixture f;const auto before=f.window.currentSession().snapshot();const auto selection=f.grid->selectionModel()->selection();for(const auto& test:{std::pair{Qt::Key_N,2},std::pair{Qt::Key_A,1},std::pair{Qt::Key_M,0}}){f.open();QTest::keyClick(f.grid,Qt::Key_F);QApplication::processEvents();require(f.window.findChild<QLabel*>("tutorial.keyTip.FN")&&f.stack()->isVisible(),"Home F prefix preserves the popup and matching native launcher keytips");int initial=-1;QTimer::singleShot(0,&f.window,[&]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){if(auto* tabs=dialog->findChild<QTabWidget*>("formatCells.tabs"))initial=tabs->currentIndex();dialog->reject();}});QTest::keyClick(f.grid,test.first);QApplication::processEvents();require(initial==test.second,"Alt H F N/A/M opens the real Font/Alignment/Number Format Cells page");}require(f.window.currentSession().snapshot()==before&&f.grid->selectionModel()->selection()==selection,"launcher cancellation preserves the native worksheet and history");}
void pointer(){Fixture f;f.open();auto* button=f.window.findChild<QToolButton*>("home.accounting");require(button&&button->isVisible(),"native Home Accounting button is visible before pointer input");QTest::mousePress(button,Qt::LeftButton);QApplication::processEvents();require(button->isVisible()&&button->isDown(),"Home popup remains available between the physical pointer press and release");QTest::mouseRelease(button,Qt::LeftButton);QApplication::processEvents();require(f.model->cellStyle(6,2).value("numberFormat",std::string()).find("* #,##0.00")!=std::string::npos,"the physical Home pointer release invokes the real format callback");}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const std::string mode=argc>1?argv[1]:"";if(mode=="tips")tips();else if(mode=="borders")borders();else if(mode=="label")label();else if(mode=="accounting")accounting();else if(mode=="workflows")workflows();else if(mode=="launcher")launcher();else if(mode=="pointer")pointer();else geometry();std::cout<<"Settled Home source controls passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
