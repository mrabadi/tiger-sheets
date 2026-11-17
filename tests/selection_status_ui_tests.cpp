#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QLabel>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct Fixture {
 MainWindow window;QTableView* view;WorksheetModel* model;QLabel* status;
 Fixture(){window.show();QApplication::processEvents();view=window.findChild<QTableView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(view->model());status=window.findChild<QLabel*>("workspace.selectionStatus");require(view&&model&&status,"actual native worksheet and status controls exist");}
 void select(int bottom,int right,int top=0,int left=0){view->setCurrentIndex(model->index(top,left));view->selectionModel()->select(QItemSelection(model->index(top,left),model->index(bottom,right)),QItemSelectionModel::ClearAndSelect);}
 bool wait(const QString& sum,const QString& count){return QTest::qWaitFor([&]{return status->text().contains(sum)&&status->text().contains(count);},3000);}
 void seedMixed(){require(model->setData(model->index(0,0),"2")&&model->setData(model->index(0,1),"50%")&&model->setData(model->index(0,2),"=A1*3"),"mixed canonical numeric, percentage and formula fixtures");}
};
void general(){Fixture f;f.seedMixed();f.select(0,2);require(f.wait("Sum: 8.5","Count: 3"),"mixed formats preserve canonical numeric selection totals");}
void percentage(){Fixture f;const auto id=f.model->sheetId().toStdString();Json edits=Json::array();for(int col=3;col<=5;++col)edits.push_back({{"type","set_cell"},{"sheet",id},{"row",7},{"col",col},{"input",std::to_string(.13+.01*col)},{"style",{{"numberFormat","0.0%"}}}});require(f.model->apply(edits),"source16/17/18percent selection fixture");const auto before=f.window.currentSession().snapshot();f.select(6,4,6,2);require(f.wait("Sum: 51.0%","Count: 3")&&f.status->text().contains("Average: 17.0%"),"source percentage statistics follow the common cell precision rather than showing raw decimals");require(f.window.currentSession().snapshot()==before,"formatting status totals never changes the workbook");}
void single(){Fixture f;f.model->setData(f.model->index(0,0),"42");f.select(0,0);f.window.recalculateNow();QApplication::processEvents();require(f.model->numericValue(0,0)==42,"single populated fixture is genuinely calculated");require(f.status->text().isEmpty(),"source single populated cell keeps the numeric summary area blank");}
void textCount(bool sparse){Fixture f;f.seedMixed();f.model->setData(f.model->index(0,3),"Label");const auto before=f.window.currentSession().snapshot();f.select(sparse?4999:0,3);require(f.wait("Sum: 8.5","Count: 4"),"source Count includes populated text while Average and Sum use only the three numeric cells");require(f.window.currentSession().snapshot()==before,"dense and sparse summary scans preserve every input and style");}
}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir dir;qputenv("TIGERSHEETS_STATE_DIR",dir.path().toUtf8());try{const std::string mode=argc>1?argv[1]:"";if(mode.empty()||mode=="general")general();if(mode.empty()||mode=="percentage")percentage();if(mode.empty()||mode=="single")single();if(mode.empty()||mode=="text")textCount(false);if(mode.empty()||mode=="sparse")textCount(true);std::cout<<"source selection status passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
