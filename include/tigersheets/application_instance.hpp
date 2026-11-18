#pragma once
#include <QObject>
#include <QStringList>
#include <functional>
#include <memory>
namespace tigersheets {
// Workbook windows share one application so formula pointing and live links
// also work when files are opened in separate desktop launches.
class ApplicationInstance final:public QObject {
public:
 explicit ApplicationInstance(QString serverName={},QObject* parent=nullptr);
 ~ApplicationInstance() override;
 bool listen();
 bool forward(const QStringList& files,int timeoutMs=2000);
 QString errorString() const;
 std::function<void(const QStringList&)> openRequested;
 static QString defaultServerName();
private:
 struct State;
 std::unique_ptr<State> state_;
};
}
