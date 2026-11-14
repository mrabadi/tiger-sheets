#include "tigersheets/main_window.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QMessageBox>
int main(int argc,char**argv){
 QApplication app(argc,argv);QApplication::setOrganizationName("TigerSheets");QApplication::setApplicationName("TigerSheets");QApplication::setApplicationVersion("0.3.0~dev.4");QGuiApplication::setDesktopFileName("tiger-sheets");
 QCommandLineParser parser;parser.setApplicationDescription("TigerSheets — local financial modeling");parser.addHelpOption();parser.addVersionOption();parser.addOption({"demo","Open the financial model example"});parser.addOption({"screenshot","Save a native workspace screenshot and exit","path"});parser.addPositionalArgument("files","Local XLSX, XLSM or CSV workbooks","[files...]");parser.process(app);
 tigersheets::MainWindow window;if(parser.isSet("demo"))window.loadFinancialExample();auto files=parser.positionalArguments();if(!files.isEmpty()&&!window.openWorkbook(files[0]))QMessageBox::warning(&window,"Cannot open workbook",window.lastError());window.show();for(int i=1;i<files.size();++i){auto* other=new tigersheets::MainWindow;other->setAttribute(Qt::WA_DeleteOnClose);if(other->openWorkbook(files[i]))other->show();else other->deleteLater();}
 if(parser.isSet("screenshot"))QTimer::singleShot(0,&app,[&]{window.recalculateNow();app.processEvents();app.exit(window.grab().save(parser.value("screenshot"))?0:1);});
 return app.exec();
}
