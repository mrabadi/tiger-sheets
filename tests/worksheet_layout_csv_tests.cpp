#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir dir;qputenv("TIGERSHEETS_STATE_DIR",dir.path().toUtf8());try{
 const Json layouts=Json::array({Json{{"mergedRanges",Json::array({{{"row1",1},{"col1",1},{"row2",1},{"col2",2}}})}},Json{{"rows",Json::array({{{"index",1},{"size",30.0}}})}},Json{{"columns",Json::array({{{"index",1},{"hidden",true}}})}}});
 for(const auto& layout:layouts){const auto path=dir.filePath("layout.csv");QFile file(path);require(file.open(QIODevice::WriteOnly),"create CSV");file.write("100\n");file.close();MainWindow window;require(window.openWorkbook(path),"open CSV");auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());Json operation=layout;operation["type"]="set_sheet_layout";operation["sheet"]=model->sheetId().toStdString();require(model->apply(Json::array({operation})),"apply worksheet layout");require(window.saveWorkbook(path),"export CSV values");bool explained=false;for(auto* label:window.findChildren<QLabel*>())explained|=label->text().contains("save XLSX to retain workbook changes");require(explained,"CSV export must explain that merged cells and dimensions require XLSX");}
 std::cout<<"worksheet layout CSV tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
