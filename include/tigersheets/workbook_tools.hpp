#pragma once
#include "tigersheets/session.hpp"
#include "tigersheets/calculation.hpp"
#include <functional>

namespace tigersheets {
// The desktop owns acceptance and saving. This bridge owns one isolated proposal.
class WorkbookTools {
public:
 struct ProposalState {std::uint64_t baseRevision=0;std::shared_ptr<const Json> operations;};
 WorkbookTools(WorkbookSession& session,Json availableFunctions);
 Json invoke(std::string tool,const Json& arguments);
 std::function<void(const Json&)> onPreview;
 std::function<Json(const Json&)> calculate;
 // Trusted host prepares frozen links and source epochs before financial work.
 std::function<CalculationContext(const Json&)> financialContext;
 std::function<bool(std::uint64_t)> calculationContextIsCurrent;
 static Json definitions();
 ProposalState proposalState() const noexcept;
 bool adoptProposal(ProposalState state,std::string* error=nullptr);
 void reset();
private:
 WorkbookSession& session_;
 Json availableFunctions_;
 std::shared_ptr<const Json> operations_=std::make_shared<const Json>(Json::array());
 bool pending_=false;
 std::uint64_t base_{};
};
}
