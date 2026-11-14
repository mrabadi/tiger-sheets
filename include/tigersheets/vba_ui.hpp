#pragma once
#include "tigersheets/session.hpp"
#include <QObject>
#include <QByteArray>
#include <QString>
#include <functional>
#include <vector>
class QProcess;
class QWidget;
namespace tigersheets {
class WorksheetModel;
inline constexpr std::size_t MaxVbaMessageBytes=16U*1024U*1024U;
class MacroRecorder {
public:
 void start(const Json& snapshot);
 void observe(const Json& snapshot);
 void stop(){active_=false;}
 bool active() const{return active_;}
 std::size_t changeCount() const{return lines_.size();}
 std::string source(const std::string& procedure) const;
private:
 Json previous_;
 std::vector<std::string> lines_;
 bool active_=false;
};
class VbaWorker final:public QObject {
public:
 explicit VbaWorker(QObject* parent=nullptr,QString executable={});
 ~VbaWorker() override;
 bool start(const Json& snapshot,const std::string& source,const std::string& procedure,bool enabled);
 void stop();
 bool running() const{return busy_;}
 QString error() const{return error_;}
 std::function<void(const Json&)> completed;
private:
 QProcess* process_;
 QString executable_,error_;
 QByteArray output_;
 Json original_;
 bool busy_=false,cancelled_=false,oversized_=false;
 void finish(int exitCode);
};
bool vbaCommand(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QString command);
bool flushVbaDrafts(QWidget* parent,QString* error=nullptr);
}
