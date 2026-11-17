#include "tigersheets/review_ribbon.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/ribbon_page.hpp"
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
namespace tigersheets {
namespace {
QIcon reviewIcon(const QString& kind){
 QPixmap image(36,36);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#505050"),1));p.setBrush(QColor("#fafafa"));
 auto paper=[&]{p.drawRect(7,4,21,28);p.setPen(QPen(QColor("#808080"),1));for(int row=0;row<3;++row)p.drawLine(11,11+row*5,24,11+row*5);};
 auto table=[&]{p.drawRect(3,5,29,25);for(int c=1;c<4;++c)p.drawLine(3+c*7,5,3+c*7,30);for(int r=1;r<4;++r)p.drawLine(3,5+r*6,32,5+r*6);};
 auto bubble=[&]{p.drawPolygon(QPolygonF{{3,6},{32,6},{32,25},{16,25},{10,32},{10,25},{3,25}});};
 if(kind=="spelling"){p.setFont(QFont("Calibri",12));p.drawText(QRect(0,0,36,22),Qt::AlignCenter,"abc");p.setPen(QPen(QColor("#4a9a54"),2.5));p.drawPolyline(QPolygonF{{10,25},{16,30},{27,18}});}
 else if(kind=="thesaurus"){p.drawPolygon(QPolygonF{{3,5},{16,5},{18,7},{20,5},{33,5},{33,29},{20,29},{18,32},{16,29},{3,29}});p.drawLine(18,7,18,31);p.setPen(QPen(QColor("#a85d61"),1));for(int y=10;y<26;y+=5){p.drawLine(7,y,14,y);p.drawLine(22,y,29,y);}}
 else if(kind=="statistics"){table();p.fillRect(5,7,25,4,QColor("#a5c5d7"));p.setPen(QPen(QColor("#b07c38"),1.5));p.drawEllipse(QRectF(19,20,11,11));p.drawLine(28,29,34,35);}
 else if(kind=="accessibility"){paper();p.setPen(QPen(QColor("#3e8bb1"),2));p.drawEllipse(QRectF(19,19,5,5));p.drawLine(22,24,22,34);p.drawLine(16,27,28,27);p.drawLine(22,30,17,35);p.drawLine(22,30,27,35);}
 else if(kind=="lookup"){p.setPen(QPen(QColor("#2996be"),1.5));p.drawEllipse(QRectF(11,3,23,23));p.setFont(QFont("Calibri",17));p.drawText(QRect(12,1,21,25),Qt::AlignCenter,"i");p.setPen(QPen(QColor("#606060"),2));p.drawLine(13,25,4,34);}
 else if(kind=="translate"){p.setPen(QColor("#257da5"));p.setFont(QFont("Calibri",17));p.drawText(QRect(0,0,24,31),Qt::AlignCenter,"a");p.setPen(QColor("#238545"));p.setFont(QFont("sans-serif",13));p.drawText(QRect(14,0,22,31),Qt::AlignCenter,QString::fromUtf8("文"));p.drawLine(7,3,20,3);p.drawLine(20,3,17,0);p.drawLine(17,32,30,32);p.drawLine(17,32,20,35);}
 else if(kind=="newComment"||kind=="deleteComment"||kind=="previousComment"||kind=="nextComment"||kind=="showComments"){bubble();if(kind=="newComment"){p.setPen(QPen(QColor("#489367"),1.5));p.drawLine(2,8,17,8);p.drawLine(9,1,9,16);}else if(kind=="deleteComment"){p.setPen(QPen(QColor("#a5a5a5"),1.5));p.drawLine(3,2,14,13);p.drawLine(14,2,3,13);}else if(kind=="previousComment"||kind=="nextComment"){p.setPen(QPen(QColor("#999999"),1.5));const bool next=kind=="nextComment";p.drawLine(3,8,18,8);p.drawLine(next?18:3,8,next?12:9,2);p.drawLine(next?18:3,8,next?12:9,14);}}
 else if(kind=="notes"){p.drawPolygon(QPolygonF{{4,4},{30,4},{30,25},{23,32},{4,32}});p.drawLine(23,25,30,25);p.drawLine(23,25,23,32);}
 else if(kind=="protectSheet"||kind=="protectWorkbook"||kind=="allowEditRanges"||kind=="unshareWorkbook"){table();if(kind=="allowEditRanges"){p.setPen(QPen(QColor("#338abd"),4));p.drawLine(17,30,33,13);p.setPen(QPen(QColor("#b29253"),2));p.drawLine(14,33,17,30);}else if(kind=="unshareWorkbook"){p.setPen(QColor("#b0b0b0"));p.drawEllipse(QRectF(17,21,12,12));}else{if(kind=="protectWorkbook"){p.drawRect(7,3,22,29);p.drawLine(11,3,11,32);}p.setPen(QPen(QColor("#b68330"),1));p.setBrush(QColor("#f2d3a1"));p.drawRoundedRect(QRectF(20,23,13,11),1,1);p.setBrush(Qt::NoBrush);p.drawRoundedRect(QRectF(23,17,7,10),3,3);}}
 else if(kind=="hideInk"){p.setPen(QPen(QColor("#bd813e"),1.5));p.drawPolyline(QPolygonF{{3,12},{14,6},{10,18},{27,10},{19,27},{32,22}});p.setPen(QPen(QColor("#c8424a"),2));p.drawLine(3,33,32,3);}
 return QIcon(image);
}
class CommentsOnboarding final:public QWidget {
public:
 int tip=146;
 explicit CommentsOnboarding(QWidget* parent):QWidget(parent){setObjectName("review.commentsOnboarding");setAccessibleName("Comments have changed");setFixedSize(284,254);setAttribute(Qt::WA_TranslucentBackground);setFocusPolicy(Qt::NoFocus);auto* body=new QFrame(this);body->setObjectName("review.commentsOnboarding.body");body->setGeometry(0,8,284,246);body->setStyleSheet("QFrame#review\\.commentsOnboarding\\.body { background:#217346; border:1px solid #bdd1c3; } QLabel { color:white; background:transparent; border:0; font:12px Calibri; }");auto* title=new QLabel("Comments have changed",body);title->setObjectName("review.commentsOnboarding.title");title->setGeometry(12,13,260,22);title->setStyleSheet("color:white; font:600 14px Calibri; border:0; background:transparent;");auto* first=new QLabel("Comments can now be used for discussions.\nWhen you put one in, you'll see that it comes\nwith a Reply box, which lets you have threaded\nconversations with other people.",body);first->setObjectName("review.commentsOnboarding.discussions");first->setGeometry(12,50,260,66);first->setAlignment(Qt::AlignLeft|Qt::AlignTop);auto* second=new QLabel("Looking for the old comments? They're now\ncalled Notes. Notes work just like comments\nused to: they're yellow, and you can move or\nresize them. (But you can't reply to a Note.)",body);second->setObjectName("review.commentsOnboarding.notes");second->setGeometry(12,130,260,68);second->setAlignment(Qt::AlignLeft|Qt::AlignTop);auto* got=new QPushButton(body);got->setObjectName("review.commentsOnboarding.gotIt");got->setAccessibleName("Got it");got->setToolTip("Got it");got->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));got->setIconSize(QSize(18,18));got->setGeometry(244,208,28,28);got->setFocusPolicy(Qt::NoFocus);got->setStyleSheet("QPushButton { background:#217346; color:white; border:1px solid #c8ddcf; border-radius:0; padding:0; font:12px Calibri; } QPushButton:hover { background:#2e8553; }");hide();}
protected:
 void paintEvent(QPaintEvent*)override{QPainter p(this);p.setPen(QColor("#bdd1c3"));p.setBrush(QColor("#217346"));p.drawPolygon(QPolygonF{{qreal(tip-9),8},{qreal(tip),0},{qreal(tip+9),8}});}
};
class ReviewRibbon final:public QObject {
 QPointer<MainWindow> window_;QPointer<QTabWidget> ribbon_;QPointer<QTableView> view_;QPointer<RibbonPage> page_;QPointer<QWidget> comments_;QPointer<CommentsOnboarding> banner_;bool seen_=false;
 void place(){
  if(!window_||!comments_||!banner_)return;
  auto* newComment=comments_->findChild<QToolButton*>("review.newComment");if(!newComment)return;
  const int center=window_->mapFromGlobal(newComment->mapToGlobal(newComment->rect().center())).x();
  const int bottom=window_->mapFromGlobal(comments_->mapToGlobal(QPoint(0,comments_->height()))).y();
  const int x=std::clamp(center-146,4,std::max(4,window_->width()-288));
  banner_->tip=std::clamp(center-x,10,274);banner_->move(x,bottom-2);banner_->raise();
 }
 void maybeShow(){if(seen_||!page_||!page_->isVisible()||!ribbon_||ribbon_->currentWidget()!=page_)return;seen_=true;place();banner_->show();banner_->raise();}
 bool eventFilter(QObject* object,QEvent* event)override{if(!banner_)return false;if(object==page_){if(event->type()==QEvent::Show)QTimer::singleShot(0,this,[this]{maybeShow();});else if(event->type()==QEvent::Hide)banner_->hide();}if((object==window_||(page_&&object==page_->window()))&&(event->type()==QEvent::Resize||event->type()==QEvent::Move))place();if(event->type()==QEvent::MouseButtonPress&&object==banner_->findChild<QPushButton*>("review.commentsOnboarding.gotIt")){banner_->hide();return false;}return false;}
public:
 ReviewRibbon(MainWindow& window,QTabWidget& ribbon,QTableView& view):QObject(&window),window_(&window),ribbon_(&ribbon),view_(&view){setObjectName("review.controller");for(int index=0;index<ribbon.count();++index)if(ribbon.tabText(index)=="Review")page_=dynamic_cast<RibbonPage*>(ribbon.widget(index));if(!page_)return;page_->setProperty("ribbon.pageHeight",132);page_->setProperty("ribbon.contentHeight",100);page_->setProperty("ribbon.popupTopMargin",-2);auto* row=page_->contentLayout();while(auto* item=row->takeAt(0)){delete item->widget();delete item;}row->setContentsMargins(0,0,0,0);row->setSpacing(0);page_->widget()->setStyleSheet("background:#f3f2f1;");
  auto group=[&](const char* id,const char* caption,int width){auto* panel=new QFrame;panel->setObjectName("review.group."+QString::fromLatin1(id));panel->setFixedSize(width,100);panel->setStyleSheet("QFrame { border:0; border-right:1px solid #b8b8b8; }");auto* col=new QVBoxLayout(panel);col->setContentsMargins(4,0,4,4);col->setSpacing(0);auto* controls=new QHBoxLayout;controls->setContentsMargins(0,0,0,0);controls->setSpacing(0);controls->setAlignment(Qt::AlignTop);col->addLayout(controls,1);auto* label=new QLabel(caption,panel);label->setFixedHeight(14);label->setAlignment(Qt::AlignHCenter);label->setStyleSheet("font:12px Calibri; color:#404040; border:0;");col->addWidget(label);row->addWidget(panel);if(QString::fromLatin1(id)=="comments")comments_=panel;return controls;};
  auto control=[&](QHBoxLayout* group,const char* id,const char* text,const char* label,const char* tip,int width,bool enabled=true){auto* button=new QToolButton;button->setObjectName("review."+QString::fromLatin1(id));button->setText(text);button->setAccessibleName(label);button->setToolTip(label);button->setIcon(reviewIcon(QString::fromLatin1(id)));button->setIconSize(QSize(36,36));button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);button->setAutoRaise(true);button->setFixedSize(width,72);button->setEnabled(enabled);button->setProperty("ribbon.keyTip",QString::fromLatin1(tip));button->setProperty("ribbon.sourceKeyTip",true);button->setStyleSheet("QToolButton { font:12px Calibri; color:#303030; border:0; background:transparent; padding:0; } QToolButton:disabled { color:#aaaaaa; } QToolButton:hover { background:#e1e1e1; }");group->addWidget(button);if(QString::fromLatin1(id)=="notes")connect(button,&QToolButton::clicked,this,[this]{if(window_)window_->executeCommand("note.edit");});return button;};
  auto* proof=group("proofing","Proofing",184);control(proof,"spelling","Spelling","Spelling","S",50);control(proof,"thesaurus","Thesaurus","Thesaurus","E",58);control(proof,"statistics","Workbook\nStatistics","Workbook Statistics","B",68);
  auto* accessibility=group("accessibility","Accessibility",96);control(accessibility,"accessibility","Check\nAccessibility","Check Accessibility","A",88);
  auto* lookup=group("insights","Insights",64);control(lookup,"lookup","Smart\nLookup","Smart Lookup","RS",56);
  auto* language=group("language","Language",64);control(language,"translate","Translate","Translate","L",56);
  auto* comments=group("comments","Comments",312);control(comments,"newComment","New\nComment","New Comment","C",58);control(comments,"deleteComment","Delete","Delete","D",48,false);control(comments,"previousComment","Previous\nComment","Previous Comment","V",57,false);control(comments,"nextComment","Next\nComment","Next Comment","N",57,false);control(comments,"showComments","Show\nComments","Show Comments","H",84);
  auto* notes=group("notes","Notes",50);control(notes,"notes","Notes","Notes","T",42);
  auto* protect=group("protect","Protect",246);control(protect,"protectSheet","Protect\nSheet","Protect Sheet","PS",53);control(protect,"protectWorkbook","Protect\nWorkbook","Protect Workbook","PW",64);control(protect,"allowEditRanges","Allow Edit\nRanges","Allow Edit Ranges","U1",65);control(protect,"unshareWorkbook","Unshare\nWorkbook","Unshare Workbook","U2",56,false);
  auto* ink=group("ink","Ink",62);control(ink,"hideInk","Hide\nInk","Hide Ink","H2",54);row->addStretch();
  banner_=new CommentsOnboarding(&window);connect(banner_->findChild<QPushButton*>("review.commentsOnboarding.gotIt"),&QPushButton::clicked,this,[this]{if(banner_)banner_->hide();if(view_)view_->setFocus(Qt::OtherFocusReason);});page_->installEventFilter(this);window.installEventFilter(this);qApp->installEventFilter(this);connect(&ribbon,&QTabWidget::currentChanged,this,[this]{if(ribbon_&&ribbon_->currentWidget()!=page_&&banner_)banner_->hide();else QTimer::singleShot(0,this,[this]{maybeShow();});});
 }
 ~ReviewRibbon()override{qApp->removeEventFilter(this);delete banner_.data();}
};
}
QObject* installReviewRibbon(MainWindow& window,QTabWidget& ribbon,QTableView& view){if(auto* existing=window.findChild<QObject*>("review.controller"))return existing;return new ReviewRibbon(window,ribbon,view);}
}
