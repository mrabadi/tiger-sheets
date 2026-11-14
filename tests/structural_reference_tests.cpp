#include "tigersheets/references.hpp"
#include <cstdlib>
#include <iostream>
void require(bool okay,const char* message){if(!okay){std::cerr<<"FAILED: "<<message<<'\n';std::exit(1);}}
int main(){using namespace tigersheets;
 require(editFormulaReferences("=A1+$A$3+B$4+$C5+SUM(A2:A4)+\"A3\"","Data","Data",true,3,2,false)=="=A1+$A$5+B$6+$C7+SUM(A2:A6)+\"A3\"","row insertion rebases locked and unlocked cells and expands intersecting ranges while preserving text");
 require(editFormulaReferences("=SUM(A2:A6,A3:A4,A6:A2)+$B$4+B6","Data","Data",true,3,2,true)=="=SUM(A2:A4,#REF!,A4:A2)+#REF!+B4","row deletion shrinks surviving and reversed ranges and invalidates deleted references");
 require(editFormulaReferences("=SUM($B:D,3:$5,A1:A8)","Data","Data",false,3,2,false)=="=SUM($B:F,3:$5,A1:A8)","column insertion expands whole-column ranges while preserving whole-row ranges");
 require(editFormulaReferences("=SUM($B:F,3:$5,F9:B2)","Data","Data",false,3,2,true)=="=SUM($B:D,3:$5,D9:B2)","column deletion handles reversed rectangles and locked axis endpoints");
 require(editFormulaReferences("=SUM(2:5,$A:$C,'Data'!$A$4,Other!A4,[Other.xlsx]Data!A4)+\"A4\"","Summary","data",true,3,2,false)=="=SUM(2:5,$A:$C,'Data'!$A$6,Other!A4,[Other.xlsx]Data!A4)+\"A4\"","only matching local sheet references change and sheet comparison is insensitive to case");
 require(editFormulaReferences("=SUM(2:5,$A:$C,A1048576)","Data","Data",true,3,2,false)=="=SUM(2:7,$A:$C,#REF!)","whole-row ranges expand and impossible shifted references become REF errors");
 require(editFormulaReferences("=A1#+'FY''26'!B2","Data","Data",true,1,1,false)=="=A2#+'FY''26'!B2","spill anchors and apostrophe qualifiers retain their syntax");
 bool rejected=false;try{editFormulaReferences("=SUM(Data:Last!A1)","Data","Data",true,1,1,false);}catch(const std::exception&){rejected=true;}
 require(rejected,"structural rebasing refuses unsupported 3D references instead of silently leaving them stale");
 std::cout<<"structural reference tests passed\n";
}
