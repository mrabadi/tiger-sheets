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
 std::cout<<"Worksheet header auto-fit UI tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
