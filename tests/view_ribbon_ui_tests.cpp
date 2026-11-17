#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ribbon_page.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QInputDialog>
#include <QHeaderView>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSlider>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTest>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using namespace tigersheets;
namespace {
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
struct Fixture{MainWindow window;QTableView* grid;WorksheetModel* model;QTabWidget* ribbon;Fixture(){window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();grid=window.findChild<QTableView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(grid->model());ribbon=window.findChild<QTabWidget*>("workspace.ribbon");grid->setCurrentIndex(model->index(3,0));window.executeCommand("view.ribbon");grid->setFocus();}void open(Qt::Key tab=Qt::Key_W){grid->setFocus();QTest::keyClick(grid,Qt::Key_Alt);QTest::keyClick(grid,tab);QApplication::processEvents();}QStackedWidget* stack(){return ribbon->findChild<QStackedWidget*>();}void capture(){if(const auto path=qEnvironmentVariable("TIGERSHEETS_VIEW_CAPTURE");!path.isEmpty()){auto frame=window.grab();QPainter p(&frame);p.drawPixmap(stack()->mapToGlobal(QPoint())-window.mapToGlobal(QPoint()),stack()->grab());p.end();require(frame.save(path),"save real View popup capture");}}};
void geometry(bool home){
 Fixture f;const auto before=f.window.currentSession().snapshot();const auto selection=f.grid->selectionModel()->selection();f.open(home?Qt::Key_H:Qt::Key_W);f.capture();
 require(f.window.property("ribbon.peek").toBool()&&f.stack()->isVisible()&&f.stack()->height()==36,"Home/View popup has one compact row of native commands");
 const auto tabBottom=f.ribbon->tabBar()->mapToGlobal(QPoint(0,f.ribbon->tabBar()->height()));require(f.stack()->mapToGlobal(QPoint()).y()==tabBottom.y()-2,"compact ribbon popup remains anchored below the tabs");
 if(!home){
  const char* groups[]={"sheetView","workbookViews","show","zoom","window","macros"};int previousRight=-1,rowTop=-1;
  for(const auto* id:groups){
   auto* panel=f.window.findChild<QWidget*>(QString("view.group.")+id);require(panel&&panel->isVisible(),"compact View retains every native command group");
   const auto at=panel->geometry();if(rowTop<0)rowTop=at.top();require(at.top()==rowTop&&at.left()>previousRight&&at.height()==32,"View groups share a single nonoverlapping compact row");previousRight=at.right();
   for(auto* caption:panel->findChildren<QLabel*>())require(!caption->isVisible(),"View group captions do not add another command row");
  }
  for(const auto* id:{"normal","pageBreakPreview","pageLayout","customViews","navigation","zoom","zoom100","zoomSelection","newWindow","arrangeAll","freezePanes","split","hide","unhide","switchWindows","macros"}){
   auto* button=f.window.findChild<QToolButton*>(QString("view.")+id);require(button&&button->isVisible()&&!button->icon().isNull(),"each View command retains its native graphical control");
   require(button->toolButtonStyle()==Qt::ToolButtonIconOnly&&!button->toolTip().isEmpty(),"View commands use compact icons with descriptive hover text");
  }
  auto* navigation=f.window.findChild<QToolButton*>("view.navigation");require(navigation->accessibleName()=="Navigation"&&navigation->toolTip().startsWith("Navigation"),"Navigation keeps its complete command name in accessibility and hover text");
  for(const auto* key:{"VF","VG","VH","Q","J","G","F","W"}){auto* tip=f.window.findChild<QLabel*>(QString("tutorial.keyTip.")+key);require(tip&&tip->isVisible(),"compact View retains all native command keytips");}
 }
 QTest::keyClick(f.grid,Qt::Key_Escape);QApplication::processEvents();require(!f.window.property("ribbon.peek").toBool()&&f.grid->selectionModel()->selection()==selection&&f.window.currentSession().snapshot()==before,"Escape closes the overlay without changing cells, selection or history");
}
void initialHeight(){
 MainWindow window;window.resize(1410,720);window.show();QApplication::processEvents();
 auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto before=window.currentSession().snapshot();
 for(const auto& entry:{std::pair{"undo","Undo"},std::pair{"redo","Redo"}}){auto* button=window.findChild<QToolButton*>(QString("command.")+entry.first);require(button&&button->toolTip()==entry.second&&button->accessibleName()==entry.second,"header icon commands expose full hover and accessible names");}
 for(const int width:{1410,820,2100}){
  window.resize(width,720);QApplication::processEvents();
  for(int tab=0;tab<ribbon->count();++tab){
   ribbon->setCurrentIndex(tab);QApplication::processEvents();
   auto* page=dynamic_cast<RibbonPage*>(ribbon->currentWidget());require(page,"every ribbon tab uses the common responsive page");
   if(ribbon->height()>ribbon->tabBar()->height()+36)std::cerr<<"Tall ribbon: "<<ribbon->tabText(tab).toStdString()<<" height="<<ribbon->height()<<'\n';
   require(ribbon->height()<=ribbon->tabBar()->height()+36,"every ribbon keeps one compact command row at wide and narrow widths");
   int center=-1;for(auto* button:page->widget()->findChildren<QToolButton*>()){
    require(button->toolButtonStyle()==Qt::ToolButtonIconOnly&&(!button->icon().isNull()||button->arrowType()!=Qt::NoArrow),"every ribbon command displays a graphical icon or native menu arrow without a visible word label");
    require(!button->toolTip().trimmed().isEmpty()&&!button->accessibleName().trimmed().isEmpty(),"icon commands retain descriptive hover and accessible names");
    require(button->toolTip()==button->accessibleName(),"hover names stay accurate when a command's enabled state changes");
    if(button->width()<24||button->height()<24||button->height()>30)std::cerr<<button->objectName().toStdString()<<" "<<button->text().toStdString()<<" size="<<button->width()<<","<<button->height()<<" minmax="<<button->minimumHeight()<<","<<button->maximumHeight()<<" panel="<<button->parentWidget()->height()<<" page="<<page->height()<<"\n";require(button->width()>=24&&button->height()>=24&&button->height()<=30,"compact commands retain usable targets");
    const auto at=button->mapTo(page->widget(),button->rect().center());if(center<0)center=at.y();require(std::abs(at.y()-center)<=1,"all command buttons share a single horizontal row");
    require(button->mapTo(page,QPoint()).y()>=0&&button->mapTo(page,QPoint(0,button->height())).y()<=page->height(),"compact rows never clip controls vertically");
   }
   for(auto* box:page->findChildren<QCheckBox*>())require(box->text().isEmpty()&&!box->icon().isNull()&&!box->toolTip().isEmpty(),"view toggles use icons, state indicators, and hover names");
   for(auto* label:page->findChildren<QLabel*>())require(!label->isVisible(),"ribbon groups do not add a caption row under commands");
   if(const auto folder=qEnvironmentVariable("TIGERSHEETS_COMPACT_CAPTURE_DIR");!folder.isEmpty()&&width!=2100)require(window.grab().save(folder+"/"+QString::number(width)+"-"+ribbon->tabText(tab).replace(' ','-')+".png"),"capture actual compact ribbon");
  }
 }
 ribbon->setCurrentIndex(0);const auto height=ribbon->height();grid->setFocus();QTest::keyClick(grid,Qt::Key_F1,Qt::ControlModifier);QTest::keyClick(grid,Qt::Key_F1,Qt::ControlModifier);QApplication::processEvents();require(ribbon->height()==height&&window.currentSession().snapshot()==before,"compact ribbon and Ctrl F1 preserve authored workbook content");
}
void display(){Fixture f;const auto before=f.window.currentSession().snapshot();const auto selection=f.grid->selectionModel()->selection();f.open();auto* lines=f.window.findChild<QCheckBox*>("view.gridlines");auto* bar=f.window.findChild<QCheckBox*>("view.formulaBar");auto* headings=f.window.findChild<QCheckBox*>("view.headings");require(lines&&bar&&headings&&lines->isChecked()&&bar->isChecked()&&headings->isChecked(),"source Show group contains actual checked Gridlines/Formula Bar/Headings controls");QTest::mouseClick(lines,Qt::LeftButton);QApplication::processEvents();require(!f.grid->showGrid()&&!lines->isChecked()&&f.grid->selectionModel()->selection()==selection,"native Gridlines checkbox toggles persisted gridlines and keeps A4 selected");require(f.window.currentSession().snapshotView().at("revision")==before.at("revision").get<int>()+1,"Gridlines is one real native layout edit");f.open();QTest::keyClick(f.grid,Qt::Key_V);QTest::keyClick(f.grid,Qt::Key_G);QApplication::processEvents();require(f.grid->showGrid()&&lines->isChecked(),"existing Alt W V G operates the source checkbox and synchronizes native grid state");f.open();QTest::mouseClick(bar,Qt::LeftButton);QApplication::processEvents();require(!f.window.findChild<QLineEdit*>("formula.input")->parentWidget()->isVisible()&&!bar->isChecked(),"Formula Bar checkbox actually hides the native formula controls");f.open();QTest::mouseClick(bar,Qt::LeftButton);f.open();QTest::mouseClick(headings,Qt::LeftButton);QApplication::processEvents();require(!f.grid->horizontalHeader()->isVisible()&&!f.grid->verticalHeader()->isVisible(),"Headings checkbox actually hides native row and column headers");f.open();QTest::mouseClick(headings,Qt::LeftButton);QApplication::processEvents();require(f.grid->horizontalHeader()->isVisible()&&f.grid->verticalHeader()->isVisible()&&f.grid->selectionModel()->selection()==selection,"view controls restore without changing cell selection");}
void zoom(){Fixture f;const auto before=f.window.currentSession().snapshot();f.open();auto* zoom=f.window.findChild<QToolButton*>("view.zoom");auto* hundred=f.window.findChild<QToolButton*>("view.zoom100");auto* slider=f.window.findChild<QSlider*>("worksheet.zoom");require(zoom&&hundred&&slider,"source View exposes working Zoom and 100 percent controls");bool opened=false;QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())){opened=true;dialog->setIntValue(150);dialog->accept();}});QTest::mouseClick(zoom,Qt::LeftButton);QApplication::processEvents();require(opened&&slider->value()==150,"native Zoom dialog applies the requested worksheet magnification");f.open();QTest::mouseClick(hundred,Qt::LeftButton);QApplication::processEvents();require(slider->value()==100&&f.window.currentSession().snapshot()==before,"100 percent restores real zoom without authored cells or Undo history");}
void statusZoom(){
 Fixture f;QApplication::processEvents();const auto before=f.window.currentSession().snapshot();const auto selection=f.grid->selectionModel()->selection();auto* focus=QApplication::focusWidget();auto* slider=f.window.findChild<QSlider*>("worksheet.zoom");auto* label=f.window.findChild<QLabel*>("status.zoom");require(slider&&label&&slider->value()==100,"native persisted zoom controls retain their existing identities and100percent state");const auto frame=f.window.grab().toImage();if(const auto path=qEnvironmentVariable("TIGERSHEETS_STATUS_ZOOM_CAPTURE");!path.isEmpty())require(frame.save(path),"retain actual native source zoom artwork");int blue=0,darkTrack=0,darkHandle=0;for(int y=702;y<717;++y)for(int x=1090;x<1225;++x){const auto color=frame.pixelColor(x,y);if(color.blue()>color.red()+30&&color.blue()>color.green()+10)++blue;if(x>=1106&&x<=1209&&y>=709&&y<=711&&std::max({color.red(),color.green(),color.blue()})<110)++darkTrack;if(x>=1155&&x<=1162&&y>=705&&y<=715&&std::max({color.red(),color.green(),color.blue()})<110)++darkHandle;}std::cerr<<"Status zoom artwork blue="<<blue<<" darkTrack="<<darkTrack<<" dark100handle="<<darkHandle<<'\n';require(blue==0&&darkTrack>=90&&darkHandle>=35,"source status zoom paints a black100pixel track with the100percent handle at the source center, without blue fill");
 auto* minus=f.window.findChild<QToolButton*>("status.zoomOut");auto* plus=f.window.findChild<QToolButton*>("status.zoomIn");require(minus&&plus&&minus->isVisible()&&plus->isVisible()&&!minus->icon().isNull()&&!plus->icon().isNull(),"source status zoom exposes real graphical minus and plus buttons");const auto track=f.window.mapFromGlobal(slider->mapToGlobal(QPoint()));const auto minusAt=f.window.mapFromGlobal(minus->mapToGlobal(minus->rect().center()));const auto plusAt=f.window.mapFromGlobal(plus->mapToGlobal(plus->rect().center()));std::cerr<<"Zoom geometry minus="<<minusAt.x()<<','<<minusAt.y()<<" track="<<track.x()<<','<<track.y()<<' '<<slider->width()<<" plus="<<plusAt.x()<<','<<plusAt.y()<<'\n';require(std::abs(track.x()-1108)<=2&&slider->width()==100&&std::abs(minusAt.x()-1099)<=2&&std::abs(plusAt.x()-1217)<=2,"source minus,100pixel slider and plus retain their measured right-anchored order and positions");
 QTest::mouseClick(plus,Qt::LeftButton);require(slider->value()==110&&label->text()=="110%","actual plus increases zoom by10and synchronizes the persisted label");QTest::mouseClick(minus,Qt::LeftButton);require(slider->value()==100&&label->text()=="100%","actual minus decreases zoom by10");slider->setValue(slider->minimum());QApplication::processEvents();require(!minus->isEnabled()&&plus->isEnabled(),"minus is disabled at the existing minimum bound");QTest::mouseClick(minus,Qt::LeftButton);require(slider->value()==slider->minimum(),"minus cannot underflow the zoom bound");QTest::mouseClick(plus,Qt::LeftButton);require(slider->value()==slider->minimum()+10,"plus works from the minimum");slider->setValue(slider->maximum());QApplication::processEvents();require(!plus->isEnabled()&&minus->isEnabled(),"plus is disabled at the existing maximum bound");QTest::mouseClick(plus,Qt::LeftButton);require(slider->value()==slider->maximum(),"plus cannot overflow the zoom bound");QTest::mouseClick(minus,Qt::LeftButton);require(slider->value()==slider->maximum()-10,"minus works from the maximum");
 QTest::mouseClick(slider,Qt::LeftButton,Qt::NoModifier,QPoint(slider->width()/2,slider->height()/2));require(slider->value()==100,"the source central slider position really selects100percent");QTest::mousePress(slider,Qt::LeftButton,Qt::NoModifier,QPoint(slider->width()/2,slider->height()/2));QTest::mouseMove(slider,QPoint(slider->width()-1,slider->height()/2));QTest::mouseRelease(slider,Qt::LeftButton,Qt::NoModifier,QPoint(slider->width()-1,slider->height()/2));require(slider->value()==slider->maximum(),"a real native slider drag reaches the maximum");slider->setValue(100);require(f.window.currentSession().snapshot()==before&&f.grid->selectionModel()->selection()==selection&&QApplication::focusWidget()==focus,"zoom buttons and slider gestures preserve cells, Undo history, selection and worksheet focus");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const std::string mode=argc>1?argv[1]:"";if(mode=="initial")initialHeight();else if(mode=="display")display();else if(mode=="zoom")zoom();else if(mode=="statuszoom")statusZoom();else geometry(mode=="home");std::cout<<"Source View/Home ribbon passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
