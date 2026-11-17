#include "tigersheets/grid.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void dateEntriesRoundTrip(){WorkbookSession session;WorksheetModel model(session);require(model.setData(model.index(2,2),"9/30/2025",Qt::EditRole)&&model.setData(model.index(2,3),"=EOMONTH(C3,1)",Qt::EditRole),"enter the exact observed month-end date workflow");model.setCalculated(CalculationEngine().calculate(session.snapshotView()));require(model.numericValue(2,2).value_or(-1)==45930&&model.numericValue(2,3).value_or(-1)==45961&&model.data(model.index(2,3)).toString()=="45961","entered dates retain their date format while source EOMONTH in General displays its numeric serial");QTemporaryDir directory;const auto file=directory.filePath("date-entry.xlsx");writeWorkbook(file.toStdString(),session.snapshotView());WorkbookSession reopened(readWorkbook(file.toStdString()).workbook);WorksheetModel restored(reopened);restored.setCalculated(CalculationEngine().calculate(reopened.snapshotView()));require(restored.raw(2,2)=="45930"&&restored.raw(2,3)=="=EOMONTH(C3,1)"&&restored.data(restored.index(2,2)).toString()=="9/30/2025"&&restored.numericValue(2,3).value_or(-1)==45961&&restored.data(restored.index(2,3)).toString()=="45961","typed dates and the source General EOMONTH serial survive native XLSX save/reopen");}
int main(int argc,char**argv){QApplication app(argc,argv);try{WorkbookSession session;WorksheetModel model(session);const auto before=session.snapshot();
 require(model.setData(model.index(0,0),"10%",Qt::EditRole),"percentage entry is accepted");model.setCalculated(CalculationEngine().calculate(session.snapshotView()));
 require(model.data(model.index(0,0)).toString()=="10%","typing 10% displays a percentage rather than its decimal fraction");
 require(model.raw(0,0)=="0.1"&&model.numericValue(0,0).value_or(-1)==0.1,"percentage entry stores a numeric fraction");
 QTemporaryDir dir;const auto file=dir.filePath("percentage.xlsx");writeWorkbook(file.toStdString(),session.snapshotView());const auto book=readWorkbook(file.toStdString()).workbook;require(book.at("sheets")[0].at("cells")[0].at("input")=="0.1","percentage entry survives XLSX as a numeric cell");
 require(session.undo()&&session.snapshotView().at("sheets")==before.at("sheets"),"one undo restores both the entered value and its inferred format");
 require(model.apply(Json::array({{{"type","set_style"},{"sheet",model.sheetId().toStdString()},{"row",1},{"col",1},{"style",{{"numberFormat","0.00%"}}}}})),"seed inherited percent format");require(model.setData(model.index(0,0),"15",Qt::EditRole),"replace a formatted percentage with fifteen");model.setCalculated(CalculationEngine().calculate(session.snapshotView()));require(model.numericValue(0,0).value_or(-1)==0.15&&model.data(model.index(0,0)).toString()=="15.00%","entering fifteen in an inherited percent cell means fifteen percent");
 require(model.apply(Json::array({{{"type","set_style"},{"sheet",model.sheetId().toStdString()},{"row",1},{"col",1},{"style",{{"numberFormat","\"%\"0"}}}}})),"seed a quoted percent literal format");require(model.setData(model.index(0,0),"15",Qt::EditRole),"enter a number into literal-percent formatting");model.setCalculated(CalculationEngine().calculate(session.snapshotView()));require(model.numericValue(0,0).value_or(-1)==15,"quoted percent literals must not rescale numeric entry");
 dateEntriesRoundTrip();std::cout<<"percentage and date entry tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
