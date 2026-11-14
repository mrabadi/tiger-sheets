#include "tigersheets/worksheet_layout.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 // Independent documented example, not a restatement of the implementation:
 // https://learn.microsoft.com/en-us/dotnet/api/documentformat.openxml.spreadsheet.column?view=openxml-3.0.1
 require(spreadsheetColumnWidth(8.0,7)==8.7109375,"eight characters encode as native width 8.7109375 at maximum digit width seven");
 require(spreadsheetColumnPixels(8.7109375,7)==61,"documented native width renders 61 pixels without adding padding twice");
 require(spreadsheetColumnCharacters(8.7109375,7)==8.0,"61-pixel native width displays eight characters in Column Width");
 require(spreadsheetColumnWidth(0.0,7)==0.0&&spreadsheetColumnPixels(0.0,7)==0&&spreadsheetColumnCharacters(0.0,7)==0.0,"zero-width columns stay hidden through every conversion");
 const auto rejects=[](auto operation){try{operation();return false;}catch(const std::exception&){return true;}};
 for(const double invalid:{-1.0,255.001,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){
  require(rejects([&]{spreadsheetColumnWidth(invalid,7);}),"invalid character counts reject before authoring native widths");
  require(rejects([&]{spreadsheetColumnPixels(invalid,7);}),"invalid native widths reject before rendering pixels");
  require(rejects([&]{spreadsheetColumnCharacters(invalid,7);}),"invalid native widths reject before displaying character counts");
 }
 for(const int invalid:{0,-1}){require(rejects([&]{spreadsheetColumnWidth(8.0,invalid);}),"width conversion requires positive maximum digit width");require(rejects([&]{spreadsheetColumnPixels(8.7109375,invalid);}),"pixel conversion requires positive maximum digit width");require(rejects([&]{spreadsheetColumnCharacters(8.7109375,invalid);}),"character conversion requires positive maximum digit width");}
 require(rejects([&]{spreadsheetColumnPixels(255.0,std::numeric_limits<int>::max());}),"pixel conversion cannot overflow its integer result");
 require(spreadsheetColumnWidth(8.0,10)==8.5&&spreadsheetColumnPixels(8.5,10)==85&&spreadsheetColumnCharacters(8.5,10)==8.0,"different normal-font digit widths preserve the character count");
 // Office's raw width attribute is bounded at255, independently of UI count.
 require(spreadsheetColumnWidth(255.0,7)==255.0&&spreadsheetColumnPixels(255.0,7)==1785&&spreadsheetColumnCharacters(255.0,7)==254.29,"authoring caps raw width at255 and exposes the resulting character limit");
 std::cout<<"Worksheet column width tests passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
