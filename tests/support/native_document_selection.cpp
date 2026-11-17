#include "native_document_selection.hpp"
#include <QApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPainterPath>
#include <QTest>
#include <QToolButton>
#include <stdexcept>
#include <algorithm>
namespace tigersheets::testing {
namespace {
QIcon icon(int index){QPixmap pixels(24,24);pixels.fill(Qt::transparent);QPainter p(&pixels);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#A0A0A0"),2));p.setBrush(Qt::NoBrush);
 if(index==0){QPainterPath path;path.moveTo(4,5);path.lineTo(20,5);path.lineTo(20,16);path.lineTo(11,16);path.lineTo(7,20);path.lineTo(7,16);path.lineTo(4,16);path.closeSubpath();p.drawPath(path);p.drawLine(7,9,17,9);p.drawLine(7,12,15,12);}
 else if(index==1){p.drawLine(5,18,17,6);p.drawLine(8,20,20,8);p.drawLine(17,6,20,8);p.drawLine(5,18,4,22);p.drawLine(4,22,8,20);}
 else if(index==2){p.drawLine(5,5,19,5);p.drawLine(12,5,12,19);p.drawLine(8,19,16,19);p.drawLine(5,5,5,8);p.drawLine(19,5,19,8);}
 else if(index==3){p.setPen(QPen(QColor("#A0A0A0"),1,Qt::DotLine));p.drawRect(5,4,15,16);p.setPen(QPen(QColor("#A0A0A0"),2));p.drawLine(3,11,3,22);p.drawLine(3,22,13,22);}
 else{QPainterPath page;page.moveTo(5,3);page.lineTo(14,3);page.lineTo(19,8);page.lineTo(19,21);page.lineTo(5,21);page.closeSubpath();p.drawPath(page);p.drawLine(14,3,14,8);p.drawLine(14,8,19,8);p.drawLine(11,14,22,14);p.drawLine(18,10,22,14);p.drawLine(18,18,22,14);}return QIcon(pixels);}
class Selection final:public QWidget {
 QRect image_;QWidget* toolbar_=nullptr;bool selected_=false;
public:
 Selection(QWidget* surface,const QRect& image,const QRect& toolbar):QWidget(surface),image_(image){setObjectName("replay.documentImageSelection");setGeometry(surface->rect());setAttribute(Qt::WA_ShowWithoutActivating);setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);setFocusPolicy(toolbar.isValid()?Qt::StrongFocus:Qt::NoFocus);if(toolbar.isValid()){toolbar_=new QWidget(this);toolbar_->setObjectName("replay.documentImageToolbar");toolbar_->setGeometry(toolbar);toolbar_->setStyleSheet("QWidget#replay\\.documentImageToolbar {background:#252525;} QToolButton {background:#252525;border:0;padding:0;} QToolButton:hover {background:#414141;}");const char* labels[]={"Comment","Highlight","Edit text","Select region","Export image"};for(int i=0;i<5;++i){auto* button=new QToolButton(toolbar_);button->setObjectName(QString("replay.documentImageAction%1").arg(i));button->setAccessibleName(labels[i]);button->setIcon(icon(i));const double scale=toolbar.width()/224.0;const int iconPixels=std::max(1,qRound(22*scale));button->setIconSize(QSize(iconPixels,iconPixels));button->setGeometry(qRound((6+i*42)*scale),0,std::max(1,qRound(40*scale)),toolbar.height());button->setFocusPolicy(Qt::NoFocus);}toolbar_->hide();}show();raise();}
protected:
 void mousePressEvent(QMouseEvent* e)override{if(e->button()==Qt::LeftButton){selected_=image_.contains(e->position().toPoint());if(selected_&&toolbar_)setFocus(Qt::MouseFocusReason);if(toolbar_)toolbar_->setVisible(selected_);update();e->accept();}}
 void keyPressEvent(QKeyEvent* e)override{if(e->key()==Qt::Key_Escape){selected_=false;if(toolbar_)toolbar_->hide();update();e->accept();}else QWidget::keyPressEvent(e);}
 void paintEvent(QPaintEvent*)override{if(!selected_)return;QPainter p(this);p.fillRect(image_,toolbar_?QColor(100,130,170,80):QColor(128,128,128,100));if(toolbar_){p.setPen(QPen(QColor("#687B8D"),2));p.drawRect(image_);}}
};
}
void selectDocumentImage(QWidget* surface,const QRect& image,const QRect& toolbar){if(!surface||!image.isValid()||!surface->rect().contains(image)||!toolbar.isValid())throw std::invalid_argument("Choose an actual visible PDF image region");clearDocumentSelection(surface);auto* selection=new Selection(surface,image,toolbar);QTest::mouseClick(selection,Qt::LeftButton,Qt::NoModifier,image.center());QApplication::processEvents();}
void selectDocumentArtwork(QWidget* surface,const QRect& image){if(!surface||!image.isValid()||!surface->rect().contains(image))throw std::invalid_argument("Choose an original visible document artwork region");const QPointer<QWidget> focus=QApplication::focusWidget();clearDocumentSelection(surface);auto* selection=new Selection(surface,image,QRect());QTest::mouseClick(selection,Qt::LeftButton,Qt::NoModifier,image.center());if(focus)focus->setFocus(Qt::OtherFocusReason);QApplication::processEvents();}
void clearDocumentSelection(QWidget* surface){if(surface)if(auto* selection=surface->findChild<QWidget*>("replay.documentImageSelection",Qt::FindDirectChildrenOnly)){const QPointer<QWidget> focus=selection->focusPolicy()==Qt::NoFocus?QApplication::focusWidget():nullptr;QTest::keyClick(selection,Qt::Key_Escape);delete selection;if(focus)focus->setFocus(Qt::OtherFocusReason);surface->update();QApplication::processEvents();}}
}
