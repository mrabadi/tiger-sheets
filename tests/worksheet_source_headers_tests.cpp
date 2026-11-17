#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QAbstractButton>
#include <QApplication>
#include <QHeaderView>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
QColor pixel(QHeaderView* header,int section,int offset=3,bool edge=false){auto* surface=header->viewport();const auto image=surface->grab().toImage();if(header->orientation()==Qt::Horizontal)return image.pixelColor(header->sectionViewportPosition(section)+offset,edge?image.height()-1:2);return image.pixelColor(edge?image.width()-1:2,header->sectionViewportPosition(section)+offset);}
double rowLabelCenter(QHeaderView* header,int section){const auto image=header->viewport()->grab().toImage();const int start=header->sectionViewportPosition(section);int left=image.width(),right=-1;for(int y=start+3;y<start+17;++y)for(int x=5;x<image.width()-4;++x)if(image.pixelColor(x,y).lightness()<120){left=std::min(left,x);right=std::max(right,x);}require(right>=left,"source row header visibly paints its actual numeric label");return (left+right)/2.0;}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");view->setFocus();QTest::keyClick(view,Qt::Key_F1,Qt::ControlModifier);QTest::keyRelease(view,Qt::Key_Control);view->setCurrentIndex(view->model()->index(1,1));QApplication::processEvents();const auto before=window.currentSession().snapshot();auto* columns=view->horizontalHeader();auto* rows=view->verticalHeader();
 require(pixel(columns,1)==QColor("#d2d2d2")&&pixel(columns,2)==QColor("#e7e7e7"),"source current column is flat gray while idle columns retain their distinct source background");
 require(pixel(rows,1)==QColor("#d2d2d2")&&pixel(rows,2)==QColor("#e7e7e7"),"source current row has the same selection-dependent flat header pattern");
 require(std::abs(rowLabelCenter(rows,0)-rowLabelCenter(rows,9))<=1&&rowLabelCenter(rows,0)>=14&&rowLabelCenter(rows,0)<=17,"source one- and two-digit row numbers share their observed horizontal center");
 require(pixel(columns,1,6,true)==QColor("#217346")&&pixel(rows,1,6,true)==QColor("#217346"),"selected source headers expose the green bottom and right edges");
 const auto cellRect=view->visualRect(view->currentIndex());const auto cellPixels=view->viewport()->grab().toImage();for(const int x:{cellRect.left()-2,cellRect.left()-1})require(cellPixels.pixelColor(x,cellRect.top()+5)==QColor("#217346"),"native boundary-crossing cell selection and source header edges share the same observed green");require(cellPixels.pixelColor(cellRect.left()+1,cellRect.top()+5)!=QColor("#217346"),"the source stroke stays on the physical cell boundary rather than returning to the old inset position");
 QTest::keyClick(view,Qt::Key_Right);QApplication::processEvents();require(pixel(columns,1)==QColor("#e7e7e7")&&pixel(columns,2)==QColor("#d2d2d2"),"native keyboard navigation repaints the old and new header without retaining stale selection");
 QTest::keyClick(view,Qt::Key_Right,Qt::ShiftModifier);QTest::keyRelease(view,Qt::Key_Shift);QApplication::processEvents();require(pixel(columns,2)==QColor("#d2d2d2")&&pixel(columns,3)==QColor("#d2d2d2")&&pixel(columns,4)==QColor("#e7e7e7"),"source multi-cell selections highlight every participating column");
 QTest::keyClick(view,Qt::Key_Space,Qt::ControlModifier);QTest::keyRelease(view,Qt::Key_Control);QApplication::processEvents();require(pixel(columns,view->currentIndex().column())==QColor("#d3f0e0")&&pixel(rows,6)==QColor("#d2d2d2"),"source whole-column selection uses green full-axis headers while participating row headers remain gray");
 QAbstractButton* corner=nullptr;for(auto* button:view->findChildren<QAbstractButton*>())if(QByteArray(button->metaObject()->className())=="QTableCornerButton")corner=button;require(corner&&corner->isVisible(),"the existing native select-all corner remains available");auto cornerImage=corner->grab().toImage();require(cornerImage.pixelColor(cornerImage.width()-4,5).lightness()<200&&cornerImage.pixelColor(3,3)==QColor("#e7e7e7"),"the source corner contains its graphical select-all triangle");QTest::mouseClick(corner,Qt::LeftButton);QApplication::processEvents();const auto selected=view->selectionModel()->selection();require(selected.size()==1&&selected.front().height()==1048576&&selected.front().width()==16384,"clicking the native corner still selects the real whole worksheet");require(pixel(columns,6)==QColor("#d3f0e0")&&pixel(rows,6)==QColor("#d3f0e0")&&corner->grab().toImage().pixelColor(3,3)==QColor("#d3f0e0"),"whole-worksheet selection paints green full-axis headers and corner without expanding selected indexes");
 require(window.currentSession().snapshot()==before,"header graphics and native navigation leave all workbook contents and history untouched");std::cout<<"source dynamic worksheet headers passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
