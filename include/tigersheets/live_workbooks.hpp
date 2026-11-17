#pragma once
#include "tigersheets/workbook_links.hpp"
#include <functional>
namespace tigersheets {
struct LiveWorkbookRegistration;
using LiveWorkbookCapture=std::function<LiveWorkbookSnapshot()>;
// The host retains this handle; the registry retains only a weak reference.
// Capture and notifications run on the caller's thread. UI hosts queue their
// invalidation callback onto their event loop and capture on the UI thread.
std::shared_ptr<LiveWorkbookRegistration> registerLiveWorkbook(LiveWorkbookCapture capture,std::function<void()> onOtherSourceChanged);
std::vector<LiveWorkbookSnapshot> captureLiveWorkbooks(const LiveWorkbookRegistration* exclude=nullptr);
void notifyLiveWorkbookChanged(const LiveWorkbookRegistration* source);
}
