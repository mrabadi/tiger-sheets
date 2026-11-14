#pragma once
#include "tigersheets/session.hpp"
#include <QAbstractTableModel>
#include <functional>
#include <map>
#include <optional>
namespace tigersheets {
class WorksheetModel : public QAbstractTableModel {
public:
 explicit WorksheetModel(WorkbookSession& session,QObject* parent=nullptr);
 ~WorksheetModel() override;
 int rowCount(const QModelIndex& parent={}) const override;
 int columnCount(const QModelIndex& parent={}) const override;
 QVariant data(const QModelIndex& index,int role=Qt::DisplayRole) const override;
 QVariant headerData(int section,Qt::Orientation orientation,int role=Qt::DisplayRole) const override;
 Qt::ItemFlags flags(const QModelIndex& index) const override;
 bool setData(const QModelIndex& index,const QVariant& value,int role=Qt::EditRole) override;
 void refresh();
 QString sheetId() const;
 void setSheetId(QString id);
 QString raw(int row,int col) const;
 std::optional<double> numericValue(int row,int col) const;
 bool apply(const Json& operations);
 void setCalculated(const Json& result);
 Json copyRange(int row1,int col1,int row2,int col2) const;
 Json cutRange(int row1,int col1,int row2,int col2) const;
 bool pasteRange(int row,int col,const Json& clipboard,const std::string& mode="all",bool skipBlanks=false,int targetRows=1,int targetColumns=1);
 bool sortRange(int row1,int col1,int row2,int col2,bool ascending);
 QString lastError() const;
 const Json& workbookView() const noexcept;
 Json calculatedSnapshot() const;
 Json cellStyle(int row,int col) const;
 void showPreview(const Json& candidate);
 // Caller retains this immutable candidate until refresh or replacement.
 void showPreviewView(const Json& candidate);
 std::function<void()> onChanged;
private:
 friend class SelectionStatistics;
 WorkbookSession& session_;
 QString sheet_;
 std::map<std::pair<int,int>,Json> cells_;
 std::map<std::pair<int,int>,Json> values_;
 std::map<std::pair<int,int>,std::string> valueTypes_;
 std::optional<Json> preview_;
 const Json* previewView_=nullptr;
 bool readOnly_=false;
 bool date1904_=false;
 bool valuesCurrent_=false;
 std::uint64_t displayRevision_=0;
 QString error_;
 QString clipboardOwner_;
 void loadSnapshot(const Json& snapshot);
 bool moveRange(int row,int col,const Json& clipboard);
};
}
