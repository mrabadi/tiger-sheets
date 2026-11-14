#include "tigersheets/calculation_scheduler.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/chart.hpp"
#include "tigersheets/vba.hpp"
#include <QMetaObject>
#include <algorithm>
#include <charconv>
#include <map>
#include <cmath>
#include <QString>

namespace tigersheets {
namespace {
void prepareDisplayValues(const Json& book,Json& result,const std::function<bool()>& cancelled){
 result["displayValues"]=Json::array();
 if(result.value("status",std::string("ok"))!="ok"||!book.contains("sheets")||!result.contains("values"))return;
 const auto iteration=result.find("iteration");
 if(iteration!=result.end()&&iteration->is_object()&&!iteration->value("converged",true))return;
 struct Source{const Json* cells;std::size_t cursor=0;std::pair<int,int> previous{-1,-1};};
 std::map<std::string,Source> sheets;
 for(const auto& sheet:book.at("sheets"))sheets.emplace(sheet.at("id").get<std::string>(),Source{&sheet.at("cells")});
 auto coordinate=[](const Json& cell){return std::pair{cell.at("row").get<int>(),cell.at("col").get<int>()};};
 std::size_t visited=0;
 for(const auto& value:result.at("values")){
  if((visited++%256)==0&&cancelled())throw std::runtime_error("Calculation display preparation cancelled");
  const auto source=sheets.find(value.at("sheet").get<std::string>());
  bool formula=false,literal=false,represented=false;
  if(source!=sheets.end()){
   auto& state=source->second;const auto& cells=state.cells->get_ref<const Json::array_t&>();const auto point=coordinate(value);
   // Canonical source and engine results are ordered. Unordered custom evaluators
   // can rewind with binary search without creating a source-sized index.
   if(point<state.previous){const auto found=std::lower_bound(cells.begin(),cells.end(),point,[&](const Json& cell,const auto& key){return coordinate(cell)<key;});state.cursor=static_cast<std::size_t>(std::distance(cells.begin(),found));}
   else while(state.cursor<cells.size()&&coordinate(cells[state.cursor])<point)++state.cursor;
   state.previous=point;
   if(state.cursor<cells.size()&&coordinate(cells[state.cursor])==point){
    const auto& input=cells[state.cursor].at("input").get_ref<const std::string&>();
    formula=!input.empty()&&input[0]=='=';literal=!input.empty()&&!formula;
    const auto& payload=value.at("value");
    if(literal){
     if(payload.is_string())represented=(input[0]=='\''?input.substr(1):input)==payload.get_ref<const std::string&>();
     else if(payload.is_boolean())represented=input==(payload.get<bool>()?"TRUE":"FALSE");
     else if(payload.is_number()){
      double number=0;const auto parsed=std::from_chars(input.data(),input.data()+input.size(),number);
      bool okay=parsed.ec==std::errc()&&parsed.ptr==input.data()+input.size();
      if(!okay)number=QString::fromStdString(input).toDouble(&okay);
      represented=okay&&std::isfinite(number)&&number==payload.get<double>();
     }
    }
   }
  }
  if(formula||value.value("type",std::string())=="error"||(literal&&!represented)||(!literal&&!value.at("value").is_null()))result["displayValues"].push_back(value);
 }
}
void prepareCharts(const Json& book,Json& result,const std::function<bool()>& cancelled){
 result["preparedCharts"]=Json::object();if(result.value("status",std::string("ok"))!="ok"||!book.contains("charts"))return;const auto iteration=result.find("iteration");if(iteration!=result.end()&&iteration->is_object()&&!iteration->value("converged",true))return;
 for(const auto& chart:book.at("charts")){if(cancelled())throw std::runtime_error("Chart preparation cancelled");const auto id=chart.at("id").get<std::string>();try{result["preparedCharts"][id]={{"data",chartData(book,result,chart)}};}catch(const std::exception& error){result["preparedCharts"][id]={{"error",error.what()}};}}
}
Json calculate(const Json& book) {
 CalculationEngine engine;
 const auto vba = book.value("vba", Json::object());
 std::string source;
 for (const auto& module : vba.value("modules", Json::array()))
  source += module.value("source", std::string()) + "\n";
 if (vba.value("enabled", false) && !source.empty())
  return engine.calculate(book, [&](const std::string& name, const Json& args) {
   return evaluateVbaFunction(book, source, name, args);
  });
 return engine.calculate(book);
}
}

CalculationScheduler::CalculationScheduler(Evaluator evaluator, QObject* parent)
 : QObject(parent), worker_(new QObject), evaluator_(evaluator ? std::move(evaluator) : calculate) {
 worker_->moveToThread(&thread_);
 connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
 thread_.start();
}

CalculationScheduler::~CalculationScheduler() {
 cancel();
 thread_.quit();
 thread_.wait();
}

void CalculationScheduler::cancel() { ++generation_; }

void CalculationScheduler::request(Json workbook,std::function<Json(Json)> prepare) {
 const auto token = ++generation_;
 QMetaObject::invokeMethod(worker_, [this, token, book = std::move(workbook), prepare=std::move(prepare)]() mutable {
  if (token != generation_) return;
  Json result;
  try { if(prepare)book=prepare(std::move(book));if(token!=generation_)return;result = evaluator_(book);result["contextGeneration"]=book.value("_tigersheetsContextGeneration",Json());if(book.contains("_tigersheetsLinkDiagnostics"))for(const auto& diagnostic:book.at("_tigersheetsLinkDiagnostics"))result["diagnostics"].push_back(diagnostic); prepareDisplayValues(book,result,[&]{return token!=generation_;}); prepareCharts(book,result,[&]{return token!=generation_;}); }
  catch (const std::exception& error) {
   result = {{"revision", book.value("revision", std::uint64_t{})}, {"status", "error"},
             {"diagnostics", Json::array({error.what()})}, {"values", Json::array()}};
  }
  QMetaObject::invokeMethod(this, [this, token, result = std::move(result)]() mutable {
   if (token == generation_ && onResult) onResult(std::move(result));
  }, Qt::QueuedConnection);
 }, Qt::QueuedConnection);
}
}
