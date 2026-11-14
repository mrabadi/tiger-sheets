#pragma once
#include "tigersheets/session.hpp"
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
namespace tigersheets {
// Copies immutable job inputs in bounded event-loop slices. Any source revision
// change aborts before the next source access, leaving the live workbook alone.
class SnapshotCapture final:public QObject {
public:
 explicit SnapshotCapture(QObject* parent=nullptr,std::size_t nodesPerSlice=512);
 ~SnapshotCapture() override;
 bool start(const Json& source,std::function<bool()> current);
 void stop();
 bool running() const;
 std::function<void(std::shared_ptr<Json>)> completed;
 std::function<void(QString)> failed;
private:
 struct State;std::unique_ptr<State> state_;
 void step();
};
}
