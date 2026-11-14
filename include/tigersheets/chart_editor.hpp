#pragma once
#include "tigersheets/chart.hpp"
#include <QDialog>
#include <optional>
class QLineEdit;
class QComboBox;
class QLabel;
namespace tigersheets {
class ChartEditorDialog : public QDialog {
public:
 ChartEditorDialog(const Json& workbook,const Json& specification,QWidget* parent=nullptr,ChartEditAction focus=ChartEditAction::General);
 std::optional<Json> resultSpecification() const;
 void accept() override;
private:
 Json original_;
 std::optional<Json> accepted_;
 QLineEdit* title_=nullptr;
 QLineEdit* range_=nullptr;
 QComboBox* sheet_=nullptr;
 QComboBox* type_=nullptr;
 QLabel* error_=nullptr;
};
std::optional<Json> editChart(QWidget* parent,const Json& workbook,const Json& specification,ChartEditAction focus=ChartEditAction::General);
}
