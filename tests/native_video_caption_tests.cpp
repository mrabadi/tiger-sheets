#include "support/native_video_caption.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QDialog>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include <QTest>
#include <QWidget>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void nativeEditorAndModal(){
 tigersheets::MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 auto*view=window.findChild<tigersheets::WorksheetView*>("worksheet.grid");auto*bar=window.findChild<QLineEdit*>("formula.input");auto*model=dynamic_cast<tigersheets::WorksheetModel*>(view->model());require(view&&bar&&model,"caption regression uses the actual native workbook");
 view->setCurrentIndex(model->index(1,1));bar->setFocus();QTest::keyClicks(bar,"=SU");QApplication::processEvents();const auto original=window.currentSession().snapshot();const auto current=view->currentIndex();const auto selection=view->selectionModel()->selection();const auto before=window.grab().toImage();const QRect bounds(449,559,384,157);
 tigersheets::testing::showVideoCaption(&window,bounds,{"Move to Next Sheet","Ctrl + PgDn (PC)",QString::fromUtf8("Ctrl + Fn + ↓ (Mac)")});QApplication::processEvents();
 require(bar->hasFocus()&&bar->text()=="=SU"&&view->currentIndex()==current&&view->selectionModel()->selection()==selection&&window.currentSession().snapshot()==original,"a source caption retains the actual unfinished formula, active cell, selection and workbook history");
 tigersheets::testing::clearVideoCaption(&window);QApplication::processEvents();require(window.grab().toImage()==before,"clearing the native workbook caption restores the original pending-formula pixels");
 tigersheets::testing::showVideoCaption(&window,bounds,{"Input remains active"});QTest::keyClicks(bar,"M(");require(bar->text()=="=SUM("&&window.currentSession().snapshot()==original,"typing continues in the actual uncommitted formula while publisher artwork is visible");tigersheets::testing::clearVideoCaption(&window);QTest::keyClick(bar,Qt::Key_Escape);QTest::keyClick(bar,Qt::Key_Escape);QApplication::processEvents();require(bar->text().isEmpty()&&window.currentSession().snapshot()==original,"native Escape dismisses formula help then cancels the test draft before opening Format Cells");view->setFocus();
 bool tested=false,overModal=false,retainedFocus=false;QTimer modalPoll;
 QObject::connect(&modalPoll,&QTimer::timeout,&window,[&]{auto*dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;auto*categories=dialog->findChild<QListWidget*>("formatCells.categoryList");if(!categories)return;modalPoll.stop();categories->setFocus();QApplication::processEvents();auto*focus=QApplication::focusWidget();const auto snapshot=window.currentSession().snapshot();auto*caption=tigersheets::testing::showVideoCaption(&window,bounds,{"Increase Decimal","Alt + H + 0 (PC)","Mac: Ctrl + Shift + 1"});QApplication::processEvents();tested=true;overModal=caption->isWindow()&&caption->windowType()==Qt::ToolTip&&caption->isVisible()&&QApplication::activeModalWidget()==dialog;retainedFocus=QApplication::focusWidget()==focus&&caption->focusPolicy()==Qt::NoFocus&&caption->testAttribute(Qt::WA_TransparentForMouseEvents)&&window.currentSession().snapshot()==snapshot;tigersheets::testing::clearVideoCaption(&window);dialog->reject();});modalPoll.start(5);
 QTimer::singleShot(5000,&window,[&]{if(!tested)if(auto*dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
 QTest::keyClick(view,Qt::Key_1,Qt::ControlModifier);QApplication::processEvents();require(tested&&overModal,"source shortcut captions remain a visible top-level overlay above an actual active Format Cells modal");require(retainedFocus&&window.currentSession().snapshot()==original&&view->currentIndex()==current&&view->selectionModel()->selection()==selection,"showing and clearing the modal caption preserves native dialog focus, worksheet selection and history");
}
}
int main(int argc,char**argv){QApplication app(argc,argv);try{
 QWidget surface;surface.resize(1280,720);surface.setStyleSheet("background:white;");
 QLineEdit editor(&surface);editor.setGeometry(450,570,200,32);editor.setText("A1");surface.show();surface.activateWindow();editor.setFocus();QApplication::processEvents();
 const QRect bounds(449,560,384,157);const auto baseline=surface.grab().toImage();
 auto* caption=tigersheets::testing::showVideoCaption(&surface,bounds,{"Move to Next Sheet","Ctrl + PgDn (PC)",QString::fromUtf8("Ctrl + Fn + ↓ (Mac)")});QApplication::processEvents();
 require(caption&&caption->isVisible()&&caption->size()==bounds.size()&&caption->mapToGlobal(QPoint())==surface.mapToGlobal(bounds.topLeft()),"the source-observed shortcut caption is a real temporary widget at the observed native-window position");
 const auto painted=caption->grab().toImage();require(painted.pixelColor(6,150).lightness()<100,"the caption paints a translucent dark banner over the actual underlying content");
 int whiteGlyphs=0;for(int y=15;y<50;++y)for(int x=8;x<375;++x)if(painted.pixelColor(x,y).lightness()>240)++whiteGlyphs;
 require(whiteGlyphs>600,"the caption visibly paints its source text instead of an empty dark rectangle");
 require(editor.hasFocus()&&caption->testAttribute(Qt::WA_TransparentForMouseEvents),"publisher artwork retains ordinary keyboard focus and lets pointer events reach the worksheet beneath");
 QTest::keyClicks(&editor,"B");require(editor.text()=="A1B","normal native typing continues while the caption is visible");editor.setText("A1");
 tigersheets::testing::clearVideoCaption(&surface);QApplication::processEvents();require(surface.findChild<QWidget*>("replay.videoCaption")==nullptr&&surface.grab().toImage()==baseline,"clearing publisher artwork removes its widget and restores the actual underlying pixels");
 surface.hide();nativeEditorAndModal();
 std::cout<<"native video caption tests passed\n";
 }catch(const std::exception&error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
