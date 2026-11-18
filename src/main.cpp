#include "tigersheets/main_window.hpp"
#include "tigersheets/application_instance.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include <QMessageBox>
int main(int argc,char**argv){
 QApplication app(argc,argv);QApplication::setOrganizationName("TigerSheets");QApplication::setApplicationName("TigerSheets");QApplication::setApplicationVersion("0.3.0~dev.4");QGuiApplication::setDesktopFileName("tiger-sheets");
 QCommandLineParser parser;parser.setApplicationDescription("TigerSheets — local financial modeling");parser.addHelpOption();parser.addVersionOption();parser.addOption({"demo","Open the financial model example"});parser.addOption({"screenshot","Save a native workspace screenshot and exit","path"});parser.addPositionalArgument("files","Local XLSX, XLSM or CSV workbooks","[files...]");parser.process(app);
 auto files=parser.positionalArguments();for(auto& path:files)path=QFileInfo(path).absoluteFilePath();
 tigersheets::ApplicationInstance instance;
 instance.openRequested=[](const QStringList& requested){
  const auto openWindow=[](const QString& path){
   if(!path.isEmpty())for(auto* widget:QApplication::topLevelWidgets())if(auto* existing=dynamic_cast<tigersheets::MainWindow*>(widget);existing&&existing->isVisible()&&QFileInfo(existing->property("workbook.path").toString()).absoluteFilePath()==path){
    if(existing->isMinimized())existing->showNormal();existing->raise();existing->activateWindow();return;
   }
   auto* other=new tigersheets::MainWindow;other->setAttribute(Qt::WA_DeleteOnClose);
   if(!path.isEmpty()&&!other->openWorkbook(path)){QMessageBox::warning(other,"Cannot open workbook",other->lastError());other->deleteLater();return;}other->show();other->raise();other->activateWindow();
  };
  if(requested.isEmpty())openWindow({});else for(const auto& path:requested)openWindow(path);
 };
 // Automated screenshots and the example remain isolated CLI operations.
 if(!parser.isSet("demo")&&!parser.isSet("screenshot")&&!instance.listen()){
  if(instance.forward(files))return 0;
  QMessageBox::warning(nullptr,"Cannot open workbook",instance.errorString());return 1;
 }
 tigersheets::MainWindow window;if(parser.isSet("demo"))window.loadFinancialExample();if(!files.isEmpty()&&!window.openWorkbook(files[0]))QMessageBox::warning(&window,"Cannot open workbook",window.lastError());window.show();for(int i=1;i<files.size();++i){auto* other=new tigersheets::MainWindow;other->setAttribute(Qt::WA_DeleteOnClose);if(other->openWorkbook(files[i]))other->show();else other->deleteLater();}
 if(parser.isSet("screenshot"))QTimer::singleShot(0,&app,[&]{window.recalculateNow();app.processEvents();app.exit(window.grab().save(parser.value("screenshot"))?0:1);});
 return app.exec();
}
