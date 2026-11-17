#pragma once
class QObject;
class QLineEdit;
namespace tigersheets {
class MainWindow;
class WorksheetView;
bool finishExternalReferenceEntry(MainWindow& window,bool accept);
QObject* installExternalReferenceEntry(MainWindow& window,WorksheetView& view,QLineEdit& formulaBar);
}
