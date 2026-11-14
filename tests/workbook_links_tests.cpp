#include "tigersheets/workbook_links.hpp"
#include "tigersheets/calculation.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace tigersheets;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
Json valueAt(const Json& result, int row, int col) {
    for (const auto& value : result.at("values"))
        if (value.at("sheet") == "model" && value.at("row") == row && value.at("col") == col) return value;
    throw std::runtime_error("Calculated linked value was missing");
}
WorkbookPackage linkedPackage(const std::filesystem::path& directory) {
    WorkbookPackage package;
    package.sourcePath = (directory / "Model.xlsx").string();
    package.workbook = Json::parse(R"({"revision":7,"sheets":[{"id":"model","name":"Model","cells":[
      {"row":1,"col":1,"input":"='[1]Inputs'!A1*2"},
      {"row":1,"col":2,"input":"=\"[1]Inputs!A1\""}]}]})");
    package.entries["xl/workbook.xml"] = R"(<workbook xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><externalReferences><externalReference r:id="link1"/></externalReferences></workbook>)";
    package.entries["xl/_rels/workbook.xml.rels"] = R"(<Relationships><Relationship Id="link1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLink" Target="externalLinks/externalLink1.xml"/></Relationships>)";
    package.entries["xl/externalLinks/externalLink1.xml"] = R"(<externalLink xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><externalBook r:id="source"><sheetNames><sheetName val="Inputs"/></sheetNames></externalBook></externalLink>)";
    package.entries["xl/externalLinks/_rels/externalLink1.xml.rels"] = R"(<Relationships><Relationship Id="source" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLinkPath" Target="Assumptions.xlsx" TargetMode="External"/></Relationships>)";
    return package;
}
void ownedResolutionMovesCanonicalStorage(const std::filesystem::path& directory) {
    Json owned={{"revision",4},{"sheets",Json::array({{{"id","s"},{"name","Source"},{"cells",Json::array({{{"row",1},{"col",1},{"input","=1+2"}}})}}})}};
    const auto* storage=&owned["sheets"][0]["cells"][0];
    auto prepared=prepareWorkbookLinksOwned(std::move(owned));
    require(&prepared.snapshot["sheets"][0]["cells"][0]==storage,"Owned resolution must reuse canonical cell storage instead of cloning it");
    require(prepared.filesRead==0&&prepared.diagnostics.empty(),"Owned resolution must not refresh source files");
    auto package=linkedPackage(directory);const auto expected=prepareWorkbookLinks(package.workbook,&package);auto captured=package.workbook;
    auto native=prepareWorkbookLinksOwned(std::move(captured),&package);
    require(native.snapshot==expected.snapshot&&native.diagnostics==expected.diagnostics&&native.links==expected.links,"Owned native-index/cache resolution must match the immutable API exactly");
}
void explicitRefreshLoadsLocalWorkbook(const std::filesystem::path& directory) {
    const auto source = Json::parse(R"({"revision":0,"sheets":[{"id":"inputs","name":"Inputs","cells":[
      {"row":1,"col":1,"input":"=3+4"}]}]})");
    writeWorkbook((directory / "Assumptions.xlsx").string(), source);
    const auto package = linkedPackage(directory);
    const auto original = package.workbook;
    WorkbookLinkOptions options; options.refresh = true;
    const auto prepared = prepareWorkbookLinks(package.workbook, &package, options);
    require(prepared.filesRead == 1, "Explicit refresh must read the linked local XLSX");
    require(prepared.links.size() == 1 && prepared.links[0]["state"] == "refreshed", "Link state must identify a successful refresh");
    const auto calculated = CalculationEngine().calculate(prepared.snapshot);
    require(valueAt(calculated, 1, 1).at("value") == 14, "External numeric indexes must resolve to the loaded source workbook");
    require(valueAt(calculated, 1, 2).at("value") == "[1]Inputs!A1", "External index normalization must preserve string literals");
    require(package.workbook == original, "Preparing links must preserve canonical formulas and metadata");
    require(prepared.snapshot.at("revision") == 7, "Link preparation must retain the current workbook revision");
}
void defaultResolutionUsesSavedCachesWithoutReadingSources(const std::filesystem::path& directory) {
    auto package = linkedPackage(directory);
    package.entries["xl/externalLinks/externalLink1.xml"] = R"(<externalLink xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><externalBook r:id="source"><sheetNames><sheetName val="Inputs"/></sheetNames><sheetDataSet><sheetData sheetId="0"><row r="1"><cell r="A1" t="n"><v>3</v></cell><cell r="B1" t="str"><v>=literal</v></cell></row></sheetData></sheetDataSet></externalBook></externalLink>)";
    package.workbook["sheets"][0]["cells"].push_back({{"row", 2}, {"col", 1}, {"input", "='[1]Inputs'!B1"}});
    const auto prepared = prepareWorkbookLinks(package.workbook, &package);
    require(prepared.filesRead == 0 && prepared.bytesRead == 0, "Default resolution must not read source files");
    require(prepared.links[0]["state"] == "cached", "Default link state must identify use of cached data");
    const auto result = CalculationEngine().calculate(prepared.snapshot);
    require(valueAt(result, 1, 1).at("value") == 6, "Saved external-link cache must supply the numeric value");
    require(valueAt(result, 2, 1).at("value") == "=literal", "Cached formula-looking strings must remain literal text");
    require(!prepared.diagnostics.empty() && prepared.diagnostics[0]["code"] == "external_link_cached",
            "Cached use must have an explicit diagnostic");
}
void missingSourcesReturnDiagnostics(const std::filesystem::path& directory) {
    auto package = linkedPackage(directory);
    package.workbook["links"] = {{{"index", 1}, {"name", "Assumptions.xlsx"}, {"path", "Missing.xlsx"}}};
    WorkbookLinkOptions options; options.refresh = true;
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 0, "A missing source must not count as a successful read");
    require(result.links[0]["state"] == "missing", "Missing sources need a visible link state");
    require(result.diagnostics[0]["code"] == "external_link_missing", "Missing sources need an actionable diagnostic");
    require(valueAt(CalculationEngine().calculate(result.snapshot), 1, 1).at("value") == "#REF!",
            "Missing sources must remain reference errors");
}
void relocatedSourcesPreserveWorkbookIdentity(const std::filesystem::path& directory) {
    auto package = linkedPackage(directory);
    package.workbook["links"] = {{{"index", 1}, {"name", "Assumptions.xlsx"}, {"path", "C:\\Old Folder\\Assumptions.xlsx"}}};
    WorkbookLinkOptions options; options.refresh = true;
    options.relocatedPaths["C:\\Old Folder\\Assumptions.xlsx"] = (directory / "Assumptions.xlsx").string();
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 1 && result.links[0]["state"] == "refreshed", "Explicit relocation must refresh the selected local source");
    require(valueAt(CalculationEngine().calculate(result.snapshot), 1, 1).at("value") == 14,
            "Relocation must preserve the original linked workbook identity");
    require(package.workbook["links"][0]["path"] == "C:\\Old Folder\\Assumptions.xlsx", "Relocation must not mutate canonical link paths");
}
void networkTargetsAreBlocked(const std::filesystem::path& directory) {
    auto package = linkedPackage(directory);
    package.workbook["links"] = {{{"index", 1}, {"name", "Assumptions.xlsx"}, {"path", "https://example.invalid/Assumptions.xlsx"}}};
    WorkbookLinkOptions options; options.refresh = true;
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 0 && result.links[0]["state"] == "blocked", "Network targets must be blocked before source reads");
    require(result.diagnostics[0]["code"] == "external_link_nonlocal", "Network targets need a local-source diagnostic");
}
void localFileUrisDecodeEscapedPaths(const std::filesystem::path& directory) {
    const auto folder = directory / "Folder with spaces";
    std::filesystem::create_directory(folder);
    std::filesystem::copy_file(directory / "Assumptions.xlsx", folder / "Assumptions.xlsx");
    auto package = linkedPackage(directory);
    auto target = "file://" + (folder / "Assumptions.xlsx").string();
    for (std::size_t position = 0; (position = target.find(' ', position)) != std::string::npos; position += 3) target.replace(position, 1, "%20");
    package.workbook["links"] = {{{"index", 1}, {"name", "Assumptions.xlsx"}, {"path", target}}};
    WorkbookLinkOptions options; options.refresh = true;
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 1, "Local file URIs must decode escaped path characters");
    require(valueAt(CalculationEngine().calculate(result.snapshot), 1, 1).at("value") == 14,
            "Local file URI sources must calculate through the actual engine");
}
void sourceReadBudgetsAreEnforced(const std::filesystem::path& directory) {
    const auto package = linkedPackage(directory);
    for (int boundary = 0; boundary < 4; ++boundary) {
        WorkbookLinkOptions options; options.refresh = true;
        if (boundary == 0) options.maxWorkbooks = 0;
        if (boundary == 1) options.maxTotalBytes = 1;
        if (boundary == 2) options.maxExpandedBytes = 1;
        if (boundary == 3) options.maxDepth = 0;
        const auto result = prepareWorkbookLinks(package.workbook, &package, options);
        require(result.filesRead == 0, "Source read budgets must prevent workbook decoding");
        require(result.links[0]["state"] == "limited", "Budget refusal must have a visible state");
        require(result.diagnostics[0]["code"] == "external_link_limit", "Budget refusal must have an actionable diagnostic");
    }
}
void linkedSourceDependenciesResolveRecursively(const std::filesystem::path& directory) {
    const auto leaf = Json::parse(R"({"revision":0,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"8"}]}]})");
    writeWorkbook((directory / "Leaf.xlsx").string(), leaf);
    auto branch = leaf;
    branch["sheets"][0]["cells"][0]["input"] = "='[Leaf.xlsx]Inputs'!A1+1";
    branch["links"] = {{{"name", "Leaf.xlsx"}, {"path", "Leaf.xlsx"}, {"enabled", true}}};
    writeWorkbook((directory / "Branch.xlsx").string(), branch);
    auto package = linkedPackage(directory);
    package.workbook["links"] = {{{"index", 1}, {"name", "Branch.xlsx"}, {"path", "Branch.xlsx"}}};
    WorkbookLinkOptions options; options.refresh = true;
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 2, "Linked source workbooks must resolve their own declared local dependencies");
    require(valueAt(CalculationEngine().calculate(result.snapshot), 1, 1).at("value") == 18,
            "Recursive local workbook links must calculate through the actual engine");
    options.maxDepth = 1;
    const auto limited = prepareWorkbookLinks(package.workbook, &package, options);
    require(limited.filesRead == 1, "Recursive refresh must respect the depth budget");
    require(limited.snapshot["links"][0]["workbook"]["links"][0]["state"] == "limited", "Depth limit must identify the unresolved dependency");
}
void workbookCyclesStopBeforeRepeatedReads(const std::filesystem::path& directory) {
    auto first = Json::parse(R"({"revision":0,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"='[CycleB.xlsx]Inputs'!A1"}]}],
      "links":[{"name":"CycleB.xlsx","path":"CycleB.xlsx","enabled":true}]})");
    auto second = first;
    second["sheets"][0]["cells"][0]["input"] = "='[CycleA.xlsx]Inputs'!A1";
    second["links"][0] = {{"name", "CycleA.xlsx"}, {"path", "CycleA.xlsx"}, {"enabled", true}};
    writeWorkbook((directory / "CycleA.xlsx").string(), first);
    writeWorkbook((directory / "CycleB.xlsx").string(), second);
    const auto package = readWorkbook((directory / "CycleA.xlsx").string());
    WorkbookLinkOptions options; options.refresh = true;
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 1, "A workbook cycle must stop before rereading the original workbook");
    require(result.diagnostics[0]["code"] == "external_link_cycle", "A workbook cycle needs an explicit diagnostic");
}
void invalidMetadataDoesNotAbortTheCurrentWorkbook(const std::filesystem::path& directory) {
    auto package = linkedPackage(directory);
    package.entries["xl/externalLinks/externalLink1.xml"] = "<broken";
    const auto result = prepareWorkbookLinks(package.workbook, &package);
    require(result.filesRead == 0, "Invalid link metadata must not trigger source reads");
    require(result.diagnostics[0]["code"] == "external_link_metadata", "Invalid metadata needs a diagnostic instead of aborting the workbook");
    require(result.snapshot["sheets"] == package.workbook["sheets"], "Invalid link metadata must preserve current cells");
}
void repeatedSourceAliasesShareOneRead(const std::filesystem::path& directory) {
    auto package = linkedPackage(directory);
    package.workbook["links"] = {{{"index", 2}, {"name", "Assumptions.xlsx"}, {"path", "./Assumptions.xlsx"}}};
    package.workbook["sheets"][0]["cells"].push_back({{"row", 1}, {"col", 3}, {"input", "='[2]Inputs'!A1*2"}});
    WorkbookLinkOptions options; options.refresh = true; options.maxWorkbooks = 1;
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 1 && result.links.size() == 2, "Repeated source aliases must share one read while retaining distinct external indexes");
    const auto calculated = CalculationEngine().calculate(result.snapshot);
    require(valueAt(calculated, 1, 1).at("value") == 14 && valueAt(calculated, 1, 3).at("value") == 14,
            "Distinct external indexes for one canonical source must both calculate");
}
void relativeRelationshipUrisDecodeEscapedFileNames(const std::filesystem::path& directory) {
    std::filesystem::copy_file(directory / "Assumptions.xlsx", directory / "Assumptions Model.xlsx");
    auto package = linkedPackage(directory);
    package.entries["xl/externalLinks/_rels/externalLink1.xml.rels"] = R"(<Relationships><Relationship Id="source" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLinkPath" Target="Assumptions%20Model.xlsx" TargetMode="External"/></Relationships>)";
    WorkbookLinkOptions options; options.refresh = true;
    const auto result = prepareWorkbookLinks(package.workbook, &package, options);
    require(result.filesRead == 1, "Relative relationship URIs must decode escaped file names");
    require(valueAt(CalculationEngine().calculate(result.snapshot), 1, 1).at("value") == 14,
            "Escaped relative relationship file names must resolve correctly");
}
void malformedCachedValuesAreRejected(const std::filesystem::path& directory) {
    for (const auto& [type, value] : std::vector<std::pair<std::string, std::string>>{{"b", "garbage"}, {"n", "=9+1"}}) {
        auto package = linkedPackage(directory);
        package.entries["xl/externalLinks/externalLink1.xml"] =
            "<externalLink xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><externalBook r:id=\"source\"><sheetNames><sheetName val=\"Inputs\"/></sheetNames><sheetDataSet><sheetData sheetId=\"0\"><row r=\"1\"><cell r=\"A1\" t=\"" + type + "\"><v>" + value + "</v></cell></row></sheetData></sheetDataSet></externalBook></externalLink>";
        const auto result = prepareWorkbookLinks(package.workbook, &package);
        require(result.filesRead == 0 && result.diagnostics[0]["code"] == "external_link_metadata",
                "Malformed cached types must be diagnosed instead of coerced or executed");
        require(valueAt(CalculationEngine().calculate(result.snapshot), 1, 1).at("value") == "#REF!",
                "Rejected cached values must remain unavailable references");
    }
}
void retainedRefreshesSupportRecalculationWithoutSourceReads(const std::filesystem::path& directory) {
    const auto package = linkedPackage(directory);
    WorkbookLinkOptions refresh; refresh.refresh = true;
    const auto live = prepareWorkbookLinks(package.workbook, &package, refresh);
    auto edited = package.workbook;
    edited["sheets"][0]["cells"][0]["input"] = "='[1]Inputs'!A1*3";
    WorkbookLinkOptions retained; retained.retainedLinks = live.snapshot["links"];
    const auto result = prepareWorkbookLinks(edited, &package, retained);
    require(result.filesRead == 0 && result.links[0]["state"] == "retained", "Ordinary recalculation must reuse refreshed source data without reading files");
    require(valueAt(CalculationEngine().calculate(result.snapshot), 1, 1).at("value") == 21,
            "Edited formulas must calculate using the retained refreshed source");
}
}
int main() {
    try {
        char pattern[] = "/tmp/tigersheets-links-XXXXXX";
        const auto* created = mkdtemp(pattern); require(created, "Temporary directory creation");
        const std::filesystem::path directory(created);
        ownedResolutionMovesCanonicalStorage(directory);
        explicitRefreshLoadsLocalWorkbook(directory);
        defaultResolutionUsesSavedCachesWithoutReadingSources(directory);
        missingSourcesReturnDiagnostics(directory);
        relocatedSourcesPreserveWorkbookIdentity(directory);
        networkTargetsAreBlocked(directory);
        localFileUrisDecodeEscapedPaths(directory);
        sourceReadBudgetsAreEnforced(directory);
        linkedSourceDependenciesResolveRecursively(directory);
        workbookCyclesStopBeforeRepeatedReads(directory);
        invalidMetadataDoesNotAbortTheCurrentWorkbook(directory);
        repeatedSourceAliasesShareOneRead(directory);
        relativeRelationshipUrisDecodeEscapedFileNames(directory);
        malformedCachedValuesAreRejected(directory);
        retainedRefreshesSupportRecalculationWithoutSourceReads(directory);
        std::filesystem::remove_all(directory);
        std::cout << "workbook links tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "workbook links test failed: " << error.what() << '\n'; return 1;
    }
}
