#include "tigersheets/chart.hpp"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <QMouseEvent>
#include <QAction>
#include <QPointer>
#include <QMenu>
#include <QContextMenuEvent>
#include <iostream>
int main(int argc,char**argv){QApplication app(argc,argv);
 using tigersheets::Json;
 tigersheets::ChartWidget selectable;
 selectable.setChart(Json::object(),Json::object());
 selectable.show();app.processEvents();
 QTest::mouseClick(&selectable,Qt::LeftButton,Qt::NoModifier,QPoint(30,20));
 if(!selectable.hasFocus()){std::cerr<<"FAILED: clicking a chart selects it for keyboard editing\n";return 1;}

 if(!selectable.isSelected()){std::cerr<<"FAILED: chart click exposes selected state\n";return 1;}
 QTest::keyClick(&selectable,Qt::Key_Escape);
 if(selectable.isSelected()){std::cerr<<"FAILED: Escape clears chart selection\n";return 1;}
 QWidget canvas;canvas.resize(1000,800);canvas.show();
 tigersheets::ChartWidget moving(&canvas);moving.setGeometry(60,70,400,280);moving.show();app.processEvents();
 int commits=0;QRect committedBefore,committedAfter;
 moving.geometryCommitted=[&](QRect before,QRect after){++commits;committedBefore=before;committedAfter=after;};
 QTest::mousePress(&moving,Qt::LeftButton,Qt::NoModifier,QPoint(80,20));
 if(!moving.isInteracting()){std::cerr<<"FAILED: active chart drag exposes interaction state\n";return 1;}
 QMouseEvent drag(QEvent::MouseMove,QPointF(103,37),QPointF(moving.mapToGlobal(QPoint(103,37))),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(&moving,&drag);
 QTest::mouseRelease(&moving,Qt::LeftButton,Qt::NoModifier,QPoint(80,20));
 if(moving.isInteracting()||commits!=1||committedBefore!=QRect(60,70,400,280)||committedAfter!=QRect(83,87,400,280)){
  std::cerr<<"commits="<<commits<<" actual="<<moving.x()<<","<<moving.y()<<" before="<<committedBefore.x()<<","<<committedBefore.y()<<" after="<<committedAfter.x()<<","<<committedAfter.y()<<"\n";
  std::cerr<<"FAILED: chart dragging preserves grab offset and commits one relative move\n";return 1;
 }
 const auto settled=moving.geometry();
 QTest::mousePress(&moving,Qt::LeftButton,Qt::NoModifier,QPoint(80,20));
 QMouseEvent cancelledDrag(QEvent::MouseMove,QPointF(95,29),QPointF(moving.mapToGlobal(QPoint(95,29))),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(&moving,&cancelledDrag);
 QTest::keyClick(&moving,Qt::Key_Escape);
 QTest::mouseRelease(&moving,Qt::LeftButton,Qt::NoModifier,QPoint(80,20));
 if(moving.geometry()!=settled||commits!=1||!moving.isSelected()){
  std::cerr<<"FAILED: Escape cancels a chart gesture without committing or losing selection\n";return 1;
 }
 const auto resizeStart=moving.geometry();
 auto corner=QPoint(moving.width()-4,moving.height()-4);
 QTest::mousePress(&moving,Qt::LeftButton,Qt::NoModifier,corner);
 QMouseEvent resize(QEvent::MouseMove,QPointF(corner+QPoint(35,22)),QPointF(moving.mapToGlobal(corner+QPoint(35,22))),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
 QApplication::sendEvent(&moving,&resize);
 QTest::mouseRelease(&moving,Qt::LeftButton,Qt::NoModifier,corner+QPoint(35,22));
 if(commits!=2||committedAfter!=QRect(resizeStart.topLeft(),resizeStart.size()+QSize(35,22))){
  std::cerr<<"FAILED: selected chart corner resizes with fixed opposite anchor and one commit\n";return 1;
 }
 for(int cornerIndex=0;cornerIndex<4;++cornerIndex){
  moving.setGeometry(200,200,400,280);moving.setSelected(true);const int beforeCount=commits;
  const bool left=cornerIndex%2==0,top=cornerIndex<2;
  const QPoint point(left?4:moving.width()-4,top?4:moving.height()-4),delta(left?-17:17,top?-13:13);
  const auto releaseGlobal=moving.mapToGlobal(point+delta);
  QTest::mousePress(&moving,Qt::LeftButton,Qt::NoModifier,point);
  QMouseEvent diagonal(QEvent::MouseMove,QPointF(point+delta),QPointF(moving.mapToGlobal(point+delta)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&moving,&diagonal);
  QTest::mouseRelease(&moving,Qt::LeftButton,Qt::NoModifier,moving.mapFromGlobal(releaseGlobal));
  const QRect expectedGeometry(left?183:200,top?187:200,417,293);
  if(moving.geometry()!=expectedGeometry||commits!=beforeCount+1){std::cerr<<"FAILED: every chart corner keeps its opposite anchor fixed\n";return 1;}
 }
 moving.setGeometry(200,200,400,280);const QPoint lowerRight(396,276);
 QTest::mousePress(&moving,Qt::LeftButton,Qt::NoModifier,lowerRight);
 QMouseEvent shrink(QEvent::MouseMove,QPointF(-604,-724),QPointF(moving.mapToGlobal(QPoint(-604,-724))),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&moving,&shrink);
 QTest::mouseRelease(&moving,Qt::LeftButton,Qt::NoModifier,QPoint(316,226));
 if(moving.geometry()!=QRect(200,200,320,230)){std::cerr<<"FAILED: chart corner resizing respects a usable minimum size\n";return 1;}
 tigersheets::ChartWidget fastDrag(&canvas);fastDrag.setGeometry(20,20,400,280);fastDrag.show();int finalCommits=0;
 fastDrag.geometryCommitted=[&](QRect,QRect){++finalCommits;};
 QTest::mousePress(&fastDrag,Qt::LeftButton,Qt::NoModifier,QPoint(50,20));
 QTest::mouseRelease(&fastDrag,Qt::LeftButton,Qt::NoModifier,QPoint(75,35));
 if(fastDrag.geometry()!=QRect(45,35,400,280)||finalCommits!=1){std::cerr<<"FAILED: chart release commits its final pointer position when move events are compressed\n";return 1;}
 int gridDeletes=0,chartDeletes=0;
 QAction parentDelete(&canvas);parentDelete.setShortcut(QKeySequence(Qt::Key_Delete));parentDelete.setShortcutContext(Qt::WidgetWithChildrenShortcut);canvas.addAction(&parentDelete);
 QObject::connect(&parentDelete,&QAction::triggered,[&]{++gridDeletes;});
 moving.deleteRequested=[&]{++chartDeletes;};moving.setFocus();
 QTest::keyClick(&moving,Qt::Key_Delete);
 if(gridDeletes||chartDeletes!=1){std::cerr<<"FAILED: selected chart Delete requests chart deletion without clearing worksheet cells\n";return 1;}
 int edits=0;tigersheets::ChartEditAction requested=tigersheets::ChartEditAction::Type;
 moving.editRequested=[&](tigersheets::ChartEditAction action){++edits;requested=action;};
 QTest::mouseDClick(&moving,Qt::LeftButton,Qt::NoModifier,QPoint(60,22));
 if(edits!=1||requested!=tigersheets::ChartEditAction::General||moving.isInteracting()){
  std::cerr<<"FAILED: double-click selects and requests chart editing without leaving an active drag\n";return 1;
 }
 QTest::keyClick(&moving,Qt::Key_F2);
 if(edits!=2){std::cerr<<"FAILED: F2 edits a selected chart\n";return 1;}
 for(const auto& item:{std::pair{"chart.edit",tigersheets::ChartEditAction::General},std::pair{"chart.selectData",tigersheets::ChartEditAction::Data},std::pair{"chart.changeType",tigersheets::ChartEditAction::Type}}){
  QContextMenuEvent context(QContextMenuEvent::Mouse,QPoint(50,30),moving.mapToGlobal(QPoint(50,30)));
  QApplication::sendEvent(&moving,&context);
  auto* menu=moving.findChild<QMenu*>("chartContextMenu");auto* action=menu?menu->findChild<QAction*>(item.first):nullptr;
  if(!menu||!action){std::cerr<<"FAILED: chart context menu exposes "<<item.first<<"\n";return 1;}
  menu->setActiveAction(action);QTest::keyClick(menu,Qt::Key_Return);
  if(requested!=item.second){std::cerr<<"FAILED: chart context action requests its matching editor page\n";return 1;}
  app.sendPostedEvents(nullptr,QEvent::DeferredDelete);
 }
 QPointer<tigersheets::ChartWidget> disposable=new tigersheets::ChartWidget(&canvas);disposable->show();
 disposable->deleteRequested=[&]{delete disposable.data();};disposable->setSelected(true);disposable->setFocus();
 QTest::keyPress(disposable,Qt::Key_Delete);
 if(disposable){std::cerr<<"FAILED: chart delete callback can synchronously rebuild its owning view\n";return 1;}
 QPointer<tigersheets::ChartWidget> editedSender=new tigersheets::ChartWidget(&canvas);editedSender->show();
 editedSender->editRequested=[&](tigersheets::ChartEditAction){delete editedSender.data();};
 QTest::mouseDClick(editedSender,Qt::LeftButton,Qt::NoModifier,QPoint(50,20));
 if(editedSender){std::cerr<<"FAILED: chart edit callback can synchronously replace its sender\n";return 1;}
 QPointer<tigersheets::ChartWidget> movedSender=new tigersheets::ChartWidget(&canvas);movedSender->setGeometry(20,20,400,280);movedSender->show();
 movedSender->geometryCommitted=[&](QRect,QRect){delete movedSender.data();};
 QTest::mousePress(movedSender,Qt::LeftButton,Qt::NoModifier,QPoint(50,20));
 QMouseEvent finalMove(QEvent::MouseMove,QPointF(60,30),QPointF(movedSender->mapToGlobal(QPoint(60,30))),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(movedSender,&finalMove);
 QTest::mouseRelease(movedSender,Qt::LeftButton,Qt::NoModifier,QPoint(50,20));
 if(movedSender){std::cerr<<"FAILED: geometry commit callback can synchronously replace its sender\n";return 1;}
 Json book={{"revision",1},{"sheets",Json::array({{{"id","s"},{"name","Forecast"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Year"}},{{"row",1},{"col",2},{"input","Revenue"}},{{"row",2},{"col",1},{"input","2026"}},{{"row",2},{"col",2},{"input","100"}},{{"row",3},{"col",1},{"input","2027"}},{{"row",3},{"col",2},{"input","=B2*1.1"}}})}}})}};
 Json calculated={{"values",Json::array({{{"sheet","s"},{"row",2},{"col",2},{"value",100}},{{"sheet","s"},{"row",3},{"col",2},{"value",110}}})}};
 auto data=tigersheets::chartData(book,calculated,{{"sheet","s"},{"range",{{"row1",1},{"col1",1},{"row2",3},{"col2",2}}}});
 if(data["series"][0]["name"]!="Revenue"||data["series"][0]["values"]!=Json::array({100.0,110.0})){std::cerr<<"FAILED: chart derives live series from calculated workbook cells\n";return 1;}
 tigersheets::ChartWidget bar;bar.resize(500,300);bar.setChart({{"type","bar"}},{{"categories",Json::array({"Revenue"})},{"series",Json::array({{{"name","Revenue"},{"values",Json::array({10})}}})}});auto rendered=bar.grab().toImage();
 if(rendered.pixelColor(75,150)!=QColor("#245648")||rendered.pixelColor(250,65)==QColor("#245648")){std::cerr<<"FAILED: bar charts render horizontal bars rather than columns\n";return 1;}
 tigersheets::ChartWidget financial;financial.resize(500,300);financial.setChart({{"type","column"}},{{"categories",Json::array({"2026"})},{"series",Json::array({{{"name","Revenue"},{"values",Json::array({1000000})}}})}});
 const auto axisImage=financial.grab().toImage();
 QImage expected(180,18,QImage::Format_ARGB32);expected.fill(Qt::white);
 {QPainter painter(&expected);auto font=financial.font();font.setPointSize(9);font.setBold(false);painter.setFont(font);painter.setPen(QColor("#65756d"));painter.drawText(QRectF(expected.rect()),Qt::AlignLeft|Qt::AlignVCenter,QStringLiteral("1.08e+06"));}
 auto ink=[](const QImage& image,const QRect& area){int count=0;for(int y=area.top();y<=area.bottom();++y)for(int x=area.left();x<=area.right();++x){const auto color=image.pixelColor(x,y);if(color.red()<200&&color.green()<210&&color.blue()<205&&std::abs(color.red()-color.green())<25)++count;}return count;};
 if(ink(axisImage,QRect(0,49,180,18))<ink(expected,expected.rect())*.90){std::cerr<<"FAILED: million-value chart axis clips its full scientific label ("<<ink(axisImage,QRect(0,49,180,18))<<" / "<<ink(expected,expected.rect())<<")\n";return 1;}
 Json formulaBook={{"revision",7},{"sheets",Json::array({{{"id","s"},{"name","Report"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Month"}},{{"row",1},{"col",2},{"input","=\"Revenue\""}},{{"row",2},{"col",1},{"input","=DATE(2024,1,1)"},{"style",{{"numberFormat","mmm-yy"}}}},{{"row",2},{"col",2},{"input","10"}},{{"row",3},{"col",1},{"input","=DATE(2024,2,1)"},{"style",{{"numberFormat","mmm-yy"}}}},{{"row",3},{"col",2},{"input","20"}}})}}})}};
 Json formulaValues={{"revision",7},{"values",Json::array({{{"sheet","s"},{"row",1},{"col",2},{"value","Revenue"}},{{"sheet","s"},{"row",2},{"col",1},{"value",45292}},{{"sheet","s"},{"row",3},{"col",1},{"value",45323}},{{"sheet","s"},{"row",2},{"col",2},{"value",10}},{{"sheet","s"},{"row",3},{"col",2},{"value",20}}})}};
 const auto formulaData=tigersheets::chartData(formulaBook,formulaValues,{{"sheet","s"},{"range",{{"row1",1},{"col1",1},{"row2",3},{"col2",2}}}});
 if(formulaData["categories"]!=Json::array({"Jan-24","Feb-24"})||formulaData["series"][0]["name"]!="Revenue"||formulaData.value("xValues",Json::array())!=Json::array({45292,45323})){std::cerr<<"FAILED: chart labels must use calculated formatted values while numeric X remains numeric\n";return 1;}

 tigersheets::ChartWidget scatterA,scatterB;scatterA.resize(500,300);scatterB.resize(500,300);
 auto scatterData=[](const Json& x){return Json{{"categories",Json::array({"a","b","c"})},{"xValues",x},{"series",Json::array({{{"name","Yield"},{"values",Json::array({10,20,30})}}})}};};
 scatterA.setChart({{"type","scatter"}},scatterData(Json::array({0,1,100})));scatterB.setChart({{"type","scatter"}},scatterData(Json::array({0,50,100})));
 if(scatterA.grab().toImage().copy(60,58,414,127)==scatterB.grab().toImage().copy(60,58,414,127)){std::cerr<<"FAILED: scatter plot geometry must change with numeric X distances\n";return 1;}

 QImage sharedChart(500,300,QImage::Format_ARGB32);sharedChart.fill(Qt::white);{QPainter painter(&sharedChart);tigersheets::renderChart(painter,QRectF(0,0,500,300),{{"type","bar"}},{{"categories",Json::array({"Revenue"})},{"series",Json::array({{{"name","Revenue"},{"values",Json::array({10})}}})}},bar.font());}
 if(sharedChart.pixelColor(75,150)!=QColor("#245648")){std::cerr<<"FAILED: shared chart painter must render supported chart graphics without a widget\n";return 1;}

 QImage plainAxis(500,300,QImage::Format_ARGB32),formattedAxis(500,300,QImage::Format_ARGB32);plainAxis.fill(Qt::white);formattedAxis.fill(Qt::white);Json axisData={{"categories",Json::array({"Year"})},{"series",Json::array({{{"name","Multiple"},{"values",Json::array({10})},{"numberFormat","General"}}})}};
 {QPainter painter(&plainAxis);tigersheets::renderChart(painter,QRectF(0,0,500,300),{{"type","column"}},axisData,bar.font());}axisData["series"][0]["numberFormat"]="0.0\"x\"";{QPainter painter(&formattedAxis);tigersheets::renderChart(painter,QRectF(0,0,500,300),{{"type","column"}},axisData,bar.font());}
 if(plainAxis.copy(0,49,180,18)==formattedAxis.copy(0,49,180,18)){std::cerr<<"FAILED: financial chart axes must apply the shared number format from their series\n";return 1;}

}
