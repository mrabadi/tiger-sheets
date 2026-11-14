#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
using namespace tigersheets;
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir directory;qputenv("TIGERSHEETS_STATE_DIR",directory.path().toUtf8());MainWindow window;auto& session=window.currentSession();const auto ops=Json::parse(R"json([{"type":"add_sheet","id":"r","name":"Report"},{"type":"set_cell","sheet":"sheet-1","row":1,"col":1,"input":"Region"},{"type":"set_cell","sheet":"sheet-1","row":1,"col":2,"input":"Revenue"},{"type":"set_cell","sheet":"sheet-1","row":2,"col":1,"input":"East"},{"type":"set_cell","sheet":"sheet-1","row":2,"col":2,"input":"='[Missing.xlsx]Inputs'!A1"},{"type":"set_metadata","key":"links","value":[{"name":"Missing.xlsx","path":"/never-read/Missing.xlsx","enabled":true,"cachedWorkbook":{"revision":0,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"7"}]}]}}]}])json");if(!session.apply(0,ops))return 1;dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model())->refresh();
 auto definition=Json::parse(R"json({"id":"p","name":"Report","source":{"sheet":"sheet-1","row1":1,"col1":1,"row2":2,"col2":2},"rows":[{"field":"Region"}],"values":[{"field":"Revenue","aggregate":"sum"}],"output":{"sheet":"r","row1":1,"col1":1}})json");if(argc>1)definition["output"]["sheet"]="";bool asked=false;QTimer answer;answer.setInterval(5);QObject::connect(&answer,&QTimer::timeout,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(widget)){if(box->windowTitle()=="Use saved linked values?"){asked=true;box->button(QMessageBox::Yes)->click();}}});answer.start();if(!window.executeCommand("pivot.create",{{"definition",definition}}))return 1;QElapsedTimer timer;timer.start();while(!session.snapshotView().contains("pivots")&&timer.elapsed()<15000)app.processEvents();if(!asked||!session.snapshotView().contains("pivots")||!session.snapshotView()["pivots"][0]["provenance"]["allowCachedLinks"].get<bool>()){std::cerr<<"desktop must request explicit cached-source consent before reporting: "<<window.lastError().toStdString()<<'\n';return 1;}std::cout<<"cached source consent passed\n";
}
