#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QFrame>
#include <QDebug>
#include <QTabBar>
#include <QLabel>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void sourceReview(MainWindow& window){window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());grid->setCurrentIndex(model->index(30,0));grid->setFocus();QTest::keyClick(grid,Qt::Key_Space,Qt::ShiftModifier);require(grid->selectionModel()->selection().size()==1&&grid->selectionModel()->selection().front().top()==30&&grid->selectionModel()->selection().front().width()==model->columnCount(),"source whole-row31 selection");require(window.executeCommand("view.ribbon"),"collapse the ribbon before the observed Alt R");QTest::keyClick(grid,Qt::Key_Alt);QTest::keyClick(grid,Qt::Key_R);QApplication::processEvents();}
void native(){
 MainWindow window;const auto before=window.currentSession().snapshot();sourceReview(window);auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");
 require(ribbon->tabText(ribbon->currentIndex())=="Review"&&window.property("ribbon.peek").toBool(),"Alt R keeps the Review ribbon open");
 const std::pair<const char*,const char*> buttons[]={{"Spelling","S"},{"Thesaurus","E"},{"Workbook Statistics","B"},{"Check Accessibility","A"},{"Smart Lookup","RS"},{"Translate","L"},{"New Comment","C"},{"Delete","D"},{"Previous Comment","V"},{"Next Comment","N"},{"Show Comments","H"},{"Notes","T"},{"Protect Sheet","PS"},{"Protect Workbook","PW"},{"Allow Edit Ranges","U1"},{"Unshare Workbook","U2"},{"Hide Ink","H2"}};
 for(const auto& [name,key]:buttons){
  QToolButton* found=nullptr;for(auto* button:ribbon->currentWidget()->findChildren<QToolButton*>())if(button->accessibleName()==name)found=button;
  require(found&&!found->icon().isNull()&&found->property("ribbon.keyTip").toString()==key,"Review retains every native graphical control and its key tip");
  auto* badge=window.findChild<QLabel*>(QString("tutorial.keyTip.")+key);require(badge&&badge->isVisible(),"every Review command has a visible keytip");
  const QRect globalBadge(badge->mapToGlobal(QPoint()),badge->size());
  if(badge->isWindow())require(badge->screen()&&badge->screen()->availableGeometry().contains(globalBadge),"top-level Review keytips stay fully within the available screen");
  else{auto* owner=badge->window();require(owner->rect().contains(QRect(owner->mapFromGlobal(globalBadge.topLeft()),badge->size())),"Review keytips remain fully visible rather than clipped by the compact popup");}
 }
 auto* stack=ribbon->findChild<QStackedWidget*>();require(stack&&stack->isVisible()&&stack->height()==36,"Review opens a single compact command row");
 const auto tabBottom=ribbon->tabBar()->mapToGlobal(QPoint(0,ribbon->tabBar()->height()));require(stack->mapToGlobal(QPoint()).y()==tabBottom.y()-2,"Review popup anchors below its actual tabs");
 auto* banner=window.findChild<QWidget*>("review.commentsOnboarding");auto* body=window.findChild<QWidget*>("review.commentsOnboarding.body");auto* title=window.findChild<QLabel*>("review.commentsOnboarding.title");auto* got=window.findChild<QPushButton*>("review.commentsOnboarding.gotIt");
 require(banner&&banner->isVisible()&&body&&body->size()==QSize(284,246)&&title&&title->text()=="Comments have changed"&&got,"first Review retains the explanatory Comments have changed onboarding and dismissal control");
 require(got->text().isEmpty()&&!got->icon().isNull()&&got->accessibleName()=="Got it"&&got->toolTip()=="Got it","onboarding dismissal uses an icon-only check with the full Got it hover and accessible name");
 require(body->grab().toImage().pixelColor(5,40)==QColor("#217346"),"onboarding body retains its green information panel");
 auto* comments=ribbon->currentWidget()->findChild<QWidget*>("review.group.comments");auto* newComment=window.findChild<QToolButton*>("review.newComment");
 require(comments&&newComment&&body->mapToGlobal(QPoint()).y()==comments->mapToGlobal(QPoint(0,comments->height())).y()+6,"onboarding body stays six pixels below its actual compact Comments group");
 const auto artwork=banner->grab().toImage();int first=-1,last=-1;for(int x=0;x<artwork.width();++x)if(artwork.pixelColor(x,0)==QColor("#bdd1c3")){if(first<0)first=x;last=x;}
 require(first>=0&&std::abs(banner->mapToGlobal(QPoint((first+last)/2,0)).x()-newComment->mapToGlobal(newComment->rect().center()).x())<=1,"onboarding triangle points at the actual New Comment button center");
 if(const auto capture=qEnvironmentVariable("TIGERSHEETS_REVIEW_CAPTURE");!capture.isEmpty()){QPixmap frame=window.grab();QPainter painter(&frame);painter.drawPixmap(stack->mapToGlobal(QPoint())-window.mapToGlobal(QPoint()),stack->grab());for(auto* tip:window.findChildren<QLabel*>())if(tip->isWindow()&&tip->isVisible()&&tip->objectName().startsWith("tutorial.keyTip."))painter.drawPixmap(tip->mapToGlobal(QPoint())-window.mapToGlobal(QPoint()),tip->grab());painter.end();frame.save(capture);}
 auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto selection=grid->selectionModel()->selection();QTest::mouseClick(got,Qt::LeftButton);
 require(!banner->isVisible()&&grid->selectionModel()->selection()==selection&&window.currentSession().snapshot()==before,"Got it dismisses onboarding without changing whole-row31 selection, cells, or history");
 window.executeCommand("view.ribbon.close");QTest::keyClick(grid,Qt::Key_Alt);QTest::keyClick(grid,Qt::Key_R);QApplication::processEvents();require(!banner->isVisible(),"acknowledged first-use onboarding does not appear again in the same window");
}
void icons(){
 MainWindow window;window.resize(1280,720);window.show();QApplication::processEvents();
 auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");require(ribbon,"native ribbon exists");
 const auto before=window.currentSession().snapshot();
 QStringList failures;
 for(const auto& tab:{QString("View"),QString("Review")}){
  int index=-1;for(int i=0;i<ribbon->count();++i)if(ribbon->tabText(i)==tab)index=i;
  require(index>=0,"native View and Review tabs exist");ribbon->setCurrentIndex(index);QApplication::processEvents();
  std::vector<std::pair<QString,QImage>> rendered;
  int commands=0;
  for(auto* button:ribbon->currentWidget()->findChildren<QAbstractButton*>()){
   if(qobject_cast<QCheckBox*>(button)&&button->icon().isNull())continue;
   const auto name=button->objectName();
   if(button->icon().isNull()){failures<<tab+": missing icon for "+name;continue;}
   if(qobject_cast<QToolButton*>(button)&&name.startsWith(tab.toLower()+"."))++commands;
   const auto image=button->icon().pixmap(QSize(24,24),QIcon::Normal,QIcon::Off).toImage().convertToFormat(QImage::Format_ARGB32);
   require(!image.isNull(),"command renders a native 24-pixel icon");
   for(const auto& [other,pixels]:rendered)if(image==pixels)failures<<tab+": "+other+" duplicates "+name;
   rendered.emplace_back(name,image);
  }
  std::cout<<tab.toStdString()<<": "<<commands<<" command icons, "<<rendered.size()<<" total icon controls\n";
  require(commands==(tab=="View"?23:17),"uniqueness covers every native command including disabled and menu buttons");
  if(tab=="View")for(const auto& [id,name]:{std::pair{"sideBySide","View Side by Side"},std::pair{"synchronousScroll","Synchronous Scrolling"},std::pair{"resetPosition","Reset Window Position"}}){
   auto* button=ribbon->currentWidget()->findChild<QToolButton*>(QString("view.")+id);require(button,"comparison command exists");
   if(button->accessibleName()!=name||button->toolTip()!=name)failures<<QString("View: %1 must expose '%2' for hover and accessibility; got '%3' / '%4'").arg(id,name,button->accessibleName(),button->toolTip());
  }
 }
 for(const auto& failure:failures)std::cerr<<failure.toStdString()<<'\n';
 require(failures.isEmpty(),"every command has distinct 24-pixel Normal artwork within its tab");
 require(window.currentSession().snapshot()==before,"viewing command icons preserves workbook content and history");
}
void dismissed(bool tab){MainWindow window;sourceReview(window);auto* banner=window.findChild<QWidget*>("review.commentsOnboarding");require(banner&&banner->isVisible(),"new window displays first-use Review onboarding");auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto selection=grid->selectionModel()->selection();const auto before=window.currentSession().snapshot();if(tab){auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");ribbon->setCurrentIndex(0);}else{QTest::keyClick(grid,Qt::Key_Escape);}QApplication::processEvents();require(!banner->isVisible()&&grid->selectionModel()->selection()==selection&&window.currentSession().snapshot()==before,"tab away or Escape closes the Review onboarding while retaining whole-row31 and history");}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto mode=argc>1?std::string(argv[1]):std::string();if(mode=="icons")icons();else if(mode=="tab"||mode=="escape")dismissed(mode=="tab");else native();std::cout<<"Native Review onboarding passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
