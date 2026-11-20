#pragma once
#include "tigersheets/session.hpp"
#include <set>
#include <string>
class QFormLayout;
class QItemSelectionRange;
namespace tigersheets {
class WorksheetModel;
struct WorksheetBorderDraft {
 Json changes=Json::object();
 bool clearAll=false;
 std::set<std::string> perimeterEdges;
 bool horizontalTouched=false,verticalTouched=false,horizontal=false,vertical=false;
 bool diagonalUpTouched=false,diagonalDownTouched=false,diagonalUp=false,diagonalDown=false;
 std::string color="#000000",style="thin";
 bool structured()const{return clearAll||!changes.empty()||!perimeterEdges.empty()||horizontalTouched||verticalTouched||diagonalUpTouched||diagonalDownTouched;}
};
void createWorksheetBorderControls(QFormLayout& layout,const Json& initial,int rows,int columns,WorksheetBorderDraft& draft);
Json worksheetBorderOperations(const WorksheetBorderDraft& draft,const WorksheetModel& model,const QItemSelectionRange& range);
}
