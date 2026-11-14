#include "tigersheets/vba.hpp"
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 WorkbookSession session;const auto original=session.snapshot();
 const auto disabled=runMacro(original,"Sub Forecast()\nRange(\"A1\").Value = 42\nEnd Sub","Forecast",false);
 require(disabled["status"]=="disabled"&&disabled["workbook"]==original,"disabled macro cannot execute");
 const auto result=runMacro(original,"Sub Forecast()\nDim amount As Double\namount = 21 * 2\nRange(\"A1\").Value = amount\nRange(\"B1\").Formula = \"=A1*2\"\nEnd Sub","Forecast",true);
 require(result["status"]=="completed","native macro completes");
 require(result["workbook"]["sheets"][0]["cells"][0]["input"]=="42","VBA variable arithmetic writes cell");
 require(result["workbook"]["sheets"][0]["cells"][1]["input"]=="=A1*2","VBA formula writes cell");
 require(original==session.snapshot(),"macro evaluation isolates live workbook");
 const auto loop=runMacro(original,"Sub Budget()\nDim i As Long\nFor i = 1 To 4\nIf i > 2 Then\nCells(i,1).Value = i * 10\nElse\nCells(i,1).Value = i\nEnd If\nNext i\nEnd Sub","Budget",true);
 require(loop["status"]=="completed","For and If native control flow");require(loop["workbook"]["sheets"][0]["cells"][2]["input"]=="30","VBA condition chooses true branch");
 const auto exited=runMacro(original,"Sub ExitNested()\nFor i = 1 To 3\nIf True Then\nExit Sub\nEnd If\nRange(\"A1\").Value = 98\nNext i\nRange(\"A1\").Value = 99\nEnd Sub","ExitNested",true);require(exited["status"]=="completed"&&exited["workbook"]==original,"Exit Sub inside nested If and For exits the entire procedure");
 const auto failed=runMacro(original,"Sub Broken()\nRange(\"A1\").Value = 9\nShell \"touch /tmp/forbidden\"\nEnd Sub","Broken",true);require(failed["status"]=="error"&&failed["workbook"]==original,"unsupported statement rolls back entire macro");
 int checks=0;const auto cancelled=runMacro(original,"Sub StopMe()\nFor i = 1 To 100\nCells(i,1).Value = i\nNext i\nEnd Sub","StopMe",true,[&]{return ++checks>3;});require(cancelled["status"]=="cancelled"&&cancelled["workbook"]==original,"cancel rolls back whole branch");
 const auto udf=evaluateVbaFunction(original,"Function Interest(principal As Double, rate As Double) As Double\nInterest = principal * rate\nEnd Function","Interest",Json::array({1000,0.05}));require(udf["status"]=="completed"&&udf["value"]==50,"read-only VBA scalar UDF");
 const auto mutating=evaluateVbaFunction(original,"Function Bad(x As Double) As Double\nRange(\"A1\").Value = x\nBad = x\nEnd Function","Bad",Json::array({9}));require(mutating["status"]=="error","worksheet UDF cannot mutate cells");
 const auto invalidName=runMacro(original,"Sub Forecast()\nRange(\"A1\").Value = 99\nEnd Sub",".*",true);require(invalidName["status"]=="error"&&invalidName["workbook"]==original,"macro entry point must be an exact VBA identifier");
 const auto integers=runMacro(original,"Sub Integers()\nRange(\"A1\").Value = CInt(1.8)\nRange(\"B1\").Value = CInt(2.5)\nRange(\"C1\").Value = CInt(3.5)\nRange(\"D1\").Value = CInt(-2.5)\nRange(\"E1\").Value = CInt(-1.8)\nEnd Sub","Integers",true);require(integers["status"]=="completed"&&integers["workbook"]["sheets"][0]["cells"][0]["input"]=="2"&&integers["workbook"]["sheets"][0]["cells"][1]["input"]=="2"&&integers["workbook"]["sheets"][0]["cells"][2]["input"]=="4"&&integers["workbook"]["sheets"][0]["cells"][3]["input"]=="-2"&&integers["workbook"]["sheets"][0]["cells"][4]["input"]=="-2","CInt rounds to nearest integer with even ties");const auto overflow=runMacro(original,"Sub Overflow()\nRange(\"A1\").Value = 9\nRange(\"B1\").Value = CInt(32767.5)\nEnd Sub","Overflow",true);require(overflow["status"]=="error"&&overflow["workbook"]==original,"CInt overflow rolls back the whole macro");
 const auto precedence=runMacro(original,"Sub Powers()\nRange(\"A1\").Value = -2 ^ 2\nRange(\"B1\").Value = (-2) ^ 2\nRange(\"C1\").Value = 2 ^ -2\nEnd Sub","Powers",true);require(precedence["status"]=="completed"&&precedence["workbook"]["sheets"][0]["cells"][0]["input"]=="-4"&&precedence["workbook"]["sheets"][0]["cells"][1]["input"]=="4"&&precedence["workbook"]["sheets"][0]["cells"][2]["input"]=="0.25","VBA exponentiation binds before unary signs and accepts signed exponents");
 std::cout<<"vba tests passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
