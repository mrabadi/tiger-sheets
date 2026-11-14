#include "tigersheets/references.hpp"
#include <cstdlib>
#include <iostream>
void expect(bool ok,const char* m){if(!ok){std::cerr<<"FAILED: "<<m<<'\n';std::exit(1);}}
int main(){
 using namespace tigersheets;
 const std::string localFormula=R"FORM(=SUM(A1:B3,$D$4,C$5,$F6)+LOG10(G7)+IF(H8="A1","quoted ""B2""",0)+Sales[A9])FORM";
 const auto local=formulaReferences(localFormula);
 expect(local.size()==6,"reference parser ignores strings, escaped quotes, structured labels and function names");
 const std::string localTokens[]={"A1:B3","$D$4","C$5","$F6","G7","H8"};
 for(std::size_t i=0;i<local.size();++i)expect(localFormula.substr(local[i].start,local[i].length)==localTokens[i]&&local[i].sheet.empty()&&!local[i].external,"local reference spans contain exactly the original token bytes");
 expect(local[0].row1==1&&local[0].col1==1&&local[0].row2==3&&local[0].col2==2&&local[3].row1==6&&local[3].col1==6,"parsed references expose 1-based rectangle coordinates");

 const std::string qualifiedFormula="=SUM('FY''26 虎'!$A1:B$3,Forecast!C4,[Budget.xlsx]Data!$D$5,'/home/model/[Other.xlsx]Cash Flow'!E6:F8)";
 const auto qualified=formulaReferences(qualifiedFormula);
 expect(qualified.size()==4,"qualified and external A1 references remain separately identifiable");
 expect(qualified[0].sheet=="FY'26 虎"&&!qualified[0].external&&qualified[1].sheet=="Forecast"&&!qualified[1].external&&qualified[2].sheet=="Data"&&qualified[2].external&&qualified[3].sheet=="Cash Flow"&&qualified[3].external,"sheet qualifiers decode apostrophes and external workbook prefixes");
 const std::string qualifiedTokens[]={"'FY''26 虎'!$A1:B$3","Forecast!C4","[Budget.xlsx]Data!$D$5","'/home/model/[Other.xlsx]Cash Flow'!E6:F8"};
 for(std::size_t i=0;i<qualified.size();++i)expect(qualifiedFormula.substr(qualified[i].start,qualified[i].length)==qualifiedTokens[i],"qualified spans include their full source bytes and UTF-8 qualifiers");
 expect(qualified[0].row2==3&&qualified[0].col2==2&&qualified[3].row1==6&&qualified[3].col1==5&&qualified[3].row2==8&&qualified[3].col2==6,"qualified reference rectangles remain 1-based");
 const auto axes=formulaReferences("=SUM($B:D,3:$5,'Forecast FY'!$XFC:$XFD,Sheet1!1048575:1048576)");
 expect(axes.size()==4,"whole-column and whole-row ranges are exposed without expanding cells");
 expect(axes[0].row1==1&&axes[0].row2==MaxRows&&axes[0].col1==2&&axes[0].col2==4&&axes[1].row1==3&&axes[1].row2==5&&axes[1].col1==1&&axes[1].col2==MaxColumns,"whole-axis references map to full worksheet bounds");
 expect(axes[2].sheet=="Forecast FY"&&axes[2].col1==16383&&axes[2].col2==16384&&axes[3].sheet=="Sheet1"&&axes[3].row1==1048575&&axes[3].row2==1048576,"qualified whole-axis bounds and mixed locks parse correctly");
 const auto boundaries=formulaReferences("=A0+XFE1+A1048577+foo.A1+A1_name+1E10+R1C1+LOG10(B2)+Table1[[A3]:[B5]]+Sheet1:Sheet3!C4+'Start:End'!D5+A1 : XFE4+XFE1 : B2+B2:A0+Forecast!A0:B2+SUM(E7:F6)");
 expect(boundaries.size()==2&&boundaries[0].row1==2&&boundaries[0].col1==2,"invalid A1 tokens, partial ranges, 3D qualifiers, identifiers and structured references do not create false outlines");
 expect(boundaries[1].row1==6&&boundaries[1].row2==7&&boundaries[1].col1==5&&boundaries[1].col2==6,"reversed reference endpoints expose a normalized rectangle without altering the source span");
 expect(formulaReferences("=SUM(1,3,A,B,1048577:1048578,XFE:XFF,0:2,A1::B2)").empty(),"numbers, names and malformed whole-axis ranges are not references");
 const auto limits=formulaReferences("=XFD1048576+$A$1+A1#");
 expect(limits.size()==3&&limits[0].row1==MaxRows&&limits[0].col1==MaxColumns,"worksheet boundary cells and spill-anchor references remain supported");
 expect(shiftFormula("=A1+$B1+C$1+$D$1",2,3)=="=D3+$B3+F$1+$D$1","copy mixed references preserves locked axes");
 expect(shiftFormula("=IF(A1=\"A1\",'FY2025'!B2,LOG10(C3))",1,1)=="=IF(B2=\"A1\",'FY2025'!C3,LOG10(D4))","only reference tokens change, not strings, sheet names, or function names");
 expect(shiftFormula("=A1+$B$2+XFD1048576",-1,-1)=="=#REF!+$B$2+XFC1048575","copy outside worksheet emits REF error");
 std::string rangeCycle="=A1:B3";
 for(const auto* expected:{"=$A$1:$B$3","=A$1:B$3","=$A1:$B3","=A1:B3"}){
  rangeCycle=cycleReference(rangeCycle,rangeCycle.size());expect(rangeCycle==expected,"F4 cycles both range endpoints through one coherent lock mode");
 }
 const std::string qualifiedCycle="='FY''26 虎'!a1 : b$3";
 expect(cycleReference(qualifiedCycle,qualifiedCycle.size())=="='FY''26 虎'!$a$1 : $b$3","F4 cycles a qualified range while preserving Unicode qualifier, case and spacing");
 const std::string twoRanges="=SUM(A1:B3,Forecast!$C$4:$D$8)";
 expect(cycleReference(twoRanges,twoRanges.find("$D$8")+3)=="=SUM(A1:B3,Forecast!C$4:D$8)","F4 changes only the range containing the caret");
 expect(cycleReference("=A1:$B3",7)=="=$A$1:$B$3"&&cycleReference("=$A1:B$3",8)=="=A1:B3","mixed endpoint locks normalize coherently from the first endpoint");
 expect(cycleReference("=A1 : XFE3",3)=="=A1 : XFE3"&&cycleReference("=Sales[A1:B3]",10)=="=Sales[A1:B3]","F4 does not partially rewrite invalid ranges or structured labels");
 expect(cycleReference("=Forecast!A1:B3",3)=="=Forecast!A1:B3","F4 preserves qualifier text when the caret is inside the sheet name");
 const std::string externalCycle="=[Budget.xlsx]Data!A1";
 expect(cycleReference(externalCycle,externalCycle.size())=="=[Budget.xlsx]Data!$A$1","F4 preserves existing external single-cell cycling behavior");
 expect(formulaReferences("").empty()&&formulaReferences("=IF(A1=\"unterminated B2").size()==1,"empty or incomplete formula text remains bounded and ignores unterminated strings");
 expect(cycleReference("=A1+B2",3)=="=$A$1+B2","F4 locks the reference at the cursor");
 expect(cycleReference("=$A$1+B2",5)=="=A$1+B2"&&cycleReference("=A$1+B2",4)=="=$A1+B2"&&cycleReference("=$A1+B2",4)=="=A1+B2","F4 cycles all four reference modes");
 expect(cycleReference("=\"A1\"",4)=="=\"A1\"","F4 does not rewrite quoted text");
 expect(shiftFormula("=SUM(Sales[A1])+A1",1,1)=="=SUM(Sales[A1])+B2","structured column labels remain unchanged when copying formulas");
 expect(shiftFormula("=SUM(A:A)+SUM(1:1)+SUM($B:B)+SUM($2:2)",2,2)=="=SUM(C:C)+SUM(3:3)+SUM($B:D)+SUM($2:4)","copying whole-axis ranges rebases only unlocked endpoints");
 expect(shiftFormula("=SUM('FY 26'!A:B,[Book.xlsx]Data!2:3)+\"A:B\"",1,1)=="=SUM('FY 26'!B:C,[Book.xlsx]Data!3:4)+\"A:B\"","whole-axis rebasing preserves qualifiers and quoted literals");
 expect(shiftFormula("=SUM(A:A)+SUM(1:1)",-1,-1)=="=SUM(#REF!)+SUM(#REF!)","whole-axis copying outside worksheet produces whole-reference errors");
 expect(cycleReference("=Sales[A1]+B2",9)=="=Sales[A1]+B2","F4 does not change a structured column label");
}
