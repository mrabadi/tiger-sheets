#pragma once
#include "tigersheets/session.hpp"
#include <string>
#include <set>
#include <utility>
#include <vector>

namespace tigersheets {
// Temporarily excludes source arrays that a bounded native replacement cannot
// mutate. Existing transaction validation and history still operate on the rest
// of the workbook, then the original allocations are restored to the result.
class NativePreparationProjection {
public:
 NativePreparationProjection(Json& workbook,const Json& operations);
 struct HistoryPatch {};
 NativePreparationProjection(Json& workbook,const Json& changes,HistoryPatch);
 ~NativePreparationProjection();
 NativePreparationProjection(const NativePreparationProjection&)=delete;
 NativePreparationProjection& operator=(const NativePreparationProjection&)=delete;
 void restore(bool appliedHistory=false) noexcept;
private:
 void detach(const std::set<std::string>& sheets);
 Json& workbook_;
 std::vector<std::pair<std::string,Json>> cells_;
 std::vector<std::pair<std::string,Json>> metadata_;
};
}
