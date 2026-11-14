#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/calculation.hpp"
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
#include <iostream>
using namespace tigersheets;
int main(int argc,char** argv){QApplication app(argc,argv);const int records=argc>1?std::stoi(argv[1]):10000;QTemporaryDir directory;qputenv("TIGERSHEETS_STATE_DIR",directory.path().toUtf8());QFile file(directory.filePath("capacity.csv"));if(!file.open(QIODevice::WriteOnly))return 2;file.write("A,B,C,D,E,F,G,H,I,J\n");for(int row=0;row<records;++row)file.write("1,2,3,4,5,6,7,8,9,10\n");file.close();MainWindow window;window.show();if(!window.openWorkbook(file.fileName()))return 2;window.recalculateNow();app.processEvents();auto* view=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());for(const auto* task:{"raw","style","data","format"}){QElapsedTimer probe;probe.start();for(int n=0;n<1000;++n){if(std::string(task)=="raw")model->raw(n%10,0);else if(std::string(task)=="style")model->cellStyle(n%10,0);else if(std::string(task)=="data")model->data(model->index(n%10,0));else formatCellValue(n%10,"General");}std::cerr<<task<<"1000="<<probe.nsecsElapsed()/1e6<<"ms\n";}
std::vector<double> samples,edits,paints;for(int i=0;i<20;++i){QElapsedTimer timer;timer.start();if(!model->setData(model->index(i<10?0:i-10,i<10?0:10),QString::number(i+100),Qt::EditRole))return 2;const auto edit=timer.nsecsElapsed()/1e6;view->viewport()->repaint();const auto total=timer.nsecsElapsed()/1e6;edits.push_back(edit);paints.push_back(total-edit);samples.push_back(total);if(i==0)std::cerr<<"first edit="<<edit<<"ms paint="<<total-edit<<"ms\n";QElapsedTimer interval;interval.start();while(interval.elapsed()<80){app.processEvents();QThread::msleep(1);}}std::sort(samples.begin(),samples.end());const auto p95=samples[18];std::cout<<Json{{"editCoverage","ten existing-cell edits and ten insertions into a new adjacent column"},{"sourceRecords",records},{"sourceRange","A1:J"+std::to_string(records+1)},{"populatedCells",records*10},{"samples",samples},{"editMs",edits},{"paintMs",paints},{"inputToPaintP95Ms",p95},{"targetMs",50},{"referenceMachineMatch",false}}.dump()<<'\n';return p95<50?0:1;}
