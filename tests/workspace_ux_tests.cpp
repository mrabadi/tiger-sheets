#include "tigersheets/main_window.hpp"
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QInputDialog>
#include <QTimer>
#include <QToolButton>
#include <QMenuBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void fileOnIdentityRow(){
 MainWindow window;window.show();QApplication::processEvents();
 auto* brand=window.findChild<QLabel*>("workspace.brand");auto* menus=window.menuBar();
 require(brand&&menus&&!menus->actions().isEmpty(),"workspace identity and File menu exist");
 const auto file=menus->actionGeometry(menus->actions().front());
 const auto logoCenter=brand->mapTo(&window,brand->rect().center());const auto menuCenter=menus->mapTo(&window,file.center());
 require(std::abs(logoCenter.y()-menuCenter.y())<=4,"File menu must share the logo and application-name row");
}
void compactWorkspace(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");require(grid!=nullptr,"worksheet exists");
 require(grid->mapTo(&window,QPoint(0,0)).y()<=155,"compact suite chrome must leave the worksheet within 155 pixels of the top");
}
void liveCellFormula(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");grid->setFocus();grid->setCurrentIndex(grid->model()->index(0,0));QTest::keyClicks(grid,"=");QApplication::processEvents();auto* editor=grid->findChild<QLineEdit*>();require(editor!=nullptr,"typing opens the native cell editor");QTest::keyClicks(editor,"SUM(");QApplication::processEvents();
 require(formula->text()=="=SUM(","in-cell formula typing must update the formula bar immediately");
 require(window.currentSession().snapshot()["sheets"][0]["cells"].empty(),"typing must not commit a cell before acceptance");
}
void cancelCellEntry(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");grid->setFocus();QTest::keyClicks(grid,"=");QApplication::processEvents();auto* editor=grid->findChild<QLineEdit*>();require(editor!=nullptr,"cell editor opens");QTest::keyClicks(editor,"10+20");QTest::keyClick(editor,Qt::Key_Escape);QApplication::processEvents();
 require(formula->text().isEmpty()&&window.currentSession().snapshot()["sheets"][0]["cells"].empty(),"Escape must cancel in-cell entry and restore the formula bar");
}
void entryNavigation(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");formula->setFocus();QTest::keyClicks(formula,"=10+20");QTest::keyClick(formula,Qt::Key_Return);QApplication::processEvents();
 require(grid->currentIndex()==grid->model()->index(1,0)&&grid->model()->data(grid->model()->index(0,0),Qt::EditRole).toString()=="=10+20","Enter must accept formula-bar entry and move down one row");
 grid->setFocus();QTest::keyClicks(grid,"4");QApplication::processEvents();auto* editor=grid->findChild<QLineEdit*>();require(editor!=nullptr,"cell editor opens");QTest::keyClick(editor,Qt::Key_Tab);QApplication::processEvents();
 require(grid->currentIndex()==grid->model()->index(1,1)&&grid->model()->data(grid->model()->index(1,0),Qt::EditRole).toString()=="4","Tab must accept cell entry and move right");
}
void pointRangeWhileTyping(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");for(int row=0;row<3;++row)grid->model()->setData(grid->model()->index(row,1),QString::number((row+1)*10),Qt::EditRole);window.recalculateNow();grid->setCurrentIndex(grid->model()->index(0,3));const auto before=window.currentSession().snapshot();
 formula->setFocus();QTest::keyClicks(formula,"=SUM(");const auto start=grid->visualRect(grid->model()->index(0,1)).center(),end=grid->visualRect(grid->model()->index(2,1)).center();QTest::mousePress(grid->viewport(),Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(grid->viewport(),end);QTest::mouseRelease(grid->viewport(),Qt::LeftButton,Qt::NoModifier,end);QApplication::processEvents();
 if(formula->text()!="=SUM(B1:B3"||window.currentSession().snapshot()!=before)std::cerr<<"point diagnostic text="<<formula->text().toStdString()<<" current="<<grid->currentIndex().row()<<","<<grid->currentIndex().column()<<" revision="<<window.currentSession().snapshot()["revision"]<<" before="<<before["revision"]<<"\n";
 require(formula->text()=="=SUM(B1:B3"&&grid->currentIndex()==grid->model()->index(0,3)&&window.currentSession().snapshot()==before,"dragging worksheet cells must insert a range while leaving the formula uncommitted");
 QTest::keyClicks(formula,")");QTest::keyClick(formula,Qt::Key_Return);window.recalculateNow();require(grid->model()->data(grid->model()->index(0,3)).toString()=="60","the accepted pointed formula must calculate normally");
}
void referenceOutline(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");grid->setCurrentIndex(grid->model()->index(0,3));formula->setFocus();QTest::keyClicks(formula,"=SUM(B1:B3)");QApplication::processEvents();const auto image=grid->viewport()->grab().toImage();const auto first=grid->visualRect(grid->model()->index(0,1)),last=grid->visualRect(grid->model()->index(2,1));int colored=0;for(int y=first.top()+2;y<last.bottom()-2;++y)for(int x=first.left();x<=first.left()+2;++x){const auto c=image.pixelColor(x,y);if(c.blue()>c.red()+40&&c.blue()>c.green()+20)++colored;}
 require(colored>20,"formula references must have a visible colored range outline in the worksheet");
}
void f4PreservesEntry(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");formula->setFocus();QTest::keyClicks(formula,"=B1");QTest::keyClick(formula,Qt::Key_F4);QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualRect(grid->model()->index(0,2)).center());QApplication::processEvents();
 require(grid->model()->data(grid->model()->index(0,0),Qt::EditRole).toString()=="=$B$1","F4 reference cycling must preserve pending formula edits when focus changes");
}
void narrowRibbon(){MainWindow window;window.resize(820,650);window.show();QApplication::processEvents();int found=0;for(auto* button:window.findChildren<QToolButton*>())if(button->text()=="Fill"||button->text()=="Center"||button->text()=="Right"){++found;require(button->width()>=button->sizeHint().width(),"narrow ribbon controls must retain usable natural text widths");}require(found==3,"Home formatting controls remain available");}
void functionCompletion(){
 MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* formula=window.findChild<QLineEdit*>("formula.input");formula->setFocus();QTest::keyClicks(formula,"=AVG");QApplication::processEvents();auto* popup=formula->findChild<QListWidget*>("formula.completions");require(popup&&popup->isVisible()&&popup->count()>0,"typing a function prefix must show available canonical worksheet functions");QTest::keyClick(formula,Qt::Key_Tab);require(formula->text()=="=AVERAGE("&&grid->currentIndex()==grid->model()->index(0,0)&&window.currentSession().snapshot()["sheets"][0]["cells"].empty(),"Tab must insert AVERAGE and its opening parenthesis without committing the formula");QTest::keyClicks(formula,"10,20)");QTest::keyClick(formula,Qt::Key_Return);window.recalculateNow();require(grid->model()->data(grid->model()->index(0,0)).toString()=="15","a completed formula must evaluate and commit through the ordinary editor");
}
void insertFunctionButton(){MainWindow window;window.show();QApplication::processEvents();auto* button=window.findChild<QToolButton*>("formula.insertFunction");require(button,"the formula bar fx control must be an actionable Insert Function button");bool opened=false;QTimer::singleShot(0,&window,[&]{if(auto* dialog=window.findChild<QInputDialog*>()){opened=true;dialog->setTextValue("SUM");dialog->accept();}});QTest::mouseClick(button,Qt::LeftButton);auto* formula=window.findChild<QLineEdit*>("formula.input");require(opened&&formula->text()=="=SUM()"&&formula->isModified(),"Insert Function must place an editable uncommitted function draft in the formula bar");}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const auto selected=argc>1?std::string(argv[1]):std::string();for(auto test:{std::pair{"file",fileOnIdentityRow},std::pair{"density",compactWorkspace},std::pair{"live",liveCellFormula},std::pair{"cancel",cancelCellEntry},std::pair{"navigation",entryNavigation},std::pair{"point",pointRangeWhileTyping},std::pair{"outline",referenceOutline},std::pair{"f4",f4PreservesEntry},std::pair{"narrow",narrowRibbon},std::pair{"completion",functionCompletion},std::pair{"fx",insertFunctionButton}})if(selected.empty()||selected==test.first)test.second();std::cout<<"workspace UX tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
