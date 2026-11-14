#include "tigersheets/report_worker.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <streambuf>
#include <stdexcept>
namespace tigersheets {
namespace {
constexpr std::uint64_t byteLimit=1024ull*1024*1024;
struct Job {
 std::shared_ptr<const Json> request;
 std::atomic<bool> cancelled{false};
 std::string directory,input,output;
 QString failure;
 Json result,revision,generation,workbookId;bool tool=false,financial=false;
 ~Job(){std::error_code error;std::filesystem::remove_all(directory,error);}
};
std::shared_ptr<Job> newJob(){return {new Job,[](Job* job){auto* thread=QThread::create([job]{delete job;});QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();}};}
class OutputBuffer final:public std::streambuf {
 std::ofstream file_;std::atomic<bool>& cancelled_;std::uint64_t count_{};
 std::streamsize xsputn(const char* data,std::streamsize size) override {
  std::streamsize written=0;
  while(written<size){if(cancelled_)throw std::runtime_error("Report job cancelled");const auto chunk=std::min<std::streamsize>(65536,size-written);if(count_+chunk>byteLimit)throw std::runtime_error("Report request exceeds the 1 GiB protocol limit");file_.write(data+written,chunk);if(!file_)throw std::runtime_error("Cannot write report request");written+=chunk;count_+=chunk;}
  return written;
 }
 int_type overflow(int_type value) override {if(traits_type::eq_int_type(value,traits_type::eof()))return traits_type::not_eof(value);const char c=traits_type::to_char_type(value);xsputn(&c,1);return value;}
 int sync() override {file_.flush();return file_?0:-1;}
public:
 OutputBuffer(const std::string& path,std::atomic<bool>& cancelled):file_(path,std::ios::binary|std::ios::trunc),cancelled_(cancelled){if(!file_)throw std::runtime_error("Cannot open report request");}
};
class InputBuffer final:public std::streambuf {
 std::ifstream file_;std::atomic<bool>& cancelled_;char data_[65536];
 int_type underflow() override {
  if(cancelled_)throw std::runtime_error("Report job cancelled");
  file_.read(data_,sizeof(data_));const auto count=file_.gcount();if(count==0)return traits_type::eof();setg(data_,data_,data_+count);return traits_type::to_int_type(*gptr());
 }
public:
 InputBuffer(const std::string& path,std::atomic<bool>& cancelled):file_(path,std::ios::binary),cancelled_(cancelled){if(!file_)throw std::runtime_error("Report worker returned no result file");}
};
QString programPath(const QString& requested){
 if(!requested.isEmpty())return requested.contains('/')?QFileInfo(requested).absoluteFilePath():QStandardPaths::findExecutable(requested);
 const auto directory=QCoreApplication::applicationDirPath();
 for(const auto& path:{directory+"/tigersheets-report-worker",directory+"/../libexec/tigersheets/tigersheets-report-worker"})if(QFileInfo(path).isExecutable())return QFileInfo(path).absoluteFilePath();
 return {};
}
}
struct ReportWorker::State {
 ReportWorker* owner;QProcess process;std::shared_ptr<Job> job;QPointer<QThread> background;QByteArray progressBytes,stderrBytes;QString lastError;
 explicit State(ReportWorker* value):owner(value),process(value){
  QObject::connect(&process,&QProcess::readyReadStandardOutput,owner,[this]{readProgress();});
  QObject::connect(&process,&QProcess::readyReadStandardError,owner,[this]{stderrBytes+=process.readAllStandardError();if(stderrBytes.size()>65536)stderrBytes=stderrBytes.right(65536);});
  QObject::connect(&process,&QProcess::errorOccurred,owner,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart&&job)finishError("Cannot start the report worker: "+process.errorString());});
  QObject::connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),owner,[this](int code,QProcess::ExitStatus status){
   auto active=job;if(!active)return;readProgress();if(job!=active)return;
   if(active->cancelled){finishCancelled();return;}
   if(status!=QProcess::NormalExit||code!=0){finishError(QString("Report worker failed (exit %1): %2").arg(code).arg(QString::fromUtf8(stderrBytes).trimmed()));return;}
   parse(active);
  });
 }
 void progress(QString text){if(owner->onProgress)owner->onProgress(std::move(text));}
 void finishError(QString message){auto old=std::move(job);if(old)old->cancelled=true;lastError=std::move(message);if(process.state()!=QProcess::NotRunning)process.kill();if(owner->onError)owner->onError(lastError);}
 void finishCancelled(){job.reset();lastError.clear();progress("Cancelled");}
 void readProgress(){
  progressBytes+=process.readAllStandardOutput();if(progressBytes.size()>65536){finishError("Report worker progress exceeded its protocol limit");return;}
  while(progressBytes.contains('\n')){
   const auto end=progressBytes.indexOf('\n');auto line=progressBytes.left(end);progressBytes.remove(0,end+1);if(line.trimmed().isEmpty())continue;
   try{const auto message=Json::parse(line.toStdString());if(message.value("version",0)!=1||message.value("type",std::string())!="progress"||!message.at("stage").is_string())throw std::runtime_error("Invalid progress frame");if(job&&!job->cancelled)progress(QString::fromStdString(message.at("stage")));}
   catch(const std::exception&){finishError("Report worker returned invalid progress");return;}
  }
 }
 void parse(const std::shared_ptr<Job>& active){
  progress("Reading report result");auto* thread=QThread::create([active]{
   try{if(std::filesystem::file_size(active->output)>byteLimit)throw std::runtime_error("Report result exceeds the 1 GiB protocol limit");InputBuffer buffer(active->output,active->cancelled);std::istream input(&buffer);input.exceptions(std::ios::badbit);active->result=Json::parse(input);}
   catch(const std::exception& error){active->failure=QString::fromUtf8(error.what());}
  });background=thread;
  QObject::connect(thread,&QThread::finished,owner,[this,active]{
   background=nullptr;if(job!=active)return;if(active->cancelled){finishCancelled();return;}if(!active->failure.isEmpty()){finishError(active->failure);return;}
   try{auto& result=active->result;
    if(!result.is_object()||result.value("version",0)!=1||result.value("revision",Json())!=active->revision||result.value("contextGeneration",Json())!=active->generation)throw std::runtime_error("Report worker returned a foreign revision or calculation generation");
    if(result.value("status",std::string())!="ok")throw std::runtime_error("Report worker did not produce a qualified result: "+result.value("diagnostics",Json::array()).dump());
    if(!active->workbookId.is_null()&&result.value("workbookId",Json())!=active->workbookId)throw std::runtime_error("Report worker returned a foreign workbook identity");if(!result.at("diagnostics").is_array())throw std::runtime_error("Report worker diagnostics are invalid");if(active->tool){if(!result.at("toolReply").is_object()||!result.at("operations").is_array()||result.at("operations").size()>10000)throw std::runtime_error("Report worker returned an invalid tool result shape");}else if(active->financial){if(result.value("kind",std::string())!="financial_model"||!result.at("analysis").is_object()||!result.at("operations").is_array()||result.at("operations").size()>10000)throw std::runtime_error("Report worker returned an invalid financial result shape");if(!result.at("analysis").value("success",false)&&!result.at("operations").empty())throw std::runtime_error("Unqualified financial result cannot contain mutating operations");}else if(!result.at("definition").is_object()||!result.at("cells").is_array())throw std::runtime_error("Report worker returned an invalid result shape");
    job.reset();lastError.clear();if(owner->onResult)owner->onResult(std::move(result));
   }catch(const std::exception& error){finishError(QString::fromUtf8(error.what()));}
  });QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
 }
};
ReportWorker::ReportWorker(QObject* parent):QObject(parent),state_(std::make_unique<State>(this)){}
ReportWorker::~ReportWorker(){
 auto& s=*state_;if(s.job)s.job->cancelled=true;s.process.disconnect();s.process.kill();s.process.waitForFinished(500);
 if(s.background){QObject::disconnect(s.background,nullptr,this,nullptr);s.background->wait(500);}
}
bool ReportWorker::start(std::shared_ptr<const Json> request,const QString& executable){
 auto& s=*state_;if(s.job){s.lastError="A report refresh is already running";return false;}
 try{
  if(!request||!request->is_object()||request->value("version",0)!=1||!request->at("workbook").is_object()||!request->at("workbook").at("revision").is_number_integer()||!request->at("contextGeneration").is_number_integer()||!request->at("retainedLinks").is_array())throw std::runtime_error("Invalid immutable report request");
  const bool tool=request->value("kind",std::string())=="workbook_tool";if(tool){if(!request->at("tool").is_string()||!request->at("args").is_object()||!request->at("previousProposal").is_array()||request->at("previousProposal").size()>10000)throw std::runtime_error("Invalid isolated workbook tool request");}else if(!request->at("definition").is_object())throw std::runtime_error("Invalid native report definition");
  const auto program=programPath(executable);if(program.isEmpty()||!QFileInfo(program).isExecutable())throw std::runtime_error("The native report worker is not installed");
  QTemporaryDir folder(QDir::tempPath()+"/tigersheets-report-XXXXXX");if(!folder.isValid())throw std::runtime_error("Cannot create a private report job directory");
  auto active=newJob();active->revision=request->at("workbook").at("revision");active->generation=request->at("contextGeneration");active->workbookId=request->value("workbookId",Json());active->tool=tool;active->financial=request->value("kind",std::string())=="financial_model";active->request=std::move(request);active->directory=folder.path().toStdString();active->input=active->directory+"/request.json";active->output=active->directory+"/result.json";
  QFile input(QString::fromStdString(active->input));if(!input.open(QIODevice::WriteOnly)||!input.setPermissions(QFile::ReadOwner|QFile::WriteOwner))throw std::runtime_error("Cannot create a private report request");input.close();folder.setAutoRemove(false);
  s.job=active;s.lastError.clear();s.progressBytes.clear();s.stderrBytes.clear();s.progress("Preparing report inputs");
  auto* thread=QThread::create([active]{try{OutputBuffer buffer(active->input,active->cancelled);std::ostream output(&buffer);output.exceptions(std::ios::badbit|std::ios::failbit);output<<*active->request;output.flush();active->request.reset();}catch(const std::exception& error){active->failure=QString::fromUtf8(error.what());}});s.background=thread;
  QObject::connect(thread,&QThread::finished,this,[this,active,program]{auto& state=*state_;state.background=nullptr;if(state.job!=active)return;if(active->cancelled){state.finishCancelled();return;}if(!active->failure.isEmpty()){state.finishError(active->failure);return;}state.progress("Starting report calculation");state.process.setProgram(program);state.process.setArguments({"--request",QString::fromStdString(active->input),"--response",QString::fromStdString(active->output)});state.process.start();});
  QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();return true;
 }catch(const std::exception& error){s.lastError=QString::fromUtf8(error.what());if(onError)onError(s.lastError);return false;}
}
void ReportWorker::stop(){auto& s=*state_;auto active=s.job;if(!active)return;active->cancelled=true;s.progress("Cancelling report refresh");if(s.process.state()!=QProcess::NotRunning){s.process.terminate();QTimer::singleShot(150,this,[this,active]{if(state_->job==active&&state_->process.state()!=QProcess::NotRunning)state_->process.kill();});}}
bool ReportWorker::running()const{return bool(state_->job);}
QString ReportWorker::error()const{return state_->lastError;}
}
