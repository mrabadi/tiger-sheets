#pragma once
#include "tigersheets/session.hpp"
#include <functional>
namespace tigersheets {
Json runMacro(const Json& snapshot,std::string source,std::string procedure,bool enabled,std::function<bool()> cancelled={});
Json evaluateVbaFunction(const Json& snapshot,std::string source,std::string functionName,const Json& args,std::function<bool()> cancelled={});
}
