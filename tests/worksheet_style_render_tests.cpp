#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/worksheet_border_paint.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QImage>
#include <QPainter>
#include <QScrollBar>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct Fixture {
 WorkbookSession session;WorksheetModel model{session};WorksheetView view;
 Fixture(){view.setModel(&model);view.resize(700,350);view.setShowGrid(false);view.horizontalHeader()->setDefaultSectionSize(140);view.verticalHeader()->setDefaultSectionSize(36);view.setCurrentIndex(model.index(5,3));view.show();QApplication::processEvents();}
 void cell(int row,const Json& style){require(model.apply(Json::array({{{"type","set_cell"},{"sheet",model.sheetId().toStdString()},{"row",row},{"col",2},{"input","Revenue"},{"style",style}}})),"seed visible formatted label");QApplication::processEvents();}
 QImage image(){QImage image(view.viewport()->size(),QImage::Format_ARGB32);image.fill(Qt::white);QPainter painter(&image);view.viewport()->render(&painter);return image;}
};
void subtotalBorder(){
 Fixture f;f.cell(2,{{"border",{{"top",{{"style","thin"},{"color","#a31515"}}}}}});
 const auto rect=f.view.visualRect(f.model.index(1,1));const auto image=f.image();int colored=0;
 for(int x=rect.left()+5;x<rect.right()-5;++x)if(image.pixelColor(x,rect.top()-1)==QColor("#a31515"))++colored;
 require(colored>=rect.width()-15,"a subtotal's explicit top border is visible across the worksheet cell");
 require(image.pixelColor(rect.center().x(),rect.bottom()-2)!=QColor("#a31515"),"top border does not invent a bottom border");
}
void borderContinuity(){
 bool continuous=true;
 for(bool grid:{true,false})for(const auto* style:{"thin","medium","thick","double"})for(const auto* side:{"top","bottom","left","right"}){
  Fixture f;f.view.setShowGrid(grid);const bool horizontal=std::string_view(side)=="top"||std::string_view(side)=="bottom";
  Json operations=Json::array();
  for(int offset=0;offset<3;++offset)operations.push_back({{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",2+(horizontal?0:offset)},{"col",2+(horizontal?offset:0)},{"input",""},{"style",{{"border",{{side,{{"style",style},{"color","#a31515"}}}}}}}});
  require(f.model.apply(operations),"prepare three adjacent cells with the same native border");QApplication::processEvents();
  const auto before=f.session.snapshot();const auto image=f.image();
  if(grid&&std::string_view(style)=="thin"&&std::string_view(side)=="top")if(const auto path=qEnvironmentVariable("TIGERSHEETS_BORDER_CAPTURE");!path.isEmpty())require(image.save(path),"retain actual continuous top-border pixels");
  const auto first=f.view.visualRect(f.model.index(1,1)),last=f.view.visualRect(f.model.index(horizontal?1:3,horizontal?3:1));
  const int start=horizontal?first.left()+3:first.top()+3,end=horizontal?last.right()-3:last.bottom()-3;
  const int boundary=std::string_view(side)=="top"?first.top()-1:std::string_view(side)=="bottom"?first.top()+f.view.rowHeight(1)-1:std::string_view(side)=="left"?first.left()-1:first.left()+f.view.columnWidth(1)-1;
  const int fixed=boundary-(std::string_view(style)=="double"?1:0);
  for(int at=start;at<=end;++at)if(image.pixelColor(horizontal?at:fixed,horizontal?fixed:at)!=QColor("#a31515")){
   continuous=false;std::cerr<<"Border gap: "<<style<<' '<<side<<" grid="<<grid<<" at="<<at<<" cell="<<first.x()<<','<<first.y()<<':'<<first.width()<<'x'<<first.height()<<'\n';
   break;
  }
  require(f.session.snapshot()==before,"painting continuous borders preserves styles, values, and history");
 }
 require(continuous,"solid and double borders must join across adjacent cells on all four sides with or without gridlines");
}
void sharedBorderPainting(){
 bool correct=true;
 for(bool grid:{true,false})for(bool horizontal:{true,false})for(int scenario=0;scenario<6;++scenario){
  Fixture f;f.view.setShowGrid(grid);Json first=Json::object(),second=Json::object();
  const auto edge=[](const char* style,const char* color){return Json{{"style",style},{"color",color}};};
  if(scenario!=1)first=edge(scenario==3?"thick":"thin","#a31515");
  if(scenario!=0)second=edge(scenario==5?"thick":"thin",scenario==2||scenario==5?"#a31515":"#174ea6");
  const auto* firstSide=horizontal?"bottom":"right";const auto* secondSide=horizontal?"top":"left";
  require(f.model.apply(Json::array({
   {{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",2},{"col",2},{"input",""},{"style",{{"border",first.empty()?Json::object():Json{{firstSide,first}}}}}},
   {{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",horizontal?3:2},{"col",horizontal?2:3},{"input",""},{"style",{{"border",second.empty()?Json::object():Json{{secondSide,second}}}}}}
  })),"prepare imported one-sided, mirrored, or conflicting shared borders");QApplication::processEvents();
  const auto before=f.session.snapshot();const auto rect=f.view.visualRect(f.model.index(1,1));const auto image=f.image();
  const int boundary=horizontal?rect.top()+f.view.rowHeight(1)-1:rect.left()+f.view.columnWidth(1)-1;
  const QColor expected(scenario==1||scenario==4?"#174ea6":"#a31515");const bool thick=scenario==3||scenario==5;
  for(int offset=-2;offset<=2;++offset){const auto pixel=image.pixelColor(horizontal?rect.center().x():boundary+offset,horizontal?boundary+offset:rect.center().y());
   if((pixel==expected)!=(thick?std::abs(offset)<=1:offset==0)||(pixel==QColor("#a31515")||pixel==QColor("#174ea6"))!=(thick?std::abs(offset)<=1:offset==0)){
    correct=false;std::cerr<<"Shared border mismatch grid="<<grid<<" horizontal="<<horizontal<<" scenario="<<scenario<<" offset="<<offset<<'\n';break;
   }
  }
  require(f.session.snapshot()==before,"resolving imported shared edges never rewrites workbook styles or history");
 }
 require(correct,"one physical shared edge uses one stroke, stronger weight wins, and ties prefer the lower/right cell");
}
void borderPatterns(){
 for(const auto* style:{"dashed","dotted","dashDot","dashDotDot","mediumDashed","mediumDashDot","mediumDashDotDot","slantDashDot","hair"}){
  Fixture f;f.view.setShowGrid(true);auto expected=f.image();Json operations=Json::array();
  for(int column=2;column<=4;++column)operations.push_back({{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",2},{"col",column},{"input",""},{"style",{{"border",{{"top",{{"style",style},{"color","#a31515"}}}}}}}});
  const auto first=f.view.visualRect(f.model.index(1,1)),last=f.view.visualRect(f.model.index(1,3));
  const int y=first.top()-1,left=first.left()-1,right=last.left()+f.view.columnWidth(3)-1;
  {QPainter painter(&expected);painter.setRenderHint(QPainter::Antialiasing,std::string_view(style)=="hair");painter.setPen(worksheetBorderPen(style,QColor("#a31515")));painter.drawLine(QLineF(left,y,right+1,y));}
  require(f.model.apply(operations),"prepare a continuous patterned border across adjacent cells");QApplication::processEvents();
  const auto before=f.session.snapshot();const QRect band(left+3,y-2,right-left-5,5);const auto actual=f.image();
  require(actual.copy(band)==expected.copy(band),"dash/dot phase, hair antialiasing, weight, and color match one uninterrupted stroke across cell joins");
  require(f.session.snapshot()==before,"pattern rendering does not edit stored line styles");
 }
}
void borderBoundaryCases(){
 Fixture f;f.view.setShowGrid(true);const Json edge={{"style","thin"},{"color","#a31515"}};
 require(f.model.apply(Json::array({{{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",1},{"col",1},{"input",""},{"style",{{"border",{{"top",edge},{"left",edge}}}}}}})),"prepare the actual first worksheet row and column edges");QApplication::processEvents();
 auto image=f.image();require(image.pixelColor(12,0)==QColor("#a31515")&&image.pixelColor(0,12)==QColor("#a31515"),"the worksheet top and left edges remain visible at viewport zero");
 f.view.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);f.view.setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);f.view.verticalScrollBar()->setValue(3);f.view.horizontalScrollBar()->setValue(3);QApplication::processEvents();image=f.image();
 require(image.pixelColor(12,0)!=QColor("#a31515")&&image.pixelColor(0,12)!=QColor("#a31515"),"scrolling away never clamps an offscreen border into a false viewport-edge line");
 f.view.verticalScrollBar()->setValue(0);f.view.horizontalScrollBar()->setValue(0);
 Json border=Json::object();for(const auto* side:{"top","bottom","left","right"})border[side]=edge;
 require(f.model.apply(Json::array({
  {{"type","set_sheet_layout"},{"sheet",f.model.sheetId().toStdString()},{"mergedRanges",Json::array({{{"row1",2},{"row2",3},{"col1",2},{"col2",4}}})}},
  {{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",2},{"col",2},{"input",""},{"style",{{"border",border}}}}
 })),"prepare a bordered merged span");QApplication::processEvents();const auto before=f.session.snapshot();image=f.image();
 const int left=f.view.columnViewportPosition(1)-1,right=f.view.columnViewportPosition(3)+f.view.columnWidth(3)-1,top=f.view.rowViewportPosition(1)-1,bottom=f.view.rowViewportPosition(2)+f.view.rowHeight(2)-1;
 for(int x=left;x<=right;++x)require(image.pixelColor(x,top)==QColor("#a31515")&&image.pixelColor(x,bottom)==QColor("#a31515"),"a merged span preserves continuous top and bottom perimeter edges");
 for(int y=top;y<=bottom;++y)require(image.pixelColor(left,y)==QColor("#a31515")&&image.pixelColor(right,y)==QColor("#a31515"),"a merged span preserves continuous left and right perimeter edges");
 require(image.pixelColor((left+right)/2,(top+bottom)/2)!=QColor("#a31515"),"a merged span does not acquire internal borders");
 if(const auto path=qEnvironmentVariable("TIGERSHEETS_BORDER_MERGED_CAPTURE");!path.isEmpty())require(image.save(path),"retain actual merged-border perimeter pixels");
 f.view.verticalScrollBar()->setValue(75);QApplication::processEvents();image=f.image();require(image.pixelColor((left+right)/2,0)!=QColor("#a31515")&&image.pixelColor(left,10)==QColor("#a31515"),"a partly visible merged span retains its side edge without inventing an offscreen top");
 require(f.session.snapshot()==before,"border clipping and scrolling preserve merged styles and workbook history");
}
void mergedPerimeterMembers(){
 Fixture f;f.view.setShowGrid(true);const Json edge={{"style","thin"},{"color","#a31515"}};Json operations=Json::array({{{"type","set_sheet_layout"},{"sheet",f.model.sheetId().toStdString()},{"mergedRanges",Json::array({{{"row1",2},{"row2",3},{"col1",2},{"col2",4}}})}}});
 for(int row=2;row<=3;++row)for(int column=2;column<=4;++column){Json border=Json::object();if(row==2)border["top"]=edge;if(row==3)border["bottom"]=edge;if(column==2)border["left"]=edge;if(column==4)border["right"]=edge;operations.push_back({{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",row},{"col",column},{"input",""},{"style",{{"border",border}}}});}
 require(f.model.apply(operations),"prepare an imported merged outline stored on its perimeter member cells");QApplication::processEvents();const auto before=f.session.snapshot();const auto image=f.image();
 const int left=f.view.columnViewportPosition(1)-1,right=f.view.columnViewportPosition(3)+f.view.columnWidth(3)-1,top=f.view.rowViewportPosition(1)-1,bottom=f.view.rowViewportPosition(2)+f.view.rowHeight(2)-1;
 require(image.pixelColor((left+right)/2,top)==QColor("#a31515")&&image.pixelColor((left+right)/2,bottom)==QColor("#a31515")&&image.pixelColor(left,(top+bottom)/2)==QColor("#a31515")&&image.pixelColor(right,(top+bottom)/2)==QColor("#a31515"),"merged outer edges retain borders stored on the last member row and column as well as the anchor");
 require(f.session.snapshot()==before,"reading merged member borders does not rewrite imported styles");
}
void hiddenBorders(){
 for(bool row:{true,false}){
  Fixture f;f.view.setShowGrid(true);const auto* side=row?"bottom":"right";
  require(f.model.apply(Json::array({
   {{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",2},{"col",2},{"input",""},{"style",{{"border",{{side,{{"style","thick"},{"color","#a31515"}}}}}}}},
   {{"type","set_sheet_layout"},{"sheet",f.model.sheetId().toStdString()},{row?"rows":"columns",Json::array({{{"index",2},{"hidden",true}}})}}
  })),"prepare a border belonging only to a hidden row or column");QApplication::processEvents();const auto before=f.session.snapshot();const auto image=f.image();
  int red=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y)==QColor("#a31515"))++red;
  require(red==0,"a hidden cell's border must not leak onto the join of neighboring visible rows or columns");require(f.session.snapshot()==before,"painting hidden sections does not change their styles or visibility");
 }
}
int firstInk(const QImage& image,const QRect& rect){for(int x=rect.left()+1;x<rect.right()-1;++x)for(int y=rect.top()+4;y<rect.bottom()-4;++y){const auto color=image.pixelColor(x,y);if(color.red()<100&&color.green()<100&&color.blue()<100)return x;}throw std::runtime_error("rendered label has visible text");}
void indentedLabel(){
 Fixture f;f.cell(2,{{"alignment","left"},{"indent",0}});f.cell(3,{{"alignment","left"},{"indent",1}});
 const auto image=f.image();const int plain=firstInk(image,f.view.visualRect(f.model.index(1,1))),indented=firstInk(image,f.view.visualRect(f.model.index(2,1)));
 require(indented-plain>=8,"one level of cell indentation moves the label three spaces to the right");
 require(f.model.raw(2,1)=="Revenue","indentation never pads or changes the stored text");
}
void worksheetDefaultFont(){
 auto book=WorkbookSession().snapshot();book["sheets"][0]["defaultStyle"]={{"fontFamily","Calibri"},{"fontSize",12},{"color","#0000ff"}};
 book["sheets"][0]["cells"]=Json::array({{{"row",2},{"col",2},{"input","Revenue"},{"style",{{"bold",true}}}}});
 WorkbookSession session(book);WorksheetModel model(session);
 require(model.data(model.index(4,4),Qt::FontRole).value<QFont>().pointSizeF()==12,"blank cells inherit the worksheet's 12-point default font");
 require(model.data(model.index(1,1),Qt::FontRole).value<QFont>().pointSizeF()==12&&model.data(model.index(1,1),Qt::FontRole).value<QFont>().bold(),"explicit cell formatting overlays the worksheet defaults");
 require(model.data(model.index(1,1),Qt::ForegroundRole).value<QColor>()==QColor("#0000ff"),"default font color remains visible on partially styled cells");
}
void verticalAlignment(){
 Fixture f;f.cell(2,Json::object());f.cell(3,{{"verticalAlignment","top"}});f.cell(4,{{"verticalAlignment","center"},{"alignment","right"}});const auto bottom=f.model.data(f.model.index(1,1),Qt::TextAlignmentRole).toInt(),top=f.model.data(f.model.index(2,1),Qt::TextAlignmentRole).toInt(),center=f.model.data(f.model.index(3,1),Qt::TextAlignmentRole).toInt();require((bottom&Qt::AlignVertical_Mask)==Qt::AlignBottom,"ordinary cells use Excel's bottom vertical alignment");require((top&Qt::AlignVertical_Mask)==Qt::AlignTop&&(center&Qt::AlignVertical_Mask)==Qt::AlignVCenter&&(center&Qt::AlignHorizontal_Mask)==Qt::AlignRight,"explicit vertical alignment is independent of horizontal alignment");const auto image=f.image();auto firstInkRow=[&](const QRect& rect){for(int y=rect.top()+2;y<rect.bottom()-2;++y)for(int x=rect.left()+6;x<rect.right()-6;++x){const auto c=image.pixelColor(x,y);if(c.red()<100&&c.green()<100&&c.blue()<100)return y-rect.top();}return -1;};require(firstInkRow(f.view.visualRect(f.model.index(1,1)))-firstInkRow(f.view.visualRect(f.model.index(2,1)))>=10,"native text visibly moves between the bottom and top of a tall row");
}
void accountingFill(){
 Fixture f;const std::string format="_(\"$\"* #,##0_);_(\"$\"* (#,##0);_(\"$\"* \"-\"_);_(@_)";require(f.model.apply(Json::array({{{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",2},{"col",2},{"input","1234"},{"style",{{"numberFormat",format}}}},{{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",3},{"col",2},{"input","-1234"},{"style",{{"numberFormat",format}}}},{{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",4},{"col",2},{"input","0"},{"style",{{"numberFormat",format}}}}})),"seed actual native Accounting sections");QApplication::processEvents();const auto before=f.session.snapshot();auto verify=[&]{const auto image=f.image();for(int row=1;row<=3;++row){const auto rect=f.view.visualRect(f.model.index(row,1));const int start=firstInk(image,rect);require(start-rect.left()<20,"Accounting currency remains at the left edge while the number/dash is right aligned");int last=-1;for(int x=rect.right()-1;x>rect.left();--x){bool found=false;for(int y=rect.top()+4;y<rect.bottom()-4;++y){const auto c=image.pixelColor(x,y);if(c.red()<100&&c.green()<100&&c.blue()<100){found=true;break;}}if(found){last=x;break;}}require(last>rect.right()-22,"the Accounting numeric section fills to the cell's right edge");}};verify();f.view.setColumnWidth(1,240);QApplication::processEvents();verify();require(f.session.snapshot()==before&&f.model.raw(1,1)=="1234"&&f.model.raw(2,1)=="-1234","Accounting fill adapts to actual width without changing values, formats or history");
}
void accountingCurrencyGlyph(){
 Fixture f;const std::string format=R"fmt(_("$"* #,##0_);_("$"* \(#,##0\);_("$"* "-"_);_(@_))fmt";require(f.model.apply(Json::array({{{"type","set_cell"},{"sheet",f.model.sheetId().toStdString()},{"row",2},{"col",2},{"input","1234"},{"style",{{"numberFormat",format}}}}})),"seed imported Accounting with a quoted dollar");QApplication::processEvents();const auto rect=f.view.visualRect(f.model.index(1,1));const auto font=f.model.data(f.model.index(1,1),Qt::FontRole).value<QFont>();const QFontMetricsF metrics(font);auto expected=f.image();expected.fill(Qt::white);{QPainter painter(&expected);painter.setFont(font);painter.setPen(Qt::black);painter.drawText(QPointF(rect.left()+2+metrics.horizontalAdvance('('),rect.bottom()-metrics.descent()+1),"$");}const auto actual=f.image();const QRect symbol(rect.left()+3,rect.top()+3,25,rect.height()-6);require(actual.copy(symbol)==expected.copy(symbol),"native quoted Accounting currency renders exactly the dollar glyph without quote characters or misplaced padding");
}
void activeCellSelectionFill(){
 Fixture f;f.view.setCurrentIndex(f.model.index(1,1));f.view.selectionModel()->select(QItemSelection(f.model.index(1,1),f.model.index(1,3)),QItemSelectionModel::ClearAndSelect);QApplication::processEvents();const auto image=f.image();auto sample=[&](int column){return image.pixelColor(f.view.visualRect(f.model.index(1,column)).topLeft()+QPoint(12,6));};require(sample(1)==QColor(Qt::white),"the active cell retains its white fill inside a selected range");const auto selected=sample(2);require(selected.red()==selected.green()&&selected.green()==selected.blue()&&selected.red()<230,"other selected cells use the source's neutral gray selection shading");require(sample(4)==QColor(Qt::white),"selection shading stops at the selected range");
}
void sourceHeaderGeometry(){
 WorkbookSession session;WorksheetModel model(session);WorksheetView view;view.setModel(&model);view.resize(1280,720);view.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);view.verticalHeader()->setDefaultSectionSize(20);view.show();QApplication::processEvents();const auto before=session.snapshot();require(view.verticalHeader()->width()==28&&view.horizontalHeader()->height()==20,"source-sized row and column headers use 28 and 20 pixels for the visible initial rows");view.verticalScrollBar()->setValue(20000);QApplication::processEvents();require(view.rowAt(0)>=999&&view.verticalHeader()->width()>=34,"the row header grows for visible four-digit row numbers");view.verticalScrollBar()->setValue(0);QApplication::processEvents();require(view.verticalHeader()->width()==28&&session.snapshot()==before,"returning to initial rows restores the narrow header without editing workbook geometry or cells");
}
void worksheetViewDefaults(){
 auto book=WorkbookSession().snapshot();book["sheets"][0]["defaultRowHeight"]=15.75;book["sheets"][0]["showGridLines"]=false;
 WorkbookSession session(book);WorksheetModel model(session);WorksheetView view;view.setModel(&model);
 require(!view.showGrid(),"the worksheet's hidden gridline state is visible after opening");
 require(view.rowHeight(400)==qRound(15.75*view.logicalDpiY()/72.0),"rows without individual sizing use the workbook's default row height");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{const std::string selected=argc>1?argv[1]:"";if(selected.empty()||selected=="accounting_glyph")accountingCurrencyGlyph();if(selected.empty()||selected=="accounting")accountingFill();if(selected.empty()||selected=="headers")sourceHeaderGeometry();if(selected.empty()||selected=="selection_fill")activeCellSelectionFill();if(selected.empty()||selected=="vertical")verticalAlignment();if(selected.empty()||selected=="border")subtotalBorder();if(selected.empty()||selected=="border_continuity")borderContinuity();if(selected.empty()||selected=="border_shared")sharedBorderPainting();if(selected.empty()||selected=="border_patterns")borderPatterns();if(selected.empty()||selected=="border_boundaries")borderBoundaryCases();if(selected.empty()||selected=="border_merged_members")mergedPerimeterMembers();if(selected.empty()||selected=="border_hidden")hiddenBorders();if(selected.empty()||selected=="indent")indentedLabel();if(selected.empty()||selected=="default_font")worksheetDefaultFont();if(selected.empty()||selected=="view_defaults")worksheetViewDefaults();std::cout<<"worksheet style rendering tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
