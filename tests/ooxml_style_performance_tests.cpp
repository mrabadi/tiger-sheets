#include "tigersheets/ooxml.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace tigersheets;
int main(){try{
 char pattern[]="/tmp/tigersheets-styled-save-XXXXXX";const auto* temporary=mkdtemp(pattern);if(!temporary)throw std::runtime_error("temporary styled-save fixture");const std::filesystem::path directory(temporary);struct Cleanup{std::filesystem::path directory;~Cleanup(){std::filesystem::remove_all(directory);}} cleanup{directory};
 Json cells=Json::array();for(int row=1;row<=2000;++row)for(int col=1;col<=10;++col)cells.push_back({{"row",row},{"col",col},{"input",std::to_string(row*10+col)},{"style",{{"numberFormat","0.00"},{"bold",true}}}});Json book={{"revision",0},{"sheets",Json::array({{{"id","data"},{"name","Data"},{"cells",std::move(cells)}}})}};const auto path=(directory/"styled.xlsx").string();const auto start=std::chrono::steady_clock::now();writeWorkbook(path,book);const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();std::cout<<"styled native save: "<<elapsed<<" ms for 20000 cells\n";if(elapsed>8000)throw std::runtime_error("20000 styled native cells must save within eight seconds; repeated whole-worksheet style targeting exceeds the budget");const auto loaded=readWorkbook(path);const auto& last=loaded.workbook.at("sheets")[0].at("cells").back();if(last.at("row")!=2000||last.at("col")!=10||last.at("input")!="20010"||!last.at("style").at("bold").get<bool>()||last.at("style").at("numberFormat")!="0.00")throw std::runtime_error("indexed style assignment must preserve the final native cell formatting");std::cout<<"styled native save test passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
