#pragma once
class QWidget;
class QTabBar;
class QTableView;
namespace tigersheets {
class MainWindow;
QWidget* createWorksheetSheetBar(MainWindow& window,QTableView& grid,QTabBar& tabs);
}
