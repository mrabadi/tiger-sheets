#include "tigersheets/recent_workbooks.hpp"
#include "tigersheets/main_window.hpp"
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
namespace tigersheets {
RecentWorkbooks::RecentWorkbooks(MainWindow& window,QMenu& fileMenu):QObject(&window),window_(window),menu_(fileMenu.addMenu("Open Recent")),settingsPath_(QDir(qEnvironmentVariable("TIGERSHEETS_STATE_DIR",QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))).filePath("recent-workbooks.ini")){
 setObjectName("recent.workbooks");menu_->setObjectName("file.openRecent");
 connect(menu_,&QMenu::aboutToShow,this,[this]{refresh();});refresh();
}
void RecentWorkbooks::remember(const QString& path){
 const QFileInfo file(path);const auto canonical=file.canonicalFilePath();const auto normalized=canonical.isEmpty()?file.absoluteFilePath():canonical;
 if(!QDir().mkpath(QFileInfo(settingsPath_).absolutePath()))return;
 QSettings settings(settingsPath_,QSettings::IniFormat);settings.sync();auto paths=settings.value("files").toStringList();paths.removeAll(normalized);paths.prepend(normalized);while(paths.size()>10)paths.removeLast();settings.setValue("files",paths);settings.sync();
}
void RecentWorkbooks::refresh(){
 QSettings settings(settingsPath_,QSettings::IniFormat);settings.sync();const auto paths=settings.value("files").toStringList().mid(0,10);menu_->clear();
 for(int i=0;i<paths.size();++i){
  const auto path=paths[i];const QFileInfo file(path);auto label=file.fileName()+" — "+QDir::toNativeSeparators(file.absolutePath());label.replace("&","&&");
  auto* action=menu_->addAction(QString("&%1 %2").arg((i+1)%10).arg(label));action->setObjectName("file.recent."+QString::number(i));action->setData(path);action->setToolTip(path);action->setStatusTip(path);action->setEnabled(file.isFile()&&file.isReadable());
  connect(action,&QAction::triggered,this,[this,path]{if(!window_.executeCommand("open",{{"path",path.toStdString()}})&&!window_.lastError().isEmpty())QMessageBox::warning(&window_,"Cannot open workbook",window_.lastError());});
 }
 if(paths.isEmpty())menu_->addAction("No recent workbooks")->setEnabled(false);
}
}
