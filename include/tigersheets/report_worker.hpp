#pragma once
#include <QObject>
#include <QString>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
namespace tigersheets {
using Json=nlohmann::json;
// One isolated immutable report job. Serialization and result parsing run off
// the UI thread; only successful matching generations reach onResult.
class ReportWorker final:public QObject {
public:
 explicit ReportWorker(QObject* parent=nullptr);
 ~ReportWorker() override;
 bool start(std::shared_ptr<const Json> request,const QString& executable={});
 void stop();
 bool running() const;
 QString error() const;
 std::function<void(Json)> onResult;
 std::function<void(QString)> onProgress,onError;
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
