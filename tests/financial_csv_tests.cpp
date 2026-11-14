#include "tigersheets/main_window.hpp"
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir dir;qputenv("TIGERSHEETS_STATE_DIR",dir.path().toUtf8());try{const auto path=dir.filePath("model.csv");QFile file(path);require(file.open(QIODevice::WriteOnly),"create CSV fixture");file.write("100\n");file.close();MainWindow window;require(window.openWorkbook(path),"open CSV model");Json definition={{"version",1},{"id","base"},{"name","Base"},{"kind","assumption_set"},{"cells",Json::array({{{"sheet","sheet-1"},{"row",1},{"col",1},{"type","number"},{"value",100}}})}};std::string error;const bool saved=window.currentSession().apply(0,Json::array({{{"type","save_financial_definition"},{"definition",definition}}}),&error);require(saved,error.c_str());require(window.saveWorkbook(path),"CSV value export succeeds");bool explained=false;for(auto* label:window.findChildren<QLabel*>())explained|=label->text().contains("save XLSX to retain workbook changes");require(explained,"CSV export must explain that saved financial definitions require XLSX");std::cout<<"financial CSV tests passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
