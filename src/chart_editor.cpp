#include "tigersheets/chart_editor.hpp"
#include "tigersheets/references.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
namespace tigersheets {
namespace {
const QStringList types={"line","column","bar","area","scatter","pie","doughnut"};
}
ChartEditorDialog::ChartEditorDialog(const Json& workbook,const Json& specification,QWidget* parent,ChartEditAction focus):QDialog(parent),original_(specification){
 setWindowTitle("Edit chart");setObjectName("chartEditor");setMinimumWidth(390);
 auto* layout=new QFormLayout(this);
 title_=new QLineEdit(QString::fromStdString(specification.value("title",std::string())),this);
 title_->setObjectName("chartTitle");layout->addRow("Title",title_);
 type_=new QComboBox(this);type_->setObjectName("chartType");
 for(const auto& type:types)if(!specification.contains("pivotId")||type=="line"||type=="column"||type=="bar")type_->addItem(type,type);
 const auto currentType=QString::fromStdString(specification.value("type",std::string("line")));
 if(type_->findData(currentType)<0)type_->addItem("Unsupported: "+currentType,currentType);
 type_->setCurrentIndex(type_->findData(currentType));layout->addRow("Chart type",type_);
 sheet_=new QComboBox(this);sheet_->setObjectName("chartSheet");
 for(const auto& sheet:workbook.at("sheets"))sheet_->addItem(QString::fromStdString(sheet.at("name").get<std::string>()),QString::fromStdString(sheet.at("id").get<std::string>()));
 sheet_->setCurrentIndex(sheet_->findData(QString::fromStdString(specification.at("sheet").get<std::string>())));layout->addRow("Worksheet",sheet_);
 const auto& range=specification.at("range");
 const auto address=QString("%1%2:%3%4").arg(QString::fromStdString(columnName(range.at("col1").get<int>()))).arg(range.at("row1").get<int>()).arg(QString::fromStdString(columnName(range.at("col2").get<int>()))).arg(range.at("row2").get<int>());
 range_=new QLineEdit(address,this);range_->setObjectName("chartRange");range_->setPlaceholderText("A1:C6");layout->addRow("Data range, including headers",range_);
 if(specification.contains("pivotId")){range_->setEnabled(false);sheet_->setEnabled(false);auto* note=new QLabel("Data comes from the linked PivotTable. Use the report field controls to change its source.",this);note->setWordWrap(true);layout->addRow(note);}
 error_=new QLabel(this);error_->setObjectName("chartValidationError");error_->setWordWrap(true);error_->setStyleSheet("color: #a43825");layout->addRow(error_);
 auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
 buttons->setObjectName("chartDialogButtons");layout->addRow(buttons);
 connect(buttons,&QDialogButtonBox::accepted,this,&ChartEditorDialog::accept);
 connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
 if(focus==ChartEditAction::Data)range_->setFocus();else if(focus==ChartEditAction::Type)type_->setFocus();else title_->setFocus();
}
void ChartEditorDialog::accept(){
 if(original_.contains("pivotId")){const auto type=type_->currentData().toString();if(type!="line"&&type!="column"&&type!="bar"){error_->setText("Choose line, column or bar for this PivotChart.");return;}auto candidate=original_;candidate["title"]=title_->text().toStdString();candidate["type"]=type.toStdString();accepted_=std::move(candidate);QDialog::accept();return;}
 const QRegularExpression expression("^\\$?([A-Za-z]{1,3})\\$?([0-9]{1,7}):\\$?([A-Za-z]{1,3})\\$?([0-9]{1,7})$");
 const auto match=expression.match(range_->text().trimmed());
 if(!match.hasMatch()){error_->setText("Enter a rectangular A1 range, such as A1:C6.");return;}
 const auto row1=match.captured(2).toInt(),row2=match.captured(4).toInt();
 const auto col1=columnNumber(match.captured(1).toUpper().toStdString()),col2=columnNumber(match.captured(3).toUpper().toStdString());
 if(row1<1||row2>MaxRows||row2<=row1||col1<1||col2>MaxColumns||col2<=col1||row2-row1>10000||col2-col1>100){
  error_->setText("Use at least two rows and two columns within A1:XFD1048576, with up to 10,000 data rows and 100 series.");return;
 }
 if(sheet_->currentIndex()<0){error_->setText("Select a worksheet for the chart data.");return;}
 if(!types.contains(type_->currentData().toString())){error_->setText("Choose a supported chart type to edit this chart.");return;}
 auto candidate=original_;
 candidate["title"]=title_->text().toStdString();candidate["type"]=type_->currentData().toString().toStdString();candidate["sheet"]=sheet_->currentData().toString().toStdString();
 candidate["range"]={{"row1",row1},{"col1",col1},{"row2",row2},{"col2",col2}};
 accepted_=std::move(candidate);QDialog::accept();
}
std::optional<Json> ChartEditorDialog::resultSpecification()const{return result()==QDialog::Accepted?accepted_:std::nullopt;}
std::optional<Json> editChart(QWidget* parent,const Json& workbook,const Json& specification,ChartEditAction focus){
 ChartEditorDialog dialog(workbook,specification,parent,focus);dialog.exec();return dialog.resultSpecification();
}
}
