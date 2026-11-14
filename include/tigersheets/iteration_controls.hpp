#pragma once
#include <QWidget>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
namespace tigersheets {
using Json=nlohmann::json;
// Draft iteration settings. Only the host can publish the reviewed callback.
class IterationControlsWidget final:public QWidget {
public:
 explicit IterationControlsWidget(QWidget* parent=nullptr);
 ~IterationControlsWidget() override;
 void setSettings(const Json& settings);
 Json settings()const;
 std::function<void(const Json&)> applyRequested;
 std::function<void()> circularExampleRequested;
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
