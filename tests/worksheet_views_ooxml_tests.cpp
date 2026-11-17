#include "tigersheets/worksheet_views_ooxml.hpp"
#include <pugixml.hpp>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 WorkbookPackage package;package.workbook=WorkbookSession().snapshot();auto& sheet=package.workbook["sheets"][0];const auto id=sheet.at("id").get<std::string>();package.workbook["activeSheetIndex"]=0;sheet["nativeView"]={{"activeCell","B2"},{"selection","B2:E4"},{"topLeftCell","C8"},{"zoomScale",125}};package.sheetParts[id]="xl/worksheets/sheet1.xml";package.entries["xl/workbook.xml"]="<m:workbook xmlns:m='http://schemas.openxmlformats.org/spreadsheetml/2006/main'><m:workbookPr/><m:sheets/></m:workbook>";package.entries["xl/worksheets/sheet1.xml"]="<m:worksheet xmlns:m='http://schemas.openxmlformats.org/spreadsheetml/2006/main'><m:sheetPr><m:pageSetUpPr fitToPage='1'/></m:sheetPr><m:sheetData/></m:worksheet>";
 writeNativeWorkbookViews(package.entries,package.workbook,package.sheetParts);const auto& xml=package.entries.at("xl/worksheets/sheet1.xml");require(xml.find("m:sheetPr")<xml.find("m:sheetViews")&&xml.find("m:sheetViews")<xml.find("m:sheetData"),"a new native sheetViews follows sheetPr and precedes sheetData in SpreadsheetML schema order");const auto& book=package.entries.at("xl/workbook.xml");require(book.find("m:workbookPr")<book.find("m:bookViews")&&book.find("m:bookViews")<book.find("m:sheets"),"native bookViews follows workbookPr and precedes sheets");package.workbook.erase("activeSheetIndex");sheet.erase("nativeView");readNativeWorkbookViews(package);require(package.workbook.at("activeSheetIndex")==0&&sheet.at("nativeView")==Json({{"activeCell","B2"},{"selection","B2:E4"},{"topLeftCell","C8"},{"zoomScale",125}}),"native selected range, active cell, visible top-left cell and zoom roundtrip through namespaced XML without private metadata");
 std::cout<<"native worksheet view XML tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
