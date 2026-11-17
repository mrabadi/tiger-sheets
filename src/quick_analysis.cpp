#include "tigersheets/quick_analysis.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QAction>
#include <QDynamicPropertyChangeEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QScrollBar>
#include <QScreen>
#include <QTabWidget>
#include <QTabBar>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
namespace tigersheets {
namespace {
QIcon analysisIcon(const QString& kind){
 QPixmap pixmap(32,32);pixmap.fill(Qt::transparent);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#555555"),1));p.setBrush(Qt::white);p.drawRect(3,4,26,22);for(int x:{11,20})p.drawLine(x,4,x,26);for(int y:{11,18})p.drawLine(3,y,29,y);
 if(kind=="colorScale")for(int row=0;row<3;++row){p.fillRect(4,5+row*7,24,6,QColor(row==0?"#63be7b":row==1?"#ffeb84":"#f8696b"));}
 else if(kind=="iconSet"){for(int row=0;row<3;++row){p.setBrush(QColor(row==0?"#67a54b":row==1?"#e6b04d":"#c95245"));p.drawEllipse(QRect(5,6+row*7,4,4));}}
 else if(kind=="clear"){p.setBrush(QColor("#e3b0df"));p.drawPolygon(QPolygonF{{16,25},{23,16},{30,22},{23,31}});}
 else if(kind=="topTenPercent"){p.fillRect(5,6,5,18,QColor("#ffc54a"));p.setFont(QFont("Calibri",10));p.drawText(QRect(17,15,16,17),Qt::AlignCenter,"%");}
 else if(kind=="greaterThan"){p.fillRect(4,5,24,6,QColor("#f5d77d"));p.fillRect(4,12,7,5,QColor("#6db465"));p.setFont(QFont("Calibri",12));p.drawText(QRect(16,13,18,20),Qt::AlignCenter,">");}
 else {for(int row=0;row<3;++row)p.fillRect(4,5+row*7,5+row*3,6,QColor("#6cb4d8"));}
 return QIcon(pixmap);
}
QIcon rangeIcon(){
 QPixmap pixmap(24,24);pixmap.fill(Qt::transparent);QPainter p(&pixmap);p.setPen(Qt::black);p.setBrush(Qt::white);p.drawRect(4,5,15,14);p.drawLine(4,7,19,7);p.drawRect(8,12,8,3);p.setPen(QPen(Qt::black,2));p.drawLine(4,19,19,19);
 p.setPen(QColor("#bf845f"));p.setBrush(QColor("#e4ab8d"));p.drawPolygon(QPolygonF{{3,1},{9,1},{6,6},{10,6},{2,13},{4,7},{1,7}});return QIcon(pixmap);
}
class QuickAnalysis final:public QObject {
 MainWindow& window_;WorksheetModel& model_;QPointer<WorksheetView> view_;QPointer<QToolButton> button_;QPointer<QFrame> popup_;QTimer keyboardSettle_;QPersistentModelIndex completedCorner_;
 void positionCompletedButton(){
  if(!completedCorner_.isValid()){button_->hide();return;}
  const auto rect=view_->visualRect(completedCorner_);const auto position=rect.bottomRight()+QPoint(5,5);button_->move(position);button_->setVisible(view_->viewport()->rect().intersects(rect)&&view_->viewport()->rect().contains(QRect(position,button_->size())));button_->raise();
 }
 void refresh(bool settled=false){
  if(!view_||!button_)return;if(view_->activeCellEditor()||view_->property("worksheet.externalPointing").toBool()){keyboardSettle_.stop();button_->hide();return;}
  const bool pointerHeld=(QApplication::mouseButtons()&Qt::LeftButton)||view_->property("worksheet.dragAnchor").value<QModelIndex>().isValid();
  if(view_->property("worksheet.selectionExtensionActive").toBool()){
   // A held extension keeps the last completed range's eligibility and cell
   // anchor, but its glyph still follows that cell when the viewport moves.
   positionCompletedButton();
   if(pointerHeld){keyboardSettle_.stop();return;}
   // Let a stationary keyboard range settle without waiting for Shift release.
   if(!settled){keyboardSettle_.start();return;}
  }else keyboardSettle_.stop();
  const auto selection=view_->selectionModel()->selection();if(selection.size()!=1||selection.front().width()*static_cast<qint64>(selection.front().height())<2||selection.front().width()*static_cast<qint64>(selection.front().height())>100000){completedCorner_=QPersistentModelIndex();button_->hide();return;}
  const auto range=selection.front();int populated=0;for(int row=range.top();row<=range.bottom()&&populated<2;++row)for(int col=range.left();col<=range.right()&&populated<2;++col)if(!model_.raw(row,col).isEmpty())++populated;
  completedCorner_=populated>=2?QPersistentModelIndex(range.bottomRight()):QPersistentModelIndex();positionCompletedButton();
 }
 void open(){
  delete popup_.data();auto* popup=new QFrame(&window_,Qt::Popup);popup_=popup;popup->setObjectName("quickAnalysis.popup");popup->setAccessibleName("Quick Analysis");popup->setFrameShape(QFrame::StyledPanel);popup->setFixedSize(418,164);popup->setStyleSheet("QFrame#quickAnalysis\\.popup { background: white; border: 1px solid #dddddd; } QTabWidget::pane { border: 0; } QTabBar::tab { background: white; border: 0; padding: 4px 5px 7px; font: 11px Calibri; } QTabBar::tab:selected { border-bottom: 3px solid #217346; } QToolButton { background: white; border: 0; font: 11px Calibri; padding: 0; } QToolButton:hover { background: #eeeeee; } QLabel { border: 0; font: 11px Calibri; }");auto* layout=new QVBoxLayout(popup);layout->setContentsMargins(11,9,11,8);layout->setSpacing(4);auto* tabs=new QTabWidget(popup);tabs->setObjectName("quickAnalysis.tabs");tabs->setDocumentMode(true);layout->addWidget(tabs);const QStringList titles={"Formatting","Charts","Totals","Tables","Sparklines"};
  for(const auto& title:titles){auto* page=new QWidget(tabs);tabs->addTab(page,title);if(title!="Formatting")continue;auto* row=new QHBoxLayout(page);row->setContentsMargins(0,9,0,0);row->setSpacing(3);const QStringList keys={"dataBars","colorScale","iconSet","greaterThan","topTenPercent","clear"},labels={"Data Bars","Color…","Icon Set","Greater…","Top 10%","Clear…"};for(int index=0;index<keys.size();++index){auto* tile=new QToolButton(page);tile->setObjectName("quickAnalysis."+keys[index]);tile->setAccessibleName(labels[index]);tile->setText(labels[index]);tile->setIcon(analysisIcon(keys[index]));tile->setIconSize(QSize(32,32));tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);tile->setFixedSize(58,61);row->addWidget(tile);}}
  auto* description=new QLabel("Conditional Formatting uses rules to highlight interesting data.",popup);description->setObjectName("quickAnalysis.description");layout->addWidget(description);auto position=button_->mapToGlobal(QPoint(button_->width()/2,button_->height()+10))-QPoint(popup->width()/2,0);const auto screen=window_.screen()->availableGeometry();position.setX(qBound(screen.left(),position.x(),screen.right()-popup->width()+1));position.setY(qBound(screen.top(),position.y(),screen.bottom()-popup->height()+1));popup->move(position);popup->show();tabs->tabBar()->setFocus();
 }
 bool eventFilter(QObject* object,QEvent* event)override{
  if(!view_)return false;if(event->type()==QEvent::MouseButtonPress&&popup_&&popup_->isVisible()){auto* widget=qobject_cast<QWidget*>(object);if(widget&&widget->window()!=popup_&&widget!=button_)popup_->hide();}
  if(object==&model_&&event->type()==QEvent::DynamicPropertyChange&&static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName()=="worksheet.contextGeneration")view_->finishSelectionExtension();
  if(event->type()==QEvent::KeyRelease&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Shift&&view_->property("worksheet.selectionExtensionActive").toBool())view_->finishSelectionExtension();
  if(object==view_&&event->type()==QEvent::DynamicPropertyChange&&static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName()=="worksheet.selectionExtensionActive")refresh();
  if(object==view_->viewport()&&event->type()==QEvent::Resize)refresh();return false;
 }
public:
 QuickAnalysis(MainWindow& window,WorksheetModel& model,WorksheetView& view):QObject(&window),window_(window),model_(model),view_(&view),keyboardSettle_(this){setObjectName("worksheet.quickAnalysisController");keyboardSettle_.setSingleShot(true);keyboardSettle_.setInterval(250);connect(&keyboardSettle_,&QTimer::timeout,this,[this]{refresh(true);});auto* button=new QToolButton(view.viewport());button_=button;button->setObjectName("worksheet.quickAnalysis");button->setAccessibleName("Quick Analysis");button->setToolTip("Quick Analysis (Ctrl+Q)");button->setIcon(rangeIcon());button->setIconSize(QSize(24,24));button->setFixedSize(24,24);button->setStyleSheet("QToolButton { padding: 0px; border: 1px solid #555555; border-radius: 0px; background: white; } QToolButton:hover { background: #eeeeee; }");button->setAutoRaise(true);button->hide();connect(button,&QToolButton::clicked,this,[this]{open();});connect(view.selectionModel(),&QItemSelectionModel::selectionChanged,this,[this]{refresh();});connect(view.selectionModel(),&QItemSelectionModel::currentChanged,this,[this]{refresh();});connect(&model,&QAbstractItemModel::dataChanged,this,[this]{refresh();});connect(&model,&QAbstractItemModel::modelReset,this,[this]{if(view_)view_->finishSelectionExtension();refresh();});connect(view.horizontalScrollBar(),&QScrollBar::valueChanged,this,[this]{refresh();});connect(view.verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{refresh();});auto* shortcut=new QAction(this);shortcut->setShortcut(QKeySequence("Ctrl+Q"));shortcut->setShortcutContext(Qt::WidgetWithChildrenShortcut);view.addAction(shortcut);connect(shortcut,&QAction::triggered,this,[this]{if(button_&&button_->isVisible()&&!view_->activeCellEditor())open();});qApp->installEventFilter(this);refresh();}
 ~QuickAnalysis()override{qApp->removeEventFilter(this);delete popup_.data();delete button_.data();}
};
}
QObject* installQuickAnalysis(MainWindow& window,WorksheetModel& model,WorksheetView& view){return new QuickAnalysis(window,model,view);}
}
