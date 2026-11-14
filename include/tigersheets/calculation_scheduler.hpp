#pragma once
#include <QObject>
#include <QThread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

namespace tigersheets {
using Json = nlohmann::json;
class CalculationScheduler final : public QObject {
public:
 using Evaluator = std::function<Json(const Json&)>;
 explicit CalculationScheduler(Evaluator evaluator = {}, QObject* parent = nullptr);
 ~CalculationScheduler() override;
 void request(Json workbook,std::function<Json(Json)> prepare = {});
 void cancel();
 std::function<void(Json)> onResult;
private:
 QThread thread_;
 QObject* worker_;
 Evaluator evaluator_;
 std::atomic<std::uint64_t> generation_{0};
};
}
