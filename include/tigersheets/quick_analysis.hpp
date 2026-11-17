#pragma once
class QObject;
namespace tigersheets {
class MainWindow;
class WorksheetModel;
class WorksheetView;
QObject* installQuickAnalysis(MainWindow&,WorksheetModel&,WorksheetView&);
}
