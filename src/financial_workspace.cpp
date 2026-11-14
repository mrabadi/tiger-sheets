#include "tigersheets/financial_workspace.hpp"
#include "tigersheets/report_worker.hpp"
#include "tigersheets/snapshot_capture.hpp"
#include <QPointer>
#include <QThread>
#include <QStringList>
#include <atomic>
#include <cmath>
namespace tigersheets {
QString financialAnalysisSummary(const Json& analysis){
 const auto bounded=[](QString text,int maximum){if(text.size()>maximum){text.truncate(maximum-1);if(!text.isEmpty()&&text.back().isHighSurrogate())text.chop(1);text+=QChar(0x2026);}return text;};
 const auto text=[&](const Json& value,int maximum=360){return value.is_string()?bounded(QString::fromStdString(value.get<std::string>()),maximum):QString();};
 QStringList parts;auto status=analysis.is_object()&&analysis.contains("status")?text(analysis.at("status"),80):QString();parts<<(!status.isEmpty()?status:QStringLiteral("unknown"));
 const auto number=[&](const char* key,const char* label){if(analysis.is_object()&&analysis.contains(key)&&analysis.at(key).is_number()){const auto value=analysis.at(key).get<double>();if(std::isfinite(value))parts<<QString::fromLatin1(label)+QStringLiteral(" ")+QString::number(value,'g',12);}};
 number("value","input");number("result","result");number("objective","objective");number("residual","residual");number("evaluations","evaluations");
 if(analysis.is_object()&&!analysis.contains("value")&&analysis.contains("changingCells")&&analysis.at("changingCells").is_array()){
  std::size_t shown=0;for(const auto& cell:analysis.at("changingCells")){if(shown++>=3)break;if(cell.contains("value")&&cell.at("value").is_number()&&std::isfinite(cell.at("value").get<double>()))parts<<QStringLiteral("input ")+QString::number(cell.at("value").get<double>(),'g',12);else if(cell.contains("input")&&cell.at("input").is_string())parts<<QStringLiteral("input ")+text(cell.at("input"),48);}
 }
 if(analysis.is_object()&&analysis.contains("provenance")&&analysis.at("provenance").is_object()){
  const auto& provenance=analysis.at("provenance");QStringList names;if(provenance.contains("links")&&provenance.at("links").is_array())for(const auto& link:provenance.at("links"))if(link.is_object()&&(link.value("state",Json())=="cached"||link.value("state",Json())=="unavailable"||link.value("state",Json())=="missing")){auto name=link.contains("name")?text(link.at("name"),64):QString();if(name.isEmpty())name=QStringLiteral("linked source");if(names.size()<4)names<<name;}
  if(!names.isEmpty())parts<<QStringLiteral("cached linked source: ")+names.join(QStringLiteral(", "));
 }
 QString diagnostic;if(analysis.is_object()&&analysis.contains("message"))diagnostic=text(analysis.at("message"));
 if(analysis.is_object()&&analysis.contains("diagnostics")&&analysis.at("diagnostics").is_array())for(const auto& item:analysis.at("diagnostics")){const auto message=item.is_string()?text(item):item.is_object()&&item.contains("message")?text(item.at("message")):QString();if(!message.isEmpty())diagnostic=message;}
 if(!diagnostic.isEmpty())parts<<diagnostic;return bounded(parts.join(QStringLiteral(" · ")),1024);
}
namespace {
struct Job {
 std::shared_ptr<Json> request;
 std::unique_ptr<WorkbookSession::PreparedTransaction> prepared;
 Json response;
 std::atomic<bool> cancelled{false};
 std::uint64_t serial=0,revision=0,generation=0;
 std::string workbook,error;
};
std::shared_ptr<Job> makeJob(){return {new Job,[](Job* job){auto* thread=QThread::create([job]{delete job;});QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();}};}
}
struct FinancialWorkspace::State {
 WorkbookSession& session;SnapshotCapture* capture;ReportWorker* worker;QPointer<QThread> publication;std::shared_ptr<Job> job;std::uint64_t serial=0;
 State(WorkbookSession& value,QObject* owner):session(value),capture(new SnapshotCapture(owner,1024)),worker(new ReportWorker(owner)){}
};
FinancialWorkspace::FinancialWorkspace(WorkbookSession& session,QObject* parent):QObject(parent),state_(std::make_unique<State>(session,this)){}
FinancialWorkspace::~FinancialWorkspace(){cancel();delete state_->capture;delete state_->worker;if(state_->publication)state_->publication->wait();}
bool FinancialWorkspace::running()const{return bool(state_->job)||bool(state_->publication)||state_->worker->running();}
void FinancialWorkspace::cancel(){auto& s=*state_;++s.serial;if(s.job)s.job->cancelled=true;s.job.reset();s.capture->stop();s.worker->stop();if(s.publication)s.publication->requestInterruption();}
bool FinancialWorkspace::start(const Json& definition,const std::string& action,bool allowCachedLinks){
 auto& s=*state_;if(running()||s.session.hasPreview()||(mayStart&&!mayStart())){if(onError)onError("Accept or discard the current preview and stop active workbook jobs before starting another model");return false;}
 auto job=makeJob();job->serial=++s.serial;job->revision=s.session.snapshotView().at("revision");const auto identity=epoch?epoch():std::pair<std::string,std::uint64_t>{"workbook",0};job->workbook=identity.first;job->generation=identity.second;job->request=std::make_shared<Json>(Json{{"version",1},{"kind","financial_model"},{"action",action},{"definition",definition},{"workbookId",job->workbook},{"contextGeneration",job->generation},{"allowCachedLinks",allowCachedLinks}});s.job=job;
 const std::weak_ptr<Job> weak=job;
 const auto current=[this,weak]{const auto job=weak.lock();if(!job)return false;const auto identity=epoch?epoch():std::pair<std::string,std::uint64_t>{"workbook",0};return !job->cancelled&&state_->job==job&&state_->serial==job->serial&&state_->session.snapshotView().at("revision")==job->revision&&identity.first==job->workbook&&identity.second==job->generation;};
 const auto fail=[this,weak](QString message){const auto job=weak.lock();if(!job||state_->job!=job)return;state_->job.reset();if(onError)onError(message);};
 s.capture->failed=fail;s.worker->onError=fail;s.worker->onProgress=[this](QString value){if(onProgress)onProgress(value);};
 s.worker->onResult=[this,weak,current,fail](Json response){auto job=weak.lock();if(!job)return;if(!current()){fail("Workbook or calculation context changed; the candidate was discarded");return;}job->response=std::move(response);
  if(job->response.at("operations").empty()){const auto analysis=job->response.at("analysis");state_->job.reset();if(onAnalysis)onAnalysis(analysis);return;}
  auto* thread=QThread::create([job]{try{if(job->cancelled)return;job->prepared=WorkbookSession::prepareTransaction(std::move(job->request->at("workbook")),job->response.at("operations"),&job->error);}catch(const std::exception& error){job->error=error.what();}job->request.reset();});state_->publication=thread;
  connect(thread,&QThread::finished,this,[this,job,current,fail,thread]{auto& s=*state_;if(s.publication==thread)s.publication=nullptr;if(!current()){fail("Workbook or calculation context changed; the candidate was discarded");return;}if(!job->prepared){fail(QString::fromStdString(job->error));return;}std::string error;if(!s.session.previewPrepared(job->revision,job->prepared,&error)){fail(QString::fromStdString(error));return;}const auto analysis=job->response.at("analysis");s.job.reset();if(onAnalysis)onAnalysis(analysis);if(onPreview)onPreview();});connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
 };
 s.capture->completed=[this,weak,current,fail](std::shared_ptr<Json> workbook){auto job=weak.lock();if(!job)return;if(!current()){fail("Workbook changed while preparing model inputs");return;}(*job->request)["workbook"]=std::move(*workbook);
  state_->capture->completed=[this,weak,current,fail](std::shared_ptr<Json> links){auto job=weak.lock();if(!job)return;if(!current()){fail("Linked inputs changed while preparing model inputs");return;}(*job->request)["retainedLinks"]=std::move(*links);try{if(prepareRequest)prepareRequest(*job->request);if(!state_->worker->start(job->request,executable))fail(state_->worker->error());}catch(const std::exception& error){fail(QString::fromUtf8(error.what()));}};
  static const Json noLinks=Json::array();if(!state_->capture->start(retainedLinks?retainedLinks():noLinks,current))fail("Cannot capture linked model inputs");
 };
 if(onStarting)onStarting();if(!s.capture->start(s.session.snapshotView(),current)){fail("Cannot capture model inputs");return false;}return true;
}
}
