#include "tigersheets/calculation.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/formula_error_checks.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_cell_format.hpp"
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QHeaderView>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
Json dateHeaders(){
 auto book=WorkbookSession().snapshot();auto& cells=book["sheets"][0]["cells"];
 for(int column=3;column<=5;++column){cells.push_back({{"row",2},{"col",column},{"input",std::to_string(43830+(column-3)*366-(column==5?1:0))},{"style",{{"numberFormat","\"FY\" yy"},{"bold",true}}}});cells.push_back({{"row",24},{"col",column},{"input","="+std::string(1,char('A'+column-1))+"2"}});}
 return book;
}
void formatsPasteRetainsCalculatedDates(){
 WorkbookSession session(dateHeaders());WorksheetModel model(session);model.setCalculated(CalculationEngine().calculate(session.snapshotView()));
 require(model.data(model.index(23,2)).toString()=="43830","fixture initially displays a calculated date serial");
 const auto input=model.raw(23,2);const auto clip=model.copyRange(1,2,1,4);
 require(model.pasteRange(23,2,clip,"formats"),"native Formats paste succeeds");
 // Deliberately observe synchronously, before any background recalculation.
 for(int column=2;column<=4;++column){
  require(model.data(model.index(23,column)).toString()==QString("FY %1").arg(19+column-2),"Formats paste must immediately display calculated FY headers rather than raw formula text");
  require(model.numericValue(23,column).has_value(),"Formats paste retains typed numeric results");
  const auto copied=model.copyRange(23,column,23,column).at("cells")[0][0];
  require(copied.contains("value")&&copied.at("value").is_number()&&copied.at("valueType")=="number","Formats paste retains current numeric clipboard values");
 }
 require(model.raw(23,2)==input&&model.cellStyle(23,2).at("bold")==true&&model.calculatedSnapshot().at("status")=="ok","format-only cache retention preserves formulas and applies the new style");
}
void nativeStyleUndoRetainsCalculation(){
 MainWindow window;auto* model=dynamic_cast<WorksheetModel*>(window.findChild<QTableView*>("worksheet.grid")->model());
 require(model&&model->setData(model->index(0,0),"42",Qt::EditRole)&&model->setData(model->index(0,1),"=A1*2",Qt::EditRole),"create native calculated formula");window.recalculateNow();
 const auto before=window.currentSession().snapshot();
 require(model->apply(Json::array({{{"type","set_style"},{"sheet",model->sheetId().toStdString()},{"row",1},{"col",2},{"style",{{"numberFormat","0.00"}}}}})),"format native calculated formula");
 require(model->data(model->index(0,1)).toString()=="84.00","native formatting immediately changes calculated display");
 require(window.executeCommand("undo"),"undo native formatting");
 require(model->data(model->index(0,1)).toString()=="84"&&model->numericValue(0,1)==84&&model->copyRange(0,1,0,1).at("cells")[0][0].at("value")==84,"style Undo immediately restores calculated display and numeric clipboard eligibility");
 require(window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"one Undo restores exact original cell inputs and styles");
 require(window.executeCommand("redo")&&model->data(model->index(0,1)).toString()=="84.00"&&model->numericValue(0,1)==84,"style Redo immediately restores the calculated formatted display");
 require(model->raw(0,1)=="=A1*2"&&window.currentSession().snapshotView().at("revision")==before.at("revision").get<std::uint64_t>()+3,"formatting Undo and Redo preserve formula input and one edit per transaction");
}
void typedValuesAndInvalidationBoundaries(){
 auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"]=Json::array({{{"row",1},{"col",1},{"input","42"}},{{"row",1},{"col",2},{"input","=-A1*2"}},{{"row",1},{"col",3},{"input","=TRUE()"}},{{"row",1},{"col",4},{"input","=\"text\""}},{{"row",1},{"col",5},{"input","=1/0"}}});
 WorkbookSession session(book);WorksheetModel model(session);const auto calculated=CalculationEngine().calculate(session.snapshotView());model.setCalculated(calculated);
 require(model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",2},{"input","=-A1*2"},{"style",{{"numberFormat","0.00;[Red](0.00)"}}}},{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",3},{"style",{{"bold",true}}}},{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",4},{"style",{{"italic",true}}}},{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",5},{"style",{{"bold",true}}}},{{"type","set_style"},{"sheet","sheet-1"},{"row",50},{"col",9},{"style",{{"bold",true}}}}})),"format unchanged formula inputs and a genuinely empty cell");
 require(!session.lastChange().at("calculationInputsChanged").get<bool>()&&model.calculatedSnapshot().at("status")=="ok","classification uses final input equality, including unchanged set_cell and styled blank materialization");
 require(model.data(model.index(0,1)).toString()=="(84.00)"&&model.numericValue(0,1)==-84&&model.data(model.index(0,1),Qt::ForegroundRole).value<QColor>()==QColor("red"),"new number format and its conditional ink use the retained calculated number immediately");
 const auto clip=model.copyRange(0,1,0,4).at("cells")[0];
 require(clip[0].at("value")==-84&&clip[0].at("valueType")=="number"&&clip[1].at("value")==true&&clip[1].at("valueType")=="boolean"&&clip[2].at("value")=="text"&&clip[2].at("valueType")=="text"&&clip[3].at("value")=="#DIV/0!"&&clip[3].at("valueType")=="error","numeric, boolean, text and error cache types remain copyable after formatting");
 auto stale=calculated;for(auto& value:stale["values"])if(value.at("row")==1&&value.at("col")==2)value["value"]=999;model.setCalculated(stale);
 require(model.numericValue(0,1)==-84,"old-revision results remain rejected after style revision carry");
 require(model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","50"}},{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",2},{"style",{{"bold",true}}}}})),"change a real precedent and formatting atomically");
 require(session.lastChange().at("calculationInputsChanged")==true&&model.calculatedSnapshot().at("status")=="stale"&&!model.numericValue(0,1).has_value()&&!model.copyRange(0,1,0,1).at("cells")[0][0].contains("value"),"actual input/dependency change invalidates numeric and copy cache even when mixed with formatting");
 require(model.apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",2},{"style",{{"italic",true}}}}}))&&model.calculatedSnapshot().at("status")=="stale"&&!model.numericValue(0,1).has_value(),"a later style edit cannot promote a previously stale cache");
 model.setCalculated(CalculationEngine().calculate(session.snapshotView()));require(model.numericValue(0,1)==-100,"real calculation publishes the changed input result");
 model.setCalculated({{"revision",session.snapshotView().at("revision")},{"status","ok"},{"iteration",{{"converged",false}}},{"values",Json::array()}});
 require(model.apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",2},{"style",{{"underline",true}}}}}))&&model.calculatedSnapshot().at("status")=="stale"&&!model.copyRange(0,1,0,1).at("cells")[0][0].contains("value"),"formatting cannot resurrect a failed or nonconverged calculation");
 model.setCalculated(CalculationEngine().calculate(session.snapshotView()));
 require(model.apply(Json::array({{{"type","set_metadata"},{"key","names"},{"value",{{"Rate","Sheet1!$A$1"}}}}}))&&model.calculatedSnapshot().at("status")=="stale","dependency metadata retains conservative invalidation");
 model.setCalculated(CalculationEngine().calculate(session.snapshotView()));
 require(model.apply(Json::array({{{"type","insert_rows"},{"sheet","sheet-1"},{"index",1},{"count",1}}}))&&model.calculatedSnapshot().at("status")=="stale","structural changes still discard calculated caches");
}
void nativeExternalWarningContinuity(){
 QTemporaryDir files;MainWindow source,origin;auto* sourceModel=dynamic_cast<WorksheetModel*>(source.findChild<QTableView*>("worksheet.grid")->model());auto* model=dynamic_cast<WorksheetModel*>(origin.findChild<QTableView*>("worksheet.grid")->model());
 Json inputs=Json::array();const int amounts[]={1782,17,3148,458,659,429,6493};for(int row=7;row<=13;++row)inputs.push_back({{"type","set_cell"},{"sheet",sourceModel->sheetId().toStdString()},{"row",row},{"col",2},{"input",std::to_string(amounts[row-7])}});
 require(sourceModel->apply(inputs)&&source.saveWorkbook(files.filePath("Warning Source.xlsx")),"prepare actual live external numeric neighbors");
 require(model->apply(Json::array({{{"type","set_metadata"},{"key","links"},{"value",Json::array({{{"name","Warning Source.xlsx"},{"path",files.filePath("Warning Source.xlsx").toStdString()},{"enabled",true}}})}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",27},{"col",3},{"input","=SUM('[Warning Source.xlsx]Sheet1'!$B$7:$B$8)"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",28},{"col",3},{"input","='[Warning Source.xlsx]Sheet1'!$B$9"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",29},{"col",3},{"input","=SUM('[Warning Source.xlsx]Sheet1'!$B$10:$B$12)"}}})),"enter observed external SUM ranges and direct-reference control");origin.recalculateNow();
 const auto warnings=[&]{require(model->numericValue(26,2)==1799&&model->numericValue(28,2)==1546,"actual external SUM results remain calculated");require(model->data(model->index(26,2),FormulaErrorCheckRole).toBool()&&model->data(model->index(28,2),FormulaErrorCheckRole).toBool()&&!model->data(model->index(27,2),FormulaErrorCheckRole).toBool(),"C27/C29 omitted-adjacent-number warnings remain visible while direct C28 remains unflagged");};warnings();
 require(model->apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",27},{"col",3},{"style",{{"bold",true}}}},{{"type","set_style"},{"sheet","sheet-1"},{"row",29},{"col",3},{"style",{{"numberFormat","#,##0"}}}}})),"style actual source SUM results");warnings();
 require(origin.executeCommand("undo"),"undo source SUM formatting");warnings();
 require(origin.executeCommand("redo"),"redo source SUM formatting");warnings();
 require(model->setData(model->index(29,2),"1",Qt::EditRole)&&!model->data(model->index(26,2),FormulaErrorCheckRole).toBool()&&!model->data(model->index(28,2),FormulaErrorCheckRole).toBool(),"a real subsequent cell edit still clears provisional formula warnings until calculation");
}
void changedExternalContextCannotCarryStyleCache(){
 QTemporaryDir files;MainWindow source,origin;auto* sourceModel=dynamic_cast<WorksheetModel*>(source.findChild<QTableView*>("worksheet.grid")->model());auto* model=dynamic_cast<WorksheetModel*>(origin.findChild<QTableView*>("worksheet.grid")->model());
 require(sourceModel->setData(sourceModel->index(0,0),"42",Qt::EditRole)&&source.saveWorkbook(files.filePath("Context Source.xlsx")),"prepare actual live context source");
 require(model->apply(Json::array({{{"type","set_metadata"},{"key","links"},{"value",Json::array({{{"name","Context Source.xlsx"},{"path",files.filePath("Context Source.xlsx").toStdString()},{"enabled",true}}})}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","='[Context Source.xlsx]Sheet1'!$A$1"}}})),"prepare actual external dependent formula");
 // Deliver only source-context notifications, never the 35ms calculation timer.
 QCoreApplication::sendPostedEvents(&origin,QEvent::MetaCall);origin.recalculateNow();require(model->numericValue(0,0)==42,"initial external result is genuinely current");
 require(sourceModel->setData(sourceModel->index(0,0),"50",Qt::EditRole),"change the live source input");QCoreApplication::sendPostedEvents(&origin,QEvent::MetaCall);
 require(model->apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",1},{"style",{{"bold",true}}}}})),"format the dependent formula before recalculation");
 require(model->calculatedSnapshot().at("status")=="stale"&&!model->numericValue(0,0).has_value()&&!model->copyRange(0,0,0,0).at("cells")[0][0].contains("value"),"style-only revision carry must not publish a prior external-context result as current");
 origin.recalculateNow();require(model->numericValue(0,0)==50,"real recalculation uses the changed live source");
 require(model->apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",1},{"style",{{"italic",true}}}}}))&&model->numericValue(0,0)==50,"a newly current result remains reusable across subsequent formatting");
}
void nativeLayoutRetainsCalculation(){
 MainWindow window;auto* view=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());
 require(model->setData(model->index(0,0),"42",Qt::EditRole)&&model->setData(model->index(0,1),"=A1*2",Qt::EditRole),"prepare native layout formula");window.recalculateNow();
 const auto before=window.currentSession().snapshot();view->setCurrentIndex(model->index(0,1));const int originalWidth=view->columnWidth(1);
 const auto current=[&]{require(model->data(model->index(0,1)).toString()=="84"&&model->numericValue(0,1)==84&&model->copyRange(0,1,0,1).at("cells")[0][0].at("value")==84,"presentation-only layout preserves immediate formula display, numeric value and clipboard cache");};
 require(window.executeCommand("columns.size",{{"size",22.0}}),"resize a native column");current();
 require(window.currentSession().lastChange().at("structure")==false&&window.currentSession().lastChange().at("calculationInputsChanged")==false,"width metadata is classified as presentation-only");
 require(view->columnWidth(1)!=originalWidth,"width-only edit updates the visible layout without changed cells");
 require(window.executeCommand("undo"),"undo column width");current();require(view->columnWidth(1)==originalWidth,"width Undo updates the visible layout");
 require(window.executeCommand("redo"),"redo column width");current();
 view->selectAll();require(formatWorksheetCells(&window,*model,*view,{{"style",{{"fontSize",18},{"fontFamily","Arial"}}}}),"format the whole native worksheet");current();
 require(model->cellStyle(90,9).at("fontSize")==18&&model->data(model->index(90,9),Qt::FontRole).value<QFont>().pointSize()==18,"default font refresh reaches unstored blank cells");
 require(window.executeCommand("undo"),"undo whole-sheet font");current();require(!model->cellStyle(90,9).contains("fontSize"),"font Undo refreshes the cached worksheet default style");
 require(window.executeCommand("redo"),"redo whole-sheet font");current();require(model->cellStyle(90,9).at("fontSize")==18,"font Redo refreshes blank defaults");
 require(model->raw(0,1)=="=A1*2"&&window.currentSession().snapshotView().at("revision")==before.at("revision").get<std::uint64_t>()+6,"layout edit Undo/Redo preserves formula and exactly one revision per transaction");
}
void layoutInvalidationBoundaries(){
 WorkbookSession session;WorksheetModel model(session);require(model.setData(model.index(0,0),"42",Qt::EditRole)&&model.setData(model.index(1,0),"8",Qt::EditRole)&&model.setData(model.index(2,0),"=SUBTOTAL(109,A1:A2)",Qt::EditRole),"prepare SUBTOTAL hidden-row control");
 const auto calculate=[&]{model.setCalculated(CalculationEngine().calculate(session.snapshotView()));};calculate();require(model.numericValue(2,0)==50,"initial subtotal is current");
 require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet","sheet-1"},{"rows",Json::array({{{"index",1},{"size",30.0}}})},{"showGridLines",false}}})),"change only row height and gridlines");require(model.numericValue(2,0)==50,"row dimensions and gridlines preserve calculated values");
 require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet","sheet-1"},{"columns",Json::array({{{"index",2},{"size",20.0}}})}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","50"}}})),"mix dimensions with a real input edit");require(session.lastChange().at("calculationInputsChanged")==true&&!model.numericValue(2,0).has_value(),"real input changes in layout batches invalidate formula caches");calculate();require(model.numericValue(2,0)==58,"changed subtotal recalculates");
 require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet","sheet-1"},{"rows",Json::array({{{"index",1},{"size",30.0},{"hidden",true}}})}}})),"hide a subtotal source row");require(session.lastChange().value("calculationInputsChanged",true)&&!model.numericValue(2,0).has_value(),"hidden rows retain conservative invalidation");calculate();require(model.numericValue(2,0)==8,"real hidden-row calculation changes SUBTOTAL");
 require(model.apply(Json::array({{{"type","set_sheet_layout"},{"sheet","sheet-1"},{"mergedRanges",Json::array({{{"row1",6},{"row2",6},{"col1",1},{"col2",2}}})}}}))&&!model.numericValue(2,0).has_value(),"merge metadata stays conservative");calculate();
 require(model.apply(Json::array({{{"type","set_metadata"},{"key","names"},{"value",{{"Rate","Sheet1!$A$1"}}}}}))&&!model.numericValue(2,0).has_value(),"names stay conservative after layout classification");
 WorkbookSession blank;WorksheetModel empty(blank);int notifications=0;QObject::connect(&empty,&QAbstractItemModel::dataChanged,&empty,[&](const QModelIndex& first,const QModelIndex& last){require(first.isValid()&&last.isValid()&&last.row()<empty.rowCount()&&last.column()<empty.columnCount(),"layout notifications use valid model bounds");++notifications;});
 require(empty.apply(Json::array({{{"type","set_sheet_layout"},{"sheet","sheet-1"},{"defaultStyle",{{"fontSize",20}}}}}))&&notifications==1&&empty.cellStyle(500,50).at("fontSize")==20,"default-only edit of an empty sheet notifies once and refreshes blank styles");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const std::string mode=argc>1?argv[1]:"all";if(mode=="all"||mode=="formats")formatsPasteRetainsCalculatedDates();if(mode=="all"||mode=="undo")nativeStyleUndoRetainsCalculation();if(mode=="all"||mode=="boundaries")typedValuesAndInvalidationBoundaries();if(mode=="all"||mode=="warnings")nativeExternalWarningContinuity();if(mode=="all"||mode=="context")changedExternalContextCannotCarryStyleCache();if(mode=="all"||mode=="layout")nativeLayoutRetainsCalculation();if(mode=="all"||mode=="layout_boundaries")layoutInvalidationBoundaries();std::cout<<"worksheet format-only calculation cache tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
