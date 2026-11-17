#include "support/native_document_context.hpp"
#include "support/native_document_text_toolbar.hpp"
#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTest>
#include <QToolButton>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
QByteArray digest(const QString& path){QFile file(path);require(file.open(QIODevice::ReadOnly),"Open actual official case PDF");return QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256);}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 const auto path=QString(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-90-Minute-Case-Study.pdf";
 const auto source=digest(path);tigersheets::testing::NativeDocumentContext viewer;viewer.resize(1280,720);viewer.show();viewer.activateWindow();viewer.openPdf(path);QApplication::processEvents();
 auto* find=viewer.findChild<QLineEdit*>("context.find");find->setFocus();QTest::keyClicks(find,"Revenue Projections");QTest::keyClick(find,Qt::Key_Return);QApplication::processEvents();auto* text=viewer.findChild<QPlainTextEdit*>("context.text");text->setFocus();const auto selection=text->textCursor().selectedText();const auto observation=viewer.observation();auto* surface=viewer.findChild<QLabel*>("context.surface");const auto raster=surface->pixmap().toImage(),before=viewer.grab().toImage();auto* focus=QApplication::focusWidget();
 auto* word=tigersheets::testing::showDocumentTextToolbar(&viewer,QRect(331,216,536,64),"word");QApplication::processEvents();
 require(word&&word->isVisible(),"Observed Word floating formatting toolbar is a real visible native widget");
 require(word->geometry()==QRect(331,216,536,64)&&word->focusPolicy()==Qt::NoFocus&&word->testAttribute(Qt::WA_TransparentForMouseEvents),"Source bounds and no-focus read-only chrome contract are retained");
 const auto combos=word->findChildren<QComboBox*>();require(combos.size()==2&&combos[0]->currentText()=="Calibri (Body)"&&combos[1]->currentText()=="12","Source font and size controls are native combos with observed text");
 const auto buttons=word->findChildren<QToolButton*>();require(buttons.size()==16,"Source Word toolbar retains all sixteen observed command controls");for(auto* button:buttons)require(!button->icon().isNull()&&button->focusPolicy()==Qt::NoFocus&&word->rect().contains(button->geometry()),"Every observed Word control uses a visible graphical icon and fits its measured bounds");
 require(QApplication::focusWidget()==focus&&text->textCursor().selectedText()==selection&&viewer.observation()==observation&&surface->pixmap().toImage()==raster&&digest(path)==source,"Showing source chrome preserves real selected text, keyboard focus, page, source raster and PDF bytes");
 auto* faded=tigersheets::testing::showDocumentTextToolbar(&viewer,QRect(331,216,536,64),"word",0.3);auto* effect=qobject_cast<QGraphicsOpacityEffect*>(faded->graphicsEffect());require(effect&&effect->opacity()==0.3,"Observed fade uses actual native graphics opacity");
 tigersheets::testing::clearDocumentTextToolbar(&viewer);QApplication::processEvents();require(viewer.grab().toImage()==before,"Dismissing source toolbar restores exact actual viewer pixels");
 auto* acrobat=tigersheets::testing::showDocumentTextToolbar(&viewer,QRect(839,360,265,41),"acrobat");QApplication::processEvents();require(acrobat&&acrobat->geometry()==QRect(839,360,265,41),"Source Acrobat toolbar uses its independently measured265 by41 bounds");const auto icons=acrobat->findChildren<QToolButton*>();require(icons.size()==6,"Source Acrobat contextual toolbar has six observed graphical controls");for(auto* button:icons)require(!button->icon().isNull()&&button->text().isEmpty()&&acrobat->rect().contains(button->geometry()),"Source Acrobat icons are graphical and all fit visibly");require(acrobat->grab().toImage().pixelColor(1,20)==QColor("#252525"),"Source Acrobat toolbar background is visibly dark");
 tigersheets::testing::clearDocumentTextToolbar(&viewer);QApplication::processEvents();require(viewer.grab().toImage()==before&&digest(path)==source&&QApplication::focusWidget()==focus,"Both external toolbar lifecycles restore source pixels and preserve focused read-only document");std::cout<<"Native source document text toolbars passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
