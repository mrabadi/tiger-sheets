#include "native_document_context.hpp"
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QTextCursor>
#include <QToolButton>
#include <QToolTip>
#include <QHelpEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QUrl>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QKeyEvent>
#include <QPointer>
#include <QTextDocumentFragment>
#include <map>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace tigersheets::testing {
namespace {
QByteArray run(const QString& tool,const QStringList& args){QProcess process;process.start(tool,args);if(!process.waitForStarted(5000)||!process.waitForFinished(30000)||process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)throw std::runtime_error("Read-only PDF adapter failed: "+tool.toStdString()+" "+process.readAllStandardError().toStdString());return process.readAllStandardOutput();}
class PdfEditTool final:public QToolButton {
 QTemporaryDir artwork_;
public:
 explicit PdfEditTool(QWidget* parent):QToolButton(parent){
  setObjectName("context.editPdf");setAccessibleName("Edit PDF");setAccessibleDescription("Read-only reference adapter; PDF editing is unavailable.");setFocusPolicy(Qt::NoFocus);setAutoRaise(true);setToolTip("Edit PDF");setToolTipDuration(5000);
  QPixmap artwork(24,24);artwork.fill(Qt::transparent);QPainter painter(&artwork);painter.setPen(QPen(QColor("#d247a3"),1.6));painter.drawRect(QRectF(2,2,20,6));painter.drawRect(QRectF(2,12,7,9));painter.drawLine(13,13,22,13);painter.drawLine(13,17,22,17);painter.drawLine(13,21,22,21);painter.end();setIcon(QIcon(artwork));setIconSize(QSize(24,24));
  const auto image=artwork_.filePath("edit-pdf.png");if(!artwork_.isValid()||!artwork.save(image))throw std::runtime_error("Cannot create native PDF tool icon");
  setToolTip(QString("<table cellspacing='0' cellpadding='0'><tr><td width='65'>Edit PDF</td><td><img src='%1' width='24' height='24'></td></tr></table>").arg(QUrl::fromLocalFile(image).toString().toHtmlEscaped()));
  setStyleSheet("QToolButton{background:#f5f5f5;border:0;} QToolButton:hover{background:#e5e5e5;} QToolTip{background:#666666;color:white;border:0;border-radius:3px;padding:3px 7px;font:12px Arial;}");
 }
protected:
 bool event(QEvent* event) override {
  if(event->type()==QEvent::ToolTip){
   const auto position=mapToGlobal(QPoint(-81,0));QToolTip::showText(position,toolTip(),this,rect(),5000);
   // This tool expands its label to the left of the rail. The default tooltip
   // placement adds a cursor offset and flips twice at the right screen edge.
   for(auto* tip:QApplication::topLevelWidgets())if(tip->isVisible()&&tip->windowType()==Qt::ToolTip)tip->move(position-(tip->mapToGlobal(QPoint())-tip->pos()));
   event->accept();return true;
  }
  if(event->type()==QEvent::Leave)QToolTip::hideText();
  return QToolButton::event(event);
 }
};
class DocumentSuggestionDelegate final:public QStyledItemDelegate {
public:
 using QStyledItemDelegate::QStyledItemDelegate;
 void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
  painter->save();painter->fillRect(option.rect,option.state&QStyle::State_Selected?QColor("#dceafa"):QColor(Qt::white));auto font=QFont("Arial");const auto term=index.data(Qt::UserRole).toString();font.setPixelSize(term.isEmpty()?10:12);font.setBold(index.data(Qt::UserRole+2).toBool());painter->setFont(font);painter->setPen(term.isEmpty()?QColor("#858585"):QColor("#3e3e3e"));painter->drawText(option.rect.adjusted(2,0,-2,0),Qt::AlignLeft|Qt::AlignVCenter,term.isEmpty()?index.data().toString():term);
  if(!term.isEmpty()){font.setBold(false);font.setPixelSize(11);painter->setFont(font);painter->drawText(option.rect.adjusted(2,0,-2,0),Qt::AlignRight|Qt::AlignVCenter,QString("(%1)").arg(index.data(Qt::UserRole+1).toInt()));}painter->restore();
 }
};
}
NativeDocumentContext::NativeDocumentContext(QWidget* parent):QWidget(parent,Qt::Window){
 setObjectName("replay.contextDocument");setWindowTitle("Read-only PDF — external context replay adapter");auto* layout=new QVBoxLayout(this);controls_=new QWidget(this);auto* controls=new QHBoxLayout(controls_);controls->setContentsMargins(0,0,0,0);
 page_=new QSpinBox(this);page_->setObjectName("context.page");page_->setPrefix("Page ");page_->setRange(1,1);page_->setKeyboardTracking(false);
 zoom_=new QDoubleSpinBox(this);zoom_->setObjectName("context.zoom");zoom_->setRange(10,400);zoom_->setDecimals(1);zoom_->setSuffix("%");zoom_->setValue(100);zoom_->setKeyboardTracking(false);
 find_=new QLineEdit(this);find_->setObjectName("context.find");find_->setPlaceholderText("Find in current document page");
 controls->addWidget(page_);controls->addWidget(zoom_);controls->addWidget(find_);layout->addWidget(controls_);
 status_=new QLabel(this);status_->setObjectName("context.status");layout->addWidget(status_);
 surface_=new QLabel(this);surface_->setObjectName("context.surface");surface_->setAlignment(Qt::AlignTop|Qt::AlignLeft);scroll_=new QScrollArea(this);scroll_->setObjectName("context.scroll");scroll_->setWidget(surface_);layout->addWidget(scroll_,5);
 text_=new QPlainTextEdit(this);text_->setObjectName("context.text");text_->setReadOnly(true);text_->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);text_->setMaximumHeight(120);layout->addWidget(text_,1);
 editPdf_=new PdfEditTool(this);editPdf_->setGeometry(width()-40,270,40,46);editPdf_->raise();
 suggestions_=new QListWidget(this);suggestions_->setObjectName("context.findSuggestions");suggestions_->setAccessibleName("Find suggestions");suggestions_->setFocusPolicy(Qt::NoFocus);suggestions_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);suggestions_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);suggestions_->setItemDelegate(new DocumentSuggestionDelegate(suggestions_));suggestions_->setStyleSheet("QListWidget{background:white;border:1px solid #b7b7b7;padding:3px;}");suggestions_->hide();qApp->installEventFilter(this);
 connect(find_,&QLineEdit::textEdited,this,[this]{updateFindSuggestions();});
 connect(suggestions_,&QListWidget::itemClicked,this,[this](QListWidgetItem* item){const auto term=item->data(Qt::UserRole).toString();if(term.isEmpty())return;find_->setText(term);suggestions_->hide();findNext();});
 connect(page_,&QSpinBox::valueChanged,this,[this]{if(!path_.isEmpty())render();});connect(zoom_,&QDoubleSpinBox::valueChanged,this,[this]{if(!path_.isEmpty())render();});
 connect(find_,&QLineEdit::returnPressed,this,[this]{findNext();});
}
void NativeDocumentContext::setPresentationView(bool enabled){
 if(enabled==presentationView_||path_.isEmpty())return;
 const auto cursor=text_->textCursor();presentationView_=enabled;
 if(enabled){
  readerZoom_=zoom_->value();readerMargins_=layout()->contentsMargins();readerSpacing_=layout()->spacing();readerFrame_=scroll_->frameStyle();
  const auto pixels=surface_->pixmap().size();const double scale=std::min(width()/(pixels.width()*100.0/readerZoom_),height()/(pixels.height()*100.0/readerZoom_));
  controls_->hide();status_->hide();text_->hide();editPdf_->hide();suggestions_->hide();layout()->setContentsMargins(0,0,0,0);layout()->setSpacing(0);scroll_->setFrameShape(QFrame::NoFrame);scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);zoom_->blockSignals(true);zoom_->setValue(100*scale);zoom_->blockSignals(false);
 }else{
  controls_->show();status_->show();text_->show();editPdf_->show();layout()->setContentsMargins(readerMargins_);layout()->setSpacing(readerSpacing_);scroll_->setFrameStyle(readerFrame_);scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);zoom_->blockSignals(true);zoom_->setValue(readerZoom_);zoom_->blockSignals(false);
 }
 layout()->activate();render();text_->setTextCursor(cursor);scroll_->horizontalScrollBar()->setValue(0);scroll_->verticalScrollBar()->setValue(0);surface_->setFocusPolicy(Qt::StrongFocus);surface_->setFocus();
}
void NativeDocumentContext::resizeEvent(QResizeEvent* event){QWidget::resizeEvent(event);editPdf_->setGeometry(width()-40,270,40,46);editPdf_->raise();}
bool NativeDocumentContext::eventFilter(QObject* watched,QEvent* event){
 if(event->type()==QEvent::KeyPress){auto* widget=qobject_cast<QWidget*>(watched);if(widget&&widget->window()==this){const auto key=static_cast<QKeyEvent*>(event)->key();if(key==Qt::Key_F5){setPresentationView(!presentationView_);return true;}if(presentationView_){if(key==Qt::Key_Escape){setPresentationView(false);return true;}if(key==Qt::Key_F8){editPdf_->setVisible(!editPdf_->isVisible());if(editPdf_->isVisible())editPdf_->raise();else{QToolTip::hideText();for(auto* tip:QApplication::topLevelWidgets())if(auto* label=qobject_cast<QLabel*>(tip);label&&label->windowType()==Qt::ToolTip&&label->text()==editPdf_->toolTip())label->hide();}return true;}if(key==Qt::Key_PageDown||key==Qt::Key_PageUp){page_->setValue(page_->value()+(key==Qt::Key_PageDown?1:-1));return true;}}}}
 if(watched==find_){
  if(event->type()==QEvent::FocusIn||event->type()==QEvent::MouseButtonPress)updateFindSuggestions();
  if(event->type()==QEvent::FocusOut)suggestions_->hide();
  if(event->type()==QEvent::KeyPress&&suggestions_->isVisible()){
   const auto key=static_cast<QKeyEvent*>(event)->key();
   if(key==Qt::Key_Escape){suggestions_->hide();return true;}
   if(key==Qt::Key_Down||key==Qt::Key_Up){const int step=key==Qt::Key_Down?1:-1;int row=suggestions_->currentRow();for(int attempt=0;attempt<suggestions_->count();++attempt){row=(row+step+suggestions_->count())%suggestions_->count();if(!suggestions_->item(row)->data(Qt::UserRole).toString().isEmpty()){suggestions_->setCurrentRow(row);break;}}return true;}
   if((key==Qt::Key_Return||key==Qt::Key_Enter)&&suggestions_->currentItem()){const auto term=suggestions_->currentItem()->data(Qt::UserRole).toString();if(!term.isEmpty()){find_->setText(term);suggestions_->hide();findNext();return true;}}
  }
 }
 if(event->type()==QEvent::MouseButtonPress&&suggestions_->isVisible()){auto* widget=qobject_cast<QWidget*>(watched);if(widget&&widget!=find_&&widget!=suggestions_&&!suggestions_->isAncestorOf(widget))suggestions_->hide();}
 return QWidget::eventFilter(watched,event);
}
void NativeDocumentContext::updateFindSuggestions(){
 const auto query=find_->text().trimmed();suggestions_->clear();if(query.isEmpty()||path_.isEmpty()){suggestions_->hide();return;}
 const auto corpus=pages_.join(QLatin1Char(' '));auto count=[&](const QString& term){int hits=0;for(int from=0;;){const auto at=corpus.indexOf(term,from,Qt::CaseInsensitive);if(at<0)break;++hits;from=at+term.size();}return hits;};
 std::map<QString,int> frequencies;QStringList words;auto tokens=QRegularExpression("[\\p{L}][\\p{L}\\p{N}-]{2,}").globalMatch(corpus.toLower());while(tokens.hasNext()){auto term=tokens.next().captured();words.push_back(term);++frequencies[term];}
 const auto stop=QString("about after also among been before being between both could each from have into more most other over same such than that their them then there these they this those through under very were what when where which while will with would year years").split(QLatin1Char(' '));const auto lower=query.toLower();auto eligible=[&](const QString& term){return term!=lower&&term.size()>=4&&!stop.contains(term)&&std::none_of(term.begin(),term.end(),[](QChar character){return character.isDigit();});};std::map<QString,double> scores;
 for(const auto& [term,total]:frequencies)if(eligible(term)&&term.startsWith(lower))scores[term]=1000+total;
 for(int at=0;at<words.size();++at)if(words[at]==lower)for(int nearby=std::max(0,at-8);nearby<std::min(static_cast<int>(words.size()),at+9);++nearby)if(eligible(words[nearby]))scores[words[nearby]]+=1.0/(std::abs(nearby-at)*std::sqrt(frequencies[words[nearby]]));
 std::vector<std::pair<QString,double>> ranked(scores.begin(),scores.end());std::sort(ranked.begin(),ranked.end(),[](const auto& left,const auto& right){return left.second!=right.second?left.second>right.second:left.first<right.first;});if(ranked.size()>4)ranked.resize(4);
 auto header=[&](const QString& text,int height){auto* item=new QListWidgetItem(text,suggestions_);item->setFlags(Qt::NoItemFlags);item->setSizeHint(QSize(193,height));};auto term=[&](const QString& text,bool exact){auto* item=new QListWidgetItem(text,suggestions_);item->setData(Qt::UserRole,text);item->setData(Qt::UserRole+1,count(text));item->setData(Qt::UserRole+2,exact);item->setSizeHint(QSize(193,exact?30:31));};
 header("EXACT MATCHES",28);term(query,true);if(!ranked.empty()){header("SUGGESTIONS",30);for(const auto& [text,score]:ranked)term(text,false);}
 suggestions_->setCurrentRow(-1);const int height=ranked.empty()?66:96+31*static_cast<int>(ranked.size());suggestions_->resize(201,height);const auto anchor=find_->mapTo(this,QPoint(find_->width()-201,find_->height()));suggestions_->move(std::clamp(anchor.x(),0,std::max(0,width()-201)),std::clamp(anchor.y(),0,std::max(0,this->height()-height)));suggestions_->show();suggestions_->raise();
}
void NativeDocumentContext::openPdf(const QString& path){
 setPresentationView(false);
 QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot open read-only document source");const auto hash=QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex();
 const auto info=QString::fromUtf8(run("/usr/bin/pdfinfo",{path}));const auto match=QRegularExpression("Pages:\\s+(\\d+)").match(info);if(!match.hasMatch()||match.captured(1).toInt()<1)throw std::runtime_error("PDF page count unavailable");
 path_=QFileInfo(path).canonicalFilePath();sha_=QString::fromLatin1(hash);pageCount_=match.captured(1).toInt();pages_=QString::fromUtf8(run("/usr/bin/pdftotext",{"-layout",path_,"-"})).split(QLatin1Char('\f'));lastQuery_.clear();matches_.clear();matchIndex_=-1;find_->clear();suggestions_->hide();page_->blockSignals(true);page_->setRange(1,pageCount_);page_->setValue(1);page_->blockSignals(false);render();
}
void NativeDocumentContext::render(){
 const auto page=QString::number(page_->value());QTemporaryDir temporary;if(!temporary.isValid())throw std::runtime_error("Cannot create PDF rendering directory");const auto prefix=temporary.filePath("page");
 run("/usr/bin/pdftoppm",{"-f",page,"-l",page,"-singlefile","-r",QString::number(96*zoom_->value()/100.0),"-png",path_,prefix});QPixmap pixels(prefix+".png");if(pixels.isNull())throw std::runtime_error("Cannot read rendered PDF page");surface_->setPixmap(pixels);surface_->resize(pixels.size());
 text_->setPlainText(pages_.value(page_->value()-1));status_->setText(QFileInfo(path_).fileName()+QString(" · %1 / %2 · read-only PDF adapter").arg(page_->value()).arg(pageCount_));
}
void NativeDocumentContext::findNext(){
 suggestions_->hide();
 const auto query=find_->text();if(query.isEmpty())return;
 if(query!=lastQuery_){lastQuery_=query;matches_.clear();matchIndex_=-1;for(int page=0;page<pageCount_;++page){const auto& content=pages_.at(page);for(int start=0;;){const int at=content.indexOf(query,start,Qt::CaseInsensitive);if(at<0)break;matches_.push_back({page,at});start=at+query.size();}}}
 if(matches_.empty()){status_->setText("No matches in the actual extracted document text");return;}
 matchIndex_=(matchIndex_+1)%static_cast<int>(matches_.size());const auto [page,offset]=matches_.at(static_cast<std::size_t>(matchIndex_));page_->setValue(page+1);auto cursor=text_->textCursor();cursor.setPosition(offset);cursor.setPosition(offset+query.size(),QTextCursor::KeepAnchor);text_->setTextCursor(cursor);status_->setText(QString("%1 / %2 · actual extracted PDF text").arg(matchIndex_+1).arg(matches_.size()));
}
Json NativeDocumentContext::observation()const{
 const bool tooltip=QToolTip::isVisible()&&QToolTip::text()==editPdf_->toolTip();auto tooltipText=tooltip?QTextDocumentFragment::fromHtml(QToolTip::text()).toPlainText():QString();tooltipText.remove(QChar::ObjectReplacementCharacter);
 auto suggestions=Json::array();for(int row=0;row<suggestions_->count();++row){auto* item=suggestions_->item(row);const auto term=item->data(Qt::UserRole).toString();if(!term.isEmpty())suggestions.push_back({{"term",term.toStdString()},{"count",item->data(Qt::UserRole+1).toInt()},{"exact",item->data(Qt::UserRole+2).toBool()}});}
 return {{"visible",isVisible()},{"presentationView",presentationView_},{"matchCount",matches_.size()},{"matchIndex",matchIndex_+1},{"path",path_.toStdString()},{"pageCount",pageCount_},{"page",page_->value()},{"zoomPercent",zoom_->value()},{"sha256",sha_.toStdString()},{"query",find_->text().toStdString()},{"selectedText",text_->textCursor().selectedText().toStdString()},{"scrollX",scroll_->horizontalScrollBar()->value()},{"scrollY",scroll_->verticalScrollBar()->value()},{"readOnly",text_->isReadOnly()},{"editPdfToolTipVisible",tooltip},{"toolTipText",tooltipText.simplified().toStdString()},{"findSuggestionsVisible",suggestions_->isVisible()},{"findSuggestions",suggestions}};
}
}
