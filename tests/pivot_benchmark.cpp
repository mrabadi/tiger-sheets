#include "tigersheets/pivot.hpp"
#include <chrono>
#include <iostream>
#include <sys/resource.h>
using namespace tigersheets;
int main(){using clock=std::chrono::steady_clock;Json cells=Json::array();auto& array=cells.get_ref<Json::array_t&>();array.reserve(1000010);const std::vector<std::string> headers={"Region","Period","Product","Revenue","Cost","Units","Budget","TaxRate","CaseId","Department"};
 for(int col=1;col<=10;++col)cells.push_back({{"row",1},{"col",col},{"input",headers[col-1]}});
 double expected=0;for(int row=2;row<=100001;++row){const auto i=row-2;std::vector<std::string> values={"Region "+std::to_string(i%20),"2026-"+std::to_string(i%12+1),"Product "+std::to_string(i%100),std::to_string(1000+i%10000),std::to_string(500+i%5000),std::to_string(i%30+1),std::to_string(1100+i%9000),"0.25","Fictitious Case "+std::to_string(i),"Department "+std::to_string(i%8)};expected+=1000+i%10000;for(int col=1;col<=10;++col)cells.push_back({{"row",row},{"col",col},{"input",values[col-1]}});}
 Json book={{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","Transactions"},{"cells",std::move(cells)}},{{"id","r"},{"name","Report"},{"cells",Json::array()}}})}};
 Json definition={{"id","benchmark"},{"name","FinancialReport"},{"source",{{"sheet","s"},{"row1",1},{"col1",1},{"row2",100001},{"col2",10}}},{"rows",Json::array({{{"field","Region"}}})},{"columns",Json::array({{{"field","Period"}}})},{"values",Json::array({{{"field","Revenue"},{"aggregate","sum"},{"name","Revenue"}}})},{"output",{{"sheet","r"},{"row1",1},{"col1",1}}}};
 const auto start=clock::now();auto report=generatePivot(book,{{"revision",0},{"status","ok"},{"values",Json::array()}},definition);const auto ms=std::chrono::duration<double,std::milli>(clock::now()-start).count();struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
 const bool valid=report.definition["layout"]["grandTotals"][0]["value"]==expected&&report.definition["cache"]["records"].size()==100000;
 std::cout<<Json{{"records",100000},{"fields",10},{"sourceRange","A1:J100001"},{"generationMs",ms},{"peakRssKiB",usage.ru_maxrss},{"valid",valid},{"referenceMachineMatch",false}}.dump()<<'\n';return valid?0:1;
}
