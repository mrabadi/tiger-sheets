#pragma once
class QObject;
class QTabWidget;
class QTableView;
namespace tigersheets {
class MainWindow;
QObject* installReviewRibbon(MainWindow&,QTabWidget&,QTableView&);
}
