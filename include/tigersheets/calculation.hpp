#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
#include "tigersheets/calc_bridge.h"

namespace tigersheets {

using Json = nlohmann::json;
using FunctionEvaluator = std::function<Json(const std::string&, const Json&)>;
struct FormattedNumber {
    std::string text;
    std::optional<unsigned> colorIndex;
};
FormattedNumber formatCellNumber(double value,const std::string& format,const std::string& locale="en");
std::string formatCellValue(double value,const std::string& format,const std::string& locale="en");

// Immutable prepared inputs for one calculation/analysis generation. The host
// injects enabled worksheet functions and tracks retained-link replacements.
struct CalculationContext {
    Json snapshot = Json::object();
    std::uint64_t generation = 0;
    bool allowCachedLinks = false;
    std::function<Json(const Json&)> evaluator;
    std::function<bool(std::uint64_t)> generationIsCurrent;
    Json calculate(const Json& candidate) const;
    bool current() const;
};

class CalculationEngine {
public:
    CalculationEngine();
    ~CalculationEngine();
    CalculationEngine(const CalculationEngine&) = delete;
    CalculationEngine& operator=(const CalculationEngine&) = delete;
    Json calculate(const Json& snapshot) const;
    Json calculate(const Json& snapshot, const FunctionEvaluator& evaluator) const;
    Json functions() const;
    Json qualifyAffine(const Json& snapshot,const Json& variables,const Json& outputs) const;
    void cancel() const;
    void resetCancellation() const;
private:
    TigerSheetsCalcHandle* handle_;
};

} // namespace tigersheets
