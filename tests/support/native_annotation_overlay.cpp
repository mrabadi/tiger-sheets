#include "native_annotation_overlay.hpp"
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTest>
#include <QWidget>
#include <stdexcept>
#include <vector>
namespace tigersheets::testing {
namespace {
struct Mark {QRect rectangle;QColor color;int width;};
class Overlay final:public QWidget {
 QWidget* surface_;std::vector<Mark> marks_;QPoint start_,end_;QColor color_=Qt::blue;int width_=3;bool drawing_=false;
public:
 explicit Overlay(QWidget* surface):QWidget(surface),surface_(surface){setObjectName("replay.annotationOverlay");setAccessibleName("Temporary presenter annotations");setFocusPolicy(Qt::NoFocus);setAttribute(Qt::WA_ShowWithoutActivating);setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);setAttribute(Qt::WA_TransparentForMouseEvents);setGeometry(surface->rect());surface->installEventFilter(this);show();raise();}
 bool drawing()const{return drawing_;}
 void prepare(const QColor& color,int width){color_=color;width_=width;setAttribute(Qt::WA_TransparentForMouseEvents,false);raise();}
protected:
 bool eventFilter(QObject* object,QEvent* event)override{if(object==surface_&&event->type()==QEvent::Resize)setGeometry(surface_->rect());return QWidget::eventFilter(object,event);}
 void mousePressEvent(QMouseEvent* event)override{if(event->button()!=Qt::LeftButton)return;start_=end_=event->position().toPoint();drawing_=true;update();event->accept();}
 void mouseMoveEvent(QMouseEvent* event)override{if(drawing_&&(event->buttons()&Qt::LeftButton)){end_=event->position().toPoint();update();event->accept();}}
 void mouseReleaseEvent(QMouseEvent* event)override{if(event->button()!=Qt::LeftButton||!drawing_)return;end_=event->position().toPoint();marks_.push_back({QRect(start_,end_).normalized(),color_,width_});drawing_=false;setAttribute(Qt::WA_TransparentForMouseEvents);update();event->accept();}
 void paintEvent(QPaintEvent*)override{QPainter painter(this);painter.setBrush(Qt::NoBrush);auto draw=[&](const Mark& mark){QPen pen(mark.color,mark.width);pen.setJoinStyle(Qt::MiterJoin);painter.setPen(pen);painter.drawRect(mark.rectangle);};for(const auto& mark:marks_)draw(mark);if(drawing_)draw({QRect(start_,end_).normalized(),color_,width_});}
};
}
QWidget* annotationOverlay(QWidget* surface){if(!surface)throw std::invalid_argument("Choose a visible annotation surface");if(auto* existing=surface->findChild<QWidget*>("replay.annotationOverlay",Qt::FindDirectChildrenOnly))return existing;return new Overlay(surface);}
void beginAnnotation(QWidget* surface,const QPoint& point,const QColor& color,int lineWidth){
 if(!surface||!surface->rect().contains(point)||!color.isValid()||lineWidth<1||lineWidth>16)throw std::invalid_argument("Choose a valid presenter start, color, and line width");auto* overlay=dynamic_cast<Overlay*>(annotationOverlay(surface));if(overlay->drawing())throw std::invalid_argument("A presenter rectangle is already held");overlay->prepare(color,lineWidth);QTest::mousePress(overlay,Qt::LeftButton,Qt::NoModifier,point);QApplication::processEvents();
}
void moveAnnotation(QWidget* surface,const QPoint& point){
 auto* overlay=surface?dynamic_cast<Overlay*>(surface->findChild<QWidget*>("replay.annotationOverlay",Qt::FindDirectChildrenOnly)):nullptr;if(!overlay||!overlay->drawing())throw std::invalid_argument("No presenter rectangle is held");QMouseEvent move(QEvent::MouseMove,point,overlay->mapToGlobal(point),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(overlay,&move);QApplication::processEvents();
}
void endAnnotation(QWidget* surface,const QPoint& point){
 auto* overlay=surface?dynamic_cast<Overlay*>(surface->findChild<QWidget*>("replay.annotationOverlay",Qt::FindDirectChildrenOnly)):nullptr;if(!overlay||!overlay->drawing())throw std::invalid_argument("No presenter rectangle is held");QTest::mouseRelease(overlay,Qt::LeftButton,Qt::NoModifier,point);QApplication::processEvents();
}
void annotateRectangle(QWidget* surface,const QRect& rectangle,const QColor& color,int lineWidth){
 if(!surface||rectangle.isEmpty()||!color.isValid()||lineWidth<1||lineWidth>16)throw std::invalid_argument("Choose a valid presenter rectangle, color, and line width");beginAnnotation(surface,rectangle.topLeft(),color,lineWidth);moveAnnotation(surface,rectangle.bottomRight());endAnnotation(surface,rectangle.bottomRight());
}
void clearAnnotations(QWidget* surface){if(surface)if(auto* overlay=surface->findChild<QWidget*>("replay.annotationOverlay",Qt::FindDirectChildrenOnly)){delete overlay;surface->update();}}
}
