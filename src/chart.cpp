#include "tigersheets/chart.hpp"
#include "tigersheets/formatting.hpp"
#include "tigersheets/pivot.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPointer>
#include <QContextMenuEvent>
#include <cmath>
#include <algorithm>
#include <map>
namespace tigersheets {
Json chartData(const Json& book,const Json& calculated,const Json& spec){
 if(spec.contains("pivotId"))return pivotChartData(book,spec);
 if(book.contains("revision")&&calculated.contains("revision")&&book.at("revision")!=calculated.at("revision"))throw std::runtime_error("Chart values are stale; calculate the workbook before rendering.");
 const auto& range=spec.at("range");const int r1=range.at("row1"),r2=range.at("row2"),c1=range.at("col1"),c2=range.at("col2");
 if(r2-r1>10000||c2-c1>100||r1<1||c1<1||r2<r1||c2<c1||r2>1048576||c2>16384)throw std::runtime_error("Chart range is invalid or too large");
 std::map<std::pair<int,int>,Json> values;std::map<std::pair<int,int>,const Json*> cells;const auto sheet=spec.at("sheet").get<std::string>();
 for(const auto& entry:book.at("sheets"))if(entry.at("id")==sheet)for(const auto& cell:entry.at("cells"))if(cell.at("row")>=r1&&cell.at("row")<=r2&&cell.at("col")>=c1&&cell.at("col")<=c2)cells[{cell.at("row"),cell.at("col")}]=&cell;
 if(calculated.contains("values"))for(const auto& cell:calculated.at("values"))if(cell.at("sheet")==sheet&&cell.at("row")>=r1&&cell.at("row")<=r2&&cell.at("col")>=c1&&cell.at("col")<=c2)values[{cell.at("row"),cell.at("col")}]=cell.at("value");
 const auto settings=book.value("settings",Json::object());const bool date1904=settings.is_object()&&settings.contains("date1904")&&settings["date1904"].is_boolean()&&settings["date1904"].get<bool>();
 auto value=[&](int row,int col)->Json{const auto key=std::pair{row,col};if(auto found=values.find(key);found!=values.end())return found->second;auto found=cells.find(key);if(found==cells.end())return nullptr;const auto input=found->second->value("input",std::string());if(input.empty())return nullptr;if(input[0]=='=')throw std::runtime_error("Calculate chart source formulas before rendering.");if(input[0]=='\'')return input.substr(1);bool okay=false;const auto text=QString::fromStdString(input);const double number=text.toDouble(&okay);if(okay&&std::isfinite(number))return number;if(text.compare("TRUE",Qt::CaseInsensitive)==0)return true;if(text.compare("FALSE",Qt::CaseInsensitive)==0)return false;return input;};
 auto label=[&](int row,int col,const Json& entry){const auto cell=cells.find({row,col});return formattedValue(entry,cell==cells.end()?Json::object():cell->second->value("style",Json::object()),date1904);};
 Json categories=Json::array(),x=Json::array(),series=Json::array();for(int row=r1+1;row<=r2;++row){const auto entry=value(row,c1);categories.push_back(label(row,c1,entry));x.push_back(entry.is_number()?entry:Json());}
 for(int col=c1+1;col<=c2;++col){Json y=Json::array();std::string format="General";for(int row=r1+1;row<=r2;++row){const auto entry=value(row,col);y.push_back(entry.is_number()?entry:Json());if(format=="General")if(const auto found=cells.find({row,col});found!=cells.end())format=found->second->value("style",Json::object()).value("numberFormat",std::string("General"));}series.push_back({{"name",label(r1,col,value(r1,col))},{"values",std::move(y)},{"numberFormat",format}});}
 return {{"categories",std::move(categories)},{"xValues",std::move(x)},{"series",std::move(series)},{"date1904",date1904}};
}

ChartWidget::ChartWidget(QWidget*p):QWidget(p){setMinimumSize(320,230);setAutoFillBackground(true);setFocusPolicy(Qt::StrongFocus);setMouseTracking(true);}
void ChartWidget::setChart(Json spec,Json data){specification_=std::move(spec);data_=std::move(data);update();}
void ChartWidget::setSelected(bool selected){
 if(selected_==selected)return;
 selected_=selected;update();
 auto callback=selectionChanged;if(callback)callback(selected);
}
int ChartWidget::cornerAt(const QPoint& point)const{
 const bool left=point.x()<12,right=point.x()>=width()-12,top=point.y()<12,bottom=point.y()>=height()-12;
 return (left||right)&&(top||bottom)?(left?1:2)|(top?4:8):0;
}
void ChartWidget::mousePressEvent(QMouseEvent* event){
 if(event->button()==Qt::LeftButton){
  event->accept();resizeCorner_=selected_?cornerAt(event->position().toPoint()):0;dragging_=true;pressGlobal_=event->globalPosition().toPoint();gestureStart_=geometry();setFocus(Qt::MouseFocusReason);setSelected(true);return;
 }
 QWidget::mousePressEvent(event);
}
void ChartWidget::mouseDoubleClickEvent(QMouseEvent* event){
 if(event->button()==Qt::LeftButton){
  event->accept();dragging_=false;QPointer<ChartWidget> guard(this);setSelected(true);if(!guard)return;
  auto callback=editRequested;if(callback)callback(ChartEditAction::General);return;
 }
 QWidget::mouseDoubleClickEvent(event);
}
void ChartWidget::contextMenuEvent(QContextMenuEvent* event){
 event->accept();QPointer<ChartWidget> guard(this);setSelected(true);if(!guard)return;
 auto* menu=new QMenu(this);menu->setObjectName("chartContextMenu");menu->setAttribute(Qt::WA_DeleteOnClose);
 auto edit=[&](const QString& label,const QString& name,ChartEditAction editAction){
  auto* action=menu->addAction(label);action->setObjectName(name);
  connect(action,&QAction::triggered,menu,[guard,editAction]{if(!guard)return;auto callback=guard->editRequested;if(callback)callback(editAction);});
 };
 edit("Edit chart…","chart.edit",ChartEditAction::General);
 edit("Select data…","chart.selectData",ChartEditAction::Data);
 edit("Change chart type…","chart.changeType",ChartEditAction::Type);
 menu->addSeparator();auto* remove=menu->addAction("Delete chart");remove->setObjectName("chart.delete");
 connect(remove,&QAction::triggered,menu,[guard]{if(!guard)return;auto callback=guard->deleteRequested;if(callback)callback();});
 menu->popup(event->globalPos());
}
void ChartWidget::updateGesture(const QPoint& globalPosition){
  const auto delta=globalPosition-pressGlobal_;
  auto moved=gestureStart_;
  if(resizeCorner_){
   if(resizeCorner_&1)moved.setLeft(std::max(0,std::clamp(gestureStart_.left()+delta.x(),gestureStart_.right()-8191,gestureStart_.right()-minimumWidth()+1)));
   if(resizeCorner_&2)moved.setRight(std::clamp(gestureStart_.right()+delta.x(),gestureStart_.left()+minimumWidth()-1,gestureStart_.left()+8191));
   if(resizeCorner_&4)moved.setTop(std::max(0,std::clamp(gestureStart_.top()+delta.y(),gestureStart_.bottom()-8191,gestureStart_.bottom()-minimumHeight()+1)));
   if(resizeCorner_&8)moved.setBottom(std::clamp(gestureStart_.bottom()+delta.y(),gestureStart_.top()+minimumHeight()-1,gestureStart_.top()+8191));
  }else{moved.translate(delta);moved.moveLeft(std::max(0,moved.left()));moved.moveTop(std::max(0,moved.top()));}
  setGeometry(moved);
}
void ChartWidget::mouseMoveEvent(QMouseEvent* event){
 if(dragging_){updateGesture(event->globalPosition().toPoint());event->accept();return;}
 const auto corner=selected_?cornerAt(event->position().toPoint()):0;
 setCursor(corner?((corner==5||corner==10)?Qt::SizeFDiagCursor:Qt::SizeBDiagCursor):Qt::OpenHandCursor);
 QWidget::mouseMoveEvent(event);
}
void ChartWidget::mouseReleaseEvent(QMouseEvent* event){
 if(event->button()==Qt::LeftButton&&dragging_){
  updateGesture(event->globalPosition().toPoint());dragging_=false;event->accept();const auto before=gestureStart_,after=geometry();
  auto callback=geometryCommitted;if(callback&&before!=after)callback(before,after);return;
 }
 QWidget::mouseReleaseEvent(event);
}
bool ChartWidget::event(QEvent* event){
 if(event->type()==QEvent::ShortcutOverride&&selected_&&isEnabled()){
  const auto* key=static_cast<QKeyEvent*>(event);
  if((key->modifiers()&~Qt::KeypadModifier)==Qt::NoModifier&&(key->key()==Qt::Key_Delete||key->key()==Qt::Key_Backspace||key->key()==Qt::Key_F2||key->key()==Qt::Key_Escape)){
   event->accept();return true;
  }
 }
 return QWidget::event(event);
}
void ChartWidget::keyPressEvent(QKeyEvent* event){
 if(selected_&&(event->modifiers()&~Qt::KeypadModifier)==Qt::NoModifier){
  if(event->key()==Qt::Key_Delete||event->key()==Qt::Key_Backspace){event->accept();auto callback=deleteRequested;if(callback)callback();return;}
  if(event->key()==Qt::Key_F2){event->accept();auto callback=editRequested;if(callback)callback(ChartEditAction::General);return;}
 }
 if(event->key()==Qt::Key_Escape){
  event->accept();if(dragging_){dragging_=false;setGeometry(gestureStart_);return;}
  setSelected(false);return;
 }
 QWidget::keyPressEvent(event);
}
void renderChart(QPainter& p,const QRectF& bounds,const Json& specification,const Json& data,const QFont& defaultFont){
 p.save();p.setClipRect(bounds,Qt::IntersectClip);p.translate(bounds.topLeft());const int width=static_cast<int>(std::lround(bounds.width())),height=static_cast<int>(std::lround(bounds.height()));
 const auto draw=[&]{
 p.setRenderHint(QPainter::Antialiasing);p.fillRect(QRect(0,0,width,height),Qt::white);p.setPen(QColor("#dce2da"));p.drawRoundedRect(QRect(0,0,width,height).adjusted(1,1,-1,-1),8,8);
 p.setPen(QColor("#202c33"));QFont title=defaultFont;title.setPixelSize(16);title.setBold(true);p.setFont(title);p.drawText(QRect(20,14,width-40,28),Qt::AlignLeft|Qt::AlignVCenter,QString::fromStdString(specification.value("title",std::string("Chart"))));
 if(!data.contains("series")||data["series"].empty())return;
 const QColor colors[]={QColor("#245648"),QColor("#df7952"),QColor("#8c58a0"),QColor("#598daa"),QColor("#bf9c42"),QColor("#a65c70")};
 auto series=data["series"];auto categories=data.value("categories",Json::array());int count=static_cast<int>(categories.size());if(count==0)return;
 QString type=QString::fromStdString(specification.value("type",std::string("line")));
 QFont normal=defaultFont;normal.setPixelSize(12);normal.setBold(false);p.setFont(normal);
 auto xValue=[&](int index)->std::optional<double>{const auto& xs=data.contains("xValues")?data.at("xValues"):categories;if(index<0||static_cast<std::size_t>(index)>=xs.size()||!xs[index].is_number())return std::nullopt;const double value=xs[index].get<double>();return std::isfinite(value)?std::optional<double>(value):std::nullopt;};
 double xLow=0,xHigh=1;bool hasX=false;if(type=="scatter"){for(int i=0;i<count;++i)if(const auto value=xValue(i)){if(!hasX)xLow=xHigh=*value;else{xLow=std::min(xLow,*value);xHigh=std::max(xHigh,*value);}hasX=true;}if(xLow==xHigh){xLow-=.5;xHigh+=.5;}const auto pad=(xHigh-xLow)*.05;xLow-=pad;xHigh+=pad;}

 double low=0,high=0;for(const auto& s:series)for(const auto& v:s["values"])if(v.is_number()){low=std::min(low,v.get<double>());high=std::max(high,v.get<double>());}if(high==low)high=low+1;const double padding=(high-low)*.08;high+=padding;if(type!="bar"&&low<0)low-=padding;
 std::string axisFormat=specification.value("valueAxisFormat",std::string());if(axisFormat.empty()){for(const auto& item:series){const auto candidate=item.value("numberFormat",std::string("General"));if(axisFormat.empty())axisFormat=candidate;else if(axisFormat!=candidate){axisFormat="General";break;}}}
 auto axisLabel=[&](double value){return axisFormat.empty()||axisFormat=="General"?QString::number(value,'g',4):QString::fromStdString(formattedValue(value,Json{{"numberFormat",axisFormat}},data.value("date1904",false)));};
 qreal axisWidth=52;if(type!="pie"&&type!="doughnut"&&type!="bar")for(int tick=0;tick<=4;++tick)axisWidth=std::max(axisWidth,static_cast<qreal>(p.fontMetrics().horizontalAdvance(axisLabel(low+(high-low)*tick/4))));
 const qreal left=axisWidth+10;QRectF plot(left,58,width-left-26,height-115);if(plot.width()<20||plot.height()<20)return;
 if(type=="pie"||type=="doughnut"){
  double total=0;for(const auto& v:series[0]["values"])if(v.is_number())total+=std::max(0.0,v.get<double>());if(total<=0)return;
  QRectF circle(plot.center().x()-plot.height()/2,plot.top(),plot.height(),plot.height());double angle=90;
  for(size_t i=0;i<series[0]["values"].size();++i){auto v=series[0]["values"][i];if(!v.is_number())continue;double span=std::max(0.0,v.get<double>())/total*360;p.setBrush(colors[i%6]);p.setPen(QPen(Qt::white,2));p.drawPie(circle,static_cast<int>(angle*16),static_cast<int>(span*16));angle+=span;}
  if(type=="doughnut"){p.setBrush(Qt::white);p.setPen(Qt::NoPen);p.drawEllipse(circle.adjusted(circle.width()*.28,circle.height()*.28,-circle.width()*.28,-circle.height()*.28));}
 }else if(type=="bar"){
  auto x=[&](double value){return plot.left()+(value-low)/(high-low)*plot.width();};
  for(int tick=0;tick<=4;++tick){double v=low+(high-low)*tick/4;auto xx=x(v);p.setPen(QColor("#e9ede7"));p.drawLine(QPointF(xx,plot.top()),QPointF(xx,plot.bottom()));p.setPen(QColor("#65756d"));p.drawText(QRectF(xx-35,plot.bottom()+8,70,18),Qt::AlignHCenter,axisLabel(v));}
  for(int i=0;i<count;++i){auto yy=plot.top()+(i+.5)/count*plot.height();p.drawText(QRectF(3,yy-10,53,20),Qt::AlignRight|Qt::AlignVCenter,QString::fromStdString(categories[i].is_string()?categories[i].get<std::string>():categories[i].dump()));}
  int si=0;for(const auto& s:series){int vi=0;for(const auto& v:s["values"]){if(v.is_number()){double h=plot.height()/count*.75/static_cast<int>(series.size());double yy=plot.top()+(vi+.125)/count*plot.height()+si*h;auto end=x(v.get<double>());p.setPen(Qt::NoPen);p.setBrush(colors[si%6]);p.drawRect(QRectF(std::min(x(0),end),yy,std::max(1.0,std::abs(end-x(0))),std::max(1.0,h-2)));}++vi;}++si;}
 }else{
  auto y=[&](double value){return plot.bottom()-(value-low)/(high-low)*plot.height();};
  for(int tick=0;tick<=4;++tick){double v=low+(high-low)*tick/4;double yy=y(v);p.setPen(QPen(QColor("#e9ede7"),1));p.drawLine(QPointF(plot.left(),yy),QPointF(plot.right(),yy));p.setPen(QColor("#65756d"));p.drawText(QRectF(4,yy-9,axisWidth,18),Qt::AlignRight|Qt::AlignVCenter,axisLabel(v));}
  int si=0;for(const auto& s:series){QPainterPath path;bool started=false;int vi=0;for(const auto& v:s["values"]){const auto numericX=type=="scatter"?xValue(vi):std::optional<double>(0);if(!v.is_number()||!numericX){started=false;++vi;continue;}double x=type=="scatter"?plot.left()+(*numericX-xLow)/(xHigh-xLow)*plot.width():plot.left()+(vi+.5)/count*plot.width();double yy=y(v.get<double>());p.setPen(QPen(colors[si%6],2.3));p.setBrush(colors[si%6]);
    if(type=="column"||type=="bar"){double w=plot.width()/count*.75/static_cast<int>(series.size());double xx=plot.left()+(vi+.125)/count*plot.width()+si*w;p.drawRect(QRectF(xx,std::min(yy,y(0)),std::max(1.0,w-2),std::max(1.0,std::abs(yy-y(0)))));}
    else if(type=="scatter")p.drawEllipse(QPointF(x,yy),3.5,3.5);
    else {if(!started){path.moveTo(x,yy);started=true;}else path.lineTo(x,yy);p.drawEllipse(QPointF(x,yy),2.5,2.5);}
    ++vi;}
   if(type!="column"&&type!="bar"&&type!="scatter"){if(type=="area"){auto fill=path;fill.lineTo(plot.left()+(count-.5)/count*plot.width(),y(0));fill.lineTo(plot.left()+.5/count*plot.width(),y(0));fill.closeSubpath();auto color=colors[si%6];color.setAlpha(35);p.fillPath(fill,color);}p.setBrush(Qt::NoBrush);p.setPen(QPen(colors[si%6],2.3));p.drawPath(path);}++si;
  }
  p.setPen(QColor("#65756d"));if(type=="scatter"){for(int i=0;i<=4;++i){const double value=xLow+(xHigh-xLow)*i/4;const double x=plot.left()+plot.width()*i/4;p.drawText(QRectF(std::clamp(x-35,0.0,double(width-70)),plot.bottom()+8,70,18),Qt::AlignHCenter,QString::number(value,'g',4));}}
  else{int stride=std::max(1,count/8);for(int i=0;i<count;i+=stride){double x=plot.left()+(i+.5)/count*plot.width();p.drawText(QRectF(x-45,plot.bottom()+8,90,18),Qt::AlignHCenter,QString::fromStdString(categories[i].is_string()?categories[i].get<std::string>():categories[i].dump()));}}
 }
 int x=62;for(size_t i=0;i<series.size();++i){p.setPen(Qt::NoPen);p.setBrush(colors[i%6]);p.drawRoundedRect(QRect(x,height-26,9,9),2,2);p.setPen(QColor("#65756d"));auto name=QString::fromStdString(series[i].value("name",std::string("Series")));p.drawText(x+15,height-17,name);x+=p.fontMetrics().horizontalAdvance(name)+40;}
 };draw();p.restore();
}
void ChartWidget::paintEvent(QPaintEvent*){
 QPainter p(this);renderChart(p,QRectF(rect()),specification_,data_,font());p.setRenderHint(QPainter::Antialiasing);
 if(selected_){p.setPen(QPen(QColor("#245648"),2));p.setBrush(Qt::NoBrush);p.drawRect(rect().adjusted(3,3,-4,-4));p.setBrush(Qt::white);for(const auto& corner:{QPoint(4,4),QPoint(width()-5,4),QPoint(4,height()-5),QPoint(width()-5,height()-5)})p.drawRect(QRect(corner-QPoint(3,3),QSize(7,7)));}
}
}
