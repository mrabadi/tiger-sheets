#include "tigersheets/calculation_scheduler.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
using namespace tigersheets;
int main(int argc,char** argv){QCoreApplication app(argc,argv);Json result;CalculationScheduler scheduler;scheduler.onResult=[&](Json value){result=std::move(value);};Json book={{"revision",7},{"sheets",Json::array({{{"id","s"},{"name","Sheet1"},{"cells",Json::array({{{"row",1},{"col",1},{"input","=Driver*2"}}})}}})}};scheduler.request(std::move(book),[](Json owned){if(QThread::currentThread()==QCoreApplication::instance()->thread())throw std::runtime_error("calculation preparation must run off the event loop");owned["names"]={{"Driver","7"}};owned["_tigersheetsContextGeneration"]=13;return owned;});QElapsedTimer timer;timer.start();while(result.is_null()&&timer.elapsed()<5000)app.processEvents();if(result.value("status",std::string())!="ok"||result.at("values")[0].at("value")!=14||result.value("contextGeneration",0)!=13){std::cerr<<"immutable captured inputs are prepared off-thread and results retain their linked-input generation\n";return 1;}std::cout<<"scheduler preparation passed\n";}
