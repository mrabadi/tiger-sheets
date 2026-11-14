#include "tigersheets/main_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void suiteControls(){
 MainWindow window;window.show();auto* dock=window.findChild<QDockWidget*>("workspace.codex");require(dock,"the workbook has a Codex dock");dock->show();QApplication::processEvents();
 auto* model=dock->findChild<QComboBox*>("codex.model");auto* effort=dock->findChild<QComboBox*>("codex.effort");auto* speed=dock->findChild<QComboBox*>("codex.speed");
 require(model&&effort&&speed,"Codex must expose OwlDocs' Model, Reasoning, and Speed selectors");
 require(model->isVisible()&&effort->isVisible()&&speed->isVisible(),"all three selectors remain visible while offline");
 require(!model->isEnabled()&&!effort->isEnabled()&&!speed->isEnabled(),"selectors wait for the runtime capability catalog");
 require(model->accessibleName()=="Codex model"&&effort->accessibleName()=="Reasoning effort"&&speed->accessibleName()=="Response speed","the suite selectors have accessible names");
 const auto a=model->mapTo(dock,QPoint()),b=effort->mapTo(dock,QPoint()),c=speed->mapTo(dock,QPoint());
 require(a.y()==b.y()&&b.y()==c.y()&&a.x()<b.x()&&b.x()<c.x(),"Model, Reasoning, and Speed use OwlDocs' compact three-column row");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{suiteControls();std::cout<<"Codex workspace tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
