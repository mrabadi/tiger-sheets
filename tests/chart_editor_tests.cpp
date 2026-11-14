#include "tigersheets/chart_editor.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <iostream>
using tigersheets::Json;
int main(int argc,char** argv){
 QApplication app(argc,argv);
 const Json workbook={{"sheets",Json::array({{{"id","sales"},{"name","Sales 2026"}},{{"id","budget"},{"name","Budget"}}})}};
 const Json original={{"id","chart-17"},{"sheet","sales"},{"type","column"},{"title","Revenue"},{"range",{{"row1",1},{"col1",1},{"row2",4},{"col2",3}}},{"anchor",{{"row",7},{"col",5},{"width",560},{"height",300}}},{"extension",{{"preserve",true}}}};
 tigersheets::ChartEditorDialog cancelled(workbook,original);
 cancelled.show();app.processEvents();
 auto* title=cancelled.findChild<QLineEdit*>("chartTitle");
 if(!title){std::cerr<<"FAILED: chart editor exposes a native title field\n";return 1;}
 title->setText("Uncommitted title");
 QTest::keyClick(&cancelled,Qt::Key_Escape);
 if(cancelled.resultSpecification()||original["title"]!="Revenue"){
  std::cerr<<"FAILED: cancelling chart edits leaves the original specification untouched\n";return 1;
 }
 tigersheets::ChartEditorDialog edited(workbook,original);
 edited.show();app.processEvents();
 auto* range=edited.findChild<QLineEdit*>("chartRange");auto* type=edited.findChild<QComboBox*>("chartType");auto* sheet=edited.findChild<QComboBox*>("chartSheet");
 if(!range||!type||!sheet){std::cerr<<"FAILED: chart editor exposes type, worksheet and A1 range fields\n";return 1;}
 edited.findChild<QLineEdit*>("chartTitle")->setText("Updated forecast");
 range->setText("$B$2:$D$8");type->setCurrentIndex(type->findData("line"));sheet->setCurrentIndex(sheet->findData("budget"));
 auto* buttons=edited.findChild<QDialogButtonBox*>("chartDialogButtons");
 QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);
 const auto result=edited.resultSpecification();
 if(!result||result->at("id")!="chart-17"||result->at("title")!="Updated forecast"||result->at("type")!="line"||result->at("sheet")!="budget"||result->at("range")!=Json({{"row1",2},{"col1",2},{"row2",8},{"col2",4}})||result->at("anchor")!=original.at("anchor")||result->at("extension")!=original.at("extension")){
  std::cerr<<"FAILED: accepting validated edits preserves chart identity, anchor and unrelated metadata\n";return 1;
 }
 for(const auto& invalid:{"A1:A4","A1:C1","A0:C4","B4:A1","A1:XFE4","A1:B1048577","A1:B10003","A1:CX4","not a range"}){
  tigersheets::ChartEditorDialog invalidDialog(workbook,original);invalidDialog.show();app.processEvents();
  invalidDialog.findChild<QLineEdit*>("chartRange")->setText(invalid);
  QTest::mouseClick(invalidDialog.findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Ok),Qt::LeftButton);
  const auto* error=invalidDialog.findChild<QLabel*>("chartValidationError");
  if(invalidDialog.resultSpecification()||!invalidDialog.isVisible()||!error||error->text().isEmpty()){
   std::cerr<<"FAILED: invalid chart range must remain editable with a useful error: "<<invalid<<"\n";return 1;
  }
 }
 auto unsupported=original;unsupported["type"]="waterfall";
 tigersheets::ChartEditorDialog retained(workbook,unsupported);retained.show();app.processEvents();
 QTest::mouseClick(retained.findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Ok),Qt::LeftButton);
 if(retained.resultSpecification()||unsupported["type"]!="waterfall"){
  std::cerr<<"FAILED: unsupported native chart type requires an explicit supported choice\n";return 1;
 }
 tigersheets::ChartEditorDialog edge(workbook,original,nullptr,tigersheets::ChartEditAction::Data);edge.show();app.processEvents();
 edge.findChild<QLineEdit*>("chartRange")->setText("$xfc$1048575:$xfd$1048576");
 QTest::mouseClick(edge.findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Ok),Qt::LeftButton);
 const auto edgeResult=edge.resultSpecification();
 if(!edgeResult||edgeResult->at("range")!=Json({{"row1",1048575},{"col1",16383},{"row2",1048576},{"col2",16384}})){
  std::cerr<<"FAILED: chart editor accepts legal lowercase absolute references at worksheet bounds\n";return 1;
 }
 tigersheets::ChartEditorDialog missingSheet(workbook,original);missingSheet.show();app.processEvents();
 missingSheet.findChild<QComboBox*>("chartSheet")->setCurrentIndex(-1);
 QTest::mouseClick(missingSheet.findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Ok),Qt::LeftButton);
 if(missingSheet.resultSpecification()||missingSheet.findChild<QLabel*>("chartValidationError")->text().isEmpty()){
  std::cerr<<"FAILED: a chart data worksheet is required\n";return 1;
 }
 auto pivot=original;pivot["pivotId"]="report-1";tigersheets::ChartEditorDialog pivotEditor(workbook,pivot);pivotEditor.show();app.processEvents();auto* pivotType=pivotEditor.findChild<QComboBox*>("chartType");auto* pivotRange=pivotEditor.findChild<QLineEdit*>("chartRange");auto* pivotSheet=pivotEditor.findChild<QComboBox*>("chartSheet");
 if(pivotType->count()!=3||pivotType->findData("line")<0||pivotType->findData("column")<0||pivotType->findData("bar")<0||pivotRange->isEnabled()||pivotSheet->isEnabled()){std::cerr<<"FAILED: PivotChart editor must offer only line/column/bar and protect its report-owned data source\n";return 1;}
 pivotRange->setText("A1:B2");pivotSheet->setCurrentIndex(pivotSheet->findData("budget"));pivotType->setCurrentIndex(pivotType->findData("bar"));pivotEditor.findChild<QLineEdit*>("chartTitle")->setText("Native pivot chart");QTest::mouseClick(pivotEditor.findChild<QDialogButtonBox*>("chartDialogButtons")->button(QDialogButtonBox::Ok),Qt::LeftButton);const auto pivotResult=pivotEditor.resultSpecification();
 if(!pivotResult||pivotResult->at("pivotId")!="report-1"||pivotResult->at("sheet")!=original.at("sheet")||pivotResult->at("range")!=original.at("range")||pivotResult->at("type")!="bar"){std::cerr<<"FAILED: PivotChart title/type editing must preserve its linked source identity and range\n";return 1;}

}
