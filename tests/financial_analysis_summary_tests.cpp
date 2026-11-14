#include "tigersheets/financial_workspace.hpp"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
 const auto goal=financialAnalysisSummary({{"status","converged"},{"success",true},{"value",114.0},{"result",40.0},{"residual",0.000000001},{"evaluations",7}});require(goal.contains("converged")&&goal.contains("114")&&goal.contains("40")&&goal.contains("1e-09")&&goal.contains("7"),"Converged Goal Seek summary retains input, result, residual and evaluation evidence");
 const auto solver=financialAnalysisSummary({{"status","optimal"},{"objective",127.5},{"evaluations",12},{"changingCells",Json::array({{{"sheet","m"},{"row",2},{"col",3},{"value",24.0}}})}});require(solver.contains("127.5")&&solver.contains("24")&&solver.contains("12"),"Solver summary retains its objective and proposed changing input");
 const auto failure=financialAnalysisSummary({{"status","calculation_error"},{"message","Could not solve"},{"diagnostics",Json::array({{{"message","Earlier diagnostic"}},{{"code","iteration_not_converged"},{"message","Circular financing did not converge"}}})}});require(failure.contains("calculation_error")&&failure.contains("Circular financing did not converge"),"Failure summaries preserve the final calculation diagnostic");
 const auto cached=financialAnalysisSummary({{"status","generated"},{"provenance",{{"allowCachedLinks",true},{"links",Json::array({{{"name","Assumptions.xlsx"},{"state","cached"}}})}}}});require(cached.contains("cached",Qt::CaseInsensitive)&&cached.contains("Assumptions.xlsx"),"Saved linked-source use is visible in the review summary");
 const auto bounded=financialAnalysisSummary({{"status","error"},{"diagnostics",Json::array({{{"message",std::string(10000,'x')}}})}});require(bounded.size()<=1024,"Analysis review summaries stay bounded");require(financialAnalysisSummary(Json::object()).contains("unknown",Qt::CaseInsensitive),"Absent analysis status has an explicit fallback");std::cout<<"financial analysis summary tests passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
