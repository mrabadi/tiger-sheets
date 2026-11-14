#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
namespace tigersheets {
using Json=nlohmann::json;
Json defaultFinancialDefinition(const std::string& kind,const std::string& sheet,
 const Json& selection=Json::object(),const Json& definitions=Json::array());
// Authoring and review only. The host owns isolated computation and transactions.
class FinancialModelPane final:public QWidget {
public:
 explicit FinancialModelPane(QWidget* parent=nullptr);
 ~FinancialModelPane() override;
 void setWorkbookContext(const Json& sheets,const QString& activeSheet,
  const Json& selection=Json::object(),const Json& definitions=Json::array());
 void setDefinition(const Json& definition);
 Json definition() const;
 void setIterationSettings(const Json& settings);
 bool allowCachedLinks() const;
 void resetCachedLinkConsent();
 void setRunState(const QString& state,const QString& detail={},bool canAccept=false);
 std::function<void(const Json&)> saveRequested,runRequested,settingsRequested;
 std::function<void(const std::string&)> deleteRequested;
 std::function<void()> acceptRequested,discardRequested,cancelRequested,exampleRequested;
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
