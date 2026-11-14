#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
using namespace tigersheets;
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir directory;qputenv("TIGERSHEETS_STATE_DIR",directory.path().toUtf8());MainWindow window;auto& session=window.currentSession();
 if(!session.apply(0,Json::array({{{"type","set_metadata"},{"key","settings"},{"value",{{"iterationEnabled",true},{"maxIterations",3},{"maxChange",1e-9}}}}})))return 1;
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());model->refresh();model->setData(model->index(0,0),"=2+3");model->setData(model->index(0,1),"=A1");window.recalculateNow();if(model->data(model->index(0,0)).toString()!="5")return 1;
 model->setData(model->index(0,0),"=A1+1");model->setData(model->index(0,1),"=A1");window.recalculateNow();if(model->data(model->index(0,1)).toString()=="5"||!window.lastError().contains("converg",Qt::CaseInsensitive)){std::cerr<<"a failed desktop calculation must remove prior usable numeric results and expose convergence diagnostics\n";return 1;}
 std::cout<<"reporting calculation UI passed\n";
}
