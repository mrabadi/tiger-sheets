#include "tigersheets/csv.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <stdexcept>
#include <limits>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 const auto rows=parseCsv("Name,Amount,Note\r\n\"Tiger, Ltd\",123.50,\"line one\nline \"\"two\"\"\"\r\n,,\r\n");
 require(rows.size()==3&&rows[1][0]=="Tiger, Ltd"&&rows[1][2]=="line one\nline \"two\"","RFC4180 quoting");
 require(rows[2].size()==3&&rows[2][2].empty(),"trailing empty fields");
 require(parseCsv(formatCsv(rows))==rows,"CSV roundtrip");
 bool rejected=false;try{parseCsv("a,\"unterminated");}catch(const std::runtime_error&){rejected=true;}require(rejected,"malformed CSV must be rejected");
 require(parseCsv("\xef\xbb\xbfName,Value")[0][0]=="Name","UTF8 BOM stripped");
 char temporary[]="/tmp/tigersheets-csv-XXXXXX";const auto descriptor=mkstemp(temporary);require(descriptor>=0,"temporary CSV fixture");close(descriptor);const std::string path=temporary;
 {std::ofstream file(path);file<<"A,B\n1,2\n";}
 const auto workbook=readCsv(path);require(workbook["sheets"][0]["cells"].size()==4,"CSV becomes sparse workbook");
 writeCsv(path,workbook);require(parseCsv(formatCsv(rows))==rows,"CSV formatting repeatable");
 require(readCsv(path)["sheets"][0]["cells"]==workbook["sheets"][0]["cells"],"CSV file roundtrip");std::filesystem::remove(path);
 const Json formulaBook={{"revision",7},{"sheets",Json::array({{{"id","model"},{"name","Model"},{"cells",Json::array({{{"row",1},{"col",1},{"input","=6*7"}}})}}})}};
 const auto originalFormulaBook=formulaBook;
 const Json calculation={{"revision",7},{"status","ok"},{"values",Json::array({{{"sheet","model"},{"row",1},{"col",1},{"type","number"},{"value",42.0}}})}};
 const auto valuesBook=csvValuesSnapshot(formulaBook,calculation);
 require(valuesBook["sheets"][0]["cells"][0]["input"]=="42","CSV formula export uses the typed result rather than formula source");
 require(formulaBook==originalFormulaBook,"CSV values preparation does not mutate the canonical workbook");
 writeCsv(path,valuesBook);require(readCsv(path)["sheets"][0]["cells"][0]["input"]=="42","CSV writes calculated formula value 42");std::filesystem::remove(path);
 const Json arrayBook={{"revision",8},{"sheets",Json::array({{{"id","other"},{"name","Other"},{"cells",Json::array({{{"row",1},{"col",1},{"input","=999"}}})}},{{"id","arrays"},{"name","Arrays"},{"cells",Json::array({{{"row",1},{"col",1},{"input","=SEQUENCE(2,2)"}}})}}})}};
 const double preciseNumber=0.12345678901234566;
 const Json arrayCalculation={{"revision",8},{"status","ok"},{"values",Json::array({{{"sheet","arrays"},{"row",1},{"col",1},{"type","number"},{"value",1.0}},{{"sheet","arrays"},{"row",1},{"col",2},{"type","number"},{"value",preciseNumber}},{{"sheet","arrays"},{"row",2},{"col",1},{"type","number"},{"value",3.0}},{{"sheet","arrays"},{"row",2},{"col",2},{"type","number"},{"value",std::uint64_t{9007199254740993ULL}}}})}};
 const auto arrayValues=csvValuesSnapshot(arrayBook,arrayCalculation,"arrays");
 require(arrayValues["sheets"][0]==arrayBook["sheets"][0],"CSV values preparation leaves unselected worksheets untouched");
 require(arrayValues["sheets"][1]["cells"].size()==4,"CSV values include generated spill cells outside sparse source cells");
 writeCsv(path,arrayValues,"arrays");std::ifstream arrayFile(path);const std::string arrayBytes((std::istreambuf_iterator<char>(arrayFile)),std::istreambuf_iterator<char>());const auto exportedArray=parseCsv(arrayBytes);
 require(exportedArray.size()==2&&exportedArray[0][0]=="1"&&std::stod(exportedArray[0][1])==preciseNumber&&exportedArray[1][0]=="3"&&exportedArray[1][1]=="9007199254740993","CSV selected spill export retains numeric precision");std::filesystem::remove(path);
 const std::vector<std::string> rawTypedInputs{"'00012","''quote","=TRUE","=1/0","=TEXT(A1,\"0\")","=EmptyResult()","plain, \"text\"\nline","=FALSE","'=not formula","uncomputed literal"};
 Json typedCells=Json::array();for(std::size_t col=0;col<rawTypedInputs.size();++col)typedCells.push_back({{"row",1},{"col",col+1},{"input",rawTypedInputs[col]}});
 const Json typedBook={{"revision",9},{"sheets",Json::array({{{"id","typed"},{"name","Typed"},{"cells",typedCells}}})}};
 const Json typedCalculation={{"revision",9},{"status","ok"},{"diagnostics",Json::array({{{"code","formula_error"},{"message","#DIV/0!"}}})},{"values",Json::array({{{"sheet","typed"},{"row",1},{"col",1},{"type","text"},{"value","00012"}},{{"sheet","typed"},{"row",1},{"col",2},{"type","text"},{"value","'quote"}},{{"sheet","typed"},{"row",1},{"col",3},{"type","boolean"},{"value",true}},{{"sheet","typed"},{"row",1},{"col",4},{"type","error"},{"value","#DIV/0!"}},{{"sheet","typed"},{"row",1},{"col",5},{"type","text"},{"value","=literal"}},{{"sheet","typed"},{"row",1},{"col",6},{"type","empty"},{"value",nullptr}},{{"sheet","typed"},{"row",1},{"col",7},{"type","text"},{"value",rawTypedInputs[6]}},{{"sheet","typed"},{"row",1},{"col",8},{"type","boolean"},{"value",false}}})}};
 const auto typedValues=csvValuesSnapshot(typedBook,typedCalculation);writeCsv(path,typedValues);std::ifstream typedFile(path);const std::string typedBytes((std::istreambuf_iterator<char>(typedFile)),std::istreambuf_iterator<char>());const auto typedRows=parseCsv(typedBytes);
 const std::vector<std::string> expectedTyped{"00012","'quote","TRUE","#DIV/0!","=literal","",rawTypedInputs[6],"FALSE","=not formula","uncomputed literal"};
 require(typedRows.size()==1&&typedRows[0]==expectedTyped,"CSV typed values preserve literal strings booleans errors empties and multiline text");require(typedBook["sheets"][0]["cells"]==typedCells,"typed CSV preparation leaves literal source escapes unchanged");std::filesystem::remove(path);
 std::vector<std::pair<std::string,Json>> invalidCalculations;
 auto stale=calculation;stale["revision"]=6;invalidCalculations.emplace_back("stale revision",stale);
 auto missing=calculation;missing["values"]=Json::array();invalidCalculations.emplace_back("missing formula result",missing);
 auto cancelledCalculation=calculation;cancelledCalculation["status"]="cancelled";invalidCalculations.emplace_back("cancelled calculation",cancelledCalculation);
 auto noStatus=calculation;noStatus.erase("status");invalidCalculations.emplace_back("missing status",noStatus);
 auto fractionalRevision=calculation;fractionalRevision["revision"]=7.0;invalidCalculations.emplace_back("noninteger revision",fractionalRevision);
 auto duplicateResult=calculation;duplicateResult["values"].push_back(duplicateResult["values"][0]);invalidCalculations.emplace_back("duplicate result coordinate",duplicateResult);
 auto invalidRow=calculation;invalidRow["values"][0]["row"]=0;invalidCalculations.emplace_back("out-of-bounds result",invalidRow);
 auto fractionalCoordinate=calculation;fractionalCoordinate["values"][0]["col"]=1.5;invalidCalculations.emplace_back("noninteger result coordinate",fractionalCoordinate);
 auto invalidEmpty=calculation;invalidEmpty["values"][0]["type"]="empty";invalidEmpty["values"][0]["value"]="not empty";invalidCalculations.emplace_back("invalid empty payload",invalidEmpty);
 auto infiniteNumber=calculation;infiniteNumber["values"][0]["value"]=std::numeric_limits<double>::infinity();invalidCalculations.emplace_back("nonfinite numeric result",infiniteNumber);
 bool allRejected=true;for(const auto& [label,result]:invalidCalculations){bool blocked=false;try{csvValuesSnapshot(formulaBook,result);}catch(const std::exception&){blocked=true;}if(!blocked){allRejected=false;std::cerr<<"Unrejected CSV calculation: "<<label<<'\n';}}require(allRejected,"CSV preparation refuses stale failed incomplete and malformed calculated results");
 bool missingSheetRejected=false;try{csvValuesSnapshot(formulaBook,calculation,"missing");}catch(const std::exception&){missingSheetRejected=true;}require(missingSheetRejected,"CSV preparation refuses an unknown selected worksheet");
 std::cout<<"csv tests passed\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
