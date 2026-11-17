#include "tigersheets/live_workbooks.hpp"
#include <algorithm>
#include <mutex>
namespace tigersheets {
struct LiveWorkbookRegistration {
 LiveWorkbookCapture capture;
 std::function<void()> changed;
 ~LiveWorkbookRegistration(){notifyLiveWorkbookChanged(this);}
};
namespace {
struct Registry {std::mutex mutex;std::vector<std::weak_ptr<LiveWorkbookRegistration>> sources;};
Registry& registry(){static Registry value;return value;}
std::vector<std::shared_ptr<LiveWorkbookRegistration>> registrations(const LiveWorkbookRegistration* exclude){
 auto& value=registry();std::lock_guard lock(value.mutex);std::vector<std::shared_ptr<LiveWorkbookRegistration>> result;
 std::erase_if(value.sources,[](const auto& source){return source.expired();});
 for(const auto& source:value.sources)if(auto held=source.lock();held&&held.get()!=exclude)result.push_back(std::move(held));return result;
}
}
std::shared_ptr<LiveWorkbookRegistration> registerLiveWorkbook(LiveWorkbookCapture capture,std::function<void()> changed){
 auto result=std::make_shared<LiveWorkbookRegistration>();result->capture=std::move(capture);result->changed=std::move(changed);
 {auto& value=registry();std::lock_guard lock(value.mutex);value.sources.push_back(result);}
 notifyLiveWorkbookChanged(result.get());return result;
}
std::vector<LiveWorkbookSnapshot> captureLiveWorkbooks(const LiveWorkbookRegistration* exclude){
 std::vector<LiveWorkbookSnapshot> result;for(const auto& source:registrations(exclude))if(source->capture){auto snapshot=source->capture();if(snapshot.workbook)result.push_back(std::move(snapshot));}return result;
}
void notifyLiveWorkbookChanged(const LiveWorkbookRegistration* source){for(const auto& registered:registrations(source))if(registered->changed)registered->changed();}
}
