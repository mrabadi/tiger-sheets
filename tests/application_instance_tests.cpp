#include "tigersheets/application_instance.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLocalSocket>
#include <QLocalServer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void runClient(const QStringList& arguments){
 QProcess client;QEventLoop loop;QTimer timeout;timeout.setSingleShot(true);
 QObject::connect(&client,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&loop,&QEventLoop::quit);
 QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
 client.start(QCoreApplication::applicationFilePath(),arguments);timeout.start(6000);loop.exec();
 require(client.state()==QProcess::NotRunning&&client.exitStatus()==QProcess::NormalExit&&client.exitCode()==0,"a separate launch forwards its request and exits successfully");
}
struct ChildProcess {
 QProcess process;
 QString ownedListener;
 ~ChildProcess(){if(process.state()!=QProcess::NotRunning){process.terminate();if(!process.waitForFinished(1000)){process.kill();process.waitForFinished(1000);}}if(!ownedListener.isEmpty()){QLocalServer::removeServer(ownedListener);QFile::remove(QDir::tempPath()+"/"+ownedListener+".lock");}}
};
void separateDesktopLaunches(const QString& executable){
 QTemporaryDir directory;require(directory.isValid(),"isolated desktop launch directory");qputenv("TIGERSHEETS_STATE_DIR",directory.path().toUtf8());
 const auto source=directory.filePath("Separate Source.csv");QFile file(source);require(file.open(QIODevice::WriteOnly)&&file.write("42,84\n")==6,"prepare a local source workbook for a separate launch");file.close();
 ChildProcess primary;primary.ownedListener=ApplicationInstance::defaultServerName();primary.process.start(executable,{});require(primary.process.waitForStarted(3000),"first real desktop process starts");
 bool listening=false;QEventLoop startup;QTimer probe,deadline;probe.setInterval(20);deadline.setSingleShot(true);
 QObject::connect(&probe,&QTimer::timeout,&startup,[&]{QLocalSocket socket;socket.connectToServer(ApplicationInstance::defaultServerName());if(socket.waitForConnected(10)){listening=true;startup.quit();}});
 QObject::connect(&deadline,&QTimer::timeout,&startup,&QEventLoop::quit);QObject::connect(&primary.process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&startup,&QEventLoop::quit);
 probe.start();deadline.start(6000);startup.exec();probe.stop();
 require(listening,"the real desktop accepts separately launched workbooks in one application");
 for(const auto files:{QStringList{source},QStringList{}}){
  ChildProcess secondary;QEventLoop delivered;QTimer timeout;timeout.setSingleShot(true);QObject::connect(&secondary.process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&delivered,&QEventLoop::quit);QObject::connect(&timeout,&QTimer::timeout,&delivered,&QEventLoop::quit);
  secondary.process.start(executable,files);timeout.start(6000);delivered.exec();
  require(secondary.process.state()==QProcess::NotRunning&&secondary.process.exitStatus()==QProcess::NormalExit&&secondary.process.exitCode()==0,"separate file and blank launches join the running desktop instead of staying in an isolated process");
  require(primary.process.state()==QProcess::Running,"opening another workbook retains the original desktop process");
 }
}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);const auto args=app.arguments();
 if(args.size()>=3&&args[1]=="--client"){ApplicationInstance client(args[2]);return client.forward(args.mid(3))?0:2;}
 if(args.size()>=3&&args[1]=="--malformed"){
  QLocalSocket client;client.connectToServer(args[2]);if(!client.waitForConnected(2000))return 3;client.write("{bad json}\n");client.waitForBytesWritten(2000);client.waitForReadyRead(2000);return client.readAll().startsWith("OK")?4:0;
 }
 if(args.size()>=3&&args[1]=="--server"){
  ApplicationInstance server(args[2]);if(!server.listen())return 5;std::cout<<"READY\n"<<std::flush;return app.exec();
 }
 try{
  if(args.size()==3&&args[1]=="--launch"){separateDesktopLaunches(args[2]);std::cout<<"Separate desktop launch tests passed\n";return 0;}
  QTemporaryDir directory;require(directory.isValid(),"isolated instance test directory");const auto name=directory.path()+"/desktop";
  int delivered=0;QStringList received;ApplicationInstance server(name);server.openRequested=[&](const QStringList& files){++delivered;received=files;};
  if(!server.listen())throw std::runtime_error(("first desktop launch listens for separately opened workbooks: "+server.errorString()).toStdString());
  const QStringList files={directory.filePath("Source with spaces.xlsx"),directory.filePath("Δ 'budget'.xlsx")};
  runClient(QStringList{"--client",name}+files);require(delivered==1&&received==files,"separate launches preserve every absolute workbook path exactly once");
  ApplicationInstance competing(name);require(!competing.listen(),"a second launch cannot replace the active application listener");
  runClient({"--client",name});require(delivered==2&&received.isEmpty(),"a separate blank launch requests another workbook window");
  runClient({"--malformed",name});require(delivered==2,"malformed launch messages cannot open a workbook");
  runClient(QStringList{"--client",name}+files);require(delivered==3&&received==files,"the existing application still accepts launches after competing and malformed clients");
  const auto staleName=directory.path()+"/stale";QProcess abandoned;abandoned.start(QCoreApplication::applicationFilePath(),{"--server",staleName});require(abandoned.waitForStarted(2000)&&abandoned.waitForReadyRead(3000)&&abandoned.readAllStandardOutput().contains("READY"),"separate primary creates its listener before stale-server fixture");abandoned.kill();require(abandoned.waitForFinished(3000),"abandoned primary exits without cleanup");
  ApplicationInstance replacement(staleName);replacement.openRequested=[&](const QStringList& paths){++delivered;received=paths;};require(replacement.listen(),"a fresh launch safely recovers the socket and lock of a dead process");
  runClient(QStringList{"--client",staleName}+files);require(delivered==4&&received==files,"launch forwarding works after recovery of an abandoned listener");
  std::cout<<"Application instance tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
