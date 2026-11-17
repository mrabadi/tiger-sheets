#include "support/native_presentation_builds.hpp"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <QWidget>
#include <iostream>
#include <stdexcept>
using namespace tigersheets::testing;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
class Page final:public QWidget {
public:Page(){setFocusPolicy(Qt::StrongFocus);resize(500,350);}
protected:void paintEvent(QPaintEvent*)override{QPainter painter(this);painter.fillRect(rect(),Qt::white);painter.fillRect(QRect(30,40,400,25),Qt::green);painter.fillRect(QRect(30,100,400,25),Qt::red);painter.fillRect(QRect(30,160,400,25),Qt::blue);}
};
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 Page page;page.show();page.activateWindow();page.setFocus();QApplication::processEvents();const auto original=page.grab().toImage();setPresentationBuilds(&page,{{QRect(25,95,420,35),QRect(25,155,420,35)},{QRect(25,155,420,35)},{}});QApplication::processEvents();require(page.grab().toImage().pixelColor(60,110)==QColor(Qt::white)&&page.grab().toImage().pixelColor(60,50)==QColor(Qt::green),"initial source build masks hide only unrevealed bullet pixels over the original page");QTest::keyClick(&page,Qt::Key_Space);QApplication::processEvents();require(page.grab().toImage().pixelColor(60,110)==QColor(Qt::red)&&page.grab().toImage().pixelColor(60,170)==QColor(Qt::white),"a native Space key reveals the next actual page region while retaining later masks");advancePresentationBuild(&page);require(page.grab().toImage()==original,"the final native reveal restores every original page pixel");advancePresentationBuild(&page,true);require(page.grab().toImage().pixelColor(60,170)==QColor(Qt::white)&&page.grab().toImage().pixelColor(60,110)==QColor(Qt::red),"native previous-build navigation reinstates only the prior stage's masks");require(page.hasFocus(),"presentation builds preserve the document surface's keyboard focus");clearPresentationBuilds(&page);QApplication::processEvents();require(page.grab().toImage()==original&&page.findChild<QWidget*>("replay.presentationBuilds")==nullptr,"clearing presentation adaptation restores the original page and removes its layer");std::cout<<"native presentation build tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
