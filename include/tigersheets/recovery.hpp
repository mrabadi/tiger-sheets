#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <QObject>
#include <functional>
#include <memory>
#include <cstdint>
struct sqlite3;
namespace tigersheets {
using Json=nlohmann::json;
class RecoveryStore {
public:
 explicit RecoveryStore(const std::string& path);
 ~RecoveryStore();
 RecoveryStore(const RecoveryStore&)=delete;
 RecoveryStore& operator=(const RecoveryStore&)=delete;
 void checkpoint(const std::string& id,const nlohmann::json& workbook,const std::string& sourceBytes={},const std::string& sourcePath={});
 void append(const std::string& id,const Json& deltas);
 void checkpointIfMissing(const std::string& id,const Json& workbook,const std::string& sourceBytes={},const std::string& sourcePath={});
 void compact(const std::string& id,const Json& workbook);
 nlohmann::json records() const;
 void erase(const std::string& id);
private:
 void checkpointImpl(const std::string& id,const Json& workbook,const std::string& sourceBytes,const std::string& sourcePath,bool missingOnly);
 sqlite3* db_{};
};
class RecoveryDriver : public QObject {
public:
 explicit RecoveryDriver(const std::string& databasePath,QObject* parent=nullptr);
 ~RecoveryDriver() override;
 void seed(std::string id,Json base,std::shared_ptr<const std::string> originalBytes,std::string sourcePath);
 void append(std::string id,Json deltas);
 void persist(std::string id);
 void erase(std::string id);
 void flush();
 std::function<void(const std::string&)> onError;
 std::function<void(const std::string&,std::uint64_t)> onDurable;
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
