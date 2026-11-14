#include "tigersheets/report_worker.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool until(const std::function<bool()>& done,int limit=5000){QElapsedTimer timer;timer.start();while(!done()&&timer.elapsed()<limit){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}return done();}
std::shared_ptr<const Json> request(){return std::make_shared<const Json>(Json::parse(R"json({"version":1,"contextGeneration":17,"retainedLinks":[],"workbook":{"revision":4,"sheets":[{"id":"s","name":"Data","cells":[{"row":1,"col":1,"input":"Region"},{"row":1,"col":2,"input":"Revenue"},{"row":2,"col":1,"input":"East"},{"row":2,"col":2,"input":"=3+4"},{"row":3,"col":1,"input":"West"},{"row":3,"col":2,"input":"11"}]},{"id":"r","name":"Report","cells":[]}]},"definition":{"id":"p","name":"RevenueReport","source":{"sheet":"s","row1":1,"col1":1,"row2":3,"col2":2},"rows":[{"field":"Region"}],"values":[{"field":"Revenue","aggregate":"sum","name":"Revenue"}],"output":{"sheet":"r","row1":1,"col1":1}}})json"));}
void nativeJob(const QString& executable){ReportWorker worker;Json result;QString error;int progress=0;worker.onResult=[&](const Json& value){result=value;};worker.onError=[&](QString message){error=message;};worker.onProgress=[&](QString){++progress;};
 require(worker.start(request(),executable),"An immutable report job must start asynchronously");
 require(until([&]{return !worker.running();}),"The native report worker must finish");
 require(error.isEmpty(),qPrintable(error));require(result.value("status",std::string())=="ok","Native report computation must publish a successful generation");
 require(result.at("revision")==4&&result.at("contextGeneration")==17,"Worker result must retain revision and calculation generation");
 require(result.at("definition").at("layout").at("grandTotals")[0].at("value")==18,"Worker must calculate formulas before aggregating real source records");
 require(result.at("cells").size()==8&&progress>0,"Worker must return owned report cells and observable progress");
}
void retainedLinksAndEnabledFunctions(const QString& executable){
 auto value=*request();value["workbook"]["sheets"][0]["cells"][3]["input"]="='[Drivers.xlsx]Inputs'!A1+Triple(1)";
 value["workbook"]["vba"]={{"enabled",true},{"modules",Json::array({{{"name","Functions"},{"source","Function Triple(x)\nTriple=x*3\nEnd Function"}}})}};
 const auto link=Json::parse(R"json({"name":"Drivers.xlsx","path":"/never-read/Drivers.xlsx","enabled":true,"workbook":{"revision":0,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"7"}]}]}})json");
 value["workbook"]["links"]=Json::array({{{"name","Drivers.xlsx"},{"path","/never-read/Drivers.xlsx"},{"enabled",true}}});value["retainedLinks"]=Json::array({link});value["sourceCache"]={{"revision",4},{"contextGeneration",17},{"fields",Json::array()},{"records",Json::array()}};
 ReportWorker worker;Json result;worker.onResult=[&](const Json& completed){result=completed;};require(worker.start(std::make_shared<const Json>(std::move(value)),executable),"Retained-source report job starts");require(until([&]{return !worker.running();}),"Retained-source job finishes");require(worker.error().isEmpty(),qPrintable(worker.error()));require(result["definition"]["layout"]["grandTotals"][0]["value"]==21,"Native report worker must compose frozen linked values with enabled read-only VBA functions");
}
void compactCurrentSourceCache(const QString& executable){
 auto value=*request();auto& book=value["workbook"];for(auto& sheet:book["sheets"])sheet["cells"]=Json::array();
 value["sourceCache"]={{"revision",4},{"contextGeneration",17},{"calculation",{{"revision",4},{"contextGeneration",17},{"status","ok"},{"converged",true}}},{"source",value["definition"]["source"]},{"fields",Json::array({{{"name","Region"},{"numberFormat","General"},{"items",Json::array({{{"type","string"},{"value","East"}},{{"type","string"},{"value","West"}}})}},{{"name","Revenue"},{"numberFormat","General"},{"items",Json::array({{{"type","number"},{"value",80}},{{"type","number"},{"value",11}}})}}})},{"records",Json::array({Json::array({0,0}),Json::array({1,1})})}};
 ReportWorker worker;Json result;worker.onResult=[&](const Json& completed){result=completed;};require(worker.start(std::make_shared<const Json>(value),executable),"Compact current-source job starts");require(until([&]{return !worker.running();}),"Compact current-source job completes");require(worker.error().isEmpty(),qPrintable(worker.error()));require(result["definition"]["layout"]["grandTotals"][0]["value"]==91,"A bound compact cache must avoid recalculation and retain changed-source current values");require(result["definition"]["cache"]["sourceRevision"]==4&&result["definition"]["cache"]["contextGeneration"]==17&&result["definition"]["provenance"]["links"].empty(),"Compact reports preserve revision, context and unlinked provenance");
 for(const auto* mismatch:{"revision","contextGeneration","source","records","date1904","nonconverged","unqualified","foreignCalculation","cacheType","fieldWidth","index","type","fractionalSource","fractionalOutput"}){auto bad=value;if(std::string(mismatch)=="fractionalSource"){bad["definition"]["source"]["row2"]=3.5;bad["sourceCache"]["source"]["row2"]=3.5;}else if(std::string(mismatch)=="fractionalOutput")bad["definition"]["output"]["row1"]=1.5;else if(std::string(mismatch)=="nonconverged")bad["sourceCache"]["calculation"]["converged"]=false;else if(std::string(mismatch)=="unqualified")bad["sourceCache"]["calculation"]["status"]="unsupported";else if(std::string(mismatch)=="foreignCalculation")bad["sourceCache"]["calculation"]["contextGeneration"]=999;else if(std::string(mismatch)=="cacheType")bad["sourceCache"]=1;else if(std::string(mismatch)=="fieldWidth")bad["sourceCache"]["fields"].erase(bad["sourceCache"]["fields"].begin());else if(std::string(mismatch)=="index")bad["sourceCache"]["records"][0][0]=999;else if(std::string(mismatch)=="type")bad["sourceCache"]["fields"][0]["items"][0]["type"]="error";else if(std::string(mismatch)=="source")bad["sourceCache"]["source"]["row2"]=4;else if(std::string(mismatch)=="records")bad["sourceCache"]["records"].erase(bad["sourceCache"]["records"].begin());else if(std::string(mismatch)=="date1904")bad["workbook"]["settings"]={{"date1904",true}};else bad["sourceCache"][mismatch]=999;ReportWorker denied;int delivered=0;denied.onResult=[&](const Json&){++delivered;};require(denied.start(std::make_shared<const Json>(std::move(bad)),executable),"Mismatched compact job starts");require(until([&]{return !denied.running();}),"Mismatched compact job finishes");require(delivered==0&&!denied.error().isEmpty(),"Foreign revision/context/source, wrong record shape or unsupported dates must never publish compact results");}
}
void fullRecordCapacity(const QString& executable){
 auto value=*request();value["definition"]["source"]["row2"]=100001;value["definition"]["source"]["col2"]=10;
 for(int col=3;col<=10;++col)value["workbook"]["sheets"][0]["cells"].push_back({{"row",1},{"col",col},{"input","Field"+std::to_string(col)}});
 ReportWorker worker;Json result;worker.onResult=[&](const Json& completed){result=completed;};require(worker.start(std::make_shared<const Json>(std::move(value)),executable),"Full-capacity report starts");require(until([&]{return !worker.running();},120000),"Full-capacity report completes within its bounded worker deadline");require(worker.error().isEmpty(),qPrintable(worker.error()));require(result["definition"]["cache"]["records"].size()==100000&&result["definition"]["cache"]["fields"].size()==10,"The worker must accept100000 data records plus headers across10 fields");
}
void cachedLinksRequireExplicitConsent(const QString& executable){
 auto value=*request();value["workbook"]["sheets"][0]["cells"][3]["input"]="='[Missing.xlsx]Inputs'!A1";
 value["workbook"]["links"]=Json::parse(R"json([{"name":"Missing.xlsx","path":"/never-read/Missing.xlsx","enabled":true,"cachedWorkbook":{"revision":0,"sheets":[{"id":"i","name":"Inputs","cells":[{"row":1,"col":1,"input":"7"}]}]}}])json");
 value["sourceCache"]={{"revision",4},{"contextGeneration",17},{"fields",Json::array()},{"records",Json::array()}};
 ReportWorker denied;int delivered=0;denied.onResult=[&](const Json&){++delivered;};require(denied.start(std::make_shared<const Json>(value),executable),"Cached-source refusal job starts");require(until([&]{return !denied.running();}),"Cached-source refusal finishes");require(delivered==0&&denied.error().contains("cached_link_consent_required"),"Financial reports must explicitly refuse saved cached links before consent");
 value["allowCachedLinks"]=true;ReportWorker allowed;Json result;allowed.onResult=[&](const Json& completed){result=completed;};require(allowed.start(std::make_shared<const Json>(std::move(value)),executable),"Explicit cached-source job starts");require(until([&]{return !allowed.running();}),"Explicit cached-source job finishes");require(allowed.error().isEmpty(),qPrintable(allowed.error()));require(result["definition"]["layout"]["grandTotals"][0]["value"]==18&&result["definition"]["provenance"]["links"][0]["state"]=="cached"&&!result["definition"]["provenance"]["diagnostics"].empty(),"Explicit cached-source report must retain visible saved-value provenance");
}
void nativeLinkPartsRemainAvailableWithoutSourceReads(const QString& executable){
 auto value=*request();value["allowCachedLinks"]=true;value["sourcePath"]="/never-read/Model.xlsx";value["workbook"]["sheets"][0]["cells"][3]["input"]="='[1]Inputs'!A1";
 value["nativeLinkParts"]={{"xl/workbook.xml",R"(<workbook xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><externalReferences><externalReference r:id="rExt"/></externalReferences></workbook>)"},{"xl/_rels/workbook.xml.rels",R"(<Relationships><Relationship Id="rExt" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLink" Target="externalLinks/externalLink1.xml"/></Relationships>)"},{"xl/externalLinks/externalLink1.xml",R"(<externalLink xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><externalBook r:id="rSource"><sheetNames><sheetName val="Inputs"/></sheetNames><sheetDataSet><sheetData sheetId="0"><row r="1"><cell r="A1" t="n"><v>7</v></cell></row></sheetData></sheetDataSet></externalBook></externalLink>)"},{"xl/externalLinks/_rels/externalLink1.xml.rels",R"(<Relationships><Relationship Id="rSource" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLinkPath" Target="Missing.xlsx" TargetMode="External"/></Relationships>)"}};
 value["sourceCache"]={{"revision",4},{"contextGeneration",17},{"fields",Json::array()},{"records",Json::array()}};
 ReportWorker worker;Json result;worker.onResult=[&](const Json& completed){result=completed;};require(worker.start(std::make_shared<const Json>(std::move(value)),executable),"In-memory native link-cache job starts");require(until([&]{return !worker.running();}),"Native link-cache job finishes");require(worker.error().isEmpty(),qPrintable(worker.error()));require(result["definition"]["layout"]["grandTotals"][0]["value"]==18&&result["definition"]["provenance"]["links"][0]["name"]=="Missing.xlsx","The worker must resolve native link indexes and saved cache parts without opening linked files");
}
QString fakeWorker(QTemporaryDir& directory,const QString& mode){
 const auto path=directory.filePath("worker-"+mode);QFile file(path);require(file.open(QIODevice::WriteOnly),"Fake worker can be written");
 file.write(R"PY(#!/usr/bin/python3
import json,os,signal,stat,sys,time
request=sys.argv[sys.argv.index('--request')+1]
response=sys.argv[sys.argv.index('--response')+1]
with open(__file__+'.job','w') as log: json.dump({'directory':os.path.dirname(request),'directoryMode':stat.S_IMODE(os.stat(os.path.dirname(request)).st_mode),'inputMode':stat.S_IMODE(os.stat(request).st_mode)},log)
with open(request) as source: job=json.load(source)
if 'cancel' in __file__:
 signal.signal(signal.SIGTERM,signal.SIG_IGN)
 print(json.dumps({'version':1,'type':'progress','stage':'Waiting'}),flush=True)
 while True: time.sleep(1)
if 'oversize' in __file__:
 with open(response,'wb') as output: output.truncate(1024*1024*1024+1)
else:
 result={'version':1,'revision':job['workbook']['revision'],'contextGeneration':job['contextGeneration'],'status':'ok','definition':{},'cells':[],'diagnostics':[]}
 if 'foreign' in __file__: result['revision']+=1
 if 'context' in __file__: result['contextGeneration']+=1
 with open(response,'w') as output: json.dump(result,output)
)PY");file.close();file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;
}
Json jobInfo(const QString& program){QFile file(program+".job");require(file.open(QIODevice::ReadOnly),"Worker job information exists");return Json::parse(file.readAll().toStdString());}
void foreignResultsAndOversizedFilesAreRefused(){
 for(const auto& mode:{QString("foreign"),QString("context"),QString("oversize")}){QTemporaryDir folder;const auto executable=fakeWorker(folder,mode);ReportWorker worker;int delivered=0;worker.onResult=[&](const Json&){++delivered;};
  require(worker.start(request(),executable),"Protocol test job starts");require(until([&]{return !worker.running();}),"Invalid response terminates its job");require(delivered==0&&!worker.error().isEmpty(),"Foreign revision/context and oversized output must never be published");
  const auto info=jobInfo(executable);require(info.at("directoryMode")==0700&&info.at("inputMode")==0600,"Report inputs and job directory must remain private");const auto path=QString::fromStdString(info.at("directory"));require(until([&]{return !QFileInfo::exists(path);}),"Failed report jobs clean their private files");
 }
}
void cancellationEscalatesWithoutPublishing(){
 QTemporaryDir folder;const auto executable=fakeWorker(folder,"cancel");ReportWorker worker;int delivered=0;bool waiting=false,cancelling=false;worker.onResult=[&](const Json&){++delivered;};worker.onProgress=[&](QString stage){waiting|=stage=="Waiting";cancelling|=stage.startsWith("Cancelling");};
 require(worker.start(request(),executable),"Cancellation job starts");require(!worker.start(request(),executable),"Only one report job can be active");require(until([&]{return waiting;}),"The child reaches its interruptible stage");const auto info=jobInfo(executable);QElapsedTimer elapsed;elapsed.start();worker.stop();require(cancelling&&elapsed.elapsed()<100,"Cancellation feedback must be immediate");
 require(until([&]{return !worker.running();},500)&&elapsed.elapsed()<500,"A noncooperative child must be killed within500ms");require(delivered==0,"Cancelled results must never reach onResult");const auto path=QString::fromStdString(info.at("directory"));require(until([&]{return !QFileInfo::exists(path);}),"Cancelled jobs remove their private files");
}
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{require(argc>1,"Pass the native report-worker executable");const std::string selected=argc>2?argv[2]:"all";if(selected=="all"||selected=="native"){nativeJob(QString::fromLocal8Bit(argv[1]));retainedLinksAndEnabledFunctions(QString::fromLocal8Bit(argv[1]));}if(selected=="all"||selected=="warm")compactCurrentSourceCache(QString::fromLocal8Bit(argv[1]));if(selected=="all"||selected=="nativecache")nativeLinkPartsRemainAvailableWithoutSourceReads(QString::fromLocal8Bit(argv[1]));if(selected=="all"||selected=="cached")cachedLinksRequireExplicitConsent(QString::fromLocal8Bit(argv[1]));if(selected=="all"||selected=="capacity")fullRecordCapacity(QString::fromLocal8Bit(argv[1]));if(selected=="all"||selected=="protocol")foreignResultsAndOversizedFilesAreRefused();if(selected=="all"||selected=="cancel")cancellationEscalatesWithoutPublishing();std::cout<<"report worker tests passed\n";}catch(const std::exception&e){std::cerr<<"report worker test failed: "<<e.what()<<'\n';return 1;}}
