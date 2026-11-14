#include "tigersheets/vba_ui.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include <map>
#include <regex>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <stdexcept>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QRegularExpression>
#include <QPointer>
#include <QTimer>
#include <QSignalBlocker>
#include <QFontDatabase>
#include <QLineEdit>
#include <QCloseEvent>
#include <algorithm>
namespace tigersheets {
namespace {
std::string vbaString(const std::string& value){std::string quoted="\"";for(const auto ch:value){if(static_cast<unsigned char>(ch)<32||ch==127)quoted+="\" & Chr("+std::to_string(static_cast<unsigned char>(ch))+") & \"";else if(ch=='\"')quoted+="\"\"";else quoted+=ch;}return quoted+'\"';}
std::string vbaColor(const Json& value){const auto color=value.get<std::string>();if(!std::regex_match(color,std::regex("#[0-9a-fA-F]{6}")))throw std::runtime_error("Recorder requires an RGB color");return "RGB("+std::to_string(std::stoul(color.substr(1,2),nullptr,16))+","+std::to_string(std::stoul(color.substr(3,2),nullptr,16))+","+std::to_string(std::stoul(color.substr(5,2),nullptr,16))+")";}
}
void MacroRecorder::start(const Json& snapshot){previous_=WorkbookSession(snapshot).snapshot();lines_.clear();active_=true;}
void MacroRecorder::observe(const Json& snapshot){
 if(!active_)return;
 const auto next=WorkbookSession(snapshot).snapshot();
 if(next==previous_)return;
 const auto fail=[this](const char* message){active_=false;throw std::runtime_error(message);};
 if(next.at("revision").get<std::uint64_t>()<=previous_.at("revision").get<std::uint64_t>())fail("Workbook was replaced while recording");
 if(next.at("sheets").size()!=previous_.at("sheets").size())fail("Worksheet creation and deletion are outside the cell recorder");
 auto additions=std::vector<std::string>{};
 for(std::size_t i=0;i<next.at("sheets").size();++i){
  const auto& sheet=next.at("sheets")[i];const auto& before=previous_.at("sheets")[i];if(sheet.at("id")!=before.at("id")||sheet.at("name")!=before.at("name"))fail("Worksheet renaming and reordering are outside the cell recorder");
  using Coordinate=std::pair<int,int>;std::map<Coordinate,Json> oldCells,newCells;
  for(const auto& cell:before.at("cells"))oldCells[{cell.at("row").get<int>(),cell.at("col").get<int>()}]=cell;
  for(const auto& cell:sheet.at("cells"))newCells[{cell.at("row").get<int>(),cell.at("col").get<int>()}]=cell;
  auto coordinates=oldCells;coordinates.insert(newCells.begin(),newCells.end());
  for(const auto& [coordinate,unused]:coordinates){(void)unused;const auto old=oldCells.contains(coordinate)?oldCells.at(coordinate):Json::object();const auto cell=newCells.contains(coordinate)?newCells.at(coordinate):Json::object();const auto prefix="Worksheets("+vbaString(sheet.at("name").get<std::string>())+").Range("+vbaString(columnName(coordinate.second)+std::to_string(coordinate.first))+").";
   if(old.value("input",std::string())!=cell.value("input",std::string()))additions.push_back(prefix+"Formula = "+vbaString(cell.value("input",std::string())));
   const auto oldStyle=old.value("style",Json::object());const auto style=cell.value("style",Json::object());
   for(const auto& key:{"bold","italic","underline","fontFamily","fontSize","color","fill","numberFormat","alignment","wrap"}){
    const auto beforeValue=oldStyle.contains(key)?oldStyle.at(key):Json();const auto afterValue=style.contains(key)?style.at(key):Json();if(beforeValue==afterValue)continue;
    std::string property,value;
    if(std::string(key)=="bold"||std::string(key)=="italic"){property=std::string(key)=="bold"?"Font.Bold":"Font.Italic";value=afterValue.is_null()||!afterValue.get<bool>()?"False":"True";}
    else if(std::string(key)=="underline"){property="Font.Underline";value=afterValue.is_null()||!afterValue.get<bool>()?"-4142":"2";}
    else if(std::string(key)=="wrap"){property="WrapText";value=afterValue.is_null()||!afterValue.get<bool>()?"False":"True";}
    else if(std::string(key)=="alignment"){property="HorizontalAlignment";const std::map<std::string,int> alignments{{"left",-4131},{"center",-4108},{"right",-4152},{"general",1},{"justify",-4130},{"fill",5},{"centerContinuous",7},{"distributed",-4117}};const auto alignment=afterValue.is_null()?std::string("general"):afterValue.get<std::string>();if(!alignments.contains(alignment))fail("Unsupported recorder alignment");value=std::to_string(alignments.at(alignment));}
    else if(std::string(key)=="fontFamily"){property="Font.Name";value=vbaString(afterValue.is_null()?"Calibri":afterValue.get<std::string>());}
    else if(std::string(key)=="fontSize"){property="Font.Size";value=afterValue.is_null()?"11":afterValue.dump();}
    else if(std::string(key)=="fill"||std::string(key)=="color"){const bool fill=std::string(key)=="fill";property=fill?"Interior.Color":"Font.Color";if(afterValue.is_null()){property+="Index";value=fill?"-4142":"-4105";}else value=vbaColor(afterValue);}
    else {property="NumberFormat";value=vbaString(afterValue.is_null()?"General":afterValue.get<std::string>());}
    additions.push_back(prefix+property+" = "+value);
   }
  }
 }
 if(lines_.size()+additions.size()>100000)fail("Recording exceeds 100000 assignments");
 std::size_t bytes=0;for(const auto& line:lines_)bytes+=line.size();for(const auto& line:additions)bytes+=line.size();if(bytes>900000)fail("Recording exceeds the macro source limit");
 lines_.insert(lines_.end(),additions.begin(),additions.end());previous_=next;
}
std::string MacroRecorder::source(const std::string& procedure) const{if(!std::regex_match(procedure,std::regex("[A-Za-z_][A-Za-z0-9_]*")))throw std::runtime_error("Use a VBA identifier for the recorded procedure");std::string result="Sub "+procedure+"()\n";for(const auto& line:lines_)result+="    "+line+"\n";return result+"End Sub\n";}
VbaWorker::VbaWorker(QObject* parent,QString executable):QObject(parent),process_(new QProcess(this)),executable_(std::move(executable)){
 process_->setProcessChannelMode(QProcess::SeparateChannels);
 connect(process_,&QProcess::readyReadStandardOutput,this,[this]{const auto chunk=process_->readAllStandardOutput();if(static_cast<std::size_t>(output_.size())+static_cast<std::size_t>(chunk.size())>MaxVbaMessageBytes){oversized_=true;process_->kill();}else output_+=chunk;});
 connect(process_,&QProcess::readyReadStandardError,this,[this]{process_->readAllStandardError();});
 connect(process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){finish(code);});
}
VbaWorker::~VbaWorker(){process_->disconnect(this);if(process_->state()!=QProcess::NotRunning){process_->kill();process_->waitForFinished(1000);}}
bool VbaWorker::start(const Json& snapshot,const std::string& source,const std::string& procedure,bool enabled){
 if(busy_){error_="A macro is already running";return false;}
 try{
  const auto request=Json{{"version",1},{"snapshot",snapshot},{"source",source},{"procedure",procedure},{"enabled",enabled}}.dump();if(request.size()>MaxVbaMessageBytes)throw std::runtime_error("Macro request exceeds 16 MiB");
  auto path=executable_;if(path.isEmpty()){const auto directory=QCoreApplication::applicationDirPath();const QStringList candidates{directory+"/tigersheets-vba-worker",QDir::cleanPath(directory+"/../libexec/tigersheets/tigersheets-vba-worker")};for(const auto& candidate:candidates)if(QFileInfo(candidate).isExecutable()){path=candidate;break;}}
  if(path.isEmpty()||!QFileInfo(path).isExecutable())throw std::runtime_error("The TigerSheets VBA worker executable is unavailable");
  QProcessEnvironment environment;environment.insert("LANG","C.UTF-8");environment.insert("PATH","/usr/bin:/bin");process_->setProcessEnvironment(environment);process_->setWorkingDirectory(QDir::tempPath());process_->setProgram(path);process_->setArguments({"--stdio"});output_.clear();cancelled_=oversized_=false;original_=snapshot;process_->start(QIODevice::ReadWrite);
  if(!process_->waitForStarted(3000))throw std::runtime_error(process_->errorString().toStdString());
  busy_=true;process_->write(QByteArray(request.data(),static_cast<qsizetype>(request.size()))+'\n');process_->closeWriteChannel();error_.clear();return true;
 }catch(const std::exception& ex){error_=QString::fromUtf8(ex.what());return false;}
}
void VbaWorker::stop(){if(!busy_)return;cancelled_=true;process_->kill();}
void VbaWorker::finish(int exitCode){
 if(!busy_)return;
 busy_=false;Json result{{"status","error"},{"workbook",original_},{"diagnostics",Json::array()}};
 if(cancelled_)result["status"]="cancelled";
 else try{output_+=process_->readAllStandardOutput();if(oversized_||static_cast<std::size_t>(output_.size())>MaxVbaMessageBytes)throw std::runtime_error("Macro response exceeds 16 MiB");if(exitCode!=0||process_->exitStatus()!=QProcess::NormalExit)throw std::runtime_error("Macro worker exited before completion (resource limit or process failure)");auto response=Json::parse(output_.constData(),output_.constData()+output_.size());if(response.value("version",0)!=1)throw std::runtime_error("Invalid macro worker response");if(response.value("status",std::string())=="completed"){WorkbookSession validate(response.at("workbook"));response["workbook"]=validate.snapshot();}else response["workbook"]=original_;result=std::move(response);}catch(const std::exception& ex){result["diagnostics"].push_back(ex.what());}
 const auto callback=completed;if(callback)callback(result);
}
namespace {
class RecorderDialog final:public QDialog {
public:
 RecorderDialog(QWidget* parent,WorkbookSession& session,WorksheetModel& model):QDialog(parent),session_(session),model_(&model){
  setObjectName("vba.recorder");setWindowTitle("TigerSheets — Record cell macro");setAttribute(Qt::WA_DeleteOnClose);resize(640,440);
  auto* layout=new QVBoxLayout(this);auto* scope=new QLabel("Records committed cell values, formulas, bold/italic/underline, font family/size/color, fill, number format, alignment and wrap. Worksheet structure, charts, selection and other application actions are outside this recorder. Stop to keep the generated source; closing an active recording discards it. Source can run locally after explicit macro enablement; native Excel VBA project export is unavailable.",this);scope->setWordWrap(true);layout->addWidget(scope);
  auto* row=new QHBoxLayout;row->addWidget(new QLabel("Procedure",this));procedure_=new QLineEdit("RecordedMacro1",this);procedure_->setObjectName("vba.record.procedure");row->addWidget(procedure_,1);start_=new QPushButton("Start recording",this);start_->setObjectName("vba.record.start");stop_=new QPushButton("Stop and keep source",this);stop_->setObjectName("vba.record.stop");stop_->setEnabled(false);row->addWidget(start_);row->addWidget(stop_);layout->addLayout(row);
  status_=new QLabel("Ready",this);status_->setWordWrap(true);layout->addWidget(status_);source_=new QPlainTextEdit(this);source_->setReadOnly(true);source_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));source_->setObjectName("vba.record.source");layout->addWidget(source_,1);auto* close=new QDialogButtonBox(QDialogButtonBox::Close,this);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,this,&QDialog::close);connect(start_,&QPushButton::clicked,this,[this]{begin();});connect(stop_,&QPushButton::clicked,this,[this]{finish();});suggestProcedure();
 }
 ~RecorderDialog() override{detach();}
private:
 WorkbookSession& session_;QPointer<WorksheetModel> model_;MacroRecorder recorder_;std::function<void()> previous_;QLineEdit* procedure_{};QPushButton* start_{};QPushButton* stop_{};QLabel* status_{};QPlainTextEdit* source_{};bool observing_=false;
 bool procedureUsed(const std::string& name) const{const auto book=session_.snapshot();if(!book.contains("vba")||!book["vba"].is_object())return false;const auto modules=book["vba"].value("modules",Json::array());if(!modules.is_array())return false;const auto pattern=std::regex("(?:^|[\\r\\n])\\s*(?:(?:Public|Private)\\s+)?(?:Sub|Function)\\s+"+name+"\\s*\\(",std::regex::icase);for(const auto& module:modules)if(module.is_object()&&module.contains("source")&&module["source"].is_string()&&std::regex_search(module["source"].get<std::string>(),pattern))return true;return false;}
 void suggestProcedure(){int index=1;std::string name;do{name="RecordedMacro"+std::to_string(index++);}while(procedureUsed(name));procedure_->setText(QString::fromStdString(name));}
 void detach(){if(!observing_)return;observing_=false;if(model_&&model_->property("tigerMacroRecorder").toULongLong()==reinterpret_cast<qulonglong>(this)){model_->onChanged=previous_;model_->setProperty("tigerMacroRecorder",QVariant());}previous_={};}
 void begin(){try{if(!model_)throw std::runtime_error("Worksheet is unavailable");if(session_.hasPreview())throw std::runtime_error("Accept or discard the Codex preview before recording");if(model_->property("tigerMacroRecorder").isValid())throw std::runtime_error("A recorder is already observing this worksheet");recorder_.start(session_.snapshot());recorder_.source(procedure_->text().toStdString());if(procedureUsed(procedure_->text().toStdString()))throw std::runtime_error("A procedure with this name already exists; use another name");previous_=model_->onChanged;observing_=true;model_->setProperty("tigerMacroRecorder",QVariant::fromValue(reinterpret_cast<qulonglong>(this)));QPointer<RecorderDialog> guard(this);const auto prior=previous_;model_->onChanged=[guard,prior]{const auto callback=prior;const auto alive=guard;if(callback)callback();if(alive)alive->observe();};procedure_->setEnabled(false);start_->setEnabled(false);stop_->setEnabled(true);source_->clear();status_->setText("Recording committed cell edits…");}catch(const std::exception& ex){recorder_.stop();status_->setText(QString::fromUtf8(ex.what()));}}
 void observe(){try{if(!model_)return;recorder_.observe(session_.snapshot());status_->setText(QString::number(recorder_.changeCount())+" VBA assignments recorded");}catch(const std::exception& ex){detach();recorder_.stop();stop_->setEnabled(false);start_->setEnabled(true);procedure_->setEnabled(true);status_->setText(QString::fromUtf8(ex.what())+". Recording discarded; start again after that change.");}}
 void finish(){detach();recorder_.stop();try{if(!model_)throw std::runtime_error("Worksheet is unavailable");if(recorder_.changeCount()==0){status_->setText("No cell edits recorded.");stop_->setEnabled(false);start_->setEnabled(true);procedure_->setEnabled(true);return;}const auto source=recorder_.source(procedure_->text().toStdString());source_->setPlainText(QString::fromStdString(source));if(session_.hasPreview())throw std::runtime_error("Accept or discard the Codex preview, then stop again to keep this source");const auto book=session_.snapshot();auto metadata=book.value("vba",Json::object());if(!metadata.is_object())metadata=Json::object();auto modules=metadata.value("modules",Json::array());if(!modules.is_array())throw std::runtime_error("The module registry is invalid");int index=1;std::string name;for(;;){name="RecordedModule"+std::to_string(index++);const auto found=std::any_of(modules.begin(),modules.end(),[&](const auto& module){return QString::fromStdString(module.value("name",std::string())).compare(QString::fromStdString(name),Qt::CaseInsensitive)==0;});if(!found)break;}modules.push_back({{"name",name},{"source",source}});metadata["modules"]=modules;if(!metadata.contains("enabled"))metadata["enabled"]=false;std::string error;if(!session_.apply(book.at("revision").get<std::uint64_t>(),Json::array({{{"type","set_metadata"},{"key","vba"},{"value",metadata}}}),&error))throw std::runtime_error(error);stop_->setEnabled(false);start_->setEnabled(true);procedure_->setEnabled(true);status_->setText("Recorded source kept in "+QString::fromStdString(name)+". Open the VBA editor to review and run it.");suggestProcedure();QPointer<RecorderDialog> alive(this);const auto callback=model_->onChanged;if(callback)callback();if(!alive)return;}catch(const std::exception& ex){status_->setText(QString::fromUtf8(ex.what()));}}
};
class VbaDialog final:public QDialog {
public:
 VbaDialog(QWidget* parent,WorkbookSession& session,WorksheetModel& model):QDialog(parent),session_(session),model_(&model),worker_(new VbaWorker(this)){
  setObjectName("vba.editor");setWindowTitle("TigerSheets — VBA editor");setAttribute(Qt::WA_DeleteOnClose);resize(880,650);setMinimumSize(640,440);
  const auto book=session_.snapshot();metadata_=book.value("vba",Json::object());if(!metadata_.is_object())metadata_=Json::object();modules_=metadata_.value("modules",Json::array());if(modules_.empty())modules_.push_back({{"name","Module1"},{"source","Sub Macro1()\n    Range(\"A1\").Value = 42\nEnd Sub\n"}});
  auto* layout=new QVBoxLayout(this);auto* limitation=new QLabel("Runs the supported native VBA subset in an isolated local process. Edited module source is retained in TigerSheets recovery. Saving new or edited Excel VBA projects is unavailable; original imported projects remain preserved.",this);if(metadata_.value("projectPresent",false))limitation->setText(limitation->text()+" Imported VBA source is unavailable; displayed drafts are new TigerSheets modules.");limitation->setObjectName("vba.limitation");limitation->setWordWrap(true);layout->addWidget(limitation);
  auto* bar=new QHBoxLayout;bar->addWidget(new QLabel("Module",this));module_=new QComboBox(this);module_->setObjectName("vba.modules");for(const auto& module:modules_)module_->addItem(QString::fromStdString(module.at("name").get<std::string>()));bar->addWidget(module_,1);auto* add=new QPushButton("Add module",this);bar->addWidget(add);enabled_=new QCheckBox("Enable macros for this workbook",this);enabled_->setObjectName("vba.enabled");enabled_->setChecked(metadata_.value("enabled",false));bar->addWidget(enabled_);layout->addLayout(bar);
  source_=new QPlainTextEdit(this);source_->setObjectName("vba.source");source_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));source_->setPlainText(QString::fromStdString(modules_[0].at("source").get<std::string>()));layout->addWidget(source_,1);
  auto* runbar=new QHBoxLayout;runbar->addWidget(new QLabel("Procedure",this));procedure_=new QComboBox(this);procedure_->setObjectName("vba.procedures");runbar->addWidget(procedure_,1);run_=new QPushButton("Run",this);run_->setObjectName("vba.run");stop_=new QPushButton("Stop",this);stop_->setObjectName("vba.stop");stop_->setEnabled(false);runbar->addWidget(run_);runbar->addWidget(stop_);layout->addLayout(runbar);
  status_=new QLabel("Ready",this);status_->setObjectName("vba.status");status_->setWordWrap(true);layout->addWidget(status_);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::close);
  debounce_.setSingleShot(true);debounce_.setInterval(250);connect(&debounce_,&QTimer::timeout,this,[this]{store();});
  connect(source_,&QPlainTextEdit::textChanged,this,[this]{dirty_=true;modules_[static_cast<std::size_t>(module_->currentIndex())]["source"]=source_->toPlainText().toStdString();procedures();debounce_.start();});
  connect(module_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){if(index<0)return;const QSignalBlocker block(source_);source_->setPlainText(QString::fromStdString(modules_[static_cast<std::size_t>(index)].at("source").get<std::string>()));procedures();});
  connect(enabled_,&QCheckBox::toggled,this,[this]{dirty_=true;QPointer<VbaDialog> alive(this);store();if(alive)procedures();});connect(add,&QPushButton::clicked,this,[this]{bool ok=false;const auto name=QInputDialog::getText(this,"Add VBA module","Module name",QLineEdit::Normal,"Module"+QString::number(module_->count()+1),&ok);if(!ok)return;static const QRegularExpression identifier("^[A-Za-z_][A-Za-z0-9_]*$");if(!identifier.match(name).hasMatch()){status_->setText("Use a VBA identifier for the module name.");return;}for(int i=0;i<module_->count();++i)if(module_->itemText(i).compare(name,Qt::CaseInsensitive)==0){status_->setText("A module with that name already exists.");return;}dirty_=true;modules_.push_back({{"name",name.toStdString()},{"source",""}});module_->addItem(name);module_->setCurrentIndex(module_->count()-1);store();});
  connect(run_,&QPushButton::clicked,this,[this]{run();});connect(stop_,&QPushButton::clicked,this,[this]{status_->setText("Stopping macro…");worker_->stop();});procedures();
 }
 ~VbaDialog() override{debounce_.stop();worker_->completed={};delete worker_;worker_=nullptr;}
 bool flush(QString* error){debounce_.stop();const bool stored=store();if(!stored&&error)*error=status_->text();return stored;}
protected:
 void closeEvent(QCloseEvent* event) override{debounce_.stop();QPointer<VbaDialog> alive(this);if(model_&&!store()){event->ignore();return;}if(!alive)return;worker_->completed={};worker_->stop();QDialog::closeEvent(event);}
private:
 WorkbookSession& session_;QPointer<WorksheetModel> model_;VbaWorker* worker_;Json metadata_,modules_;QComboBox* module_{};QComboBox* procedure_{};QCheckBox* enabled_{};QPlainTextEdit* source_{};QPushButton* run_{};QPushButton* stop_{};QLabel* status_{};QTimer debounce_;bool dirty_=false;
 void procedures(){const auto selected=procedure_->currentText();procedure_->clear();static const QRegularExpression sub("^\\s*(?:(?:Public|Private)\\s+)?Sub\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*\\(\\s*\\)",QRegularExpression::CaseInsensitiveOption|QRegularExpression::MultilineOption);for(const auto& module:modules_){auto matches=sub.globalMatch(QString::fromStdString(module.at("source").get<std::string>()));while(matches.hasNext()){const auto name=matches.next().captured(1);if(procedure_->findText(name,Qt::MatchFixedString)<0)procedure_->addItem(name);}}const auto index=procedure_->findText(selected);if(index>=0)procedure_->setCurrentIndex(index);run_->setEnabled(enabled_->isChecked()&&procedure_->count()>0&&!worker_->running());}
 bool store(){if(!dirty_)return true;if(!model_)return false;if(session_.hasPreview()){status_->setText("Accept or discard the current Codex preview before changing macro source.");return false;}metadata_["enabled"]=enabled_->isChecked();metadata_["modules"]=modules_;const auto book=session_.snapshot();if(book.contains("vba")&&book["vba"]==metadata_){dirty_=false;return true;}std::string error;if(!session_.apply(book.at("revision").get<std::uint64_t>(),Json::array({{{"type","set_metadata"},{"key","vba"},{"value",metadata_}}}),&error)){status_->setText(QString::fromStdString(error));return false;}dirty_=false;if(model_->onChanged)model_->onChanged();return true;}
 void run(){debounce_.stop();QPointer<VbaDialog> alive(this);if(!enabled_->isChecked())return;dirty_=true;if(!store()||!alive||!model_)return;auto snapshot=session_.snapshot();const auto base=snapshot;const auto revision=snapshot.at("revision").get<std::uint64_t>();snapshot["activeSheet"]=model_->sheetId().toStdString();std::string source;for(const auto& module:modules_)source+=module.at("source").get<std::string>()+"\n";
  worker_->completed=[this,revision,base](const Json& result){if(!model_||!isVisible())return;stop_->setEnabled(false);procedures();const auto state=result.value("status",std::string("error"));if(state=="completed"){
    const auto current=session_.snapshot();if(current.at("revision").get<std::uint64_t>()!=revision||current!=base||session_.hasPreview()){status_->setText("The workbook changed while the macro ran. Its candidate was discarded; run it again.");return;}Json operations=Json::array();for(const auto& sheet:result.at("workbook").at("sheets")){const auto id=sheet.at("id");auto original=std::find_if(current.at("sheets").begin(),current.at("sheets").end(),[&](const auto& value){return value.at("id")==id;});if(original==current.at("sheets").end()){status_->setText("Macro returned an unknown worksheet; candidate discarded.");return;}if(original->at("cells")!=sheet.at("cells"))operations.push_back({{"type","replace_sheet_cells"},{"sheet",id},{"cells",sheet.at("cells")}});}QPointer<VbaDialog> alive(this);const auto accepted=model_->apply(operations);if(!alive)return;if(!accepted){status_->setText("The macro candidate could not be accepted.");return;}status_->setText("Macro completed. Workbook changes are one Undo step.");
   }else if(state=="cancelled")status_->setText("Macro stopped. Its worksheet changes were discarded.");else{QStringList messages;for(const auto& item:result.value("diagnostics",Json::array()))messages.push_back(QString::fromStdString(item.get<std::string>()));status_->setText(messages.isEmpty()?"Macro failed. Its worksheet changes were discarded.":messages.join("\n"));}
  };
  if(!worker_->start(snapshot,source,procedure_->currentText().toStdString(),true)){status_->setText(worker_->error());return;}status_->setText("Running macro in an isolated local process…");run_->setEnabled(false);stop_->setEnabled(true);
 }
};
}
bool vbaCommand(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QString command){if(command!="vba.editor"&&command!="vba.record")return false;const auto name=command=="vba.record"?QString("vba.recorder"):command;if(parent)if(auto* existing=parent->findChild<QDialog*>(name)){existing->show();existing->raise();existing->activateWindow();return true;}QDialog* dialog=command=="vba.record"?static_cast<QDialog*>(new RecorderDialog(parent,session,model)):static_cast<QDialog*>(new VbaDialog(parent,session,model));dialog->show();return true;}
bool flushVbaDrafts(QWidget* parent,QString* error){if(error)error->clear();if(!parent)return true;std::vector<QPointer<VbaDialog>> editors;for(auto* dialog:parent->findChildren<QDialog*>())if(auto* editor=dynamic_cast<VbaDialog*>(dialog))editors.emplace_back(editor);for(const auto& editor:editors)if(editor&&!editor->flush(error))return false;return true;}

}
