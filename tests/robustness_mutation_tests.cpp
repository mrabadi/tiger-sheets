#include "tigersheets/ooxml.hpp"
#include "tigersheets/vba.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <unistd.h>
using namespace tigersheets;
int main(){
 char directory[]="/tmp/tigersheets-mutations-XXXXXX";if(!mkdtemp(directory))return 2;
 const auto path=(std::filesystem::path(directory)/"input.xlsx").string();
 try{
  WorkbookSession workbook;writeWorkbook(path,workbook.snapshot());std::ifstream input(path,std::ios::binary);const std::string baseline((std::istreambuf_iterator<char>(input)),{});input.close();
  std::mt19937 random(426);int accepted=0,rejected=0;
  for(int sample=0;sample<200;++sample){auto bytes=baseline;if(sample%4==0)bytes.resize(random()%bytes.size());else for(int edit=0;edit<1+sample%5;++edit)bytes[random()%bytes.size()]=static_cast<char>(random());std::ofstream output(path,std::ios::binary|std::ios::trunc);output.write(bytes.data(),bytes.size());output.close();try{const auto package=readWorkbook(path);WorkbookSession checked(package.workbook);++accepted;}catch(const std::exception&){++rejected;}}
  if(rejected==0)throw std::runtime_error("Mutation corpus did not exercise corrupt-package refusal");
  for(int sample=0;sample<200;++sample){std::string source="Sub Fuzz()\n";for(int i=0;i<128;++i)source+=static_cast<char>(32+random()%95);source+="\nEnd Sub";int ticks=0;const auto result=runMacro(workbook.snapshot(),source,"Fuzz",true,[&]{return ++ticks>1000;});if(result.at("status")!="completed"&&result.at("workbook")!=workbook.snapshot())throw std::runtime_error("Failed mutated macro leaked workbook changes");}
  std::filesystem::remove_all(directory);std::cout<<"200 package mutations ("<<accepted<<" readable, "<<rejected<<" rejected), 200 VBA mutations completed\n";
 }catch(const std::exception& error){std::filesystem::remove_all(directory);std::cerr<<error.what()<<'\n';return 1;}
}
