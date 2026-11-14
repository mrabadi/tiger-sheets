#include "tigersheets/recovery.hpp"
#include <QCryptographicHash>
#include <sqlite3.h>
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <limits>
#include <QMetaObject>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
namespace tigersheets {
namespace {
constexpr int schemaVersion=3;
using Statement=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
void check(int result,sqlite3* db){if(result!=SQLITE_OK&&result!=SQLITE_DONE&&result!=SQLITE_ROW)throw std::runtime_error(std::string("Recovery database error: ")+sqlite3_errmsg(db));}
void execute(sqlite3* db,const char* sql){check(sqlite3_exec(db,sql,nullptr,nullptr,nullptr),db);}
Statement prepare(sqlite3* db,const char* sql){sqlite3_stmt* statement=nullptr;check(sqlite3_prepare_v2(db,sql,-1,&statement,nullptr),db);return {statement,sqlite3_finalize};}
std::string bytes(sqlite3_stmt* statement,int column){const auto* data=static_cast<const char*>(sqlite3_column_blob(statement,column));return data?std::string(data,static_cast<std::size_t>(sqlite3_column_bytes(statement,column))):std::string();}
void bindText(sqlite3* db,sqlite3_stmt* statement,int column,const std::string& value){check(sqlite3_bind_text64(statement,column,value.data(),value.size(),SQLITE_TRANSIENT,SQLITE_UTF8),db);}
void bindBlob(sqlite3* db,sqlite3_stmt* statement,int column,const std::string& value){check(sqlite3_bind_blob64(statement,column,value.data(),value.size(),SQLITE_TRANSIENT),db);}
std::string checksum(const std::string& snapshot,const std::string& source,const std::string& path){
 QCryptographicHash hash(QCryptographicHash::Sha256);
 for(const auto* field:{&snapshot,&source,&path}){
  const auto size=static_cast<std::uint64_t>(field->size());std::array<char,8> prefix{};
  for(std::size_t index=0;index<prefix.size();++index)prefix[index]=static_cast<char>((size>>(56U-8U*static_cast<unsigned>(index)))&0xffU);
  hash.addData(prefix.data(),static_cast<qsizetype>(prefix.size()));hash.addData(field->data(),static_cast<qsizetype>(field->size()));
 }
 const auto digest=hash.result();return std::string(digest.constData(),static_cast<std::size_t>(digest.size()));
}
[[noreturn]] void corrupt(const std::string& id,const std::string& reason){throw std::runtime_error("Recovery checkpoint is corrupt (id: "+id+"): "+reason);}
Json parseSnapshot(const std::string& id,const std::string& snapshot){try{return Json::parse(snapshot);}catch(const Json::exception&){corrupt(id,"invalid snapshot JSON");}}
int version(sqlite3* db){auto statement=prepare(db,"PRAGMA user_version");check(sqlite3_step(statement.get()),db);return sqlite3_column_int(statement.get(),0);}
std::vector<std::string> columns(sqlite3* db){auto statement=prepare(db,"PRAGMA table_info(checkpoints)");std::vector<std::string> fields;for(;;){const auto result=sqlite3_step(statement.get());if(result==SQLITE_DONE)break;check(result,db);fields.push_back(bytes(statement.get(),1));}return fields;}
void migrate(sqlite3* db){
 execute(db,"BEGIN IMMEDIATE");
 try{
  execute(db,"ALTER TABLE checkpoints ADD COLUMN digest BLOB");
  auto records=prepare(db,"SELECT rowid,id,snapshot,source,path FROM checkpoints");
  for(;;){const auto result=sqlite3_step(records.get());if(result==SQLITE_DONE)break;check(result,db);const auto id=bytes(records.get(),1),snapshot=bytes(records.get(),2),source=bytes(records.get(),3),path=bytes(records.get(),4);static_cast<void>(parseSnapshot(id,snapshot));const auto digest=checksum(snapshot,source,path);auto update=prepare(db,"UPDATE checkpoints SET digest=?1 WHERE rowid=?2");bindBlob(db,update.get(),1,digest);check(sqlite3_bind_int64(update.get(),2,sqlite3_column_int64(records.get(),0)),db);check(sqlite3_step(update.get()),db);}
  execute(db,"PRAGMA user_version=2;COMMIT");
 }catch(...){sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}

std::uint64_t revision(const Json& value){
 if(!value.is_number_integer()||value.is_number_integer()&&!value.is_number_unsigned()&&value.get<std::int64_t>()<0)throw std::runtime_error("Recovery revision must be a nonnegative integer");
 const auto result=value.get<std::uint64_t>();if(result>static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max()))throw std::runtime_error("Recovery revision exceeds SQLite journal limits");return result;
}
std::string chainStart(const std::string& id,const std::string& digest,std::uint64_t base){return checksum("TigerSheets recovery journal v1",digest,id+":"+std::to_string(base));}
void validateDelta(const Json& delta,std::uint64_t base){
 if(!delta.is_object()||delta.value("version",0)!=1||revision(delta.at("baseRevision"))!=base||base==static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max())||revision(delta.at("revision"))!=base+1||!delta.at("patch").is_boolean())throw std::runtime_error("Recovery journal revision or envelope is invalid");
 if(delta.at("patch").get<bool>()?!delta.at("changes").is_array():!delta.at("changes").is_object())throw std::runtime_error("Recovery journal changes are invalid");
}
Json& recoverySheet(Json& book,const Json& change){for(auto& sheet:book.at("sheets"))if(sheet.at("id")==change.at("sheet"))return sheet;throw std::runtime_error("Recovery journal refers to a missing worksheet");}
void applyDelta(Json& book,const Json& delta){
 validateDelta(delta,revision(book.at("revision")));const auto& changes=delta.at("changes");
 if(delta.at("patch").get<bool>())book=book.patch(changes);
 else{
  for(const auto& change:changes.at("cells")){
   auto& cells=recoverySheet(book,change).at("cells");const std::pair<int,int> key{change.at("row"),change.at("col")};
   if(key.first<1||key.first>1048576||key.second<1||key.second>16384)throw std::runtime_error("Recovery cell address is invalid");
   auto at=std::lower_bound(cells.begin(),cells.end(),key,[](const Json& cell,const auto& address){return std::pair<int,int>{cell.at("row"),cell.at("col")}<address;});
   const bool exists=at!=cells.end()&&std::pair<int,int>{at->at("row"),at->at("col")}==key;
   if(change.at("cell").is_null()){if(exists)cells.erase(at);}
   else{if(std::pair<int,int>{change.at("cell").at("row"),change.at("cell").at("col")}!=key)throw std::runtime_error("Recovery cell delta address is inconsistent");if(exists)*at=change.at("cell");else cells.insert(at,change.at("cell"));}
  }
  if(changes.contains("stale"))for(const auto& state:changes.at("stale")){const auto collection=state.value("collection",std::string("pivots"));if(collection!="pivots"&&collection!="financialModels")throw std::runtime_error("Invalid recovery report collection");if(book.contains(collection))for(auto& report:book.at(collection))if(report.at("id")==state.at("id")){if(state.at("present").get<bool>())report["stale"]=state.at("value");else report.erase("stale");}}
  for(const auto* kind:{"metadata","sheetMetadata"})if(changes.contains(kind))for(const auto& change:changes.at(kind)){
   const auto key=change.at("key").get<std::string>();if(key=="revision"||key=="sheets"||key=="cells"||key=="id")throw std::runtime_error("Recovery metadata key is invalid");
   auto& object=std::string(kind)=="metadata"?book:recoverySheet(book,change);if(change.at("present").get<bool>())object[key]=change.at("value");else object.erase(key);
  }
 }
 book["revision"]=delta.at("revision");
}
void migrateJournal(sqlite3* db){
 execute(db,"BEGIN IMMEDIATE");
 try{
  execute(db,"CREATE TABLE recovery_heads(id TEXT PRIMARY KEY REFERENCES checkpoints(id) ON DELETE CASCADE,base_revision INTEGER NOT NULL,revision INTEGER NOT NULL,digest BLOB NOT NULL);CREATE TABLE recovery_journal(id TEXT NOT NULL REFERENCES checkpoints(id) ON DELETE CASCADE,revision INTEGER NOT NULL,base_revision INTEGER NOT NULL,delta BLOB NOT NULL,previous BLOB NOT NULL,digest BLOB NOT NULL,PRIMARY KEY(id,revision));");
  auto rows=prepare(db,"SELECT id,snapshot,source,path,digest FROM checkpoints");
  while(sqlite3_step(rows.get())==SQLITE_ROW){const auto id=bytes(rows.get(),0),snapshot=bytes(rows.get(),1),source=bytes(rows.get(),2),path=bytes(rows.get(),3),digest=bytes(rows.get(),4);if(digest!=checksum(snapshot,source,path))corrupt(id,"checksum mismatch during journal migration");const auto base=revision(parseSnapshot(id,snapshot).at("revision"));auto insert=prepare(db,"INSERT INTO recovery_heads(id,base_revision,revision,digest) VALUES(?1,?2,?2,?3)");bindText(db,insert.get(),1,id);check(sqlite3_bind_int64(insert.get(),2,static_cast<sqlite3_int64>(base)),db);bindBlob(db,insert.get(),3,chainStart(id,digest,base));check(sqlite3_step(insert.get()),db);}
  execute(db,"PRAGMA user_version=3;COMMIT");
 }catch(...){sqlite3_exec(db,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}

std::optional<std::pair<std::string,std::string>> compactedBase(sqlite3* db,const std::string& id,const std::string& originalDigest){
 auto row=prepare(db,"SELECT snapshot,digest FROM recovery_bases WHERE id=?1");bindText(db,row.get(),1,id);const auto result=sqlite3_step(row.get());if(result==SQLITE_DONE)return {};check(result,db);auto snapshot=bytes(row.get(),0),digest=bytes(row.get(),1);if(digest!=checksum(snapshot,originalDigest,id))corrupt(id,"compacted base checksum mismatch");return std::pair{std::move(snapshot),std::move(digest)};
}
struct Head {std::uint64_t base,current;std::string digest;};
Head head(sqlite3* db,const std::string& id){auto row=prepare(db,"SELECT base_revision,revision,digest FROM recovery_heads WHERE id=?1");bindText(db,row.get(),1,id);const auto result=sqlite3_step(row.get());if(result!=SQLITE_ROW){check(result,db);corrupt(id,"journal head is missing");}const auto base=sqlite3_column_int64(row.get(),0),current=sqlite3_column_int64(row.get(),1);if(base<0||current<base)corrupt(id,"journal head revision is invalid");return {static_cast<std::uint64_t>(base),static_cast<std::uint64_t>(current),bytes(row.get(),2)};}
void replay(sqlite3* db,const std::string& id,const std::string& baseDigest,const Head& endpoint,Json* book){
 auto previous=chainStart(id,baseDigest,endpoint.base);auto current=endpoint.base;auto rows=prepare(db,"SELECT revision,base_revision,delta,previous,digest FROM recovery_journal WHERE id=?1 ORDER BY revision");bindText(db,rows.get(),1,id);
 try{for(;;){const auto result=sqlite3_step(rows.get());if(result==SQLITE_DONE)break;check(result,db);const auto raw=bytes(rows.get(),2),digest=bytes(rows.get(),4);const auto delta=Json::parse(raw);validateDelta(delta,current);if(sqlite3_column_int64(rows.get(),0)!=static_cast<sqlite3_int64>(current+1)||sqlite3_column_int64(rows.get(),1)!=static_cast<sqlite3_int64>(current)||bytes(rows.get(),3)!=previous||digest!=checksum(raw,previous,id))corrupt(id,"journal hash or revision chain is invalid");if(book)applyDelta(*book,delta);previous=digest;++current;}
 }catch(const std::exception& failure){corrupt(id,failure.what());}
 if(current!=endpoint.current||previous!=endpoint.digest)corrupt(id,"journal head/tail is incomplete or corrupt");
}
}
RecoveryStore::RecoveryStore(const std::string& path){
 static std::mutex initializationMutex;std::lock_guard initializationLock(initializationMutex);
 const auto result=sqlite3_open_v2(path.c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr);
 if(result!=SQLITE_OK){const auto error=db_?std::string(sqlite3_errmsg(db_)):std::string("Cannot open recovery store");if(db_)sqlite3_close(db_);db_=nullptr;throw std::runtime_error(error);}
 try{
  check(sqlite3_busy_timeout(db_,5000),db_);const auto current=version(db_);const auto fields=columns(db_);
  if(current<0||current>schemaVersion)throw std::runtime_error("Unsupported recovery database version: "+std::to_string(current));
  const std::vector<std::string> legacy{"id","snapshot","source","path"},integrity{"id","snapshot","source","path","digest"};
  if((current>=2&&fields!=integrity)||(current==1&&fields!=legacy)||(current==0&&!fields.empty()&&fields!=legacy))throw std::runtime_error("Unsupported or corrupt recovery database schema");
  execute(db_,"PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;PRAGMA foreign_keys=ON");
  if(current==0&&fields.empty())execute(db_,"BEGIN IMMEDIATE;CREATE TABLE checkpoints(id TEXT PRIMARY KEY,snapshot BLOB NOT NULL,source BLOB NOT NULL,path TEXT NOT NULL,digest BLOB NOT NULL);PRAGMA user_version=2;COMMIT");
  else if(current<2)migrate(db_);
  if(version(db_)<schemaVersion)migrateJournal(db_);
  execute(db_,"CREATE TABLE IF NOT EXISTS recovery_bases(id TEXT PRIMARY KEY REFERENCES checkpoints(id) ON DELETE CASCADE,snapshot BLOB NOT NULL,digest BLOB NOT NULL)");
 }catch(...){sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);sqlite3_close(db_);db_=nullptr;throw;}
}
RecoveryStore::~RecoveryStore(){if(db_)sqlite3_close(db_);}

void RecoveryStore::checkpointImpl(const std::string& id,const Json& book,const std::string& source,const std::string& path,bool missingOnly){
 const auto base=revision(book.at("revision"));execute(db_,"BEGIN IMMEDIATE");
 try{if(missingOnly){auto existing=prepare(db_,"SELECT 1 FROM checkpoints WHERE id=?1");bindText(db_,existing.get(),1,id);const auto found=sqlite3_step(existing.get());if(found==SQLITE_ROW){if(head(db_,id).current!=base)throw std::runtime_error("Recovery journal revision conflict with another writer");execute(db_,"COMMIT");return;}check(found,db_);}
  const auto snapshot=book.dump(),digest=checksum(snapshot,source,path);auto statement=prepare(db_,"INSERT INTO checkpoints(id,snapshot,source,path,digest) VALUES(?1,?2,?3,?4,?5) ON CONFLICT(id) DO UPDATE SET snapshot=excluded.snapshot,source=excluded.source,path=excluded.path,digest=excluded.digest");bindText(db_,statement.get(),1,id);bindBlob(db_,statement.get(),2,snapshot);bindBlob(db_,statement.get(),3,source);bindText(db_,statement.get(),4,path);bindBlob(db_,statement.get(),5,digest);check(sqlite3_step(statement.get()),db_);
  auto clearBase=prepare(db_,"DELETE FROM recovery_bases WHERE id=?1");bindText(db_,clearBase.get(),1,id);check(sqlite3_step(clearBase.get()),db_);auto erase=prepare(db_,"DELETE FROM recovery_journal WHERE id=?1");bindText(db_,erase.get(),1,id);check(sqlite3_step(erase.get()),db_);auto header=prepare(db_,"INSERT INTO recovery_heads(id,base_revision,revision,digest) VALUES(?1,?2,?2,?3) ON CONFLICT(id) DO UPDATE SET base_revision=excluded.base_revision,revision=excluded.revision,digest=excluded.digest");bindText(db_,header.get(),1,id);check(sqlite3_bind_int64(header.get(),2,static_cast<sqlite3_int64>(base)),db_);bindBlob(db_,header.get(),3,chainStart(id,digest,base));check(sqlite3_step(header.get()),db_);execute(db_,"COMMIT");
 }catch(...){sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}
void RecoveryStore::checkpoint(const std::string& id,const Json& book,const std::string& source,const std::string& path){checkpointImpl(id,book,source,path,false);}
void RecoveryStore::checkpointIfMissing(const std::string& id,const Json& book,const std::string& source,const std::string& path){checkpointImpl(id,book,source,path,true);}
void RecoveryStore::append(const std::string& id,const Json& deltas){
 if(!deltas.is_array())throw std::runtime_error("Recovery deltas must be an ordered array");if(deltas.empty())return;execute(db_,"BEGIN IMMEDIATE");
 try{auto endpoint=head(db_,id);for(const auto& delta:deltas){validateDelta(delta,endpoint.current);const auto raw=delta.dump(),digest=checksum(raw,endpoint.digest,id);auto insert=prepare(db_,"INSERT INTO recovery_journal(id,revision,base_revision,delta,previous,digest) VALUES(?1,?2,?3,?4,?5,?6)");bindText(db_,insert.get(),1,id);check(sqlite3_bind_int64(insert.get(),2,static_cast<sqlite3_int64>(endpoint.current+1)),db_);check(sqlite3_bind_int64(insert.get(),3,static_cast<sqlite3_int64>(endpoint.current)),db_);bindBlob(db_,insert.get(),4,raw);bindBlob(db_,insert.get(),5,endpoint.digest);bindBlob(db_,insert.get(),6,digest);check(sqlite3_step(insert.get()),db_);++endpoint.current;endpoint.digest=digest;}
  auto update=prepare(db_,"UPDATE recovery_heads SET revision=?2,digest=?3 WHERE id=?1");bindText(db_,update.get(),1,id);check(sqlite3_bind_int64(update.get(),2,static_cast<sqlite3_int64>(endpoint.current)),db_);bindBlob(db_,update.get(),3,endpoint.digest);check(sqlite3_step(update.get()),db_);execute(db_,"COMMIT");
 }catch(...){sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}

void RecoveryStore::compact(const std::string& id,const Json& book){
 execute(db_,"BEGIN IMMEDIATE");
 try{
  const auto endpoint=head(db_,id);if(revision(book.at("revision"))!=endpoint.current)throw std::runtime_error("Recovery compaction revision conflict");
  auto row=prepare(db_,"SELECT digest FROM checkpoints WHERE id=?1");bindText(db_,row.get(),1,id);if(sqlite3_step(row.get())!=SQLITE_ROW)corrupt(id,"original checkpoint is missing");const auto originalDigest=bytes(row.get(),0);const auto oldBase=compactedBase(db_,id,originalDigest);replay(db_,id,oldBase?oldBase->second:originalDigest,endpoint,nullptr);
  const auto snapshot=book.dump(),digest=checksum(snapshot,originalDigest,id);auto update=prepare(db_,"INSERT INTO recovery_bases(id,snapshot,digest) VALUES(?1,?2,?3) ON CONFLICT(id) DO UPDATE SET snapshot=excluded.snapshot,digest=excluded.digest");bindText(db_,update.get(),1,id);bindBlob(db_,update.get(),2,snapshot);bindBlob(db_,update.get(),3,digest);check(sqlite3_step(update.get()),db_);
  auto erase=prepare(db_,"DELETE FROM recovery_journal WHERE id=?1");bindText(db_,erase.get(),1,id);check(sqlite3_step(erase.get()),db_);auto header=prepare(db_,"UPDATE recovery_heads SET base_revision=revision,digest=?2 WHERE id=?1");bindText(db_,header.get(),1,id);bindBlob(db_,header.get(),2,chainStart(id,digest,endpoint.current));check(sqlite3_step(header.get()),db_);execute(db_,"COMMIT");
 }catch(...){sqlite3_exec(db_,"ROLLBACK",nullptr,nullptr,nullptr);throw;}
}
Json RecoveryStore::records()const{
 auto statement=prepare(db_,"SELECT id,snapshot,source,path,digest FROM checkpoints ORDER BY id");auto records=Json::array();
 for(;;){const auto result=sqlite3_step(statement.get());if(result==SQLITE_DONE)break;check(result,db_);const auto id=bytes(statement.get(),0),snapshot=bytes(statement.get(),1),source=bytes(statement.get(),2),path=bytes(statement.get(),3),digest=bytes(statement.get(),4);if(digest!=checksum(snapshot,source,path))corrupt(id,"checksum mismatch");const auto base=compactedBase(db_,id,digest);auto book=parseSnapshot(id,base?base->first:snapshot);const auto endpoint=head(db_,id);if(revision(book.at("revision"))!=endpoint.base)corrupt(id,"base revision differs from journal head");replay(db_,id,base?base->second:digest,endpoint,&book);records.push_back({{"id",id},{"workbook",std::move(book)},{"sourceBytes",source},{"sourcePath",path}});}
 return records;
}
void RecoveryStore::erase(const std::string& id){auto statement=prepare(db_,"DELETE FROM checkpoints WHERE id=?1");bindText(db_,statement.get(),1,id);check(sqlite3_step(statement.get()),db_);}

struct RecoveryDriver::State {
 enum class Kind {Seed,Append,Persist,Erase,Barrier,Stop};
 struct Command {
  Kind kind;std::string id;Json data;std::shared_ptr<const std::string> source;std::string path;std::shared_ptr<std::promise<void>> barrier;
 };
 struct Document {std::string id;Json book;std::shared_ptr<const std::string> source;std::string path;bool persisted=false;std::size_t transactions=0,journalBytes=0;};
 RecoveryDriver* owner;std::string path;std::mutex mutex;std::condition_variable wake;std::deque<Command> queue;std::thread worker;
 explicit State(RecoveryDriver* driver,std::string databasePath):owner(driver),path(std::move(databasePath)),worker([this]{run();}){}
 void enqueue(Command command){{std::lock_guard lock(mutex);queue.push_back(std::move(command));}wake.notify_one();}
 void reportError(std::string message){QMetaObject::invokeMethod(owner,[driver=owner,message=std::move(message)]{if(driver->onError)driver->onError(message);},Qt::QueuedConnection);}
 void reportDurable(const std::string& id,std::uint64_t value){QMetaObject::invokeMethod(owner,[driver=owner,id,value]{if(driver->onDurable)driver->onDurable(id,value);},Qt::QueuedConnection);}
 void run(){
  std::unique_ptr<RecoveryStore> store;std::exception_ptr initialization;std::map<std::string,std::string> failures;std::optional<Document> document;
  try{store=std::make_unique<RecoveryStore>(path);}catch(const std::exception& failure){initialization=std::current_exception();reportError(failure.what());}
  for(;;){
   Command command;{std::unique_lock lock(mutex);wake.wait(lock,[&]{return !queue.empty();});command=std::move(queue.front());queue.pop_front();}
   if(command.kind==Kind::Stop)return;
   if(command.kind==Kind::Barrier){try{if(initialization)std::rethrow_exception(initialization);if(!failures.empty())throw std::runtime_error(failures.begin()->second);command.barrier->set_value();}catch(...){command.barrier->set_exception(std::current_exception());}continue;}
   try{
    if(initialization)std::rethrow_exception(initialization);
    if(command.kind==Kind::Seed){revision(command.data.at("revision"));document=Document{std::move(command.id),std::move(command.data),std::move(command.source),std::move(command.path)};failures.erase(document->id);continue;}
    if(command.kind==Kind::Erase){store->erase(command.id);if(document&&document->id==command.id){document->persisted=false;document->transactions=0;document->journalBytes=0;}failures.erase(command.id);continue;}
    if(failures.contains(command.id))throw std::runtime_error(failures.at(command.id));
    if(!document||document->id!=command.id)throw std::runtime_error("Recovery document identity was not seeded");
    if(command.kind==Kind::Persist){if(!document->persisted){static const std::string empty;store->checkpoint(document->id,document->book,document->source?*document->source:empty,document->path);document->persisted=true;}reportDurable(document->id,revision(document->book.at("revision")));continue;}
    if(!command.data.is_array())throw std::runtime_error("Recovery deltas must be an ordered array");if(command.data.empty())continue;
    auto base=revision(document->book.at("revision"));for(const auto& delta:command.data){validateDelta(delta,base);++base;}
    {static const std::string empty;store->checkpointIfMissing(document->id,document->book,document->source?*document->source:empty,document->path);document->persisted=true;}
    const auto journalBytes=command.data.dump().size();for(const auto& delta:command.data)applyDelta(document->book,delta);
    store->append(command.id,command.data);document->transactions+=command.data.size();document->journalBytes+=journalBytes;
    if(document->transactions>=128||document->journalBytes>=16U*1024U*1024U){store->compact(document->id,document->book);document->transactions=0;document->journalBytes=0;}
    reportDurable(document->id,revision(document->book.at("revision")));
   }catch(const std::exception& failure){failures[command.id]=failure.what();reportError(failure.what());}
  }
 }
};
RecoveryDriver::RecoveryDriver(const std::string& path,QObject* parent):QObject(parent),state_(std::make_unique<State>(this,path)){}
RecoveryDriver::~RecoveryDriver(){try{flush();}catch(...){/* The queued error callback and explicit flush report failures. */}state_->enqueue({State::Kind::Stop});state_->worker.join();}
void RecoveryDriver::seed(std::string id,Json base,std::shared_ptr<const std::string> source,std::string path){state_->enqueue({State::Kind::Seed,std::move(id),std::move(base),std::move(source),std::move(path)});}
void RecoveryDriver::append(std::string id,Json deltas){state_->enqueue({State::Kind::Append,std::move(id),std::move(deltas)});}
void RecoveryDriver::persist(std::string id){state_->enqueue({State::Kind::Persist,std::move(id)});}
void RecoveryDriver::erase(std::string id){state_->enqueue({State::Kind::Erase,std::move(id)});}
void RecoveryDriver::flush(){auto barrier=std::make_shared<std::promise<void>>();auto future=barrier->get_future();State::Command command{State::Kind::Barrier};command.barrier=barrier;state_->enqueue(std::move(command));future.get();}

}
