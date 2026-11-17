#pragma once
class QObject;
class QTabWidget;
class QTableView;
namespace tigersheets {
class MainWindow;
class WorksheetModel;
QObject* installViewRibbon(MainWindow&,QTabWidget&,QTableView&,WorksheetModel&);
}
