#include "tigersheets/worksheet_border_paint.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include "tigersheets/worksheet_headers.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/formula_reference_presentation.hpp"
#include "tigersheets/formatting.hpp"
#include "tigersheets/formula_error_checks.hpp"
#include <QLineEdit>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QPaintEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QShowEvent>
#include <QHideEvent>
#include <QFocusEvent>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTextLayout>
#include <QtMath>
#include <algorithm>
#include <array>
#include <set>
#include <tuple>
namespace tigersheets {
namespace {
QItemSelection selectionRectangles(const QItemSelection& selected){
 if(selected.size()<2)return selected;QRegion area;for(const auto& range:selected)area+=QRect(range.left(),range.top(),range.width(),range.height());const auto bounds=area.boundingRect();if(!area.isEmpty()&&area==QRegion(bounds)){const auto* model=selected.front().model();return QItemSelection(model->index(bounds.top(),bounds.left()),model->index(bounds.bottom(),bounds.right()));}return selected;
}
QRect cellBoundaryRect(const WorksheetView& view,const QModelIndex& index){
 auto rect=view.visualRect(index);if(!rect.isEmpty()&&!view.showGrid())rect.adjust(0,0,-1,-1);return rect;
}
void selectionName(WorksheetView& view,bool dimensions){
 auto* hint=view.viewport()->findChild<QLabel*>("worksheet.selectionRangeHint");if(hint)hint->hide();
 auto* name=view.window()->findChild<QLineEdit*>("formula.name");if(!name||!view.currentIndex().isValid())return;
 const auto selected=selectionRectangles(view.selectionModel()->selection());if(dimensions&&selected.size()==1){
  const auto range=selected.front();const auto text=range.height()==view.model()->rowCount()?QString::number(range.width())+"C":QString::number(range.height())+"R x "+QString::number(range.width())+"C";
  const auto anchor=view.property("worksheet.dragAnchor").value<QModelIndex>();
  if(anchor.isValid()&&!view.viewport()->rect().intersects(view.visualRect(anchor))&&(range.width()>1||range.height()>1)){
   name->clear();if(!hint){hint=new QLabel(view.viewport());hint->setObjectName("worksheet.selectionRangeHint");hint->setAccessibleName("Selection dimensions");hint->setFocusPolicy(Qt::NoFocus);hint->setAttribute(Qt::WA_TransparentForMouseEvents);hint->setStyleSheet("color:#000000;background:#ffffff;border:1px solid #767676;padding:0px 3px;");}
   hint->setText(text);hint->setFixedSize(std::max(46,hint->fontMetrics().horizontalAdvance(text)+8),20);const auto preferred=view.visualRect(range.bottomRight()).bottomRight()+QPoint(4,4);hint->move(std::clamp(preferred.x(),0,std::max(0,view.viewport()->width()-hint->width())),std::clamp(preferred.y(),0,std::max(0,view.viewport()->height()-hint->height())));hint->show();hint->raise();
  }else name->setText(text);return;
 }
 const auto index=view.currentIndex();name->setText(QString::fromStdString(columnName(index.column()+1))+QString::number(index.row()+1));
}
void retainDragAnchor(WorksheetView& view,const QModelIndex& anchor){
 if(!anchor.isValid())return;
 const bool autoScroll=view.hasAutoScroll();view.setAutoScroll(false);
 view.selectionModel()->setCurrentIndex(anchor,QItemSelectionModel::NoUpdate);
 view.setAutoScroll(autoScroll);
}
void syncVisibleHeaders(WorksheetView& view){
 const int last=std::max(0,view.rowAt(std::max(0,view.viewport()->height()-1)));const int width=std::max(28,view.verticalHeader()->fontMetrics().horizontalAdvance(QString::number(last+1))+10);if(view.verticalHeader()->width()!=width||view.verticalHeader()->minimumWidth()!=width)view.verticalHeader()->setFixedWidth(width);
}
int partialColumnStart(const QLineEdit& editor,int position){
 const auto prefix=editor.text().left(position);int start=prefix.size();while(start>0&&((prefix[start-1]>='A'&&prefix[start-1]<='Z')||(prefix[start-1]>='a'&&prefix[start-1]<='z')))--start;
 if(start==prefix.size()||!columnNumber(prefix.mid(start).toStdString()))return -1;if(start>0&&prefix[start-1]=='$')--start;const auto before=prefix.left(start).trimmed();return !before.isEmpty()&&QString("=+-*/^&(<>,:;").contains(before.back())?start:-1;
}
void entryContext(WorksheetView& view,const QString& mode,const QString& text={},const QModelIndex& pointed={},bool range=false){
 view.setProperty("worksheet.entryMode",mode);
 if(auto* status=view.window()->findChild<QLabel*>("status.saveState"))status->setText(mode);
 if(text.startsWith('=')){if(auto* name=view.window()->findChild<QLineEdit*>("formula.name")){const auto references=formulaReferences(text.toStdString());const bool externalOrigin=view.hasReferenceSheetEntry()&&std::any_of(references.begin(),references.end(),[](const auto& reference){return reference.external;});if(externalOrigin||(mode=="Enter"&&text=="="))name->setText("SUM");else if(mode=="Point"){if(pointed.isValid()){if(range)name->clear();else name->setText(QString::fromStdString(columnName(pointed.column()+1))+QString::number(pointed.row()+1));}}else if(mode=="Edit"){auto* bar=view.window()->findChild<QLineEdit*>("formula.input");if(bar&&bar->hasFocus())name->clear();else name->setText(view.window()->property("formula.lastInsertFunction").toString().isEmpty()?QString("SUM"):view.window()->property("formula.lastInsertFunction").toString());}else selectionName(view,false);}}
 else selectionName(view,false);
}
bool balancedFormulaParentheses(const QString& input){
 if(!input.startsWith('='))return true;
 int depth=0;QChar quote;bool bracket=false;
 for(int index=1;index<input.size();++index){const auto ch=input[index];
  if(!quote.isNull()){if(ch==quote){if(index+1<input.size()&&input[index+1]==quote)++index;else quote=QChar();}continue;}
  if(ch=='"'||ch=='\''){quote=ch;continue;}
  if(ch=='['){bracket=true;continue;}if(ch==']'){bracket=false;continue;}if(bracket)continue;
  if(ch=='(')++depth;else if(ch==')'&&--depth<0)return false;
 }
 return depth==0;
}
QString singleMissingFunctionClose(const QString& input){
 if(!input.startsWith('='))return {};
 std::vector<int> openings;QChar quote;bool bracket=false;
 for(int index=1;index<input.size();++index){const auto ch=input[index];
  if(!quote.isNull()){if(ch==quote){if(index+1<input.size()&&input[index+1]==quote)++index;else quote=QChar();}continue;}
  if(ch=='"'||ch=='\''){quote=ch;continue;}
  if(ch=='['){bracket=true;continue;}if(ch==']'){if(!bracket)return {};bracket=false;continue;}if(bracket)continue;
  if(ch=='(')openings.push_back(index);else if(ch==')'){if(openings.empty())return {};openings.pop_back();}
 }
 if(!quote.isNull()||bracket||openings.size()!=1)return {};
 const auto before=input.left(openings.front()).trimmed();int start=before.size();
 while(start>0&&(before[start-1].isLetterOrNumber()||before[start-1]=='_'||before[start-1]=='.'))--start;
 if(start==before.size()||(!before[start].isLetter()&&before[start]!='_')||(start>0&&!QString("=+-*/^&(<>,:;").contains(before[start-1])))return {};
 const auto tail=input.trimmed();if(tail.isEmpty()||(!tail.back().isLetterOrNumber()&&!QString("\")]%").contains(tail.back())))return {};
 const auto corrected=input+")";return balancedFormulaParentheses(corrected)?corrected:QString();
}
QRegion copyBorderRegion(const QRect& rect){return QRegion(rect.adjusted(-2,-2,2,2))-QRegion(rect.adjusted(2,2,-2,-2));}
class CopyBorder final:public QWidget {
public:
 explicit CopyBorder(QWidget* parent):QWidget(parent){
  setObjectName("worksheet.copyBorder");setAccessibleName("Copied cells");setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);setFocusPolicy(Qt::NoFocus);setProperty("dashOffset",0.0);hide();
  animation=new QVariantAnimation(this);animation->setObjectName("worksheet.copyAnimation");animation->setStartValue(0.0);animation->setEndValue(6.0);animation->setDuration(600);animation->setLoopCount(-1);connect(animation,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){setProperty("dashOffset",value);update(copyBorderRegion(property("sourceRect").toRect()));});
 }
 QVariantAnimation* animation;
protected:
 void paintEvent(QPaintEvent*)override{const auto source=property("sourceRect").toRect();if(source.isEmpty())return;QPainter painter(this);painter.setBrush(Qt::NoBrush);painter.setPen(QPen(Qt::white,2));painter.drawRect(source);QPen ants(QColor("#217346"),2);ants.setDashPattern({3,3});ants.setDashOffset(property("dashOffset").toReal());painter.setPen(ants);painter.drawRect(source);}
 void showEvent(QShowEvent* event)override{QWidget::showEvent(event);if(animation->state()==QAbstractAnimation::Paused)animation->resume();else if(animation->state()!=QAbstractAnimation::Running)animation->start();}
 void hideEvent(QHideEvent* event)override{if(animation->state()==QAbstractAnimation::Running)animation->pause();QWidget::hideEvent(event);}
};
int normalDigitWidth(const QTableView&){return SpreadsheetNormalDigitWidth;}
void expandFormulaEditor(QLineEdit& editor,const WorksheetView& view){
 const auto base=editor.property("worksheet.editorBaseRect").toRect();if(!base.isValid())return;auto bounds=base;
 const auto index=editor.property("worksheet.editorIndex").value<QPersistentModelIndex>();if(index.isValid()&&index.model()==view.model()){const auto original=editor.property("worksheet.editorCellRect").toRect(),current=view.visualRect(index);if(original.isValid()&&current.isValid())bounds.translate(current.topLeft()-original.topLeft());}
 const auto anchored=bounds;bool wrap=false;
 if(editor.text().startsWith('=')){
  editor.setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
  // Use the same fractional glyph layout as wrapping. Integer font metrics
  // can round a bold formula down and strand its closing parenthesis.
  QTextLayout unwrapped(editor.text(),editor.font());unwrapped.beginLayout();auto fullLine=unwrapped.createLine();fullLine.setLineWidth(100000000);unwrapped.endLayout();
  const int required=qCeil(fullLine.naturalTextWidth())+4;
  bounds.setWidth(std::max(anchored.width(),std::min(required,view.viewport()->width()-anchored.left())));
  if(editor.property("formula.supportsSoftWrap").toBool()){
   QTextLayout layout(editor.text(),editor.font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(option);int rows=0;
   layout.beginLayout();for(;;){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(std::max(1,bounds.width()-4));++rows;}layout.endLayout();
   wrap=rows>1;const int pitch=std::max(anchored.height(),editor.fontMetrics().height()+2);editor.setProperty("formula.linePitch",pitch);
   if(wrap)bounds.setHeight(std::max(anchored.height(),std::min(rows*pitch,view.viewport()->height()-anchored.top())));
  }
 }
 editor.setProperty("formula.softWrap",wrap);if(editor.geometry()!=bounds)editor.setGeometry(bounds);
}
struct FilledNumber {QString text;std::vector<QChar> padding,fills;};
std::optional<FilledNumber> filledNumber(double value,const std::string& format){
 const auto code=QString::fromStdString(format);if(!code.contains('*'))return std::nullopt;FilledNumber result;QString adjusted;bool quoted=false;
 for(int i=0;i<code.size();++i){const auto ch=code[i];if(ch=='"'){quoted=!quoted;adjusted+=ch;continue;}if(!quoted&&ch=='\\'&&i+1<code.size()){adjusted+=ch;adjusted+=code[++i];continue;}if(!quoted&&ch=='['){do{adjusted+=code[i];}while(code[i]!=']'&&++i<code.size());continue;}if(!quoted&&(ch=='_'||ch=='*')&&i+1<code.size()){auto& list=ch=='_'?result.padding:result.fills;if(list.size()>=128)return std::nullopt;const QChar marker((ch=='_'?0xe100:0xe000)+static_cast<ushort>(list.size()));list.push_back(code[++i]);adjusted+='\\';adjusted+=marker;continue;}adjusted+=ch;}
 try{result.text=QString::fromStdString(formatCellValue(value,adjusted.toStdString()));}catch(const std::exception&){return std::nullopt;}bool found=false;for(const auto ch:result.text)found|=ch.unicode()>=0xe000&&ch.unicode()<0xe000+result.fills.size();if(!found)return std::nullopt;return result;
}
class CellDelegate:public QStyledItemDelegate {
public:
 explicit CellDelegate(WorksheetView* view):QStyledItemDelegate(view),view_(view){}
 void setFormulaBar(QLineEdit* bar,std::function<bool()> transfer){bar_=bar;transferToBar_=std::move(transfer);}
 QWidget* createEditor(QWidget* parent,const QStyleOptionViewItem&,const QModelIndex& index)const override{return view_->createCellEditor(parent,index);}
 void updateEditorGeometry(QWidget* editor,const QStyleOptionViewItem& option,const QModelIndex& index)const override{QStyledItemDelegate::updateEditorGeometry(editor,option,index);if(auto* line=qobject_cast<QLineEdit*>(editor)){line->setProperty("worksheet.editorBaseRect",line->geometry());line->setProperty("worksheet.editorCellRect",option.rect);line->setProperty("worksheet.editorIndex",QVariant::fromValue(QPersistentModelIndex(index)));expandFormulaEditor(*line,*view_);}}
 void setEditorData(QWidget* editor,const QModelIndex& index)const override{if(auto* line=qobject_cast<QLineEdit*>(editor);line&&(line->isModified()||line->text()==index.data(Qt::EditRole).toString()))return;QStyledItemDelegate::setEditorData(editor,index);}
 void setModelData(QWidget* editor,QAbstractItemModel* model,const QModelIndex& index)const override{if(auto* line=qobject_cast<QLineEdit*>(editor);line&&line->property("worksheet.replaceInput").toBool()&&dynamic_cast<WorksheetModel*>(model)){model->setData(index,line->text(),WorksheetModel::ReplacementEditRole);return;}QStyledItemDelegate::setModelData(editor,model,index);}
 QRect textOverflowRect(const QModelIndex& index,const QStyleOptionViewItem& option)const{
  const auto* model=dynamic_cast<const WorksheetModel*>(index.model());const int row=index.row(),column=index.column();if(!model||model->numericValue(row,column)||option.text.isEmpty()||(option.features&QStyleOptionViewItem::WrapText)||!(option.displayAlignment&Qt::AlignLeft)||view_->rowSpan(row,column)!=1||view_->columnSpan(row,column)!=1||(view_->activeCellEditor()&&index==view_->currentIndex()))return {};
  const auto original=view_->visualRect(index);auto expanded=original;const int indent=model->cellStyle(row,column).value("indent",0)*option.fontMetrics.horizontalAdvance("   ");const int required=option.fontMetrics.horizontalAdvance(option.text)+indent+6;if(required<=original.width())return {};
  for(int next=column+1;next<model->columnCount()&&expanded.width()<required&&expanded.right()<view_->viewport()->width();++next){if(view_->isColumnHidden(next))continue;if(!model->raw(row,next).isEmpty()||view_->rowSpan(row,next)!=1||view_->columnSpan(row,next)!=1)break;expanded=expanded.united(view_->visualRect(model->index(row,next)));}
  return expanded.right()>original.right()?expanded:QRect();
 }
 void paintTextOverflow(QPainter& painter)const{
  const auto* model=dynamic_cast<const WorksheetModel*>(view_->model());if(!model)return;
  const int firstRow=std::max(0,view_->rowAt(0)),firstColumn=std::max(0,view_->columnAt(0));const int rowEnd=view_->rowAt(view_->viewport()->height()-1),columnEnd=view_->columnAt(view_->viewport()->width()-1);const int lastRow=rowEnd<0?model->rowCount()-1:rowEnd,lastColumn=columnEnd<0?model->columnCount()-1:columnEnd;
  for(int row=firstRow;row<=lastRow;++row)for(int column=firstColumn;column<=lastColumn;++column){
   if(view_->isRowHidden(row)||view_->isColumnHidden(column)||model->raw(row,column).isEmpty()||model->numericValue(row,column)||view_->rowSpan(row,column)!=1||view_->columnSpan(row,column)!=1)continue;
   const auto index=model->index(row,column);QStyleOptionViewItem option;option.initFrom(view_);option.widget=view_;option.features.setFlag(QStyleOptionViewItem::WrapText,view_->wordWrap());initStyleOption(&option,index);const auto expanded=textOverflowRect(index,option);if(expanded.isEmpty())continue;const int indent=model->cellStyle(row,column).value("indent",0)*option.fontMetrics.horizontalAdvance("   ");option.rect=expanded.adjusted(indent,0,0,0);option.textElideMode=Qt::ElideNone;option.backgroundBrush=Qt::NoBrush;option.features.setFlag(QStyleOptionViewItem::Alternate,false);option.state&=~(QStyle::State_Selected|QStyle::State_HasFocus);painter.save();painter.setClipRect(expanded,Qt::IntersectClip);view_->style()->drawControl(QStyle::CE_ItemViewItem,&option,&painter,view_);painter.restore();
  }
 }
 void paintBorders(QPainter& painter)const{
  const auto* model=dynamic_cast<const WorksheetModel*>(view_->model());if(!model)return;
  auto visibleSections=[&](bool columns){
   std::vector<int> result;const int extent=columns?view_->viewport()->width():view_->viewport()->height();
   for(int at=0;at<extent;){const int section=columns?view_->columnAt(at):view_->rowAt(at);if(section<0)break;result.push_back(section);const int next=columns?view_->columnViewportPosition(section)+view_->columnWidth(section):view_->rowViewportPosition(section)+view_->rowHeight(section);at=std::max(at+1,next);}
   return result;
  };
  const auto rows=visibleSections(false),columns=visibleSections(true);if(rows.empty()||columns.empty())return;
  using Span=std::array<int,4>;std::vector<Span> spans;
  for(const auto& sheet:model->workbookView().at("sheets"))if(sheet.at("id")==model->sheetId().toStdString()){
   if(const auto merges=sheet.find("mergedRanges");merges!=sheet.end())for(const auto& range:*merges){
    const Span span{range.at("row1").get<int>()-1,range.at("row2").get<int>()-1,range.at("col1").get<int>()-1,range.at("col2").get<int>()-1};
    const int top=view_->rowViewportPosition(span[0]),bottom=view_->rowViewportPosition(span[1])+view_->rowHeight(span[1]),left=view_->columnViewportPosition(span[2]),right=view_->columnViewportPosition(span[3])+view_->columnWidth(span[3]);
    if(bottom>top&&right>left&&bottom>=-4&&top<=view_->viewport()->height()+4&&right>=-4&&left<=view_->viewport()->width()+4)spans.push_back(span);
   }break;
  }
  auto spanAt=[&](int row,int column){for(const auto& span:spans)if(row>=span[0]&&row<=span[1]&&column>=span[2]&&column<=span[3])return span;return Span{row,row,column,column};};
  const Json emptyStyle=Json::object(),noEdge;std::map<std::pair<int,int>,Json> styles;
  auto styleAt=[&](int row,int column)->const Json&{
   if(row<0||column<0||row>=model->rowCount()||column>=model->columnCount())return emptyStyle;
   const auto key=std::pair{row,column};const auto found=styles.find(key);if(found!=styles.end())return found->second;
   return styles.emplace(key,model->cellStyle(key.first,key.second)).first->second;
  };
  auto edgeAt=[&](int row,int column,const char* side)->const Json&{const auto& style=styleAt(row,column);const auto border=style.find("border");if(border==style.end()||!border->is_object())return noEdge;const auto edge=border->find(side);return edge==border->end()?noEdge:*edge;};
  auto mergedEdgeAt=[&](int row,int column,const char* side){
   const auto span=spanAt(row,column);return effectiveWorksheetBorderEdge(edgeAt(row,column,side),edgeAt(span[0],span[2],side));
  };
  auto opposingEdgeAt=[&](int row,int column,const char* side){
   if(row<0||column<0||row>=model->rowCount()||column>=model->columnCount()||view_->isRowHidden(row)||view_->isColumnHidden(column))return Json();
   return mergedEdgeAt(row,column,side);
  };
  // Resolve each physical edge once, then join equal collinear segments so
  // grid separators and per-cell dash restarts cannot interrupt a border.
  using Line=std::pair<bool,int>;using Segment=std::pair<int,int>;
  std::map<Line,std::map<Segment,Json>> edges;
  auto add=[&](bool horizontal,int fixed,int start,int end,const Json& edge){
   if(edge.is_null()||start>end||fixed<-4||fixed>(horizontal?view_->viewport()->height():view_->viewport()->width())+4)return;
   auto& segments=edges[{horizontal,fixed}];const Segment segment{start,end};const auto prior=segments.find(segment);if(prior==segments.end())segments.emplace(segment,edge);else prior->second=effectiveWorksheetBorderEdge(edge,prior->second);
  };
  std::set<std::pair<int,int>> visited;std::vector<std::pair<QRect,Json>> diagonals;
  for(int row:rows)for(int column:columns){
   const auto span=spanAt(row,column);if(!visited.emplace(span[0],span[2]).second)continue;
   const auto& style=styleAt(span[0],span[2]);if(const auto border=style.find("border");border!=style.end()&&(border->contains("diagonalUp")||border->contains("diagonalDown")))diagonals.emplace_back(view_->visualRect(model->index(span[0],span[2])),*border);
   auto horizontalEdges=[&](int at){
    const int start=view_->columnViewportPosition(at)-(at?1:0),end=view_->columnViewportPosition(at)+view_->columnWidth(at)-1;
    add(true,view_->rowViewportPosition(span[0])-(span[0]?1:0),start,end,effectiveWorksheetBorderEdge(mergedEdgeAt(span[0],at,"top"),opposingEdgeAt(span[0]-1,at,"bottom")));
    add(true,view_->rowViewportPosition(span[1])+view_->rowHeight(span[1])-1,start,end,effectiveWorksheetBorderEdge(opposingEdgeAt(span[1]+1,at,"top"),mergedEdgeAt(span[1],at,"bottom")));
   };
   auto verticalEdges=[&](int at){
    const int start=view_->rowViewportPosition(at)-(at?1:0),end=view_->rowViewportPosition(at)+view_->rowHeight(at)-1;
    add(false,view_->columnViewportPosition(span[2])-(span[2]?1:0),start,end,effectiveWorksheetBorderEdge(mergedEdgeAt(at,span[2],"left"),opposingEdgeAt(at,span[2]-1,"right")));
    add(false,view_->columnViewportPosition(span[3])+view_->columnWidth(span[3])-1,start,end,effectiveWorksheetBorderEdge(opposingEdgeAt(at,span[3]+1,"left"),mergedEdgeAt(at,span[3],"right")));
   };
   if(span[2]==span[3])horizontalEdges(span[2]);else for(int at:columns)if(at>=span[2]&&at<=span[3])horizontalEdges(at);
   if(span[0]==span[1])verticalEdges(span[0]);else for(int at:rows)if(at>=span[0]&&at<=span[1])verticalEdges(at);
  }
  struct Run{bool horizontal;int fixed,start,end;Json edge;};std::vector<Run> runs;
  auto same=[](const Json& a,const Json& b){return a.at("style")==b.at("style")&&QColor(QString::fromStdString(a.at("color")))==QColor(QString::fromStdString(b.at("color")));};
  for(const auto& [line,segments]:edges){
   std::optional<Run> run;
   for(const auto& [segment,edge]:segments){if(run&&segment.first<=run->end+1&&same(run->edge,edge))run->end=std::max(run->end,segment.second);else{if(run)runs.push_back(std::move(*run));run=Run{line.first,line.second,segment.first,segment.second,edge};}}
   if(run)runs.push_back(std::move(*run));
  }
  std::stable_sort(runs.begin(),runs.end(),[](const Run& a,const Run& b){return worksheetBorderStrength(a.edge.at("style").get_ref<const std::string&>())<worksheetBorderStrength(b.edge.at("style").get_ref<const std::string&>());});
  for(const auto& run:runs)paintWorksheetBorderLine(painter,run.horizontal?QLineF(run.start,run.fixed,run.end,run.fixed):QLineF(run.fixed,run.start,run.fixed,run.end),run.edge.at("style").get_ref<const std::string&>(),QColor(QString::fromStdString(run.edge.at("color"))));
  for(const auto& [rect,borders]:diagonals)for(const auto* side:{"diagonalUp","diagonalDown"})if(const auto edge=borders.find(side);edge!=borders.end())paintWorksheetBorder(painter,rect,side,edge->value("style",std::string()),QColor(QString::fromStdString(edge->value("color",std::string("#000000")))));
 }
 void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override{
  const auto* model=dynamic_cast<const WorksheetModel*>(index.model());if(!model)return;const auto style=model->cellStyle(index.row(),index.column());
  auto content=option;const int indent=style.value("indent",0);
  if(indent>0){
   QStyleOptionViewItem background(option);initStyleOption(&background,index);
   const auto* widget=option.widget;const auto* nativeStyle=widget?widget->style():QApplication::style();nativeStyle->drawPrimitive(QStyle::PE_PanelItemViewItem,&background,painter,widget);
   const int padding=std::min(content.rect.width(),indent*background.fontMetrics.horizontalAdvance("   "));
   if(background.displayAlignment&Qt::AlignRight)content.rect.adjust(0,0,-padding,0);else content.rect.adjust(padding,0,0,0);
  }
  const auto number=model->numericValue(index.row(),index.column());if(!number)content.textElideMode=Qt::ElideNone;const auto filled=number?filledNumber(*number,style.value("numberFormat",std::string())):std::nullopt;
  if(filled){
   QStyleOptionViewItem native(content);initStyleOption(&native,index);native.text.clear();const auto* widget=option.widget;const auto* nativeStyle=widget?widget->style():QApplication::style();nativeStyle->drawControl(QStyle::CE_ItemViewItem,&native,painter,widget);painter->save();painter->setClipRect(content.rect);painter->setFont(native.font);painter->setPen(native.palette.color(QPalette::Text));const QFontMetricsF metrics(native.font);const auto rect=content.rect.adjusted(2,0,-2,0);int fillAt=-1;QChar fillChar=' ';for(int i=0;i<filled->text.size();++i){const auto marker=filled->text[i].unicode();if(marker>=0xe000&&marker<0xe000+filled->fills.size()){fillAt=i;fillChar=filled->fills[marker-0xe000];break;}}
   const auto left=filled->text.left(fillAt),right=filled->text.mid(fillAt+1);auto width=[&](const QString& text){qreal value=0;QString run;for(const auto ch:text){if(ch.unicode()>=0xe100&&ch.unicode()<0xe100+filled->padding.size()){value+=metrics.horizontalAdvance(run)+metrics.horizontalAdvance(filled->padding[ch.unicode()-0xe100]);run.clear();}else run+=ch;}return value+metrics.horizontalAdvance(run);};const qreal leftWidth=width(left),rightWidth=width(right),gap=std::max(qreal(0),rect.width()-leftWidth-rightWidth);qreal y=rect.bottom()-metrics.descent()+1;if(native.displayAlignment&Qt::AlignTop)y=rect.top()+metrics.ascent();else if(native.displayAlignment&Qt::AlignVCenter)y=rect.top()+(rect.height()-metrics.height())/2+metrics.ascent();auto draw=[&](const QString& text,qreal x){QString run;auto flush=[&]{if(!run.isEmpty()){painter->drawText(QPointF(x,y),run);x+=metrics.horizontalAdvance(run);run.clear();}};for(const auto ch:text){if(ch.unicode()>=0xe100&&ch.unicode()<0xe100+filled->padding.size()){flush();x+=metrics.horizontalAdvance(filled->padding[ch.unicode()-0xe100]);}else run+=ch;}flush();};draw(left,rect.left());if(fillChar!=' '){const qreal advance=metrics.horizontalAdvance(fillChar);if(advance>0)draw(QString(std::min(10000,static_cast<int>(gap/advance)),fillChar),rect.left()+leftWidth);}draw(right,rect.left()+leftWidth+gap);painter->restore();
  }else{QStyleOptionViewItem native(content);initStyleOption(&native,index);if(!textOverflowRect(index,native).isEmpty()){native.text.clear();const auto* widget=option.widget;const auto* nativeStyle=widget?widget->style():QApplication::style();nativeStyle->drawControl(QStyle::CE_ItemViewItem,&native,painter,widget);}else QStyledItemDelegate::paint(painter,content,index);}
  if(index.data(FormulaErrorCheckRole).toBool()){painter->save();painter->setClipRect(option.rect);painter->setPen(Qt::NoPen);painter->setBrush(QColor("#008000"));const auto corner=option.rect.topLeft();painter->drawPolygon(QPolygon({corner,corner+QPoint(6,0),corner+QPoint(0,6)}));painter->restore();}

 }

protected:
 void initStyleOption(QStyleOptionViewItem* option,const QModelIndex& index)const override{
  QStyledItemDelegate::initStyleOption(option,index);const auto* model=dynamic_cast<const WorksheetModel*>(index.model());if(!model)return;const auto style=model->cellStyle(index.row(),index.column());if(style.contains("wrap")&&style.at("wrap").is_boolean())option->features.setFlag(QStyleOptionViewItem::WrapText,style.at("wrap").get<bool>());
  const auto preview=view_->property("worksheet.stylePreview").toMap();if(preview.value("range").toRect().contains(QPoint(index.column(),index.row()))){const QColor color(preview.value("color").toString());if(preview.value("property")=="fill")option->backgroundBrush=color.isValid()?QBrush(color):QBrush(Qt::white);else if(color.isValid())option->palette.setColor(QPalette::Text,color);}
  for(const auto& item:view_->property("worksheet.referenceFills").toList()){const auto fill=item.toMap();if(!fill.value("range").toRect().contains(QPoint(index.column(),index.row())))continue;const auto color=fill.value("color").value<QColor>();const auto base=option->backgroundBrush.style()==Qt::NoBrush?option->palette.color(QPalette::Base):option->backgroundBrush.color();option->backgroundBrush=QColor(qRound(base.red()*.88+color.red()*.12),qRound(base.green()*.88+color.green()*.12),qRound(base.blue()*.88+color.blue()*.12));break;}
  if(option->state&QStyle::State_Selected){const auto base=option->backgroundBrush.style()==Qt::NoBrush?option->palette.color(QPalette::Base):option->backgroundBrush.color();option->backgroundBrush=index==view_->currentIndex()||view_->property("worksheet.externalPointing").toBool()?base:QColor(qRound(base.red()*0.78),qRound(base.green()*0.78),qRound(base.blue()*0.78));option->state&=~QStyle::State_Selected;}option->state&=~QStyle::State_HasFocus;
 }
 bool eventFilter(QObject* object,QEvent* event)override{if(event->type()==QEvent::FocusOut){if(view_->property("worksheet.draftWarning").toBool()||static_cast<QFocusEvent*>(event)->reason()==Qt::ActiveWindowFocusReason)return true;if(bar_&&QApplication::focusWidget()==bar_&&object==view_->activeCellEditor()&&transferToBar_&&transferToBar_())return true;}if(event->type()==QEvent::KeyPress)if(auto* edit=qobject_cast<QLineEdit*>(object))if(view_->handleEditorKey(edit,static_cast<QKeyEvent*>(event)))return true;return QStyledItemDelegate::eventFilter(object,event);}
private:WorksheetView* view_;QPointer<QLineEdit> bar_;std::function<bool()> transferToBar_;
};
}
WorksheetView::WorksheetView(QWidget* parent):QTableView(parent){QFont headerFont("Calibri");headerFont.setPointSizeF(9);horizontalHeader()->setFont(headerFont);verticalHeader()->setFont(headerFont);horizontalHeader()->setFixedHeight(20);QTimer::singleShot(0,this,[this]{syncVisibleHeaders(*this);});verticalHeader()->setMinimumSectionSize(1);horizontalHeader()->setMinimumSectionSize(1);horizontalHeader()->viewport()->installEventFilter(this);connect(horizontalHeader(),&QHeaderView::sectionPressed,this,[this]{if(headerSelectionActive_)updateSelectionDimensions(true);});connect(horizontalHeader(),&QHeaderView::sectionEntered,this,[this]{if(headerSelectionActive_)updateSelectionDimensions(true);});setMouseTracking(true);setFocusPolicy(Qt::TabFocus);setItemDelegate(new CellDelegate(this));fillScrollTimer_=new QTimer(this);fillScrollTimer_->setInterval(50);connect(fillScrollTimer_,&QTimer::timeout,this,[this]{scrollFillViewport();});connect(horizontalHeader(),&QHeaderView::sectionHandleDoubleClicked,this,[this](int column){persistAutoFitColumn(column);});connect(itemDelegate(),&QAbstractItemDelegate::closeEditor,this,[this](QWidget* editor,QAbstractItemDelegate::EndEditHint){if(editor==cellEditor_){cellEditor_.clear();if(formulaBar_){formulaBar_->setText(model()?model()->data(currentIndex(),Qt::EditRole).toString():QString());formulaBar_->setModified(false);}entryContext(*this,"Ready");}});}
void WorksheetView::setModel(QAbstractItemModel* model){
 cancelFill();clearCopyRange();QObject::disconnect(layoutChanged_);QTableView::setModel(model);installWorksheetHeaders(*this);layoutSheet_.clear();mergedDefinition_.clear();
 if(model)layoutChanged_=connect(model,&QAbstractItemModel::dataChanged,this,[this](const QModelIndex& first,const QModelIndex& last){synchronizeLayout();updateCopyBorder();const auto current=currentIndex();if(current.isValid()&&current.row()>=first.row()&&current.row()<=last.row()&&current.column()>=first.column()&&current.column()<=last.column())viewport()->update();else{const int top=std::max(0,rowViewportPosition(first.row())),bottom=std::min(viewport()->height(),rowViewportPosition(last.row())+rowHeight(last.row()));if(bottom>top)viewport()->update(QRect(0,top,viewport()->width(),bottom-top));}});synchronizeLayout();
}
void WorksheetView::setCopyRange(const QString& sheet,const QItemSelectionRange& range){
 if(sheet.isEmpty()||!range.isValid()||range.model()!=model()){clearCopyRange();return;}if(!copyBorder_){copyBorder_=new CopyBorder(viewport());connect(horizontalHeader(),&QHeaderView::sectionResized,this,[this]{updateCopyBorder();});connect(verticalHeader(),&QHeaderView::sectionResized,this,[this]{updateCopyBorder();});}copySheet_=sheet;copyRange_=range;updateCopyBorder();
}
void WorksheetView::clearCopyRange(){copySheet_.clear();copyRange_=QItemSelectionRange();if(copyBorder_){static_cast<CopyBorder*>(copyBorder_)->animation->stop();copyBorder_->hide();copyBorder_->setProperty("sourceRect",QRect());}}
void WorksheetView::updateCopyBorder(){
 if(!copyBorder_)return;const auto* worksheet=dynamic_cast<const WorksheetModel*>(model());if(!worksheet||!copyRange_.isValid()||worksheet->sheetId()!=copySheet_){copyBorder_->hide();return;}
 const auto previous=copyBorder_->property("sourceRect").toRect();QRect source(columnViewportPosition(copyRange_.left()),rowViewportPosition(copyRange_.top()),columnViewportPosition(copyRange_.right())+columnWidth(copyRange_.right())-columnViewportPosition(copyRange_.left())-1,rowViewportPosition(copyRange_.bottom())+rowHeight(copyRange_.bottom())-rowViewportPosition(copyRange_.top())-1);
 if(source.isEmpty()){copyBorder_->hide();return;}source=source.united(visualRect(copyRange_.topLeft())).united(visualRect(copyRange_.bottomRight())).adjusted(1,1,-1,-1);if(property("worksheet.frozenBody").toBool()&&copyRange_.top()==0&&isRowHidden(0))source.setTop(-2);copyBorder_->setGeometry(viewport()->rect());copyBorder_->setProperty("sourceRect",source);copyBorder_->setVisible(source.intersects(viewport()->rect()));copyBorder_->raise();copyBorder_->update(copyBorderRegion(previous)|copyBorderRegion(source));
}
void WorksheetView::alignHorizontalEnd(){
 // QTableView aligns the final section to the viewport edge even in item
 // mode. Keep the leading column whole, allowing the unused trailing space.
 auto* bar=horizontalScrollBar();auto* header=horizontalHeader();
 if(horizontalScrollMode()!=ScrollPerItem||layoutDirection()!=Qt::LeftToRight||bar->maximum()<=bar->minimum()||bar->value()!=bar->maximum())return;
 const int first=columnAt(0);if(first<0||columnViewportPosition(first)>=0)return;
 int visual=header->visualIndex(first)+1;
 while(visual<header->count()&&header->isSectionHidden(header->logicalIndex(visual)))++visual;
 if(visual<header->count()){header->setOffset(header->sectionPosition(header->logicalIndex(visual)));viewport()->update();}
}
void WorksheetView::resizeEvent(QResizeEvent* event){QTableView::resizeEvent(event);syncVisibleHeaders(*this);alignHorizontalEnd();updateCopyBorder();}
void WorksheetView::scrollContentsBy(int dx,int dy){QTableView::scrollContentsBy(dx,dy);syncVisibleHeaders(*this);alignHorizontalEnd();updateCopyBorder();if(property("worksheet.dragAnchor").value<QModelIndex>().isValid())selectionName(*this,true);}
void WorksheetView::synchronizeLayout(){
 const auto* worksheet=dynamic_cast<WorksheetModel*>(model());if(!worksheet)return;
 for(const auto& sheet:worksheet->workbookView().at("sheets"))if(sheet.at("id")==worksheet->sheetId().toStdString()){
  const auto merges=sheet.value("mergedRanges",Json::array());const auto rows=sheet.value("rowLayout",Json::array()),columns=sheet.value("columnLayout",Json::array());
  const auto hiddenRows=sheet.value("hiddenRows",Json::array()),hiddenColumns=sheet.value("hiddenColumns",Json::array());
  const auto definition=QString::fromStdString(Json::array({merges,rows,columns,hiddenRows,hiddenColumns,sheet.value("showGridLines",true),sheet.value("defaultRowHeight",0.0),sheet.value("defaultColumnWidth",0.0)}).dump());
  if(layoutSheet_==worksheet->sheetId()&&mergedDefinition_==definition)return;
  layoutSheet_=worksheet->sheetId();mergedDefinition_=definition;clearSpans();
  setShowGrid(sheet.value("showGridLines",true));
  const int previousDefaultRow=verticalHeader()->defaultSectionSize(),previousDefaultColumn=horizontalHeader()->defaultSectionSize();
  const int nextDefaultRow=std::max(verticalHeader()->minimumSectionSize(),sheet.contains("defaultRowHeight")?qRound(sheet.at("defaultRowHeight").get<double>()*logicalDpiY()/72.0):property("worksheet.baseRowHeight").isValid()?property("worksheet.baseRowHeight").toInt():previousDefaultRow);
  const int nextDefaultColumn=sheet.contains("defaultColumnWidth")?std::max(1,spreadsheetColumnPixels(sheet.at("defaultColumnWidth").get<double>(),normalDigitWidth(*this))):property("worksheet.baseColumnWidth").isValid()?property("worksheet.baseColumnWidth").toInt():previousDefaultColumn;
  const bool rowDefaultChanged=nextDefaultRow!=previousDefaultRow,columnDefaultChanged=nextDefaultColumn!=previousDefaultColumn;
  // Qt emits sectionResized for every row when a default changes. Publish the
  // completed geometry once, after custom and hidden sections are restored.
  QSignalBlocker rowSignals(verticalHeader()),columnSignals(horizontalHeader());
  if(!rowDefaultChanged)rowSignals.unblock();if(!columnDefaultChanged)columnSignals.unblock();
  // With updates enabled, Qt resolves all section positions after each custom
  // resize. Defer that work until the sparse overrides have been restored.
  const bool resumeRowUpdates=rowDefaultChanged&&verticalHeader()->updatesEnabled(),resumeColumnUpdates=columnDefaultChanged&&horizontalHeader()->updatesEnabled();
  if(resumeRowUpdates)verticalHeader()->setUpdatesEnabled(false);if(resumeColumnUpdates)horizontalHeader()->setUpdatesEnabled(false);
  if(sheet.contains("defaultRowHeight")){
   if(!property("worksheet.baseRowHeight").isValid())setProperty("worksheet.baseRowHeight",verticalHeader()->defaultSectionSize());
   if(rowDefaultChanged)verticalHeader()->setDefaultSectionSize(nextDefaultRow);
  }else if(property("worksheet.baseRowHeight").isValid()){if(rowDefaultChanged)verticalHeader()->setDefaultSectionSize(nextDefaultRow);setProperty("worksheet.baseRowHeight",QVariant());}
  if(sheet.contains("defaultColumnWidth")){
   if(!property("worksheet.baseColumnWidth").isValid())setProperty("worksheet.baseColumnWidth",horizontalHeader()->defaultSectionSize());
   if(columnDefaultChanged)horizontalHeader()->setDefaultSectionSize(nextDefaultColumn);
  }else if(property("worksheet.baseColumnWidth").isValid()){if(columnDefaultChanged)horizontalHeader()->setDefaultSectionSize(nextDefaultColumn);setProperty("worksheet.baseColumnWidth",QVariant());}
  for(const auto& range:merges)setSpan(range.at("row1").get<int>()-1,range.at("col1").get<int>()-1,range.at("row2").get<int>()-range.at("row1").get<int>()+1,range.at("col2").get<int>()-range.at("col1").get<int>()+1);
  SectionLayouts nextRows,nextColumns;
  for(const auto& entry:rows)nextRows[entry.at("index").get<int>()-1]={entry.contains("size")?qRound(entry.at("size").get<double>()*logicalDpiY()/72.0):verticalHeader()->defaultSectionSize(),entry.value("hidden",false)};
  for(const auto& entry:columns)nextColumns[entry.at("index").get<int>()-1]={entry.contains("size")?std::max(1,spreadsheetColumnPixels(entry.at("size").get<double>(),normalDigitWidth(*this))):horizontalHeader()->defaultSectionSize(),entry.value("hidden",false)};
  for(const auto& entry:hiddenRows){const int row=entry.get<int>()-1;if(!nextRows.contains(row))nextRows[row]={verticalHeader()->defaultSectionSize(),true};else nextRows[row].second=true;}
  for(const auto& entry:hiddenColumns){const int col=entry.get<int>()-1;if(!nextColumns.contains(col))nextColumns[col]={horizontalHeader()->defaultSectionSize(),true};else nextColumns[col].second=true;}
  auto applySections=[](QHeaderView* header,SectionLayouts& previous,SectionLayouts next,bool defaultChanged){
   for(const auto& [index,layout]:previous)if(!next.contains(index)){if(header->isSectionHidden(index))header->setSectionHidden(index,false);if(header->sectionSize(index)!=header->defaultSectionSize())header->resizeSection(index,header->defaultSectionSize());}
   for(const auto& [index,layout]:next){
    const auto old=previous.find(index);const bool hidden=header->isSectionHidden(index);
    // Hidden sections report zero pixels, so compare their retained target size.
    if(defaultChanged||old==previous.end()||old->second.first!=layout.first||(!hidden&&header->sectionSize(index)!=layout.first))header->resizeSection(index,layout.first);
    if(hidden!=layout.second)header->setSectionHidden(index,layout.second);
   }
   previous=std::move(next);
  };
  applySections(verticalHeader(),layoutRows_,std::move(nextRows),previousDefaultRow!=verticalHeader()->defaultSectionSize());
  applySections(horizontalHeader(),layoutColumns_,std::move(nextColumns),previousDefaultColumn!=horizontalHeader()->defaultSectionSize());
  if(resumeRowUpdates)verticalHeader()->setUpdatesEnabled(true);if(resumeColumnUpdates)horizontalHeader()->setUpdatesEnabled(true);
  rowSignals.unblock();columnSignals.unblock();
  if(rowDefaultChanged||columnDefaultChanged){updateGeometries();if(rowDefaultChanged)verticalHeader()->geometriesChanged();if(columnDefaultChanged)horizontalHeader()->geometriesChanged();}
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
void WorksheetView::setFormulaBar(QLineEdit* editor){formulaBar_=editor;if(auto* delegate=dynamic_cast<CellDelegate*>(itemDelegate()))delegate->setFormulaBar(editor,[this]{if(!prepareReferenceChange(true))return false;pointMode_=false;referenceStart_=-1;referenceLength_=0;referenceCursor_={};entryContext(*this,"Edit",formulaBar_->text());return true;});editor->installEventFilter(this);connect(editor,&QLineEdit::textChanged,this,[this]{if(!insertingReference_){referenceStart_=-1;referenceLength_=0;}viewport()->update();});connect(editor,&QLineEdit::textEdited,this,[this](const QString& text){entryContext(*this,pointMode_?"Enter":"Edit",text);});}
void WorksheetView::setCurrentSheetName(const QString& name){if(currentSheetName_!=name){currentSheetName_=name;viewport()->update();}}
void WorksheetView::currentChanged(const QModelIndex& current,const QModelIndex& previous){QTableView::currentChanged(current,previous);viewport()->update();}
void WorksheetView::selectionChanged(const QItemSelection& selected,const QItemSelection& deselected){QTableView::selectionChanged(selected,deselected);viewport()->update();}
void WorksheetView::paintEvent(QPaintEvent* event){
 const bool externalPointing=property("worksheet.externalPointing").toBool();
 auto* editor=referenceCandidate();const auto formula=editor?editor->text():QString();const auto utf8=formula.toUtf8();const std::string_view source(utf8.constData(),static_cast<std::size_t>(utf8.size()));const auto references=formula.startsWith('=')?formulaReferences(source):std::vector<FormulaReference>();const auto colors=formulaReferenceColorIndexes(source,references);
 const auto* worksheet=dynamic_cast<const WorksheetModel*>(model());const bool localReferencesHere=!hasReferenceSheetEntry()||!worksheet||worksheet->sheetId()==entryOriginSheet_;
 auto referenceHere=[&](const FormulaReference& reference){const bool externalPointing=property("worksheet.externalPointing").toBool();if(reference.external){if(!externalPointing)return false;const auto fragment=QString::fromUtf8(utf8.mid(reference.start,reference.length));if(!fragment.contains("["+window()->property("workbook.name").toString()+"]",Qt::CaseInsensitive))return false;}else if(externalPointing)return false;if(!reference.sheet.empty()&&QString::fromStdString(reference.sheet).compare(currentSheetName_,Qt::CaseInsensitive)!=0)return false;return !reference.sheet.empty()||localReferencesHere;};
 QVariantList fills;if(!externalPointing)for(std::size_t i=0;i<references.size();++i)if(referenceHere(references[i])){const auto& reference=references[i];fills.push_back(QVariantMap{{"range",QRect(reference.col1-1,reference.row1-1,reference.col2-reference.col1+1,reference.row2-reference.row1+1)},{"color",formulaReferenceColor(colors[i])}});}if(property("worksheet.referenceFills")!=QVariant(fills))setProperty("worksheet.referenceFills",fills);
 QTableView::paintEvent(event);
 QPainter painter(viewport());painter.setClipRect(event->rect());
 if(const auto* delegate=dynamic_cast<const CellDelegate*>(itemDelegate())){delegate->paintTextOverflow(painter);delegate->paintBorders(painter);}
 if(externalPointing){const auto range=property("worksheet.externalPointRange").toRect();if(range.isValid()){const QRect bounds(columnViewportPosition(range.left()),rowViewportPosition(range.top()),columnViewportPosition(range.right())+columnWidth(range.right())-columnViewportPosition(range.left()),rowViewportPosition(range.bottom())+rowHeight(range.bottom())-rowViewportPosition(range.top()));QPen pen(QColor("#217346"),2);pen.setDashPattern({3,1.5});painter.setPen(pen);painter.setBrush(Qt::NoBrush);painter.drawRect(bounds);}return;}
 if(selectionModel()){
  painter.setPen(QPen(QColor("#217346"),2));painter.setBrush(Qt::NoBrush);
  for(const auto& range:selectionRectangles(selectionModel()->selection())){QRect bounds(columnViewportPosition(range.left()),rowViewportPosition(range.top()),columnViewportPosition(range.right())+columnWidth(range.right())-columnViewportPosition(range.left())-1,rowViewportPosition(range.bottom())+rowHeight(range.bottom())-rowViewportPosition(range.top())-1);bounds=bounds.united(cellBoundaryRect(*this,range.topLeft())).united(cellBoundaryRect(*this,range.bottomRight())).adjusted(-1,-1,1,1);if(property("worksheet.frozenBody").toBool()&&range.top()==0&&isRowHidden(0))bounds.setTop(-2);if(bounds.intersects(viewport()->rect()))painter.drawRect(bounds);}
 }
 if(filling_&&fillSource_.isValid()&&fillTarget_.isValid()){
  painter.setPen(QPen(QColor("#217346"),1,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawRect(visualRect(fillSource_).united(visualRect(fillTarget_)).adjusted(1,1,-1,-1));
 }
 const auto handle=fillHandleRect();if(!handle.isEmpty()){painter.setPen(Qt::NoPen);painter.setBrush(QColor("#217346"));painter.drawRect(handle);}
 if(!model()||references.empty())return;
 const auto width=viewport()->width(),height=viewport()->height();
 for(std::size_t i=0;i<references.size();++i){
  const auto& reference=references[i];if(!referenceHere(reference))continue;
  const int firstRow=std::max(0,reference.row1-1),lastRow=std::min(model()->rowCount()-1,reference.row2-1),firstCol=std::max(0,reference.col1-1),lastCol=std::min(model()->columnCount()-1,reference.col2-1);
  if(lastRow<firstRow||lastCol<firstCol)continue;
  const int left=columnViewportPosition(firstCol)+1,right=columnViewportPosition(lastCol)+columnWidth(lastCol)-2,top=rowViewportPosition(firstRow)+1,bottom=rowViewportPosition(lastRow)+rowHeight(lastRow)-2;
  if(right<0||left>=width||bottom<0||top>=height)continue;
  const auto color=formulaReferenceColor(colors[i]);painter.setPen(QPen(color,2,Qt::DashLine));
  const int clippedLeft=std::max(0,left),clippedRight=std::min(width-1,right),clippedTop=std::max(0,top),clippedBottom=std::min(height-1,bottom);
  if(left>=0&&left<width)painter.drawLine(left,clippedTop,left,clippedBottom);
  if(right>=0&&right<width)painter.drawLine(right,clippedTop,right,clippedBottom);
  if(top>=0&&top<height)painter.drawLine(clippedLeft,top,clippedRight,top);
  if(bottom>=0&&bottom<height)painter.drawLine(clippedLeft,bottom,clippedRight,bottom);
  painter.setPen(Qt::NoPen);painter.setBrush(color);for(const auto& corner:{QPoint(left,top),QPoint(right,top),QPoint(left,bottom),QPoint(right,bottom)})if(viewport()->rect().contains(corner))painter.drawRect(QRect(corner-QPoint(1,1),QSize(4,4)));
 }
}
bool WorksheetView::eventFilter(QObject* object,QEvent* event){
 if(object==horizontalHeader()->viewport()){
  if(event->type()==QEvent::MouseButtonPress&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton){headerSelectionActive_=true;setProperty("worksheet.selectionExtensionActive",true);}
  if(event->type()==QEvent::MouseButtonRelease&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton&&headerSelectionActive_){headerSelectionActive_=false;finishSelectionExtension();}
 }
 if(auto* editor=qobject_cast<QLineEdit*>(object);editor&&editor==formulaBar_){
  if(event->type()==QEvent::FocusIn||event->type()==QEvent::FocusOut)viewport()->update();
  if(event->type()==QEvent::KeyPress&&handleEditorKey(editor,static_cast<QKeyEvent*>(event)))return true;
 }
 return QTableView::eventFilter(object,event);
}
bool WorksheetView::commitPendingEditor(bool force){
 auto* editor=cellEditor_?cellEditor_.data():formulaBar_.data();if(!editor||(!force&&!cellEditor_&&!editor->isModified()))return true;
 const QModelIndex target=hasReferenceSheetEntry()?QModelIndex(entryOriginIndex_):cellEditor_?QModelIndex(editIndex_):currentIndex();if(!target.isValid())return false;auto input=editor->text();
 if(!balancedFormulaParentheses(input)){
  if(property("worksheet.draftWarning").toBool())return false;
  const auto corrected=singleMissingFunctionClose(input);bool accepted=false;setProperty("worksheet.draftWarning",true);
  {const QSignalBlocker blocker(editor);if(!corrected.isEmpty()){QMessageBox question(QMessageBox::Information,"TigerSheets","We found a typo in your formula and tried to correct it to:\n\n"+corrected+"\n\nDo you want to accept this correction?",QMessageBox::Yes|QMessageBox::No,window());question.setObjectName("worksheet.formulaCorrection");question.setTextFormat(Qt::PlainText);question.setDefaultButton(QMessageBox::Yes);accepted=question.exec()==QMessageBox::Yes;}
   if(!accepted)QMessageBox::warning(window(),"TigerSheets","The formula is missing an opening or closing parenthesis.",QMessageBox::Ok);window()->activateWindow();editor->setFocus(Qt::OtherFocusReason);}
  if(accepted){input=corrected;editor->setText(input);editor->setModified(true);}setProperty("worksheet.draftWarning",false);if(!accepted)return false;
 }
 if(hasReferenceSheetEntry()){
  auto* worksheet=dynamic_cast<WorksheetModel*>(model());if(!worksheet||!(worksheet->flags(target)&Qt::ItemIsEditable))return false;
  Json operation={{"type","set_cell"},{"sheet",entryOriginSheet_.toStdString()},{"row",target.row()+1},{"col",target.column()+1},{"input",input.toStdString()}};
  if(worksheet->sheetId()==entryOriginSheet_)if(const auto format=worksheet->inferredFormulaNumberFormat(target,input))operation["style"]={{"numberFormat",*format}};
  if(!worksheet->apply(Json::array({std::move(operation)})))return false;
 }else{
  const bool replacement=editor->property("worksheet.replaceInput").toBool();
  if((replacement||model()->data(target,Qt::EditRole).toString()!=input)&&!model()->setData(target,input,replacement&&dynamic_cast<WorksheetModel*>(model())?WorksheetModel::ReplacementEditRole:Qt::EditRole))return false;
 }
 referenceStart_=-1;picking_=false;pointMode_=true;referenceCursor_={};editor->setModified(false);viewport()->update();if(formulaBar_)formulaBar_->setModified(false);if(cellEditor_)itemDelegate()->closeEditor(cellEditor_,QAbstractItemDelegate::NoHint);finishReferenceSheetEntry();entryContext(*this,"Ready");return true;
}
bool WorksheetView::hasReferenceSheetEntry()const{return !entryOriginSheet_.isEmpty();}
bool WorksheetView::prepareReferenceSheetChange(){return !property("worksheet.externalPointing").toBool()&&prepareReferenceChange(false);}
bool WorksheetView::prepareReferenceWorkbookChange(){return prepareReferenceChange(true);}
QLineEdit* WorksheetView::resumeReferenceCellEditor(){
 if(cellEditor_)return cellEditor_;
 const auto* worksheet=dynamic_cast<const WorksheetModel*>(model());
 if(!formulaBar_||!worksheet||!hasReferenceSheetEntry()||worksheet->sheetId()!=entryOriginSheet_||!entryOriginIndex_.isValid())return nullptr;
 const auto text=formulaBar_->text();const int cursor=formulaBar_->cursorPosition(),selection=formulaBar_->selectionStart(),length=formulaBar_->selectedText().size();
 formulaBar_->setModified(false);
 {
  const QSignalBlocker blocker(formulaBar_);edit(entryOriginIndex_);
  if(!cellEditor_){formulaBar_->setText(text);formulaBar_->setModified(true);return nullptr;}
  cellEditor_->setText(text);cellEditor_->setModified(true);
  if(selection>=0)cellEditor_->setSelection(selection,length);else cellEditor_->setCursorPosition(cursor);
  formulaBar_->setText(text);formulaBar_->setModified(false);
 }
 cellEditor_->setFocus(Qt::OtherFocusReason);return cellEditor_;
}
bool WorksheetView::prepareReferenceChange(bool anyFormula){
 if(finishingReferenceEntry_||(cellEditor_&&!formulaBar_))return false;auto* editor=referenceCandidate();if(!editor||(!hasReferenceSheetEntry()&&!canPoint(editor)&&!(anyFormula&&editor->text().startsWith('='))))return false;
 if(!hasReferenceSheetEntry()){const auto* worksheet=dynamic_cast<const WorksheetModel*>(model());if(!worksheet)return false;entryOriginSheet_=worksheet->sheetId();entryOriginIndex_=cellEditor_?QModelIndex(editIndex_):currentIndex();}
 if(cellEditor_){
  const auto text=editor->text();const int cursor=editor->cursorPosition(),selection=editor->selectionStart(),length=editor->selectedText().size();const bool pointed=referenceEditor_==editor;
  cellEditor_.clear();itemDelegate()->closeEditor(editor,QAbstractItemDelegate::RevertModelCache);insertingReference_=true;formulaBar_->setText(text);formulaBar_->setModified(true);formulaBar_->setFocus(Qt::OtherFocusReason);if(selection>=0)formulaBar_->setSelection(selection,length);else formulaBar_->setCursorPosition(cursor);if(pointed)referenceEditor_=formulaBar_;insertingReference_=false;
 }
 return true;
}
void WorksheetView::finishReferenceSheetEntry(){
 const auto origin=entryOriginSheet_;entryOriginSheet_.clear();entryOriginIndex_=QModelIndex();
 if(!origin.isEmpty()&&entrySheetRequested){finishingReferenceEntry_=true;entrySheetRequested(origin);finishingReferenceEntry_=false;}
}
void WorksheetView::cancelEntry(){cancelFill();referenceStart_=-1;picking_=false;pointMode_=true;referenceCursor_={};finishReferenceSheetEntry();viewport()->update();if(formulaBar_){formulaBar_->setText(model()->data(currentIndex(),Qt::EditRole).toString());formulaBar_->setModified(false);}if(cellEditor_)itemDelegate()->closeEditor(cellEditor_,QAbstractItemDelegate::RevertModelCache);entryContext(*this,"Ready");setFocus();}
void WorksheetView::keyPressEvent(QKeyEvent* event){
 if(filling_&&event->key()==Qt::Key_Escape){cancelFill();event->accept();return;}
 const bool tab=event->key()==Qt::Key_Tab||event->key()==Qt::Key_Backtab;
 const bool enter=event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter;
 if((tab||(enter&&!copyRange_.isValid()))&&!(event->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))&&!cellEditor_&&!(formulaBar_&&formulaBar_->isModified())&&!hasReferenceSheetEntry()&&!property("worksheet.externalPointing").toBool()&&selectionModel()){
  const auto selected=selectionModel()->selection();const auto current=currentIndex();
  if(selected.size()==1&&selected.front().contains(current)&&(selected.front().height()>1||selected.front().width()>1)){
   const auto range=selected.front();int row=current.row(),column=current.column();
   if(enter){
    if(event->modifiers()&Qt::ShiftModifier){if(row>range.top())--row;else{row=range.bottom();column=column>range.left()?column-1:range.right();}}
    else if(row<range.bottom())++row;else{row=range.top();column=column<range.right()?column+1:range.left();}
   }
   else if(event->key()==Qt::Key_Backtab||(event->modifiers()&Qt::ShiftModifier)){if(column>range.left())--column;else{column=range.right();row=row>range.top()?row-1:range.bottom();}}
   else if(column<range.right())++column;else{column=range.left();row=row<range.bottom()?row+1:range.top();}
   const auto next=model()->index(row,column);selectionModel()->setCurrentIndex(next,QItemSelectionModel::NoUpdate);setProperty("worksheet.keyboardAnchor",QVariant());setProperty("worksheet.keyboardExtent",QVariant());setProperty("worksheet.selectionExtensionActive",false);scrollTo(next);selectionName(*this,false);event->accept();return;
  }
 }
 const bool arrow=event->key()==Qt::Key_Left||event->key()==Qt::Key_Right||event->key()==Qt::Key_Up||event->key()==Qt::Key_Down;
 if(arrow&&!(event->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))&&((event->modifiers()&Qt::ShiftModifier)||property("worksheet.externalPointing").toBool())){
  const auto from=navigationIndex(true);if(from.isValid()){const auto row=std::clamp(from.row()+(event->key()==Qt::Key_Down?1:event->key()==Qt::Key_Up?-1:0),0,model()->rowCount()-1),col=std::clamp(from.column()+(event->key()==Qt::Key_Right?1:event->key()==Qt::Key_Left?-1:0),0,model()->columnCount()-1);navigateTo(model()->index(row,col),event->modifiers()&Qt::ShiftModifier);event->accept();return;}
 }
 if(arrow){setProperty("worksheet.keyboardAnchor",QVariant());setProperty("worksheet.keyboardExtent",QVariant());}
 const bool freshF2=event->key()==Qt::Key_F2&&event->modifiers()==Qt::NoModifier&&!cellEditor_;
 const bool freshInput=!cellEditor_&&!event->text().isEmpty()&&event->text().front().isPrint()&&!(event->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier));
 QTableView::keyPressEvent(event);
 if(freshInput&&cellEditor_)cellEditor_->setProperty("worksheet.replaceInput",true);
 if(freshF2&&cellEditor_){pointMode_=false;cellEditor_->deselect();cellEditor_->setCursorPosition(cellEditor_->text().size());entryContext(*this,"Edit",cellEditor_->text());}
}
void WorksheetView::updateSelectionDimensions(bool held){setProperty("worksheet.selectionExtensionActive",held);selectionName(*this,held);}
void WorksheetView::finishSelectionExtension(){setProperty("worksheet.columnShortcutHeld",false);setProperty("worksheet.selectionExtensionActive",false);if(!cellEditor_&&!(formulaBar_&&formulaBar_->isModified())&&!property("worksheet.externalPointing").toBool())selectionName(*this,false);}
void WorksheetView::keyReleaseEvent(QKeyEvent* event){QTableView::keyReleaseEvent(event);if(event->key()==Qt::Key_Shift||(event->key()==Qt::Key_Control&&property("worksheet.columnShortcutHeld").toBool()))finishSelectionExtension();}
QModelIndex WorksheetView::navigationIndex(bool extend)const{
 const auto current=currentIndex();if(!extend||!current.isValid()||!selectionModel())return current;const auto* worksheet=dynamic_cast<const WorksheetModel*>(model());const auto anchor=property("worksheet.keyboardAnchor").value<QModelIndex>(),extent=property("worksheet.keyboardExtent").value<QModelIndex>();const auto ranges=selectionModel()->selection();
 if(anchor==current&&extent.isValid()&&ranges.size()==1&&ranges.front().contains(extent)&&worksheet&&property("worksheet.keyboardSheet").toString()==worksheet->sheetId())return extent;
 if(ranges.size()==1){const auto range=ranges.front();return model()->index(current.row()==range.bottom()?range.top():range.bottom(),current.column()==range.right()?range.left():range.right());}return current;
}
void WorksheetView::navigateTo(const QModelIndex& target,bool extend){
 if(!target.isValid()||target.model()!=model()||!selectionModel())return;const auto anchor=extend&&currentIndex().isValid()?currentIndex():target;
 setProperty("worksheet.selectionExtensionActive",extend);
 selectionModel()->setCurrentIndex(anchor,QItemSelectionModel::NoUpdate);selectionModel()->select(QItemSelection(anchor,target),QItemSelectionModel::ClearAndSelect);setProperty("worksheet.keyboardAnchor",extend?QVariant::fromValue(anchor):QVariant());setProperty("worksheet.keyboardExtent",extend?QVariant::fromValue(target):QVariant());if(const auto* worksheet=dynamic_cast<const WorksheetModel*>(model()))setProperty("worksheet.keyboardSheet",worksheet->sheetId());scrollTo(target);selectionName(*this,extend);
}
bool WorksheetView::cyclePointedReference(QLineEdit* editor){
 if(!pointMode_||referenceEditor_!=editor||referenceStart_<0||editor->cursorPosition()!=referenceStart_+referenceLength_)return false;
 const bool selected=editor->hasSelectedText();if(selected&&(editor->selectionStart()!=referenceStart_||editor->selectedText().size()!=referenceLength_))return false;
 const auto before=editor->text();const auto cursor=editor->cursorPosition();const auto after=QString::fromStdString(cycleReference(before.toStdString(),before.left(cursor).toUtf8().size()));if(after==before)return false;
 insertingReference_=true;editor->selectAll();editor->insert(after);referenceLength_+=after.size()-before.size();if(selected)editor->setSelection(referenceStart_,referenceLength_);else editor->setCursorPosition(cursor+after.size()-before.size());editor->setModified(true);insertingReference_=false;entryContext(*this,"Point",after);return true;
}
bool WorksheetView::handleEditorKey(QLineEdit* editor,QKeyEvent* key){
 if(beforeEditorKey&&beforeEditorKey(editor,key))return true;
 if(key->key()==Qt::Key_Escape){cancelEntry();return true;}
 if(key->key()==Qt::Key_F2&&editor->text().startsWith('=')){pointMode_=!pointMode_;entryContext(*this,pointMode_?"Enter":"Edit",editor->text());key->accept();return true;}
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
  const auto index=hasReferenceSheetEntry()?QModelIndex(entryOriginIndex_):currentIndex();const bool tab=key->key()==Qt::Key_Tab||key->key()==Qt::Key_Backtab;const bool retain=!tab&&(key->modifiers()&Qt::ControlModifier);const int direction=retain?0:(key->modifiers()&Qt::ShiftModifier)||key->key()==Qt::Key_Backtab?-1:1;
  if(commitPendingEditor(true)){const auto next=model()->index(std::clamp(index.row()+(tab?0:direction),0,model()->rowCount()-1),std::clamp(index.column()+(tab?direction:0),0,model()->columnCount()-1));setCurrentIndex(next);scrollTo(next);setFocus();}return true;
 }return false;
}
QLineEdit* WorksheetView::activeCellEditor()const{return cellEditor_;}
QLineEdit* WorksheetView::createCellEditor(QWidget* parent,const QModelIndex& index){
 pointMode_=true;referenceStart_=-1;referenceLength_=0;referenceCursor_={};
 auto* editor=editorFactory?editorFactory(parent):new QLineEdit(parent);cellEditor_=editor;editIndex_=index;editor->setObjectName("worksheet.cellEditor");editor->setAccessibleName(QString("Cell %1%2").arg(QString::fromStdString(columnName(index.column()+1))).arg(index.row()+1));editor->setFont(index.data(Qt::FontRole).value<QFont>());
 // The delegate already supplies the cell's padded content rectangle. Toolbar
 // line-edit minimums and chrome would make a compact-row editor protrude.
 editor->setFrame(false);editor->setStyleSheet("QLineEdit { min-height: 0px; min-width: 0px; padding: 0px; border: 0px; border-radius: 0px; }");
 connect(editor,&QLineEdit::textChanged,this,[this](const QString& text){if(formulaBar_){formulaBar_->setText(text);formulaBar_->setModified(false);}});
 connect(editor,&QLineEdit::textChanged,this,[this]{if(!insertingReference_){referenceStart_=-1;referenceLength_=0;}viewport()->update();});
 connect(editor,&QLineEdit::textChanged,this,[this,editor]{expandFormulaEditor(*editor,*this);});
 connect(editor,&QLineEdit::textEdited,this,[this](const QString& text){entryContext(*this,pointMode_?"Enter":"Edit",text);});entryContext(*this,"Edit");
 return editor;
}
bool WorksheetView::setProposedReferenceRange(const QModelIndex& first,const QModelIndex& last){
 auto* editor=cellEditor_.data();if(!editor||!editor->text().startsWith('=')||!editor->hasSelectedText()||!first.isValid()||!last.isValid()||first.model()!=model()||last.model()!=model())return false;
 pointMode_=true;referenceEditor_=editor;referenceAnchor_=first;referenceCursor_=last;referenceStart_=editor->selectionStart();referenceLength_=editor->selectedText().size();
 entryContext(*this,"Point",editor->text());viewport()->update();return true;
}
QLineEdit* WorksheetView::referenceCandidate()const{return cellEditor_?cellEditor_.data():(formulaBar_&&(formulaBar_->hasFocus()||formulaBar_->isModified())?formulaBar_.data():nullptr);}
bool WorksheetView::canPoint(QLineEdit* editor)const{
 if(!editor||editor->isReadOnly()||!editor->text().startsWith('='))return false;const auto prefix=editor->text().left(editor->cursorPosition());bool quoted=false;for(int i=0;i<prefix.size();++i)if(prefix[i]=='"'){if(quoted&&i+1<prefix.size()&&prefix[i+1]=='"'){++i;continue;}quoted=!quoted;}if(quoted)return false;
 if(referenceEditor_==editor&&referenceStart_>=0&&editor->cursorPosition()==referenceStart_+referenceLength_)return true;
 const auto trimmed=prefix.trimmed();return (!trimmed.isEmpty()&&QString("=+-*/^&(<>,:;").contains(trimmed.back()))||(!editor->hasSelectedText()&&partialColumnStart(*editor,editor->cursorPosition())>=0);
}
void WorksheetView::insertPointedRange(const QModelIndex& end){
 if(!referenceEditor_||!end.isValid()||!referenceAnchor_.isValid())return;const int row1=std::min(referenceAnchor_.row(),end.row()),row2=std::max(referenceAnchor_.row(),end.row()),col1=std::min(referenceAnchor_.column(),end.column()),col2=std::max(referenceAnchor_.column(),end.column());
 if(referenceLength_==0)if(const int start=partialColumnStart(*referenceEditor_,referenceStart_);start>=0){referenceLength_=referenceStart_-start;referenceStart_=start;}
 auto address=[](int row,int col){return QString::fromStdString(columnName(col+1))+QString::number(row+1);};auto text=address(row1,col1);bool customReference=false;if(row1!=row2||col1!=col2)text+=":"+address(row2,col2);else if(pointedReference){auto custom=pointedReference(referenceAnchor_,end);if(custom.startsWith('='))custom.remove(0,1);if(!custom.isEmpty()){text=std::move(custom);customReference=true;}}
 if(hasReferenceSheetEntry()&&!customReference)if(const auto* worksheet=dynamic_cast<const WorksheetModel*>(model());worksheet&&worksheet->sheetId()!=entryOriginSheet_)for(const auto& sheet:worksheet->workbookView().at("sheets"))if(sheet.at("id")==worksheet->sheetId().toStdString()){
  auto name=QString::fromStdString(sheet.at("name").get<std::string>());bool simple=!name.isEmpty()&&(name.front().isLetter()||name.front()=='_');for(const auto ch:name)if(!ch.isLetterOrNumber()&&ch!='_'&&ch!='.')simple=false;
  if(!simple){name.replace("'","''");name="'"+name+"'";}text=name+"!"+text;break;
 }
 insertingReference_=true;referenceEditor_->setSelection(referenceStart_,referenceLength_);referenceEditor_->insert(text);referenceEditor_->setModified(true);referenceLength_=text.size();referenceCursor_=end;insertingReference_=false;entryContext(*this,pointMode_?"Point":"Edit",referenceEditor_->text(),end,row1!=row2||col1!=col2);viewport()->update();
}
void WorksheetView::mousePressEvent(QMouseEvent* event){
 auto* editor=referenceCandidate();const auto index=indexAt(event->pos());if(event->button()==Qt::LeftButton&&index.isValid()&&canPoint(editor)){
  if(referenceEditor_!=editor||referenceStart_<0||editor->cursorPosition()!=referenceStart_+referenceLength_){referenceStart_=editor->hasSelectedText()?editor->selectionStart():editor->cursorPosition();referenceLength_=editor->selectedText().size();}
  referenceEditor_=editor;referenceAnchor_=index;picking_=true;insertPointedRange(index);event->accept();return;
 }
 if(event->button()==Qt::LeftButton&&fillHandleRect().adjusted(-2,-2,2,2).contains(event->pos())&&!fillHandleRect().isEmpty()){
  if(commitPendingEditor()){fillSource_=currentIndex();fillTarget_=fillSource_;filling_=true;fillPosition_=event->pos();setProperty("worksheet.fillDragged",false);setFocus(Qt::MouseFocusReason);viewport()->update();}event->accept();return;
 }
 if(event->button()==Qt::LeftButton&&index.isValid()){setProperty("worksheet.selectionExtensionActive",true);setProperty("worksheet.dragAnchor",QVariant::fromValue(event->modifiers()&Qt::ShiftModifier?currentIndex():index));}
 setFocus(Qt::MouseFocusReason);QTableView::mousePressEvent(event);
 if(event->button()==Qt::LeftButton&&event->modifiers()==Qt::NoModifier&&index.isValid())selectionName(*this,false);
}
QRect WorksheetView::fillHandleRect()const{
 const auto index=currentIndex();if(!dynamic_cast<WorksheetModel*>(model())||!index.isValid()||cellEditor_||!(model()->flags(index)&Qt::ItemIsEditable))return {};
 if(!selectionModel()||!selectionModel()->isSelected(index))return {};const auto selected=selectionRectangles(selectionModel()->selection());if(selected.size()!=1)return {};const auto& range=selected.front();if(range.width()==model()->columnCount()||range.height()==model()->rowCount())return {};
 const auto rect=cellBoundaryRect(*this,range.bottomRight());if(rect.isEmpty())return {};return QRect(rect.bottomRight()-QPoint(1,1),QSize(5,5));
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
int WorksheetView::autoFitColumnPixels(int column) const{
 if(!dynamic_cast<const WorksheetModel*>(model())||column<0||column>=model()->columnCount())return columnWidth(column);
 const auto widths=autoFitColumnPixels(QItemSelection(model()->index(0,column),model()->index(model()->rowCount()-1,column)));
 const auto found=widths.find(column);return found==widths.end()?horizontalHeader()->defaultSectionSize():found->second;
}
std::map<int,int> WorksheetView::autoFitColumnPixels(const QItemSelection& ranges) const{
 const auto* worksheet=dynamic_cast<const WorksheetModel*>(model());if(!worksheet)return {};
 using Interval=std::pair<int,int>;
 std::map<int,std::vector<Interval>> rowsByColumn;
 for(const auto& range:ranges){
  if(!range.isValid()||range.model()!=model())continue;
  for(int column=range.left();column<=range.right();++column)rowsByColumn[column].emplace_back(range.top(),range.bottom());
 }
 if(rowsByColumn.empty())return {};
 for(auto& [column,rows]:rowsByColumn){
  std::sort(rows.begin(),rows.end());std::size_t count=0;
  for(const auto& interval:rows){
   if(count&&interval.first<=rows[count-1].second+1)rows[count-1].second=std::max(rows[count-1].second,interval.second);
   else rows[count++]=interval;
  }
  rows.resize(count);
 }
 auto selected=[&](int row,int column){
  const auto found=rowsByColumn.find(column);if(found==rowsByColumn.end())return false;
  const auto& rows=found->second;
  const auto after=std::upper_bound(rows.begin(),rows.end(),row,[](int value,const Interval& interval){return value<interval.first;});
  return after!=rows.begin()&&row<=std::prev(after)->second;
 };
 std::map<int,int> widths;
 for(const auto& sheet:worksheet->workbookView().at("sheets"))if(sheet.at("id")==worksheet->sheetId().toStdString()){
  const auto merges=sheet.find("mergedRanges");
  auto measure=[&](int row,int column){
   if(!selected(row,column))return;
   if(merges!=sheet.end()&&std::any_of(merges->begin(),merges->end(),[&](const Json& range){return row+1>=range.at("row1")&&row+1<=range.at("row2")&&column+1>=range.at("col1")&&column+1<=range.at("col2");}))return;
   const auto index=worksheet->index(row,column);const auto text=worksheet->data(index,Qt::DisplayRole).toString();if(text.isEmpty())return;
   const auto style=worksheet->cellStyle(row,column);
   // Use the delegate's presentation font, including platform substitutions.
   const QFontMetricsF metrics(worksheet->data(index,Qt::FontRole).value<QFont>());
   qreal content=0;for(const auto& line:text.split('\n'))content=std::max(content,metrics.horizontalAdvance(line));
   const int margin=worksheet->numericValue(row,column)?6:13;
   const int width=qCeil(content+style.value("indent",0)*metrics.horizontalAdvance("   "))+margin;
   widths[column]=std::max(widths[column],std::clamp(width,1,spreadsheetColumnPixels(255.0,SpreadsheetNormalDigitWidth)));
  };
  // Scan sparse stored inputs once for the entire selection, never once per column.
  for(const auto& cell:sheet.at("cells"))if(cell.at("input")!="")measure(cell.at("row").get<int>()-1,cell.at("col").get<int>()-1);
  // Current spill destinations can have no stored input or style record.
  const auto calculated=worksheet->calculatedSnapshot();
  if(calculated.value("status",std::string())=="ok")for(const auto& cell:calculated.at("values")){
   const int row=cell.at("row").get<int>()-1,column=cell.at("col").get<int>()-1;
   if(selected(row,column)&&worksheet->raw(row,column).isEmpty())measure(row,column);
  }
  break;
 }
 return widths;
}
void WorksheetView::persistAutoFitColumn(int column){
 auto* worksheet=dynamic_cast<WorksheetModel*>(model());if(!worksheet||column<0||column>=model()->columnCount()||!(model()->flags(model()->index(0,column))&Qt::ItemIsEditable))return;
 const auto selected=selectionModel()->selection();QItemSelection fitting;
 const auto fullColumn=[this](const QItemSelectionRange& range){return range.top()==0&&range.bottom()==model()->rowCount()-1;};
 const bool clickedSelected=std::any_of(selected.begin(),selected.end(),[&](const QItemSelectionRange& range){return fullColumn(range)&&column>=range.left()&&column<=range.right();});
 if(clickedSelected){for(const auto& range:selected)if(fullColumn(range))fitting.push_back(range);}
 else fitting.select(model()->index(0,column),model()->index(model()->rowCount()-1,column));
 const auto widths=autoFitColumnPixels(fitting);const int digitWidth=normalDigitWidth(*this);
 for(const auto& sheet:worksheet->workbookView().at("sheets"))if(sheet.at("id")==worksheet->sheetId().toStdString()){
  std::map<int,Json> columns;for(const auto& item:sheet.value("columnLayout",Json::array()))columns[item.at("index").get<int>()]=item;
  for(const auto& hidden:sheet.value("hiddenColumns",Json::array())){const int index=hidden.get<int>();if(!columns.contains(index))columns[index]={{"index",index}};columns[index]["hidden"]=true;}
  const auto original=columns;
  for(const auto& range:fitting)for(int at=range.left();at<=range.right();++at){
   const auto measured=widths.find(at);if(measured==widths.end()&&!columns.contains(at+1))continue;
   const int pixels=measured==widths.end()?horizontalHeader()->defaultSectionSize():measured->second;
   const double characters=std::clamp((pixels-5.0)/digitWidth,0.0,spreadsheetColumnCharacters(255.0,digitWidth));
   const double width=spreadsheetColumnWidth(characters,digitWidth);if(width<=0)continue;
   auto& entry=columns[at+1];if(entry.is_null())entry={{"index",at+1}};entry["size"]=width;entry["bestFit"]=true;
  }
  if(columns==original)return;Json layout=Json::array();for(const auto& [index,item]:columns)layout.push_back(item);
  worksheet->apply(Json::array({{{"type","set_sheet_layout"},{"sheet",worksheet->sheetId().toStdString()},{"columns",std::move(layout)}}}));return;
 }
}
void WorksheetView::mouseMoveEvent(QMouseEvent* event){
 if(picking_){insertPointedRange(indexAt(event->pos()));event->accept();return;}
 if(filling_){if(!property("worksheet.fillDragged").toBool()&&(event->pos()-fillPosition_).manhattanLength()>=QApplication::startDragDistance())setProperty("worksheet.fillDragged",true);if(property("worksheet.fillDragged").toBool())updateFillTarget(event->pos());viewport()->update();event->accept();return;}
 if(!fillHandleRect().isEmpty()&&fillHandleRect().adjusted(-2,-2,2,2).contains(event->pos()))viewport()->setCursor(Qt::CrossCursor);else viewport()->unsetCursor();QTableView::mouseMoveEvent(event);const auto anchor=property("worksheet.dragAnchor").value<QModelIndex>();if(anchor.isValid()){retainDragAnchor(*this,anchor);selectionName(*this,true);}
}
void WorksheetView::mouseReleaseEvent(QMouseEvent* event){
 if(picking_&&event->button()==Qt::LeftButton){insertPointedRange(indexAt(event->pos()));picking_=false;event->accept();return;}
 if(filling_&&event->button()==Qt::LeftButton){const bool dragged=property("worksheet.fillDragged").toBool();if(dragged)updateFillTarget(event->pos());filling_=false;if(dragged)finishFill();cancelFill();event->accept();return;}
 QTableView::mouseReleaseEvent(event);
 if(event->button()==Qt::LeftButton){const auto anchor=property("worksheet.dragAnchor").value<QModelIndex>();setProperty("worksheet.dragAnchor",QVariant());if(anchor.isValid()){retainDragAnchor(*this,anchor);selectionName(*this,false);}setProperty("worksheet.selectionExtensionActive",false);}
}

}
