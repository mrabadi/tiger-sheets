#pragma once
#include <QObject>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
namespace tigersheets {
class WorksheetModel;
struct SelectionTotals {double sum=0;std::uint64_t count=0,populated=0;std::string numberFormat="General";};
// GUI-thread cooperative scan: no workbook copy or blank rectangle enumeration.
class SelectionStatistics : public QObject {
public:
 explicit SelectionStatistics(QObject* parent=nullptr);
 ~SelectionStatistics() override;
 void start(WorksheetModel& model,int top,int left,int bottom,int right,
            std::function<bool()> current,std::function<void(SelectionTotals)> complete);
 void cancel();
private:
 struct Job;
 std::shared_ptr<Job> job_;
 void advance(const std::shared_ptr<Job>& job);
};
}
