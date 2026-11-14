#include "tigersheets/calculation.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
using tigersheets::Json;
int main(int argc,char** argv){
 try{
  const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
  std::ifstream input(argc>1?std::filesystem::path(argv[1]):root/"tests/fixtures/formulas.json");if(!input)throw std::runtime_error("Cannot read formula reference fixtures");Json fixtures;input>>fixtures;
  Json report={{"schemaVersion",1},{"functions",Json::object()},{"cases",Json::array()}};tigersheets::CalculationEngine engine;int failed=0;
  const std::regex call("([A-Z][A-Z0-9_.]*)\\s*\\(");
  for(const auto& test:fixtures.at("cases")){
   Json cells=Json::array();for(const auto& c:test.value("cells",Json::array()))cells.push_back({{"row",c[0]},{"col",c[1]},{"input",c[2]}});
   const auto formula=test.at("formula").get<std::string>();cells.push_back({{"row",1000},{"col",1},{"input",formula}});
   Json snapshot={{"revision",1},{"sheets",Json::array({{{"id","reference"},{"name","Sheet1"},{"cells",cells}}})}};
   auto result=engine.calculate(snapshot);Json actual;std::string type;
   for(const auto& cell:result.value("values",Json::array()))if(cell["sheet"]=="reference"&&cell["row"]==1000&&cell["col"]==1){actual=cell["value"];type=cell.value("type",std::string());break;}
   const auto expected=test.at("expected");bool passed=false;
   if(expected.is_number()&&actual.is_number()){double e=expected.get<double>(),a=actual.get<double>(),t=test.value("tolerance",1e-9);passed=std::isfinite(a)&&std::abs(a-e)<=t*std::max(1.0,std::abs(e));}
   else passed=actual==expected;
   if(!passed){++failed;std::cerr<<test["id"]<<": "<<formula<<" expected "<<expected<<" but got "<<actual<<" ("<<type<<")\n";}
   auto id=test.at("id").get<std::string>();report["cases"].push_back({{"id",id},{"formula",formula},{"expected",expected},{"actual",actual},{"passed",passed}});
   for(auto i=std::sregex_iterator(formula.begin(),formula.end(),call);i!=std::sregex_iterator();++i){auto name=(*i)[1].str();auto& evidence=report["functions"][name];if(evidence.is_null())evidence={{"passed",true},{"test","formula_catalog_tests"},{"cases",Json::array()}};evidence["passed"]=evidence["passed"].get<bool>()&&passed;evidence["cases"].push_back(id);}
  }
  report["engine"]=engine.functions();report["passed"]=failed==0;
  if(argc>2){std::ofstream output(argv[2]);if(!output)throw std::runtime_error("Cannot write compatibility evidence");output<<report.dump(2)<<'\n';}
  std::cout<<fixtures["cases"].size()-static_cast<size_t>(failed)<<"/"<<fixtures["cases"].size()<<" formula reference cases passed\n";return failed?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}
}
