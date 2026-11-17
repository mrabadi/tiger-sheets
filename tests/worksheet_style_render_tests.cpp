#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
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
 for(int x=rect.left()+5;x<rect.right()-5;++x)for(int y=rect.top();y<=rect.top()+1;++y)if(image.pixelColor(x,y)==QColor("#a31515"))++colored;
 require(colored>=rect.width()-15,"a subtotal's explicit top border is visible across the worksheet cell");
 require(image.pixelColor(rect.center().x(),rect.bottom()-2)!=QColor("#a31515"),"top border does not invent a bottom border");
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
int main(int argc,char** argv){QApplication app(argc,argv);try{const std::string selected=argc>1?argv[1]:"";if(selected.empty()||selected=="accounting_glyph")accountingCurrencyGlyph();if(selected.empty()||selected=="accounting")accountingFill();if(selected.empty()||selected=="headers")sourceHeaderGeometry();if(selected.empty()||selected=="selection_fill")activeCellSelectionFill();if(selected.empty()||selected=="vertical")verticalAlignment();if(selected.empty()||selected=="border")subtotalBorder();if(selected.empty()||selected=="indent")indentedLabel();if(selected.empty()||selected=="default_font")worksheetDefaultFont();if(selected.empty()||selected=="view_defaults")worksheetViewDefaults();std::cout<<"worksheet style rendering tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
