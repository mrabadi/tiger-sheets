#include "tigersheets/calculation.hpp"
#include "tigersheets/pivot.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using tigersheets::Json;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Json valueAt(const Json& result, const char* sheet, int row, int col) {
    for (const auto& value : result.at("values")) {
        if (value.at("sheet") == sheet && value.at("row") == row && value.at("col") == col)
            return value;
    }
    throw std::runtime_error("Expected calculated cell missing");
}

void crossSheetCalculation() {
    const Json snapshot = {{"revision", 37}, {"sheets", {
        {{"id", "inputs"}, {"name", "Inputs"}, {"cells", {
            {{"row", 1}, {"col", 1}, {"input", "10"}},
            {{"row", 2}, {"col", 1}, {"input", "20"}}
        }}},
        {{"id", "model"}, {"name", "Financial model"}, {"cells", {
            {{"row", 1}, {"col", 1}, {"input", "=SUM(Inputs!A1:A2)*2"}}
        }}}
    }}};
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    require(result.at("revision") == 37, "Calculation must retain input revision");
    const auto cell = valueAt(result, "model", 1, 1);
    require(cell.at("type") == "number", "SUM must calculate a number");
    require(cell.at("value") == 60, "Cross-sheet SUM must calculate 60");
    require(result.at("diagnostics").empty(), "Supported formulas must not produce diagnostics");
}

void financialFunctionsAndLookups() {
    const auto snapshot = Json::parse(R"json({"revision":38,"sheets":[{"id":"finance","name":"Finance","cells":[
      {"row":1,"col":1,"input":"-100"},{"row":2,"col":1,"input":"110"},
      {"row":1,"col":2,"input":"44927"},{"row":2,"col":2,"input":"45292"},
      {"row":1,"col":3,"input":"=IRR(A1:A2)"},
      {"row":2,"col":3,"input":"=XIRR(A1:A2,B1:B2)"},
      {"row":3,"col":3,"input":"=XNPV(0.1,A1:A2,B1:B2)"},
      {"row":4,"col":3,"input":"=NPV(0.1,A2)+A1"},
      {"row":5,"col":3,"input":"=PMT(0.1,1,100)"},
      {"row":6,"col":3,"input":"=XLOOKUP(110,A1:A2,B1:B2)"},
      {"row":7,"col":3,"input":"=INDEX(B1:B2,MATCH(110,A1:A2,0))"},
      {"row":8,"col":3,"input":"=LET(rate,0.1,NPV(rate,A2)+A1)"}
    ]}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    const double expected[] = {0.1, 0.1, 0.0, 0.0, -110.0, 45292.0, 45292.0, 0.0};
    for (int row = 1; row <= 8; ++row) {
        const auto cell = valueAt(result, "finance", row, 3);
        require(cell.at("type") == "number", "Financial formula must calculate a number");
        require(std::abs(cell.at("value").get<double>() - expected[row - 1]) < 1e-7,
                "Financial formula differs from the reference cash flow");
    }
    require(result.at("diagnostics").empty(), "Supported finance formulas must evaluate cleanly");
}

void formulaErrorsAreExplicit() {
    const auto snapshot = Json::parse(R"json({"revision":39,"sheets":[{"id":"errors","name":"Errors","cells":[
      {"row":1,"col":1,"input":"=1/0"}
    ]}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    const auto cell = valueAt(result, "errors", 1, 1);
    require(cell.at("type") == "error", "A formula error must not become plain text");
    require(cell.at("value") == "#DIV/0!", "Formula error must retain its Excel code");
    require(result.at("diagnostics").size() == 1, "Formula error needs an actionable diagnostic");
    require(result.at("diagnostics").at(0).at("code") == "formula_error", "Expected formula error diagnostic");
}

void pinnedEngineVersion() {
    const auto snapshot = Json::parse(R"json({"revision":40,"sheets":[{"id":"meta","name":"Metadata","cells":[
      {"row":1,"col":1,"input":"=INFO(\"release\")"}
    ]}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    require(valueAt(result, "meta", 1, 1).at("value") == "v0.9.1",
            "Engine version must identify the pinned dependency, not the parent repository");
}

void dynamicArraysReturnSpilledCells() {
    const auto snapshot = Json::parse(R"json({"revision":41,"sheets":[{"id":"arrays","name":"Arrays","cells":[
      {"row":1,"col":1,"input":"=SEQUENCE(3,2)"},
      {"row":1,"col":4,"input":"=SUM(A1#)"}
    ]}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    require(valueAt(result, "arrays", 3, 2).at("value") == 6,
            "Calculation must return generated dynamic-array cells");
    require(valueAt(result, "arrays", 1, 4).at("value") == 21,
            "Dependent formulas must read the complete spill range");
}

void invalidSnapshotsAreRejectedAtomically() {
    const auto valid = Json::parse(R"json({"revision":42,"sheets":[{"id":"s","name":"Sheet1","cells":[
      {"row":1,"col":1,"input":"7"}
    ]}]})json");
    auto empty = valid;
    empty["sheets"] = Json::array();
    auto duplicateSheet = valid;
    duplicateSheet["sheets"].push_back({{"id", "s"}, {"name", "Sheet2"}, {"cells", Json::array()}});
    auto duplicateCell = valid;
    duplicateCell["sheets"][0]["cells"].push_back({{"row", 1}, {"col", 1}, {"input", "9"}});
    auto invalidRow = valid;
    invalidRow["sheets"][0]["cells"][0]["row"] = 0;
    auto invalidColumn = valid;
    invalidColumn["sheets"][0]["cells"][0]["col"] = 16385;
    auto invalidInputType = valid;
    invalidInputType["sheets"][0]["cells"][0]["input"] = 7;
    for (const auto& snapshot : {empty, duplicateSheet, duplicateCell, invalidRow, invalidColumn, invalidInputType}) {
        const auto result = tigersheets::CalculationEngine().calculate(snapshot);
        require(result.at("status") == "invalid_snapshot", "Invalid snapshots must be rejected atomically");
        require(result.at("revision") == 42, "Rejected snapshots must retain their revision");
        require(result.at("values").empty(), "Rejected snapshots must not leak partial results");
    }
}

void cancellationIsPerEngineAndResettable() {
    const auto snapshot = Json::parse(R"json({"revision":43,"sheets":[{"id":"s","name":"Sheet1","cells":[
      {"row":1,"col":1,"input":"=3+4"}
    ]}]})json");
    tigersheets::CalculationEngine cancelledEngine;
    cancelledEngine.cancel();
    const auto cancelled = cancelledEngine.calculate(snapshot);
    require(cancelled.at("status") == "cancelled", "Cancelled calculation must not publish values");
    require(cancelled.at("values").empty(), "Cancelled calculation must return no values");
    require(cancelled.at("revision") == 43, "Cancellation must preserve the snapshot revision");
    require(valueAt(tigersheets::CalculationEngine().calculate(snapshot), "s", 1, 1).at("value") == 7,
            "Cancelling one engine must not cancel a different engine");
    cancelledEngine.resetCancellation();
    require(valueAt(cancelledEngine.calculate(snapshot), "s", 1, 1).at("value") == 7,
            "Reset cancellation must allow another calculation");
}

void registeredFunctionCatalogIsAuditable() {
    const Json catalog = tigersheets::CalculationEngine().functions();
    require(catalog.at("version") == "v0.9.1", "Catalog must identify its exact engine release");
    require(catalog.at("commit") == "eb9ca75bd99160c17902359d2d87225c4571c468", "Catalog must identify its audited source");
    require(catalog.at("availability_only").get<bool>(), "Registry availability must not claim conformance");
    const auto names = catalog.at("functions").get<std::vector<std::string>>();
    require(names.size() == 500, "Catalog must include the pinned registry and owned AGGREGATE/GETPIVOTDATA/FIELDVALUE extensions");
    require(std::is_sorted(names.begin(), names.end()), "Function catalog must have stable ordering");
    for (const std::string name : {"XIRR", "XNPV", "INDEX", "MATCH", "XLOOKUP", "LET", "LAMBDA", "FILTER", "GETPIVOTDATA", "FIELDVALUE"})
        require(std::find(names.begin(), names.end(), name) != names.end(), "Expected financial-model function missing");
}

void customFunctionsComposeInsideFormulaTrees() {
    const auto snapshot = Json::parse(R"json({"revision":44,"sheets":[{"id":"s","name":"Sheet1","cells":[
      {"row":1,"col":1,"input":"7"},
      {"row":1,"col":2,"input":"=SUM(Triple(A1+1),2)*3"},
      {"row":1,"col":3,"input":"=B1+Triple(2)"}
    ]}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot,
        [](const std::string& name, const Json& args) -> Json {
            require(name == "Triple", "Host must receive the named function");
            return {{"status", "completed"}, {"value", args.at(0).get<double>() * 3}};
        });
    require(valueAt(result, "s", 1, 2).at("value") == 78,
            "Custom functions must compose with built-in calls and arithmetic");
    require(valueAt(result, "s", 1, 3).at("value") == 84,
            "Dependent cells must receive custom function results");
    require(snapshot["sheets"][0]["cells"][1]["input"] == "=SUM(Triple(A1+1),2)*3",
            "Evaluation must preserve canonical formula text");
    const auto failed = tigersheets::CalculationEngine().calculate(snapshot,
        [](const std::string&, const Json&) -> Json { throw std::runtime_error("Workbook writes are forbidden"); });
    require(valueAt(failed, "s", 1, 2).at("type") == "error" &&
            valueAt(failed, "s", 1, 2).at("value") == "#VALUE!",
            "Host function exceptions must become typed formula errors");
}

void circularIterationUsesPreviousPassValues() {
    auto snapshot = Json::parse(R"json({"revision":45,"settings":{"iterationEnabled":true,"maxIterations":100,"maxChange":1e-9},
      "sheets":[{"id":"s","name":"Sheet1","cells":[
      {"row":1,"col":1,"input":"=B1/2+5"},
      {"row":1,"col":2,"input":"=INDIRECT(\"A1\")/2+5"},
      {"row":1,"col":3,"input":"=A1+B1"}
    ]}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    require(valueAt(result, "s", 1, 1).at("type") == "number",
            "Enabled iteration must evaluate circular formulas numerically");
    require(std::abs(valueAt(result, "s", 1, 1).at("value").get<double>() - 10) < 1e-8,
            "Circular financial model must converge to its fixed point");
    require(std::abs(valueAt(result, "s", 1, 3).at("value").get<double>() - 20) < 1e-8,
            "Downstream formulas must receive converged circular values");
    require(result.at("iteration").at("converged").get<bool>(), "Convergence must be explicit");
    snapshot["settings"]["iterationEnabled"] = false;
    require(valueAt(tigersheets::CalculationEngine().calculate(snapshot), "s", 1, 1).at("value") == "#CIRC!",
            "Disabled iteration must preserve explicit circular-reference errors");
    snapshot["settings"]["iterationEnabled"] = true;
    snapshot["settings"]["maxIterations"] = 3;
    snapshot["sheets"][0]["cells"] = {{{"row", 1}, {"col", 1}, {"input", "=A1+1"}}};
    const auto divergent = tigersheets::CalculationEngine().calculate(snapshot);
    require(divergent.at("status") == "not_converged" && divergent.at("values").empty(),
            "Divergent iteration must publish no usable calculated values");
    require(!divergent.at("iteration").at("converged").get<bool>(),
            "A divergent cycle must not claim convergence");
    require(divergent.at("diagnostics")[0].at("code") == "iteration_not_converged",
            "Iteration limit must have an explicit diagnostic");
}

void linkedWorkbooksUseTransientAliases() {
    const auto snapshot = Json::parse(R"json({"revision":46,"sheets":[{"id":"s","name":"Model","cells":[
      {"row":1,"col":1,"input":"='[Assumptions.xlsx]Drivers'!A1*2"},
      {"row":1,"col":2,"input":"=\"[Assumptions.xlsx]Drivers!A1\""}
    ]}],"links":[{"name":"Assumptions.xlsx","path":"/tmp/Assumptions.xlsx","enabled":true,"workbook":{
      "revision":1,"sheets":[{"id":"d","name":"Drivers","cells":[{"row":1,"col":1,"input":"=Rates!A1+1"}]},
      {"id":"r","name":"Rates","cells":[{"row":1,"col":1,"input":"6"}]}]
    }}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    require(valueAt(result, "s", 1, 1).at("value") == 14,
            "Linked workbooks must evaluate their source formulas and cross-sheet dependencies");
    require(valueAt(result, "s", 1, 2).at("value") == "[Assumptions.xlsx]Drivers!A1",
            "Reference remapping must preserve Excel string literals");
    require(result.at("values").size() == 2, "Linked workbook cells must not leak into root values");
    require(snapshot["sheets"][0]["cells"][0]["input"] == "='[Assumptions.xlsx]Drivers'!A1*2",
            "Linked calculation must not rewrite canonical formula text");
}

void unavailableLinksReturnReferenceErrors() {
    const auto snapshot = Json::parse(R"json({"revision":47,"sheets":[{"id":"s","name":"Model","cells":[
      {"row":1,"col":1,"input":"='[Missing.xlsx]Drivers'!A1+2"}
    ]}],"links":[{"name":"Missing.xlsx","path":"/tmp/Missing.xlsx","enabled":false}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    require(valueAt(result, "s", 1, 1).at("value") == "#REF!",
            "Unavailable workbook sources must return a reference error");
    require(result.at("diagnostics")[0].at("code") == "external_link_unavailable",
            "Unavailable workbook sources must have an explicit diagnostic");
}

void disabledLinksUseOnlyExplicitLiteralCaches() {
    auto snapshot = Json::parse(R"json({"revision":48,"sheets":[{"id":"s","name":"Model","cells":[
      {"row":1,"col":1,"input":"='[Cached.xlsx]Inputs'!A1+2"}
    ]}],"links":[{"name":"Cached.xlsx","path":"/tmp/Cached.xlsx","enabled":false,
      "workbook":{"revision":1,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"1000"}]}]},
      "cachedWorkbook":{"revision":1,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"3"}]}]}
    }]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    require(valueAt(result, "s", 1, 1).at("value") == 5,
            "Disabled links must read their explicit cache instead of live source data");
    require(result.at("diagnostics")[0].at("code") == "external_link_cached",
            "Cached link use must be explicit");
    snapshot["links"][0]["cachedWorkbook"]["sheets"][0]["cells"][0]["input"] = "=2+1";
    require(tigersheets::CalculationEngine().calculate(snapshot).at("status") == "invalid_snapshot",
            "Disabled link caches must not execute formulas");
}

void workbookNamesResolveRangesAndLambdas() {
    const auto snapshot = Json::parse(R"json({"revision":49,"names":{
      "CashFlows":"'Finance'!$A$1:$A$2","Triple":"=LAMBDA(n,n*3)"},
      "sheets":[{"id":"s","name":"Finance","cells":[
      {"row":1,"col":1,"input":"-100"},{"row":2,"col":1,"input":"110"},
      {"row":1,"col":2,"input":"=IRR(CashFlows)"},
      {"row":2,"col":2,"input":"=Triple(5)+SUM(CashFlows)"}
    ]}]})json");
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    const auto irr = valueAt(result, "s", 1, 2);
    require(irr.at("type") == "number", "Workbook named ranges must resolve inside financial formulas");
    require(std::abs(irr.at("value").get<double>() - 0.1) < 1e-7,
            "Named range IRR must use its underlying cash flows");
    require(valueAt(result, "s", 2, 2).at("value") == 25,
            "Workbook named lambdas must compose with named ranges");
}

void aggregateSupportsStatisticsErrorsAndHiddenRows() {
    Json cells = Json::array();
    const int data[] = {1, 2, 2, 4, 5};
    for (int row = 1; row <= 5; ++row) cells.push_back({{"row", row}, {"col", 1}, {"input", std::to_string(data[row-1])}});
    const double expected[] = {2.8,5,5,5,1,80,std::sqrt(2.7),std::sqrt(2.16),14,2.7,2.16,2,2,4,2,2,2,1.5,1.5};
    for (int mode = 1; mode <= 19; ++mode) {
        std::string formula = "=AGGREGATE(" + std::to_string(mode) + ",0,A1:A5";
        if (mode >= 14) formula += (mode == 16 || mode == 18) ? ",0.25" : (mode == 17 || mode == 19) ? ",1" : ",2";
        cells.push_back({{"row", mode}, {"col", 2}, {"input", formula + ")"}});
    }
    cells.push_back({{"row", 6}, {"col", 1}, {"input", "=1/0"}});
    cells.push_back({{"row", 7}, {"col", 1}, {"input", "=SUBTOTAL(9,A1:A5)"}});
    cells.push_back({{"row", 20}, {"col", 2}, {"input", "=AGGREGATE(9,2,A1:A7)"}});
    cells.push_back({{"row", 21}, {"col", 2}, {"input", "=AGGREGATE(9,1,A1:A5)"}});
    const Json snapshot = {{"revision", 50}, {"sheets", {{{"id", "s"}, {"name", "Stats"}, {"hiddenRows", {5}}, {"cells", cells}}}}};
    const auto result = tigersheets::CalculationEngine().calculate(snapshot);
    for (int mode = 1; mode <= 19; ++mode) {
        const auto value = valueAt(result, "s", mode, 2);
        if (value.at("type") != "number")
            throw std::runtime_error("AGGREGATE mode " + std::to_string(mode) + " produced " + value.dump());
        require(std::abs(value.at("value").get<double>() - expected[mode-1]) < 1e-8,
                "AGGREGATE statistic must match its reference data");
    }
    require(valueAt(result, "s", 20, 2).at("value") == 14,
            "AGGREGATE option2 must ignore errors and nested subtotals");
    require(valueAt(result, "s", 21, 2).at("value") == 9,
            "AGGREGATE option1 must ignore hidden rows");
}
void sharedNumberFormattingPreservesFinancialCodes() {
    require(tigersheets::formatCellValue(123456789,"#,##0,,")=="123","Trailing commas must scale financial report values");
    require(tigersheets::formatCellValue(1.25,"0.0\"x\"")=="1.3x","Quoted units must remain literal");
    require(tigersheets::formatCellValue(1.005,"0.00")=="1.01"&&tigersheets::formatCellValue(-1.25,"0.0")=="-1.3",
        "Financial formatting must round decimal ties away from zero without binary-noise errors");
    require(tigersheets::formatCellValue(45292.5,"m/d/yyyy h:mm")=="1/1/2024 12:00","Date and time formatting must retain the fractional day");
    const auto accounting=tigersheets::formatCellValue(-1234,"_($* #,##0.00_);_($* (#,##0.00);_($* \"-\"??_);_(@_)");
    require(accounting.find("1,234.00")!=std::string::npos&&accounting.find('$')!=std::string::npos&&accounting.find('(')!=std::string::npos,
        "Accounting format must retain currency, precision and negative parentheses");
}
void pivotFormulasUseReferenceIdentityAndTypedItems() {
    auto book=Json::parse(R"json({"revision":54,"sheets":[{"id":"s","name":"Data","cells":[
      {"row":1,"col":1,"input":"Region"},{"row":1,"col":2,"input":"Revenue"},
      {"row":2,"col":1,"input":"123"},{"row":2,"col":2,"input":"10"},
      {"row":3,"col":1,"input":"'123"},{"row":3,"col":2,"input":"20"}]},
      {"id":"r","name":"Report","cells":[]}]})json");
    const auto definition=Json::parse(R"json({"id":"p","name":"RevenueReport","source":{"sheet":"s","row1":1,"col1":1,"row2":3,"col2":2},"rows":[{"field":"Region"}],"values":[{"field":"Revenue","aggregate":"sum","name":"Revenue"}],"output":{"sheet":"r","row1":1,"col1":1}})json");
    auto generated=tigersheets::generatePivot(book,{{"revision",54},{"status","ok"},{"values",Json::array()}},definition);
    book["pivots"]=Json::array({generated.definition});book["sheets"][1]["cells"]=generated.cells;
    auto& cells=book["sheets"][0]["cells"];
    const char* formulas[]={"=GETPIVOTDATA(\"Revenue\",Report!$A$1,\"Region\",123)","=GETPIVOTDATA(\"Revenue\",Report!A2,\"Region\",\"123\")","=GETPIVOTDATA(\"Revenue\",999,\"Region\",123)","=GETPIVOTDATA(\"Revenue\",Report!A1)","=UnknownWorksheetFunction(1)"};
    for(int row=1;row<=5;++row)cells.push_back({{"row",row},{"col",4},{"input",formulas[row-1]}});
    const auto result=tigersheets::CalculationEngine().calculate(book);
    require(valueAt(result,"s",1,4).at("value")==10&&valueAt(result,"s",2,4).at("value")==20,
        "GETPIVOTDATA must preserve report reference identity and numeric versus text member types");
    require(valueAt(result,"s",3,4).at("type")=="error"&&valueAt(result,"s",3,4).at("value")=="#REF!",
        "A literal pivot-table value cannot impersonate a worksheet reference");
    require(valueAt(result,"s",4,4).at("value")==30,"GETPIVOTDATA without item pairs returns the report grand total");
    require(valueAt(result,"s",5,4).at("value")=="#NAME?","Installing native pivot functions must preserve unknown-function errors");
    for(const auto& cell:Json::array({{{"row",4},{"col",1},{"input",""}},{{"row",4},{"col",2},{"input","5"}},{{"row",5},{"col",1},{"input","TRUE"}},{{"row",5},{"col",2},{"input","7"}},{{"row",6},{"col",1},{"input","'North \"Region\""}},{{"row",6},{"col",2},{"input","9"}}}))book["sheets"][0]["cells"].push_back(cell);
    auto expandedDefinition=definition;expandedDefinition["source"]["row2"]=6;
    generated=tigersheets::generatePivot(book,{{"revision",54},{"status","ok"},{"values",Json::array()}},expandedDefinition);book["pivots"]=Json::array({generated.definition});book["sheets"][1]["cells"]=generated.cells;
    const auto& data=generated.definition.at("layout").at("data");
    for(std::size_t row=0;row<data.size();++row){const auto formula=tigersheets::pivotReferenceFormula(book,"r",static_cast<int>(row)+2,2);require(!formula.empty(),"Every generated result cell has a pointed-reference formula");book["sheets"][0]["cells"].push_back({{"row",static_cast<int>(row)+1},{"col",5},{"input",formula}});}
    const auto pointed=tigersheets::CalculationEngine().calculate(book);
    for(std::size_t row=0;row<data.size();++row)require(valueAt(pointed,"s",static_cast<int>(row)+1,5).at("value")==data[row][0].at("value"),"Generated GETPIVOTDATA formulas preserve blanks, booleans, numbers, text numbers and embedded quotes");
}
void workbookScalarNamesPreserveTheirTypes() {
    const auto book=Json::parse(R"json({"revision":53,"names":{"WACC":"0.1","Label":"\"Forecast\"","Enabled":"TRUE","Debt":"=-5"},"sheets":[{"id":"s","name":"Model","cells":[
      {"row":1,"col":1,"input":"=100*WACC"},{"row":2,"col":1,"input":"=Label"},
      {"row":3,"col":1,"input":"=IF(Enabled,Debt,0)"},{"row":4,"col":1,"input":"=ISREF(WACC)"}]}]})json");
    const auto result=tigersheets::CalculationEngine().calculate(book);
    require(result.at("status")=="ok","Workbook scalar names must be accepted as values");
    require(valueAt(result,"s",1,1).at("value")==10&&valueAt(result,"s",2,1).at("value")=="Forecast"&&valueAt(result,"s",3,1).at("value")==-5,
        "Numeric, text, boolean and signed scalar names must retain their original types");
    require(valueAt(result,"s",4,1).at("value")==false,"A scalar name must not become a cell reference");
}
void filteredAndManuallyHiddenRowsRemainDistinct() {
    const auto snapshot=Json::parse(R"json({"revision":52,"sheets":[{"id":"s","name":"Filtered","filteredRows":[2],"hiddenRows":[3],"cells":[
      {"row":1,"col":1,"input":"10"},{"row":2,"col":1,"input":"20"},{"row":3,"col":1,"input":"30"},{"row":4,"col":1,"input":"50"},
      {"row":1,"col":2,"input":"=SUBTOTAL(9,A1:A4)"},{"row":2,"col":2,"input":"=SUBTOTAL(109,A1:A4)"},
      {"row":3,"col":2,"input":"=SUBTOTAL(110,A1:A4)"},{"row":4,"col":2,"input":"=SUBTOTAL(111,A1:A4)"},
      {"row":5,"col":2,"input":"=AGGREGATE(9,0,A1:A4)"},{"row":6,"col":2,"input":"=AGGREGATE(9,1,A1:A4)"},
      {"row":7,"col":2,"input":"=SUM(A1:A4)"}]}]})json");
    const auto result=tigersheets::CalculationEngine().calculate(snapshot);
    const double expected[]={90,60,800,400,90,60,110};
    for(int row=1;row<=7;++row)require(valueAt(result,"s",row,2).at("value")==expected[row-1],
        "Filtered rows must always be excluded from SUBTOTAL/AGGREGATE; manual hiding obeys their selected mode");
}
void unsupported1904DateSystemCannotPublishCalculatedValues() {
    auto snapshot = Json::parse(R"json({"revision":51,"settings":{"date1904":true},"sheets":[{"id":"s","name":"Dates","cells":[
      {"row":1,"col":1,"input":"0"},
      {"row":1,"col":2,"input":"=YEAR(A1)"},
      {"row":2,"col":2,"input":"=DATE(1904,1,1)"}
    ]}]})json");
    tigersheets::CalculationEngine engine;
    const auto result = engine.calculate(snapshot);
    require(result.at("status") == "unsupported_date_system",
            "1904 workbooks must not silently calculate using the 1900 date epoch");
    require(result.at("revision") == 51 && result.at("values").empty(),
            "Unsupported date systems must preserve revision and publish no partial values");
    require(result.at("diagnostics")[0].at("code") == "unsupported_date_system" &&
            !result.at("diagnostics")[0].at("message").get<std::string>().empty(),
            "Unsupported date systems need a structured diagnostic");
    bool invoked = false;
    const auto custom = engine.calculate(snapshot, [&](const std::string&, const Json&) -> Json {
        invoked = true; return {{"status", "completed"}, {"value", 7}};
    });
    require(custom == result && !invoked, "The custom-function calculation overload must apply the same date-system guard");
    snapshot["settings"]["date1904"] = false;
    require(valueAt(engine.calculate(snapshot), "s", 2, 2).at("value") == 1462,
            "The ordinary 1900 date-system calculation must remain supported");
}
}

int main(int argc,char** argv) {
    try {
        const std::string selected=argc>1?argv[1]:"all";
        if(selected=="format"){sharedNumberFormattingPreservesFinancialCodes();std::cout<<"formatting tests passed\n";return 0;}
        if(selected=="getpivot"){pivotFormulasUseReferenceIdentityAndTypedItems();std::cout<<"pivot formula tests passed\n";return 0;}
        crossSheetCalculation();
        financialFunctionsAndLookups();
        formulaErrorsAreExplicit();
        pinnedEngineVersion();
        dynamicArraysReturnSpilledCells();
        invalidSnapshotsAreRejectedAtomically();
        cancellationIsPerEngineAndResettable();
        registeredFunctionCatalogIsAuditable();
        customFunctionsComposeInsideFormulaTrees();
        circularIterationUsesPreviousPassValues();
        linkedWorkbooksUseTransientAliases();
        unavailableLinksReturnReferenceErrors();
        disabledLinksUseOnlyExplicitLiteralCaches();
        workbookNamesResolveRangesAndLambdas();
        aggregateSupportsStatisticsErrorsAndHiddenRows();
        filteredAndManuallyHiddenRowsRemainDistinct();
        workbookScalarNamesPreserveTheirTypes();
        sharedNumberFormattingPreservesFinancialCodes();
        pivotFormulasUseReferenceIdentityAndTypedItems();
        unsupported1904DateSystemCannotPublishCalculatedValues();
        std::cout << "calculation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "calculation test failed: " << error.what() << '\n';
        return 1;
    }
}
