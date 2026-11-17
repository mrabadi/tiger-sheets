#pragma once
#include "tigersheets/calculation.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
namespace tigersheets {
// Four sections retain Accounting's fill, closing-parenthesis padding, zero
// placeholder width, and text handling in native Excel files.
inline std::string accountingNumberFormat(int decimals,std::string_view symbol="$",std::string_view locale="",bool symbolAfter=false){
 if(decimals<0||decimals>30)throw std::invalid_argument("Accounting precision must be between 0 and 30");
 if(symbol.size()>32||symbol.find_first_of(";[]\"\\*_\n\r")!=std::string_view::npos)throw std::invalid_argument("Invalid Accounting currency symbol");
 if(locale.size()>8||(!locale.empty()&&!std::all_of(locale.begin(),locale.end(),[](unsigned char ch){return std::isxdigit(ch);}))||(symbol.empty()&&!locale.empty()))throw std::invalid_argument("Invalid Accounting currency locale");
 std::string token;if(!symbol.empty())token=locale.empty()?"\""+std::string(symbol)+"\"":"[$"+std::string(symbol)+"-"+std::string(locale)+"]";
 const std::string numeric=std::string("#,##0")+(decimals?"."+std::string(decimals,'0'):"");
 if(symbolAfter&&!token.empty())return "_(* "+numeric+" "+token+"_);_(* \\("+numeric+"\\) "+token+";_(* \"-\""+std::string(decimals,'?')+" "+token+"_);_(@_)";
 const auto prefix="_("+token+"* ";return prefix+numeric+"_);"+prefix+"\\("+numeric+"\\);"+prefix+"\"-\""+std::string(decimals,'?')+"_);_(@_)";
}
// Display conversion never changes the underlying value or its XLSX/cache type.
inline bool dateNumberFormat(std::string_view format){
 std::string tokens;bool quoted=false;for(std::size_t i=0;i<format.size();++i){const char c=format[i];if(c=='"'){quoted=!quoted;continue;}if(quoted)continue;if(c=='\\'||c=='_'||c=='*'){if(i+1<format.size())++i;continue;}if(c=='['){while(i+1<format.size()&&format[i+1]!=']')++i;if(i+1<format.size())++i;continue;}tokens+=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));}
 return tokens.find_first_of("yd")!=std::string::npos||(tokens.find('m')!=std::string::npos&&tokens.find_first_of("hs")==std::string::npos);
}
inline std::string formattedValue(const Json& value,const Json& style=Json::object(),bool date1904=false){
 if(value.is_null())return {};if(value.is_string())return value.get<std::string>();if(value.is_boolean())return value.get<bool>()?"TRUE":"FALSE";
 if(value.is_number()){const auto format=style.value("numberFormat",std::string("General"));try{return formatCellValue(value.get<double>()+(date1904&&dateNumberFormat(format)?1462.0:0.0),format);}catch(const std::exception&){return "#FORMAT!";}}
 return {};
}
}
