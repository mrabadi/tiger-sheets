#include "tigersheets/calculation.hpp"
#include <memory>
#include <stdexcept>
namespace tigersheets {
std::string formatCellValue(double value,const std::string& format,const std::string& locale) {
    std::unique_ptr<char,decltype(&tigersheets_calc_free)> response(tigersheets_calc_format_number(value,format.c_str(),locale.c_str()),&tigersheets_calc_free);
    if(!response)throw std::runtime_error("Number formatter returned no result");
    const auto result=Json::parse(response.get());
    if(result.value("status",std::string())!="ok")throw std::runtime_error(result.value("message",std::string("Unsupported number format")));
    return result.at("text").get<std::string>();
}
}
