#include "tigersheets/calculation.hpp"
#include "tigersheets/pivot.hpp"
#include "tigersheets/workbook_links.hpp"
#include "tigersheets/vba.hpp"
#include "tigersheets/workbook_tools.hpp"
#include "tigersheets/financial_workflows.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
namespace {
using namespace tigersheets;
volatile std::sig_atomic_t cancelled=0;
void cancelSignal(int){cancelled=1;}
constexpr std::uint64_t byteLimit=1024ull*1024*1024;
bool requiresCachedConsent(const Json& diagnostics){for(const auto& entry:diagnostics)if(entry.is_object()&&entry.value("code",std::string()).starts_with("external_link_"))return true;return false;}
void requireLinkConsent(const Json& request,Json& diagnostics){if(!request.value("allowCachedLinks",false)&&requiresCachedConsent(diagnostics)){diagnostics.push_back({{"code","cached_link_consent_required"},{"message","Linked source values are cached or unavailable. Explicitly allow saved values before creating this report."}});throw std::runtime_error("cached_link_consent_required");}}
void validateRequest(const Json& request){
 if(!request.is_object()||request.value("version",0)!=1||!request.at("workbook").is_object()||!request.at("retainedLinks").is_array()||!request.at("contextGeneration").is_number_integer()||(!request.at("contextGeneration").is_number_unsigned()&&request.at("contextGeneration").get<std::int64_t>()<0))throw std::runtime_error("Invalid report request protocol");
 const auto& revision=request.at("workbook").at("revision");if(!revision.is_number_integer()||(!revision.is_number_unsigned()&&revision.get<std::int64_t>()<0))throw std::runtime_error("Invalid report workbook revision");
 const auto kind=request.value("kind",std::string("pivot"));if(kind=="financial_model"){if(!request.at("definition").is_object()||!request.at("action").is_string())throw std::runtime_error("Invalid financial workflow request");return;}if(kind=="workbook_tool"){if(!request.at("tool").is_string()||request.at("tool").get_ref<const std::string&>().size()>128||!request.at("args").is_object()||!request.at("previousProposal").is_array()||request.at("previousProposal").size()>10000)throw std::runtime_error("Invalid isolated workbook tool request");return;}if(kind!="pivot"&&kind!="report")throw std::runtime_error("Unknown report worker request kind");
 if(!request.at("definition").is_object())throw std::runtime_error("Missing report definition");
 const auto& source=request.at("definition").at("source");const auto& output=request.at("definition").at("output");
 for(const auto* coordinate:{"row1","row2","col1","col2"})if(!source.at(coordinate).is_number_integer())throw std::runtime_error("Report source coordinates must be integers");
 for(const auto* coordinate:{"row1","col1"})if(!output.at(coordinate).is_number_integer())throw std::runtime_error("Report output coordinates must be integers");
 const auto r1=source.at("row1").get<std::int64_t>(),r2=source.at("row2").get<std::int64_t>(),c1=source.at("col1").get<std::int64_t>(),c2=source.at("col2").get<std::int64_t>();
 if(r1<1||r2<=r1||r2>1048576||c1<1||c2<c1||c2>16384||r2-r1>100000||c2-c1+1>10)throw std::runtime_error("Report source exceeds 100000 data records or ten fields");
 if(output.at("row1").get<std::int64_t>()<1||output.at("row1").get<std::int64_t>()>1048576||output.at("col1").get<std::int64_t>()<1||output.at("col1").get<std::int64_t>()>16384)throw std::runtime_error("Report output address is invalid");
 const auto& values=request.at("definition").at("values");if(!values.is_array()||values.empty()||values.size()>4)throw std::runtime_error("Report requires one to four measures");
}
}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QString input,output;
 for(int i=1;i+1<argc;i+=2){const auto option=QString::fromLocal8Bit(argv[i]);if(option=="--request")input=QString::fromLocal8Bit(argv[i+1]);else if(option=="--response")output=QString::fromLocal8Bit(argv[i+1]);else return 2;}
 if(argc!=5||input.isEmpty()||output.isEmpty()||QFileInfo(input).absolutePath()!=QFileInfo(output).absolutePath())return 2;
 std::signal(SIGTERM,cancelSignal);std::signal(SIGINT,cancelSignal);
 Json result={{"version",1},{"revision",nullptr},{"contextGeneration",nullptr},{"status","error"},{"diagnostics",Json::array()}};
 CalculationEngine engine;std::atomic<int> phase{0};
 std::jthread heartbeat([&](std::stop_token stop){const char* stages[]={"Reading report inputs","Preparing retained workbook links","Calculating source formulas","Grouping report records","Writing report result"};while(!stop.stop_requested()){if(cancelled)engine.cancel();std::cout<<Json{{"version",1},{"type","progress"},{"stage",stages[phase.load()]}}.dump()<<std::endl;std::this_thread::sleep_for(std::chrono::milliseconds(50));}});
 try{
  if(std::filesystem::file_size(input.toStdString())>byteLimit)throw std::runtime_error("Report input exceeds the 1 GiB protocol limit");
  std::ifstream file(input.toStdString(),std::ios::binary);if(!file)throw std::runtime_error("Cannot open private report inputs");auto request=Json::parse(file);
  result["revision"]=request.at("workbook").at("revision");result["contextGeneration"]=request.at("contextGeneration");
  if(request.contains("workbookId"))result["workbookId"]=request.at("workbookId");
  validateRequest(request);
  if(cancelled)throw std::runtime_error("Report refresh cancelled");
  phase=1;
  WorkbookLinkOptions options;options.retainedLinks=request.at("retainedLinks");WorkbookPackage package;const WorkbookPackage* sourcePackage=nullptr;
  if(request.contains("nativeLinkParts")){
   const auto& parts=request.at("nativeLinkParts");if(!parts.is_object())throw std::runtime_error("Native link parts must be an XML map");
   for(const auto& [part,xml]:parts.items()){
    if((part!="xl/workbook.xml"&&part!="xl/_rels/workbook.xml.rels"&&!part.starts_with("xl/externalLinks/"))||!xml.is_string())throw std::runtime_error("Unsupported native link package part");
    package.entries[part]=xml.get<std::string>();
   }
   package.sourcePath=request.value("sourcePath",std::string());sourcePackage=&package;
  }
  if(request.value("kind",std::string())=="financial_model"){
   const auto& definition=request.at("definition");const auto action=request.at("action").get<std::string>();const auto kind=definition.value("kind",std::string());
   const bool calculates=(action=="run"||action=="save")&&(kind=="forecast"||kind=="scenario_comparison"||kind=="sensitivity"||(action=="run"&&(kind=="goal_seek"||kind=="solver")));
   CalculationContext context;context.generation=request.at("contextGeneration");context.allowCachedLinks=request.value("allowCachedLinks",false);context.generationIsCurrent=[&](std::uint64_t generation){return generation==request.at("contextGeneration").get<std::uint64_t>()&&!cancelled;};
   if(calculates){auto prepared=prepareWorkbookLinksOwned(std::move(request.at("workbook")),sourcePackage,options);context.snapshot=std::move(prepared.snapshot);result["diagnostics"]=std::move(prepared.diagnostics);requireLinkConsent(request,result["diagnostics"]);}
   else context.snapshot=std::move(request.at("workbook"));
   context.evaluator=[&](const Json& candidate){
    if(cancelled)throw std::runtime_error("Financial workflow cancelled");phase=2;const auto vba=candidate.value("vba",Json::object());std::string source;if(vba.value("enabled",false))for(const auto& module:vba.value("modules",Json::array()))source+=module.value("source",std::string())+"\n";
    auto calculated=source.empty()?engine.calculate(candidate):engine.calculate(candidate,[&](const std::string& name,const Json& args){return evaluateVbaFunction(candidate,source,name,args,[]{return cancelled!=0;});});
    for(const auto& diagnostic:result["diagnostics"])if(std::find(calculated["diagnostics"].begin(),calculated["diagnostics"].end(),diagnostic)==calculated["diagnostics"].end())calculated["diagnostics"].push_back(diagnostic);
    for(const auto& diagnostic:calculated.value("diagnostics",Json::array()))if(std::find(result["diagnostics"].begin(),result["diagnostics"].end(),diagnostic)==result["diagnostics"].end())result["diagnostics"].push_back(diagnostic);
    requireLinkConsent(request,result["diagnostics"]);phase=3;return calculated;
   };
   auto completed=executeFinancialWorkflow(context.snapshot,definition,action,context,[]{return cancelled!=0;});for(auto& [key,value]:completed.items())result[key]=std::move(value);result["kind"]="financial_model";result["status"]="ok";
   if(cancelled)throw std::runtime_error("Financial workflow cancelled");
  }else if(request.value("kind",std::string())=="workbook_tool"){
   result["kind"]="workbook_tool";const auto generation=request.at("contextGeneration").get<std::uint64_t>();WorkbookSession session(std::move(request.at("workbook")));WorkbookTools tools(session,engine.functions());
   tools.calculationContextIsCurrent=[&](std::uint64_t candidate){return candidate==generation&&!cancelled;};
   tools.financialContext=[&](const Json& candidate){auto prepared=prepareWorkbookLinks(candidate,sourcePackage,options);CalculationContext context;context.snapshot=std::move(prepared.snapshot);context.generation=generation;context.allowCachedLinks=request.value("allowCachedLinks",false);return context;};
   tools.calculate=[&](const Json& candidate){
    if(cancelled)throw std::runtime_error("Workbook tool cancelled");phase=2;auto prepared=prepareWorkbookLinks(candidate,sourcePackage,options);bool financial=false;if(request.at("args").contains("operations")&&request.at("args").at("operations").is_array())for(const auto& operation:request.at("args").at("operations"))if(operation.is_object()){const auto type=operation.value("type",std::string());financial|=type=="save_financial_model"||type=="run_financial_model"||type=="apply_financial_scenario"||type=="delete_financial_model";}if(request.at("tool")=="workbook_v1_preview"&&!financial)prepared.snapshot["_tigersheetsComputedResults"]=true;else prepared.snapshot.erase("_tigersheetsComputedResults");const auto& book=prepared.snapshot;const auto vba=book.value("vba",Json::object());std::string source;if(vba.value("enabled",false))for(const auto& module:vba.value("modules",Json::array()))source+=module.value("source",std::string())+"\n";
    auto calculated=source.empty()?engine.calculate(book):engine.calculate(book,[&](const std::string& name,const Json& args){return evaluateVbaFunction(book,source,name,args,[]{return cancelled!=0;});});
    for(const auto& diagnostic:prepared.diagnostics){calculated["diagnostics"].push_back(diagnostic);if(std::find(result["diagnostics"].begin(),result["diagnostics"].end(),diagnostic)==result["diagnostics"].end())result["diagnostics"].push_back(diagnostic);}
    Json links=Json::array();for(const auto& link:prepared.snapshot.value("links",Json::array()))links.push_back({{"name",link.value("name",std::string())},{"path",link.value("originalPath",link.value("path",std::string()))},{"state",link.value("state",std::string("unavailable"))}});
    calculated["calculationContext"]={{"generation",generation},{"allowCachedLinks",request.value("allowCachedLinks",false)},{"links",std::move(links)},{"diagnostics",prepared.diagnostics}};phase=3;return calculated;
   };
   const auto& previous=request.at("previousProposal");if(!previous.empty()){std::string error;if(session.preview(session.snapshotView().at("revision"),previous,&error).is_null())throw std::runtime_error(error);if(!tools.adoptProposal({session.snapshotView().at("revision"),std::make_shared<const Json>(previous)},&error))throw std::runtime_error(error);}
   result["toolReply"]=tools.invoke(request.at("tool").get<std::string>(),request.at("args"));if(cancelled)throw std::runtime_error("Workbook tool cancelled");result["operations"]=result["toolReply"].value("status",std::string())=="preview"?*tools.proposalState().operations:Json::array();result["status"]="ok";
  }else{
  auto prepared=prepareWorkbookLinksOwned(std::move(request.at("workbook")),sourcePackage,options);prepared.snapshot["_tigersheetsComputedResults"]=true;
  result["diagnostics"]=prepared.diagnostics;requireLinkConsent(request,result["diagnostics"]);const auto& book=prepared.snapshot;phase=2;
  bool nativeExternal=false;for(const auto& [part,xml]:package.entries)if(part.starts_with("xl/externalLinks/")||xml.find("externalReferences")!=std::string::npos||xml.find("relationships/externalLink")!=std::string::npos)nativeExternal=true;
  const bool unlinked=(!book.contains("links")||book.at("links").empty())&&request.at("retainedLinks").empty()&&!nativeExternal&&!requiresCachedConsent(result["diagnostics"]);
  PivotGeneration generated;
  if(request.contains("sourceCache")&&unlinked){
   auto& cache=request.at("sourceCache");const auto& definition=request.at("definition");const auto& range=definition.at("source");
   if(!cache.is_object()||cache.at("revision")!=book.at("revision")||cache.at("contextGeneration")!=request.at("contextGeneration")||cache.at("source")!=range)throw std::runtime_error("Current report source cache does not match workbook revision, calculation context or source range");
   const auto& calculation=cache.at("calculation");if(!calculation.is_object()||calculation.at("revision")!=book.at("revision")||calculation.at("contextGeneration")!=request.at("contextGeneration")||calculation.value("status",std::string())!="ok"||!calculation.value("converged",false))throw std::runtime_error("Current report source cache requires matching converged calculation provenance");
   if(!cache.at("fields").is_array()||cache.at("fields").size()!=range.at("col2").get<std::size_t>()-range.at("col1").get<std::size_t>()+1||!cache.at("records").is_array()||cache.at("records").size()!=range.at("row2").get<std::size_t>()-range.at("row1").get<std::size_t>())throw std::runtime_error("Current report source cache has the wrong field or record count");
   if(book.value("settings",Json::object()).value("date1904",false))throw std::runtime_error("1904 date-system report calculation is unsupported");
   phase=3;generated=generatePivotFromCache(book,definition,std::move(cache.at("fields")),std::move(cache.at("records")),request.at("contextGeneration").get<std::uint64_t>(),[]{return cancelled!=0;});
  }else{
   const auto vba=book.value("vba",Json::object());std::string source;
   if(vba.value("enabled",false))for(const auto& module:vba.value("modules",Json::array()))source+=module.value("source",std::string())+"\n";
   const auto calculated=source.empty()?engine.calculate(book):engine.calculate(book,[&](const std::string& name,const Json& args){return evaluateVbaFunction(book,source,name,args,[]{return cancelled!=0;});});
   for(const auto& diagnostic:calculated.value("diagnostics",Json::array()))result["diagnostics"].push_back(diagnostic);
   requireLinkConsent(request,result["diagnostics"]);
   if(cancelled||calculated.value("status",std::string())=="cancelled")throw std::runtime_error("Report refresh cancelled");
   if(calculated.value("status",std::string())!="ok")throw std::runtime_error("Report source calculation did not converge or is unsupported");
   phase=3;generated=generatePivot(book,calculated,request.at("definition"),request.at("contextGeneration").get<std::uint64_t>(),[]{return cancelled!=0;});
  }
  Json links=Json::array();for(const auto& link:prepared.snapshot.value("links",Json::array()))links.push_back({{"name",link.value("name",std::string())},{"path",link.value("originalPath",link.value("path",std::string()))},{"state",link.value("state",std::string("unavailable"))}});
  generated.definition["provenance"]={{"links",std::move(links)},{"diagnostics",result["diagnostics"]},{"allowCachedLinks",request.value("allowCachedLinks",false)}};
  if(cancelled)throw std::runtime_error("Report refresh cancelled");
  result["definition"]=std::move(generated.definition);result["cells"]=std::move(generated.cells);result["status"]="ok";
  }
 }catch(const std::exception& error){result["status"]=cancelled?"cancelled":"error";result["diagnostics"].push_back({{"code",cancelled?"cancelled":"report_error"},{"message",error.what()}});}
 phase=4;
 try{const auto bytes=result.dump();if(bytes.size()>byteLimit)throw std::runtime_error("Report output exceeds the 1 GiB protocol limit");QSaveFile destination(output);if(!destination.open(QIODevice::WriteOnly)||!destination.setPermissions(QFile::ReadOwner|QFile::WriteOwner)||destination.write(bytes.data(),static_cast<qint64>(bytes.size()))!=static_cast<qint64>(bytes.size())||!destination.commit())throw std::runtime_error("Cannot publish private report result");}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 return 0;
}
