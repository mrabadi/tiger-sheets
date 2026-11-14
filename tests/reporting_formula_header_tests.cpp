#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/pivot_fields_pane.hpp"
#include <QApplication>
#include <QTableView>
#include <QPushButton>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <iostream>
using namespace tigersheets;
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir dir;qputenv("TIGERSHEETS_STATE_DIR",dir.path().toUtf8());MainWindow window;auto* view=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());model->setData(model->index(0,0),"=\"Region\"");model->setData(model->index(0,1),"Revenue");model->setData(model->index(1,0),"East");model->setData(model->index(1,1),"=8+4");window.recalculateNow();view->selectionModel()->select(QItemSelection(model->index(0,0),model->index(1,1)),QItemSelectionModel::ClearAndSelect);if(!window.executeCommand("pivot.create")){std::cerr<<"Report field discovery failed: "<<window.lastError().toStdString()<<"\n";return 2;}PivotFieldsPane* pane=nullptr;for(auto* widget:window.findChildren<QWidget*>())if(auto* candidate=dynamic_cast<PivotFieldsPane*>(widget))pane=candidate;if(!pane||pane->definition()["rows"][0]["field"]!="Region"){std::cerr<<"Report field discovery must use current calculated formula header results\n";return 1;}auto* apply=pane->findChild<QPushButton*>("pivot.apply");if(!apply)return 2;apply->click();QElapsedTimer elapsed;elapsed.start();while(window.property("reportBusy").toBool()&&elapsed.elapsed()<10000)app.processEvents();const auto& book=window.currentSession().snapshotView();if(!book.contains("pivots")||book["pivots"][0]["layout"]["grandTotals"][0]["value"]!=12){std::cerr<<"Calculated field names and records must produce the same report through the desktop worker\n";return 1;}std::cout<<"calculated formula headers passed\n";}
