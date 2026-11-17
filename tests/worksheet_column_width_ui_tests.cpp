#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include <QApplication>
#include <QFontMetrics>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
int main(int argc,char** argv){QApplication app(argc,argv);try{
 auto book=WorkbookSession().snapshot();book["sheets"][0]["columnLayout"]=Json::array({{{"index",1},{"size",10.0}}});WorkbookSession session(book);WorksheetModel model(session);WorksheetView view;const QFont normal("Calibri",11);view.setFont(normal);view.setModel(&model);const int digit=SpreadsheetNormalDigitWidth;
 // For an integer stored width of10, the normative pixel conversion is
 // exactly ten maximum-digit widths: its rounding term is always below1px.
 if(view.columnWidth(0)!=10*digit)throw std::runtime_error("native column width must not receive an additional five pixels of padding");
 std::cout<<"Worksheet column width UI tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
