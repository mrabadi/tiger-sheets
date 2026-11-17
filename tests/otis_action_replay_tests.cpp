#include "support/native_action_replay.hpp"
#include "support/native_popup_composition_test.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QElapsedTimer>
#include <QImage>
#include <QProcess>
#include <QPointer>
#include <QScrollBar>
#include <QTableView>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QMessageBox>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QTest>
#include <QToolTip>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
Json acceptanceStep(const char* id,const Json& action,const Json& expected=Json::object()){
 return {{"id",id},{"videoTime","synthetic acceptance"},{"evidence",{{"kind","acceptance"},{"reference","Runner contract; this is not a source-video observation"}}},{"action",action},{"expect",expected}};
}
void nativeEffectiveBoldStyleCheckpoints(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshot();
 const Json observe={{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}};
 const auto plain=replay.run({{"steps",Json::array({acceptanceStep("effective-default-plain",observe,{{"cells",Json::array({{{"address","A1"},{"style",{{"bold",false}}}}})}})})}});
 require(plain.at("executionComplete")==true,"An absent native bold property means the actual default plain font: "+plain.dump());require(window.currentSession().snapshot()==before,"Reading an effective plain font never authors an explicit style or history");
 const auto wrong=replay.run({{"steps",Json::array({acceptanceStep("effective-default-wrong-bold",observe,{{"cells",Json::array({{{"address","A1"},{"style",{{"bold",true}}}}})}})})}});
 require(wrong.at("executionComplete")==false&&wrong.at("steps").back().at("error")=="Replay cell style mismatch: A1 bold"&&window.currentSession().snapshot()==before,"An incorrect bold expectation fails without changing actual native styles");
 const auto explicitStyles=replay.run({{"steps",Json::array({acceptanceStep("effective-explicit-bold",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+B"}}},{{"cells",Json::array({{{"address","A1"},{"style",{{"bold",true}}}}})}}),acceptanceStep("effective-explicit-plain",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+B"}}},{{"cells",Json::array({{{"address","A1"},{"style",{{"bold",false}}}}})}})})}});
 require(explicitStyles.at("executionComplete")==true,"Explicit native bold true and false remain distinguishable: "+explicitStyles.dump());
}
void nativeWidgetHoverPreservesState(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* grid=window.findChild<WorksheetView*>("worksheet.grid");auto* fx=window.findChild<QToolButton*>("formula.insertFunction");require(grid&&fx,"actual source formula control exists");grid->setFocus();QTest::mouseMove(grid->viewport(),QPoint(20,20));QApplication::processEvents();const auto before=window.currentSession().snapshot();auto* focus=QApplication::focusWidget();testing::NativeActionReplay replay(window,output.path());const auto report=replay.run({{"steps",Json::array({acceptanceStep("source-widget-hover",{{"type","mouse"},{"target","formula.insertFunction"},{"gesture","hover"}})})}});require(report.at("executionComplete")==true,"source widget hover executes a genuine pointer move: "+report.dump());require(fx->underMouse()&&QApplication::focusWidget()==focus&&!QApplication::activeModalWidget(),"hover reaches the actual control without clicking Insert Function or moving focus");require(window.currentSession().snapshot()==before,"source pointer hover preserves workbook data and history");
}
void nativeWidgetHoverDwell(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* grid=window.findChild<WorksheetView*>("worksheet.grid");auto* fx=window.findChild<QToolButton*>("formula.insertFunction");require(grid&&fx&&!fx->toolTip().isEmpty(),"real formula control has its native tooltip");grid->setFocus();QTest::mouseMove(grid->viewport(),QPoint(20,20));QApplication::processEvents();const auto before=window.currentSession().snapshot();auto* focus=QApplication::focusWidget();testing::NativeActionReplay replay(window,output.path());const auto report=replay.run({{"steps",Json::array({acceptanceStep("source-widget-tooltip-dwell",{{"type","mouse"},{"target","formula.insertFunction"},{"gesture","hover"},{"dwellMs",1500}},{{"toolTip",fx->toolTip().toStdString()}})})}});require(report.at("executionComplete")==true,"native delayed hover executes");require(QToolTip::isVisible()&&QToolTip::text()==fx->toolTip(),"source hover dwell lets the actual Qt tooltip timer display the real control text");require(fx->underMouse()&&QApplication::focusWidget()==focus&&window.currentSession().snapshot()==before,"tooltip dwell preserves pointer target, focus, data and history");QToolTip::hideText();
}
void nativeReadOnlyDocumentTooltipReplay(){
 class PointerEvents final:public QObject{public:int entered=0,moved=0,tooltip=0;protected:bool eventFilter(QObject* object,QEvent* event)override{if(object->objectName()=="context.editPdf"){entered+=event->type()==QEvent::Enter;moved+=event->type()==QEvent::MouseMove;tooltip+=event->type()==QEvent::ToolTip;}return false;}} pointerEvents;qApp->installEventFilter(&pointerEvents);
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const std::string source=std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-10-K.pdf";
 const auto before=window.currentSession().snapshot();
 const auto report=replay.run({{"windowSize",{1280,720}},{"steps",Json::array({
  acceptanceStep("actual-pdf-open",{{"type","document_context"},{"operation","open"},{"path",source}}),
  acceptanceStep("actual-page-select",{{"type","key_sequence"},{"target","context.page"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("actual-page60",{{"type","text"},{"target","context.page"},{"text","60"}}),
  acceptanceStep("actual-page-accept",{{"type","key_sequence"},{"target","context.page"},{"keys",{"Return"}}}),
  acceptanceStep("actual-pointer-away",{{"type","mouse"},{"target","context.surface"},{"gesture","hover"}}),
  acceptanceStep("actual-edit-pdf-tooltip",{{"type","mouse"},{"target","context.editPdf"},{"gesture","hover"},{"dwellMs",1800}},{{"externalDocument",{{"page",60},{"editPdfToolTipVisible",true},{"toolTipText","Edit PDF"}}}})
 })}});
 require(report.at("executionComplete")==true,"Actual read-only document tooltip is visible through native replay: "+report.dump());require(window.currentSession().snapshot()==before,"Native PDF tooltip replay preserves worksheet data and history");
 require(pointerEvents.entered&&pointerEvents.moved&&pointerEvents.tooltip,"Native hover delivers actual Enter, MouseMove and ToolTip events to the real document control");
 const auto& capture=report.at("steps").back();require(capture.contains("contextScreenshot")&&capture.contains("sourceCompositeScreenshot"),"Foreground PDF and native tooltip pixels have independent and composed captures");const QImage document(output.filePath(QString::fromStdString(capture.at("contextScreenshot").get<std::string>()))),composite(output.filePath(QString::fromStdString(capture.at("sourceCompositeScreenshot").get<std::string>())));require(!document.isNull()&&document.size()==composite.size(),"Actual foreground source captures retain document geometry");
 int different=0;for(int y=0;y<document.height();++y)for(int x=0;x<document.width();++x){bool popup=false;for(const auto& surface:capture.at("ownedWindowScreenshots")){const auto& at=surface.at("position");const auto& size=surface.at("size");popup|=QRect(at[0].get<int>(),at[1].get<int>(),size[0].get<int>(),size[1].get<int>()).contains(x,y);}if(!popup&&document.pixel(x,y)!=composite.pixel(x,y))++different;}
 require(different==0,"The actual foreground PDF, rather than the obscured workbook, supplies every composed background pixel");
}
void nativeWindowInspectionRetainsCompleteDraft(){
 auto scenario=[](bool useReplay){
  QTemporaryDir files;MainWindow origin,source;origin.resize(1280,720);source.resize(1280,720);origin.show();source.show();QApplication::processEvents();require(origin.saveWorkbook(files.filePath("Origin.xlsx"))&&source.saveWorkbook(files.filePath("InspectSource.xlsx")),"name two genuine native workbook windows");auto* view=origin.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* raw=source.findChild<WorksheetView*>("worksheet.grid");auto* rawModel=dynamic_cast<WorksheetModel*>(raw->model());require(model->setData(model->index(61,2),"2")&&model->setData(model->index(62,2),"3")&&rawModel->setData(rawModel->index(33,0),"Read only"),"independent native subtotal and source-inspection fixtures");raw->setCurrentIndex(rawModel->index(33,0));raw->scrollTo(raw->currentIndex());origin.activateWindow();QApplication::processEvents();view->setCurrentIndex(model->index(63,2));view->scrollTo(view->currentIndex());view->setFocus();QTest::keyClick(view,Qt::Key_Equal,Qt::AltModifier);QTest::keyRelease(view,Qt::Key_Alt);QApplication::processEvents();require(view->activeCellEditor()&&view->activeCellEditor()->text()=="=SUM(C62:C63)"&&model->raw(63,2).isEmpty(),"native AltEquals leaves the complete source C64 subtotal draft uncommitted");const auto before=origin.currentSession().snapshot(),rawBefore=source.currentSession().snapshot();
  if(useReplay){testing::NativeActionReplay replay(origin,files.filePath("inspection"));const auto report=replay.run({{"steps",Json::array({acceptanceStep("source-inspect-other-window",{{"type","activate_window"},{"documentName","InspectSource.xlsx"}}),acceptanceStep("source-inspect-a34",{{"type","mouse_cell"},{"address","A34"}}),acceptanceStep("source-return-with-complete-draft",{{"type","activate_window"},{"documentName","Origin.xlsx"}},{{"formulaDraft","=SUM(C62:C63)"},{"revision",before.at("revision")}})})}});require(report.at("executionComplete")==true,"read-only source activation returns the complete uncommitted native SUM draft: "+report.dump());}
  else{source.raise();source.activateWindow();QApplication::processEvents();raw->setCurrentIndex(rawModel->index(33,0));raw->scrollTo(raw->currentIndex());raw->setFocus();QTest::mouseClick(raw->viewport(),Qt::LeftButton,Qt::NoModifier,raw->visualRect(raw->currentIndex()).center());origin.raise();origin.activateWindow();QApplication::processEvents();}
  require(origin.currentSession().snapshot()==before&&source.currentSession().snapshot()==rawBefore&&model->raw(63,2).isEmpty()&&view->activeCellEditor()&&view->activeCellEditor()->text()=="=SUM(C62:C63)","native source inspection preserves the full draft, both documents and history until the observed late Enter");QTest::keyClick(view->activeCellEditor(),Qt::Key_Return);QApplication::processEvents();origin.recalculateNow();require(model->raw(63,2)=="=SUM(C62:C63)"&&model->numericValue(63,2)==5,"separate late Enter commits the actual source subtotal");require(origin.executeCommand("undo")&&origin.currentSession().snapshotView().at("sheets")==before.at("sheets"),"late acceptance is exactly one native Undo");
 };scenario(false);scenario(true);
}
void nativeActivePointingWindowPreservesCaret(){
 QTemporaryDir files;MainWindow origin,source;origin.resize(1280,720);source.resize(1280,720);origin.show();source.show();QApplication::processEvents();require(origin.saveWorkbook(files.filePath("PointOrigin.xlsx"))&&source.saveWorkbook(files.filePath("PointSource.xlsx")),"name actual external-pointing surfaces");auto* originView=origin.findChild<WorksheetView*>("worksheet.grid");auto* originModel=dynamic_cast<WorksheetModel*>(originView->model());auto* sourceView=source.findChild<WorksheetView*>("worksheet.grid");auto* sourceModel=dynamic_cast<WorksheetModel*>(sourceView->model());require(sourceModel->setData(sourceModel->index(8,2),"180"),"prepare real source C9");sourceView->setCurrentIndex(sourceModel->index(49,0));sourceView->scrollTo(sourceView->currentIndex(),QAbstractItemView::PositionAtTop);QApplication::processEvents();require(sourceView->rowAt(0)>0,"source fixture starts scrolled below its reference cell");const auto before=origin.currentSession().snapshot(),sourceBefore=source.currentSession().snapshot();origin.activateWindow();QApplication::processEvents();originView->setFocus();QTest::keyClicks(originView,"=");QApplication::processEvents();require(originView->activeCellEditor(),"start actual inline source formula");testing::NativeActionReplay replay(origin,files.filePath("pointing"));const auto report=replay.run({{"steps",Json::array({acceptanceStep("point-switch-source",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Ctrl+Tab"}}}),acceptanceStep("point-reactivate-current-source",{{"type","activate_window"},{"documentName","PointSource.xlsx"}}),acceptanceStep("point-scroll-source-home",{{"type","vertical_view"},{"topRow",1}},{{"topRow",1},{"formulaDraft","="}}),acceptanceStep("point-source-cell",{{"type","mouse_cell"},{"address","C9"}},{{"formulaDraft","='[PointSource.xlsx]Sheet1'!$C$9"}}),acceptanceStep("point-relative-cycle",{{"type","key_sequence"},{"target","formula.input"},{"keys",{"F4","F4","F4"}}},{{"formulaDraft","='[PointSource.xlsx]Sheet1'!C9"}}),acceptanceStep("point-return-origin",{{"type","key_sequence"},{"target","formula.input"},{"keys",{"Ctrl+Tab"}}}),acceptanceStep("point-commit",{{"type","key_sequence"},{"target","formula.input"},{"keys",{"Return"}}},{{"cells",Json::array({{{"address","A1"},{"input","='[PointSource.xlsx]Sheet1'!C9"},{"value",180}}})}})})}});require(report.at("executionComplete")==true,"native source scrollbar Home preserves the genuine external draft caret and F4 modes: "+report.dump());require(source.currentSession().snapshot()==sourceBefore,"source inspection and pointing preserve its data and history");require(origin.executeCommand("undo")&&origin.currentSession().snapshotView().at("sheets")==before.at("sheets"),"actual external reference commits in one Undo");
}
void nativeReversePointSwitchRetainsActualSource(){
 QTemporaryDir files;MainWindow origin,source;origin.resize(1280,720);source.resize(1280,720);origin.show();source.show();QApplication::processEvents();require(origin.saveWorkbook(files.filePath("ReverseOrigin.xlsx"))&&source.saveWorkbook(files.filePath("ReverseSource.xlsx")),"name actual reverse-pointing surfaces");
 auto* view=origin.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* sourceView=source.findChild<WorksheetView*>("worksheet.grid");auto* sourceModel=dynamic_cast<WorksheetModel*>(sourceView->model());require(sourceModel->setData(sourceModel->index(9,2),"1546"),"prepare independently known source C10");source.window()->activateWindow();QApplication::processEvents();origin.activateWindow();QApplication::processEvents();
 testing::NativeActionReplay replay(origin,files.filePath("reverse"));
 for(int row:{56,57}){
  view->setCurrentIndex(model->index(row-1,2));view->scrollTo(model->index(43,0),QAbstractItemView::PositionAtTop);view->setFocus();QApplication::processEvents();const auto before=origin.currentSession().snapshot(),sourceBefore=source.currentSession().snapshot();
  const auto report=replay.run({{"steps",Json::array({
   acceptanceStep("reverse-point-begin",{{"type","text"},{"target","worksheet.grid"},{"text","="}},{{"formulaDraft","="}}),
   acceptanceStep("reverse-point-switch",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Ctrl+Shift+Tab"}}},{{"documentName","ReverseSource.xlsx"},{"formulaDraft","="}}),
   acceptanceStep("reverse-point-expose",{{"type","wheel_to_cell"},{"address","C10"}},{{"documentName","ReverseSource.xlsx"},{"formulaDraft","="}}),
   acceptanceStep("reverse-point-source",{{"type","mouse_cell"},{"address","C10"}},{{"formulaDraft","='[ReverseSource.xlsx]Sheet1'!$C$10"}}),
   acceptanceStep("reverse-point-relative",{{"type","key_sequence"},{"target","formula.input"},{"keys",{"F4","F4","F4"}}},{{"documentName","ReverseSource.xlsx"},{"formulaDraft","='[ReverseSource.xlsx]Sheet1'!C10"}}),
   acceptanceStep("reverse-point-return",{{"type","key_sequence"},{"target","formula.input"},{"keys",{"Ctrl+Tab"}}},{{"documentName","ReverseOrigin.xlsx"},{"formulaDraft","='[ReverseSource.xlsx]Sheet1'!C10"}}),
   acceptanceStep("reverse-point-commit",{{"type","key_sequence"},{"target","formula.input"},{"keys",{"Return"}}},{{"cells",Json::array({{{"address","C"+std::to_string(row)},{"input","='[ReverseSource.xlsx]Sheet1'!C10"},{"value",1546}}})}})
  })}});
  require(report.at("executionComplete")==true,"reverse workbook Point switches to the actual source on each inline entry without an activation rescue: "+report.dump());require(source.currentSession().snapshot()==sourceBefore,"switching and pointing leave source input/history untouched");require(origin.executeCommand("undo")&&origin.currentSession().snapshotView().at("sheets")==before.at("sheets"),"each actual external entry remains one native Undo");
 }
}
void nativeNumberFormatPopupKeyboardSelection(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());require(model->setData(model->index(0,0),"-1799"),"prepare independent numeric input before the actual popup selection");if(!window.property("ribbon.collapsed").toBool())QTest::keyClick(view,Qt::Key_F1,Qt::ControlModifier);QApplication::processEvents();const auto before=window.currentSession().snapshot();testing::NativeActionReplay replay(window,output.path());
 const auto opening=replay.run({{"steps",Json::array({
  acceptanceStep("format-popup-show-home",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Alt","H"}}}),
  acceptanceStep("format-popup-open-combo",{{"type","mouse"},{"target","ribbon.numberFormat"},{"gesture","click"}})
 })}});require(opening.at("executionComplete")==true,"open the real source Number Format popup");
 auto* formats=window.findChild<QComboBox*>("ribbon.numberFormat");require(formats&&formats->view()->window()->isVisible(),"the real number-format dropdown is open before keyboard input");
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("format-popup-select-number",{{"type","key_sequence"},{"target","ribbon.numberFormat"},{"keys",{"Home","Down","Return"}}},{{"cells",Json::array({{{"address","A1"},{"input","-1799"},{"style",{{"numberFormat","#,##0.00"}}}}})}})
 })}});
 require(report.at("executionComplete")==true,"keyboard popup selection accepts Number without sending a later key into the worksheet: "+report.dump());require(!formats->view()->window()->isVisible()&&!view->activeCellEditor(),"Return closes the actual Number Format dropdown without opening a cell draft");require(model->raw(0,0)=="-1799"&&window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"selecting a number format preserves raw input and remains one Undo");
 require(window.executeCommand("redo"),"restore the genuine Number selection before the following source shortcut chain");
 const auto following=replay.run({{"steps",Json::array({acceptanceStep("format-popup-source-following-shortcuts",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Escape","Alt","4","Alt","4"}}},{{"cells",Json::array({{{"address","A1"},{"input","-1799"},{"style",{{"numberFormat","#,##0"}}}}})}})})}});
 require(following.at("executionComplete")==true&&!QApplication::activePopupWidget()&&!view->activeCellEditor(),"the source Escape and repeated decimal shortcuts dismiss Home and format the original cell instead of entering a stray4: "+following.dump());for(int count=0;count<3;++count)require(window.executeCommand("undo"),"each accepted format or decimal shortcut has its own Undo");require(window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"the whole source popup/decimal chain preserves input and independent styling");
}
void nativeQuickAnalysisVisibilityCheckpoints(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* button=window.findChild<QToolButton*>("worksheet.quickAnalysis");require(button,"the real native Quick Analysis control exists even while hidden");
 for(const auto& [column,value]:std::vector<std::pair<int,int>>{{6,1267},{7,1056},{8,1420}})require(model->setData(model->index(22,column),QString::number(value),Qt::EditRole),"prepare the source net-income values");if(!window.property("ribbon.collapsed").toBool())QTest::keyClick(view,Qt::Key_F1,Qt::ControlModifier);QApplication::processEvents();view->setCurrentIndex(model->index(23,6));view->scrollTo(model->index(3,0),QAbstractItemView::PositionAtTop);view->setFocus();QApplication::processEvents();const auto before=window.currentSession().snapshot();testing::NativeActionReplay replay(window,output.path());
 const Json checkpoint={{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}};
 const auto held=replay.run({{"steps",Json::array({acceptanceStep("net-income-press",{{"type","mouse_cell"},{"address","G23"},{"gesture","press"}}),acceptanceStep("net-income-expand",{{"type","mouse_cell"},{"address","I24"},{"gesture","move"}}),acceptanceStep("net-income-reduce",{{"type","mouse_cell"},{"address","I23"},{"gesture","move"}}),acceptanceStep("net-income-held-qa-hidden",checkpoint,{{"nameBoxText","1R x 3C"},{"quickAnalysisVisible",false}})})}});
 require(held.at("executionComplete")==true&&!button->isVisible(),"a held net-income range exposes the actual hidden Quick Analysis state without targeting an unavailable widget: "+held.dump());
 const auto wrongHidden=replay.run({{"steps",Json::array({acceptanceStep("net-income-reject-visible-while-held",checkpoint,{{"quickAnalysisVisible",true}})})}});require(wrongHidden.at("executionComplete")==false&&wrongHidden.at("steps").back().at("error")=="Replay checkpoint mismatch: quickAnalysisVisible"&&!button->isVisible(),"a false positive visibility expectation fails without showing the button");
 const auto released=replay.run({{"steps",Json::array({acceptanceStep("net-income-release",{{"type","mouse_cell"},{"address","I23"},{"gesture","release"}},{{"nameBoxText","G23"}}),acceptanceStep("net-income-released-qa-visible",checkpoint,{{"quickAnalysisVisible",true},{"nameBoxText","G23"}})})}});require(released.at("executionComplete")==true&&button->isVisible(),"a completed net-income range exposes the real visible Quick Analysis button: "+released.dump());
 const auto wrongVisible=replay.run({{"steps",Json::array({acceptanceStep("net-income-reject-hidden-after-release",checkpoint,{{"quickAnalysisVisible",false}})})}});require(wrongVisible.at("executionComplete")==false&&wrongVisible.at("steps").back().at("error")=="Replay checkpoint mismatch: quickAnalysisVisible"&&button->isVisible(),"a false negative visibility expectation fails without hiding the button");require(window.currentSession().snapshot()==before,"visibility checkpoints and selection gestures preserve workbook contents and history");
}
void nativeKeySequenceFollowsEditorFocus(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());const QString original="Pensions & Other Items:";
 require(model->setData(model->index(56,1),original,Qt::EditRole),"prepare the source B57 literal before editing");view->setCurrentIndex(model->index(56,1));view->scrollTo(model->index(33,1),QAbstractItemView::PositionAtTop);view->setFocus();QApplication::processEvents();const auto before=window.currentSession().snapshot();testing::NativeActionReplay replay(window,output.path());
 const auto edit=replay.run({{"steps",Json::array({acceptanceStep("source-b57-f2-home",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"F2","Home"}}})})}});
 auto* editor=view->activeCellEditor();require(edit.at("executionComplete")==true&&view->currentIndex()==model->index(56,1)&&editor&&editor->hasFocus()&&editor->cursorPosition()==0&&editor->text()==original,"F2 then Home follows the actual new editor focus and retains B57 instead of navigating the grid to A57");require(window.currentSession().snapshot()==before,"opening and moving within the real B57 draft does not create history");
 const auto commit=replay.run({{"steps",Json::array({acceptanceStep("source-b57-insert-prefix",{{"type","text"},{"target","worksheet.cellEditor"},{"text","(+/-) "}}),acceptanceStep("source-b57-commit-then-navigate",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return","Down"}}})})}});
 require(commit.at("executionComplete")==true&&model->raw(56,1)=="(+/-) "+original&&view->currentIndex()==model->index(58,1)&&!view->activeCellEditor(),"a sequence also follows focus back to the grid after committing the draft");require(window.currentSession().undo()&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"one Undo restores the original literal after prefix insertion");
 view->setCurrentIndex(model->index(56,1));view->setFocus();QApplication::processEvents();const auto cancelBefore=window.currentSession().snapshot();const auto cancel=replay.run({{"steps",Json::array({acceptanceStep("source-b57-select-and-cancel",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"F2","Home","Shift+End","Escape","Down"}}})})}});
 require(cancel.at("executionComplete")==true&&view->currentIndex()==model->index(57,1)&&!view->activeCellEditor()&&window.currentSession().snapshot()==cancelBefore,"Home, shifted editor selection, Escape and subsequent grid navigation preserve the cancelled draft and history");
}
void nativeSameWindowLiteralDraftSurvivesActivation(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());view->setCurrentIndex(model->index(43,1));view->scrollTo(view->currentIndex(),QAbstractItemView::PositionAtCenter);view->setFocus();QApplication::processEvents();const auto before=window.currentSession().snapshot();const auto name=window.findChild<QLabel*>("workspace.documentName")->text().toStdString();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("literal-start",{{"type","text"},{"target","worksheet.grid"},{"text","C"}}),
  acceptanceStep("literal-continue",{{"type","text"},{"target","worksheet.cellEditor"},{"text","ommon"}},{{"formulaDraft","Common"},{"revision",0}}),
  acceptanceStep("literal-same-window-activation",{{"type","activate_window"},{"documentName",name}},{{"formulaDraft","Common"},{"revision",0}}),
  acceptanceStep("literal-scroll-during-entry",{{"type","vertical_view"},{"topRow",26}},{{"formulaDraft","Common"},{"revision",0},{"topRow",26},{"row",44},{"column",2}}),
  acceptanceStep("literal-source-next-characters",{{"type","text"},{"target","worksheet.cellEditor"},{"text"," Shares:"}},{{"formulaDraft","Common Shares:"},{"revision",0}}),
  acceptanceStep("literal-explicit-acceptance",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}},{{"revision",1},{"cells",Json::array({{{"address","B44"},{"input","Common Shares:"}}})}})
 })}});
 require(report.at("executionComplete")==true,"reactivating the same workbook and scrolling preserves the actual literal editor until the observed Enter: "+report.dump());require(!view->activeCellEditor()&&model->raw(43,1)=="Common Shares:","the entire source label commits only after explicit acceptance");require(window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"literal entry remains one Undo with no premature partial-label transaction");
}
void firstKeyboardCheckpoint(){
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();
 testing::NativeActionReplay replay(window,output.path());
 const Json manifest={{"sourceVideo","https://www.youtube.com/watch?v=O2uPNfErmJo"},
  {"scope","Synthetic native UI acceptance; not a claim of source gesture observation"},
  {"steps",Json::array({{{"id","008-hide-gridlines"},{"videoTime","08:05-08:14"},
   {"evidence",{{"kind","acceptance"},{"reference","Transcript ledger row 8; source gestures await frame evidence"}}},
   {"action",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Alt","W","V","G"}}}},
   {"expect",{{"showGridLines",false},{"ribbon","View"}}}}})}};
 const auto report=replay.run(manifest);
 require(report.at("executionComplete")==true,"real Alt W V G replay passes and emits a native UI checkpoint: "+report.dump());
 const auto& step=report.at("steps").at(0);
 require(step.at("status")=="passed"&&step.at("before").at("showGridLines")==true&&step.at("after").at("showGridLines")==false,"trace records actual before and after view state");
 require(step.at("after").at("revision").get<int>()==step.at("before").at("revision").get<int>()+1,"keyboard gesture persists exactly one gridline transaction");
 require(!QImage(output.filePath("008-hide-gridlines.png")).isNull(),"each executed step writes a rendered screenshot");
 QFile trace(output.filePath("action-trace.jsonl"));require(trace.open(QIODevice::ReadOnly),"machine-readable trace exists");
 require(Json::parse(trace.readAll().toStdString())==step,"JSONL retains the timestamp, evidence, action, observed state and screenshot");
 require(window.currentSession().snapshotView().at("sheets").at(0).at("cells").empty(),"navigation replay never types key-tip letters into worksheet cells");
}
void typedFormulaAndCorrection(){
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();
 testing::NativeActionReplay replay(window,output.path());Json steps=Json::array();
 auto keys=[&](const char* id,const char* target,const Json& values,const Json& expected=Json::object()){steps.push_back(acceptanceStep(id,{{"type","key_sequence"},{"target",target},{"keys",values}},expected));};
 auto text=[&](const char* id,const char* target,const char* value){steps.push_back(acceptanceStep(id,{{"type","text"},{"target",target},{"text",value}}));};
 keys("001-select-address","formula.name",{"Ctrl+A"});text("002-type-address","formula.name","C4");keys("003-go-to-address","formula.name",{"Return"},{{"row",4},{"column",3}});
 keys("004-select-formula","formula.input",{"Ctrl+A"});text("005-type-initial-formula","formula.input","=1+2");keys("006-commit-initial-formula","formula.input",{"Return"},{{"cells",Json::array({{{"address","C4"},{"input","=1+2"}}})}});
 keys("007-select-correction-address","formula.name",{"Ctrl+A"});text("008-type-correction-address","formula.name","C4");keys("009-return-to-cell","formula.name",{"Return"});
 keys("010-select-formula-for-correction","formula.input",{"Ctrl+A"});text("011-type-corrected-formula","formula.input","=1+3");keys("012-commit-correction","formula.input",{"Return"},{{"cells",Json::array({{{"address","C4"},{"input","=1+3"}}})}});
 const auto report=replay.run({{"scope","Synthetic formula entry and correction acceptance"},{"steps",steps}});
 require(report.at("executionComplete")==true,"native replay enters and corrects formulas through visible editors: "+report.dump());
 require(report.at("steps").size()==12,"each selection, draft and commit retains a distinct ordered trace record");
 const auto& original=report.at("steps").at(5);const auto& corrected=report.at("steps").at(11);
 require(original.at("after").at("revision")==1&&corrected.at("after").at("revision")==2,"the initial wrong formula and later correction remain separate real transactions");
 require(window.currentSession().cellAt("sheet-1",4,3)->at("input")=="=1+3","the final corrected cell is authored by UI entry");
 require(window.currentSession().undo()&&window.currentSession().cellAt("sheet-1",4,3)->at("input")=="=1+2","one undo reveals the actual intermediate formula instead of a cloned final sheet");
}
void nativeSourceCurrencyDropdownAcceptance(){
 QTemporaryDir output;QString manifestPath=QString::fromLocal8Bit(qgetenv("TIGERSHEETS_SOURCE_MODAL_MANIFEST"));
 if(manifestPath.isEmpty()){
  manifestPath=output.filePath("generated-history.json");QProcess generator;
  generator.start("python3",{QString(TIGERSHEETS_SOURCE_DIR)+"/tools/build_otis_history_manifest.py","--workspace",output.filePath("source-fixture"),"--include-intro","--through","1430","--output",manifestPath});
  require(generator.waitForFinished(60000)&&generator.exitStatus()==QProcess::NormalExit&&generator.exitCode()==0,"Generate the actual source-backed modal sequences: "+generator.readAllStandardError().toStdString());
 }
 QFile source(manifestPath);require(source.open(QIODevice::ReadOnly),"Read the generated source-modal manifest");const auto manifest=Json::parse(source.readAll().toStdString());
 int openedCurrencyModals=0;for(const auto& step:manifest.at("steps"))if(step.at("action").at("type")=="modal"){
  bool opened=false;for(const auto& child:step.at("action").at("steps")){
   const auto& action=child.at("action");if(action.value("target",std::string())!="formatCells.currencySymbol")continue;
   opened|=action.at("type")=="mouse"&&action.value("gesture",std::string())=="click";
   if(action.at("type")=="key_sequence")for(const auto& key:action.at("keys"))opened|=key=="Alt+Down";
  }openedCurrencyModals+=opened;
 }
 require(openedCurrencyModals==3,"Cover every actual generated mouse or keyboard opened currency popup, including the later None choice");
 for(const auto& ids:{std::pair{"historical-01731-format","historical-01729-dialog-action"},std::pair{"historical-02268-format","historical-02266-dialog-action"},std::pair{"historical-02789-format","historical-02787-dialog-action"}}){
  Json modal;for(const auto& step:manifest.at("steps"))if(step.at("id")==ids.first)modal=step;
  require(!modal.is_null(),std::string("Retain actual source modal ID ")+ids.first);
  MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());
  for(int column=2;column<=4;++column)require(model->setData(model->index(3,column),QString::number(5648+column)),"Independent numeric fixture for native currency formatting");
  view->selectionModel()->setCurrentIndex(model->index(3,2),QItemSelectionModel::ClearAndSelect);view->selectionModel()->select(QItemSelection(model->index(3,2),model->index(3,4)),QItemSelectionModel::ClearAndSelect);view->setFocus();QApplication::processEvents();
  const auto before=window.currentSession().snapshot();testing::NativeActionReplay replay(window,output.filePath(ids.first));const auto report=replay.run({{"steps",Json::array({modal})}});
  require(report.at("executionComplete")==true,std::string("Source currency popup commits before the separate dialog acceptance: ")+ids.first+" "+report.dump());
  const Json* selected=nullptr;for(const auto& child:report.at("steps").at(0).at("children"))if(child.at("id")==ids.second)selected=&child;
  require(selected&&selected->at("after").at("modal")=="formatCells.dialog","The symbol choice retains the real Format Cells modal until its separate acceptance");
  for(const auto& surface:selected->value("ownedWindowScreenshots",Json::array()))require(surface.at("windowType")!=static_cast<int>(Qt::Popup),"The actual source symbol-choice checkpoint has no remaining currency popup");
  require(!QApplication::activeModalWidget()&&!QApplication::activePopupWidget(),"Separate source dialog acceptance closes the real modal and popup");
  const auto style=model->copyRange(3,2,3,2).at("cells").at(0).at(0).at("style");const bool expectedDollars=std::string(ids.first)!="historical-02789-format";require((style.at("numberFormat").get<std::string>().find('$')!=std::string::npos)==expectedDollars,"The real dialog applies the source-observed dollar or None Accounting symbol");
  require(window.executeCommand("undo")&&window.currentSession().snapshotView().at("sheets")==before.at("sheets"),"Each actual currency dialog remains one Undo and preserves numeric inputs");
 }
}
void modalFormattingCheckpoints(){
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto custom=std::string("\"FY\"yy");
 const Json children=Json::array({
  acceptanceStep("format-01-custom-category",{{"type","key_sequence"},{"target","formatCells.categoryList"},{"keys",{"End"}}}),
  acceptanceStep("format-02-select-code",{{"type","key_sequence"},{"target","formatCells.numberFormat"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("format-03-type-code",{{"type","text"},{"target","formatCells.numberFormat"},{"text",custom}}),
  acceptanceStep("format-04-accept",{{"type","key_sequence"},{"target","formatCells.dialog"},{"keys",{"Return"}}})});
 const auto step=acceptanceStep("format-date",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+1"}}}},{"steps",children}},{{"cells",Json::array({{{"address","A1"},{"style",{{"numberFormat",custom}}}}})}});
 const auto report=replay.run({{"steps",Json::array({step})}});
 require(report.at("executionComplete")==true,"Ctrl+1 replay formats through a real modal dialog: "+report.dump());
 const auto& nested=report.at("steps").at(0).at("children");require(nested.size()==4&&nested.at(0).at("before").at("modal")=="formatCells.dialog","each dialog choice and edit receives its own observed checkpoint");
 require(QFile::exists(output.filePath("format-03-type-code.modal.png")),"a checkpoint inside Format Cells saves the actual modal UI");
 require(window.currentSession().snapshotView().at("revision")==1&&window.currentSession().undo(),"the dialog edits create one native undo transaction");
}
void nativeLegacySequenceOpensModal(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());auto* grid=window.findChild<QTableView*>("worksheet.grid");const int original=grid->columnWidth(0);
 const auto children=Json::array({
  acceptanceStep("legacy-width-select",{{"type","key_sequence"},{"target","qt_spinbox_lineedit"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("legacy-width-type",{{"type","text"},{"target","qt_spinbox_lineedit"},{"text","2"}}),
  acceptanceStep("legacy-width-accept",{{"type","key_sequence"},{"target","qt_spinbox_lineedit"},{"keys",{"Return"}}})});
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("legacy-width-dialog",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Alt","O","C","W"}}}},{"steps",children}})})}});
 require(report.at("executionComplete")==true,"modal replay waits through the actual Alt,O,C,W sequence before driving the opened Column Width dialog: "+report.dump());
 require(grid->columnWidth(0)<original&&window.currentSession().snapshotView().at("revision")==1&&window.currentSession().undo(),"the real legacy width dialog creates exactly one undoable column change");
}
void nativeReplayReleasesModifiers(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 for(const auto* chord:{"Ctrl+F1","Ctrl+Shift+5","Alt+H"}){
  const auto report=replay.run({{"steps",Json::array({acceptanceStep("released-modifier-chord",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{chord}}})})}});
  require(report.at("executionComplete")==true,"the native modifier chord executes");
  require(QApplication::keyboardModifiers()==Qt::NoModifier,std::string("native replay releases every physical modifier after ")+chord);
 }
}
void nativeExternalTextClipboardBoundary(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("external-editor-open",{{"type","external_text_editor"},{"operation","open"}}),
  acceptanceStep("external-label-one",{{"type","text"},{"target","replay.externalTextEditor"},{"text","Accounts Receivable:"}}),
  acceptanceStep("external-label-newline",{{"type","key_sequence"},{"target","replay.externalTextEditor"},{"keys",{"Return"}}}),
  acceptanceStep("external-label-two",{{"type","text"},{"target","replay.externalTextEditor"},{"text","Inventory & Other:"}}),
  acceptanceStep("external-cut-list",{{"type","key_sequence"},{"target","replay.externalTextEditor"},{"keys",{"Ctrl+A","Ctrl+X"}}}),
  acceptanceStep("external-return-model",{{"type","activate_window"},{"documentName","Book1.xlsx"}}),
  acceptanceStep("external-paste-list",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+V"}}},{{"cells",Json::array({{{"address","A1"},{"input","Accounts Receivable:"}},{{"address","A2"},{"input","Inventory & Other:"}}})}}),
  acceptanceStep("external-editor-cleanup",{{"type","external_text_editor"},{"operation","close"}})
 })}});
 require(report.at("executionComplete")==true,"external text adapter types, cuts to the real clipboard, then pastes into the worksheet: "+report.dump());
 require(report.at("steps").at(4).at("after").at("externalText")==""&&QFile::exists(output.filePath("external-label-two.external.png")),"external cut state and a separate real editor screenshot remain inspectable");
 require(window.currentSession().snapshotView().at("revision")==1&&window.currentSession().undo(),"the external clipboard list creates exactly one native worksheet paste transaction");
 require(!window.findChild<QWidget*>("replay.externalTextEditor"),"the adapter closes and releases its own temporary text window");
}
void nativeUnnamedWarningModal(){
 QElapsedTimer elapsed;elapsed.start();int safetyDismissals=0;QTimer cleanup;QObject::connect(&cleanup,&QTimer::timeout,&cleanup,[&]{if(elapsed.elapsed()>2500)if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){++safetyDismissals;dialog->reject();}});cleanup.start(50);
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("warning-begin-formula",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),
  acceptanceStep("warning-incomplete-formula",{{"type","text"},{"target","worksheet.cellEditor"},{"text","SUM(1"}}),
  acceptanceStep("warning-actual-modal",{{"type","modal"},{"dialogSequence",true},{"open",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}}},{"steps",Json::array({acceptanceStep("correction-information-visible",{{"type","key_sequence"},{"target","@active_modal"},{"keys",Json::array()}}),acceptanceStep("correction-decline",{{"type","key_sequence"},{"target","@active_modal"},{"keys",{"Escape"}}}),acceptanceStep("parenthesis-warning-visible",{{"type","key_sequence"},{"target","@active_modal"},{"keys",Json::array()}}),acceptanceStep("warning-dismiss-actual-modal",{{"type","key_sequence"},{"target","@active_modal"},{"keys",{"Return"}}})})}},{{"formulaDraft","=SUM(1"},{"revision",0}}),
  acceptanceStep("warning-cancel-draft",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Escape"}}})
 })}});
 cleanup.stop();require(safetyDismissals==0&&report.at("executionComplete")==true,"one original Return must drive real correction-No then parenthesis-OK dialogs in sequence without an emergency dismissal: "+report.dump());
 const auto& children=report.at("steps")[2].at("children");require(children.size()==4&&children[0].contains("modalScreenshot")&&children[2].contains("modalScreenshot")&&QImage(output.filePath("correction-information-visible.modal.png"))!=QImage(output.filePath("parenthesis-warning-visible.modal.png")),"both distinct real dialogs receive independent observed checkpoints");
 require(QFile::exists(output.filePath("warning-dismiss-actual-modal.png"))&&window.currentSession().snapshotView().at("revision")==0,"warning dismissal/cancellation preserves the untouched worksheet");
}
void nativeCalculatedValueCheckpoints(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("value-begin-formula",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),
  acceptanceStep("value-type-formula",{{"type","text"},{"target","worksheet.cellEditor"},{"text","1+2"}}),
  acceptanceStep("value-commit-formula",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}},{{"cells",Json::array({{{"address","A1"},{"input","=1+2"},{"value",3},{"display","3"}}})}})
 })}});
 require(report.at("executionComplete")==true,"a native formula reaches its calculated numeric/display checkpoint");
 const auto badValue=replay.run({{"steps",Json::array({acceptanceStep("wrong-numeric-checkpoint",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}},{{"cells",Json::array({{{"address","A1"},{"value",4}}})}})})}});
 require(badValue.at("executionComplete")==false,"native replay rejects a wrong calculated-value checkpoint instead of ignoring it");
 const auto badDisplay=replay.run({{"steps",Json::array({acceptanceStep("wrong-display-checkpoint",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}},{{"cells",Json::array({{{"address","A1"},{"display","wrong"}}})}})})}});
 require(badDisplay.at("executionComplete")==false,"native replay rejects a wrong displayed-value checkpoint");
 for(const auto& [row,formula,value,type]:{std::tuple{2,"=1+\"x\"",Json("#VALUE!"),"error"},std::tuple{3,"=1=1",Json(true),"boolean"},std::tuple{4,"=\"text\"",Json("text"),"text"}}){
  auto* grid=window.findChild<QTableView*>("worksheet.grid");grid->setCurrentIndex(grid->model()->index(row-1,0));
  const auto typed=replay.run({{"steps",Json::array({acceptanceStep("typed-value-begin",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),acceptanceStep("typed-value-input",{{"type","text"},{"target","worksheet.cellEditor"},{"text",std::string(formula).substr(1)}}),acceptanceStep("typed-value-commit",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}},{{"cells",Json::array({{{"address","A"+std::to_string(row)},{"value",value},{"valueType",type}}})}})})}});
  require(typed.at("executionComplete")==true,std::string("native checkpoint retains typed calculation result: ")+type+" "+typed.dump());
 }
}
void nativeNumericToleranceCheckpoints(){
 QTemporaryDir output;MainWindow window;window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto entered=replay.run({{"steps",Json::array({acceptanceStep("tolerance-begin",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),acceptanceStep("tolerance-input",{{"type","text"},{"target","worksheet.cellEditor"},{"text","0.1+0.2"}}),acceptanceStep("tolerance-commit",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}})})}});
 require(entered.at("executionComplete")==true,"native tolerance fixture is entered through actual formula keys");
 auto check=[&](const char* id,Json cell){cell["address"]="A1";return replay.run({{"steps",Json::array({acceptanceStep(id,{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}},{{"cells",Json::array({cell})}})})}});};
 const auto withinAbsolute=check("within-absolute-tolerance",{{"value",0.300000004},{"valueType","number"},{"valueTolerance",{{"absolute",1e-8},{"relative",1e-12}}}});
 require(withinAbsolute.at("executionComplete")==true,"explicit numeric tolerance accepts an independently specified nearby value without changing the native result: "+withinAbsolute.dump());
 require(check("within-relative-tolerance",{{"value",0.3000000000001},{"valueType","number"},{"valueTolerance",{{"absolute",0},{"relative",1e-12}}}}).at("executionComplete")==true,"relative tolerance scales with the observed and expected magnitudes");
 require(check("outside-numeric-tolerance",{{"value",0.31},{"valueTolerance",{{"absolute",1e-8},{"relative",1e-12}}}}).at("executionComplete")==false,"numeric tolerance never accepts a materially different value");
 require(check("exact-value-stays-exact",{{"value",0.300000004}}).at("executionComplete")==false,"omitted tolerance retains exact numeric comparisons including source-exact zero checks");
 require(check("typed-tolerance-stays-number",{{"value","0.3"},{"valueTolerance",{{"absolute",1e-8}}}}).at("executionComplete")==false,"numeric tolerances cannot coerce text or other result types");
 require(check("invalid-numeric-tolerance",{{"value",0.3},{"valueTolerance",{{"absolute",-1}}}}).at("executionComplete")==false,"negative numeric tolerance is rejected");
 require(window.currentSession().cellAt("sheet-1",1,1)->at("input")=="=0.1+0.2"&&window.currentSession().snapshotView().at("revision")==1,"read-only tolerant checkpoints preserve the actual formula and history");
}
void nativeObservationPreservesDraftFocus(){
 QTemporaryDir output;MainWindow window;window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());Json steps=Json::array();int id=0;
 auto action=[&](Json a,Json expected=Json::object()){const auto name="draft-observe-"+std::to_string(++id);steps.push_back(acceptanceStep(name.c_str(),a,expected));};
 auto key=[&](const char* target,Json keys,Json expected=Json::object()){action({{"type","key_sequence"},{"target",target},{"keys",keys}},expected);};
 auto go=[&](const char* cell){key("formula.name",{"Ctrl+A"});action({{"type","text"},{"target","formula.name"},{"text",cell}});key("formula.name",{"Return"});};
 auto text=[&](const char* target,const char* value){action({{"type","text"},{"target",target},{"text",value}});};
 go("A1");text("worksheet.grid","1");key("worksheet.cellEditor",{"Return"});go("B1");text("worksheet.grid","2");key("worksheet.cellEditor",{"Return"});go("C1");text("worksheet.grid","=");
 action({{"type","mouse_cell"},{"address","A1"},{"gesture","click"}});text("worksheet.cellEditor","-");key("worksheet.grid",Json::array(),{{"formulaDraft","=A1-"}});
 action({{"type","mouse_cell"},{"address","B1"},{"gesture","click"}});text("worksheet.cellEditor","+3");key("worksheet.cellEditor",{"Return"},{{"cells",Json::array({{{"address","C1"},{"input","=A1-B1+3"},{"value",2}}})}});
 const auto report=replay.run({{"steps",steps}});require(report.at("executionComplete")==true,"a read-only empty-key checkpoint preserves the actual editor focus and subsequent pointer insertion: "+report.dump());
}
void nativeHorizontalViewportNavigation(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());auto* grid=window.findChild<QTableView*>("worksheet.grid");
 const auto end=replay.run({{"steps",Json::array({acceptanceStep("horizontal-select-whole-row",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Shift+Space"}}}),acceptanceStep("horizontal-native-end",{{"type","horizontal_view"},{"edge","end"}})})}});
 require(end.at("executionComplete")==true,"native horizontal scrollbar End exposes the last worksheet columns: "+end.dump());
 require(grid->columnAt(0)>16000&&grid->viewport()->rect().intersects(grid->visualRect(grid->model()->index(0,grid->model()->columnCount()-1))),"native End exposes XFD rather than inventing a viewport state");
 const auto exact=replay.run({{"steps",Json::array({acceptanceStep("horizontal-native-k",{{"type","horizontal_view"},{"firstColumn","K"}},{{"firstColumn",11},{"row",1},{"column",1},{"revision",0}})})}});
 require(exact.at("executionComplete")==true,"real native scrollbar/wheel events place K at the left edge: "+exact.dump());
 const auto selection=grid->selectionModel()->selection();require(selection.size()==1&&selection.front().width()==grid->model()->columnCount()&&selection.front().height()==1,"viewport navigation preserves the whole-row selection and current cell");
 require(!exact.at("steps").at(0).at("wheelEvents").empty(),"every native wheel event used for the target viewport is retained");
}
void nativeVerticalViewportNavigation(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("vertical-select-name",{{"type","key_sequence"},{"target","formula.name"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("vertical-type-anchor",{{"type","text"},{"target","formula.name"},{"text","B53"}}),
  acceptanceStep("vertical-select-anchor",{{"type","key_sequence"},{"target","formula.name"},{"keys",{"Return"}}}),
  acceptanceStep("vertical-source-row52",{{"type","vertical_view"},{"topRow",52}},{{"topRow",52},{"row",53},{"column",2},{"revision",0}}),
  acceptanceStep("vertical-return-first-row",{{"type","vertical_view"},{"topRow",1}},{{"topRow",1},{"row",53},{"column",2},{"revision",0}})
 })}});
 require(report.at("executionComplete")==true,"native scrollbar and wheel navigation reproduces exact top rows without moving the inspection anchor: "+report.dump());
 require(!report.at("steps").at(3).at("wheelEvents").empty(),"source viewport scroll retains every real wheel event in the trace");
}
void nativeViewportAlignment(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());auto* grid=window.findChild<QTableView*>("worksheet.grid");
 const auto partial=replay.run({{"steps",Json::array({acceptanceStep("viewport-partial-row",{{"type","wheel"},{"target","worksheet.grid"},{"angleDelta",{0,-35}}})})}});
 require(partial.at("executionComplete")==true&&grid->rowAt(0)==0&&grid->rowViewportPosition(0)<0,"native fine wheel establishes an actually clipped first row");
 const auto firstAligned=replay.run({{"steps",Json::array({acceptanceStep("viewport-align-first-row",{{"type","vertical_view"},{"topRow",1}})})}});
 require(firstAligned.at("executionComplete")==true&&grid->rowViewportPosition(0)==0,"native topRow1 is completely visible rather than accepted with a clipped top edge");
 const auto aligned=replay.run({{"steps",Json::array({acceptanceStep("viewport-align-row52",{{"type","vertical_view"},{"topRow",52}})})}});
 const auto first=QImage(output.filePath("viewport-align-first-row.png"));require(!first.isNull(),"aligned viewport has a native checkpoint");
 require(aligned.at("executionComplete")==true&&grid->rowViewportPosition(51)==0,"requested top row52 is fully visible at pixel0; actual offset="+std::to_string(grid->rowViewportPosition(51)));require(grid->currentIndex()==grid->model()->index(0,0)&&window.currentSession().snapshotView().at("revision")==0,"native viewport alignment preserves the active cell and workbook");
}
void sourceObservationGate(){
 QTemporaryDir output;MainWindow window;window.show();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto acceptance=acceptanceStep("capability-only",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Alt","Escape"}}});
 auto report=replay.run({{"steps",Json::array({acceptance})}});
 require(report.at("complete")==false,"passing synthetic acceptance steps cannot claim complete source-video reproduction");
 require(report.at("executionComplete")==true,"successful capability execution is reported separately from source coverage");
 auto caption=acceptance;caption["id"]="unobserved-caption";caption["evidence"]["kind"]="transcript";caption["action"]["keys"]={"Alt","W","V","G"};
 const auto before=window.currentSession().snapshot();report=replay.run({{"sourceReviewComplete",true},{"steps",Json::array({caption})}});
 require(report.at("complete")==false&&report.at("executionComplete")==false&&report.at("steps").at(0).at("status")=="pending"&&window.currentSession().snapshot()==before,"caption-only actions remain unexecuted even when a manifest requests source completion");
}
void nestedSourceObservationGate(){
 QTemporaryDir output;MainWindow window;window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 auto step=acceptanceStep("observed-dialog",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+1"}}}},{"steps",Json::array({acceptanceStep("unobserved-dialog-dismissal",{{"type","key_sequence"},{"target","formatCells.dialog"},{"keys",{"Escape"}}})})}});
 step["evidence"]={{"kind","visual"},{"reference","Synthetic visually marked parent for recursive coverage regression"}};
 const auto report=replay.run({{"sourceReviewComplete",true},{"steps",Json::array({step})}});
 require(report.at("executionComplete")==true,"the native dialog and its acceptance dismissal execute successfully");
 require(report.at("complete")==false,"a visually verified modal parent cannot hide an acceptance-only child and claim full source coverage");
}
void nativeNameBoxObservationTracksHeldSelection(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();require(window.executeCommand("view.ribbon"),"collapse the real source ribbon");auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* name=window.findChild<QLineEdit*>("formula.name");require(model->setData(model->index(26,1),"Anchor",Qt::EditRole),"seed a real anchor formula-bar value");view->setCurrentIndex(model->index(26,1));view->scrollTo(model->index(19,1),QAbstractItemView::PositionAtTop);view->setFocus();QApplication::processEvents();const auto before=window.currentSession().snapshot();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("name-box-native-press",{{"type","mouse_cell"},{"address","B27"},{"gesture","press"}},{{"nameBoxText","B27"},{"formulaDraft","Anchor"},{"row",27},{"column",2}}),
  acceptanceStep("name-box-native-held-drag",{{"type","mouse_cell"},{"address","B32"},{"gesture","move"}},{{"nameBoxText","6R x 1C"},{"formulaDraft","Anchor"},{"row",27},{"column",2}}),
  acceptanceStep("name-box-native-release",{{"type","mouse_cell"},{"address","B32"},{"gesture","release"}},{{"nameBoxText","B27"},{"formulaDraft","Anchor"},{"row",27},{"column",2}})
 })}});require(report.at("executionComplete")==true,"Replay reads actual anchor/live-dimension/release Name Box text: "+report.dump());require(report.at("steps")[1].at("after").at("nameBoxText")=="6R x 1C"&&name->text()=="B27"&&window.currentSession().snapshot()==before,"read-only observations preserve native selection lifecycle, anchor and workbook history");
 const auto wrong=replay.run({{"steps",Json::array({acceptanceStep("name-box-wrong-header-rejected",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}},{{"nameBoxText","A100"}})})}});require(wrong.at("executionComplete")==false&&name->text()=="B27"&&window.currentSession().snapshot()==before,"an incorrect expected header fails without writing the expected text into native UI or workbook");
}
void nativeHeldShiftCountsPreserveCopyState(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();window.executeCommand("view.ribbon");auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(view->model());auto* name=window.findChild<QLineEdit*>("formula.name");require(model->setData(model->index(31,5),"10",Qt::EditRole),"seed a real copied source range");view->setCurrentIndex(model->index(31,5));view->selectionModel()->select(QItemSelection(model->index(31,5),model->index(45,5)),QItemSelectionModel::ClearAndSelect);view->scrollTo(model->index(29,5),QAbstractItemView::PositionAtTop);view->setFocus();QApplication::processEvents();const auto before=window.currentSession().snapshot();
 class ShiftEvents final:public QObject {public:int releases=0;bool eventFilter(QObject*,QEvent* event)override{if(event->type()==QEvent::KeyRelease&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Shift)++releases;return false;}} shiftEvents;view->installEventFilter(&shiftEvents);testing::NativeActionReplay replay(window,output.path());Json steps=Json::array({acceptanceStep("held-shift-copy",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+C"}}})});
 for(int width=2;width<=5;++width){const auto id="held-shift-width-"+std::to_string(width);steps.push_back(acceptanceStep(id.c_str(),{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Shift+Right"}},{"releaseModifiers",false}},{{"nameBoxText","15R x "+std::to_string(width)+"C"},{"formulaDraft","10"},{"row",32},{"column",6}}));}
 const auto held=replay.run({{"steps",steps}});require(held.at("executionComplete")==true,"Actual Shift extensions retain every source count before modifier release: "+held.dump());auto* copy=view->viewport()->findChild<QWidget*>("worksheet.copyBorder");require(shiftEvents.releases==0&&name->text()=="15R x 5C"&&copy&&copy->isVisible()&&window.currentSession().snapshot()==before,"held Shift emits no premature release and preserves actual marching ants, anchor and workbook data");
 const auto invalid=replay.run({{"steps",Json::array({acceptanceStep("held-shift-invalid-release",{{"type","modifier_release"},{"target","worksheet.grid"},{"keys",{"Shift","Down"}}})})}});require(invalid.at("executionComplete")==false&&shiftEvents.releases==0&&name->text()=="15R x 5C"&&window.currentSession().snapshot()==before,"invalid modifier lists fail before partially releasing held native state");
 const auto released=replay.run({{"steps",Json::array({acceptanceStep("held-shift-release",{{"type","modifier_release"},{"target","worksheet.grid"},{"keys",{"Shift"}}},{{"nameBoxText","F32"}})})}});require(released.at("executionComplete")==true&&shiftEvents.releases==1&&copy->isVisible()&&window.currentSession().snapshot()==before,"explicit native Shift release restores the anchor header without cancelling copied range or creating history");
}
void nativePointRangeHintObservation(){
 QTemporaryDir output;MainWindow origin,source;origin.resize(1280,720);source.resize(1280,720);origin.show();source.show();QApplication::processEvents();auto* sourceView=source.findChild<WorksheetView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(sourceView->model());auto* bar=origin.findChild<QLineEdit*>("formula.input");require(model->setData(model->index(6,0),"Prior source cell")&&source.saveWorkbook(output.filePath("Point Hint.xlsx")),"prepare independent real Point source");sourceView->setCurrentIndex(model->index(6,0));sourceView->scrollTo(sourceView->currentIndex());const auto sourceBefore=source.currentSession().snapshot(),originBefore=origin.currentSession().snapshot();origin.activateWindow();bar->setFocus();QTest::keyClicks(bar,"=SUM(");QTest::keyClick(bar,Qt::Key_Tab,Qt::ControlModifier);QTest::keyRelease(source.findChild<QLineEdit*>("formula.input"),Qt::Key_Control);QApplication::processEvents();testing::NativeActionReplay replay(source,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("point-hint-native-press",{{"type","mouse_cell"},{"address","B10"},{"gesture","press"}},{{"nameBoxText","A7"},{"row",7},{"column",1},{"pointRangeHint",{{"visible",false},{"text",""}}}}),
  acceptanceStep("point-hint-native-held-range",{{"type","mouse_cell"},{"address","B12"},{"gesture","move"}},{{"nameBoxText","A7"},{"row",7},{"column",1},{"pointRangeHint",{{"visible",true},{"text","3R x 1C"}}}}),
  acceptanceStep("point-hint-native-release",{{"type","mouse_cell"},{"address","B12"},{"gesture","release"}},{{"nameBoxText","A7"},{"pointRangeHint",{{"visible",false},{"text","3R x 1C"}}}})
 })}});require(report.at("executionComplete")==true,"Replay reads actual held/released external Point label and unchanged source anchor: "+report.dump());auto* hint=source.findChild<QLabel*>("worksheet.pointRangeHint");require(hint&&hint->text()=="3R x 1C"&&!hint->isVisible()&&source.currentSession().snapshot()==sourceBefore&&origin.currentSession().snapshot()==originBefore,"Point hint observation preserves actual native label, both histories and pending formula");
 const auto wrong=replay.run({{"steps",Json::array({acceptanceStep("point-hint-wrong-state-rejected",{{"type","key_sequence"},{"target","formula.input"},{"keys",Json::array()}},{{"pointRangeHint",{{"visible",true},{"text","8R x 2C"}}}})})}});require(wrong.at("executionComplete")==false&&hint->text()=="3R x 1C"&&!hint->isVisible()&&source.currentSession().snapshot()==sourceBefore&&origin.currentSession().snapshot()==originBefore,"incorrect Point dimensions fail without writing expected label text or changing native state");QTest::keyClick(source.findChild<QLineEdit*>("formula.input"),Qt::Key_Escape);QApplication::processEvents();
}
void nativeStatusMessageObservationProvesRefresh(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* view=window.findChild<WorksheetView*>("worksheet.grid");view->setFocus();const auto before=window.currentSession().snapshot();require(window.executeCommand("links.refresh"),"execute actual local-source refresh");testing::NativeActionReplay replay(window,output.path());
 const auto actual=replay.run({{"steps",Json::array({acceptanceStep("read-actual-refresh-result",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}},{{"statusBarMessage","Refreshed 0 local workbook files"}})})}});require(actual.at("executionComplete")==true,"replay observes the actual native Refresh result: "+actual.dump());
 const auto wrong=replay.run({{"steps",Json::array({acceptanceStep("reject-unread-source-claim",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}},{{"statusBarMessage","Refreshed 1 local workbook files"}})})}});require(wrong.at("executionComplete")==false&&wrong.at("steps")[0].at("after").at("statusBarMessage")=="Refreshed 0 local workbook files"&&window.currentSession().snapshot()==before,"an incorrect file-read claim fails without replacing status text or changing workbook history");
}
void nativeCompletedModalStopsItsTimeout(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();window.findChild<QTableView*>("worksheet.grid")->setFocus();const auto before=window.currentSession().snapshot();testing::NativeActionReplay replay(window,output.path());
 const auto finished=replay.run({{"steps",Json::array({acceptanceStep("modal-completed-before-ui-observation",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+1"}}}},{"steps",Json::array({acceptanceStep("close-actual-format-modal",{{"type","key_sequence"},{"target","formatCells.dialog"},{"keys",{"Escape"}}}),acceptanceStep("observe-real-ui-after-modal-closed",{{"type","mouse"},{"target","formula.insertFunction"},{"gesture","hover"},{"dwellMs",3000}}),acceptanceStep("observe-real-ui-after-modal-closed-again",{{"type","mouse"},{"target","formula.insertFunction"},{"gesture","hover"},{"dwellMs",3000}})})}})})}});
 require(finished.at("executionComplete")==true,"a completed native modal cannot time out during subsequent real UI observation: "+finished.dump());require(!QApplication::activeModalWidget()&&window.currentSession().snapshot()==before,"completed-dialog observation preserves cells and history");
 const auto unfinished=replay.run({{"steps",Json::array({acceptanceStep("modal-still-open-remains-bounded",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+1"}}}},{"steps",Json::array({acceptanceStep("observe-modal-without-closing",{{"type","mouse"},{"target","formatCells.dialog"},{"gesture","hover"},{"dwellMs",3000}}),acceptanceStep("observe-modal-still-without-closing",{{"type","mouse"},{"target","formatCells.dialog"},{"gesture","hover"},{"dwellMs",3000}})})}})})}});
 require(unfinished.at("executionComplete")==false&&unfinished.at("steps")[0].value("nestedError",std::string())=="Modal replay timed out"&&!QApplication::activeModalWidget()&&window.currentSession().snapshot()==before,"the actual still-open modal keeps its timeout and is natively dismissed without workbook changes");QToolTip::hideText();
}
void nativeCellPointingAndDrag(){
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());Json steps=Json::array();int id=0;
 auto action=[&](const Json& a,const Json& expected=Json::object()){const auto name="pointer-"+std::to_string(++id);steps.push_back(acceptanceStep(name.c_str(),a,expected));};
 auto key=[&](const char* target,const Json& keys,const Json& expected=Json::object()){action({{"type","key_sequence"},{"target",target},{"keys",keys}},expected);};
 auto text=[&](const char* target,const char* value){action({{"type","text"},{"target",target},{"text",value}});};
 auto go=[&](const char* cell){key("formula.name",{"Ctrl+A"});text("formula.name",cell);key("formula.name",{"Return"});};
 auto input=[&](const char* value){key("formula.input",{"Ctrl+A"});text("formula.input",value);key("formula.input",{"Return"});};
 go("A1");input("2");go("B1");input("3");go("D5");key("formula.input",{"Ctrl+A"});text("formula.input","=");
 action({{"type","mouse_cell"},{"address","A1"},{"gesture","click"}});text("formula.input","+");
 action({{"type","mouse_cell"},{"address","B1"},{"gesture","click"}});
 key("formula.input",{"Return"},{{"cells",Json::array({{{"address","D5"},{"input","=A1+B1"}}})}});
 action({{"type","mouse_cell"},{"address","A1"},{"gesture","press"}});
 action({{"type","mouse_cell"},{"address","C1"},{"gesture","move"}});
 action({{"type","mouse_cell"},{"address","C1"},{"gesture","release"}});
 key("worksheet.grid",{"Ctrl+R"},{{"cells",Json::array({{{"address","A1"},{"input","2"}},{{"address","B1"},{"input","2"}},{{"address","C1"},{"input","2"}},{{"address","D5"},{"input","=A1+B1"}}})}});
 const auto report=replay.run({{"steps",steps}});
 require(report.at("executionComplete")==true,"native pointer replay inserts actual cell references and drags a range for fill-right: "+report.dump());
 require(window.currentSession().undo()&&window.currentSession().cellAt("sheet-1",1,2)->at("input")=="3"&&!window.currentSession().cellAt("sheet-1",1,3),"one native undo reverses the real drag-selected fill transaction");
}
void nativeWheelDuringFormulaEntry(){
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto before=window.currentSession().snapshot();
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("wheel-formula-draft",{{"type","text"},{"target","formula.input"},{"text","=SUM("}}),
  acceptanceStep("wheel-down",{{"type","wheel"},{"target","worksheet.grid"},{"angleDelta",{0,-480}}},{{"row",1},{"column",1},{"formulaDraft","=SUM("}})})}});
 require(report.at("executionComplete")==true,"native wheel replay scrolls during an unfinished formula without committing or moving the active cell: "+report.dump());
 require(grid->verticalScrollBar()->value()>0&&window.currentSession().snapshot()==before,"real viewport wheel scrolls without editing workbook data");
 require(report.at("steps").at(1).at("after").at("topRow").get<int>()>1,"native trace captures the actual first visible worksheet row");
}
void nativeWheelToDistantFormulaReference(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("distant-reference-begin",{{"type","text"},{"target","formula.input"},{"text","="}}),
  acceptanceStep("distant-reference-scroll",{{"type","wheel_to_cell"},{"address","A80"}},{{"row",1},{"column",1},{"formulaDraft","="}}),
  acceptanceStep("distant-reference-point",{{"type","mouse_cell"},{"address","A80"},{"gesture","click"}},{{"formulaDraft","=A80"}}),
  acceptanceStep("distant-reference-continue",{{"type","text"},{"target","formula.input"},{"text","+1"}}),
  acceptanceStep("distant-reference-commit",{{"type","key_sequence"},{"target","formula.input"},{"keys",{"Return"}}},{{"cells",Json::array({{{"address","A1"},{"input","=A80+1"}}})}})})}});
 require(report.at("executionComplete")==true,"native wheel navigation exposes a distant formula reference for an actual pointer click: "+report.dump());
 require(!report.at("steps").at(1).at("wheelEvents").empty(),"every native wheel event used to expose the reference is retained in the trace");
 require(window.currentSession().snapshotView().at("revision")==1,"scrolling and pointing preserve the original cell until its single real commit");
}
void nativeFormatTabAndCheckboxClicks(){
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto children=Json::array({
  acceptanceStep("pointer-font-tab",{{"type","mouse_tab"},{"target","formatCells.tabs"},{"tab","Font"}}),
  acceptanceStep("pointer-font-bold",{{"type","mouse"},{"target","formatCells.dialog"},{"childAccessibleName","Bold"},{"gesture","click"}}),
  acceptanceStep("pointer-font-color-select",{{"type","key_sequence"},{"target","formatCells.color"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("pointer-font-color-type",{{"type","text"},{"target","formatCells.color"},{"text","#0000FF"}}),
  acceptanceStep("pointer-font-accept",{{"type","key_sequence"},{"target","formatCells.dialog"},{"keys",{"Return"}}})});
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("pointer-format-dialog",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+1"}}}},{"steps",children}},{{"cells",Json::array({{{"address","A1"},{"style",{{"bold",true},{"color","#0000ff"}}}}})}})})}});
 require(report.at("executionComplete")==true,"native replay clicks a real Font tab and Bold checkbox and commits the actual dialog: "+report.dump());
 require(window.currentSession().snapshotView().at("revision")==1&&window.currentSession().undo(),"the clicked formatting choices create one native undo transaction");
}
void nativeInCellDraftSequence(){
 QTemporaryDir output;MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("cell-draft-begin",{{"type","text"},{"target","worksheet.grid"},{"text","="}},{{"formulaDraft","="}}),
  acceptanceStep("cell-draft-continue",{{"type","text"},{"target","worksheet.cellEditor"},{"text","1+2"}},{{"formulaDraft","=1+2"}}),
  acceptanceStep("cell-draft-commit",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}},{{"cells",Json::array({{{"address","A1"},{"input","=1+2"}}})}}),
  acceptanceStep("cell-draft-second-cell",{{"type","mouse_cell"},{"address","B2"},{"gesture","click"}}),
  acceptanceStep("cell-draft-second-begin",{{"type","text"},{"target","worksheet.grid"},{"text","a"}}),
  acceptanceStep("cell-draft-second-continue",{{"type","text"},{"target","worksheet.cellEditor"},{"text","b"}}),
  acceptanceStep("cell-draft-second-commit",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}},{{"cells",Json::array({{{"address","B2"},{"input","ab"}}})}})})}});
 require(report.at("executionComplete")==true,"native replay types through successive in-cell editors and mirrors each formula draft: "+report.dump());
 require(window.currentSession().snapshotView().at("revision")==2,"two in-cell commits create exactly two workbook transactions");
}
void nativeClipboardConflictBoundary(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("clipboard-type-source",{{"type","text"},{"target","worksheet.grid"},{"text","7"}}),
  acceptanceStep("clipboard-commit-source",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return","Up"}}}),
  acceptanceStep("clipboard-conflict-copy",{{"type","modal"},{"clipboardOwnershipConflict",true},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+C"}}}},{"steps",Json::array({acceptanceStep("clipboard-warning-ok",{{"type","key_sequence"},{"target","@active_modal"},{"keys",{"Return"}}})})}}),
  acceptanceStep("clipboard-next-cell",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Right"}}}),
  acceptanceStep("clipboard-local-paste",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+V"}}},{{"cells",Json::array({{{"address","B1"},{"input","7"}}})}})})}});
 require(report.at("executionComplete")==true,"scoped external clipboard ownership conflict executes the actual native warning and retains local paste: "+report.dump());require(report.at("steps")[2].at("clipboardOwnershipReplaced")==true,"trace proves actual system clipboard ownership was replaced only at the source warning boundary");require(window.currentSession().undo()&&!window.currentSession().cellAt("sheet-1",1,2),"local fallback paste remains one native Undo transaction");
}
void nativeDocumentContextPreservesDraft(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("draft-begin",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),
  acceptanceStep("draft-reference",{{"type","text"},{"target","worksheet.cellEditor"},{"text","C1"}}),
  acceptanceStep("draft-open-pdf",{{"type","document_context"},{"operation","open"},{"path",std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-Investor-Presentation.pdf"}},{{"formulaDraft","=C1"},{"revision",0}}),
  acceptanceStep("draft-return-model",{{"type","document_context"},{"operation","hide"}},{{"formulaDraft","=C1"},{"revision",0}}),
  acceptanceStep("draft-cancel",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Escape"}}},{{"revision",0}})})}});
 require(report.at("executionComplete")==true,"actual external PDF context switch preserves an uncommitted formula and returns it without a model transaction: "+report.dump());
}
void nativePresentationBuildActions(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("presentation-open",{{"type","document_context"},{"operation","open"},{"path",std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-90-Minute-Blank-Sheet-Case-Study-Slides.pdf"}}),
  acceptanceStep("presentation-initial-mask",{{"type","presentation_builds"},{"operation","set"},{"target","context.surface"},{"masks",Json::array({Json::array({Json::array({80,100,700,300})}),Json::array()})}}),
  acceptanceStep("presentation-reveal",{{"type","presentation_builds"},{"operation","advance"},{"target","context.surface"}}),
  acceptanceStep("presentation-clear",{{"type","presentation_builds"},{"operation","clear"},{"target","context.surface"}})})}});
 require(report.at("executionComplete")==true,"native replay advances a disclosed slide build over actual PDF page pixels: "+report.dump());require(report.at("steps")[1].at("presentationStage")==0&&report.at("steps")[2].at("presentationStage")==1,"trace records real presentation stage properties after native Space/Right handling");require(window.currentSession().snapshotView()==before,"read-only presentation masks never change the workbook");
}
void nativeDocumentContextActions(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("context-open-source",{{"type","document_context"},{"operation","open"},{"path",std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-90-Minute-Case-Study.pdf"}}),
  acceptanceStep("context-query",{{"type","text"},{"target","context.find"},{"text","Revenue Projections"}}),
  acceptanceStep("context-find",{{"type","key_sequence"},{"target","context.find"},{"keys",{"Return"}}},{{"externalDocument",{{"page",1},{"selectedText","Revenue Projections"}}}}),
  acceptanceStep("context-native-scroll",{{"type","scroll_view"},{"target","context.scroll"},{"topPixel",200}},{{"externalDocument",{{"scrollY",200},{"selectedText","Revenue Projections"}}}}),
  acceptanceStep("context-hide",{{"type","document_context"},{"operation","hide"}},{{"externalDocument",{{"visible",false},{"selectedText","Revenue Projections"}}}}),
  acceptanceStep("context-restore",{{"type","document_context"},{"operation","activate"}},{{"externalDocument",{{"visible",true},{"selectedText","Revenue Projections"}}}}),
  acceptanceStep("context-close",{{"type","document_context"},{"operation","close"}})})}});
 require(report.at("executionComplete")==true,"native replay opens and operates a real read-only PDF context: "+report.dump());const auto& state=report.at("steps")[2].at("after").at("externalDocument");require(state.at("selectedText")=="Revenue Projections"&&state.at("pageCount")==1,"trace includes actual source text selection and page count");require(QFile::exists(output.filePath("context-find.context.png")),"document context receives its own actual rendered checkpoint");require(window.currentSession().snapshotView()==before,"context navigation never mutates the workbook");
}
void nativeDocumentContextAcrossWorkbookWindows(){
 QTemporaryDir files;QFile source(files.filePath("source.csv"));require(source.open(QIODevice::WriteOnly),"Create native CSV boundary fixture");source.write("Source\n7\n");source.close();MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,files.filePath("evidence"));
 const auto report=replay.run({{"windowSize",{1280,720}},{"steps",Json::array({
  acceptanceStep("context-windows-open-source",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+O"}}}},{"steps",Json::array({acceptanceStep("context-windows-file-select",{{"type","key_sequence"},{"target","fileNameEdit"},{"keys",{"Ctrl+A"}}}),acceptanceStep("context-windows-file-name",{{"type","text"},{"target","fileNameEdit"},{"text",source.fileName().toStdString()}}),acceptanceStep("context-windows-file-confirm",{{"type","key_sequence"},{"target","fileNameEdit"},{"keys",{"Return"}}})})}}),
  acceptanceStep("context-windows-new-model",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+N"}}}),
  acceptanceStep("context-windows-open-pdf",{{"type","document_context"},{"operation","open"},{"path",std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-Investor-Presentation.pdf"}}),
  acceptanceStep("context-windows-inspect-raw",{{"type","activate_window"},{"documentName","source.csv"}}),
  acceptanceStep("context-windows-hide-pdf",{{"type","document_context"},{"operation","hide"}},{{"externalDocument",{{"visible",false}}},{"documentName","source.csv"},{"cells",Json::array({{{"address","A2"},{"input","7"}}})}}),
  acceptanceStep("context-windows-return-model",{{"type","activate_window"},{"documentName","Book1.xlsx"}}),
  acceptanceStep("context-windows-close-pdf",{{"type","document_context"},{"operation","close"}})
 })}});
 QList<QPointer<MainWindow>> others;for(auto* widget:QApplication::topLevelWidgets())if(auto* other=dynamic_cast<MainWindow*>(widget);other&&other!=&window)others.push_back(other);for(auto other:others)if(other)delete other;
 require(report.at("executionComplete")==true,"retained PDF context survives native source/model window activation and closes independently: "+report.dump());
}
void nativeDocumentImageSelectionActions(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto workbook=window.currentSession().snapshotView();
 const auto report=replay.run({{"windowSize",{1280,720}},{"steps",Json::array({
  acceptanceStep("image-context-open",{{"type","document_context"},{"operation","open"},{"path",std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-Investor-Presentation.pdf"}}),
  acceptanceStep("image-context-page-select",{{"type","key_sequence"},{"target","context.page"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("image-context-page-type",{{"type","text"},{"target","context.page"},{"text","41"}}),
  acceptanceStep("image-context-page-commit",{{"type","key_sequence"},{"target","context.page"},{"keys",{"Return"}}}),
  acceptanceStep("image-context-select",{{"type","document_image_selection"},{"operation","select"},{"target","context.surface"},{"rect",{730,174,510,464}},{"toolbarRect",{793,130,242,43}}}),
  acceptanceStep("image-context-clear",{{"type","document_image_selection"},{"operation","clear"},{"target","context.surface"}}),
  acceptanceStep("image-context-close",{{"type","document_context"},{"operation","close"}})
 })}});
 require(report.at("executionComplete")==true,"native replay executes genuine read-only PDF image selection and its graphical toolbar: "+report.dump());
 const auto selected=QImage(output.filePath("image-context-select.context.png")),cleared=QImage(output.filePath("image-context-clear.context.png"));require(!selected.isNull()&&!cleared.isNull()&&selected!=cleared,"selection and actual Escape dismissal have separately captured original-PDF states");require(window.currentSession().snapshotView()==workbook,"source image selection never changes the worksheet");
}
void nativeAnnotationActions(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto active=grid->currentIndex();
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("annotation-range",{{"type","annotation"},{"operation","rectangle"},{"target","worksheet.grid"},{"cellRange","B4:C5"},{"color","#0000FF"},{"lineWidth",3}}),acceptanceStep("annotation-clear",{{"type","annotation"},{"operation","clear"},{"target","worksheet.grid"}}),acceptanceStep("annotation-held-press",{{"type","annotation"},{"operation","press"},{"target","worksheet.grid"},{"point",{220,150}}}),acceptanceStep("annotation-held-move",{{"type","annotation"},{"operation","move"},{"target","worksheet.grid"},{"point",{420,160}}}),acceptanceStep("annotation-held-expand",{{"type","annotation"},{"operation","move"},{"target","worksheet.grid"},{"point",{600,250}}}),acceptanceStep("annotation-held-release",{{"type","annotation"},{"operation","release"},{"target","worksheet.grid"},{"point",{600,250}}}),acceptanceStep("annotation-held-clear",{{"type","annotation"},{"operation","clear"},{"target","worksheet.grid"}})})}});
 require(report.at("executionComplete")==true,"native replay executes ephemeral observed source annotations: "+report.dump());require(grid->currentIndex()==active&&window.currentSession().snapshotView()==before,"drawing and clearing preserve genuine current cell and model");const auto image=QImage(output.filePath("annotation-range.png"));bool blue=false;for(int y=0;y<image.height()&&!blue;++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y)==QColor("#0000FF")){blue=true;break;}require(blue,"native annotation checkpoint contains the actual blue rectangle");
}
void nativeWindowAnnotationActions(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();auto* grid=window.findChild<QTableView*>("worksheet.grid");const auto active=grid->currentIndex();auto* focus=QApplication::focusWidget();
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("annotation-whole-window",{{"type","annotation"},{"operation","rectangle"},{"target","@active_window"},{"rect",{70,110,835,53}},{"color","#0000FF"},{"lineWidth",3}}),acceptanceStep("annotation-whole-window-clear",{{"type","annotation"},{"operation","clear"},{"target","@active_window"}})})}});
 require(report.at("executionComplete")==true,"native window-coordinate overlay reproduces source chrome/cell rectangles without inventing worksheet formats: "+report.dump());
 const QImage image(output.filePath("annotation-whole-window.png"));require(!image.isNull()&&image.pixelColor(100,110)==QColor("#0000FF"),"the real native window capture contains the observed source-position blue outline");require(window.currentSession().snapshotView()==before&&grid->currentIndex()==active&&QApplication::focusWidget()==focus,"source-position overlay and clear preserve workbook, selection and editor focus");
}
void nativeWorksheetTabAnnotationAnchors(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 auto* tabs=window.findChild<QTabBar*>("workspace.sheets");auto* grid=window.findChild<QTableView*>("worksheet.grid");require(tabs&&tabs->count()==1&&tabs->tabText(0)=="Sheet1","actual worksheet tab anchor exists");grid->setFocus();QApplication::processEvents();
 const auto book=window.currentSession().snapshot();const auto current=grid->currentIndex();const int sheet=tabs->currentIndex();auto* focus=QApplication::focusWidget();const auto original=window.grab().toImage();
 const QRect tab(tabs->mapTo(&window,tabs->tabRect(0).topLeft()),tabs->tabRect(0).size());const QRect expected=tab.adjusted(-2,-2,3,3);
 testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"steps",Json::array({
  acceptanceStep("tab-annotation-rectangle",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","rectangle"},{"padding",{2,2,3,3}}}),
  acceptanceStep("tab-annotation-clear",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","clear"}})})}});
 require(report.at("executionComplete")==true,"source tab annotations resolve actual native tab geometry: "+report.dump());
 require(report.at("steps").at(0).at("annotationRect")==Json::array({expected.x(),expected.y(),expected.width(),expected.height()}),"annotation uses actual QTabBar tabRect plus signed padding");
 const QImage marked(output.filePath("tab-annotation-rectangle.png"));require(marked.pixelColor(expected.center().x(),expected.top())==QColor("#0000FF"),"real held pointer overlay draws blue above the actual native tab");require(window.grab().toImage()==original,"native tab overlay clear restores original window pixels");
 const auto invalid=replay.run({{"steps",Json::array({acceptanceStep("tab-annotation-missing",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","missing"},{"operation","rectangle"}})})}});
 require(invalid.at("executionComplete")==false&&invalid.at("steps").at(0).at("error").get<std::string>().find("tab")!=std::string::npos,"missing native tab is an explicit bounded replay failure");require(window.grab().toImage()==original&&!window.findChild<QWidget*>("replay.annotationOverlay",Qt::FindDirectChildrenOnly),"invalid tab leaves no annotation or changed pixels");
 require(window.currentSession().snapshot()==book&&grid->currentIndex()==current&&tabs->currentIndex()==sheet&&QApplication::focusWidget()==focus,"tab annotation and invalid input preserve workbook, selection, focus and history");
 const QPoint anchor=tab.topLeft()+QPoint(qRound(-.1*(tab.width()-1)),qRound(-.3*(tab.height()-1)));
 const QPoint partial=tab.topLeft()+QPoint(qRound(.9*(tab.width()-1)),qRound(.7*(tab.height()-1)));
 const QPoint expanded=tab.topLeft()+QPoint(qRound(1.1*(tab.width()-1)),qRound(1.1*(tab.height()-1)));
 const auto held=replay.run({{"steps",Json::array({
  acceptanceStep("tab-annotation-press",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","press"},{"tabPoint",{-.1,-.3}}}),
  acceptanceStep("tab-annotation-partial",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","move"},{"tabPoint",{.9,.7}}}),
  acceptanceStep("tab-annotation-expanded",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","move"},{"tabPoint",{1.1,1.1}}}),
  acceptanceStep("tab-annotation-release",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","release"},{"tabPoint",{1.1,1.1}}}),
  acceptanceStep("tab-annotation-held-clear",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","clear"}})})}});
 require(held.at("executionComplete")==true,"tab-relative normalized points preserve each held native source annotation phase: "+held.dump());
 require(held.at("steps").at(0).at("annotationPoint")==Json::array({anchor.x(),anchor.y()})&&held.at("steps").at(1).at("annotationPoint")==Json::array({partial.x(),partial.y()})&&held.at("steps").at(2).at("annotationPoint")==Json::array({expanded.x(),expanded.y()}),"held points are mapped from actual tab dimensions");
 const auto partialImage=QImage(output.filePath("tab-annotation-partial.png")),expandedImage=QImage(output.filePath("tab-annotation-expanded.png"));require(partialImage.pixelColor(partial.x(),partial.y())==QColor("#0000FF")&&expandedImage.pixelColor(expanded.x(),expanded.y())==QColor("#0000FF")&&partialImage!=expandedImage,"partial and expanded states have genuine different native blue pointer outlines");
 const auto malformed=replay.run({{"steps",Json::array({acceptanceStep("tab-annotation-invalid-point",{{"type","annotation"},{"target","workspace.sheets"},{"tabName","Sheet1"},{"operation","press"},{"tabPoint",{"invalid",0}}})})}});
 require(malformed.at("executionComplete")==false&&window.grab().toImage()==original&&!window.findChild<QWidget*>("replay.annotationOverlay",Qt::FindDirectChildrenOnly),"invalid normalized point is rejected before creating an overlay");require(window.currentSession().snapshot()==book&&grid->currentIndex()==current&&tabs->currentIndex()==sheet&&QApplication::focusWidget()==focus,"held lifecycle and malformed point preserve native selection, focus, data and history");
}
void nativeSourceChromeActions(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto book=window.currentSession().snapshotView();
 const auto report=replay.run({{"windowSize",{1280,720}},{"steps",Json::array({
  acceptanceStep("source-caption-modal",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+1"}}}},{"steps",Json::array({
   acceptanceStep("source-caption-show",{{"type","video_caption"},{"operation","show"},{"target","@active_window"},{"rect",{200,560,780,150}},{"lines",{"Format Cells Dialog","Ctrl + 1 (PC / Mac)"}},{"fontPixelSize",42}}),
   acceptanceStep("source-caption-close-modal",{{"type","key_sequence"},{"target","formatCells.dialog"},{"keys",{"Escape"}}})})}}),
  acceptanceStep("source-caption-clear",{{"type","video_caption"},{"operation","clear"},{"target","@active_window"}}),
  acceptanceStep("source-toolbar-document",{{"type","document_context"},{"operation","open"},{"path",std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-90-Minute-Case-Study.pdf"}}),
  acceptanceStep("source-toolbar-show",{{"type","document_text_toolbar"},{"operation","show"},{"target","@context_window"},{"rect",{331,216,536,64}},{"kind","word"},{"opacity",0.3}}),
  acceptanceStep("source-toolbar-clear",{{"type","document_text_toolbar"},{"operation","clear"},{"target","@context_window"}}),
  acceptanceStep("source-toolbar-close",{{"type","document_context"},{"operation","close"}})
 })}});
 require(report.at("executionComplete")==true,"Native replay executes measured read-only external chrome and caption above an actual modal: "+report.dump());
 const auto& caption=report.at("steps").at(0).at("children").at(0);require(caption.at("after").at("modal")=="formatCells.dialog","Source caption preserves the real active Format Cells modal");
 require(caption.contains("captionScreenshot")&&caption.contains("sourceCompositeScreenshot")&&caption.contains("modalScreenshot"),"Trace preserves separate actual caption/modal grabs and disclosed source-position composition");
 const QImage plain(output.filePath("source-caption-show.png")),composite(output.filePath(QString::fromStdString(caption.at("sourceCompositeScreenshot").get<std::string>())));require(!plain.isNull()&&!composite.isNull()&&plain.size()==composite.size()&&plain!=composite,"Composed source checkpoint actually includes the native top-level caption that MainWindow.grab omits");
 require(report.at("steps").at(3).at("documentTextToolbarKind")=="word"&&QImage(output.filePath("source-toolbar-show.context.png"))!=QImage(output.filePath("source-toolbar-clear.context.png")),"Actual read-only document captures include the native faded toolbar and its dismissal");require(window.currentSession().snapshotView()==book,"Source toolbar, modal cancellation and caption never author worksheet data or styles");
}
void nativeCaptionAcrossWorkbookWindows(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());
 const auto report=replay.run({{"windowSize",{1280,720}},{"steps",Json::array({
  acceptanceStep("caption-origin-show",{{"type","video_caption"},{"operation","show"},{"target","@active_window"},{"rect",{449,559,384,157}},{"lines",{"Anchor Cell","PC: F4","Mac: F4"}}}),
  acceptanceStep("caption-new-workbook",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+N"}}},{{"windowCount",2}}),
  acceptanceStep("caption-other-workbook-clear",{{"type","video_caption"},{"operation","clear"},{"target","@active_window"}})
 })}});
 QList<QPointer<MainWindow>> others;for(auto* widget:QApplication::topLevelWidgets())if(auto* other=dynamic_cast<MainWindow*>(widget);other&&other!=&window)others.push_back(other);for(auto other:others)if(other)delete other;
 require(report.at("executionComplete")==true,"Native caption lifecycle survives actual workbook-window switches: "+report.dump());require(report.at("steps").at(1).contains("captionScreenshot"),"Source caption remains captured above the genuinely active destination workbook");require(!window.findChild<QWidget*>("replay.videoCaption",Qt::FindDirectChildrenOnly),"Caption clear from the active destination removes the actual publisher overlay owned by the source window");
}
void nativeUnexpectedModalIsBounded(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());bool emergency=false;QTimer fallback;fallback.setSingleShot(true);QObject::connect(&fallback,&QTimer::timeout,&window,[&]{if(auto* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){emergency=true;dialog->accept();}});fallback.start(3000);
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("unexpected-draft-start",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),acceptanceStep("unexpected-draft-type",{{"type","text"},{"target","worksheet.cellEditor"},{"text","IF(1,1"}}),acceptanceStep("unexpected-draft-commit",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Return"}}})})}});fallback.stop();
 require(!emergency&&report.at("executionComplete")==false,"Every undeclared native dialog from one Return is bounded without an emergency dismissal: "+report.dump());const auto& step=report.at("steps").back();require(step.at("status")=="failed"&&step.at("unexpectedModal").at("objectName")=="worksheet.formulaCorrection"&&step.at("unexpectedModals").size()==2&&step.at("unexpectedModals")[1].at("text")=="The formula is missing an opening or closing parenthesis.","Failed source action retains the actual correction prompt and subsequent warning");require(QFile::exists(output.filePath(QString::fromStdString(step.at("unexpectedModalScreenshot").get<std::string>()))),"Actual unexpected modal pixels are retained before bounded native dismissal");require(window.currentSession().snapshotView().at("revision")==0&&step.at("after").at("formulaDraft")=="=IF(1,1","Unexpected-warning failure preserves genuine uncommitted draft and worksheet history");
}
void nativeOwnedPopupCapture(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("owned-popup-collapse",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+F1"}}}),acceptanceStep("owned-popup-home-tips",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Alt","H"}}}),acceptanceStep("owned-popup-dismiss",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Escape"}}})})}});
 require(report.at("executionComplete")==true,"Source Home popup opens through real collapsed ribbon keytips");const auto& step=report.at("steps").at(1);require(step.contains("ownedWindowScreenshots"),"Actual collapsed ribbon popup and owned top-level keytip windows are separately retained in native replay evidence");bool popup=false,launcher=false;for(const auto& capture:step.at("ownedWindowScreenshots")){popup=popup||capture.at("objectName")=="ribbon.peek";launcher=launcher||capture.at("objectName")=="tutorial.keyTip.FN";require(QFile::exists(output.filePath(QString::fromStdString(capture.at("screenshot").get<std::string>()))),"Every composed native popup/keytip retains its actual original widget grab");}require(popup&&launcher,"Source-position composition includes actual Home page and the unclipped FN launcher keytip below it");require(step.contains("sourceCompositeScreenshot")&&QImage(output.filePath("owned-popup-home-tips.png"))!=QImage(output.filePath(QString::fromStdString(step.at("sourceCompositeScreenshot").get<std::string>()))),"Actual native popup/keytips are visibly included in the disclosed composition omitted by MainWindow.grab");require(window.currentSession().snapshotView()==before,"Source popup capture and dismissal do not author worksheet data or styles");
}
void nativeDocumentArtworkReplay(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("artwork-context-open",{{"type","document_context"},{"operation","open"},{"path",std::string(TIGERSHEETS_SOURCE_DIR)+"/docs/validation/otis-90-minute/reference/101-01-Otis-90-Minute-Case-Study.pdf"}}),acceptanceStep("artwork-context-select",{{"type","document_image_selection"},{"operation","artwork"},{"target","context.surface"},{"rect",{91,152,362,199}}}),acceptanceStep("artwork-context-clear",{{"type","document_image_selection"},{"operation","clear"},{"target","context.surface"}}),acceptanceStep("artwork-context-close",{{"type","document_context"},{"operation","close"}})})}});
 require(report.at("executionComplete")==true,"Native replay selects the actual source case-study artwork without an unobserved image toolbar: "+report.dump());require(QImage(output.filePath("artwork-context-select.context.png"))!=QImage(output.filePath("artwork-context-clear.context.png")),"Actual read-only document checkpoints retain observed gray artwork selection and clear");require(window.currentSession().snapshotView()==before,"Artwork selection does not manufacture worksheet data or style changes");
}
void nativeSourceWindowGeometry(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();window.findChild<QTableView*>("worksheet.grid")->setFocus();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();auto* focus=QApplication::focusWidget();
 const auto report=replay.run({{"windowSize",{1280,720}},{"steps",Json::array({acceptanceStep("source-window-restored",{{"type","window_geometry"},{"rect",{105,66,1000,680}}},{{"windowGeometry",{105,66,1000,680}}}),acceptanceStep("source-window-real-navigation",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Right"}}},{{"windowGeometry",{105,66,1000,680}},{"row",1},{"column",2}}),acceptanceStep("source-window-reactivate",{{"type","activate_window"},{"documentName","Book1.xlsx"}},{{"windowGeometry",{105,66,1000,680}}}),acceptanceStep("source-window-source-viewport",{{"type","window_geometry"},{"rect",{0,0,1280,720}}},{{"windowGeometry",{2,2,1280,720}},{"requestedWindowGeometry",{0,0,1280,720}},{"row",1},{"column",2}})})}});
 require(report.at("executionComplete")==true,"Observed restored-window geometry persists through actual native keys and window activation: "+report.dump());require(report.at("steps").at(0).contains("windowCanvasScreenshot")&&QImage(output.filePath(QString::fromStdString(report.at("steps").at(0).at("windowCanvasScreenshot").get<std::string>()))).size()==QSize(1280,720),"Observed clipped/restored window position has a source-size canvas from actual native widget pixels");require(window.currentSession().snapshotView()==before,"Source window move/resize preserves actual worksheet contents, styles and history");require(QApplication::focusWidget()==focus,"Source window move/resize preserves focused grid");
 const auto draft=replay.run({{"steps",Json::array({acceptanceStep("source-window-draft-start",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),acceptanceStep("source-window-draft-prefix",{{"type","text"},{"target","worksheet.cellEditor"},{"text","SU"}}),acceptanceStep("source-window-live-draft-move",{{"type","window_geometry"},{"rect",{-18,10,1280,720}}},{{"formulaDraft","=SU"},{"revision",0}}),acceptanceStep("source-window-live-draft-continue",{{"type","text"},{"target","worksheet.cellEditor"},{"text","M("}},{{"formulaDraft","=SUM("},{"revision",0}}),acceptanceStep("source-window-live-draft-cancel",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Escape"}}},{{"revision",0}})})}});require(draft.at("executionComplete")==true&&window.currentSession().snapshotView()==before,"Observed window move preserves actual in-cell formula editor and continuation without premature commit: "+draft.dump());
}
void nativePartialTopRow(){
 QTemporaryDir output;MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output.path());const auto before=window.currentSession().snapshotView();
 const auto report=replay.run({{"steps",Json::array({acceptanceStep("partial-row-draft-start",{{"type","text"},{"target","worksheet.grid"},{"text","="}}),acceptanceStep("partial-row-draft-prefix",{{"type","text"},{"target","worksheet.cellEditor"},{"text","SUM("}}),acceptanceStep("partial-row-source-offset",{{"type","vertical_view"},{"topRow",7},{"topRowOffset",-5}},{{"topRow",7},{"topRowOffset",-5},{"row",1},{"column",1},{"formulaDraft","=SUM("},{"revision",0}}),acceptanceStep("partial-row-full-return",{{"type","vertical_view"},{"topRow",1}},{{"topRow",1},{"topRowOffset",0},{"formulaDraft","=SUM("},{"revision",0}}),acceptanceStep("partial-row-cancel-draft",{{"type","key_sequence"},{"target","worksheet.cellEditor"},{"keys",{"Escape"}}})})}});
 require(report.at("executionComplete")==true,"Actual fine wheel events reproduce an observed partially clipped first row without changing live formula draft, active anchor or history: "+report.dump());require(window.currentSession().snapshotView()==before,"Partial-row viewport counterpart never edits worksheet state");
}
void persistentManifestEntryPoint(){
 QTemporaryDir files;const auto path=files.filePath("source actions.json"),output=files.filePath("persistent checkpoints");
 QFile manifest(path);require(manifest.open(QIODevice::WriteOnly),"create a CLI acceptance manifest");
 const auto step=acceptanceStep("008-hide-gridlines",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Alt","W","V","G"}}},{{"showGridLines",false}});
 manifest.write(QByteArray::fromStdString(Json({{"windowSize",{1280,720}},{"steps",Json::array({step})}}).dump()));manifest.close();
 QProcess process;process.start(QCoreApplication::applicationFilePath(),{"--manifest",path,"--output",output,"--acceptance"});
 require(process.waitForFinished(10000),"native manifest CLI finishes without interactive prompts");
 require(process.exitCode()==0,"manifest CLI executes a timestamped native action trace: "+process.readAllStandardError().toStdString());
 QFile reportFile(output+"/action-replay.json");require(reportFile.open(QIODevice::ReadOnly),"CLI persists the execution report outside test temporary state");const auto report=Json::parse(reportFile.readAll().toStdString());
 require(report.at("executionComplete")==true&&report.at("complete")==false&&!QImage(output+"/008-hide-gridlines.png").isNull(),"CLI retains actual UI PNG and separates acceptance from source completion");
 require(QImage(output+"/008-hide-gridlines.png").size()==QSize(1280,720),"source manifest controls the actual native viewport and checkpoint image dimensions");
}
void nativeFilesAndWindows(){
 QTemporaryDir files;const auto path=files.filePath("raw financials.csv");QFile source(path);require(source.open(QIODevice::WriteOnly),"create an independent raw-file fixture");source.write("Historical source\n");source.close();
 MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,files.filePath("evidence"));
 const Json openSteps=Json::array({
  acceptanceStep("open-01-select-filename",{{"type","key_sequence"},{"target","fileNameEdit"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("open-02-type-filename",{{"type","text"},{"target","fileNameEdit"},{"text",path.toStdString()}}),
  acceptanceStep("open-03-confirm",{{"type","key_sequence"},{"target","fileNameEdit"},{"keys",{"Return"}}})});
 const Json steps=Json::array({
  acceptanceStep("open-raw-file",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+O"}}}},{"steps",openSteps}},{{"cells",Json::array({{{"address","A1"},{"input","Historical source"}}})}}),
  acceptanceStep("new-model-window",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+N"}}},{{"documentName","Book1.xlsx"},{"windowCount",2}}),
  acceptanceStep("return-to-source-window",{{"type","activate_window"},{"documentName","raw financials.csv"}},{{"documentName","raw financials.csv"},{"windowCount",2},{"cells",Json::array({{{"address","A1"},{"input","Historical source"}}})}}),
  acceptanceStep("close-inspected-source",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+W"}}},{{"documentName","Book1.xlsx"},{"windowCount",1},{"revision",0}})});
 const auto report=replay.run({{"windowSize",{1280,720}},{"steps",steps}});
 QList<QPointer<MainWindow>> others;for(auto* widget:QApplication::topLevelWidgets())if(auto* other=dynamic_cast<MainWindow*>(widget);other&&other!=&window)others.push_back(other);
 for(auto other:others)if(other)delete other;
 require(report.at("executionComplete")==true,"replay opens files through Ctrl+O, creates a model with Ctrl+N, then activates the retained source window: "+report.dump());
 require(QImage(files.filePath("evidence/new-model-window.png")).size()==QSize(1280,720),"source viewport configuration applies to newly created native workbook windows");
}
void observedRawSheetTraversal(){
 QTemporaryDir files;const QString root=TIGERSHEETS_SOURCE_DIR;const auto raw=files.filePath("Otis-10-K-Raw.xls");require(QFile::copy(root+"/docs/validation/otis-90-minute/reference/101-01-Otis-10-K-Raw.xls",raw),"copy the untouched120-sheet source into an isolated replay fixture");
 std::ifstream input((root+"/docs/validation/otis-90-minute/intro-visual-steps.json").toStdString());Json observed;input>>observed;
 MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,files.filePath("checkpoints"));
 Json steps=Json::array({acceptanceStep("fixture-open-raw",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+O"}}}},{"steps",Json::array({
  acceptanceStep("fixture-select-raw-name",{{"type","key_sequence"},{"target","fileNameEdit"},{"keys",{"Ctrl+A"}}}),
  acceptanceStep("fixture-type-raw-name",{{"type","text"},{"target","fileNameEdit"},{"text",raw.toStdString()}}),
  acceptanceStep("fixture-open-raw-name",{{"type","key_sequence"},{"target","fileNameEdit"},{"keys",{"Return"}}})})}},{{"sheetName","securities registered purs"}})});
 for(auto step:observed.at("rawNavigation").at("steps")){step["expect"]={{"sheetName",step.at("observed").at("sheetName")},{"row",1},{"column",1}};steps.push_back(std::move(step));}
 const auto report=replay.run({{"sourceVideo",observed.at("sourceVideo")},{"scope","Observed38source-sheet transitions with explicit synthetic fixture opening"},{"steps",steps}});
 require(report.at("executionComplete")==true,"native replay follows every observed forward/backward source tab arrival: "+report.dump());
 require(report.at("steps").size()==39&&report.at("steps").back().at("after").at("sheetName")=="operations"&&report.at("steps").back().at("after").at("revision")==0,"38actual native sheet changes preserve the untouched raw workbook and end on Operations");
 require(report.at("complete")==false,"one observed segment never claims complete video coverage");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{
 if(qgetenv("TIGERSHEETS_REPLAY_TEST_CASE")=="effective_bold"){nativeEffectiveBoldStyleCheckpoints();std::cout<<"Native effective bold style checkpoints passed\n";return 0;}
 if(argc==1&&qgetenv("TIGERSHEETS_REPLAY_TEST_CASE").isEmpty())nativeEffectiveBoldStyleCheckpoints();
 if(qgetenv("TIGERSHEETS_REPLAY_TEST_CASE")=="tab_annotation"){nativeWorksheetTabAnnotationAnchors();std::cout<<"Native worksheet-tab annotation passed\n";return 0;}
 if(argc>1){
  QString manifestPath,output;bool acceptance=false;
  for(int index=1;index<argc;++index){const QString arg=QString::fromLocal8Bit(argv[index]);if(arg=="--manifest"&&index+1<argc)manifestPath=QString::fromLocal8Bit(argv[++index]);else if(arg=="--output"&&index+1<argc)output=QString::fromLocal8Bit(argv[++index]);else if(arg=="--acceptance")acceptance=true;else throw std::runtime_error("Unknown replay argument: "+arg.toStdString());}
  require(!manifestPath.isEmpty()&&!output.isEmpty(),"Replay requires --manifest FILE --output DIRECTORY");
  QFile source(manifestPath);require(source.open(QIODevice::ReadOnly),"Cannot read replay manifest");const auto manifest=Json::parse(source.readAll().toStdString());
  MainWindow window;window.resize(1440,960);window.show();window.activateWindow();QApplication::processEvents();testing::NativeActionReplay replay(window,output);const auto report=replay.run(manifest);
  std::cout<<report.dump(2)<<'\n';return report.at(acceptance?"executionComplete":"complete")==true?0:2;
 }
 const auto focused=qgetenv("TIGERSHEETS_REPLAY_TEST_CASE");if(focused=="serial_modal"){nativeUnnamedWarningModal();std::cout<<"Native serial correction/warning dialogs passed\n";return 0;}if(focused=="source_currency_popup"){nativeSourceCurrencyDropdownAcceptance();std::cout<<"Native source currency dropdown acceptance passed\n";return 0;}if(focused=="release_capture"){testing::nativeSelectionReleaseCaptureFreshness();std::cout<<"Native selection-release capture freshness passed\n";return 0;}if(focused=="modal_popup_composition"){testing::nativeModalPopupComposition();std::cout<<"Native modal popup composition passed\n";return 0;}if(focused=="format_popup"){nativeNumberFormatPopupKeyboardSelection();std::cout<<"Native number-format popup keyboard selection passed\n";return 0;}if(focused=="reverse_point"){nativeReversePointSwitchRetainsActualSource();std::cout<<"Native reverse Point window switch passed\n";return 0;}if(focused=="analysis_visibility"){nativeQuickAnalysisVisibilityCheckpoints();std::cout<<"Native Quick Analysis visibility checkpoints passed\n";return 0;}if(focused=="key_focus"){nativeKeySequenceFollowsEditorFocus();std::cout<<"Native key sequence focus passed\n";return 0;}if(focused=="same_window_literal"){nativeSameWindowLiteralDraftSurvivesActivation();std::cout<<"Native same-window literal draft passed\n";return 0;}if(focused=="point_hint"){nativePointRangeHintObservation();std::cout<<"Native Point range hint observation passed\n";return 0;}if(focused=="completed_modal"){nativeCompletedModalStopsItsTimeout();std::cout<<"Native completed-modal timeout lifecycle passed\n";return 0;}if(focused=="status_message"){nativeStatusMessageObservationProvesRefresh();std::cout<<"Native refresh status observation passed\n";return 0;}if(focused=="held_shift"){nativeHeldShiftCountsPreserveCopyState();std::cout<<"Native held Shift source counts passed\n";return 0;}if(focused=="name_box"){nativeNameBoxObservationTracksHeldSelection();std::cout<<"Native live Name Box observation passed\n";return 0;}if(focused=="context_tooltip"){nativeReadOnlyDocumentTooltipReplay();std::cout<<"Native document tooltip replay passed\n";return 0;}if(focused=="hover_dwell"){nativeWidgetHoverDwell();std::cout<<"Native hover dwell passed\n";return 0;}if(focused=="active_pointing_window"){nativeActivePointingWindowPreservesCaret();std::cout<<"Native active source caret passed\n";return 0;}if(focused=="widget_hover"){nativeWidgetHoverPreservesState();std::cout<<"Native widget hover passed\n";return 0;}if(focused=="window_inspection"){nativeWindowInspectionRetainsCompleteDraft();std::cout<<"Native complete-draft inspection passed\n";return 0;}if(focused=="partial_row"){nativePartialTopRow();std::cout<<"Native partial first row passed\n";return 0;}if(focused=="window_geometry"){nativeSourceWindowGeometry();std::cout<<"Native source window geometry passed\n";return 0;}if(focused=="artwork"){nativeDocumentArtworkReplay();std::cout<<"Native document artwork replay passed\n";return 0;}if(focused=="owned_popups"){nativeOwnedPopupCapture();std::cout<<"Native owned popup capture passed\n";return 0;}if(focused=="unexpected_modal"){nativeUnexpectedModalIsBounded();std::cout<<"Native unexpected modal bound passed\n";return 0;}if(focused=="caption_windows"){nativeCaptionAcrossWorkbookWindows();std::cout<<"Native caption window lifecycle passed\n";return 0;}if(focused=="source_chrome"){nativeSourceChromeActions();std::cout<<"Native source chrome replay passed\n";return 0;}if(focused=="window_annotation"){nativeWindowAnnotationActions();std::cout<<"Native source-window annotations passed\n";return 0;}if(focused=="observe_draft"){nativeObservationPreservesDraftFocus();std::cout<<"Native observation preserves draft focus passed\n";return 0;}if(focused=="value_tolerance"){nativeNumericToleranceCheckpoints();std::cout<<"Native numeric tolerance checkpoints passed\n";return 0;}if(focused=="viewport_alignment"){nativeViewportAlignment();std::cout<<"Native full-row viewport alignment passed\n";return 0;}if(focused=="context_windows"){nativeDocumentContextAcrossWorkbookWindows();std::cout<<"Native cross-workbook document context passed\n";return 0;}if(focused=="image_selection"){nativeDocumentImageSelectionActions();std::cout<<"Native document image selection replay passed\n";return 0;}if(focused=="close"){nativeFilesAndWindows();std::cout<<"Native close retarget passed\n";return 0;}if(focused=="draft_context"){nativeDocumentContextPreservesDraft();std::cout<<"Native document draft preservation passed\n";return 0;}if(focused=="presentation"){nativePresentationBuildActions();std::cout<<"Native presentation replay passed\n";return 0;}if(focused=="clipboard"){nativeClipboardConflictBoundary();std::cout<<"Native clipboard boundary passed\n";return 0;}if(focused=="context"){nativeDocumentContextActions();std::cout<<"Native context replay passed\n";return 0;}if(focused=="annotation"){nativeAnnotationActions();std::cout<<"Native annotation replay passed\n";return 0;}
 testing::nativeSelectionReleaseCaptureFreshness();testing::nativeModalPopupComposition();nativeSourceCurrencyDropdownAcceptance();nativeNumberFormatPopupKeyboardSelection();nativeReversePointSwitchRetainsActualSource();nativeQuickAnalysisVisibilityCheckpoints();nativeKeySequenceFollowsEditorFocus();nativeSameWindowLiteralDraftSurvivesActivation();nativeWorksheetTabAnnotationAnchors();nativeWidgetHoverDwell();nativeReadOnlyDocumentTooltipReplay();nativeActivePointingWindowPreservesCaret();nativeWidgetHoverPreservesState();nativeWindowInspectionRetainsCompleteDraft();firstKeyboardCheckpoint();typedFormulaAndCorrection();modalFormattingCheckpoints();nativeLegacySequenceOpensModal();nativeReplayReleasesModifiers();nativeExternalTextClipboardBoundary();nativeUnnamedWarningModal();nativeCalculatedValueCheckpoints();nativeNumericToleranceCheckpoints();nativeObservationPreservesDraftFocus();nativeHorizontalViewportNavigation();nativeVerticalViewportNavigation();nativeViewportAlignment();sourceObservationGate();nestedSourceObservationGate();nativeNameBoxObservationTracksHeldSelection();nativeHeldShiftCountsPreserveCopyState();nativePointRangeHintObservation();nativeStatusMessageObservationProvesRefresh();nativeCompletedModalStopsItsTimeout();nativeCellPointingAndDrag();nativeWheelDuringFormulaEntry();nativeWheelToDistantFormulaReference();nativeFormatTabAndCheckboxClicks();nativeInCellDraftSequence();nativeClipboardConflictBoundary();nativeDocumentContextPreservesDraft();nativePresentationBuildActions();nativeDocumentContextActions();nativeDocumentContextAcrossWorkbookWindows();nativeDocumentImageSelectionActions();nativeAnnotationActions();nativeWindowAnnotationActions();nativeSourceChromeActions();nativeCaptionAcrossWorkbookWindows();nativeUnexpectedModalIsBounded();nativeOwnedPopupCapture();nativeDocumentArtworkReplay();nativeSourceWindowGeometry();nativePartialTopRow();persistentManifestEntryPoint();nativeFilesAndWindows();observedRawSheetTraversal();std::cout<<"native Otis action replay tests passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
