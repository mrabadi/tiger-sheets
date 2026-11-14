#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QLabel>
#include <QElapsedTimer>
#include <QTableView>
#include <QThread>
#include <QTemporaryDir>
#include <iostream>
using namespace tigersheets;
bool until(QApplication& app,const std::function<bool()>& ready){QElapsedTimer timer;timer.start();while(!ready()&&timer.elapsed()<5000){app.processEvents();QThread::msleep(1);}return ready();}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir dir;qputenv("TIGERSHEETS_STATE_DIR",dir.path().toUtf8());MainWindow window;auto& session=window.currentSession();Json cached={{"revision",0},{"sheets",Json::array({{{"id","i"},{"name","Inputs"},{"cells",Json::array({{{"row",1},{"col",1},{"input","7"}}})}}})}};if(!session.apply(0,Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","='[Assumptions.xlsx]Inputs'!A1"}},{{"type","set_metadata"},{"key","links"},{"value",Json::array({{{"name","Assumptions.xlsx"},{"path","/missing/Assumptions.xlsx"},{"enabled",true},{"cachedWorkbook",cached}}})}}})))return 2;window.recalculateNow();auto* status=window.findChild<QLabel*>("workspace.linkStatus");if(!status||!status->text().contains("saved",Qt::CaseInsensitive)||!status->toolTip().contains("Assumptions.xlsx")){std::cerr<<"The grid must expose saved/unavailable linked source status beside its calculated values\n";return 1;}
 auto* grid=window.findChild<QTableView*>("worksheet.grid");if(!grid)return 2;status->clear();status->setToolTip({});if(!grid->model()->setData(grid->model()->index(1,1),"1")||!until(app,[&]{return status->text().contains("saved",Qt::CaseInsensitive)&&status->toolTip().contains("Assumptions.xlsx");})){std::cerr<<"Asynchronous calculation must retain named linked-source status\n";return 1;}
 auto* model=dynamic_cast<WorksheetModel*>(grid->model());if(!model||!model->apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","9"}},{{"type","set_metadata"},{"key","links"},{"value",Json::array()}}}))||!until(app,[&]{return status->text().isEmpty()&&status->toolTip().isEmpty();})){std::cerr<<"A healthy new calculation must clear the previous linked-source warning\n";return 1;}
 if(!model->apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","='[Assumptions.xlsx]Inputs'!A1"}},{{"type","set_metadata"},{"key","links"},{"value",Json::array({{{"name","Assumptions.xlsx"},{"path","/missing/Assumptions.xlsx"},{"enabled",true}}})}}})))return 2;window.recalculateNow();if(!status->text().contains("unavailable",Qt::CaseInsensitive)||!status->toolTip().contains("Assumptions.xlsx")){std::cerr<<"Unavailable source status must remain visible even when formula calculation fails\n";return 1;}
 std::cout<<"linked source status passed\n";}
