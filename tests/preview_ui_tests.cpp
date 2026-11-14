#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/codex.hpp"
#include "codex_preview_test.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void previewView(){MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto book=window.currentSession().snapshot();const auto result=test::isolatedPreview(window,{{"revision",book["revision"]},{"operations",Json::array({{{"type","set_cell"},{"sheet",book["sheets"][0]["id"]},{"row",1},{"col",1},{"input","=99"}}})}});check(result.value("status","")=="preview","host creates preview");window.executeCommand("view.formulas");check(!(grid->model()->flags(grid->model()->index(0,0))&Qt::ItemIsEditable),"View commands must preserve preview isolation");}
void malformedChart(){MainWindow window;auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());check(model->apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",Json::array({{{"id","incomplete"},{"type","line"},{"sheet",model->sheetId().toStdString()}}})}}})),"legacy metadata is accepted for preservation");QElapsedTimer timer;timer.start();while(timer.elapsed()<100)QApplication::processEvents();check(!window.lastError().isEmpty(),"asynchronous display errors need a diagnostic");}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());int failed=0;for(const auto& test:{std::pair{"previewView",previewView},std::pair{"malformedChart",malformedChart}})try{test.second();}catch(const std::exception& error){std::cerr<<"FAILED "<<test.first<<": "<<error.what()<<'\n';++failed;}return failed?1:0;}
