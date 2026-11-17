#pragma once
class QObject;
namespace tigersheets {
class MainWindow;
class WorksheetModel;
class WorksheetView;
QObject* installWorksheetErrorIndicator(MainWindow& window,WorksheetModel& model,WorksheetView& view);
}
