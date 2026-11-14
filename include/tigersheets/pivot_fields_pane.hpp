#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
namespace tigersheets {
using Json=nlohmann::json;
class PivotFieldsPane final:public QWidget {
public:
 explicit PivotFieldsPane(QWidget* parent=nullptr);
 ~PivotFieldsPane() override;
 void setDefinition(const Json& definition,const Json& sheets,const Json& fields);
 void setMemberResults(const QString& field,const Json& items,std::size_t total);
 void setStatus(const Json& status);
 Json definition() const;
 std::function<void(const Json&)> applyRequested;
 std::function<void(QString)> insertChartRequested;
 std::function<void()> deleteRequested;
 std::function<void(QString,QString)> sourceRequested;
 std::function<void(QString,QString)> memberSearchRequested;
protected:
 bool eventFilter(QObject* watched,QEvent* event) override;
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
