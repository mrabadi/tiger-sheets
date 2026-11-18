#include "tigersheets/home_ribbon.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/ribbon_page.hpp"
#include "tigersheets/command_icons.hpp"
#include <QComboBox>
#include <QFontComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
namespace tigersheets {
namespace {
QIcon homeIcon(const QString& kind){
 QPixmap pixmap(32,32);pixmap.fill(Qt::transparent);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#555555"),1.2));p.setBrush(Qt::white);
 if(kind=="growFont"||kind=="shrinkFont"){p.setFont(QFont("Calibri",kind=="growFont"?20:15));p.drawText(QRect(1,1,25,28),Qt::AlignCenter,"A");p.setBrush(QColor("#555555"));p.drawPolygon(kind=="growFont"?QPolygon{{25,10},{31,10},{28,5}}:QPolygon{{25,5},{31,5},{28,10}});}
 else if(kind=="borders"||kind=="cellStyles"||kind=="formatAsTable"||kind=="conditionalFormatting"){p.drawRect(3,4,26,24);for(int i=1;i<3;++i){p.drawLine(3+i*9,4,3+i*9,28);p.drawLine(3,4+i*8,29,4+i*8);}if(kind!="borders"){p.fillRect(4,5,24,6,QColor(kind=="conditionalFormatting"?"#ea9090":"#7bb6d8"));if(kind=="cellStyles")p.fillRect(4,14,15,5,QColor("#f0c773"));}}
 else if(kind.startsWith("vertical")){const int top=kind=="verticalTop"?5:kind=="verticalCenter"?11:17;p.drawLine(4,3,28,3);p.drawLine(4,29,28,29);for(int i=0;i<3;++i)p.drawLine(7,top+i*4,25,top+i*4);}
 else if(kind=="orientation"){p.save();p.translate(15,19);p.rotate(-35);p.setFont(QFont("Calibri",12));p.drawText(-11,0,"ab");p.drawLine(-12,4,12,4);p.restore();}
 else if(kind=="mergeCenter"){p.drawRect(3,5,26,22);p.drawLine(3,12,29,12);p.drawLine(3,20,29,20);p.setPen(QPen(QColor("#2687b9"),1.6));p.drawLine(5,16,12,16);p.drawLine(20,16,27,16);p.drawPolyline(QPolygon{{9,13},{12,16},{9,19}});p.drawPolyline(QPolygon{{23,13},{20,16},{23,19}});}
 else if(kind=="insert"||kind=="delete"||kind=="format"){p.drawRect(4,4,24,24);for(int i=1;i<3;++i){p.drawLine(4+i*8,4,4+i*8,28);p.drawLine(4,4+i*8,28,4+i*8);}p.setPen(QPen(QColor(kind=="delete"?"#bb5555":"#368bb5"),2.5));p.drawLine(2,13,14,13);if(kind=="insert")p.drawLine(8,7,8,19);}
 else if(kind=="formatPainter"){p.setBrush(QColor("#d9b15f"));p.drawRect(4,4,22,10);p.setBrush(Qt::NoBrush);p.drawPolyline(QPolygon{{26,9},{30,9},{30,19},{17,19},{17,28}});}
 else if(kind=="clear"){p.setBrush(QColor("#dbb3d2"));p.drawPolygon(QPolygon{{5,21},{17,7},{28,17},{17,29}});p.setPen(QColor("#555555"));p.drawLine(5,29,29,29);}
 else if(kind=="sortFilter"){p.setFont(QFont("Calibri",13));p.setPen(QColor("#2d91b5"));p.drawText(0,12,"A");p.drawText(0,27,"Z");p.setPen(QColor("#555555"));p.drawLine(16,4,16,27);p.drawPolyline(QPolygon{{12,23},{16,27},{20,23}});p.drawPolygon(QPolygon{{20,5},{31,5},{27,12},{27,22},{24,24},{24,12}});}
 else if(kind=="findSelect"){p.setPen(QPen(QColor("#555555"),1.7));p.drawEllipse(3,2,20,20);p.drawLine(20,20,30,30);}
 else if(kind=="analyzeData"){p.drawRect(4,4,24,24);p.fillRect(7,18,4,8,QColor("#5b9aca"));p.fillRect(14,12,4,14,QColor("#5b9aca"));p.fillRect(21,7,4,19,QColor("#5b9aca"));p.drawEllipse(11,8,16,16);p.drawLine(25,23,31,29);}
 else if(kind.endsWith("Dialog")){p.drawLine(9,8,25,8);p.drawLine(25,8,25,24);p.drawLine(11,23,24,10);p.drawLine(11,14,11,23);p.drawLine(11,23,20,23);}
 else if(kind=="comma"){p.setFont(QFont("Calibri",26));p.drawText(QRect(1,-2,30,31),Qt::AlignCenter,",");}
 else return spreadsheetCommandIcon(kind);
 return QIcon(pixmap);
}
void tag(QWidget& widget,const char* key){widget.setProperty("ribbon.keyTip",key);widget.setProperty("ribbon.sourceKeyTip",true);}
}
void arrangeSourceHomeRibbon(MainWindow& window,RibbonPage& page){
 auto* row=page.contentLayout();QList<QWidget*> originals;QList<QWidget*> oldPanels;
 for(int n=0;n<row->count();++n)if(auto* panel=row->itemAt(n)->widget()){oldPanels.push_back(panel);if(auto* outer=qobject_cast<QVBoxLayout*>(panel->layout());outer&&outer->count())if(auto* controls=outer->itemAt(0)->layout())for(int i=0;i<controls->count();++i)if(auto* widget=controls->itemAt(i)->widget())originals.push_back(widget);}
 for(auto* widget:originals)widget->setParent(page.widget());while(auto* item=row->takeAt(0))delete item;for(auto* panel:oldPanels)delete panel;
 auto existing=[&](const char* name,const char* accessible="")->QWidget*{for(auto* widget:originals)if((name&&*name&&widget->objectName()==name)||(!*name&&widget->accessibleName()==accessible))return widget;return nullptr;};
 int sourceLeft=0;auto group=[&](const char* id,const char* title,int width){auto* panel=new QFrame(page.widget());panel->setObjectName("home.group."+QString(id));panel->setProperty("home.sourceLeft",sourceLeft);sourceLeft+=width;panel->setFixedSize(width,100);panel->setStyleSheet("QFrame {border:0; border-right:1px solid #b8b8b8; background:#f3f2f1;}");auto* caption=new QLabel(title,panel);caption->setGeometry(0,84,width-1,15);caption->setAlignment(Qt::AlignHCenter);caption->setStyleSheet("font:12px Calibri; color:#404040; border:0;");row->addWidget(panel);return panel;};
 auto place=[&](QWidget* widget,QWidget* panel,QRect rect,const char* key){if(!widget)return;widget->setParent(panel);widget->setMinimumSize(0,0);widget->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);widget->setFixedSize(rect.size());widget->move(rect.topLeft());tag(*widget,key);const QString tip(key);const bool above=QStringList{"FF","FS","FG","FK","AT","AM","AB","FQ","W","N","I","U","X"}.contains(tip);const int tipY=QStringList{"FN","FA","FM","FO"}.contains(tip)?160:above?59:tip=="C"?96:QStringList{"D","FI"}.contains(tip)?94:135;widget->setProperty("ribbon.keyTipPoint",QPoint(rect.width()/2,tipY-64-rect.y()));if(auto* button=qobject_cast<QToolButton*>(widget)){button->setToolButtonStyle(Qt::ToolButtonIconOnly);button->setIconSize(QSize(18,18));button->setAutoRaise(true);button->setStyleSheet("QToolButton {border:0; padding:0; background:transparent; font:12px Calibri;} QToolButton:hover:enabled {background:#e1e1e1;} QToolButton:checked {background:#d7d7d7; border:1px solid #b8b8b8;}");}widget->show();};
 auto created=[&](QWidget* panel,const char* id,const char* text,QRect rect,const char* key,bool enabled=false){auto* button=new QToolButton(panel);button->setObjectName("home."+QString(id));button->setText(text);button->setAccessibleName(text);button->setToolTip(QString(text)+(enabled?QString():" is unavailable"));button->setIcon(homeIcon(id));place(button,panel,rect,key);button->setEnabled(enabled);if(QString(id).endsWith("Dialog"))button->setIconSize(QSize(8,8));return button;};
 auto launcher=[&](QWidget* panel,const char* id,const char* text,QRect rect,const char* key,int tab){auto* button=created(panel,id,text,rect,key,true);QObject::connect(button,&QToolButton::clicked,&window,[&window,tab]{window.executeCommand("view.ribbon.close");window.executeCommand("cells.format",{{"initialTab",tab}});});return button;};
 auto large=[](QToolButton* button){button->setIconSize(QSize(32,32));button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);};
 auto* clipboard=group("clipboard","Clipboard",99);auto* paste=qobject_cast<QToolButton*>(existing("","Paste"));place(paste,clipboard,{8,6,42,68},"V");if(paste)large(paste);place(existing("","Cut"),clipboard,{55,4,25,24},"X");place(existing("","Copy"),clipboard,{55,29,25,24},"C");created(clipboard,"formatPainter","Format Painter",{55,54,25,24},"FP");place(existing("","▾"),clipboard,{23,68,14,14},"");created(clipboard,"clipboardDialog","Clipboard dialog",{87,84,11,13},"FO");
 auto* font=group("font","Font",232);place(existing("ribbon.fontFamily"),font,{8,12,124,24},"FF");place(existing("ribbon.fontSize"),font,{134,12,52,24},"FS");created(font,"growFont","Increase Font Size",{189,12,22,24},"FG");created(font,"shrinkFont","Decrease Font Size",{211,12,20,24},"FK");place(existing("command.style.bold"),font,{12,42,24,25},"1");place(existing("command.style.italic"),font,{36,42,24,25},"2");place(existing("command.style.underline"),font,{60,42,24,25},"3");auto* borders=existing("tutorial.home.borders");place(borders,font,{98,42,28,25},"B");if(borders)borders->setObjectName("home.borders");place(existing("","Fill"),font,{138,42,28,25},"H");place(existing("","A"),font,{182,42,28,25},"");place(existing("tutorial.home.fontColor.menu"),font,{210,42,12,25},"FC");launcher(font,"fontDialog","Font dialog",{220,84,11,13},"FN",2);
 auto* alignment=group("alignment","Alignment",182);created(alignment,"verticalTop","Top Align",{4,11,24,24},"AT");created(alignment,"verticalCenter","Middle Align",{28,11,24,24},"AM");auto* bottom=created(alignment,"verticalBottom","Bottom Align",{52,11,24,24},"AB");bottom->setCheckable(true);bottom->setChecked(true);created(alignment,"orientation","Orientation",{80,11,27,24},"FQ");place(existing("","Wrap"),alignment,{127,9,28,28},"W");place(existing("","Left"),alignment,{4,42,24,25},"AL");place(existing("","Center"),alignment,{28,42,24,25},"AC");place(existing("","Right"),alignment,{52,42,24,25},"AR");place(existing("tutorial.home.decreaseIndent"),alignment,{77,42,24,25},"5");place(existing("tutorial.home.increaseIndent"),alignment,{101,42,24,25},"6");created(alignment,"mergeCenter","Merge & Center",{129,42,30,25},"M");launcher(alignment,"alignmentDialog","Alignment dialog",{170,84,11,13},"FA",1);
 auto* number=group("number","Number",156);auto* formats=qobject_cast<QComboBox*>(existing("ribbon.numberFormat"));place(formats,number,{8,12,144,24},"N");if(formats)formats->setItemText(4,"Percentage");auto* accounting=existing("tutorial.home.accounting");place(accounting,number,{4,42,34,25},"AN");if(accounting)accounting->setObjectName("home.accounting");place(existing("","%"),number,{47,42,24,25},"P");created(number,"comma","Comma Style",{71,42,24,25},"K");place(existing("tutorial.home.increaseDecimals"),number,{101,42,24,25},"0");place(existing("tutorial.home.decreaseDecimals"),number,{125,42,24,25},"9");place(existing("ribbon.numberFormatOptions"),number,{144,84,11,13},"FM");if(auto* numberLauncher=qobject_cast<QToolButton*>(existing("ribbon.numberFormatOptions"))){numberLauncher->setIcon(homeIcon("numberDialog"));numberLauncher->setIconSize(QSize(8,8));QObject::disconnect(numberLauncher,&QToolButton::clicked,&window,nullptr);QObject::connect(numberLauncher,&QToolButton::clicked,&window,[&window]{window.executeCommand("view.ribbon.close");window.executeCommand("cells.format",{{"initialTab",0}});});}
 auto* styles=group("styles","Styles",194);large(created(styles,"conditionalFormatting","Conditional\nFormatting",{2,6,75,74},"L"));large(created(styles,"formatAsTable","Format as\nTable",{77,6,58,74},"T"));large(created(styles,"cellStyles","Cell\nStyles",{135,6,58,74},"J"));
 auto* cells=group("cells","Cells",94);for(const auto& command:{std::tuple{"insert","Insert","I",5},std::tuple{"delete","Delete","D",30},std::tuple{"format","Format","O",55}}){
  const bool format=QString::fromLatin1(std::get<0>(command))=="format";
  auto* button=created(cells,std::get<0>(command),std::get<1>(command),{7,std::get<3>(command),86,24},std::get<2>(command),format);button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  if(format){
   auto* menu=new QMenu(button);menu->setObjectName("home.formatMenu");menu->setToolTipsVisible(true);
   auto* width=menu->addAction("Column &Width...");width->setToolTip("Set the selected column widths in characters (Alt, H, O, W)");
   QObject::connect(width,&QAction::triggered,&window,[&window]{window.executeCommand("view.ribbon.close");window.executeCommand("columns.size");});
   auto* autoFit=menu->addAction("AutoF&it Column Width");autoFit->setToolTip("Fit the selected columns to their contents (Alt, H, O, I)");
   QObject::connect(autoFit,&QAction::triggered,&window,[&window]{window.executeCommand("view.ribbon.close");window.executeCommand("columns.autofit");});
   button->setMenu(menu);button->setPopupMode(QToolButton::InstantPopup);
  }
 }
 auto* editing=group("editing","Editing",143);auto* sum=existing("","Σ");place(sum,editing,{4,5,36,24},"U");place(existing("","Fill ↓"),editing,{4,30,36,24},"FI");created(editing,"clear","Clear",{4,55,36,24},"E");large(created(editing,"sortFilter","Sort &\nFilter",{41,6,53,74},"S"));large(created(editing,"findSelect","Find &\nSelect",{89,6,52,74},"FD"));auto* analysis=group("analysis","Analysis",62);large(created(analysis,"analyzeData","Analyze\nData",{3,6,56,74},"Y1"));row->addStretch();
}
}
