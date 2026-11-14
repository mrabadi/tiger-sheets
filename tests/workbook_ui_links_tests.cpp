#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
using namespace tigersheets;
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir folder;qputenv("TIGERSHEETS_STATE_DIR",folder.path().toUtf8());
 Json source={{"revision",0},{"sheets",Json::array({{{"id","inputs"},{"name","Inputs"},{"cells",Json::array({{{"row",1},{"col",1},{"input","=3+4"}}})}}})}};const auto path=folder.filePath("Assumptions.xlsx");writeWorkbook(path.toStdString(),source);
 MainWindow window;auto& session=window.currentSession();auto book=session.snapshot();session.apply(0,Json::array({{{"type","set_cell"},{"sheet",book["sheets"][0]["id"]},{"row",1},{"col",1},{"input","='[Assumptions.xlsx]Inputs'!A1*2"}},{{"type","set_metadata"},{"key","links"},{"value",Json::array({{{"name","Assumptions.xlsx"},{"path",path.toStdString()},{"enabled",true}}})}}}));
 auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());model->refresh();window.recalculateNow();
 if(!window.executeCommand("links.refresh")||model->data(model->index(0,0)).toString()!="14"){std::cerr<<"FAILED: explicit desktop link refresh calculates local workbook inputs\n";return 1;}
 model->setData(model->index(0,1),"1",Qt::EditRole);model->setData(model->index(0,2),"=A1*B1",Qt::EditRole);window.recalculateNow();
 if(!window.executeCommand("analysis.goalSeek",{{"output","C1"},{"target",28},{"changing","B1"},{"accept",true}})){std::cerr<<"FAILED: desktop Goal Seek must use the same retained-link calculation context: "<<window.lastError().toStdString()<<'\n';return 1;}
 window.recalculateNow();if(model->data(model->index(0,2)).toString()!="28"){std::cerr<<"FAILED: linked analysis result is consistent with the worksheet\n";return 1;}
 QFile::remove(path);model->setData(model->index(0,1),"=A1+1",Qt::EditRole);window.recalculateNow();
 if(model->data(model->index(0,1)).toString()!="15"){std::cerr<<"FAILED: ordinary edits reuse the explicitly loaded source snapshot\n";return 1;}
}
