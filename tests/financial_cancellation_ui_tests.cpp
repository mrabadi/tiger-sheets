#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QTableView>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir dir;qputenv("TIGERSHEETS_STATE_DIR",dir.path().toUtf8());try{MainWindow window;auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model->setData(model->index(0,0),"=1+1",Qt::EditRole),"schedule ordinary formula calculation");const Json definition={{"version",1},{"id","base"},{"kind","assumption_set"},{"name","Base"},{"cells",Json::array({{{"sheet","sheet-1"},{"row",1},{"col",2},{"type","number"},{"value",10}}})}};require(window.executeCommand("financial.save",{{"definition",definition}}),"start isolated setup capture before ordinary recalculation");require(window.executeCommand("financial.cancel"),"cancel captured financial job");QElapsedTimer deadline;deadline.start();while(!model->numericValue(0,0)&&deadline.elapsed()<3000){app.processEvents();QThread::msleep(1);}require(!window.currentSession().hasPreview()&&model->numericValue(0,0)==2,"cancelling financial work must resume pending ordinary worksheet calculation");std::cout<<"financial cancellation UI tests passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
