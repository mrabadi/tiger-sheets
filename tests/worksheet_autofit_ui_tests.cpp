#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include <QApplication>
#include <QHeaderView>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 auto book=WorkbookSession().snapshot();book["sheets"][0]["cells"]=Json::array({{{"row",2},{"col",3},{"input","Revenue Growth Rate / Month"}},{{"row",2},{"col",4},{"input","Keep adjacent column"}}});
 book["sheets"][0]["hiddenColumns"]=Json::array({2});
 WorkbookSession session(book);WorksheetModel model(session);WorksheetView view;view.horizontalHeader()->setDefaultSectionSize(72);view.setModel(&model);view.resize(1000,500);view.show();QApplication::processEvents();
 const auto before=session.snapshot();const int original=view.columnWidth(2),neighbor=view.columnWidth(3);auto* header=view.horizontalHeader();const QPoint boundary(header->sectionViewportPosition(2)+header->sectionSize(2)-1,header->viewport()->height()/2);
 QTest::mouseDClick(header->viewport(),Qt::LeftButton,Qt::NoModifier,boundary);QApplication::processEvents();const int fitted=view.columnWidth(2);
 require(fitted>original,"double-clicking a column header boundary fits its visible content");
 const auto& sheet=session.snapshotView().at("sheets")[0];const auto layout=sheet.value("columnLayout",Json::array());const auto entry=std::find_if(layout.begin(),layout.end(),[](const Json& value){return value.at("index")==3;});
 require(entry!=layout.end()&&entry->contains("size"),"header auto-fit persists the fitted column width in the workbook");
 require(view.isColumnHidden(1)&&sheet.at("hiddenColumns")==Json::array({2}),"header auto-fit preserves existing hidden columns stored without width entries");
 require(layout.size()==2&&view.columnWidth(3)==neighbor&&model.raw(1,2)=="Revenue Growth Rate / Month","header auto-fit preserves other columns and cell values");
 model.setCalculated({{"revision",session.snapshotView().at("revision")},{"values",Json::array()}});model.refresh();require(view.columnWidth(2)==fitted,"calculation and model refresh preserve the fitted width");
 QTemporaryDir directory;const auto path=directory.filePath("autofit.xlsx").toStdString();writeWorkbook(path,session.snapshotView());const auto reopened=readWorkbook(path);WorkbookSession saved(reopened.workbook);WorksheetModel savedModel(saved);WorksheetView savedView;savedView.horizontalHeader()->setDefaultSectionSize(72);savedView.setModel(&savedModel);
 require(savedView.columnWidth(2)==fitted&&savedView.columnWidth(3)==neighbor,"fitted width survives native workbook save and reopen");
 require(session.undo(),"header auto-fit is one undoable worksheet change");model.refresh();require(session.snapshotView().at("sheets")==before.at("sheets")&&view.columnWidth(2)==original,"one undo restores the original column layout and visible width");
 {
  auto multiBook=WorkbookSession().snapshot();multiBook["sheets"][0]["cells"]=Json::array({
   {{"row",2},{"col",1},{"input","A long label in the first selected column"}},
   {{"row",2},{"col",2},{"input","Keep this unselected column unchanged"}},
   {{"row",2},{"col",3},{"input","Short"}}
  });multiBook["sheets"][0]["hiddenColumns"]=Json::array({5});
  WorkbookSession multiSession(multiBook);WorksheetModel multiModel(multiSession);WorksheetView multiView;multiView.setModel(&multiModel);multiView.resize(1000,500);multiView.show();QApplication::processEvents();
  const int firstBefore=multiView.columnWidth(0),gapBefore=multiView.columnWidth(1),lastBefore=multiView.columnWidth(2);const auto multiBefore=multiSession.snapshot();
  QItemSelection selected;selected.select(multiModel.index(0,0),multiModel.index(multiModel.rowCount()-1,0));selected.select(multiModel.index(0,2),multiModel.index(multiModel.rowCount()-1,2));
  multiView.selectionModel()->setCurrentIndex(multiModel.index(0,2),QItemSelectionModel::NoUpdate);multiView.selectionModel()->select(selected,QItemSelectionModel::ClearAndSelect);
  auto* multiHeader=multiView.horizontalHeader();const QPoint multiBoundary(multiHeader->sectionViewportPosition(2)+multiHeader->sectionSize(2)-1,multiHeader->viewport()->height()/2);
  QTest::mouseDClick(multiHeader->viewport(),Qt::LeftButton,Qt::NoModifier,multiBoundary);QApplication::processEvents();
  require(multiView.columnWidth(0)>firstBefore,"double-click auto-fit sizes every selected column, including disjoint selections");
  require(multiView.columnWidth(2)<lastBefore&&multiView.columnWidth(0)>multiView.columnWidth(2),"each selected column fits its own content");
  require(multiView.columnWidth(1)==gapBefore&&multiView.isColumnHidden(4),"multi-column auto-fit preserves unselected gaps and hidden columns");
  require(multiView.selectionModel()->selection()==selected,"header auto-fit preserves the multi-column selection");
  require(multiSession.undo(),"multi-column header auto-fit creates an undoable change");multiModel.refresh();
  require(multiSession.snapshotView().at("sheets")==multiBefore.at("sheets")&&multiView.columnWidth(0)==firstBefore&&multiView.columnWidth(2)==lastBefore,"one undo restores every selected column together");
 }
 std::cout<<"Worksheet header auto-fit UI tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
