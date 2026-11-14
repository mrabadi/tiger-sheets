#include "tigersheets/main_window.hpp"
#include "tigersheets/recovery.hpp"
#include "tigersheets/ooxml.hpp"
#include <QApplication>
#include <QAbstractButton>
#include <QDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir folder;qputenv("TIGERSHEETS_STATE_DIR",folder.path().toUtf8());tigersheets::MainWindow window;
 if(!window.saveWorkbook(folder.filePath("draft.xlsx"))||!window.executeCommand("vba.editor"))return 1;
 auto* source=window.findChild<QPlainTextEdit*>("vba.source");if(!source)return 1;source->setPlainText("Sub Draft()\nRange(\"A1\") = 91\nEnd Sub");
 QTimer::singleShot(0,&window,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::Cancel)->click();});
 if(window.close()){std::cerr<<"immediate parent close must see the pending VBA draft and offer save/discard/cancel\n";return 1;}
 const auto book=window.currentSession().snapshot();if(book.value("vba",tigersheets::Json::object()).value("modules",tigersheets::Json::array()).empty()||book["vba"]["modules"][0]["source"].get<std::string>().find("Draft")==std::string::npos){std::cerr<<"pending source must be flushed before closing\n";return 1;}
 source->setPlainText("Sub ReplacementDraft()\nRange(\"A1\") = 17\nEnd Sub");tigersheets::writeWorkbook(folder.filePath("replacement.xlsx").toStdString(),{{"revision",0},{"sheets",tigersheets::Json::array({{{"id","s"},{"name","Sheet1"},{"cells",tigersheets::Json::array()}}})}});if(!window.openWorkbook(folder.filePath("replacement.xlsx"))||!window.executeCommand("recovery.flush"))return 1;bool retained=false;tigersheets::RecoveryStore recovery(folder.filePath("recovery.sqlite").toStdString());for(const auto& record:recovery.records())retained|=record.at("workbook").dump().find("ReplacementDraft")!=std::string::npos;if(!retained){std::cerr<<"Direct workbook replacement must flush pending VBA source into recovery before destroying its editor\n";return 1;}
 std::cout<<"VBA draft workspace tests passed\n";
}
