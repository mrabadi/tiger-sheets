#include "native_document_text_toolbar.hpp"
#include <QComboBox>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVariant>
#include <QWidget>
#include <cmath>
#include <stdexcept>
namespace tigersheets::testing {
namespace {
QIcon symbol(const QString& name,bool dark){
 QPixmap pixels(30,30);pixels.fill(Qt::transparent);QPainter p(&pixels);p.setRenderHint(QPainter::Antialiasing);const QColor ink(dark?"#BEBEBE":"#292D2F");p.setPen(QPen(ink,1.7));p.setBrush(Qt::NoBrush);QFont font("Arial");font.setPixelSize(23);font.setBold(name=="Bold");font.setItalic(name=="Italic");p.setFont(font);
 if(name=="Bold"||name=="Italic"||name=="Underline"){p.drawText(QRect(1,1,28,27),Qt::AlignCenter,name.left(1));if(name=="Underline")p.drawLine(8,27,22,27);}
 else if(name=="Grow Font"||name=="Shrink Font"){font.setPixelSize(name=="Grow Font"?24:20);p.setFont(font);p.drawText(QRect(0,3,24,27),Qt::AlignCenter,"A");p.drawLine(24,7,27,4);p.drawLine(27,4,29,7);}
 else if(name=="Font Color"){p.drawText(QRect(1,0,28,24),Qt::AlignCenter,"A");p.fillRect(4,26,22,4,Qt::red);}
 else if(name=="Highlight"){p.drawLine(6,23,22,7);p.drawLine(10,26,26,10);p.drawLine(22,7,26,10);p.drawLine(6,23,4,28);p.fillRect(2,27,26,3,dark?ink:QColor("#FFFF00"));}
 else if(name=="Underline Text"||name=="Strike Text"||name=="Redact Text"){font.setBold(true);p.setFont(font);p.drawText(QRect(1,0,28,29),Qt::AlignCenter,"T");if(name=="Underline Text")p.drawLine(7,27,23,27);else if(name=="Strike Text")p.drawLine(3,15,27,15);else p.fillRect(4,18,23,11,ink);}
 else if(name=="Copy Selection"){p.setPen(QPen(ink,1.5,Qt::DotLine));p.drawRect(10,3,17,18);p.setPen(QPen(ink,4));p.drawLine(5,13,5,26);p.drawLine(5,26,19,26);}
 else if(name=="Edit PDF"){QPainterPath path;path.moveTo(5,2);path.lineTo(18,2);path.lineTo(25,9);path.lineTo(25,26);path.lineTo(5,26);path.closeSubpath();p.drawPath(path);p.drawLine(18,2,18,9);p.drawLine(18,9,25,9);p.drawLine(14,25,28,11);p.drawLine(17,28,30,15);}
 else if(name=="New Comment"){QPainterPath path;path.moveTo(3,5);path.lineTo(27,5);path.lineTo(27,22);path.lineTo(12,22);path.lineTo(7,27);path.lineTo(7,22);path.lineTo(3,22);path.closeSubpath();p.drawPath(path);p.setPen(QPen(QColor("#3E765C"),2));p.drawLine(7,1,7,13);p.drawLine(1,7,13,7);}
 else if(name=="Bullets"||name=="Numbering"){for(int y=5,n=1;y<27;y+=9,++n){if(name=="Bullets")p.fillRect(2,y,4,4,QColor("#4282A0"));else{QFont small("Arial");small.setPixelSize(9);p.setFont(small);p.drawText(QRect(0,y-4,9,10),Qt::AlignCenter,QString::number(n));}p.drawLine(11,y+2,28,y+2);}}
 else if(name=="Center"||name=="Line and Paragraph Spacing"||name=="Paragraph"){for(int y=5;y<28;y+=6){const int inset=name=="Center"&&y%12?5:0;p.drawLine(4+inset,y,27-inset,y);}if(name=="Line and Paragraph Spacing"){p.setPen(QPen(QColor("#398BA8"),2));p.drawLine(2,3,2,28);p.drawLine(0,6,2,3);p.drawLine(2,3,5,6);p.drawLine(0,25,2,28);p.drawLine(2,28,5,25);}if(name=="Paragraph"){p.setPen(ink);font.setPixelSize(20);p.setFont(font);p.drawText(QRect(12,8,18,23),Qt::AlignCenter,QString(QChar(0x00B6)));}}
 else if(name=="Clear Formatting"){font.setPixelSize(11);p.setFont(font);p.drawText(0,9,"abc");p.setPen(QColor("#48A5C5"));font.setPixelSize(22);p.setFont(font);p.drawText(9,28,"A");}
 else if(name=="Styles"){p.drawText(QRect(1,0,25,28),Qt::AlignCenter,"A");p.setPen(QPen(QColor("#3E91B1"),4));p.drawLine(15,28,28,14);}
 else {p.setPen(QPen(QColor("#BC873E"),4));p.drawLine(5,25,15,15);p.setPen(QPen(ink,2));p.drawRect(12,6,12,8);p.drawLine(17,6,24,0);}
 return QIcon(pixels);
}
class Toolbar final:public QWidget {
 bool dark_;
public:
 Toolbar(QWidget* surface,const QRect& bounds,const QString& kind,qreal opacity):QWidget(surface),dark_(kind=="acrobat"){
  setObjectName("replay.documentTextToolbar");setProperty("toolbar.kind",kind);setProperty("toolbar.opacity",opacity);setGeometry(bounds);setFocusPolicy(Qt::NoFocus);setAttribute(Qt::WA_ShowWithoutActivating);setAttribute(Qt::WA_TransparentForMouseEvents);
  auto* effect=new QGraphicsOpacityEffect(this);effect->setOpacity(opacity);setGraphicsEffect(effect);
  const double sx=width()/(dark_?265.0:536.0),sy=height()/(dark_?41.0:64.0);auto rect=[=](int x,int y,int w,int h){return QRect(qRound(x*sx),qRound(y*sy),qRound(w*sx),qRound(h*sy));};
  auto button=[&](const QString& name,const QRect& geometry,const QString& text=QString()){
   auto* result=new QToolButton(this);result->setObjectName("replay.documentTextToolbar."+QString(name).remove(' '));result->setAccessibleName(name);result->setToolTip(name);result->setFocusPolicy(Qt::NoFocus);result->setGeometry(geometry);result->setIcon(symbol(name,dark_));result->setIconSize(QSize(qRound(25*sx),qRound(25*sy)));result->setText(text);if(!text.isEmpty())result->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);QFont font("Arial");font.setPixelSize(qRound(11*sy));result->setFont(font);result->setStyleSheet(dark_?"QToolButton{border:0;background:transparent;color:#BEBEBE;padding:0}":"QToolButton{border:0;background:transparent;color:#202426;padding:0}");return result;
  };
  if(dark_){const QString names[]={"Highlight","Underline Text","Strike Text","Redact Text","Copy Selection","Edit PDF"};for(int i=0;i<6;++i)button(names[i],rect(i==5?226:8+i*40,2,i==5?33:35,37));}
  else{
   auto combo=[&](const char* name,const QString& text,const QRect& geometry){auto* result=new QComboBox(this);result->setObjectName(name);result->addItem(text);result->setGeometry(geometry);result->setFocusPolicy(Qt::NoFocus);QFont font("Arial");font.setPixelSize(qRound(12*sy));result->setFont(font);result->setStyleSheet("QComboBox{background:white;border:1px solid #C9C9C9;padding:1px}");};
   combo("replay.documentTextToolbar.font","Calibri (Body)",rect(4,5,81,24));combo("replay.documentTextToolbar.fontSize","12",rect(85,5,45,24));
   const QString top[]={"Grow Font","Shrink Font","Clear Formatting","Format Painter"};for(int i=0;i<4;++i)button(top[i],rect(132+i*22,3,22,27));
   const QString bottom[]={"Bold","Italic","Underline","Highlight","Font Color","Bullets","Numbering"};const int positions[]={4,27,50,73,108,145,180};for(int i=0;i<7;++i)button(bottom[i],rect(positions[i],32,i<3?23:34,28));
   button("Styles",rect(222,3,40,58),"Styles");button("New Comment",rect(263,3,58,58),"New\nComment");button("Center",rect(322,3,44,58),"Center");button("Line and Paragraph Spacing",rect(367,3,110,58),"Line and Paragraph\nSpacing");button("Paragraph",rect(478,3,55,58),"Paragraph");
  }
  show();raise();
 }
 void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),dark_?QColor("#252525"):Qt::white);p.setPen(dark_?QColor("#252525"):QColor("#C9C9C9"));p.drawRect(rect().adjusted(0,0,-1,-1));if(dark_){p.setPen(QColor("#676767"));const int x=qRound(width()*213/265.0);p.drawLine(x,8,x,height()-8);}else{p.setPen(QColor("#E2E2E2"));for(int x:{262,321,367,477})p.drawLine(qRound(width()*x/536.0),5,qRound(width()*x/536.0),height()-5);}}
};
}
QWidget* showDocumentTextToolbar(QWidget* surface,const QRect& bounds,const QString& kind,qreal opacity){
 if(!surface||!bounds.isValid()||!surface->rect().contains(bounds)||(kind!="word"&&kind!="acrobat")||!std::isfinite(opacity)||opacity<0||opacity>1)throw std::invalid_argument("Choose a visible document toolbar region, kind and opacity");
 clearDocumentTextToolbar(surface);return new Toolbar(surface,bounds,kind,opacity);
}
void clearDocumentTextToolbar(QWidget* surface){if(surface)if(auto* toolbar=surface->findChild<QWidget*>("replay.documentTextToolbar",Qt::FindDirectChildrenOnly)){delete toolbar;surface->update();}}
}
