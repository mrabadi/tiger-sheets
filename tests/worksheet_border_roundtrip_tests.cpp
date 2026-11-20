#include "tigersheets/biff.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/worksheet_cell_format.hpp"
#include <QApplication>
#include <QColor>
#include <QTableView>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
Json line(const char* style,const char* color){return {{"style",style},{"color",color}};}
Json border(const WorksheetModel& model,int row,int column,const char* side){
 const auto edges=model.cellStyle(row,column).value("border",Json::object());
 return edges.contains(side)&&edges.at(side).value("style",std::string())!="none"?edges.at(side):Json();
}
void boundary(const WorksheetModel& model,int row,int column,const char* side,int neighborRow,int neighborColumn,const char* opposite,const Json& expected){
 const auto first=border(model,row,column,side),second=border(model,neighborRow,neighborColumn,opposite);
 if(expected.is_null())require(first.is_null()&&second.is_null(),"clearing either side removes both representations of the shared boundary");
 else require((first==expected||second==expected)&&(first.is_null()||first==expected)&&(second.is_null()||second==expected),"the shared boundary has the new line without a conflicting opposite edge");
}
void select(QTableView& view,WorksheetModel& model,int row,int column){
 const auto index=model.index(row,column);view.setCurrentIndex(index);view.selectionModel()->select(index,QItemSelectionModel::ClearAndSelect);
}
void apply(QTableView& view,WorksheetModel& model,int row,int column,const char* side,const Json& edge){
 select(view,model,row,column);
 require(formatWorksheetCells(nullptr,model,view,{{"style",{{"border",{{side,edge}}}}}}),"the public formatter accepts the selected native border edit");
}
Json fixture(bool legacy){
 auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"]=Json::array({
  {{"row",1},{"col",8},{"input",legacy?"'=B2*2":"=B2*2"},{"style",{{"numberFormat","0.00"},{"italic",true}}}},
  {{"row",2},{"col",2},{"input","12"},{"style",{{"bold",true},{"fill","#ffff00"},{"border",{{"bottom",line("medium","#ff0000")},{"diagonalUp",line("thin","#008000")}}}}}},
  {{"row",2},{"col",6},{"input",""},{"style",{{"fill","#00ffff"},{"border",{{"bottom",line("double","#0000ff")}}}}}},
  {{"row",3},{"col",2},{"input","24.5"},{"style",{{"numberFormat","0.00"},{"italic",true},{"border",{{"top",line("thick","#0000ff")}}}}}},
  {{"row",4},{"col",3},{"input","Label"},{"style",{{"underline",true},{"border",{{"right",line("dotted","#ff0000")}}}}}},
  {{"row",4},{"col",4},{"input","7"},{"style",{{"color","#008000"},{"border",{{"left",line("double","#0000ff")}}}}}},
  {{"row",5},{"col",5},{"input","Unrelated"},{"style",{{"bold",true},{"border",{{"top",line("double","#008000")},{"diagonalDown",line("thin","#ff0000")}}}}}}
 });return book;
}
void preservedCells(const WorksheetModel& before,const WorksheetModel& after){
 const auto& sheet=before.workbookView().at("sheets")[0];
 for(const auto& cell:sheet.at("cells")){
  const int row=cell.at("row").get<int>()-1,column=cell.at("col").get<int>()-1;
  require(after.raw(row,column)==before.raw(row,column),"border edits and native persistence preserve every original input, including formula text");
  const auto original=before.cellStyle(row,column),current=after.cellStyle(row,column);
  for(const auto* key:{"fontFamily","fontSize","bold","italic","underline","fill","color","numberFormat","alignment","verticalAlignment","wrap","indent"})if(original.contains(key)){
   require(current.contains(key),"native persistence retains each unrelated style property");
   if(std::string_view(key)=="color"||std::string_view(key)=="fill")require(QColor(QString::fromStdString(original.at(key).get<std::string>()))==QColor(QString::fromStdString(current.at(key).get<std::string>())),"native persistence preserves unrelated RGB styles");
   else require(current.at(key)==original.at(key),"native persistence preserves unrelated font, alignment and numeric styles");
  }
  for(const auto* diagonal:{"diagonalUp","diagonalDown"}){
   const auto a=border(before,row,column,diagonal),b=border(after,row,column,diagonal);
   require(a.is_null()==b.is_null(),"shared orthogonal edits preserve independent diagonal directions");
   if(!a.is_null())require(a.at("style")==b.at("style")&&QColor(QString::fromStdString(a.at("color")))==QColor(QString::fromStdString(b.at("color"))),"native persistence preserves independent diagonal line styles and colors");
  }
 }
}
void roundtrip(bool legacy){
 QTemporaryDir files;require(files.isValid(),"isolated native roundtrip directory");
 const auto input=files.filePath(legacy?"imported.xls":"imported.xlsx").toStdString();const auto output=files.filePath(legacy?"edited.xls":"edited.xlsx").toStdString();
 const auto authored=fixture(legacy);if(legacy)writeBiffWorkbook(input,authored);else writeWorkbook(input,authored);
 auto package=readWorkbook(input);WorkbookSession session(package.workbook),originalSession(package.workbook);WorksheetModel model(session),original(originalSession);QTableView view;view.setModel(&model);
 const auto originalSheets=session.snapshotView().at("sheets");const auto fresh=line("thin","#112233");
 apply(view,model,2,1,"top",fresh);boundary(model,2,1,"top",1,1,"bottom",fresh);
 require(session.undo(),"a border command and its opposite neighbor change are one Undo");model.refresh();
 require(session.snapshotView().at("sheets")==originalSheets&&!session.canUndo(),"one Undo restores the exact imported one-sided and conflicting styles");
 apply(view,model,2,1,"top",fresh);apply(view,model,1,1,"bottom",nullptr);boundary(model,2,1,"top",1,1,"bottom",nullptr);
 apply(view,model,3,3,"left",fresh);boundary(model,3,3,"left",3,2,"right",fresh);
 apply(view,model,3,2,"right",nullptr);boundary(model,3,3,"left",3,2,"right",nullptr);
 apply(view,model,2,5,"top",nullptr);boundary(model,2,5,"top",1,5,"bottom",nullptr);
 const auto unchanged=session.snapshot();const auto history=session.historyBytes();apply(view,model,2,5,"top",nullptr);
 require(session.snapshot()==unchanged&&session.historyBytes()==history,"clearing an already empty shared boundary is a no-op");
 preservedCells(original,model);require(border(model,4,4,"top")==border(original,4,4,"top"),"unrelated orthogonal borders remain unchanged");
 if(legacy)writeBiffWorkbook(output,session.snapshotView(),&package);else writeWorkbook(output,session.snapshotView(),&package);
 auto reopened=readWorkbook(output);WorkbookSession savedSession(reopened.workbook);WorksheetModel saved(savedSession);preservedCells(model,saved);
 boundary(saved,2,1,"top",1,1,"bottom",nullptr);boundary(saved,3,3,"left",3,2,"right",nullptr);boundary(saved,2,5,"top",1,5,"bottom",nullptr);
 const auto prior=border(model,4,4,"top"),restored=border(saved,4,4,"top");require(prior.at("style")==restored.at("style")&&QColor(QString::fromStdString(prior.at("color")))==QColor(QString::fromStdString(restored.at("color"))),"save and reopen preserve an untouched orthogonal border");
 require(saved.raw(0,7)==(legacy?"'=B2*2":"=B2*2"),"XLSX preserves the real formula and XLS preserves the supported literal input");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{roundtrip(false);roundtrip(true);std::cout<<"Native shared-border XLSX/XLS roundtrips passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
