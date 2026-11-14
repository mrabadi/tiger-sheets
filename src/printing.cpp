#include "tigersheets/printing.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/chart.hpp"
#include "tigersheets/print_settings.hpp"
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QTableView>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace tigersheets {
bool printWorksheet(QWidget* parent,WorksheetModel& model,QTableView& view,const QString& pdfPath,const Json& workbook,const Json& calculated){
 const auto& book=workbook.empty()?model.workbookView():workbook;Json settings=Json::object();const auto bookSettings=book.value("settings",Json::object());if(bookSettings.is_object()&&bookSettings.contains("printSheets")&&bookSettings.at("printSheets").contains(model.sheetId().toStdString()))settings=bookSettings.at("printSheets").at(model.sheetId().toStdString());validatePrintSettings(settings);
 QPrinter printer(QPrinter::HighResolution);printer.setResolution(300);
 if(!pdfPath.isEmpty()){printer.setOutputFormat(QPrinter::PdfFormat);printer.setOutputFileName(pdfPath);}
 else{QPrintDialog dialog(&printer,parent);if(dialog.exec()!=QDialog::Accepted)return false;}
 if(settings.value("orientation",std::string("portrait"))=="landscape")printer.setPageOrientation(QPageLayout::Landscape);
 auto selection=view.selectionModel()->selection();auto current=view.currentIndex();if(!current.isValid())current=model.index(0,0);
 auto range=selection.isEmpty()?QItemSelectionRange(current,current):selection.front();
 const auto sheetId=model.sheetId().toStdString();std::vector<Json> charts;for(const auto& chart:book.value("charts",Json::array()))if(chart.value("sheet",std::string())==sheetId)charts.push_back(chart);
 auto columnWidth=[&](int col){return std::max(1,view.columnWidth(col)>0?view.columnWidth(col):view.horizontalHeader()->defaultSectionSize());};
 auto heightAt=[&](int row){return std::max(1,view.rowHeight(row)>0?view.rowHeight(row):view.verticalHeader()->defaultSectionSize());};
 if(range.height()==1&&range.width()==1){int top=1048576,left=16384,bottom=0,right=0;for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==model.sheetId().toStdString())for(const auto& cell:sheet.at("cells"))if(!cell.value("input",std::string()).empty()||cell.contains("pivotOwner")||(cell.contains("style")&&!cell.at("style").empty())){const int row=cell.at("row"),col=cell.at("col");top=std::min(top,row);left=std::min(left,col);bottom=std::max(bottom,row);right=std::max(right,col);}for(const auto& chart:charts){const auto anchor=chart.value("anchor",Json::object());const int firstRow=anchor.value("row",12),firstCol=anchor.value("col",4),width=anchor.value("width",560),height=anchor.value("height",300);if(firstRow<1||firstCol<1||firstRow>1048576||firstCol>16384||width<1||height<1||width>8192||height>8192)throw std::runtime_error("Chart print geometry is invalid");int lastRow=firstRow,lastCol=firstCol;double remaining=height+anchor.value("offsetY",0);while(lastRow<1048576&&remaining>heightAt(lastRow-1))remaining-=heightAt(lastRow++-1);remaining=width+anchor.value("offsetX",0);while(lastCol<16384&&remaining>columnWidth(lastCol-1))remaining-=columnWidth(lastCol++-1);top=std::min(top,firstRow);left=std::min(left,firstCol);bottom=std::max(bottom,lastRow);right=std::max(right,lastCol);}range=bottom?QItemSelectionRange(model.index(top-1,left-1),model.index(bottom-1,right-1)):QItemSelectionRange(current,current);}
 if(settings.contains("area")){const auto& area=settings.at("area");range=QItemSelectionRange(model.index(area.at("row1").get<int>()-1,area.at("col1").get<int>()-1),model.index(area.at("row2").get<int>()-1,area.at("col2").get<int>()-1));}
 if(range.height()>10000||range.width()>200||static_cast<std::int64_t>(range.height())*range.width()>250000)throw std::runtime_error("Print range exceeds 10000 rows, 200 columns or 250000 cells");
 const auto page=printer.pageLayout().paintRectPixels(printer.resolution());const double deviceScale=printer.resolution()/96.0;
 std::vector<double> widths;double totalWidth=0;
 for(int col=range.left();col<=range.right();++col){const double width=std::max(1,view.columnWidth(col)>0?view.columnWidth(col):view.horizontalHeader()->defaultSectionSize());widths.push_back(width);totalWidth+=width;}
 const double scale=deviceScale*(settings.value("fitWidth",true)?std::min(1.0,page.width()/(deviceScale*totalWidth)):1.0);const double pageHeight=page.height()/scale,pageWidth=page.width()/scale;
 std::vector<double> heights;
 for(int row=range.top();row<=range.bottom();++row){const double height=std::max(1,view.rowHeight(row)>0?view.rowHeight(row):view.verticalHeader()->defaultSectionSize());if(height>pageHeight)throw std::runtime_error("A worksheet row is taller than the printable page; reduce its height before printing");heights.push_back(height);}
 std::vector<int> headers;if(settings.contains("repeatHeaderRows")){const auto& rows=settings.at("repeatHeaderRows");for(int row=rows.at("row1").get<int>()-1;row<rows.at("row2").get<int>();++row)headers.push_back(row);if(headers.front()>range.top()&&headers.front()<=range.bottom())throw std::runtime_error("Repeated headers inside the print area must begin at its first row.");}
 auto rowHeight=[&](int row){return static_cast<double>(std::max(1,view.rowHeight(row)>0?view.rowHeight(row):view.verticalHeader()->defaultSectionSize()));};
 double headerHeight=0;for(int row:headers)headerHeight+=rowHeight(row);if(headerHeight>=pageHeight)throw std::runtime_error("Repeated headers are taller than the printable page.");
 struct HorizontalPage {int first,last;double offset,width;};std::vector<HorizontalPage> horizontal;
 if(settings.value("fitWidth",true))horizontal.push_back({range.left(),range.right(),0,totalWidth});else{int first=range.left();double used=0,offset=0;for(int col=range.left();col<=range.right();++col){const auto width=widths[static_cast<std::size_t>(col-range.left())];if(width>pageWidth)throw std::runtime_error("A column is wider than the printable page at actual size; enable fit width.");if(used>0&&used+width>pageWidth){horizontal.push_back({first,col-1,offset,used});offset+=used;used=0;first=col;}used+=width;}horizontal.push_back({first,range.right(),offset,used});}
 std::vector<std::vector<int>> vertical(1);double usedHeight=headerHeight;for(int row=range.top();row<=range.bottom();++row){if(std::find(headers.begin(),headers.end(),row)!=headers.end())continue;const auto height=rowHeight(row);if(headerHeight+height>pageHeight)throw std::runtime_error("A worksheet row does not fit below its repeated headers.");if(usedHeight+height>pageHeight){vertical.emplace_back();usedHeight=headerHeight;}vertical.back().push_back(row);usedHeight+=height;}
 struct PrintedChart{Json specification,data;QRectF bounds;};std::vector<PrintedChart> printedCharts;Json fallback;const Json* chartValues=&calculated;if(calculated.empty()&&!charts.empty()){fallback=model.calculatedSnapshot();chartValues=&fallback;}
 for(const auto& chart:charts){const auto anchor=chart.value("anchor",Json::object());const int row=anchor.value("row",12)-1,col=anchor.value("col",4)-1;const QRectF bounds(view.horizontalHeader()->sectionPosition(col)-view.horizontalHeader()->sectionPosition(range.left())+anchor.value("offsetX",0),view.verticalHeader()->sectionPosition(row)-view.verticalHeader()->sectionPosition(range.top())+anchor.value("offsetY",0),anchor.value("width",560),anchor.value("height",300));double rangeHeight=0;for(const auto height:heights)rangeHeight+=height;if(!bounds.intersects(QRectF(0,0,totalWidth,rangeHeight)))continue;const auto type=chart.value("type",std::string("line"));if(type!="line"&&type!="column"&&type!="bar"&&type!="area"&&type!="scatter"&&type!="pie"&&type!="doughnut")throw std::runtime_error("This chart type cannot be printed; choose a supported chart type.");printedCharts.push_back({chart,chartData(book,*chartValues,chart),bounds});}
 QPainter painter;if(!painter.begin(&printer))throw std::runtime_error("Cannot start worksheet printer");painter.scale(scale,scale);
 auto drawRow=[&](int row,double top,const HorizontalPage& part){const auto height=rowHeight(row);double left=0;
  for(int col=part.first;col<=part.last;++col){
   const auto index=model.index(row,col);const double width=widths[static_cast<std::size_t>(col-range.left())];const QRectF cell(left,top,width,height);
   const auto fill=model.data(index,Qt::BackgroundRole);if(fill.isValid())painter.fillRect(cell,fill.value<QColor>());
   painter.setPen(QColor("#dce2da"));painter.drawRect(cell);painter.save();painter.setClipRect(cell.adjusted(1,1,-1,-1),Qt::IntersectClip);
   auto font=model.data(index,Qt::FontRole).value<QFont>();if(font.pointSizeF()>0)font.setPixelSize(std::max(1,static_cast<int>(std::lround(font.pointSizeF()*96.0/72.0))));painter.setFont(font);
   const auto foreground=model.data(index,Qt::ForegroundRole);painter.setPen(foreground.isValid()?foreground.value<QColor>():QColor("#202c33"));
   painter.drawText(cell.adjusted(4,0,-4,0),model.data(index,Qt::TextAlignmentRole).toInt()|Qt::TextSingleLine,model.data(index).toString());
   painter.restore();left+=width;
  }
 };
 bool firstPage=true;for(const auto& rows:vertical)for(const auto& part:horizontal){if(!firstPage&&!printer.newPage())throw std::runtime_error("Cannot create worksheet print page");firstPage=false;painter.save();painter.setClipRect(QRectF(0,0,pageWidth,pageHeight));double top=0;for(int row:headers){drawRow(row,top,part);top+=rowHeight(row);}for(int row:rows){drawRow(row,top,part);top+=rowHeight(row);}const double bodyOffset=rows.empty()?0:view.verticalHeader()->sectionPosition(rows.front())-view.verticalHeader()->sectionPosition(range.top());painter.save();if(!rows.empty())painter.setClipRect(QRectF(0,headerHeight,pageWidth,pageHeight-headerHeight),Qt::IntersectClip);for(const auto& chart:printedCharts){auto bounds=chart.bounds.translated(-part.offset,headerHeight-bodyOffset);if(bounds.intersects(QRectF(0,headerHeight,pageWidth,pageHeight-headerHeight)))renderChart(painter,bounds,chart.specification,chart.data,view.font());}painter.restore();painter.restore();}

 return painter.end();
}
}
