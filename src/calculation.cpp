#include "tigersheets/calculation.hpp"
#include "tigersheets/pivot.hpp"

#include <memory>
#include <optional>
#include <stdexcept>

namespace tigersheets {
bool CalculationContext::current() const { return !generationIsCurrent || generationIsCurrent(generation); }
Json CalculationContext::calculate(const Json& candidate) const {
    if (!current() || candidate.value("revision", Json()) != snapshot.value("revision", Json()))
        throw std::runtime_error("The calculation context changed while analysis was running. Run it again.");
    if(!allowCachedLinks)for(const auto& link:snapshot.value("links",Json::array())){const auto state=link.value("state",std::string());if(!state.empty()&&state!="retained"&&state!="refreshed")throw std::runtime_error("Linked inputs are saved or unavailable. Refresh the source, or explicitly allow saved linked values for this analysis.");}
    auto result = evaluator ? evaluator(candidate) : CalculationEngine().calculate(candidate);
    if (!current() || result.value("revision", Json()) != snapshot.value("revision", Json()))
        throw std::runtime_error("The calculation context or result revision changed during analysis. Run it again.");
    return result;
}
namespace {
std::optional<Json> unsupportedDateSystem(const Json& snapshot) {
    const auto settings = snapshot.find("settings");
    if (settings == snapshot.end() || !settings->is_object() || !settings->contains("date1904") ||
        !settings->at("date1904").is_boolean() || !settings->at("date1904").get<bool>()) return std::nullopt;
    return Json{{"revision", snapshot.value("revision", Json())}, {"status", "unsupported_date_system"},
        {"values", Json::array()}, {"diagnostics", Json::array({{{"code", "unsupported_date_system"},
            {"message", "Calculation of workbooks using the 1904 date system is not supported; no calculated values were published."}}})}};
}
struct FunctionContext {
    const Json& snapshot;
    const FunctionEvaluator& evaluator;
    std::string response;
};
const char* evaluateFunction(void* context, const char* request) noexcept {
    auto& call = *static_cast<FunctionContext*>(context);
    try {
        const auto parsed = Json::parse(request);
        if(parsed.at("name")=="__TIGERSHEETS_GETPIVOTDATA") {
            const auto& args=parsed.at("args");
            const auto value=getPivotData(call.snapshot,args.at("sheet").get<std::string>(),args.at("row").get<int>(),args.at("col").get<int>(),args.at("measure").get<std::string>(),args.at("fieldItems"));
            call.response=(value.value("type",std::string())=="error"?
                Json{{"status","error"},{"code",value.at("value")}}:Json{{"status","completed"},{"value",value.at("value")}}).dump();
        } else call.response = call.evaluator(parsed.at("name").get<std::string>(), parsed.at("args")).dump();
    } catch (const std::exception& error) {
        call.response = Json{{"status", "error"}, {"diagnostics", {error.what()}}}.dump();
    } catch (...) {
        call.response = R"({"status":"error","diagnostics":["Worksheet function failed"]})";
    }
    return call.response.c_str();
}
}
CalculationEngine::CalculationEngine() : handle_(tigersheets_calc_new()) {
    if (tigersheets_calc_abi_version() != 1 || !handle_)
        throw std::runtime_error("Incompatible TigerSheets calculation engine");
}

CalculationEngine::~CalculationEngine() { tigersheets_calc_destroy(handle_); }

void CalculationEngine::cancel() const { tigersheets_calc_cancel(handle_); }
void CalculationEngine::resetCancellation() const { tigersheets_calc_reset_cancel(handle_); }

Json CalculationEngine::calculate(const Json& snapshot) const {
    return calculate(snapshot,FunctionEvaluator{});
}

Json CalculationEngine::calculate(const Json& snapshot, const FunctionEvaluator& evaluator) const {
    if (auto error = unsupportedDateSystem(snapshot)) return *error;
    const auto input = snapshot.dump();
    FunctionContext context{snapshot,evaluator, {}};
    std::unique_ptr<char, decltype(&tigersheets_calc_free)> response(
        tigersheets_calc_calculate_with_references(handle_, input.c_str(),evaluator?evaluateFunction:nullptr,evaluateFunction,&context),
        &tigersheets_calc_free);
    if (!response) throw std::runtime_error("Calculation engine returned no result");
    return Json::parse(response.get());
}

Json CalculationEngine::functions() const {
    std::unique_ptr<char, decltype(&tigersheets_calc_free)> response(
        tigersheets_calc_functions(), &tigersheets_calc_free);
    if (!response) throw std::runtime_error("Calculation engine returned no function catalog");
    return Json::parse(response.get());
}
Json CalculationEngine::qualifyAffine(const Json& snapshot,const Json& variables,const Json& outputs) const {
    const auto input=snapshot.dump(),query=Json{{"variables",variables},{"outputs",outputs}}.dump();
    std::unique_ptr<char,decltype(&tigersheets_calc_free)> response(tigersheets_calc_qualify_affine(handle_,input.c_str(),query.c_str()),&tigersheets_calc_free);
    if(!response)throw std::runtime_error("Linear qualification returned no result");
    return Json::parse(response.get());
}
} // namespace tigersheets
