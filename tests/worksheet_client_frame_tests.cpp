#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QScrollBar>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 require(window.executeCommand("view.ribbon"),"collapse the real native ribbon to the source initial view");QApplication::processEvents();
 auto* grid=window.findChild<WorksheetView*>("worksheet.grid");require(grid&&!window.findChild<QWidget*>("workspace.quickAccess"),"the native worksheet has no duplicate Quick Access row");
 const auto before=window.currentSession().snapshot();const auto origin=grid->viewport()->mapTo(&window,QPoint());
 std::cout<<"viewport="<<origin.x()<<","<<origin.y()<<" rowHeader="<<grid->verticalHeader()->mapTo(&window,QPoint()).x()<<":"<<grid->verticalHeader()->width()<<" scrollbar="<<grid->verticalScrollBar()->mapTo(&window,QPoint()).x()<<":"<<grid->verticalScrollBar()->width()<<"\n";
 require(origin.x()==29,"actual worksheet first white pixel follows the source row-header separator at x28");
 require(grid->viewport()->width()==1229&&grid->verticalScrollBar()->mapTo(&window,QPoint()).x()==1258&&grid->verticalScrollBar()->width()==22,"the source left frame preserves the existing right worksheet separator and scrollbar footprint");
 require(origin.y()==130&&grid->horizontalHeader()->mapTo(&window,QPoint()).y()==110,"the worksheet uses the space recovered by removing the duplicate formatting row");
 require(grid->columnWidth(0)==64&&grid->columnWidth(1)==64,"source column pitches are preserved instead of moving individual cell outlines");
 require(grid->mapTo(&window,QPoint()).x()==0,"the worksheet client frame stays aligned with the window without another toolbar");
 grid->setShowGrid(false);QApplication::processEvents();require(grid->viewport()->mapTo(&window,QPoint())==origin,"hiding gridlines preserves the actual worksheet origin");
 window.resize(1380,800);QApplication::processEvents();require(grid->viewport()->mapTo(&window,QPoint()).x()==29,"the real client frame remains stable when the window grows");
 require(window.currentSession().snapshot()==before,"source client framing, view-only grid visibility and resizing preserve workbook data and history");
 std::cout<<"Native source worksheet client frame passed\n";
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
