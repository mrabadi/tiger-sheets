#include "tigersheets/worksheet_border_paint.hpp"
#include <QActionGroup>
#include "tigersheets/border_menu.hpp"
#include <QAction>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QWidgetAction>
#include <array>
namespace tigersheets {
namespace {
QIcon commandIcon(const QString& command){
 QPixmap image(24,24);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing,false);
 p.setPen(QPen(QColor("#929292"),1,Qt::DotLine));p.drawRect(3,3,16,16);p.drawLine(11,3,11,19);p.drawLine(3,11,19,11);
 auto edge=[&](const QString& side,int width=1){p.setPen(QPen(Qt::black,width));if(side=="top")p.drawLine(3,3,19,3);if(side=="bottom")p.drawLine(3,19,19,19);if(side=="left")p.drawLine(3,3,3,19);if(side=="right")p.drawLine(19,3,19,19);};
 if(command=="bottom"||command=="top"||command=="left"||command=="right")edge(command);
 if(command=="all"||command=="outside"||command=="thickOutside"||command=="more"){for(const auto* side:{"top","bottom","left","right"})edge(side,command=="thickOutside"?2:1);if(command=="all"||command=="more"){p.setPen(Qt::black);p.drawLine(11,3,11,19);p.drawLine(3,11,19,11);}}
 if(command=="bottomDouble"||command=="topDouble"){edge("bottom");p.drawLine(3,16,19,16);}
 if(command=="thickBottom"||command=="topThick")edge("bottom",3);
 if(command=="topBottom"){edge("bottom");edge("top");}
 if(command=="topThick"||command=="topDouble")edge("top");
 if(command=="draw"||command=="drawGrid"||command=="lineColor"){
  if(command=="drawGrid"){p.setPen(Qt::black);p.drawRect(3,3,16,16);p.drawLine(11,3,11,19);p.drawLine(3,11,19,11);}
  p.setPen(QPen(QColor("#398db3"),3));p.drawLine(8,13,19,2);p.setPen(Qt::black);p.drawLine(6,15,8,11);p.drawLine(6,15,10,13);if(command=="lineColor"){p.setPen(QPen(Qt::black,4));p.drawLine(3,21,21,21);}
 }
 if(command=="erase"){p.setPen(QPen(QColor("#8c5199"),2));p.setBrush(QColor("#c4a0cc"));p.drawPolygon(QPolygon({{4,14},{14,4},{21,11},{11,21}}));p.setBrush(Qt::white);p.drawPolygon(QPolygon({{4,14},{8,10},{15,17},{11,21}}));}
 if(command=="lineStyle"){p.fillRect(image.rect(),Qt::white);p.setPen(QPen(Qt::black,1));p.drawLine(3,5,21,5);p.setPen(QPen(Qt::black,1,Qt::DashLine));p.drawLine(3,12,21,12);p.setPen(QPen(Qt::black,2));p.drawLine(3,20,21,20);}
 return QIcon(image);
}
class BorderMenu final:public QMenu {
 bool keyboardFrame_=false,activeHighlight_=true;
protected:
 void showEvent(QShowEvent* event)override{keyboardFrame_=false;activeHighlight_=true;QMenu::showEvent(event);}
 void mouseMoveEvent(QMouseEvent* event)override{keyboardFrame_=false;activeHighlight_=true;QMenu::mouseMoveEvent(event);update();}
 void keyPressEvent(QKeyEvent* event)override{
  if(event->key()==Qt::Key_Alt){setProperty("border.keyTipsVisible",!property("border.keyTipsVisible").toBool());keyboardFrame_=false;activeHighlight_=false;update();event->accept();return;}
  if(event->key()==Qt::Key_Down||event->key()==Qt::Key_Up||event->key()==Qt::Key_Home||event->key()==Qt::Key_End){keyboardFrame_=!property("border.keyTipsVisible").toBool();activeHighlight_=true;}
  if(!(event->modifiers()&(Qt::ControlModifier|Qt::MetaModifier))){const auto key=QString(QChar(event->key()));for(auto* action:actions())if(action->property("ribbon.keyTip").toString()==key){event->accept();if(!action->isEnabled())return;setActiveAction(action);if(auto* submenu=action->menu())submenu->popup(mapToGlobal(actionGeometry(action).topRight()+QPoint(1,0)));else{close();action->trigger();}return;}}
  QMenu::keyPressEvent(event);
 }
 void keyReleaseEvent(QKeyEvent* event)override{if(event->key()==Qt::Key_Alt){event->accept();return;}QMenu::keyReleaseEvent(event);}
 void paintEvent(QPaintEvent*)override{
  QPainter p(this);p.fillRect(rect(),Qt::white);p.setPen(QColor("#b7b7b7"));p.drawRect(rect().adjusted(0,0,-1,-1));p.setFont(font());
  for(auto* action:actions()){
   const auto row=actionGeometry(action);if(row.isEmpty())continue;
   if(action->isSeparator()){if(action->text().isEmpty()){p.setPen(QColor("#e2e2e2"));p.drawLine(1,row.center().y(),width()-2,row.center().y());}continue;}
   if(action==activeAction()&&action->isEnabled()&&activeHighlight_){p.fillRect(row.adjusted(1,0,-1,0),QColor("#d0d0d0"));if(keyboardFrame_){p.setPen(QPen(Qt::black,2));p.drawRect(row.adjusted(2,1,0,0));}}
   p.save();if(!action->isEnabled())p.setOpacity(.55);action->icon().paint(&p,QRect(9,row.top()+2,24,24));p.restore();
   p.setPen(action->isEnabled()?QColor("#202020"):QColor("#929292"));p.drawText(row.adjusted(38,0,-18,0),Qt::AlignVCenter|Qt::AlignLeft,action->text());
   if(action->menu()){p.setPen(QPen(QColor("#555555"),1));const auto y=row.center().y();p.drawLine(width()-13,y-4,width()-9,y);p.drawLine(width()-9,y,width()-13,y+4);}
  }
  if(property("border.keyTipsVisible").toBool())for(auto* action:actions()){
   const auto key=action->property("ribbon.keyTip").toString();if(key.isEmpty())continue;const auto row=actionGeometry(action);const QRect badge(22,row.top()+15,15,17);p.fillRect(badge,QColor("#5b5b5b"));p.setPen(Qt::white);p.drawText(badge,Qt::AlignCenter,key);
  }
 }
public:
 explicit BorderMenu(QWidget* parent):QMenu(parent){
  setObjectName("tutorial.borderMenu");setAccessibleName("Borders");setFixedWidth(228);QFont text("Arial");text.setPixelSize(14);text.setStretch(85);setFont(text);setToolTipsVisible(true);
  setStyleSheet("QMenu{background:white;border:1px solid #b7b7b7;padding:0px;} QMenu::item{height:28px;min-height:28px;max-height:28px;padding:0px;margin:0px;border:0px;} QMenu::separator{height:4px;padding:0px;margin:0px;}");
 }
 void heading(const QString& title){auto* action=new QWidgetAction(this);action->setText(title);action->setSeparator(true);auto* label=new QLabel(title,this);label->setFixedHeight(title=="Borders"?29:26);label->setIndent(13);auto bold=font();bold.setBold(true);label->setFont(bold);label->setAttribute(Qt::WA_TransparentForMouseEvents);label->setStyleSheet("QLabel{background:white;color:#202020;border:0;padding:0;margin:0;}");action->setDefaultWidget(label);addAction(action);}
};
}
QMenu* createWorksheetBorderMenu(QWidget* parent,const std::function<void(const Json&)>& apply,const std::function<void()>& showMore){
 auto* menu=new BorderMenu(parent);menu->heading("Borders");
 auto add=[&](const char* text,const char* key,const char* icon,bool enabled=true){auto* action=menu->addAction(commandIcon(QString::fromLatin1(icon)),QString::fromLatin1(text));action->setIconVisibleInMenu(true);action->setProperty("ribbon.keyTip",QString::fromLatin1(key));action->setEnabled(enabled);if(!enabled)action->setToolTip("This border command is not supported yet.");return action;};
 menu->setProperty("border.lineStyle","thin");menu->setProperty("border.lineColor","#000000");
 auto currentEdge=[menu]{const auto kind=menu->property("border.lineStyle").toString().toStdString();return kind=="none"?Json():Json{{"style",kind},{"color",menu->property("border.lineColor").toString().toStdString()}};};
 for(const auto& item:std::array<std::array<const char*,3>,4>{{{{"Bottom Border","O","bottom"}},{{"Top Border","P","top"}},{{"Left Border","L","left"}},{{"Right Border","R","right"}}}}){auto* action=add(item[0],item[1],item[2]);QObject::connect(action,&QAction::triggered,menu,[apply,side=std::string(item[2]),currentEdge]{apply({{"border",{{side,currentEdge()}}}});});}
 menu->addSeparator();auto* none=add("No Border","N","none");QObject::connect(none,&QAction::triggered,menu,[apply]{apply({{"border",{{"top",nullptr},{"bottom",nullptr},{"left",nullptr},{"right",nullptr}}}});});
 auto preset=[&](const char* text,const char* key,const char* icon,const char* name){auto* action=add(text,key,icon);QObject::connect(action,&QAction::triggered,menu,[menu,apply,name]{apply({{"borderPreset",name},{"borderLineStyle",menu->property("border.lineStyle").toString().toStdString()},{"borderLineColor",menu->property("border.lineColor").toString().toStdString()}});});};
 preset("All Borders","A","all","all");preset("Outside Borders","S","outside","outside");preset("Thick Outside Borders","T","thickOutside","thickOutside");menu->addSeparator();
 preset("Bottom Double Border","B","bottomDouble","bottomDouble");preset("Thick Bottom Border","H","thickBottom","thickBottom");preset("Top and Bottom Border","D","topBottom","topBottom");preset("Top and Thick Bottom Border","C","topThick","topThick");preset("Top and Double Bottom Border","U","topDouble","topDouble");menu->addSeparator();menu->heading("Draw Borders");
 auto drawing=[&](const char* title,const char* key,const char* icon,const char* mode){auto* action=add(title,key,icon);QObject::connect(action,&QAction::triggered,menu,[menu,apply,mode]{apply({{"borderDrawing",mode},{"borderLineStyle",menu->property("border.lineStyle").toString().toStdString()},{"borderLineColor",menu->property("border.lineColor").toString().toStdString()}});});};drawing("Draw Border","W","draw","draw");drawing("Draw Border Grid","G","drawGrid","grid");drawing("Erase Border","E","erase","erase");
 auto* color=add("Line Color","I","lineColor");auto* colors=new QMenu("Line Color",menu);colors->setObjectName("tutorial.borderLineColor");auto* colorGroup=new QActionGroup(colors);for(const auto& entry:std::array<std::pair<const char*,const char*>,7>{{{"Automatic","#000000"},{"Black","#000000"},{"Blue","#0000ff"},{"Red","#ff0000"},{"Green","#008000"},{"Yellow","#ffff00"},{"White","#ffffff"}}}){QPixmap pixels(18,14);pixels.fill(QColor(entry.second));auto* action=colors->addAction(QIcon(pixels),entry.first);action->setData(entry.second);action->setCheckable(true);colorGroup->addAction(action);action->setChecked(QString::fromLatin1(entry.first)=="Automatic");QObject::connect(action,&QAction::triggered,menu,[menu,value=QString::fromLatin1(entry.second)]{menu->setProperty("border.lineColor",value);});}color->setMenu(colors);
 auto* style=add("Line Style","Y","lineStyle");auto* lineStyles=new QMenu("Line Style",menu);lineStyles->setObjectName("tutorial.borderLineStyle");auto* styleGroup=new QActionGroup(lineStyles);for(const auto* name:{"none","thin","medium","dashed","dotted","thick","double","hair","mediumDashed","dashDot","mediumDashDot","dashDotDot","mediumDashDotDot","slantDashDot"}){QPixmap pixels(22,14);pixels.fill(Qt::transparent);QPainter painter(&pixels);paintWorksheetBorder(painter,QRect(1,5,20,8),"top",name,Qt::black);painter.end();auto* action=lineStyles->addAction(QIcon(pixels),QString::fromLatin1(name));action->setData(name);action->setCheckable(true);styleGroup->addAction(action);action->setChecked(std::string_view(name)=="thin");QObject::connect(action,&QAction::triggered,menu,[menu,value=QString::fromLatin1(name)]{menu->setProperty("border.lineStyle",value);});}style->setMenu(lineStyles);
 menu->addSeparator();auto* more=add("More Borders...","M","more");QObject::connect(more,&QAction::triggered,menu,showMore);return menu;
}
}
