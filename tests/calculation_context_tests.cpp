#include "tigersheets/calculation.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
 CalculationContext context;context.snapshot={{"revision",7},{"links",Json::array({{{"name","Assumptions.xlsx"},{"enabled",true},{"state","cached"}}})}};
 context.evaluator=[](const Json& book){return Json{{"revision",book.at("revision")},{"status","ok"},{"values",Json::array()}};};
 auto live=context;live.snapshot["links"][0]["state"]="open";require(live.calculate(live.snapshot).at("status")=="ok","Live open workbook inputs are fresh and do not require cached-value consent");
 bool refused=false;try{context.calculate(context.snapshot);}catch(const std::exception& e){refused=std::string(e.what()).find("saved")!=std::string::npos;}require(refused,"analysis requires explicit consent before using cached linked inputs");
 context.snapshot["links"][0]["enabled"]=false;refused=false;try{context.calculate(context.snapshot);}catch(const std::exception&){refused=true;}require(refused,"normalized disabled saved sources still require explicit analysis consent");
 context.allowCachedLinks=true;require(context.calculate(context.snapshot).at("status")=="ok","explicit saved-value permission remains functional");
 bool current=true;context.generation=17;context.generationIsCurrent=[&](std::uint64_t generation){return current&&generation==17;};
 context.evaluator=[&](const Json& book){current=false;return Json{{"revision",book.at("revision")},{"status","ok"},{"values",Json::array()}};};
 refused=false;try{context.calculate(context.snapshot);}catch(const std::exception&){refused=true;}require(refused,"A generation replaced during model evaluation must never return usable financial results");
 current=true;context.evaluator=[](const Json&){return Json{{"revision",8},{"status","ok"},{"values",Json::array()}};};
 refused=false;try{context.calculate(context.snapshot);}catch(const std::exception&){refused=true;}require(refused,"A foreign result revision must not escape a shared calculation context");
 std::cout<<"calculation context passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
