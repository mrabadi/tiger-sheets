#pragma once
#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "tigersheets/calculation.hpp"
namespace tigersheets {
struct GoalSeekOptions {
 double tolerance=1e-9,relativeTolerance=0;
 int maxIterations=100,maxEvaluations=200;
 std::optional<double> lower,upper;
};
struct GoalSeekResult {std::string status;double value=0;double result=0;int evaluations=0;int iterations=0;double residual=0;std::string message;};
GoalSeekResult goalSeek(const std::function<double(double)>& model,double target,double guess,const GoalSeekOptions& options,const std::function<bool()>& cancelled={});
GoalSeekResult goalSeek(const std::function<double(double)>& model,double target,double guess,double tolerance=1e-9,int iterations=100,const std::function<bool()>& cancelled={});
Json normalizeGoalSeekSpecification(const Json& snapshot,const Json& specification);
Json goalSeekWorkbook(const Json& snapshot,const Json& specification,const CalculationContext* context=nullptr,const std::function<bool()>& cancelled={});
std::vector<std::vector<double>> sensitivityTable(const std::vector<double>& rowInputs,const std::vector<double>& columnInputs,const std::function<double(double,double)>& model);
nlohmann::json pivotRows(const nlohmann::json& records,const std::vector<std::string>& dimensions,const std::string& measure,const std::string& aggregate="sum");
}
