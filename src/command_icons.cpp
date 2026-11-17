#include "tigersheets/command_icons.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QApplication>
#include <QStyle>
#include <QHash>
namespace tigersheets {
QIcon spreadsheetCommandIcon(const QString& requested){
 static const QHash<QString,QString> aliases={{"Σ","AutoSum"},{"Fill ↓","Fill down"},{"ƒx","Insert function"},{"Format…","Format cells"},{"▾","Paste special"},{"ruler","Ruler"},{"formulaBar","Formula Bar"},{"gridlines","Gridlines"},{"headings","Headings"}};
 const auto label=aliases.value(requested,requested);
 const auto theme=[&](const char* name){return QIcon::fromTheme(QString::fromLatin1(name));};
 const auto semantic=[&]()->QIcon{
  if(label=="Copy")return theme("edit-copy");if(label=="Paste")return theme("edit-paste");if(label=="Cut")return theme("edit-cut");
  if(label=="Open")return theme("document-open");if(label=="Save")return theme("document-save");if(label=="Find")return theme("edit-find");
  if(label=="↶")return theme("edit-undo");if(label=="↷")return theme("edit-redo");return {};
 }();if(!semantic.isNull())return semantic;
 QPixmap canvas(24,24);canvas.fill(Qt::transparent);QPainter p(&canvas);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#334b43"),1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
 auto page=[&](QRectF rect){p.setBrush(QColor("#ffffff"));p.drawRoundedRect(rect,1,1);p.setBrush(Qt::NoBrush);};
 auto grid=[&](QRectF rect){p.drawRect(rect);for(int i=1;i<3;++i){const auto x=rect.x()+i*rect.width()/3,y=rect.y()+i*rect.height()/3;p.drawLine(QPointF(x,rect.top()),QPointF(x,rect.bottom()));p.drawLine(QPointF(rect.left(),y),QPointF(rect.right(),y));}};
 auto check=[&](QPointF at){p.setPen(QPen(QColor("#3b8f6d"),2.5));p.drawPolyline(QPolygonF{at+QPointF(-4,0),at+QPointF(-1,3),at+QPointF(5,-4)});};
 auto chart=[&]{p.drawLine(3,3,3,21);p.drawLine(3,21,22,21);p.setPen(Qt::NoPen);p.setBrush(QColor("#5593c6"));p.drawRect(6,13,3,7);p.drawRect(11,9,3,11);p.drawRect(16,5,3,15);p.setPen(QPen(QColor("#334b43"),1.5));p.setBrush(Qt::NoBrush);};
 if(label=="Copy"){page({4,3,12,15});page({8,7,12,15});p.drawLine(11,11,17,11);p.drawLine(11,15,17,15);}
 else if(label=="Paste"){p.setBrush(QColor("#d5a653"));p.drawRoundedRect(QRectF{5,4,14,18},1,1);page({8,8,13,14});p.setBrush(QColor("#f4e8cc"));p.drawRoundedRect(QRectF{8,2,8,5},1,1);p.drawLine(11,12,18,12);p.drawLine(11,16,18,16);}
 else if(label=="Cut"){p.drawEllipse(QRectF{3,14,6,6});p.drawEllipse(QRectF{14,14,6,6});p.drawLine(7,15,18,3);p.drawLine(16,15,5,3);}
 else if(label=="Save"){p.setBrush(QColor("#5d91bc"));p.drawRoundedRect(QRectF{3,3,18,18},1,1);page({7,3,10,6});page({7,13,10,8});p.drawLine(14,4,14,8);}
 else if(label=="Open"){p.setBrush(QColor("#e6bc62"));p.drawPolygon(QPolygonF{{3,6},{10,6},{12,8},{21,8},{21,20},{3,20}});p.setBrush(QColor("#f6d486"));p.drawPolygon(QPolygonF{{3,20},{6,11},{23,11},{20,20}});}
 else if(label=="Find"){p.drawEllipse(QRectF{3,3,12,12});p.drawLine(14,14,21,21);}
 else if(label=="Calculate now"){page({4,2,16,20});p.setBrush(QColor("#dbece3"));p.drawRect(QRectF{7,5,10,4});p.setBrush(QColor("#4e9675"));for(int y:{12,16})for(int x:{7,12})p.drawRect(QRectF{qreal(x),qreal(y),2,2});p.drawLine(17,12,17,19);}
 else if(label=="Insert function"){p.setFont(QFont("DejaVu Sans",12,QFont::DemiBold));p.drawText(QRect(1,1,20,18),Qt::AlignCenter,"ƒx");p.setPen(QPen(QColor("#3b8f6d"),2));p.drawLine(17,16,17,22);p.drawLine(14,19,20,19);}
 else if(label=="Define name"){p.setBrush(QColor("#f2d58a"));p.drawPolygon(QPolygonF{{3,5},{14,5},{22,12},{14,19},{3,19}});p.setBrush(Qt::white);p.drawEllipse(QRectF{5,10,3,3});p.setFont(QFont("DejaVu Sans",9,QFont::Bold));p.drawText(QRect(9,6,9,12),Qt::AlignCenter,"A");}
 else if(label=="GETPIVOTDATA"){p.drawRect(QRectF{2,3,17,16});for(int y:{8,13})p.drawLine(2,y,19,y);for(int x:{8,14})p.drawLine(x,3,x,19);p.setPen(QPen(QColor("#3b8f6d"),3));p.drawLine(3,9,13,9);p.setPen(QPen(QColor("#3b8f6d"),1.8));p.drawPolyline(QPolygonF{{16,15},{21,20},{21,15}});p.drawLine(21,20,16,20);}
 else if(label=="Insert chart"||label=="Remove charts"){chart();p.setBrush(Qt::white);p.drawEllipse(QRectF{13,1,10,10});p.setPen(QPen(QColor(label=="Insert chart"?"#3b8f6d":"#b33c35"),2));if(label=="Insert chart"){p.drawLine(18,3,18,9);p.drawLine(15,6,21,6);}else{p.drawLine(16,4,20,8);p.drawLine(16,8,20,4);}}
 else if(label=="New sheet"){page({3,2,15,20});p.drawLine(6,7,14,7);p.drawLine(6,11,14,11);p.setPen(QPen(QColor("#3b8f6d"),2.5));p.drawLine(18,14,18,22);p.drawLine(14,18,22,18);}
 else if(label=="Rename"){page({2,5,15,16});p.drawLine(5,10,11,10);p.setBrush(QColor("#e6bc62"));p.drawPolygon(QPolygonF{{10,17},{11,12},{19,3},{22,6},{14,15}});p.setBrush(Qt::NoBrush);p.drawLine(18,5,20,7);}
 else if(label=="Print"||label=="Print settings"){page({6,2,12,8});p.setBrush(QColor("#dbe4e8"));p.drawRoundedRect(QRectF{3,8,18,10},2,2);page({6,14,12,8});p.drawPoint(18,11);if(label=="Print settings"){p.setBrush(Qt::white);p.drawEllipse(QRectF{14,14,9,9});p.drawEllipse(QRectF{17,17,3,3});for(int i=0;i<8;++i){p.save();p.translate(18.5,18.5);p.rotate(i*45);p.drawLine(QPointF(0,-5),QPointF(0,-3));p.restore();}}}
 else if(label=="Export PDF"){page({3,2,15,20});p.drawLine(6,6,14,6);p.setPen(QColor("#b33c35"));p.setFont(QFont("DejaVu Sans",6,QFont::Bold));p.drawText(QRect(3,8,16,9),Qt::AlignCenter,"PDF");p.setPen(QPen(QColor("#3b8f6d"),1.8));p.drawLine(15,19,22,19);p.drawPolyline(QPolygonF{{19,16},{22,19},{19,22}});}
 else if(label=="A → Z"||label=="Z → A"){p.setFont(QFont("DejaVu Sans",9,QFont::Bold));p.drawText(QRect(1,0,12,12),Qt::AlignCenter,label.startsWith("A")?"A":"Z");p.drawText(QRect(1,12,12,12),Qt::AlignCenter,label.startsWith("A")?"Z":"A");p.drawLine(17,3,17,20);p.drawPolyline(QPolygonF{{14,17},{17,20},{20,17}});}
 else if(label=="Data Validation"){grid({2,3,17,17});p.setBrush(Qt::white);p.setPen(Qt::NoPen);p.drawRect(QRectF{10,11,14,13});check({16,16});}
 else if(label=="Forecast"){p.drawLine(3,3,3,21);p.drawLine(3,21,22,21);p.setPen(QPen(QColor("#3b8f6d"),2));p.drawPolyline(QPolygonF{{4,17},{8,12},{12,14}});p.setPen(QPen(QColor("#3b8f6d"),2,Qt::DashLine));p.drawPolyline(QPolygonF{{12,14},{17,8},{22,5}});}
 else if(label=="Assumptions"){for(int i=0;i<3;++i){const int y=5+i*7,x=i==1?15:8;p.drawLine(3,y,21,y);p.setBrush(QColor("#dbece3"));p.drawRoundedRect(QRectF{qreal(x-2),qreal(y-3),4,6},1,1);p.setBrush(Qt::NoBrush);}}
 else if(label=="Compare scenarios"){p.drawLine(12,3,12,21);p.drawLine(2,21,22,21);p.setBrush(QColor("#5593c6"));p.drawRect(QRectF{3,12,3,8});p.drawRect(QRectF{7,6,3,14});p.setBrush(QColor("#64a17e"));p.drawRect(QRectF{14,5,3,15});p.drawRect(QRectF{18,10,3,10});}
 else if(label=="Goal Seek"){p.drawEllipse(QRectF{3,3,18,18});p.drawEllipse(QRectF{7,7,10,10});p.setBrush(QColor("#b33c35"));p.drawEllipse(QRectF{10,10,4,4});p.setPen(QPen(QColor("#334b43"),1.7));p.drawLine(22,2,12,12);p.drawPolyline(QPolygonF{{12,7},{12,12},{17,12}});}
 else if(label=="Solver"){p.drawLine(5,5,17,12);p.drawLine(5,19,17,12);p.setBrush(QColor("#dbece3"));p.drawRect(QRectF{2,2,6,6});p.drawRect(QRectF{2,16,6,6});p.drawEllipse(QRectF{12,7,10,10});p.drawEllipse(QRectF{15,10,4,4});}
 else if(label=="Scenarios"){page({8,2,13,15});page({5,5,13,15});page({2,8,13,14});p.setPen(QPen(QColor("#3b8f6d"),2));p.drawPolyline(QPolygonF{{4,18},{7,13},{10,16},{13,11}});}
 else if(label=="Convert summary…"){grid({2,3,12,17});p.setPen(QPen(QColor("#3b8f6d"),2));p.drawLine(11,12,22,12);p.drawPolyline(QPolygonF{{18,8},{22,12},{18,16}});p.drawLine(17,3,22,3);p.drawLine(17,21,22,21);}
 else if(label=="Data table"){grid({2,3,20,18});p.setPen(QPen(QColor("#5593c6"),3));p.drawLine(3,6,21,6);p.drawLine(5,4,5,20);}
 else if(label=="Refresh links"){p.save();p.translate(12,12);p.rotate(-35);p.drawRoundedRect(QRectF{-10,-3,11,6},3,3);p.drawRoundedRect(QRectF{-1,-3,11,6},3,3);p.restore();p.setPen(QPen(QColor("#3b8f6d"),1.5));p.drawArc(QRectF{3,2,18,19},20*16,105*16);p.drawPolyline(QPolygonF{{5,1},{4,6},{9,5}});}
 else if(label=="Cell note"){p.setBrush(QColor("#fff0af"));p.drawPolygon(QPolygonF{{3,3},{21,3},{21,17},{15,17},{11,22},{11,17},{3,17}});p.drawLine(6,7,18,7);p.drawLine(6,11,16,11);}
 else if(label=="Show formulas"){grid({2,3,20,18});p.setBrush(Qt::white);p.drawRect(QRectF{6,7,14,11});p.setFont(QFont("DejaVu Sans",8,QFont::Bold));p.drawText(QRect(6,7,14,11),Qt::AlignCenter,"fx");}
 else if(label=="Freeze top row"){grid({3,3,18,18});p.setBrush(QColor("#b4d9ec"));p.drawRect(QRectF{3,3,18,5});p.setPen(QPen(QColor("#397ba1"),2));p.drawLine(2,9,22,9);p.drawLine(16,13,16,21);p.drawLine(12,17,20,17);p.drawLine(13,14,19,20);p.drawLine(13,20,19,14);}
 else if(label=="Visual Basic"){page({2,3,20,18});p.setPen(QColor("#66599c"));p.setFont(QFont("DejaVu Sans",8,QFont::Bold));p.drawText(QRect(2,7,20,11),Qt::AlignCenter,"VB");p.drawLine(4,6,8,6);}
 else if(label=="Record macro"){page({3,2,16,20});p.drawLine(6,6,15,6);p.drawLine(6,10,15,10);p.setBrush(QColor("#c14542"));p.drawEllipse(QRectF{12,12,11,11});}
 else if(label=="Codex"){p.drawRoundedRect(QRectF{2,3,19,15},3,3);p.drawPolyline(QPolygonF{{6,18},{6,22},{11,18}});p.drawPolyline(QPolygonF{{8,7},{5,10},{8,13}});p.drawPolyline(QPolygonF{{15,7},{18,10},{15,13}});p.drawLine(13,6,10,14);}
 else if(label=="Accept"){p.drawEllipse(QRectF{2,2,20,20});check({11,12});}
 else if(label=="Discard"){p.setBrush(QColor("#f0d8d6"));p.drawRoundedRect(QRectF{6,6,12,15},1,1);p.setBrush(Qt::NoBrush);p.drawLine(4,5,20,5);p.drawPolyline(QPolygonF{{9,5},{9,2},{15,2},{15,5}});p.drawLine(10,9,10,18);p.drawLine(14,9,14,18);}
 else if(label=="Ruler"){p.setBrush(QColor("#f2d58a"));p.drawRect(QRectF{2,7,20,10});for(int x=5;x<22;x+=3)p.drawLine(x,8,x,x%2?12:15);}
 else if(label=="Formula Bar"||label=="Formula bar"){p.drawRoundedRect(QRectF{2,5,20,14},2,2);p.setFont(QFont("DejaVu Sans",8,QFont::DemiBold));p.drawText(QRect(3,6,10,12),Qt::AlignCenter,"fx");p.drawLine(15,9,15,16);p.drawLine(14,9,17,9);p.drawLine(14,16,17,16);}
 else if(label=="Headings"){grid({2,3,20,18});p.setBrush(QColor("#c8dbe8"));p.drawRect(QRectF{2,3,20,5});p.drawRect(QRectF{2,8,5,13});p.setFont(QFont("DejaVu Sans",5,QFont::Bold));p.drawText(QRect(8,3,12,6),Qt::AlignCenter,"A B");p.drawText(QRect(2,8,5,11),Qt::AlignCenter,"1\n2");}
 else if(label=="AutoSum"){p.setFont(QFont("DejaVu Serif",19,QFont::DemiBold));p.drawText(canvas.rect(),Qt::AlignCenter,"Σ");}
 else if(label=="Fill down"){grid({2,3,13,18});p.setBrush(QColor("#b4d9ec"));p.drawRect(QRectF{2,3,13,5});p.setPen(QPen(QColor("#3b8f6d"),2));p.drawLine(20,5,20,20);p.drawPolyline(QPolygonF{{17,17},{20,20},{23,17}});}
 else if(label=="Format cells"){grid({2,3,17,17});p.setBrush(QColor("#e6bc62"));p.drawPolygon(QPolygonF{{10,21},{12,16},{20,6},{23,9},{15,19}});}
 else if(label=="Paste special"){p.setBrush(QColor("#d5a653"));p.drawRoundedRect(QRectF{3,4,14,17},1,1);page({6,7,13,14});p.drawRect(QRectF{7,2,7,4});p.setPen(QPen(QColor("#3b8f6d"),1.7));p.drawLine(10,11,10,18);p.drawLine(13,11,17,11);p.drawLine(13,15,17,15);p.drawLine(13,18,17,18);}
 else if(label=="↶"||label=="↷"){p.save();if(label=="↷"){p.translate(24,0);p.scale(-1,1);}p.drawArc(QRectF{5,7,15,13},0,210*16);p.drawPolyline(QPolygonF{{3,5},{3,12},{10,12}});p.restore();}
 else if(label=="Fill"){p.setBrush(QColor("#ffdf60"));p.drawPolygon(QPolygonF{{5,10},{11,4},{19,12},{13,18}});p.drawLine(4,10,13,19);p.setBrush(QColor("#4c97c8"));p.drawEllipse(QRectF{18,16,4,6});p.setPen(QPen(QColor("#ffcd35"),3));p.drawLine(3,22,17,22);}
 else if(label=="Left"||label=="Center"||label=="Right"){for(int i=0;i<4;++i){const int width=i%2?11:18;const int x=label=="Left"?3:label=="Right"?21-width:(24-width)/2;p.drawLine(x,5+i*5,x+width,5+i*5);}}
 else if(label=="Gridlines"||label=="Format…"||label.contains("Pivot")||label=="▾"){p.drawRect(QRectF{3,4,18,16});p.drawLine(3,9,21,9);p.drawLine(3,14,21,14);p.drawLine(9,4,9,20);p.drawLine(15,4,15,20);if(label.contains("Pivot")){p.setPen(QPen(QColor("#3b8f6d"),3));p.drawLine(3,9,21,9);}}
 else if(label.contains("Chart")||label=="Chart"){p.drawLine(3,3,3,21);p.drawLine(3,21,22,21);p.setPen(Qt::NoPen);p.setBrush(QColor("#5593c6"));p.drawRect(6,12,4,8);p.setBrush(QColor("#e4ad47"));p.drawRect(12,7,4,13);p.setBrush(QColor("#64a17e"));p.drawRect(18,4,4,16);}
 else if(label=="B"||label=="I"||label=="U"||label=="A"||label=="$"||label=="%"||label=="ƒx"){QFont font("DejaVu Sans",14);font.setBold(label=="B");font.setItalic(label=="I");font.setUnderline(label=="U");p.setFont(font);p.drawText(canvas.rect(),Qt::AlignCenter,label);if(label=="A"){p.setPen(QPen(QColor("#b33c35"),3));p.drawLine(4,22,20,22);}}
 else if(label=="Wrap"){p.drawLine(3,5,21,5);p.drawLine(3,10,19,10);p.drawPolyline(QPolygonF{{19,10},{19,15},{9,15}});p.drawPolyline(QPolygonF{{12,12},{9,15},{12,18}});p.drawLine(3,20,21,20);}
 else {p.drawRoundedRect(QRectF{3,3,18,18},2,2);p.drawLine(3,8,21,8);p.setPen(QPen(QColor("#4e9675"),2));p.drawPolyline(QPolygonF{{6,16},{10,12},{14,16},{19,10}});}
 p.end();return QIcon(canvas);
}
}
