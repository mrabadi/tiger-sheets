#pragma once
class QObject;
class QLineEdit;
namespace tigersheets {
class MainWindow;
class WorksheetView;
QObject* installFormulaBarControls(MainWindow& window,WorksheetView& view,QLineEdit& name,QLineEdit& formula);
}
