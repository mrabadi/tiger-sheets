#include "tigersheets/grid.hpp"
#include "tigersheets/calculation.hpp"
#include <QApplication>
#include <QColor>
#include <iostream>
#include <tuple>
#include <vector>
bool dateFormulaEntry(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);
 model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",3},{"col",3},{"input","45930"},{"style",{{"numberFormat","m/d/yyyy"}}}},{{"type","set_style"},{"sheet","sheet-1"},{"row",3},{"col",4},{"style",{{"bold",true}}}}}));
 const auto before=session.snapshot();if(!model.setData(model.index(2,3),"=EOMONTH(C3,1)",Qt::EditRole))return false;
 model.setCalculated(CalculationEngine().calculate(session.snapshotView()));
 if(model.data(model.index(2,3)).toString()!="10/31/2025"){std::cerr<<"FAILED: entering EOMONTH beside a date must display 10/31/2025 instead of serial "<<model.data(model.index(2,3)).toString().toStdString()<<'\n';return false;}
 if(model.raw(2,3)!="=EOMONTH(C3,1)"||model.numericValue(2,3).value_or(-1)!=45961||!model.cellStyle(2,3).value("bold",false)){std::cerr<<"FAILED: inferred date format preserves the formula, numeric result and unrelated formatting\n";return false;}
 if(!session.undo()||session.snapshotView().at("sheets")!=before.at("sheets")){std::cerr<<"FAILED: one undo must restore both the date formula and inferred format\n";return false;}
 return true;
}
bool dateFormulaBoundaries(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);int row=0;
 for(const auto& [input,format]:std::vector<std::pair<const char*,const char*>>{{"=EDATE(45930,1)","m/d/yyyy"},{"=date(2025,10,31)","m/d/yyyy"},{"=TODAY()","m/d/yyyy"},{"=NOW()","m/d/yyyy h:mm"}}){if(!model.setData(model.index(row,0),input,Qt::EditRole)||model.cellStyle(row,0).value("numberFormat",std::string())!=format){std::cerr<<"FAILED: complete date-producing call must infer its date/time format: "<<input<<'\n';return false;}++row;}
 for(const auto* input:{"=YEAR(DATE(2025,10,31))","=MONTH(DATE(2025,10,31))","=DAY(DATE(2025,10,31))","=DAYS(DATE(2025,10,31),DATE(2025,9,30))","=DATE(2025,10,31)-DATE(2025,9,30)","=EOMONTH(DATE(2025,9,30),1)-DATE(2025,9,30)","'=DATE(2025,10,31)"}){if(!model.setData(model.index(row,0),input,Qt::EditRole)||model.cellStyle(row,0).contains("numberFormat")){std::cerr<<"FAILED: scalar date parts, differences and escaped text must not infer a date: "<<input<<'\n';return false;}++row;}
 for(const auto* format:{"General","0.00","$#,##0","0%","@","dd-mmm-yyyy"}){if(!model.apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",row+1},{"col",1},{"style",{{"numberFormat",format}}}}}))||!model.setData(model.index(row,0),"=EOMONTH(DATE(2025,9,30),1)",Qt::EditRole)||model.cellStyle(row,0).at("numberFormat")!=format){std::cerr<<"FAILED: an explicit user number format must survive date formula entry: "<<format<<'\n';return false;}++row;}
 return true;
}
bool dateLiteralEntry(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);const auto before=session.snapshot();
 if(!model.setData(model.index(2,2),"9/30/2025",Qt::EditRole)||model.raw(2,2)!="45930"||model.numericValue(2,2).value_or(-1)!=45930||model.data(model.index(2,2)).toString()!="9/30/2025"){std::cerr<<"FAILED: unambiguous 9/30/2025 entry must store date serial45930 and display the entered date\n";return false;}
 if(!session.undo()||session.snapshotView().at("sheets")!=before.at("sheets")||!session.redo()){std::cerr<<"FAILED: typed date value and inferred format require one Undo/Redo transaction\n";return false;}model.refresh();
 if(!model.setData(model.index(2,3),"=EOMONTH(C3,1)",Qt::EditRole))return false;model.setCalculated(CalculationEngine().calculate(session.snapshotView()));if(model.data(model.index(2,3)).toString()!="10/31/2025"){std::cerr<<"FAILED: EOMONTH of a typed date must show the next month end\n";return false;}
 int row=4;for(const auto& [input,raw,display]:std::vector<std::tuple<const char*,const char*,const char*>>{{"2025-09-30","45930","2025-09-30"},{"30/9/2025","45930","30/9/2025"},{"1900-02-28","59","1900-02-28"},{"1900-03-01","61","1900-03-01"}}){if(!model.setData(model.index(row,0),input,Qt::EditRole)||model.raw(row,0)!=raw||model.data(model.index(row,0)).toString()!=display){std::cerr<<"FAILED: typed date normalization must preserve date order and Excel epoch: "<<input<<'\n';return false;}++row;}
 for(const auto* text:{"'9/30/2025","2025-02-30"}){if(!model.setData(model.index(row,0),text,Qt::EditRole))return false;model.setCalculated(CalculationEngine().calculate(session.snapshotView()));if(model.numericValue(row,0).has_value()||model.cellStyle(row,0).contains("numberFormat")){std::cerr<<"FAILED: escaped or invalid date entry must remain text: "<<text<<'\n';return false;}++row;}
 for(const auto& [format,display,numeric]:std::vector<std::tuple<const char*,const char*,bool>>{{"0.00","45930.00",true},{"@","9/30/2025",false}}){if(!model.apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",row+1},{"col",1},{"style",{{"numberFormat",format}}}}}))||!model.setData(model.index(row,0),"9/30/2025",Qt::EditRole))return false;model.setCalculated(CalculationEngine().calculate(session.snapshotView()));if(model.cellStyle(row,0).at("numberFormat")!=format||model.data(model.index(row,0)).toString()!=display||model.numericValue(row,0).has_value()!=numeric){std::cerr<<"FAILED: typed dates must respect explicit numeric and Text formats\n";return false;}++row;}
 WorkbookSession alternative;WorksheetModel date1904(alternative);date1904.apply(Json::array({{{"type","set_metadata"},{"key","settings"},{"value",{{"date1904",true}}}}}));if(!date1904.setData(date1904.index(0,0),"1904-01-01",Qt::EditRole)||date1904.raw(0,0)!="0"||date1904.data(date1904.index(0,0)).toString()!="1904-01-01"){std::cerr<<"FAILED: typed dates must use the selected 1904 date epoch\n";return false;}if(CalculationEngine().calculate(alternative.snapshotView()).value("status",std::string())!="unsupported_date_system"){std::cerr<<"FAILED: entry inference must not bypass unsupported 1904 calculation refusal\n";return false;}
 return true;
}
bool dateEditRole(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);const auto empty=session.snapshot();
 const std::vector<std::tuple<const char*,const char*,const char*>> cases={
  {"45930","m/d/yyyy","2025-09-30"},{"45659","m/d/yy","2025-01-02"},{"45930.0","dd-mmm-yy","2025-09-30"},
  {"59","yyyy-mm-dd","1900-02-28"},{"61","m/d/yyyy","1900-03-01"},
  {"45930.5","m/d/yyyy","45930.5"},{"45930.50000000001","m/d/yyyy h:mm","45930.50000000001"},
  {"60","m/d/yyyy","60"},{"45930","General","45930"},{"45930.0","0.00","45930.0"},{"45930","@","45930"},
  {"'45930","m/d/yyyy","'45930"},{"=EOMONTH(45930,1)","m/d/yyyy","=EOMONTH(45930,1)"}};
 Json operations=Json::array();for(std::size_t row=0;row<cases.size();++row)operations.push_back({{"type","set_cell"},{"sheet","sheet-1"},{"row",row+1},{"col",1},{"input",std::get<0>(cases[row])},{"style",{{"numberFormat",std::get<1>(cases[row])}}}});if(!model.apply(operations))return false;
 const auto original=session.snapshot();for(std::size_t row=0;row<cases.size();++row){const auto index=model.index(static_cast<int>(row),0);const auto edit=model.data(index,Qt::EditRole).toString();if(edit!=std::get<2>(cases[row])){std::cerr<<"FAILED: date EditRole must expose a lossless full-year date for whole literals and preserve formulas, fractions and nondate input: "<<std::get<0>(cases[row])<<'\n';return false;}
  if(!model.setData(index,edit,Qt::EditRole)||session.snapshot()!=original){std::cerr<<"FAILED: committing unchanged date edit text must preserve exact workbook input, revision and history\n";return false;}}
 if(!session.undo()||session.snapshotView().at("sheets")!=empty.at("sheets")||session.canUndo()){std::cerr<<"FAILED: unchanged F2 edits must not add an undo transaction\n";return false;}
 WorkbookSession alternative;WorksheetModel epoch(alternative);if(!epoch.apply(Json::array({{{"type","set_metadata"},{"key","settings"},{"value",{{"date1904",true}}}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","0"},{"style",{{"numberFormat","m/d/yyyy"}}}}})))return false;const auto epochBefore=alternative.snapshot();const auto edit=epoch.data(epoch.index(0,0),Qt::EditRole);if(edit.toString()!="1904-01-01"||!epoch.setData(epoch.index(0,0),edit,Qt::EditRole)||alternative.snapshot()!=epochBefore){std::cerr<<"FAILED: 1904 date EditRole must use its epoch and preserve no-op edits\n";return false;}
 return true;
}
bool dateEntryPatterns(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);int row=0;
 for(const auto& [input,serial,format]:std::vector<std::tuple<const char*,const char*,const char*>>{
  {"09/30/2025","45930","mm/dd/yyyy"},{"9-30-2025","45930","m-dd-yyyy"},{"09-30-25","45930","mm-dd-yy"},
  {"2025-9-3","45903","yyyy-m-d"},{"2025/09/03","45903","yyyy/mm/dd"},{"30-09-2025","45930","dd-mm-yyyy"}}){
  const auto index=model.index(row,0);if(!model.setData(index,input,Qt::EditRole)||model.raw(row,0)!=serial||model.cellStyle(row,0).value("numberFormat",std::string())!=format||model.data(index).toString()!=input){std::cerr<<"FAILED: initial date entry must preserve separator, order, padding and year width: "<<input<<'\n';return false;}
  const auto before=session.snapshot();if(!model.setData(index,model.data(index,Qt::EditRole),Qt::EditRole)||session.snapshot()!=before){std::cerr<<"FAILED: accepting full-year date edit text must retain the initial display pattern\n";return false;}++row;
 }
 return true;
}
bool dateEntryLocale(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);int row=0;
 for(const auto& [input,serial,edit]:std::vector<std::tuple<const char*,const char*,const char*>>{{"1/2/2025","45659","2025-01-02"},{"01/02/25","45659","2025-01-02"},{"01/01/29","47119","2029-01-01"},{"01/01/30","10959","1930-01-01"}}){const auto index=model.index(row,0);if(!model.setData(index,input,Qt::EditRole)||model.raw(row,0)!=serial||model.data(index).toString()!=input||model.data(index,Qt::EditRole).toString()!=edit){std::cerr<<"FAILED: numeric dates must use the app's English-US month order and two-digit year convention: "<<input<<'\n';return false;}++row;}
 return true;
}
bool dateEntryGeneral(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);if(!model.apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",1},{"style",{{"numberFormat","General"},{"bold",true}}}}})))return false;const auto before=session.snapshot();
 if(!model.setData(model.index(0,0),"09/30/25",Qt::EditRole)||model.raw(0,0)!="45930"||model.cellStyle(0,0).value("numberFormat",std::string())!="mm/dd/yy"||!model.cellStyle(0,0).value("bold",false)||model.data(model.index(0,0)).toString()!="09/30/25"){std::cerr<<"FAILED: typing a date into General must infer its entered pattern while preserving unrelated style\n";return false;}
 if(!session.undo()||session.snapshotView().at("sheets")!=before.at("sheets")){std::cerr<<"FAILED: one undo must restore General format and the previous date input together\n";return false;}
 return true;
}
bool textFormattedEntry(){
 using namespace tigersheets;
 for(const auto& [input,raw,display]:std::vector<std::tuple<const char*,const char*,const char*>>{{"00123","'00123","00123"},{"10%","'10%","10%"},{"=1+2","'=1+2","=1+2"},{"'00123","'00123","00123"},{"","",""}}){
  WorkbookSession session;WorksheetModel model(session);if(!model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","'previous"},{"style",{{"numberFormat","@"},{"bold",true}}}}})))return false;const auto before=session.snapshot();const auto style=model.cellStyle(0,0);
  if(!model.setData(model.index(0,0),input,Qt::EditRole)||model.raw(0,0)!=raw||model.cellStyle(0,0)!=style){std::cerr<<"FAILED: new Text-formatted entries must keep leading zeros, percentages and formula-looking text without changing style: "<<input<<'\n';return false;}
  model.setCalculated(CalculationEngine().calculate(session.snapshotView()));if(model.data(model.index(0,0)).toString()!=display||model.numericValue(0,0).has_value()){std::cerr<<"FAILED: Text-formatted entries must calculate and display as literal text: "<<input<<'\n';return false;}
  if(!session.undo()||session.snapshotView().at("sheets")!=before.at("sheets")){std::cerr<<"FAILED: one undo must restore the previous Text cell input and its style\n";return false;}
 }
 WorkbookSession session;WorksheetModel model(session);if(!model.setData(model.index(0,0),"123",Qt::EditRole)||!model.apply(Json::array({{{"type","set_style"},{"sheet","sheet-1"},{"row",1},{"col",1},{"style",{{"numberFormat","@"}}}}})))return false;model.setCalculated(CalculationEngine().calculate(session.snapshotView()));const auto before=session.snapshot();if(model.numericValue(0,0).value_or(-1)!=123||!model.setData(model.index(0,0),model.data(model.index(0,0),Qt::EditRole),Qt::EditRole)||session.snapshot()!=before||model.numericValue(0,0).value_or(-1)!=123){std::cerr<<"FAILED: formatting an existing number as Text or accepting an unchanged editor must preserve its numeric value\n";return false;}
 return true;
}
bool rejectedCalculationsClearOldDisplay(){
 using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);model.apply(Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","1"}},{{"type","set_cell"},{"sheet","sheet-1"},{"row",50},{"col",5},{"input","=A1+1"}}}));
 for(const auto& status:std::vector<std::string>{"not_converged","error","ok"}){const auto revision=session.snapshotView().at("revision");model.setCalculated({{"revision",revision},{"status","ok"},{"values",Json::array({{{"sheet","sheet-1"},{"row",50},{"col",5},{"type","number"},{"value",2}}})}});model.setData(model.index(0,0),QString::number(revision.get<int>()+1),Qt::EditRole);if(model.data(model.index(49,4)).toString()!="2"){std::cerr<<"FAILED: fast edit fixture retains prior dependent value until calculation result\n";return false;}bool repainted=false;const auto connection=QObject::connect(&model,&QAbstractItemModel::dataChanged,[&](QModelIndex first,QModelIndex last){if(first.row()<=49&&last.row()>=49&&first.column()<=4&&last.column()>=4)repainted=true;});model.setCalculated({{"revision",session.snapshotView().at("revision")},{"status",status},{"iteration",{{"converged",false}}},{"values",Json::array()}});QObject::disconnect(connection);if(model.data(model.index(49,4)).toString()!="=A1+1"||!repainted||model.calculatedSnapshot().at("status")!="stale"||model.copyRange(49,4,49,4).at("cells")[0][0].contains("value")){std::cerr<<"FAILED: failed or nonconverged calculation must clear and repaint previous dependent numeric values and reject cache export\n";return false;}}
 return true;
}
bool borrowedPreviewView(){using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);model.setData(model.index(0,0),"Live",Qt::EditRole);const auto live=session.snapshot();auto first=std::make_unique<Json>(live);(*first)["revision"]=live.at("revision").get<int>()+1;(*first)["sheets"][0]["cells"]=Json::array({{{"row",1},{"col",1},{"input","'00123"},{"style",{{"fill","#123456"},{"bold",true}}}},{{"row",1},{"col",2},{"input","=1+2"}}});(*first)["sheets"].push_back({{"id","other"},{"name","Other"},{"cells",Json::array({{{"row",1},{"col",1},{"input","'=lookup"}}})}});model.showPreviewView(*first);if(&model.workbookView()!=first.get()||model.data(model.index(0,0)).toString()!="00123"||model.data(model.index(0,0),Qt::BackgroundRole).value<QColor>()!=QColor("#123456")||(model.flags(model.index(0,0))&Qt::ItemIsEditable)||model.setData(model.index(0,0),"leak",Qt::EditRole)){std::cerr<<"FAILED: borrowed preview must read the exact caller-owned values and style without cloning or allowing edits\n";return false;}model.setCalculated({{"revision",first->at("revision")},{"status","ok"},{"values",Json::array({{{"sheet","sheet-1"},{"row",1},{"col",2},{"type","number"},{"value",3}}})}});const auto clip=model.copyRange(0,0,0,1);if(clip.at("cells")[0][0].at("input")!="'00123"||clip.at("cells")[0][1].at("value")!=3||clip.at("cells")[0][1].at("valueType")!="number"){std::cerr<<"FAILED: borrowed preview preserves typed copy values and literal source\n";return false;}model.setSheetId("other");if(model.data(model.index(0,0)).toString()!="=lookup"||(model.flags(model.index(0,0))&Qt::ItemIsEditable))return false;model.setSheetId("sheet-1");auto replacement=std::make_unique<Json>(*first);(*replacement)["sheets"][0]["cells"][0]["input"]="Replacement";model.showPreviewView(*replacement);first.reset();for(int step=0;step<3;++step){auto next=std::make_unique<Json>(*replacement);(*next)["sheets"][0]["cells"][0]["input"]="View"+std::to_string(step);model.showPreviewView(*next);replacement.reset();replacement=std::move(next);if(model.raw(0,0)!="View"+QString::number(step)||&model.workbookView()!=replacement.get())return false;}model.refresh();replacement.reset();if(model.data(model.index(0,0)).toString()!="Live"||!(model.flags(model.index(0,0))&Qt::ItemIsEditable)||session.snapshot()!=live){std::cerr<<"FAILED: clearing borrowed previews must restore live state before caller releases storage\n";return false;}return true;}
bool removedSheetFallsBackToLiveSheet(){using namespace tigersheets;WorkbookSession session;WorksheetModel model(session);model.setData(model.index(0,0),"Live",Qt::EditRole);auto preview=session.snapshot();preview["sheets"].push_back({{"id","converted"},{"name","Converted"},{"cells",Json::array({{{"row",1},{"col",1},{"input","Preview"}}})}});model.showPreviewView(preview);model.setSheetId("converted");if(model.raw(0,0)!="Preview")return false;model.refresh();if(model.sheetId()!="sheet-1"||model.raw(0,0)!="Live"){std::cerr<<"FAILED: discarded preview worksheet must fall back to an existing live worksheet\n";return false;}if(!session.apply(session.snapshotView().at("revision"),Json::array({{{"type","add_sheet"},{"id","added"},{"name","Added"}}})))return false;model.refresh();model.setSheetId("added");if(!session.undo())return false;model.refresh();if(model.sheetId()!="sheet-1"||model.raw(0,0)!="Live"){std::cerr<<"FAILED: undo removing the selected worksheet must fall back to an existing live worksheet\n";return false;}return true;}
int main(int argc,char**argv){
 QApplication app(argc,argv);
 if(argc>1&&std::string(argv[1])=="date_formula")return dateFormulaEntry()?0:1;
 if(argc>1&&std::string(argv[1])=="date_kinds")return dateFormulaBoundaries()?0:1;
 if(argc>1&&std::string(argv[1])=="date_literal")return dateLiteralEntry()?0:1;
 if(argc>1&&std::string(argv[1])=="date_edit")return dateEditRole()?0:1;
 if(argc>1&&std::string(argv[1])=="date_pattern")return dateEntryPatterns()?0:1;
 if(argc>1&&std::string(argv[1])=="date_locale")return dateEntryLocale()?0:1;
 if(argc>1&&std::string(argv[1])=="date_general")return dateEntryGeneral()?0:1;
 if(argc>1&&std::string(argv[1])=="text_entry")return textFormattedEntry()?0:1;
 if(!dateFormulaEntry()||!dateFormulaBoundaries()||!dateLiteralEntry()||!dateEditRole()||!dateEntryPatterns()||!dateEntryLocale()||!dateEntryGeneral()||!textFormattedEntry())return 1;
 if(!rejectedCalculationsClearOldDisplay()||!borrowedPreviewView()||!removedSheetFallsBackToLiveSheet())return 1;
 tigersheets::WorkbookSession session;
 tigersheets::WorksheetModel model(session);
 if(model.rowCount()!=1048576||model.columnCount()!=16384){std::cerr<<"FAILED: Excel worksheet dimensions\n";return 1;}
 if(!model.setData(model.index(0,0),"Revenue",Qt::EditRole)||model.data(model.index(0,0),Qt::DisplayRole)!="Revenue"){std::cerr<<"FAILED: native grid edits canonical workbook\n";return 1;}
 model.setData(model.index(0,1),"=100*1.25",Qt::EditRole);
 model.setCalculated({{"revision",session.snapshot()["revision"]},{"values",tigersheets::Json::array({{{"sheet",model.sheetId().toStdString()},{"row",1},{"col",2},{"type","number"},{"value",125.0}}})}});
 if(model.data(model.index(0,1),Qt::DisplayRole).toString()!="125"||model.data(model.index(0,1),Qt::EditRole)!="=100*1.25"){std::cerr<<"FAILED: calculated display is separate from formula source\n";return 1;}
 model.setData(model.index(0,1),"=200",Qt::EditRole);
 model.setCalculated({{"revision",0},{"values",tigersheets::Json::array({{{"sheet",model.sheetId().toStdString()},{"row",1},{"col",2},{"type","number"},{"value",999.0}}})}});
 if(model.data(model.index(0,1),Qt::DisplayRole).toString()=="999"){std::cerr<<"FAILED: stale calculation must be discarded\n";return 1;}
 auto clip=model.copyRange(0,0,0,1);
 if(!model.pasteRange(2,2,clip)||model.raw(2,3)!="=200"){std::cerr<<"FAILED: internal clipboard copies cells into canonical model\n";return 1;}
 model.setData(model.index(4,0),"1234.5",Qt::EditRole);
 model.apply(tigersheets::Json::array({{{"type","set_style"},{"sheet",model.sheetId().toStdString()},{"row",5},{"col",1},{"style",{{"numberFormat","$#,##0.00"},{"fill","#ffcc00"}}}}}));
 if(model.data(model.index(4,0),Qt::DisplayRole).toString()!="$1,234.50"||model.data(model.index(4,0),Qt::BackgroundRole).value<QColor>()!=QColor("#ffcc00")){std::cerr<<"FAILED: cell number format and fill render from semantic style\n";return 1;}
 model.setData(model.index(5,0),"45292",Qt::EditRole);model.apply(tigersheets::Json::array({{{"type","set_style"},{"sheet",model.sheetId().toStdString()},{"row",6},{"col",1},{"style",{{"numberFormat","m/d/yyyy"}}}}}));
 if(model.data(model.index(5,0)).toString()!="1/1/2024"){std::cerr<<"FAILED: Excel serial date displays as a date\n";return 1;}
 model.apply(tigersheets::Json::array({{{"type","set_cell"},{"sheet",model.sheetId().toStdString()},{"row",7},{"col",1},{"input","0"},{"style",{{"numberFormat","m/d/yyyy"}}}},{{"type","set_metadata"},{"key","settings"},{"value",{{"date1904",true}}}}}));
 if(model.data(model.index(6,0)).toString()!="1/1/1904"){std::cerr<<"FAILED: 1904 date-system serial zero displays January 1 1904\n";return 1;}
 model.apply(tigersheets::Json::array({{{"type","set_metadata"},{"key","settings"},{"value",{{"date1904",false}}}}}));
 for(const auto& [input,format,expected]:std::vector<std::tuple<const char*,const char*,const char*>>{{"1234567","#,##0,,","1"},{"2.5","0.0\"x\"","2.5x"},{"45292.5","m/d/yyyy h:mm","1/1/2024 12:00"}}){model.apply(tigersheets::Json::array({{{"type","set_cell"},{"sheet",model.sheetId().toStdString()},{"row",8},{"col",1},{"input",input},{"style",{{"numberFormat",format}}}}}));if(model.data(model.index(7,0)).toString()!=expected){std::cerr<<"FAILED: shared financial number formatter "<<format<<" expected "<<expected<<" got "<<model.data(model.index(7,0)).toString().toStdString()<<'\n';return 1;}}
 auto candidate=session.preview(session.snapshot()["revision"],tigersheets::Json::array({{{"type","set_cell"},{"sheet",model.sheetId().toStdString()},{"row",1},{"col",1},{"input","Preview"}}}));
 model.showPreview(candidate);
 if(model.data(model.index(0,0)).toString()!="Preview"||model.setData(model.index(0,0),"leak",Qt::EditRole)||session.snapshot()["sheets"][0]["cells"][0]["input"]=="Preview"){std::cerr<<"FAILED: isolated read-only preview renders without mutating live workbook\n";return 1;}
 tigersheets::WorkbookSession twoSheets;
 if(!twoSheets.apply(0,tigersheets::Json::array({{{"type","add_sheet"},{"id","second"},{"name","Second"}},{{"type","set_cell"},{"sheet","second"},{"row",1},{"col",1},{"input","Live second"}}}))){std::cerr<<"FAILED: two-sheet preview fixture\n";return 1;}
 tigersheets::WorksheetModel previewModel(twoSheets);
 const auto liveBeforePreview=twoSheets.snapshot();
 const auto twoSheetCandidate=twoSheets.preview(liveBeforePreview["revision"],tigersheets::Json::array({{{"type","set_cell"},{"sheet","sheet-1"},{"row",1},{"col",1},{"input","Preview first"}},{{"type","set_cell"},{"sheet","second"},{"row",1},{"col",1},{"input","Preview second"}}}));
 previewModel.showPreview(twoSheetCandidate);
 previewModel.setSheetId("second");
 const auto visibleSecond=previewModel.data(previewModel.index(0,0)).toString();
 const bool previewEditable=previewModel.flags(previewModel.index(0,0))&Qt::ItemIsEditable;
 const bool previewWrite=previewModel.setData(previewModel.index(0,0),"leaked edit",Qt::EditRole);
 if(visibleSecond!="Preview second"||previewEditable||previewWrite||twoSheets.snapshot()!=liveBeforePreview){std::cerr<<"FAILED: preview sheet switching retains candidate values and read-only isolation\n";return 1;}
 previewModel.setSheetId("sheet-1");
 if(previewModel.data(previewModel.index(0,0)).toString()!="Preview first"||(previewModel.flags(previewModel.index(0,0))&Qt::ItemIsEditable)){std::cerr<<"FAILED: return to first preview worksheet remains isolated\n";return 1;}
 twoSheets.discard();previewModel.refresh();previewModel.setSheetId("second");
 if(previewModel.data(previewModel.index(0,0)).toString()!="Live second"||!(previewModel.flags(previewModel.index(0,0))&Qt::ItemIsEditable)){std::cerr<<"FAILED: explicit refresh restores live editable workbook after discard\n";return 1;}

}
