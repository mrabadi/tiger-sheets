#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include "tigersheets/references.hpp"
#include <QLineEdit>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QPaintEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QTimer>
#include <algorithm>
namespace tigersheets {
namespace {
int normalDigitWidth(const QTableView& view){const QFontMetrics metrics(QFont("Calibri",11),view.viewport());int width=1;for(char digit='0';digit<='9';++digit)width=std::max(width,metrics.horizontalAdvance(QChar(digit)));return width;}
class CellDelegate:public QStyledItemDelegate {
public:
 explicit CellDelegate(WorksheetView* view):QStyledItemDelegate(view),view_(view){}
 QWidget* createEditor(QWidget* parent,const QStyleOptionViewItem&,const QModelIndex& index)const override{return view_->createCellEditor(parent,index);}
 void setEditorData(QWidget* editor,const QModelIndex& index)const override{if(auto* line=qobject_cast<QLineEdit*>(editor);line&&(line->isModified()||line->text()==index.data(Qt::EditRole).toString()))return;QStyledItemDelegate::setEditorData(editor,index);}

protected:
 void initStyleOption(QStyleOptionViewItem* option,const QModelIndex& index)const override{
  QStyledItemDelegate::initStyleOption(option,index);const auto* model=dynamic_cast<const WorksheetModel*>(index.model());if(!model)return;const auto style=model->cellStyle(index.row(),index.column());if(style.contains("wrap")&&style.at("wrap").is_boolean())option->features.setFlag(QStyleOptionViewItem::WrapText,style.at("wrap").get<bool>());
 }
 bool eventFilter(QObject* object,QEvent* event)override{if(event->type()==QEvent::KeyPress)if(auto* edit=qobject_cast<QLineEdit*>(object))if(view_->handleEditorKey(edit,static_cast<QKeyEvent*>(event)))return true;return QStyledItemDelegate::eventFilter(object,event);}
private:WorksheetView* view_;
};
}
WorksheetView::WorksheetView(QWidget* parent):QTableView(parent){verticalHeader()->setMinimumSectionSize(1);horizontalHeader()->setMinimumSectionSize(1);setMouseTracking(true);setFocusPolicy(Qt::TabFocus);setItemDelegate(new CellDelegate(this));fillScrollTimer_=new QTimer(this);fillScrollTimer_->setInterval(50);connect(fillScrollTimer_,&QTimer::timeout,this,[this]{scrollFillViewport();});connect(horizontalHeader(),&QHeaderView::sectionHandleDoubleClicked,this,[this](int column){persistAutoFitColumn(column);});connect(itemDelegate(),&QAbstractItemDelegate::closeEditor,this,[this](QWidget* editor,QAbstractItemDelegate::EndEditHint){if(editor==cellEditor_){cellEditor_.clear();if(formulaBar_){formulaBar_->setText(model()?model()->data(currentIndex(),Qt::EditRole).toString():QString());formulaBar_->setModified(false);}}});}
void WorksheetView::setModel(QAbstractItemModel* model){
 cancelFill();QObject::disconnect(layoutChanged_);QTableView::setModel(model);layoutSheet_.clear();mergedDefinition_.clear();
 if(model)layoutChanged_=connect(model,&QAbstractItemModel::dataChanged,this,[this]{synchronizeLayout();});synchronizeLayout();
}
void WorksheetView::synchronizeLayout(){
 const auto* worksheet=dynamic_cast<WorksheetModel*>(model());if(!worksheet)return;
 for(const auto& sheet:worksheet->workbookView().at("sheets"))if(sheet.at("id")==worksheet->sheetId().toStdString()){
  const auto merges=sheet.value("mergedRanges",Json::array());const auto rows=sheet.value("rowLayout",Json::array()),columns=sheet.value("columnLayout",Json::array());
  const auto hiddenRows=sheet.value("hiddenRows",Json::array()),hiddenColumns=sheet.value("hiddenColumns",Json::array());
  const auto definition=QString::fromStdString(Json::array({merges,rows,columns,hiddenRows,hiddenColumns}).dump());
  if(layoutSheet_==worksheet->sheetId()&&mergedDefinition_==definition)return;
  layoutSheet_=worksheet->sheetId();mergedDefinition_=definition;clearSpans();
  for(const auto& range:merges)setSpan(range.at("row1").get<int>()-1,range.at("col1").get<int>()-1,range.at("row2").get<int>()-range.at("row1").get<int>()+1,range.at("col2").get<int>()-range.at("col1").get<int>()+1);
  for(const int row:layoutRows_){setRowHidden(row,false);setRowHeight(row,verticalHeader()->defaultSectionSize());}layoutRows_.clear();
  for(const int col:layoutColumns_){setColumnHidden(col,false);setColumnWidth(col,horizontalHeader()->defaultSectionSize());}layoutColumns_.clear();
  for(const auto& entry:rows){const int row=entry.at("index").get<int>()-1;layoutRows_.push_back(row);if(entry.contains("size"))setRowHeight(row,qRound(entry.at("size").get<double>()*logicalDpiY()/72.0));setRowHidden(row,entry.value("hidden",false));}
  for(const auto& entry:columns){const int col=entry.at("index").get<int>()-1;layoutColumns_.push_back(col);if(entry.contains("size"))setColumnWidth(col,std::max(1,spreadsheetColumnPixels(entry.at("size").get<double>(),normalDigitWidth(*this))));setColumnHidden(col,entry.value("hidden",false));}
  for(const auto& entry:hiddenRows){const int row=entry.get<int>()-1;setRowHidden(row,true);layoutRows_.push_back(row);}
  for(const auto& entry:hiddenColumns){const int col=entry.get<int>()-1;setColumnHidden(col,true);layoutColumns_.push_back(col);}
  viewport()->update();return;
 }
}
WorksheetView::~WorksheetView(){
 auto* delegate=itemDelegate();QPointer<QLineEdit> editor=cellEditor_;
 if(editor&&delegate)delegate->closeEditor(editor,QAbstractItemDelegate::RevertModelCache);
 if(delegate)QObject::disconnect(delegate,nullptr,this,nullptr);
 if(editor)QObject::disconnect(editor,nullptr,this,nullptr);
 if(formulaBar_){formulaBar_->removeEventFilter(this);QObject::disconnect(formulaBar_,nullptr,this,nullptr);}
}
void WorksheetView::setFormulaBar(QLineEdit* editor){formulaBar_=editor;editor->installEventFilter(this);connect(editor,&QLineEdit::textChanged,this,[this]{if(!insertingReference_){referenceStart_=-1;referenceLength_=0;}viewport()->update();});}
void WorksheetView::setCurrentSheetName(const QString& name){if(currentSheetName_!=name){currentSheetName_=name;viewport()->update();}}
void WorksheetView::paintEvent(QPaintEvent* event){
 QTableView::paintEvent(event);
 QPainter painter(viewport());painter.setClipRect(event->rect());
 if(currentIndex().isValid()){
  const auto current=visualRect(currentIndex());if(current.intersects(viewport()->rect())){painter.setPen(QPen(QColor("#245648"),2));painter.setBrush(Qt::NoBrush);painter.drawRect(current.adjusted(1,1,-1,-1));}
 }
 if(filling_&&fillSource_.isValid()&&fillTarget_.isValid()){
  painter.setPen(QPen(QColor("#245648"),1,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawRect(visualRect(fillSource_).united(visualRect(fillTarget_)).adjusted(1,1,-1,-1));
 }
 const auto handle=fillHandleRect();if(!handle.isEmpty()){painter.setPen(Qt::NoPen);painter.setBrush(QColor("#245648"));painter.drawRect(handle);}
 auto* editor=referenceCandidate();if(!model()||!editor||!editor->text().startsWith('='))return;
 const auto utf8=editor->text().toUtf8();const auto references=formulaReferences(std::string_view(utf8.constData(),static_cast<std::size_t>(utf8.size())));
 const QColor colors[]={QColor("#2b66df"),QColor("#159567"),QColor("#9d4edd"),QColor("#e1822f"),QColor("#d1495b")};
 const auto width=viewport()->width(),height=viewport()->height();
 for(std::size_t i=0;i<references.size();++i){
  const auto& reference=references[i];if(reference.external||(!reference.sheet.empty()&&QString::fromStdString(reference.sheet).compare(currentSheetName_,Qt::CaseInsensitive)!=0))continue;
  const int firstRow=std::max(0,reference.row1-1),lastRow=std::min(model()->rowCount()-1,reference.row2-1),firstCol=std::max(0,reference.col1-1),lastCol=std::min(model()->columnCount()-1,reference.col2-1);
  if(lastRow<firstRow||lastCol<firstCol)continue;
  const int left=columnViewportPosition(firstCol)+1,right=columnViewportPosition(lastCol)+columnWidth(lastCol)-2,top=rowViewportPosition(firstRow)+1,bottom=rowViewportPosition(lastRow)+rowHeight(lastRow)-2;
  if(right<0||left>=width||bottom<0||top>=height)continue;
  painter.setPen(QPen(colors[i%5],2));
  const int clippedLeft=std::max(0,left),clippedRight=std::min(width-1,right),clippedTop=std::max(0,top),clippedBottom=std::min(height-1,bottom);
  if(left>=0&&left<width)painter.drawLine(left,clippedTop,left,clippedBottom);
  if(right>=0&&right<width)painter.drawLine(right,clippedTop,right,clippedBottom);
  if(top>=0&&top<height)painter.drawLine(clippedLeft,top,clippedRight,top);
  if(bottom>=0&&bottom<height)painter.drawLine(clippedLeft,bottom,clippedRight,bottom);
 }
}
bool WorksheetView::eventFilter(QObject* object,QEvent* event){
 if(auto* editor=qobject_cast<QLineEdit*>(object);editor&&editor==formulaBar_){
  if(event->type()==QEvent::FocusIn||event->type()==QEvent::FocusOut)viewport()->update();
  if(event->type()==QEvent::KeyPress&&handleEditorKey(editor,static_cast<QKeyEvent*>(event)))return true;
 }
 return QTableView::eventFilter(object,event);
}
bool WorksheetView::commitPendingEditor(bool force){
 auto* editor=cellEditor_?cellEditor_.data():formulaBar_.data();if(!editor||(!force&&!cellEditor_&&!editor->isModified()))return true;
 const QModelIndex target=cellEditor_?QModelIndex(editIndex_):currentIndex();if(!target.isValid())return false;const auto input=editor->text();
 if(model()->data(target,Qt::EditRole).toString()!=input&&!model()->setData(target,input,Qt::EditRole))return false;
 referenceStart_=-1;picking_=false;pointMode_=true;referenceCursor_={};editor->setModified(false);viewport()->update();if(formulaBar_)formulaBar_->setModified(false);if(cellEditor_)itemDelegate()->closeEditor(cellEditor_,QAbstractItemDelegate::NoHint);return true;
}
void WorksheetView::cancelEntry(){cancelFill();referenceStart_=-1;picking_=false;pointMode_=true;referenceCursor_={};viewport()->update();if(formulaBar_){formulaBar_->setText(model()->data(currentIndex(),Qt::EditRole).toString());formulaBar_->setModified(false);}if(cellEditor_)itemDelegate()->closeEditor(cellEditor_,QAbstractItemDelegate::RevertModelCache);setFocus();}
void WorksheetView::keyPressEvent(QKeyEvent* event){
 if(filling_&&event->key()==Qt::Key_Escape){cancelFill();event->accept();return;}
 QTableView::keyPressEvent(event);
}
bool WorksheetView::handleEditorKey(QLineEdit* editor,QKeyEvent* key){
 if(beforeEditorKey&&beforeEditorKey(editor,key))return true;
 if(key->key()==Qt::Key_Escape){cancelEntry();return true;}
 if(key->key()==Qt::Key_F2&&editor->text().startsWith('=')){pointMode_=!pointMode_;key->accept();return true;}
 const bool arrow=key->key()==Qt::Key_Left||key->key()==Qt::Key_Right||key->key()==Qt::Key_Up||key->key()==Qt::Key_Down;
 if(arrow&&pointMode_&&!(key->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))&&canPoint(editor)){
  const bool continuing=referenceEditor_==editor&&referenceStart_>=0&&editor->cursorPosition()==referenceStart_+referenceLength_&&referenceCursor_.isValid();
  const auto base=continuing?referenceCursor_:currentIndex();if(!base.isValid())return false;
  const int row=std::clamp(base.row()+(key->key()==Qt::Key_Up?-1:key->key()==Qt::Key_Down?1:0),0,model()->rowCount()-1);
  const int col=std::clamp(base.column()+(key->key()==Qt::Key_Left?-1:key->key()==Qt::Key_Right?1:0),0,model()->columnCount()-1);const auto target=model()->index(row,col);
  if(!continuing){referenceStart_=editor->hasSelectedText()?editor->selectionStart():editor->cursorPosition();referenceLength_=editor->selectedText().size();referenceEditor_=editor;referenceAnchor_=(key->modifiers()&Qt::ShiftModifier)?base:target;}
  else if(!(key->modifiers()&Qt::ShiftModifier))referenceAnchor_=target;
  insertPointedRange(target);scrollTo(target);key->accept();return true;
 }

 if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter||key->key()==Qt::Key_Tab||key->key()==Qt::Key_Backtab){
  const auto index=currentIndex();const bool tab=key->key()==Qt::Key_Tab||key->key()==Qt::Key_Backtab;const int direction=(key->modifiers()&Qt::ShiftModifier)||key->key()==Qt::Key_Backtab?-1:1;
  if(commitPendingEditor(true)){const auto next=model()->index(std::clamp(index.row()+(tab?0:direction),0,model()->rowCount()-1),std::clamp(index.column()+(tab?direction:0),0,model()->columnCount()-1));setCurrentIndex(next);scrollTo(next);setFocus();}return true;
 }return false;
}
QLineEdit* WorksheetView::activeCellEditor()const{return cellEditor_;}
QLineEdit* WorksheetView::createCellEditor(QWidget* parent,const QModelIndex& index){
 pointMode_=true;referenceStart_=-1;referenceLength_=0;referenceCursor_={};
 auto* editor=editorFactory?editorFactory(parent):new QLineEdit(parent);cellEditor_=editor;editIndex_=index;editor->setObjectName("worksheet.cellEditor");editor->setAccessibleName(QString("Cell %1%2").arg(QString::fromStdString(columnName(index.column()+1))).arg(index.row()+1));
 connect(editor,&QLineEdit::textChanged,this,[this](const QString& text){if(formulaBar_){formulaBar_->setText(text);formulaBar_->setModified(false);}});
 connect(editor,&QLineEdit::textChanged,this,[this]{if(!insertingReference_){referenceStart_=-1;referenceLength_=0;}viewport()->update();});
 return editor;
}
QLineEdit* WorksheetView::referenceCandidate()const{return cellEditor_?cellEditor_.data():(formulaBar_&&(formulaBar_->hasFocus()||formulaBar_->isModified())?formulaBar_.data():nullptr);}
bool WorksheetView::canPoint(QLineEdit* editor)const{
 if(!editor||editor->isReadOnly()||!editor->text().startsWith('='))return false;const auto prefix=editor->text().left(editor->cursorPosition());bool quoted=false;for(int i=0;i<prefix.size();++i)if(prefix[i]=='"'){if(quoted&&i+1<prefix.size()&&prefix[i+1]=='"'){++i;continue;}quoted=!quoted;}if(quoted)return false;
 if(referenceEditor_==editor&&referenceStart_>=0&&editor->cursorPosition()==referenceStart_+referenceLength_)return true;
 const auto trimmed=prefix.trimmed();return !trimmed.isEmpty()&&QString("=+-*/^&(<>,:;").contains(trimmed.back());
}
void WorksheetView::insertPointedRange(const QModelIndex& end){
 if(!referenceEditor_||!end.isValid()||!referenceAnchor_.isValid())return;const int row1=std::min(referenceAnchor_.row(),end.row()),row2=std::max(referenceAnchor_.row(),end.row()),col1=std::min(referenceAnchor_.column(),end.column()),col2=std::max(referenceAnchor_.column(),end.column());
 auto address=[](int row,int col){return QString::fromStdString(columnName(col+1))+QString::number(row+1);};auto text=address(row1,col1);if(row1!=row2||col1!=col2)text+=":"+address(row2,col2);else if(pointedReference){auto custom=pointedReference(referenceAnchor_,end);if(custom.startsWith('='))custom.remove(0,1);if(!custom.isEmpty())text=std::move(custom);}
 insertingReference_=true;referenceEditor_->setSelection(referenceStart_,referenceLength_);referenceEditor_->insert(text);referenceEditor_->setModified(true);referenceLength_=text.size();referenceCursor_=end;insertingReference_=false;viewport()->update();
}
void WorksheetView::mousePressEvent(QMouseEvent* event){
 auto* editor=referenceCandidate();const auto index=indexAt(event->pos());if(event->button()==Qt::LeftButton&&index.isValid()&&canPoint(editor)){
  if(referenceEditor_!=editor||referenceStart_<0||editor->cursorPosition()!=referenceStart_+referenceLength_){referenceStart_=editor->hasSelectedText()?editor->selectionStart():editor->cursorPosition();referenceLength_=editor->selectedText().size();}
  referenceEditor_=editor;referenceAnchor_=index;picking_=true;insertPointedRange(index);event->accept();return;
 }
 if(event->button()==Qt::LeftButton&&fillHandleRect().adjusted(-2,-2,2,2).contains(event->pos())&&!fillHandleRect().isEmpty()){
  if(commitPendingEditor()){fillSource_=currentIndex();fillTarget_=fillSource_;filling_=true;setFocus(Qt::MouseFocusReason);viewport()->update();}event->accept();return;
 }
 setFocus(Qt::MouseFocusReason);QTableView::mousePressEvent(event);
}
QRect WorksheetView::fillHandleRect()const{
 const auto index=currentIndex();if(!dynamic_cast<WorksheetModel*>(model())||!index.isValid()||cellEditor_||!(model()->flags(index)&Qt::ItemIsEditable)||rowSpan(index.row(),index.column())!=1||columnSpan(index.row(),index.column())!=1)return {};
 const auto rect=visualRect(index);return QRect(rect.bottomRight()-QPoint(4,4),QSize(7,7));
}
void WorksheetView::cancelFill(){if(fillScrollTimer_)fillScrollTimer_->stop();filling_=false;fillSource_=QModelIndex();fillTarget_=QModelIndex();viewport()->unsetCursor();viewport()->update();}
void WorksheetView::finishFill(){
 auto* worksheet=dynamic_cast<WorksheetModel*>(model());if(!worksheet||!fillSource_.isValid()||!fillTarget_.isValid()||fillSource_==fillTarget_)return;
 const int sourceRow=fillSource_.row(),sourceColumn=fillSource_.column();const auto input=worksheet->raw(sourceRow,sourceColumn).toStdString();const auto style=worksheet->cellStyle(sourceRow,sourceColumn);Json operations=Json::array();
 for(int row=std::min(sourceRow,fillTarget_.row());row<=std::max(sourceRow,fillTarget_.row());++row)for(int col=std::min(sourceColumn,fillTarget_.column());col<=std::max(sourceColumn,fillTarget_.column());++col){if(row==sourceRow&&col==sourceColumn)continue;auto copied=input;if(!copied.empty()&&copied[0]=='=')copied=shiftFormula(copied,row-sourceRow,col-sourceColumn);operations.push_back({{"type","set_cell"},{"sheet",worksheet->sheetId().toStdString()},{"row",row+1},{"col",col+1},{"input",copied},{"style",style},{"replaceStyle",true}});}
 worksheet->apply(operations);
}
void WorksheetView::updateFillTarget(const QPoint& position){
 fillPosition_=position;const auto index=indexAt(QPoint(std::clamp(position.x(),0,std::max(0,viewport()->width()-1)),std::clamp(position.y(),0,std::max(0,viewport()->height()-1))));if(!index.isValid()||!fillSource_.isValid())return;
 const bool vertical=std::abs(index.row()-fillSource_.row())>std::abs(index.column()-fillSource_.column());fillTarget_=model()->index(vertical?index.row():fillSource_.row(),vertical?fillSource_.column():index.column());
 const int coordinate=vertical?position.y():position.x(),extent=vertical?viewport()->height():viewport()->width();
 if(filling_&&hasAutoScroll()&&(coordinate<autoScrollMargin()||coordinate>=extent-autoScrollMargin())){if(!fillScrollTimer_->isActive())fillScrollTimer_->start();}else fillScrollTimer_->stop();
}
void WorksheetView::scrollFillViewport(){
 if(!filling_||!fillSource_.isValid()||!fillTarget_.isValid()){cancelFill();return;}
 const bool vertical=fillSource_.row()!=fillTarget_.row();const int coordinate=vertical?fillPosition_.y():fillPosition_.x(),extent=vertical?viewport()->height():viewport()->width();
 const int direction=coordinate<autoScrollMargin()?-1:coordinate>=extent-autoScrollMargin()?1:0;if(!direction){fillScrollTimer_->stop();return;}
 auto* bar=vertical?verticalScrollBar():horizontalScrollBar();const bool pixels=(vertical?verticalScrollMode():horizontalScrollMode())==ScrollPerPixel;
 const int step=pixels?std::max(1,(vertical?rowHeight(fillTarget_.row()):columnWidth(fillTarget_.column()))/3):1;const int before=bar->value();bar->setValue(before+direction*step);
 if(bar->value()==before){fillScrollTimer_->stop();return;}updateFillTarget(fillPosition_);viewport()->update();
}
void WorksheetView::persistAutoFitColumn(int column){
 auto* worksheet=dynamic_cast<WorksheetModel*>(model());if(!worksheet||column<0||column>=model()->columnCount()||!(model()->flags(model()->index(0,column))&Qt::ItemIsEditable))return;
 resizeColumnToContents(column);const int digitWidth=normalDigitWidth(*this);
 const double characters=std::clamp((columnWidth(column)-5.0)/digitWidth,0.0,spreadsheetColumnCharacters(255.0,digitWidth));
 const double width=spreadsheetColumnWidth(characters,digitWidth);if(width<=0)return;
 for(const auto& sheet:worksheet->workbookView().at("sheets"))if(sheet.at("id")==worksheet->sheetId().toStdString()){
  auto columns=sheet.value("columnLayout",Json::array());
  for(const auto& hidden:sheet.value("hiddenColumns",Json::array())){const int index=hidden.get<int>();auto existing=std::find_if(columns.begin(),columns.end(),[index](const Json& value){return value.at("index")==index;});if(existing==columns.end())columns.push_back({{"index",index},{"hidden",true}});else (*existing)["hidden"]=true;}
  auto entry=std::find_if(columns.begin(),columns.end(),[column](const Json& value){return value.at("index")==column+1;});
  if(entry==columns.end())columns.push_back({{"index",column+1},{"size",width}});else (*entry)["size"]=width;
  worksheet->apply(Json::array({{{"type","set_sheet_layout"},{"sheet",worksheet->sheetId().toStdString()},{"columns",std::move(columns)}}}));return;
 }
}
void WorksheetView::mouseMoveEvent(QMouseEvent* event){
 if(picking_){insertPointedRange(indexAt(event->pos()));event->accept();return;}
 if(filling_){updateFillTarget(event->pos());viewport()->update();event->accept();return;}
 if(!fillHandleRect().isEmpty()&&fillHandleRect().adjusted(-2,-2,2,2).contains(event->pos()))viewport()->setCursor(Qt::CrossCursor);else viewport()->unsetCursor();QTableView::mouseMoveEvent(event);
}
void WorksheetView::mouseReleaseEvent(QMouseEvent* event){
 if(picking_&&event->button()==Qt::LeftButton){insertPointedRange(indexAt(event->pos()));picking_=false;event->accept();return;}
 if(filling_&&event->button()==Qt::LeftButton){updateFillTarget(event->pos());filling_=false;finishFill();cancelFill();event->accept();return;}
 QTableView::mouseReleaseEvent(event);
}

}
