#include "tigersheets/application_instance.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
namespace tigersheets {
namespace {
constexpr int MaxRequestBytes=1024*1024;
constexpr int MaxFiles=1024;
}
struct ApplicationInstance::State {
 QString name,error;
 QLocalServer server;
 QLockFile lock;
 explicit State(QString endpoint):name(std::move(endpoint)),lock(name.startsWith('/')?name+".lock":QDir::tempPath()+"/"+name+".lock"){lock.setStaleLockTime(0);}
 ~State(){server.close();lock.unlock();}
};
QString ApplicationInstance::defaultServerName(){
 const auto identity=QDir::homePath()+"\n"+QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+"\n"+qEnvironmentVariable("DISPLAY")+"\n"+qEnvironmentVariable("WAYLAND_DISPLAY")+"\n"+qEnvironmentVariable("QT_QPA_PLATFORM")+"\n"+qEnvironmentVariable("TIGERSHEETS_STATE_DIR");
 return "tiger-sheets-"+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
}
ApplicationInstance::ApplicationInstance(QString name,QObject* parent):QObject(parent),state_(std::make_unique<State>(name.isEmpty()?defaultServerName():std::move(name))){
 connect(&state_->server,&QLocalServer::newConnection,this,[this]{
  while(auto* socket=state_->server.nextPendingConnection()){
   socket->setReadBufferSize(MaxRequestBytes+1);auto bytes=std::make_shared<QByteArray>();auto handled=std::make_shared<bool>(false);
   connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
   auto receive=[this,socket,bytes,handled]{
    if(*handled)return;bytes->append(socket->readAll());
    if(bytes->size()>MaxRequestBytes){*handled=true;socket->abort();return;}
    const auto end=bytes->indexOf('\n');if(end<0)return;*handled=true;
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(bytes->left(end),&parse);const auto request=document.object();const auto paths=request.value("files");
    if(parse.error!=QJsonParseError::NoError||!document.isObject()||request.value("version").toInt()!=1||!paths.isArray()||paths.toArray().size()>MaxFiles){socket->abort();return;}
    QStringList files;for(const auto& path:paths.toArray()){if(!path.isString()||!QFileInfo(path.toString()).isAbsolute()){socket->abort();return;}files.push_back(path.toString());}
    socket->write("OK\n");socket->flush();socket->disconnectFromServer();
    QTimer::singleShot(0,this,[this,files]{if(openRequested)openRequested(files);});
   };
   connect(socket,&QLocalSocket::readyRead,this,receive);receive();
  }
 });
}
ApplicationInstance::~ApplicationInstance()=default;
bool ApplicationInstance::listen(){
 if(state_->server.isListening())return true;
 if(!state_->lock.tryLock()){state_->error="Another TigerSheets application owns this desktop session.";return false;}
 // Only the lock owner may remove a stale socket left by an exited process.
 QLocalServer::removeServer(state_->name);state_->server.setSocketOptions(QLocalServer::UserAccessOption);
 if(!state_->server.listen(state_->name)){state_->error=state_->server.errorString();state_->lock.unlock();return false;}
 state_->error.clear();return true;
}
bool ApplicationInstance::forward(const QStringList& files,int timeoutMs){
 QJsonArray paths;for(const auto& path:files)paths.push_back(path);
 const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"files",paths}}).toJson(QJsonDocument::Compact)+'\n';
 if(files.size()>MaxFiles||bytes.size()>MaxRequestBytes){state_->error="Too many workbooks in one launch request.";return false;}
 QLocalSocket socket;QElapsedTimer elapsed;elapsed.start();auto remaining=[&]{return std::max(0,timeoutMs-static_cast<int>(elapsed.elapsed()));};
 socket.connectToServer(state_->name);
 if(!socket.waitForConnected(remaining())){state_->error=socket.errorString();return false;}
 socket.write(bytes);if(socket.bytesToWrite()>0&&!socket.waitForBytesWritten(remaining())){state_->error=socket.errorString();return false;}
 while(!socket.canReadLine()){if(remaining()==0||!socket.waitForReadyRead(remaining())){state_->error="The running TigerSheets application did not acknowledge the workbook request.";return false;}}
 if(socket.readLine()!="OK\n"){state_->error="The running TigerSheets application rejected the workbook request.";return false;}
 state_->error.clear();return true;
}
QString ApplicationInstance::errorString()const{return state_->error;}
}
