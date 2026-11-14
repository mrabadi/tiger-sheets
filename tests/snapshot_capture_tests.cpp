#include "tigersheets/snapshot_capture.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <iostream>
using namespace tigersheets;
bool guardDestroyed=false;
struct CallbackGuard {~CallbackGuard(){guardDestroyed=true;}};
bool largeRetirement(){Json source={{"cells",Json::array()}};auto& cells=source["cells"].get_ref<Json::array_t&>();cells.reserve(200000);for(int row=0;row<200000;++row)cells.push_back({{"row",row},{"col",1},{"input","Financial source"},{"style",{{"numberFormat","$#,##0.00"},{"bold",true},{"fill","#e7efe6"}}}});SnapshotCapture capture(nullptr,4096);std::shared_ptr<Json> result;capture.completed=[&](auto value){result=std::move(value);};capture.start(source,[]{return true;});QElapsedTimer elapsed;elapsed.start();while(capture.running()&&elapsed.elapsed()<30000)QCoreApplication::processEvents();if(!result){std::cerr<<"large financial snapshot capture completes\n";return false;}elapsed.restart();result.reset();const auto release=elapsed.elapsed();if(release>=50){std::cerr<<"releasing a completed 200000-cell financial snapshot must return within 50 ms; observed "<<release<<" ms\n";return false;}return true;}
int main(int argc,char** argv){QCoreApplication app(argc,argv);Json source={{"revision",7},{"cells",Json::array()}};for(int i=0;i<10000;++i)source["cells"].push_back({{"row",i},{"input","Financial source"}});
 SnapshotCapture capture(nullptr,32);std::shared_ptr<Json> result;capture.completed=[&](auto value){result=std::move(value);};int ticks=0;QTimer ticker;QObject::connect(&ticker,&QTimer::timeout,[&]{++ticks;});ticker.start(0);
 if(!capture.start(source,[]{return true;}))return 1;QElapsedTimer elapsed;elapsed.start();while(capture.running()&&elapsed.elapsed()<10000)app.processEvents();
 if(!result||*result!=source||ticks<10){std::cerr<<"snapshot capture must preserve every source value and let UI events run between slices\n";return 1;}
 QString failure;capture.failed=[&](QString text){failure=text;};if(capture.start(source,[]{return false;})||capture.running()||failure.isEmpty()){std::cerr<<"already stale input must be rejected immediately\n";return 1;}
 auto guard=std::make_shared<CallbackGuard>();bool callbackSurvived=false;capture.completed=[guard,&capture,&callbackSurvived](auto){auto* observed=&callbackSurvived;capture.completed={};*observed=!guardDestroyed;};guard.reset();capture.start(source,[]{return true;});elapsed.restart();while(capture.running()&&elapsed.elapsed()<10000)app.processEvents();if(!callbackSurvived){std::cerr<<"completion callback must remain alive when it replaces itself for the next capture phase\n";return 1;}
 if(!largeRetirement())return 1;
 std::cout<<"snapshot capture tests passed\n";
}
