#include "tigersheets/calculation.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace tigersheets;
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
int main(int argc,char**argv) {
 QApplication app(argc,argv);
 try {
  const int count=argc>1?std::stoi(argv[1]):100000;
  if(count<100||count>1000000||count%100)throw std::runtime_error("Cell count must be a multiple of 100, at most 1000000");
  Json book={{"revision",0},{"sheets",Json::array()}};
  for(int sheet=0;sheet<20;++sheet){Json cells=Json::array();for(int i=0;i<count/20;++i){int row=i/20+1,col=i%20+1;std::string input=col%5==0?"=SUM("+columnName(col-4)+std::to_string(row)+":"+columnName(col-1)+std::to_string(row)+")":"100";cells.push_back({{"row",row},{"col",col},{"input",input}});}book["sheets"].push_back({{"id","s"+std::to_string(sheet)},{"name","Sheet"+std::to_string(sheet+1)},{"cells",std::move(cells)}});}
  CalculationEngine engine;Json times=Json::array();double worst=0;
  for(int i=0;i<(count==1000000?1:3);++i){auto start=Clock::now();auto result=engine.calculate(book);auto ms=elapsed(start);worst=std::max(worst,ms);times.push_back(ms);if(result.value("status",std::string())!="ok"||result.at("values").size()!=static_cast<size_t>(count))throw std::runtime_error("Benchmark calculation did not produce every expected cell");for(const auto& value:result["values"])if(value["col"].get<int>()%5==0&&value["value"]!=400.0)throw std::runtime_error("Benchmark formula returned an incorrect value");}
  Json edits=Json::array();double p95=0;
  if(count<=100000){WorkbookSession session(std::move(book));WorksheetModel model(session);std::vector<double> samples;for(int i=0;i<11;++i){auto start=Clock::now();if(!model.setData(model.index(0,0),QString::number(i+1),Qt::EditRole))throw std::runtime_error("Benchmark edit failed");samples.push_back(elapsed(start));}edits=samples;std::sort(samples.begin(),samples.end());p95=samples[static_cast<size_t>(std::ceil(.95*samples.size()))-1];}
  Json report={{"cells",count},{"formulas",count/5},{"sheets",20},{"calculationMs",times},{"ordinaryCalculationBelow1000ms",worst<1000},{"modelEditMs",edits},{"modelEditP95Ms",p95},{"modelEditBelow50ms",!edits.empty()&&p95<50},{"inputMeasurement","WorksheetModel::setData; excludes rendering and desktop recovery checkpoint"},{"referenceMachineMatch",false}};
  std::cout<<report.dump(2)<<'\n';if(argc>2){std::ofstream output(argv[2]);output<<report.dump(2)<<'\n';}
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
