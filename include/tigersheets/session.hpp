#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <string_view>
namespace tigersheets {
using Json = nlohmann::json;
class WorkbookSession {
public:
 class PreparedTransaction {
 public:
  ~PreparedTransaction();
  const Json& snapshot() const noexcept;
  std::uint64_t baseRevision() const noexcept;
  bool changed() const noexcept;
 private:
  struct Data;
  std::unique_ptr<Data> data_;
  explicit PreparedTransaction(std::unique_ptr<Data> data);
  friend class WorkbookSession;
 };
 // Prepare on a worker from an owned canonical capture; publication moves the
 // validated candidate/delta and keeps retired roots in the holder for worker disposal.
 static std::unique_ptr<PreparedTransaction> prepareTransaction(Json captured,const Json& operations,std::string* error=nullptr);
 bool commitPrepared(std::uint64_t expectedRevision,PreparedTransaction& prepared,std::string* error=nullptr);
 bool previewPrepared(std::uint64_t expectedRevision,std::unique_ptr<PreparedTransaction>& incoming,std::string* error=nullptr);
 bool acceptPrepared(std::unique_ptr<PreparedTransaction>& retired,std::string* error=nullptr);
 std::unique_ptr<PreparedTransaction> discardPrepared();
 const Json* previewView() const noexcept;
 WorkbookSession();
 explicit WorkbookSession(Json workbook);
 Json snapshot() const;
 const Json& snapshotView() const noexcept;
 std::shared_ptr<const Json> captureSnapshot() const;
 const Json& lastChange() const noexcept;
 const Json* cellAt(std::string_view sheet,int row,int col) const noexcept;
 std::size_t historyBytes() const noexcept;
 Json takeRecoveryDelta();
 bool apply(std::uint64_t expectedRevision,const Json& operations,std::string* error=nullptr);
 bool undo(std::string* error=nullptr);
 bool redo(std::string* error=nullptr);
 Json preview(std::uint64_t expectedRevision,const Json& operations,std::string* error=nullptr);
 bool accept(std::string* error=nullptr);
 void discard();
 bool hasPreview() const;
 bool canUndo() const;
 bool canRedo() const;
private:
 struct HistoryEntry {Json undo,redo,change;std::size_t bytes=0;bool patch=false;};
 Json workbook_;
 Json recoveryDeltas_=Json::array();
 std::vector<HistoryEntry> undo_,redo_;
 std::size_t historyBytes_=0;
 Json lastChange_={{"structure",true},{"metadata",false},{"sheets",Json::array()}};
 std::optional<Json> preview_;
 std::unique_ptr<PreparedTransaction> preparedPreview_;
 std::uint64_t previewBase_{};
 bool applyCells(const Json& operations,std::string* error);
 void remember(HistoryEntry entry);
 void restore(const HistoryEntry& entry,bool forward);
 HistoryEntry difference(const Json& before,const Json& after) const;
};
}
