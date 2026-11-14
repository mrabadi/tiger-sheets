#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QAbstractItemDelegate>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QImage>
#include <QPainter>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
int inkRows(QAbstractItemDelegate* delegate,const QStyleOptionViewItem& option,const QModelIndex& index){QImage image(option.rect.size(),QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter painter(&image);delegate->paint(&painter,option,index);painter.end();int rows=0;for(int y=0;y<image.height();++y){bool ink=false;for(int x=0;x<image.width();++x){const auto color=image.pixelColor(x,y);if(color.red()<160&&color.green()<160&&color.blue()<160){ink=true;break;}}rows+=ink;}return rows;}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 const std::string text="Forecast revenue and operating income across multiple financial reporting periods";
 WorkbookSession session({{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","Report"},{"cells",Json::array({{{"row",1},{"col",1},{"input",text},{"style",{{"wrap",true},{"fontFamily","DejaVu Sans"},{"fontSize",11}}}},{{"row",2},{"col",1},{"input",text},{"style",{{"wrap",false},{"fontFamily","DejaVu Sans"},{"fontSize",11}}}}})}}})}});WorksheetModel model(session);WorksheetView view;view.setModel(&model);view.setWordWrap(false);
 QStyleOptionViewItem option;option.initFrom(&view);option.widget=&view;option.rect=QRect(0,0,115,220);option.state=QStyle::State_Enabled;option.textElideMode=Qt::ElideNone;option.palette.setColor(QPalette::Text,Qt::black);option.palette.setColor(QPalette::Base,Qt::white);option.features.setFlag(QStyleOptionViewItem::WrapText,false);
 QStyledItemDelegate standard;auto globallyWrapped=option;globallyWrapped.features.setFlag(QStyleOptionViewItem::WrapText,true);const auto normal=standard.sizeHint(option,model.index(1,0)),control=standard.sizeHint(globallyWrapped,model.index(1,0));require(control.height()>normal.height()*2,"bounded Qt delegate fixture must distinguish wrapped and single-line text");
 const auto before=session.snapshot();auto* delegate=view.itemDelegate();const auto wrapped=delegate->sizeHint(option,model.index(0,0)),single=delegate->sizeHint(option,model.index(1,0));
 require(wrapped.height()>single.height()*2,"wrap=true must reach the cell delegate even when worksheet word wrap is disabled");
 require(inkRows(delegate,option,model.index(0,0))>inkRows(delegate,option,model.index(1,0))*2,"native delegate painting must display wrapped text on multiple visible lines");
 require(delegate->sizeHint(globallyWrapped,model.index(1,0)).height()==single.height(),"wrap=false must retain a single line even when a view option enables wrapping");
 require(session.snapshot()==before,"delegate sizing and painting preserve workbook state");std::cout<<"worksheet wrapping tests passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
