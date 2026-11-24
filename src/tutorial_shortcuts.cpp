#include "tigersheets/tutorial_shortcuts.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/command_icons.hpp"
#include "tigersheets/worksheet_cell_format.hpp"
#include "tigersheets/formatting.hpp"
#include "tigersheets/color_dialog.hpp"
#include "tigersheets/border_menu.hpp"
#include "tigersheets/worksheet_border_drawing.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/ribbon_page.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QAbstractButton>
#include <QColorDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QFrame>
#include <QHideEvent>
#include <QGridLayout>
#include <QLayout>
#include <QLineEdit>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QStatusBar>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <vector>
namespace tigersheets {
namespace {
QIcon borderIcon(const QString& side,bool dropdown=false){QPixmap image(dropdown?32:24,24);image.fill(Qt::transparent);QPainter painter(&image);painter.setPen(QPen(QColor("#a7ada9"),1,Qt::DotLine));painter.drawRect(4,4,16,16);painter.drawLine(12,4,12,20);painter.drawLine(4,12,20,12);painter.setPen(QPen(QColor("#263e35"),2));if(side=="top")painter.drawLine(4,4,20,4);if(side=="bottom")painter.drawLine(4,20,20,20);if(side=="left")painter.drawLine(4,4,4,20);if(side=="right")painter.drawLine(20,4,20,20);if(dropdown){painter.setPen(Qt::NoPen);painter.setBrush(QColor("#555555"));painter.drawPolygon(QPolygon({{24,10},{30,10},{27,14}}));}return QIcon(image);}
QIcon adjustmentIcon(bool increase,bool indent){QPixmap image(24,24);image.fill(Qt::transparent);QPainter painter(&image);painter.setPen(QPen(QColor("#263e35"),1.5));if(indent){for(int y:{5,10,15,20})painter.drawLine(11,y,22,y);painter.drawLine(2,12,8,12);const int x=increase?8:2;const int tail=increase?5:5;painter.drawLine(x,12,tail,9);painter.drawLine(x,12,tail,15);}else{painter.setFont(QFont("DejaVu Sans",8));painter.drawText(QRect(0,0,24,13),Qt::AlignCenter,increase?".00":".0");painter.drawLine(4,18,20,18);const int x=increase?4:20;const int tail=increase?8:16;painter.drawLine(x,18,tail,15);painter.drawLine(x,18,tail,21);}return QIcon(image);}
QIcon fontColorIcon(const QColor& color){QPixmap image(24,24);image.fill(Qt::transparent);QPainter painter(&image);painter.setPen(QColor("#263e35"));painter.setFont(QFont("Calibri",14));painter.drawText(QRect(0,0,24,22),Qt::AlignCenter,"A");painter.setPen(QPen(color,3));painter.drawLine(4,22,20,22);return QIcon(image);}
std::string decimalFormat(std::string code,int direction,const QString& displayed){
 if(code.empty()||QString::fromStdString(code).compare("General",Qt::CaseInsensitive)==0){const auto point=displayed.indexOf('.');int places=point<0?0:displayed.size()-point-1;places=std::clamp(places+direction,0,30);return places?"0."+std::string(places,'0'):"0";}
 if(dateNumberFormat(code)||code=="@")return code;
 std::string result;bool quoted=false,escaped=false,bracket=false;std::size_t start=0;
 auto section=[&](const std::string& source){auto changed=source;
  if(source.find('*')!=std::string::npos){const auto dash=source.find("\"-\"");if(dash!=std::string::npos){const auto first=dash+3;auto end=first;while(end<source.size()&&source[end]=='?')++end;const auto count=static_cast<int>(end-first),next=std::clamp(count+direction,0,30);changed.replace(first,end-first,std::string(next,'?'));return changed;}}
  bool quote=false,skip=false,inside=false;std::size_t integer=std::string::npos,point=std::string::npos,end=std::string::npos;int decimals=0;
  for(std::size_t index=0;index<source.size();++index){const auto c=source[index];if(skip){skip=false;continue;}if(c=='"'){quote=!quote;continue;}if(quote)continue;if(c=='\\'||c=='_'||c=='*'){skip=true;continue;}if(c=='['){inside=true;continue;}if(c==']'){inside=false;continue;}if(inside)continue;if((c=='E'||c=='e')&&integer!=std::string::npos)break;if(c=='/'&&integer!=std::string::npos)return source;if(c=='.'&&integer!=std::string::npos&&point==std::string::npos){point=index;end=index+1;continue;}if(c=='0'||c=='#'||c=='?'){if(point==std::string::npos)integer=index;else if(end==index){++decimals;end=index+1;}}}
  if(integer==std::string::npos)return source;if(direction>0&&decimals<30){if(point==std::string::npos)changed.insert(integer+1,".0");else changed.insert(end,"0");}else if(direction<0&&decimals){changed.erase(end-1,1);if(decimals==1)changed.erase(point,1);}return changed;};
 for(std::size_t index=0;index<=code.size();++index){if(index==code.size()||(!quoted&&!bracket&&!escaped&&code[index]==';')){if(start)result+=';';result+=section(code.substr(start,index-start));start=index+1;continue;}const auto c=code[index];if(escaped){escaped=false;continue;}if(c=='"')quoted=!quoted;else if(!quoted&&(c=='\\'||c=='_'||c=='*'))escaped=true;else if(!quoted&&c=='[')bracket=true;else if(!quoted&&c==']')bracket=false;}
 return result;
}
QIcon paletteCommandIcon(const QString& kind){QPixmap image(22,22);image.fill(Qt::transparent);QPainter painter(&image);if(kind=="more"){painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(QColor("#51655c"),1));painter.setBrush(QColor("#f5f5eb"));painter.drawEllipse(QRectF(3,3,16,15));painter.setPen(Qt::NoPen);for(const auto& item:{std::pair{QPoint(7,7),QColor("#419899")},std::pair{QPoint(12,5),QColor("#bf8c37")},std::pair{QPoint(16,9),QColor("#8a6f9b")},std::pair{QPoint(14,14),QColor("#b15d55")}}){painter.setBrush(item.second);painter.drawEllipse(QPointF(item.first),1.5,1.5);}painter.setPen(QPen(QColor("#51655c"),1));painter.setBrush(Qt::white);painter.drawEllipse(QRectF(5,11,5,5));}else{const QRect box(4,4,14,14);painter.fillRect(box,kind=="automatic"?QColor(Qt::black):QColor("#b7b7b7"));painter.setPen(QColor("#777777"));painter.drawRect(box);if(kind=="none"){painter.setPen(Qt::white);painter.drawRect(box.adjusted(1,1,-1,-1));}}return QIcon(image);}
class PaletteCommand final:public QToolButton {
public:
 explicit PaletteCommand(QWidget* parent):QToolButton(parent){setFocusPolicy(Qt::StrongFocus);QFont text("Arial");text.setPixelSize(13);text.setStretch(95);setFont(text);}
protected:
 void paintEvent(QPaintEvent*)override{QPainter painter(this);painter.fillRect(rect(),hasFocus()||underMouse()?QColor("#d0d0d0"):QColor(Qt::white));if(hasFocus()){painter.setPen(QPen(Qt::black,1));painter.drawRect(rect().adjusted(0,0,-1,-1));}icon().paint(&painter,QRect(10,(height()-22)/2,22,22));painter.setFont(font());painter.setPen(QColor("#202020"));painter.drawText(QRect(36,0,width()-38,height()),Qt::AlignLeft|Qt::AlignVCenter,text());}
};
class FillPalette final : public QFrame {
 MainWindow& window_;
 WorksheetModel& model_;
 WorksheetView& view_;
 bool font_=false;
 const char* colorProperty()const{return font_?"fontColor":"fillColor";}
 const char* styleProperty()const{return font_?"color":"fill";}
 std::vector<QToolButton*> colors_;
 std::vector<QLabel*> keyTips_;
 int focus_=0;
 QPointer<WorksheetView> previewView_;
 void clearPreview(){if(previewView_){previewView_->setProperty("worksheet.stylePreview",QVariant());previewView_->viewport()->update();}}
 void showKeyTips(bool visible){setProperty("palette.keyTipsVisible",visible);for(auto* tip:keyTips_)tip->setVisible(visible);}
 void preview(const QString& color){const auto selection=view_.selectionModel()->selection();const auto range=selection.empty()?QItemSelectionRange(view_.currentIndex()):selection.front();if(!range.isValid())return;view_.setProperty("worksheet.stylePreview",QVariantMap{{"range",QRect(range.left(),range.top(),range.width(),range.height())},{"property",QString::fromLatin1(styleProperty())},{"color",color}});view_.viewport()->update();}
 void choose(const QString& color){try{if(view_.commitPendingEditor()&&formatWorksheetCells(&window_,model_,view_,{{"style",{{styleProperty(),color.toStdString()}}}})){if(font_){window_.setProperty("tutorial.currentFontColor",color);if(auto* button=window_.findChild<QToolButton*>("tutorial.home.fontColor"))button->setIcon(fontColorIcon(QColor(color.isEmpty()?QString("#000000"):color)));}if(!color.isEmpty()){auto recent=window_.property(font_?"tutorial.recentFontColors":"tutorial.recentFillColors").toStringList();recent.removeAll(color);recent.prepend(color);while(recent.size()>10)recent.removeLast();window_.setProperty(font_?"tutorial.recentFontColors":"tutorial.recentFillColors",recent);}}else window_.statusBar()->showMessage(model_.lastError());}catch(const std::exception& error){window_.statusBar()->showMessage(QString::fromUtf8(error.what()));}close();view_.setFocus(Qt::ShortcutFocusReason);}
 bool navigate(QObject* object,QKeyEvent* key){
  if(key->key()==Qt::Key_Escape){close();view_.setFocus(Qt::ShortcutFocusReason);return true;}
  if(key->key()==Qt::Key_Alt){if(!key->isAutoRepeat())showKeyTips(!property("palette.keyTipsVisible").toBool());return true;}
  if(key->key()==Qt::Key_M){findChild<QToolButton*>(font_?"tutorial.font.moreColors":"tutorial.fill.moreColors")->click();return true;}
  if(!font_&&key->key()==Qt::Key_N){choose({});return true;}
  int move=0;switch(key->key()){case Qt::Key_Left:move=-1;break;case Qt::Key_Right:move=1;break;case Qt::Key_Up:move=-10;break;case Qt::Key_Down:move=10;break;}
  if(move){focus_=std::clamp(focus_+move,0,static_cast<int>(colors_.size())-1);colors_[focus_]->setFocus(Qt::TabFocusReason);return true;}
  if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){if(auto* button=qobject_cast<QToolButton*>(object)){if(!button->property(colorProperty()).isValid())return false;choose(button->property(colorProperty()).toString());}else choose(colors_[focus_]->property(colorProperty()).toString());return true;}return false;
 }
 bool eventFilter(QObject* object,QEvent* event) override {
  auto* widget=qobject_cast<QWidget*>(object);if(!widget||widget->window()!=this)return false;
  if((event->type()==QEvent::FocusIn||event->type()==QEvent::Enter)&&widget->property("fillIndex").isValid()){focus_=widget->property("fillIndex").toInt();preview(widget->property(colorProperty()).toString());}
  if(event->type()==QEvent::Leave&&widget->property("fillIndex").isValid())clearPreview();
  if(event->type()==QEvent::KeyPress)return navigate(object,static_cast<QKeyEvent*>(event));
  if(event->type()==QEvent::KeyRelease&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Alt)return true;
  if(event->type()==QEvent::ShortcutOverride){auto* key=static_cast<QKeyEvent*>(event);if(key->key()==Qt::Key_Escape||key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter||key->key()==Qt::Key_Alt||key->key()==Qt::Key_M||(!font_&&key->key()==Qt::Key_N)||(key->key()>=Qt::Key_Left&&key->key()<=Qt::Key_Down)||(key->modifiers()&Qt::AltModifier)){key->accept();return true;}}
  return false;
 }
public:
 FillPalette(MainWindow& window,WorksheetModel& model,WorksheetView& view,bool font=false,bool keyboard=false):QFrame(&window,Qt::Popup|Qt::FramelessWindowHint),window_(window),model_(model),view_(view),font_(font){
  previewView_=&view;
  setObjectName(font_?"tutorial.fontPalette":"tutorial.fillPalette");setAccessibleName(font_?"Font Color":"Fill Color");setAttribute(Qt::WA_DeleteOnClose);setFrameShape(QFrame::NoFrame);setStyleSheet("QFrame{background:white;border:1px solid #b7b7b7;} QLabel{border:0;color:#202020;}");
  const auto recent=window_.property(font_?"tutorial.recentFontColors":"tutorial.recentFillColors").toStringList();const int extra=recent.isEmpty()?0:57,offset=font_?31:0;setFixedSize(174,255+extra);
  auto divider=[&](int y){auto* line=new QWidget(this);line->setGeometry(1,y,172,1);line->setStyleSheet("background:#dedede;border:0;");};if(font_)divider(30);divider(135+offset);divider(192+offset);if(extra)divider(249+offset);
  const auto initial=QString::fromStdString(model_.cellStyle(view_.currentIndex().row(),view_.currentIndex().column()).value(styleProperty(),std::string()));
  auto group=[&](const QString& title,const QStringList& values){const bool theme=title=="Theme Colors",standard=title=="Standard Colors";const int titleY=theme?8+offset:standard?144+offset:201+offset;auto* label=new QLabel(title,this);label->setAccessibleName(title);label->setGeometry(15,titleY,155,15);QFont heading("Arial");heading.setPixelSize(13);heading.setStretch(90);heading.setBold(true);label->setFont(heading);
   for(int index=0;index<values.size();++index){const auto color=values[index].toLower();auto* button=new QToolButton(this);const int row=index/10,y=theme?(row?58+offset+(row-1)*13:33+offset):standard?169+offset:226+offset;button->setGeometry(5+(index%10)*17,y,12,theme&&row?13:12);button->setFocusPolicy(Qt::StrongFocus);button->setProperty(colorProperty(),color);button->setProperty("fillIndex",static_cast<int>(colors_.size()));button->setAccessibleName(QString(font_?"Font color ":"Fill color ")+color.toUpper());button->setToolTip(color.toUpper());
    if(theme){const QStringList names={"White, Background 1","Black, Text 1","Light Gray, Background 2","Blue-Gray, Text 2","Blue, Accent 1","Orange, Accent 2","Gray, Accent 3","Gold, Accent 4","Blue, Accent 5","Green, Accent 6"};QString name=names[index%10];if(index>=10){const int whiteDarker[]={0,5,15,25,35,50};if(index%10==0)name+=QString(", Darker %1%").arg(whiteDarker[row]);else{const bool dark=index%10==1||index%10==3;const int light[]={0,80,60,40,25,50};name+=QString(", %1 %2%").arg(row<=3||dark?"Lighter":"Darker").arg(light[row]);}}button->setToolTip(name);}
    const bool current=color==(initial.isEmpty()?(font_?QString("#000000"):QString("#ffffff")):initial.toLower());button->setStyleSheet("QToolButton{min-width:0;min-height:0;border-radius:0;background:"+color+";border:1px solid "+(current?QString("#e8792b"):QString("#d0d0d0"))+";padding:0;} QToolButton:hover,QToolButton:focus{border:1px solid #e8792b;}");connect(button,&QToolButton::clicked,this,[this,color]{choose(color);});colors_.push_back(button);
   }
  };
  group("Theme Colors",{
   "#ffffff","#000000","#e7e6e6","#44546a","#4472c4","#ed7d31","#a5a5a5","#ffc000","#5b9bd5","#70ad47",
   "#f2f2f2","#7f7f7f","#d0cece","#d6dce4","#d9e2f3","#fbe4d5","#ededed","#fff2cc","#deebf6","#e2efd9",
   "#d9d9d9","#595959","#aeaaaa","#adb9ca","#b4c6e7","#f7caac","#dbdbdb","#ffe599","#bdd7ee","#c5e0b3",
   "#bfbfbf","#3f3f3f","#757171","#8497b0","#8eaadb","#f4b083","#c9c9c9","#ffd966","#9bc2e6","#a8d08d",
   "#a5a5a5","#262626","#3a3838","#333f4f","#2f5496","#c55a11","#7b7b7b","#bf9000","#2f75b5","#548235",
   "#7f7f7f","#0c0c0c","#171616","#222a35","#1f3864","#833c0b","#525252","#7f6000","#1f4e78","#375623"});
  group("Standard Colors",{"#c00000","#ff0000","#ffc000","#ffff00","#92d050","#00b050","#00b0f0","#0070c0","#002060","#7030a0"});if(extra)group("Recent Colors",recent);
  auto* none=new PaletteCommand(this);none->setObjectName(font_?"tutorial.font.automatic":"tutorial.fill.noFill");none->setText(font_?"Automatic":"No Fill");none->setAccessibleName(font_?"Automatic font color":"No Fill");none->setProperty(colorProperty(),QString());none->setIcon(paletteCommandIcon(font_?"automatic":"none"));none->setGeometry(2,font_?2:193+extra,170,font_?27:30);connect(none,&QToolButton::clicked,this,[this]{choose({});});
  auto* more=new PaletteCommand(this);more->setObjectName(font_?"tutorial.font.moreColors":"tutorial.fill.moreColors");more->setText("More Colors...");more->setAccessibleName("More Colors");more->setIcon(paletteCommandIcon("more"));more->setGeometry(2,224+extra,170,30);connect(more,&QToolButton::clicked,this,[this]{hide();const auto color=chooseWorksheetColor(&window_,QColor(QString::fromStdString(model_.cellStyle(view_.currentIndex().row(),view_.currentIndex().column()).value(styleProperty(),std::string(font_?"#000000":"#ffffff")))));if(color.isValid())choose(color.name());else close();});
  auto keyTip=[&](const QString& key,QToolButton* command){command->setProperty("ribbon.keyTip",key);auto* tip=new QLabel(key,this);tip->setObjectName("tutorial.palette.keyTip."+key);tip->setAccessibleName("Key tip "+key);tip->setAttribute(Qt::WA_TransparentForMouseEvents);tip->setAlignment(Qt::AlignCenter);tip->setStyleSheet("QLabel{background:#5b5b5b;color:white;border:0;font:12px Arial;}");tip->setGeometry(23,command->y()+16,15,15);tip->raise();keyTips_.push_back(tip);};if(!font_)keyTip("N",none);keyTip("M",more);showKeyTips(keyboard);
  for(int index=0;index<static_cast<int>(colors_.size());++index)if(colors_[index]->property(colorProperty()).toString()==initial){focus_=index;break;}qApp->installEventFilter(this);
 }
 ~FillPalette() override {clearPreview();qApp->removeEventFilter(this);}
 void focusColor(){if(font_)findChild<QToolButton*>("tutorial.font.automatic")->setFocus(Qt::PopupFocusReason);else colors_[focus_]->setFocus(Qt::PopupFocusReason);}
protected:
 void hideEvent(QHideEvent* event)override{clearPreview();QFrame::hideEvent(event);}
};
QPoint popupPosition(QWidget* anchor,WorksheetView& view,const QPoint& offset){
 if(anchor&&anchor->isVisible()){
  bool visible=true;for(auto* parent=anchor->parentWidget();parent;parent=parent->parentWidget())if(auto* page=dynamic_cast<RibbonPage*>(parent)){const QRect bounds(page->viewport()->mapToGlobal(QPoint()),page->viewport()->size());visible=bounds.contains(QRect(anchor->mapToGlobal(QPoint()),anchor->size()));break;}
  if(visible)return anchor->mapToGlobal(offset);
 }
 const auto rect=view.visualRect(view.currentIndex());return view.viewport()->mapToGlobal(rect.bottomLeft());
}
void showPalette(MainWindow& window,WorksheetModel& model,WorksheetView& view,bool font,bool keyboard){const QString name=font?"tutorial.fontPalette":"tutorial.fillPalette";if(auto* existing=window.findChild<QWidget*>(name)){existing->setObjectName(name+".retired");existing->close();}auto* palette=new FillPalette(window,model,view,font,keyboard);auto* anchor=window.findChild<QToolButton*>(font?"tutorial.home.fontColor":"tutorial.home.fill");const auto position=popupPosition(anchor,view,QPoint(font?-3:-8,anchor?anchor->height():0));palette->move(position);palette->show();palette->focusColor();}
class TutorialShortcuts final : public QObject {
 enum class Mode { Idle,Root,Home,View,Show,Review,LegacyFormat,LegacyColumn,LegacyEdit };
 MainWindow& window_;
 WorksheetModel& model_;
 WorksheetView& view_;
 QTabWidget& ribbon_;
 Mode mode_=Mode::Idle;
 QString homePrefix_;
 bool altDown_=false;
 int dimensionAnchor_=-1,dimensionExtent_=-1;
 QPersistentModelIndex dimensionCurrent_;
 bool dimensionRows_=false,dimensionExtending_=false;
 QPointer<QWidget> returnFocus_;
 std::vector<QPointer<QLabel>> badges_;
 QToolButton* center_=nullptr;
 QToolButton* borders_=nullptr;
 QToolButton* increaseDecimals_=nullptr;
 QToolButton* decreaseDecimals_=nullptr;
 QToolButton* fontColor_=nullptr;
 QToolButton* fontMenu_=nullptr;
 QToolButton* accounting_=nullptr;
 QSpinBox* fontSize_=nullptr;
 QToolButton* increaseIndent_=nullptr;
 QToolButton* decreaseIndent_=nullptr;
 QToolButton* fill_=nullptr;
 QMenu* borderMenu_=nullptr;
 bool formattingReady()const{const auto mode=view_.property("worksheet.entryMode").toString();return mode.isEmpty()||mode=="Ready";}
 void syncEntryChrome(){
  const bool ready=formattingReady();for(auto* control:{fontColor_,fontMenu_,accounting_,center_,fill_,borders_,increaseDecimals_,decreaseDecimals_})if(control)control->setEnabled(ready);
  if(fontSize_){const QSignalBlocker blocked(fontSize_);const auto pointSize=view_.property("worksheet.entryFontSize");const auto* editor=fontSize_->findChild<QLineEdit*>();const bool sizeDraft=editor&&(fontSize_->hasFocus()||editor->hasFocus())&&editor->isModified()&&fontSize_->cleanText()!=QString::number(fontSize_->value());if(!sizeDraft)fontSize_->setValue(qRound(pointSize.isValid()?pointSize.toDouble():model_.cellStyle(view_.currentIndex().row(),view_.currentIndex().column()).value("fontSize",11.0)));fontSize_->setEnabled(ready);}
 }
 QLabel* legacyBanner_=nullptr;
 void showLegacyAccess(const QString& keys){
  clearBadges();if(!legacyBanner_){legacyBanner_=new QLabel(&window_);legacyBanner_->setObjectName("tutorial.legacyAccessKey");legacyBanner_->setAccessibleName("Office access key");legacyBanner_->setWordWrap(true);legacyBanner_->setStyleSheet("QLabel { background: #ffffff; color: #202020; border: 1px solid #b5b5b5; padding: 8px 16px; font: 11px Calibri; }");legacyBanner_->setFixedWidth(247);}
  legacyBanner_->setText("<b>Office access key: "+keys.toHtmlEscaped()+",</b><br><br>Continue typing the menu key sequence from an earlier version of Office or press ESC to cancel.");legacyBanner_->adjustSize();legacyBanner_->move((window_.width()-legacyBanner_->width())/2,10);legacyBanner_->show();legacyBanner_->raise();
 }
 void clearBadges(){for(const auto& badge:badges_)delete badge.data();badges_.clear();}
 void badge(const QString& key,QWidget* anchor,const QPoint& point){
  if(!anchor)return;QRect ribbonBounds;
  if(auto* page=dynamic_cast<RibbonPage*>(ribbon_.currentWidget());page&&page->isAncestorOf(anchor)){
   if(!anchor->isVisible())return;ribbonBounds=QRect(page->viewport()->mapToGlobal(QPoint()),page->viewport()->size());
   const QRect control(anchor->mapToGlobal(QPoint()),anchor->size());
   if(control.left()<ribbonBounds.left()||control.right()>ribbonBounds.right())return;
  }
  auto* owner=anchor->window();auto* label=new QLabel(key,owner);label->setObjectName("tutorial.keyTip."+key);label->setAccessibleName("Key tip "+key);label->setAttribute(Qt::WA_TransparentForMouseEvents);label->setStyleSheet("QLabel { background: #fffce8; color: #202020; border: 1px solid #8b897d; border-radius: 2px; padding: 1px 4px; font: 10px sans-serif; }");
  const bool sourceTip=anchor->property("ribbon.sourceKeyTip").toBool();if(sourceTip)label->setStyleSheet("QLabel {background:#5b5b5b; color:white; border:0; border-radius:0; padding:1px 3px; font:12px Calibri;}");label->adjustSize();
  const auto global=anchor->mapToGlobal(point);const int left=global.x()-label->width()/2;
  if(!ribbonBounds.isNull()&&(left<ribbonBounds.left()||left+label->width()-1>ribbonBounds.right())){delete label;return;}
  if(sourceTip){label->setWindowFlags(Qt::ToolTip|Qt::FramelessWindowHint);label->setAttribute(Qt::WA_ShowWithoutActivating);label->setFocusPolicy(Qt::NoFocus);}const auto center=sourceTip?global:anchor->mapTo(owner,point);label->move(center.x()-label->width()/2,center.y());label->show();label->raise();badges_.push_back(label);
 }
 QWidget* command(const QString& text){for(auto* control:ribbon_.currentWidget()->findChildren<QAbstractButton*>())if(control->accessibleName()==text)return control;return nullptr;}
 void tabTip(const QString& text,const QString& key){for(int index=0;index<ribbon_.count();++index)if(ribbon_.tabText(index)==text){const auto rect=ribbon_.tabBar()->tabRect(index);badge(key,ribbon_.tabBar(),QPoint(rect.center().x(),rect.bottom()-4));}}
 void showTips(){clearBadges();if(auto* page=dynamic_cast<RibbonPage*>(ribbon_.currentWidget())){connect(page->horizontalScrollBar(),&QScrollBar::valueChanged,this,&TutorialShortcuts::showTips,Qt::UniqueConnection);connect(page->horizontalScrollBar(),&QScrollBar::rangeChanged,this,&TutorialShortcuts::showTips,Qt::UniqueConnection);}if(mode_==Mode::Root){for(const auto& [title,key]:{std::pair{"Home","H"},std::pair{"Insert","N"},std::pair{"Page Layout","P"},std::pair{"Formulas","M"},std::pair{"Data","A"},std::pair{"Review","R"},std::pair{"View","W"},std::pair{"Developer","L"}})tabTip(QString::fromLatin1(title),QString::fromLatin1(key));const QList<QWidget*> controls={fontColor_,accounting_,increaseDecimals_,decreaseDecimals_,center_,fill_,borders_,fontSize_};for(int index=0;index<controls.size();++index)if(auto* control=controls[index];control&&control->isVisible())badge(QString::number(index+1),control,QPoint(control->width()/2,control->height()-3));}else if(mode_==Mode::Home){for(auto* control:ribbon_.currentWidget()->findChildren<QWidget*>()){const auto key=control->property("ribbon.keyTip").toString();if(!key.isEmpty()&&(homePrefix_.isEmpty()||key.startsWith(homePrefix_)))badge(key,control,control->property("ribbon.keyTipPoint").isValid()?control->property("ribbon.keyTipPoint").toPoint():QPoint(control->width()/2,control->height()-4));}}else if(mode_==Mode::Review){bool notes=false;for(auto* control:ribbon_.currentWidget()->findChildren<QToolButton*>()){auto key=control->property("ribbon.keyTip").toString();if(key.isEmpty()&&(control->accessibleName()=="Notes"||control->accessibleName()=="Cell note"))key="T";if(!key.isEmpty()){badge(key,control,QPoint(control->width()/2,control->height()-3));if(key=="T")notes=true;}}}else if(mode_==Mode::View||mode_==Mode::Show){for(auto* control:ribbon_.currentWidget()->findChildren<QWidget*>()){const auto key=control->property("ribbon.keyTip").toString();if(key.isEmpty()||(mode_==Mode::Show&&!key.startsWith('V')))continue;badge(mode_==Mode::Show?key.mid(1):key,control,QPoint(control->width()/2,control->height()-4));}}}
 void reset(bool focus=true){mode_=Mode::Idle;homePrefix_.clear();clearBadges();if(legacyBanner_)legacyBanner_->hide();window_.executeCommand("view.ribbon.close");if(focus&&returnFocus_)returnFocus_->setFocus(Qt::ShortcutFocusReason);returnFocus_.clear();}
 bool selectTab(const QString& text){for(int index=0;index<ribbon_.count();++index)if(ribbon_.tabText(index)==text){ribbon_.setCurrentIndex(index);window_.executeCommand("view.ribbon.open");return true;}return false;}
 bool owned(QObject* object) const {auto* widget=qobject_cast<QWidget*>(object);return widget&&(widget->window()==&window_||(widget->window()->objectName()=="ribbon.peek"&&window_.isAncestorOf(widget)));}
 bool keyTipKey(int key) const {
  if(key==Qt::Key_Alt||key==Qt::Key_Escape)return true;
  if(mode_==Mode::Root){switch(key){case Qt::Key_H:case Qt::Key_N:case Qt::Key_P:case Qt::Key_M:case Qt::Key_A:case Qt::Key_R:case Qt::Key_W:case Qt::Key_L:case Qt::Key_O:case Qt::Key_E:case Qt::Key_1:case Qt::Key_2:case Qt::Key_4:case Qt::Key_8:case Qt::Key_3:case Qt::Key_5:case Qt::Key_6:case Qt::Key_7:return true;default:return false;}}
  if(mode_==Mode::Home)return (key>=Qt::Key_A&&key<=Qt::Key_Z)||(key>=Qt::Key_0&&key<=Qt::Key_9);
  if(mode_==Mode::View)return key>=Qt::Key_A&&key<=Qt::Key_Z;
  if(mode_==Mode::Review)return key==Qt::Key_T;
  if(mode_==Mode::Show)return key==Qt::Key_G||key==Qt::Key_F||key==Qt::Key_H||key==Qt::Key_R;
  if(mode_==Mode::LegacyFormat)return key==Qt::Key_C;
  if(mode_==Mode::LegacyColumn)return key==Qt::Key_W||key==Qt::Key_A;
  if(mode_==Mode::LegacyEdit)return key==Qt::Key_S;
  return false;
 }
 void format(const Json& style){try{if(!view_.commitPendingEditor())return;if(style.contains("borderDrawing")){beginWorksheetBorderDrawing(view_,model_,QString::fromStdString(style.at("borderDrawing").get<std::string>()),QString::fromStdString(style.at("borderLineStyle").get<std::string>()),QString::fromStdString(style.at("borderLineColor").get<std::string>()));return;}const bool applied=style.contains("borderPreset")?formatWorksheetBorders(model_,view_,style):formatWorksheetCells(&window_,model_,view_,{{"style",style}});if(!applied)window_.statusBar()->showMessage(model_.lastError());}catch(const std::exception& error){window_.statusBar()->showMessage(QString::fromUtf8(error.what()));}}
 void formatEach(const std::function<Json(int,int)>& patch){try{if(!view_.commitPendingEditor()||!formatWorksheetCells(&window_,model_,view_,{{"style",Json::object()}}))return;const auto selection=view_.selectionModel()->selection();const auto current=view_.currentIndex().isValid()?view_.currentIndex():model_.index(0,0);const auto range=selection.empty()?QItemSelectionRange(current):selection.front();Json operations=Json::array();for(int row=range.top();row<=range.bottom();++row)for(int col=range.left();col<=range.right();++col){const auto style=validatedWorksheetCellStyle(patch(row,col));if(!style.empty())operations.push_back({{"type","set_style"},{"sheet",model_.sheetId().toStdString()},{"row",row+1},{"col",col+1},{"style",style}});}if(!model_.apply(operations))window_.statusBar()->showMessage(model_.lastError());}catch(const std::exception& error){window_.statusBar()->showMessage(QString::fromUtf8(error.what()));}}
 void centerSelection(){format({{"alignment",model_.cellStyle(view_.currentIndex().row(),view_.currentIndex().column()).value("alignment",std::string())=="center"?"":"center"}});}
 void decimals(int direction){formatEach([&](int row,int col){const auto code=model_.cellStyle(row,col).value("numberFormat",std::string("General"));const auto changed=decimalFormat(code,direction,model_.data(model_.index(row,col)).toString());return changed==code?Json::object():Json{{"numberFormat",changed}};});}
 void indent(int direction){formatEach([&](int row,int col){
  const auto style=model_.cellStyle(row,col);const auto value=style.value("indent",0);const int changed=std::clamp(value+direction,0,250);if(changed==value)return Json::object();
  Json patch={{"indent",changed}};const auto alignment=style.value("alignment",std::string("general"));const auto input=model_.raw(row,col);
  if(direction>0&&(alignment.empty()||alignment=="general")&&!input.isEmpty()&&!input.startsWith('=')&&!model_.numericValue(row,col))patch["alignment"]="left";
  return patch;
 });}
 QToolButton* homeControl(QWidget* sibling,const QString& name,const QString& title,const QString& text){if(!sibling||!sibling->parentWidget()->layout())return nullptr;auto* layout=sibling->parentWidget()->layout()->itemAt(0)->layout();if(!layout)return nullptr;auto* button=new QToolButton(sibling->parentWidget());button->setObjectName("tutorial.home."+name);button->setAccessibleName(title);button->setToolTip(title);button->setText(text);button->setIcon(adjustmentIcon(title.startsWith("Increase"),title.contains("Indent")));button->setToolButtonStyle(Qt::ToolButtonIconOnly);button->setAutoRaise(true);layout->addWidget(button);return button;}
 void openBorders(bool keyboard=true){const auto position=popupPosition(borders_,view_,QPoint(0,borders_?borders_->height():0));reset();if(borders_){borderMenu_->setProperty("border.keyTipsVisible",keyboard);borderMenu_->popup(position);if(keyboard)for(auto* action:borderMenu_->actions())if(!action->isSeparator()&&action->isEnabled()){borderMenu_->setActiveAction(action);break;}}}
 bool dimensionKey(QObject* object,QKeyEvent* key,bool overrideOnly){
  if(object!=&view_&&object!=view_.viewport())return false;const auto current=view_.currentIndex();if(!current.isValid())return false;
  const bool rowSpace=key->key()==Qt::Key_Space&&key->modifiers()==Qt::ShiftModifier,columnSpace=key->key()==Qt::Key_Space&&key->modifiers()==Qt::ControlModifier,allSpace=key->key()==Qt::Key_Space&&key->modifiers()==(Qt::ControlModifier|Qt::ShiftModifier);
  const auto selection=view_.selectionModel()->selection();if(selection.size()!=1&&!rowSpace&&!columnSpace&&!allSpace)return false;const auto range=selection.empty()?QItemSelectionRange(current):selection.front();const bool rows=range.width()==model_.columnCount()&&range.height()!=model_.rowCount(),columns=range.height()==model_.rowCount()&&range.width()!=model_.columnCount();
  const bool extend=key->modifiers()==Qt::ShiftModifier&&((rows&&(key->key()==Qt::Key_Up||key->key()==Qt::Key_Down))||(columns&&(key->key()==Qt::Key_Left||key->key()==Qt::Key_Right)));
  if(!rowSpace&&!columnSpace&&!allSpace&&!extend)return false;if(overrideOnly){key->accept();return true;}if(mode_!=Mode::Idle)reset(false);
  if(allSpace){view_.selectAll();return true;}const bool selectRows=rowSpace||(extend&&rows);const int coordinate=selectRows?current.row():current.column(),limit=selectRows?model_.rowCount():model_.columnCount();
  if(rowSpace||columnSpace){dimensionAnchor_=std::min(coordinate,selectRows?range.top():range.left());dimensionExtent_=std::max(selectRows?range.bottom():range.right(),coordinate+(selectRows?view_.rowSpan(current.row(),current.column()):view_.columnSpan(current.row(),current.column()))-1);dimensionRows_=selectRows;dimensionCurrent_=current;dimensionExtending_=false;}
  else{const int first=selectRows?range.top():range.left(),last=selectRows?range.bottom():range.right();const bool backwards=key->key()==Qt::Key_Up||key->key()==Qt::Key_Left;if(!dimensionExtending_||dimensionRows_!=selectRows||dimensionCurrent_!=current||first!=std::min(dimensionAnchor_,dimensionExtent_)||last!=std::max(dimensionAnchor_,dimensionExtent_)){dimensionRows_=selectRows;dimensionCurrent_=current;dimensionAnchor_=backwards?last:first;dimensionExtent_=backwards?first:last;dimensionExtending_=true;}dimensionExtent_=std::clamp(dimensionExtent_+(backwards?-1:1),0,limit-1);}
  const int first=std::min(dimensionAnchor_,dimensionExtent_),last=std::max(dimensionAnchor_,dimensionExtent_);view_.selectionModel()->select(QItemSelection(model_.index(selectRows?first:0,selectRows?0:first),model_.index(selectRows?last:model_.rowCount()-1,selectRows?model_.columnCount()-1:last)),QItemSelectionModel::ClearAndSelect);view_.selectionModel()->setCurrentIndex(current,QItemSelectionModel::NoUpdate);view_.scrollTo(model_.index(selectRows?dimensionExtent_:current.row(),selectRows?current.column():dimensionExtent_));if(columnSpace)view_.setProperty("worksheet.columnShortcutHeld",true);view_.updateSelectionDimensions(extend||columnSpace);return true;
 }
 bool structuralKey(QObject* object,QKeyEvent* key){
  if(object!=&view_&&object!=view_.viewport())return false;
  if(!(key->modifiers()&Qt::ControlModifier)||(key->modifiers()&(Qt::AltModifier|Qt::MetaModifier)))return false;
  const bool insert=key->key()==Qt::Key_Plus||key->key()==Qt::Key_Equal,remove=key->key()==Qt::Key_Minus;if(!insert&&!remove)return false;
  const auto selection=view_.selectionModel()->selection();if(selection.size()!=1)return false;const auto range=selection.front();const bool rows=range.width()==model_.columnCount(),columns=range.height()==model_.rowCount();if(!rows&&!columns)return false;
  const auto current=view_.currentIndex();const int row=current.row(),column=current.column(),horizontal=view_.horizontalScrollBar()->value(),vertical=view_.verticalScrollBar()->value();
  const auto command=rows?(insert?"rows.insertAbove":"rows.delete"):(insert?"columns.insertLeft":"columns.delete");if(window_.executeCommand(command)){const auto index=model_.index(row,column);view_.selectionModel()->setCurrentIndex(index,QItemSelectionModel::NoUpdate);view_.selectionModel()->select(QItemSelection(model_.index(range.top(),range.left()),model_.index(range.bottom(),range.right())),QItemSelectionModel::ClearAndSelect);view_.horizontalScrollBar()->setValue(horizontal);view_.verticalScrollBar()->setValue(vertical);view_.setFocus(Qt::ShortcutFocusReason);}return true;
 }
 bool eventFilter(QObject* object,QEvent* event) override {
  if(object==&view_&&event->type()==QEvent::DynamicPropertyChange){syncEntryChrome();return false;}
  if(event->type()==QEvent::WindowDeactivate&&object==&window_){if(!window_.property("ribbon.peek").toBool())reset(false);return false;}
  if(event->type()==QEvent::MouseButtonPress&&mode_!=Mode::Idle){auto* widget=qobject_cast<QWidget*>(object);if(widget&&ribbon_.currentWidget()->isAncestorOf(widget)){mode_=Mode::Idle;homePrefix_.clear();clearBadges();returnFocus_.clear();}else reset(false);return false;}
  if(!owned(object)||QApplication::activeModalWidget())return false;
  if(event->type()==QEvent::KeyRelease){auto* key=static_cast<QKeyEvent*>(event);if(key->key()==Qt::Key_Alt&&altDown_){altDown_=false;return true;}return false;}
  if(QApplication::activePopupWidget()&&QApplication::activePopupWidget()->objectName()!="ribbon.peek")return false;
  if(event->type()!=QEvent::KeyPress&&event->type()!=QEvent::ShortcutOverride)return false;
  auto* key=static_cast<QKeyEvent*>(event);if(dimensionKey(object,key,event->type()==QEvent::ShortcutOverride))return true;if(key->modifiers()&(Qt::ControlModifier|Qt::MetaModifier)){if(mode_!=Mode::Idle)reset(false);if(event->type()==QEvent::ShortcutOverride){if((object==&view_||object==view_.viewport())&&(key->key()==Qt::Key_Plus||key->key()==Qt::Key_Equal||key->key()==Qt::Key_Minus)){key->accept();return true;}return false;}return structuralKey(object,key);}
  if(mode_!=Mode::Idle&&(key->modifiers()&Qt::AltModifier)&&!keyTipKey(key->key())){reset(false);return false;}
  if(key->key()!=Qt::Key_Alt&&mode_==Mode::Idle)return false;
  if(event->type()==QEvent::ShortcutOverride){key->accept();return true;}
  if(key->key()==Qt::Key_Alt){if(key->isAutoRepeat())return true;altDown_=true;if(mode_==Mode::Idle){returnFocus_=QApplication::focusWidget();mode_=Mode::Root;showTips();}else reset();return true;}
  if(key->key()==Qt::Key_Escape){reset();return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_E){mode_=Mode::LegacyEdit;showLegacyAccess("ALT, E");return true;}
  if(mode_==Mode::LegacyEdit&&key->key()==Qt::Key_S){reset();window_.executeCommand("paste.special");return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_O){mode_=Mode::LegacyFormat;showLegacyAccess("ALT, O");return true;}
  if(mode_==Mode::LegacyFormat&&key->key()==Qt::Key_C){mode_=Mode::LegacyColumn;showLegacyAccess("ALT, O, C");return true;}
  if(mode_==Mode::LegacyColumn&&key->key()==Qt::Key_W){reset();window_.executeCommand("columns.size");return true;}
  if(mode_==Mode::LegacyColumn&&key->key()==Qt::Key_A){reset();window_.executeCommand("columns.autofit");return true;}
  if(mode_==Mode::Root&&key->key()>=Qt::Key_1&&key->key()<=Qt::Key_8){if(!formattingReady()){reset();return true;}}
  if(mode_==Mode::Root&&key->key()==Qt::Key_1){reset();showPalette(window_,model_,view_,true,true);return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_2){reset();format({{"numberFormat",accountingNumberFormat(2)}});return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_4){reset();decimals(-1);return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_8){reset(false);if(fontSize_&&selectTab("Home")){if(auto* page=dynamic_cast<RibbonPage*>(ribbon_.currentWidget()))page->ensureWidgetVisible(fontSize_,8,0);fontSize_->setFocus(Qt::ShortcutFocusReason);fontSize_->selectAll();}return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_3){reset();decimals(1);return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_5){reset();centerSelection();return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_6){reset();showPalette(window_,model_,view_,false,true);return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_7){openBorders();return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_H&&selectTab("Home")){mode_=Mode::Home;showTips();return true;}
  if(mode_==Mode::Root&&key->key()==Qt::Key_R&&selectTab("Review")){mode_=Mode::Review;showTips();return true;}
  if(mode_==Mode::Review&&key->key()==Qt::Key_T){auto* notes=qobject_cast<QToolButton*>(command("Notes"));reset();if(notes)notes->click();else window_.executeCommand("note.edit");return true;}
  if(mode_==Mode::Root){QString title;switch(key->key()){case Qt::Key_N:title="Insert";break;case Qt::Key_P:title="Page Layout";break;case Qt::Key_M:title="Formulas";break;case Qt::Key_A:title="Data";break;case Qt::Key_R:title="Review";break;case Qt::Key_L:title="Developer";break;}if(!title.isEmpty()&&selectTab(title)){reset();return true;}}
  if(mode_==Mode::Home&&(key->key()==Qt::Key_0||key->key()==Qt::Key_9)){const int direction=key->key()==Qt::Key_0?1:-1;reset();decimals(direction);return true;}
  if(mode_==Mode::Home&&(key->key()==Qt::Key_6||key->key()==Qt::Key_5)){const int direction=key->key()==Qt::Key_6?1:-1;reset();indent(direction);return true;}
  if(mode_==Mode::Home){homePrefix_+=QChar(key->key());bool prefix=false;for(auto* control:ribbon_.currentWidget()->findChildren<QWidget*>()){const auto tip=control->property("ribbon.keyTip").toString();if(tip==homePrefix_){if(!control->isEnabled()){reset();return true;}if(auto* button=qobject_cast<QAbstractButton*>(control)){
    if(auto* tool=qobject_cast<QToolButton*>(button);tool&&tool->menu()&&(tool->popupMode()==QToolButton::InstantPopup||tool->property("ribbon.keyTipOpensMenu").toBool())){const auto position=popupPosition(tool,view_,QPoint(0,tool->height()));auto* menu=tool->menu();reset();menu->popup(position);}else{reset();button->click();}
   }else{mode_=Mode::Idle;homePrefix_.clear();clearBadges();control->setFocus(Qt::ShortcutFocusReason);if(auto* combo=qobject_cast<QComboBox*>(control);combo&&combo->isEditable())combo->lineEdit()->selectAll();if(auto* spin=qobject_cast<QSpinBox*>(control))spin->selectAll();}return true;}if(tip.startsWith(homePrefix_))prefix=true;}if(prefix){for(const auto& badge:badges_)if(badge&&!badge->text().startsWith(homePrefix_))badge->hide();return true;}}
  if(mode_==Mode::Root&&key->key()==Qt::Key_W&&selectTab("View")){mode_=Mode::View;showTips();return true;}
  if(mode_==Mode::View&&key->key()==Qt::Key_V){mode_=Mode::Show;showTips();return true;}
  if(mode_==Mode::View){const QString typed(QChar(key->key()));for(auto* control:ribbon_.currentWidget()->findChildren<QToolButton*>())if(control->property("ribbon.keyTip").toString()==typed){reset(false);if(control->isEnabled()){if(auto* menu=control->menu())menu->popup(control->mapToGlobal(QPoint(0,control->height())));else control->click();}return true;}}
  if(mode_==Mode::Show&&(key->key()==Qt::Key_G||key->key()==Qt::Key_F||key->key()==Qt::Key_H||key->key()==Qt::Key_R)){auto* control=qobject_cast<QAbstractButton*>(command(key->key()==Qt::Key_G?"Gridlines":key->key()==Qt::Key_F?"Formula Bar":key->key()==Qt::Key_H?"Headings":"Ruler"));reset();if(control&&control->isEnabled())control->click();view_.setFocus(Qt::ShortcutFocusReason);return true;}
  reset();return true;
 }
public:
 TutorialShortcuts(MainWindow& window,WorksheetModel& model,WorksheetView& view,QTabWidget& ribbon):QObject(&window),window_(window),model_(model),view_(view),ribbon_(ribbon){
  setObjectName("tutorial.shortcuts");
  fontColor_=qobject_cast<QToolButton*>(command("A"));fill_=qobject_cast<QToolButton*>(command("Fill"));center_=qobject_cast<QToolButton*>(command("Center"));fontSize_=window_.findChild<QSpinBox*>("ribbon.fontSize");
  if(fontColor_){fontColor_->setObjectName("tutorial.home.fontColor");fontColor_->setToolTip("Font Color (Alt+1)");fontColor_->setIcon(fontColorIcon(QColor("#ff0000")));disconnect(fontColor_,&QToolButton::clicked,&window_,nullptr);connect(fontColor_,&QToolButton::clicked,this,[this]{format({{"color",window_.property("tutorial.currentFontColor").isValid()?window_.property("tutorial.currentFontColor").toString().toStdString():std::string("#ff0000")}});});}
  fontMenu_=homeControl(fontColor_,"fontColor.menu","Font Color palette","▾");if(fontMenu_){fontMenu_->setIcon(QIcon());fontMenu_->setArrowType(Qt::DownArrow);connect(fontMenu_,&QToolButton::clicked,this,[this]{window_.executeCommand("style.color");});}
  if(fill_)fill_->setObjectName("tutorial.home.fill");if(center_)center_->setObjectName("tutorial.home.center");
  auto* formats=window_.findChild<QWidget*>("ribbon.numberFormat");accounting_=homeControl(formats,"accounting","Accounting Number Format","$");if(accounting_){accounting_->setIcon(spreadsheetCommandIcon("$"));accounting_->setPopupMode(QToolButton::MenuButtonPopup);auto* menu=new QMenu(&window_);menu->setObjectName("tutorial.accountingMenu");auto* dollar=menu->addAction("$ Dollar");auto* none=menu->addAction("None");menu->addSeparator();auto* more=menu->addAction("More Number Formats...");connect(dollar,&QAction::triggered,this,[this]{format({{"numberFormat",accountingNumberFormat(2)}});});connect(none,&QAction::triggered,this,[this]{format({{"numberFormat",accountingNumberFormat(2,"")}});});connect(more,&QAction::triggered,this,[this]{window_.executeCommand("cells.format",{{"initialTab",0}});});accounting_->setMenu(menu);connect(accounting_,&QToolButton::clicked,this,[this]{format({{"numberFormat",accountingNumberFormat(2)}});window_.executeCommand("view.ribbon.close");});}
  borders_=homeControl(fontColor_,"borders","Borders","Borders");if(borders_)borders_->setIcon(borderIcon("top",true));borderMenu_=createWorksheetBorderMenu(&window_,[this](const Json& style){format(style);},[this]{window_.executeCommand("cells.format",{{"initialTab",3}});});
  increaseDecimals_=homeControl(formats,"increaseDecimals","Increase Decimal",".0 ← .00");decreaseDecimals_=homeControl(formats,"decreaseDecimals","Decrease Decimal",".00 → .0");if(increaseDecimals_)connect(increaseDecimals_,&QToolButton::clicked,this,[this]{decimals(1);});if(decreaseDecimals_)connect(decreaseDecimals_,&QToolButton::clicked,this,[this]{decimals(-1);});increaseIndent_=homeControl(command("Left"),"increaseIndent","Increase Indent","→");decreaseIndent_=homeControl(command("Left"),"decreaseIndent","Decrease Indent","←");if(increaseIndent_)connect(increaseIndent_,&QToolButton::clicked,this,[this]{indent(1);});if(decreaseIndent_)connect(decreaseIndent_,&QToolButton::clicked,this,[this]{indent(-1);});
  if(borders_)connect(borders_,&QToolButton::clicked,this,[this]{openBorders(false);});connect(borderMenu_,&QMenu::aboutToHide,this,[this]{view_.setFocus(Qt::ShortcutFocusReason);});
  connect(view_.selectionModel(),&QItemSelectionModel::currentChanged,this,[this]{syncEntryChrome();});connect(&model_,&QAbstractItemModel::dataChanged,this,[this]{syncEntryChrome();});connect(&model_,&QAbstractItemModel::modelReset,this,[this]{syncEntryChrome();});syncEntryChrome();qApp->installEventFilter(this);
 }
 ~TutorialShortcuts() override {qApp->removeEventFilter(this);clearBadges();}
};
}
QObject* installTutorialShortcuts(MainWindow& window,WorksheetModel& model,WorksheetView& view,QTabWidget& ribbon){if(auto* existing=window.findChild<QObject*>("tutorial.shortcuts"))return existing;return new TutorialShortcuts(window,model,view,ribbon);}
void showWorksheetFontPalette(MainWindow& window,WorksheetModel& model,WorksheetView& view){showPalette(window,model,view,true,false);}
void showWorksheetFillPalette(MainWindow& window,WorksheetModel& model,WorksheetView& view){showPalette(window,model,view,false,false);}
}
