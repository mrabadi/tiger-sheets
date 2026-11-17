#include "support/native_annotation_overlay.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QImage>
#include <QEvent>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
class Gestures final:public QObject {
public:int press=0,move=0,release=0;
protected:bool eventFilter(QObject*,QEvent* event)override{if(event->type()==QEvent::MouseButtonPress)++press;if(event->type()==QEvent::MouseMove)++move;if(event->type()==QEvent::MouseButtonRelease)++release;return false;}
};
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 WorkbookSession session;WorksheetModel model(session);WorksheetView view;view.setModel(&model);view.resize(900,500);view.horizontalHeader()->setDefaultSectionSize(90);view.verticalHeader()->setDefaultSectionSize(24);view.setShowGrid(false);view.show();view.activateWindow();view.setCurrentIndex(model.index(1,1));view.setFocus();QApplication::processEvents();const auto before=session.snapshot();const auto current=view.currentIndex();const auto selection=view.selectionModel()->selection();auto* surface=view.viewport();auto* overlay=testing::annotationOverlay(surface);Gestures gestures;overlay->installEventFilter(&gestures);const QRect rect(210,110,300,150);testing::annotateRectangle(surface,rect,QColor("#0000ff"),3);QApplication::processEvents();const auto marked=surface->grab().toImage();require(marked.pixelColor(rect.left()+20,rect.top())==QColor("#0000ff")&&marked.pixelColor(rect.center())==QColor(Qt::white),"presenter rectangle is visibly drawn as a blue outline over the live worksheet");require(gestures.press==1&&gestures.move>=1&&gestures.release==1,"annotation replay performs genuine press, held move and release events");require(view.currentIndex()==current&&view.selectionModel()->selection()==selection&&view.hasFocus()&&session.snapshot()==before,"presenter marks retain worksheet focus, active cell, selection and workbook history");testing::annotateRectangle(surface,QRect(250,170,80,50),QColor("#ff0000"),2);QApplication::processEvents();require(surface->grab().toImage().pixelColor(270,170)==QColor("#ff0000"),"a second native annotation can use an independent color and line width");testing::clearAnnotations(surface);QApplication::processEvents();require(surface->grab().toImage().pixelColor(rect.left()+20,rect.top())==QColor(Qt::white)&&surface->findChild<QWidget*>("replay.annotationOverlay")==nullptr&&session.snapshot()==before,"clearing presenter marks removes the overlay and restores the underlying worksheet without a cell edit");auto* heldOverlay=testing::annotationOverlay(surface);Gestures held;heldOverlay->installEventFilter(&held);testing::beginAnnotation(surface,QPoint(220,150),QColor("#0000ff"),3);testing::moveAnnotation(surface,QPoint(420,160));QApplication::processEvents();require(surface->grab().toImage().pixelColor(240,150)==QColor("#0000ff")&&held.press==1&&held.move==1&&held.release==0,"an annotation can remain held and visibly narrow before the source drag finishes");testing::moveAnnotation(surface,QPoint(600,250));QApplication::processEvents();require(surface->grab().toImage().pixelColor(300,159)==QColor(Qt::white)&&surface->grab().toImage().pixelColor(300,250)==QColor("#0000ff"),"the second held move expands the same rectangle instead of leaving a second completed mark");testing::endAnnotation(surface,QPoint(600,250));require(held.release==1&&session.snapshot()==before&&view.currentIndex()==current&&view.hasFocus(),"one real release commits only an ephemeral mark and preserves the active worksheet");testing::clearAnnotations(surface);std::cout<<"native annotation overlay tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
