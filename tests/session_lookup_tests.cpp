#include "tigersheets/session.hpp"
#include <chrono>
#include <iostream>
using namespace tigersheets;
int main(){Json cells=Json::array();for(int row=1;row<=100000;++row)cells.push_back({{"row",row},{"col",1},{"input",std::to_string(row)}});WorkbookSession session({{"revision",0},{"sheets",Json::array({{{"id","s"},{"name","Source"},{"cells",std::move(cells)}}})}});const auto start=std::chrono::steady_clock::now();long long checksum=0;for(int i=0;i<10000;++i){const auto row=1+(i*7919)%100000;const auto* cell=session.cellAt("s",row,1);if(!cell||cell->at("row")!=row)return 1;checksum+=cell->at("row").get<int>();}const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();std::cout<<Json{{"lookups",10000},{"populatedCells",100000},{"lookupMs",ms},{"checksum",checksum}}.dump()<<'\n';if(ms>=100){std::cerr<<"sparse lookup must use random-access binary search, without linear iterator traversal\n";return 1;}return 0;}
