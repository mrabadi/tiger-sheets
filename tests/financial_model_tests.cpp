#include "tigersheets/main_window.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/csv.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/printing.hpp"
#include <QApplication>
#include <QDir>
#include <QTableView>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
using namespace tigersheets;
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir state,temporary;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 const QString directory=argc>1?QString::fromLocal8Bit(argv[1]):temporary.path();if(!QDir().mkpath(directory))throw std::runtime_error("Cannot create fixture directory");
 MainWindow window;window.loadFinancialExample();auto book=window.currentSession().snapshot();const auto result=CalculationEngine().calculate(book);bool found=false;for(const auto& value:result.at("values"))if(value["row"]==12&&value["col"]==4){found=true;if(!value["value"].is_number()||std::abs(value["value"].get<double>()-6301151.123557135)>0.00001)throw std::runtime_error("DCF differs from independent five-year cash-flow expectation");}if(!found)throw std::runtime_error("DCF result missing");
 const auto xlsx=directory+"/financial-model.xlsx";if(!window.saveWorkbook(xlsx))throw std::runtime_error(window.lastError().toStdString());const auto reopened=readWorkbook(xlsx.toStdString()).workbook;if(reopened.at("charts").size()!=1)throw std::runtime_error("Forecast native chart missing after reopen");
 auto* view=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());if(!printWorksheet(&window,*model,*view,directory+"/financial-model.pdf"))throw std::runtime_error("PDF was not exported");writeCsv((directory+"/financial-model.csv").toStdString(),csvValuesSnapshot(book,result,model->sheetId().toStdString()),model->sheetId().toStdString());
 std::cout<<"DCF 6301151.123557135; native XLSX/chart reopen, typed CSV and PDF exported\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
