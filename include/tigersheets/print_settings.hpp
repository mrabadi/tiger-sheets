#pragma once
#include "tigersheets/session.hpp"
#include <stdexcept>
namespace tigersheets {
inline void validatePrintSettings(const Json& settings){
 if(!settings.is_object())throw std::runtime_error("Print settings must be an object");
 auto integer=[](const Json& value,int low,int high){if(!value.is_number_integer())throw std::runtime_error("Print coordinates must be integers");const auto number=value.get<std::int64_t>();if(number<low||number>high)throw std::runtime_error("Print coordinates exceed worksheet bounds");return static_cast<int>(number);};
 if(settings.contains("area")){const auto& area=settings.at("area");if(!area.is_object())throw std::runtime_error("Print area must be a rectangular cell range");const auto r1=integer(area.at("row1"),1,1048576),r2=integer(area.at("row2"),1,1048576),c1=integer(area.at("col1"),1,16384),c2=integer(area.at("col2"),1,16384);if(r2<r1||c2<c1)throw std::runtime_error("Print area is reversed");}
 if(settings.contains("repeatHeaderRows")){const auto& rows=settings.at("repeatHeaderRows");if(!rows.is_object())throw std::runtime_error("Repeated headers must be a row range");const auto first=integer(rows.at("row1"),1,1048576),last=integer(rows.at("row2"),1,1048576);if(last<first||last-first>=100)throw std::runtime_error("Repeat between one and 100 header rows");}
 if(settings.contains("fitWidth")&&!settings.at("fitWidth").is_boolean())throw std::runtime_error("Fit width must be Boolean");
 if(settings.contains("orientation")&&settings.at("orientation")!="portrait"&&settings.at("orientation")!="landscape")throw std::runtime_error("Print orientation must be portrait or landscape");
}
}
