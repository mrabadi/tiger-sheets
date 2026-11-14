#pragma once
#include "tigersheets/session.hpp"
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
namespace tigersheets {
// Bounded review text retaining numerical evidence and source-use diagnostics.
QString financialAnalysisSummary(const Json& analysis);
// Owns immutable capture, isolated computation and reviewable publication.
class FinancialWorkspace final:public QObject {
public:
 FinancialWorkspace(WorkbookSession& session,QObject* parent=nullptr);
 ~FinancialWorkspace() override;
 bool start(const Json& definition,const std::string& action,bool allowCachedLinks=false);
 void cancel();
 bool running() const;
 std::function<std::pair<std::string,std::uint64_t>()> epoch;
 std::function<void(Json&)> prepareRequest;
 std::function<const Json&()> retainedLinks;
 std::function<bool()> mayStart;
 std::function<void()> onStarting,onPreview;
 std::function<void(const Json&)> onAnalysis;
 std::function<void(QString)> onError,onProgress;
 QString executable;
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
