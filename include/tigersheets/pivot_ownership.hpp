#pragma once
#include "tigersheets/session.hpp"
namespace tigersheets {
void validatePivotEdit(const Json& before,const Json& operation);
void invalidatePivots(Json& after,const Json& operation);
void applyPivotTransaction(Json& candidate,const Json& operation);
void validatePivots(const Json& workbook);
}
