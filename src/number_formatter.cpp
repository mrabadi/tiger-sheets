#include "tigersheets/calculation.hpp"
#include <memory>
#include <stdexcept>
namespace tigersheets {
FormattedNumber formatCellNumber(double value,const std::string& format,const std::string& locale) {
    std::unique_ptr<char,decltype(&tigersheets_calc_free)> response(tigersheets_calc_format_number(value,format.c_str(),locale.c_str()),&tigersheets_calc_free);
    if(!response)throw std::runtime_error("Number formatter returned no result");
    const auto result=Json::parse(response.get());
    if(result.value("status",std::string())!="ok")throw std::runtime_error(result.value("message",std::string("Unsupported number format")));
    FormattedNumber formatted{result.at("text").get<std::string>(),std::nullopt};
    if(result.contains("color")&&result.at("color").is_number_integer()&&result.at("color").get<int>()>0)formatted.colorIndex=static_cast<unsigned>(result.at("color").get<int>());
    return formatted;
}
std::string formatCellValue(double value,const std::string& format,const std::string& locale) {
    return formatCellNumber(value,format,locale).text;
}
}
