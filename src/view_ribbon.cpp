#include "tigersheets/view_ribbon.hpp"
#include "tigersheets/home_ribbon.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ribbon_page.hpp"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
namespace tigersheets {
namespace {
QIcon viewIcon(const QString& kind){
 QPixmap image(36,36);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#505050"),1));p.setBrush(Qt::white);
 const QColor accent("#0078b8");
 const auto table=[&]{p.drawRect(4,4,28,28);for(int i=1;i<4;++i){p.drawLine(4+i*7,4,4+i*7,32);p.drawLine(4,4+i*7,32,4+i*7);}};
 const auto frame=[&](int x,int y,int width,int height){p.setPen(QPen(QColor("#505050"),1));p.setBrush(Qt::white);p.drawRect(x,y,width,height);p.fillRect(x+1,y+1,width-1,3,QColor("#c4dfe9"));p.drawLine(x,y+5,x+width,y+5);};
 if(kind=="zoom"||kind=="zoomSelection"){if(kind=="zoomSelection")table();p.setBrush(QColor("#f3f2f1"));p.drawEllipse(5,3,19,19);p.setPen(QPen(QColor("#505050"),2));p.drawLine(22,21,32,32);}
 else if(kind=="zoom100"){p.drawRect(8,3,20,29);p.setPen(accent);p.setFont(QFont("Calibri",10));p.drawText(QRect(5,13,28,15),Qt::AlignCenter,"100");}
 else if(kind=="keepSheetView"||kind=="exitSheetView"||kind=="newSheetView"||kind=="sheetViewOptions"){
  frame(6,3,23,29);p.setPen(QPen(accent,2));
  if(kind=="keepSheetView"){p.setPen(QPen(QColor("#378653"),3));p.drawPolyline(QPolygon{{10,21},{16,27},{27,14}});}
  else if(kind=="exitSheetView"){p.drawLine(14,21,34,21);p.drawPolyline(QPolygon{{27,14},{34,21},{27,28}});}
  else if(kind=="newSheetView"){p.drawLine(18,14,18,28);p.drawLine(11,21,25,21);}
  else{for(int y:{15,21,27})p.drawLine(10,y,26,y);p.setBrush(Qt::white);p.drawEllipse(QPoint(15,15),2,2);p.drawEllipse(QPoint(22,21),2,2);p.drawEllipse(QPoint(17,27),2,2);}
 }
 else if(kind=="normal"){table();}
 else if(kind=="pageBreakPreview"){table();p.setPen(QPen(accent,2,Qt::DashLine));p.drawLine(4,14,32,14);p.drawLine(21,4,21,32);}
 else if(kind=="pageLayout"){p.drawRect(3,4,13,28);p.drawRect(20,4,13,28);p.setPen(QColor("#9c9c9c"));for(int y:{12,17,22}){p.drawLine(6,y,13,y);p.drawLine(23,y,30,y);}}
 else if(kind=="customViews"){table();p.setPen(QPen(accent,2));p.setBrush(Qt::white);p.drawPolygon(QPolygon{{8,25},{20,18},{32,25},{20,32}});p.setBrush(accent);p.drawEllipse(QPoint(20,25),3,3);}
 else if(kind=="freezePanes"){table();p.fillRect(5,5,27,6,QColor("#a5d4e9"));p.fillRect(5,5,6,27,QColor("#a5d4e9"));p.setPen(QPen(accent,2));p.drawLine(4,12,32,12);p.drawLine(12,4,12,32);}
 else if(kind=="newWindow"){frame(3,4,23,20);frame(11,12,22,19);p.setPen(QPen(accent,2.5));p.drawLine(22,18,22,28);p.drawLine(17,23,27,23);}
 else if(kind=="arrangeAll"){for(int y:{4,20})for(int x:{3,20})frame(x,y,13,12);}
 else if(kind=="split"){frame(3,4,30,28);p.setPen(QPen(accent,2));p.drawLine(18,10,18,32);p.drawLine(3,21,33,21);}
 else if(kind=="switchWindows"){frame(3,4,23,20);frame(11,12,22,19);p.setPen(QPen(accent,2));p.drawLine(4,24,17,24);p.drawPolyline(QPolygon{{12,19},{17,24},{12,29}});}
 else if(kind=="navigation"){p.drawRect(4,5,28,25);p.setBrush(QColor("#8ec2dc"));p.drawRect(5,6,7,23);}
 else if(kind=="hide"||kind=="unhide"){frame(3,5,30,25);p.setPen(QPen(accent,1.5));p.drawEllipse(QRectF(8,14,19,10));p.setBrush(accent);p.drawEllipse(QPoint(18,19),3,3);if(kind=="hide"){p.setPen(QPen(QColor("#b44a48"),2.5));p.drawLine(7,28,29,11);}}
 else if(kind=="sideBySide"){frame(2,4,13,28);frame(21,4,13,28);p.setPen(QPen(accent,2));p.drawLine(11,21,25,21);p.drawPolyline(QPolygon{{15,17},{11,21},{15,25}});p.drawPolyline(QPolygon{{21,17},{25,21},{21,25}});}
 else if(kind=="synchronousScroll"){frame(2,4,12,28);frame(22,4,12,28);p.setPen(QPen(accent,2));p.drawLine(18,12,18,28);p.drawPolyline(QPolygon{{14,16},{18,12},{22,16}});p.drawPolyline(QPolygon{{14,24},{18,28},{22,24}});}
 else if(kind=="resetPosition"){frame(3,3,13,14);frame(20,19,13,14);p.setPen(QPen(accent,2));p.drawPolyline(QPolygon{{21,9},{28,9},{28,16}});p.drawPolyline(QPolygon{{24,12},{28,16},{32,12}});p.drawPolyline(QPolygon{{15,27},{8,27},{8,20}});p.drawPolyline(QPolygon{{4,24},{8,20},{12,24}});}
 else if(kind=="macros"){table();p.setPen(QColor("#a23c64"));p.drawRect(14,14,18,18);p.setFont(QFont("Calibri",14));p.drawText(QRect(14,13,18,20),Qt::AlignCenter,"M");}
 else{frame(5,6,26,24);}
 return QIcon(image);
}
void sourcePage(RibbonPage& page){page.setProperty("ribbon.pageHeight",132);page.setProperty("ribbon.contentHeight",100);page.setProperty("ribbon.popupTopMargin",-2);page.contentLayout()->setContentsMargins(0,0,0,0);page.contentLayout()->setSpacing(0);page.widget()->setStyleSheet("background:#f3f2f1;");}
class ViewRibbon final:public QObject {
 MainWindow& window_;QTabWidget& ribbon_;QTableView& view_;WorksheetModel& model_;QPointer<RibbonPage> page_;QCheckBox *gridlines_=nullptr,*formula_=nullptr,*headings_=nullptr;QToolButton *hide_=nullptr,*unhide_=nullptr;
 QSlider* slider()const{return window_.findChild<QSlider*>("worksheet.zoom");}
 void close(){window_.executeCommand("view.ribbon.close");view_.setFocus(Qt::ShortcutFocusReason);}
 void sync(){if(!gridlines_)return;const QSignalBlocker a(gridlines_),b(formula_),c(headings_);gridlines_->setChecked(view_.showGrid());auto* field=window_.findChild<QLineEdit*>("formula.input");formula_->setChecked(field&&field->parentWidget()->isVisible());headings_->setChecked(!view_.horizontalHeader()->isHidden()&&!view_.verticalHeader()->isHidden());int visible=0,hidden=0;for(auto* widget:QApplication::topLevelWidgets())if(auto* window=dynamic_cast<MainWindow*>(widget)){visible+=window->isVisible();hidden+=window->property("view.hiddenWindow").toBool();}hide_->setEnabled(visible>1);unhide_->setEnabled(hidden>0);}
 bool eventFilter(QObject* object,QEvent* event)override{if(object==page_&&event->type()==QEvent::Show)QTimer::singleShot(0,this,[this]{sync();});return false;}
public:
 ViewRibbon(MainWindow& window,QTabWidget& ribbon,QTableView& view,WorksheetModel& model):QObject(&window),window_(window),ribbon_(ribbon),view_(view),model_(model){
  setObjectName("view.controller");for(int index=0;index<ribbon.count();++index)if(auto* page=dynamic_cast<RibbonPage*>(ribbon.widget(index))){if(ribbon.tabText(index)=="Home"){sourcePage(*page);arrangeSourceHomeRibbon(window,*page);}if(ribbon.tabText(index)=="View")page_=page;}if(!page_)return;sourcePage(*page_);auto* row=page_->contentLayout();while(auto* item=row->takeAt(0)){delete item->widget();delete item;}
  auto group=[&](const char* id,const char* caption,int width,int margin=6){auto* panel=new QFrame;panel->setObjectName("view.group."+QString::fromLatin1(id));panel->setFixedSize(width,100);panel->setStyleSheet("QFrame {border:0; border-right:1px solid #b8b8b8;}");auto* column=new QVBoxLayout(panel);column->setContentsMargins(margin,4,margin,2);column->setSpacing(0);auto* content=new QWidget(panel);auto* controls=new QHBoxLayout(content);controls->setContentsMargins(0,0,0,0);controls->setSpacing(0);column->addWidget(content,1);auto* label=new QLabel(caption,panel);label->setFixedHeight(14);label->setAlignment(Qt::AlignHCenter);label->setStyleSheet("font:12px Calibri; color:#404040; border:0;");column->addWidget(label);row->addWidget(panel);return controls;};
  auto control=[&](QLayout* layout,const char* id,const char* text,const char* label,const char* tip,int width,bool small=false,bool enabled=true){auto* button=new QToolButton;button->setObjectName("view."+QString::fromLatin1(id));button->setText(text);button->setAccessibleName(label);button->setToolTip(enabled?QString::fromLatin1(label):QString::fromLatin1(label)+" is unavailable");button->setProperty("ribbon.keyTip",QString::fromLatin1(tip));button->setProperty("ribbon.sourceKeyTip",true);button->setIcon(viewIcon(QString::fromLatin1(id)));button->setIconSize(small?QSize(16,16):QSize(32,32));button->setToolButtonStyle(small?Qt::ToolButtonTextBesideIcon:Qt::ToolButtonTextUnderIcon);button->setFixedSize(width,small?24:72);button->setAutoRaise(true);button->setEnabled(enabled);button->setStyleSheet("QToolButton {font:12px Calibri; color:#303030; border:0; background:transparent; padding:0;} QToolButton:disabled {color:#aaa;} QToolButton:hover {background:#e1e1e1;} QToolButton:checked {background:#d4d4d4; border:1px solid #b2b2b2;}");layout->addWidget(button);return button;};
  auto* sheet=group("sheetView","Sheet View",252);auto* sheetColumn=new QVBoxLayout;sheetColumn->setContentsMargins(0,6,0,0);sheetColumn->setSpacing(4);sheet->addLayout(sheetColumn);auto* choices=new QComboBox;choices->setObjectName("view.sheetView");choices->addItem("Default");choices->setFixedHeight(24);choices->setEnabled(false);choices->setProperty("ribbon.keyTip","VC");choices->setProperty("ribbon.sourceKeyTip",true);sheetColumn->addWidget(choices);auto* sheetButtons=new QHBoxLayout;sheetButtons->setContentsMargins(0,0,0,0);sheetButtons->setSpacing(0);sheetColumn->addLayout(sheetButtons);for(const auto& entry:{std::array<const char*,3>{"keepSheetView","Keep","VK"},{"exitSheetView","Exit","VE"},{"newSheetView","New","VN"},{"sheetViewOptions","Options","VO"}})control(sheetButtons,entry[0],entry[1],entry[1],entry[2],60,true,false);
  auto* books=group("workbookViews","Workbook Views",236);auto* normal=control(books,"normal","Normal","Normal","L",45);normal->setCheckable(true);normal->setChecked(true);connect(normal,&QToolButton::clicked,this,[this,normal]{normal->setChecked(true);close();});control(books,"pageBreakPreview","Page Break\nPreview","Page Break Preview","I",62,false,false);auto* bookColumn=new QVBoxLayout;bookColumn->setContentsMargins(0,0,0,0);bookColumn->setSpacing(3);books->addLayout(bookColumn);control(bookColumn,"pageLayout","Page Layout","Page Layout","P",116,true,false);control(bookColumn,"customViews","Custom Views","Custom Views","C",116,true,false);
  auto* navigation=group("navigation","",60,0);control(navigation,"navigation","Navigation","Navigation","K",60,false,false);
  auto* show=group("show","Show",176);auto* checks=new QGridLayout;checks->setContentsMargins(0,7,0,13);checks->setHorizontalSpacing(3);checks->setVerticalSpacing(10);show->addLayout(checks);auto checkbox=[&](const char* id,const char* text,const char* tip,int r,int c){auto* box=new QCheckBox(text);box->setObjectName("view."+QString::fromLatin1(id));box->setAccessibleName(text);box->setProperty("ribbon.keyTip",QString::fromLatin1(tip));box->setProperty("ribbon.sourceKeyTip",true);box->setStyleSheet("QCheckBox {font:12px Calibri; color:#303030; spacing:3px;} QCheckBox:disabled {color:#aaa;}");checks->addWidget(box,r,c);return box;};auto* ruler=checkbox("ruler","Ruler","R",0,0);ruler->setEnabled(false);ruler->setChecked(true);formula_=checkbox("formulaBar","Formula Bar","VF",0,1);gridlines_=checkbox("gridlines","Gridlines","VG",1,0);headings_=checkbox("headings","Headings","VH",1,1);
  connect(gridlines_,&QCheckBox::clicked,this,[this]{window_.executeCommand("view.gridlines");sync();close();});connect(formula_,&QCheckBox::clicked,this,[this](bool shown){if(auto* field=window_.findChild<QLineEdit*>("formula.input"))field->parentWidget()->setVisible(shown);close();});connect(headings_,&QCheckBox::clicked,this,[this](bool shown){view_.horizontalHeader()->setVisible(shown);view_.verticalHeader()->setVisible(shown);close();});
  auto* zoom=group("zoom","Zoom",154);auto* magnify=control(zoom,"zoom","Zoom","Zoom","Q",42);auto* hundred=control(zoom,"zoom100","100%","100%","J",40);auto* selected=control(zoom,"zoomSelection","Zoom to\nSelection","Zoom to Selection","G",60);connect(magnify,&QToolButton::clicked,this,[this]{close();auto* zoom=slider();if(!zoom)return;bool accepted=false;const int value=QInputDialog::getInt(&window_,"Zoom","Magnification:",zoom->value(),25,400,1,&accepted);if(accepted)zoom->setValue(value);});connect(hundred,&QToolButton::clicked,this,[this]{if(auto* zoom=slider())zoom->setValue(100);close();});connect(selected,&QToolButton::clicked,this,[this]{auto* zoom=slider();const auto ranges=view_.selectionModel()->selection();if(zoom&&!ranges.empty()){const auto& r=ranges.front();const int width=view_.horizontalHeader()->sectionPosition(r.right())+view_.columnWidth(r.right())-view_.horizontalHeader()->sectionPosition(r.left()),height=view_.verticalHeader()->sectionPosition(r.bottom())+view_.rowHeight(r.bottom())-view_.verticalHeader()->sectionPosition(r.top());if(width>0&&height>0)zoom->setValue(std::clamp(qRound(zoom->value()*std::min(double(view_.viewport()->width())/width,double(view_.viewport()->height())/height)),25,400));}close();});
  auto* windows=group("window","Window",296);auto* first=new QVBoxLayout;first->setContentsMargins(0,0,0,0);first->setSpacing(0);windows->addLayout(first);control(first,"newWindow","New Window","New Window","N",110,true,false);auto* arrange=control(first,"arrangeAll","Arrange All","Arrange All","A",110,true);auto* freeze=control(first,"freezePanes","Freeze Panes","Freeze Panes","F",110,true);auto* freezeMenu=new QMenu(freeze);freezeMenu->addAction("Freeze Panes")->setEnabled(false);freezeMenu->addAction("Freeze Top Row",this,[this]{window_.executeCommand("view.freeze");close();});freezeMenu->addAction("Freeze First Column")->setEnabled(false);freeze->setMenu(freezeMenu);freeze->setPopupMode(QToolButton::InstantPopup);connect(arrange,&QToolButton::clicked,this,[this]{QList<MainWindow*> windows;for(auto* widget:QApplication::topLevelWidgets())if(auto* window=dynamic_cast<MainWindow*>(widget);window&&window->isVisible())windows.push_back(window);if(!windows.empty()){const auto area=window_.screen()->availableGeometry();const int width=area.width()/windows.size();for(int i=0;i<windows.size();++i)windows[i]->setGeometry(area.x()+i*width,area.y(),width,area.height());}close();});auto* second=new QVBoxLayout;second->setContentsMargins(0,0,0,0);second->setSpacing(0);windows->addLayout(second);control(second,"split","Split","Split","S",68,true,false);hide_=control(second,"hide","Hide","Hide","H",68,true);unhide_=control(second,"unhide","Unhide","Unhide","U",68,true,false);connect(hide_,&QToolButton::clicked,this,[this]{close();window_.setProperty("view.hiddenWindow",true);window_.hide();});auto* unhideMenu=new QMenu(unhide_);unhide_->setMenu(unhideMenu);unhide_->setPopupMode(QToolButton::InstantPopup);connect(unhideMenu,&QMenu::aboutToShow,this,[this,unhideMenu]{unhideMenu->clear();for(auto* widget:QApplication::topLevelWidgets())if(auto* window=dynamic_cast<MainWindow*>(widget);window&&window->property("view.hiddenWindow").toBool()){QPointer<MainWindow> target=window;unhideMenu->addAction(window->property("workbook.name").toString(),this,[target]{if(target){target->setProperty("view.hiddenWindow",false);target->show();target->activateWindow();}});}});auto* compare=new QVBoxLayout;compare->setContentsMargins(0,0,0,0);compare->setSpacing(0);windows->addLayout(compare);for(const auto& entry:{std::array<const char*,3>{"sideBySide","View Side by Side","B"},std::array<const char*,3>{"synchronousScroll","Synchronous Scrolling","VS"},std::array<const char*,3>{"resetPosition","Reset Window Position","T"}}){auto* button=control(compare,entry[0],"",entry[1],entry[2],28,true,false);button->setToolButtonStyle(Qt::ToolButtonIconOnly);}auto* switcher=control(windows,"switchWindows","Switch\nWindows","Switch Windows","W",72);auto* switchMenu=new QMenu(switcher);switcher->setMenu(switchMenu);switcher->setPopupMode(QToolButton::InstantPopup);connect(switchMenu,&QMenu::aboutToShow,this,[this,switchMenu]{switchMenu->clear();for(auto* widget:QApplication::topLevelWidgets())if(auto* window=dynamic_cast<MainWindow*>(widget);window&&window->isVisible()){QPointer<MainWindow> target=window;auto* action=switchMenu->addAction(window->property("workbook.name").toString(),this,[this,target]{close();if(target){target->raise();target->activateWindow();}});action->setCheckable(true);action->setChecked(window==&window_);}});
  auto* macros=group("macros","Macros",64);control(macros,"macros","Macros","Macros","M",52,false,false);row->addStretch();page_->installEventFilter(this);connect(&model,&QAbstractItemModel::dataChanged,this,[this]{sync();});sync();window_.executeCommand("view.ribbon.open");
 }
};
}
QObject* installViewRibbon(MainWindow& window,QTabWidget& ribbon,QTableView& view,WorksheetModel& model){if(auto* existing=window.findChild<QObject*>("view.controller"))return existing;return new ViewRibbon(window,ribbon,view,model);}
}
