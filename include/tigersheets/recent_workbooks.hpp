#pragma once
#include <QObject>
#include <QString>
class QMenu;
namespace tigersheets {
class MainWindow;
class RecentWorkbooks final : public QObject {
public:
 RecentWorkbooks(MainWindow& window,QMenu& fileMenu);
 void remember(const QString& path);
private:
 void refresh();
 MainWindow& window_;
 QMenu* menu_;
 QString settingsPath_;
};
}
