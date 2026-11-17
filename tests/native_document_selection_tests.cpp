#include "support/native_document_selection.hpp"
#include "support/native_document_context.hpp"
#include <QApplication>
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QImage>
#include <QSpinBox>
#include <QToolButton>
#include <QTest>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
QByteArray digest(const QString& path){QFile file(path);require(file.open(QIODevice::ReadOnly),"Open the actual supplied source PDF");return QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256);}
void actualPdfEscape(){
 const auto path=QString(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-Investor-Presentation.pdf";
 const auto beforeFile=digest(path);tigersheets::testing::NativeDocumentContext viewer;viewer.resize(1280,720);viewer.show();viewer.activateWindow();viewer.openPdf(path);QApplication::processEvents();
 auto* page=viewer.findChild<QSpinBox*>("context.page");page->setFocus();QTest::keyClick(page,Qt::Key_A,Qt::ControlModifier);QTest::keyRelease(page,Qt::Key_Control);QTest::keyClicks(page,"41");QTest::keyClick(page,Qt::Key_Return);QApplication::processEvents();
 require(viewer.observation().at("page")==41,"Actual native PDF controls reach the observed capital-allocation page41");
 auto* surface=viewer.findChild<QLabel*>("context.surface");const auto raster=surface->pixmap().toImage();require(raster.size()==QSize(1280,720),"Use the actual supplied investor page at96dpi");
 // Source80:34 image bounds mapped from the retained source affine transform
 // into actual PDF96dpi coordinates; the visible pie image extends below the
 // native viewport, while its click center and toolbar remain visible.
 const QRect image(730,174,510,464),toolbar(793,130,242,43);tigersheets::testing::selectDocumentImage(surface,image,toolbar);QApplication::processEvents();
 auto* selection=surface->findChild<QWidget*>("replay.documentImageSelection");require(selection&&selection->isVisible(),"Native click selects the actual investor PDF pie image");require(surface->grab().toImage().pixelColor(image.center())!=raster.pixelColor(image.center()),"Selection visibly grays the original PDF pie pixels");require(surface->pixmap().toImage()==raster&&digest(path)==beforeFile,"Selection leaves the real source PDF and its original raster untouched");require(surface->grab().toImage().pixelColor(toolbar.topLeft()+QPoint(1,1))==QColor("#252525"),"The actual source floating image toolbar paints a solid dark background between its graphical controls");
 auto* focused=QApplication::focusWidget();require(focused,"Native PDF selection retains a keyboard focus target");QTest::keyClick(focused,Qt::Key_Escape);QApplication::processEvents();
 require(surface->grab().toImage()==raster,"Real Escape routed through the focused native PDF control clears the gray selection and floating toolbar");
 require(viewer.observation().at("page")==41&&viewer.observation().at("readOnly")==true&&digest(path)==beforeFile,"Native deselection keeps the actual page and file read-only");
 tigersheets::testing::clearDocumentSelection(surface);
 auto* zoom=viewer.findChild<QDoubleSpinBox*>("context.zoom");zoom->setFocus();QTest::keyClick(zoom,Qt::Key_A,Qt::ControlModifier);QTest::keyRelease(zoom,Qt::Key_Control);QTest::keyClicks(zoom,"80.6");QTest::keyClick(zoom,Qt::Key_Return);QApplication::processEvents();
 require(viewer.observation().at("zoomPercent")==80.6,"Real native zoom control reaches the observed source80.6percent");
 const auto zoomRaster=surface->pixmap().toImage();require(zoomRaster.size()==QSize(1032,581),"The actual PDF renders at the source adapter80.6percent zoom");
 const QRect zoomImage(588,140,411,374),zoomToolbar(639,105,195,35);tigersheets::testing::selectDocumentImage(surface,zoomImage,zoomToolbar);
 auto* floating=surface->findChild<QWidget*>("replay.documentImageToolbar");require(floating&&floating->isVisible(),"Source-zoom PDF image selection opens the floating graphical toolbar");
 const auto actions=floating->findChildren<QToolButton*>();require(actions.size()==5,"Source-zoom toolbar retains all five image actions");
 for(auto* action:actions)require(action->isVisible()&&floating->rect().contains(action->geometry())&&!action->icon().isNull(),"All five actual source-zoom graphical controls fit visibly inside the floating toolbar");
 QTest::keyClick(QApplication::focusWidget(),Qt::Key_Escape);QApplication::processEvents();require(surface->grab().toImage()==zoomRaster&&digest(path)==beforeFile,"Source-zoom keyboard deselection also restores the exact read-only PDF raster");
 tigersheets::testing::clearDocumentSelection(surface);
}
void actualCaseArtwork(){
 const auto path=QString(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-90-Minute-Case-Study.pdf";const auto file=digest(path);tigersheets::testing::NativeDocumentContext viewer;viewer.resize(1280,720);viewer.show();viewer.activateWindow();viewer.openPdf(path);QApplication::processEvents();auto* find=viewer.findChild<QLineEdit*>("context.find");find->setFocus();QTest::keyClicks(find,"Revenue Projections");QTest::keyClick(find,Qt::Key_Return);auto* text=viewer.findChild<QPlainTextEdit*>("context.text");text->setFocus();const auto selected=text->textCursor().selectedText();auto* focus=QApplication::focusWidget();auto* surface=viewer.findChild<QLabel*>("context.surface");const auto pixels=surface->pixmap().toImage();
 // Independently source-reviewed gray artwork rectangle, inverse-mapped from
 // source361.5 by354 actual original PDF/video feature correspondences.
 const QRect artwork(91,152,362,199);tigersheets::testing::selectDocumentArtwork(surface,artwork);QApplication::processEvents();
 auto* overlay=surface->findChild<QWidget*>("replay.documentImageSelection");require(overlay&&overlay->isVisible()&&surface->grab().toImage().pixelColor(artwork.center())!=pixels.pixelColor(artwork.center()),"Actual case-study heading and both pie charts visibly receive the observed Word gray artwork selection");require(!surface->findChild<QWidget*>("replay.documentImageToolbar"),"Word artwork selection does not introduce an unobserved Acrobat image toolbar");require(QApplication::focusWidget()==focus,"Selecting original artwork preserves the focused native document control");require(text->textCursor().selectedText()==selected,"Selecting original artwork preserves literal selected text");require(surface->pixmap().toImage()==pixels&&digest(path)==file,"Selecting original artwork preserves source raster and PDF bytes");tigersheets::testing::clearDocumentSelection(surface);QApplication::processEvents();require(surface->grab().toImage()==pixels&&QApplication::focusWidget()==focus,"Clearing Word artwork restores original source pixels without focus or file changes");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 QLabel surface;surface.resize(640,500);QPixmap original(640,500);original.fill(QColor("#C4A052"));surface.setPixmap(original);surface.show();surface.activateWindow();surface.setFocus();QApplication::processEvents();const auto before=surface.pixmap().toImage();
 const QRect image(100,140,400,280),toolbar(150,95,224,40);tigersheets::testing::selectDocumentImage(&surface,image,toolbar);QApplication::processEvents();auto* overlay=surface.findChild<QWidget*>("replay.documentImageSelection");require(overlay&&overlay->isVisible()&&surface.grab().toImage().pixelColor(image.center())!=before.pixelColor(image.center()),"native image selection visibly overlays the actual PDF image without replacing its pixels");require(surface.pixmap().toImage()==before,"selection leaves the original PDF raster untouched");const auto buttons=overlay->findChildren<QToolButton*>();require(buttons.size()==5,"observed black floating image toolbar exposes its five graphical controls");for(auto* button:buttons)require(!button->icon().isNull()&&button->text().isEmpty(),"floating source viewer controls use icons rather than text substitutes");tigersheets::testing::clearDocumentSelection(&surface);QApplication::processEvents();require(surface.findChild<QWidget*>("replay.documentImageSelection")==nullptr&&surface.grab().toImage()==before,"native deselection clears gray overlay and floating toolbar and restores the exact original pixels");surface.hide();actualPdfEscape();actualCaseArtwork();std::cout<<"Native document image selection passed\n";
 }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
