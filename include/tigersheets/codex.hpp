#pragma once
#include <QObject>
#include <QString>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
namespace tigersheets {
using Json=nlohmann::json;
// Optional, event-driven app-server connection. Callbacks run on this object's Qt thread.
// Workbook operations are delegated to the host; this client never accepts or saves edits.
class CodexClient final:public QObject {
public:
 explicit CodexClient(QObject* parent=nullptr);
 ~CodexClient() override;
 std::function<void(QString)> onText,onStatus,onError;
 std::function<void()> onModelsChanged;
 std::function<Json(const QString&,const Json&)> onTool;
 std::function<void(QString,Json,std::function<void(Json)>)> onToolAsync;
 std::function<void()> onCancelTools;
 void connectRuntime(QString executable=QStringLiteral("codex"));
 void disconnectRuntime();
 void send(QString prompt,const Json& workbook);
 void cancel();
 void resetWorkbook();
 bool isReady() const;
 bool isBusy() const;
 Json models() const;
 void setModel(QString model);
 QString model() const;
 void setReasoningEffort(QString effort);
 QString reasoningEffort() const;
 void setServiceTier(QString tier);
 QString serviceTier() const;
 static Json toolDefinitions();
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
