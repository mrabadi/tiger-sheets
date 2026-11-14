#pragma once
#include "tigersheets/calculation.hpp"
#include <cctype>
#include <string_view>
namespace tigersheets {
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
