#pragma once
class QObject;
class QTabWidget;
namespace tigersheets {
class MainWindow;
class WorksheetModel;
class WorksheetView;
QObject* installTutorialShortcuts(MainWindow& window,WorksheetModel& model,WorksheetView& view,QTabWidget& ribbon);
void showWorksheetFontPalette(MainWindow& window,WorksheetModel& model,WorksheetView& view);
void showWorksheetFillPalette(MainWindow& window,WorksheetModel& model,WorksheetView& view);
}
