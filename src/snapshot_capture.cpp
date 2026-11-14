#include "tigersheets/snapshot_capture.hpp"
#include <QElapsedTimer>
#include <QTimer>
#include <QThread>
#include <algorithm>
#include <vector>
namespace tigersheets {
namespace {
std::shared_ptr<Json> capturedRoot(){return {new Json,[](Json* value){auto* thread=QThread::create([value]{delete value;});QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();}};}
}
struct SnapshotCapture::State {
 struct Frame {const Json* source;Json* destination;Json::const_iterator next;bool initialized=false;};
 std::vector<Frame> stack;std::shared_ptr<Json> result;std::function<bool()> current;std::size_t limit;std::uint64_t generation=0;bool busy=false;
};
SnapshotCapture::SnapshotCapture(QObject* parent,std::size_t limit):QObject(parent),state_(std::make_unique<State>()){state_->limit=std::max<std::size_t>(1,limit);}
SnapshotCapture::~SnapshotCapture(){stop();}
bool SnapshotCapture::running()const{return state_->busy;}
void SnapshotCapture::stop(){auto& state=*state_;++state.generation;state.busy=false;state.stack.clear();state.result.reset();state.current={};}
bool SnapshotCapture::start(const Json& source,std::function<bool()> current){
 if(running())return false;if(current&&!current()){if(failed)failed("The workbook changed while preparing the report; run it again.");return false;}auto& state=*state_;state.result=capturedRoot();state.current=std::move(current);state.stack.push_back({&source,state.result.get(),{},false});state.busy=true;const auto generation=++state.generation;
 QTimer::singleShot(0,this,[this,generation]{if(state_->generation==generation)step();});return true;
}
void SnapshotCapture::step(){
 auto& state=*state_;if(!state.busy)return;
 if(state.current&&!state.current()){stop();if(failed)failed("The workbook changed while preparing the report; run it again.");return;}
 try{
  QElapsedTimer timer;timer.start();std::size_t nodes=0;
  while(!state.stack.empty()&&nodes<state.limit&&timer.elapsed()<4){
   auto& frame=state.stack.back();++nodes;
   if(!frame.initialized){
    if(!frame.source->is_structured()){*frame.destination=*frame.source;state.stack.pop_back();continue;}
    *frame.destination=frame.source->is_array()?Json::array():Json::object();if(frame.source->is_array())frame.destination->get_ref<Json::array_t&>().reserve(frame.source->size());frame.next=frame.source->cbegin();frame.initialized=true;continue;
   }
   if(frame.next==frame.source->cend()){state.stack.pop_back();continue;}
   const auto* source=&frame.next.value();Json* destination;
   if(frame.source->is_object())destination=&(*frame.destination)[frame.next.key()];else{frame.destination->push_back(nullptr);destination=&frame.destination->back();}
   ++frame.next;state.stack.push_back({source,destination,{},false});
  }
  if(state.stack.empty()){auto result=std::move(state.result);state.busy=false;state.current={};auto callback=completed;if(callback)callback(std::move(result));return;}
  const auto generation=state.generation;QTimer::singleShot(0,this,[this,generation]{if(state_->generation==generation)step();});
 }catch(const std::exception& error){const auto message=QString::fromUtf8(error.what());stop();if(failed)failed(message);}
}
}
