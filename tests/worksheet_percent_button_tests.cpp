#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{MainWindow window;window.show();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model->setData(model->index(12,2),"0.1",Qt::EditRole),"seed COGS assumption");grid->setCurrentIndex(model->index(12,2));auto* ribbon=window.findChild<QTabWidget*>("workspace.ribbon");ribbon->setCurrentIndex(0);QToolButton* percent=nullptr;for(auto* button:ribbon->currentWidget()->findChildren<QToolButton*>())if(button->text()=="%")percent=button;require(percent,"Home ribbon provides the percent toolbar action used in the tutorial");QTest::mouseClick(percent,Qt::LeftButton);window.recalculateNow();require(model->data(model->index(12,2)).toString()=="10%"&&model->numericValue(12,2).value_or(-1)==0.1,"percent toolbar shows whole percentages without changing numeric assumptions");std::cout<<"percent toolbar tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
