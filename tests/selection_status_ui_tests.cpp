#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QLabel>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <iostream>
using namespace tigersheets;
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir dir;qputenv("TIGERSHEETS_STATE_DIR",dir.path().toUtf8());MainWindow window;auto* view=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());model->setData(model->index(0,0),"2");model->setData(model->index(0,1),"50%");model->setData(model->index(0,2),"=A1*3");view->selectionModel()->select(QItemSelection(model->index(0,0),model->index(0,2)),QItemSelectionModel::ClearAndSelect);QElapsedTimer timer;timer.start();while(timer.elapsed()<3000){app.processEvents();for(auto* label:window.findChildren<QLabel*>())if(label->text().contains("Sum: 8.5")&&label->text().contains("Count: 3")){std::cout<<"sparse selection status passed\n";return 0;}}std::cerr<<"Selection status must include canonical numeric literals and sparse calculated percent/formula results\n";return 1;}
