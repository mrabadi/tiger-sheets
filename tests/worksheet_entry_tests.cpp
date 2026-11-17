#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QTest>
#include <QVBoxLayout>
#include <QPointer>
#include <QAccessible>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
 WorkbookSession session;
 WorksheetModel model{session};
 QWidget window;
 QLineEdit bar{&window};
 WorksheetView view{&window};
 ~Fixture(){view.cancelEntry();}
 Fixture(){auto* layout=new QVBoxLayout(&window);layout->addWidget(&bar);layout->addWidget(&view);view.setModel(&model);view.setFormulaBar(&bar);window.resize(820,560);window.show();QApplication::processEvents();view.setCurrentIndex(model.index(0,0));}
};
class PaintObserver:public QObject {
public:
 QRegion painted;
protected:
 bool eventFilter(QObject*,QEvent* event)override{if(event->type()==QEvent::Paint)painted+=static_cast<QPaintEvent*>(event)->region();return false;}
};
void calculationPreservesDirtyEntry(){
 Fixture fixture;fixture.model.setData(fixture.model.index(0,0),"=1+2",Qt::EditRole);fixture.view.setFocus();fixture.view.edit(fixture.model.index(0,0));QApplication::processEvents();
 auto* editor=fixture.view.activeCellEditor();require(editor!=nullptr,"native in-cell editor opens");QTest::keyClick(editor,Qt::Key_End);QTest::keyClicks(editor,"+9");
 const auto draft=editor->text();const auto cursor=editor->cursorPosition();const auto before=fixture.session.snapshot();require(editor->isModified()&&draft=="=1+2+9","user draft is modified before calculation");
 fixture.model.setCalculated({{"revision",before["revision"]},{"values",Json::array({{{"sheet",fixture.model.sheetId().toStdString()},{"row",1},{"col",1},{"type","number"},{"value",3}}})}});QApplication::processEvents();
 require(editor->text()==draft&&editor->cursorPosition()==cursor&&editor->isModified(),"calculation dataChanged must preserve the dirty in-cell draft and caret");
 require(fixture.bar.text()==draft&&fixture.session.snapshot()==before,"calculation delivery leaves the mirrored formula draft and canonical workbook untouched");
 fixture.view.cancelEntry();
}
int blueEdge(const QImage& image,const QRect& first,const QRect& last){int pixels=0;for(int y=std::max(0,first.top()+2);y<std::min(image.height(),last.bottom()-2);++y)for(int x=std::max(0,first.left());x<=std::min(image.width()-1,first.left()+2);++x){const auto color=image.pixelColor(x,y);if(color.blue()>color.red()+40&&color.blue()>color.green()+20)++pixels;}return pixels;}
void selectedFormulaHasReferenceOutline(){
 Fixture fixture;const auto formula=fixture.model.index(0,3);require(fixture.model.setData(formula,"=SUM(B1:B3)",Qt::EditRole),"seed a committed formula for ordinary selection");fixture.view.setFocus();QApplication::processEvents();const auto before=fixture.session.snapshot();PaintObserver paint;fixture.view.viewport()->installEventFilter(&paint);
 QTest::mouseClick(fixture.view.viewport(),Qt::LeftButton,Qt::NoModifier,fixture.view.visualRect(formula).center());QApplication::processEvents();
 const auto first=fixture.view.visualRect(fixture.model.index(0,1)),last=fixture.view.visualRect(fixture.model.index(2,1));
 const bool referencesRepainted=paint.painted.contains(first.united(last));
 require(blueEdge(fixture.view.viewport()->grab().toImage(),first,last)==0,"Ready selection shows the committed formula without displaying precedent outlines");
 require(referencesRepainted,"ordinary formula selection must repaint referenced cells outside the old and new selected cell rectangles");
 require(!fixture.view.activeCellEditor()&&fixture.view.currentIndex()==formula&&fixture.session.snapshot()==before,"selected-formula outlines neither open an editor nor mutate or navigate the workbook");
 paint.painted=QRegion();const auto plain=fixture.model.index(4,0);QTest::mouseClick(fixture.view.viewport(),Qt::LeftButton,Qt::NoModifier,fixture.view.visualRect(plain).center());QApplication::processEvents();const bool previousReferencesRepainted=paint.painted.contains(first.united(last));
 require(blueEdge(fixture.view.viewport()->grab().toImage(),first,last)==0,"selecting a nonformula cell clears the previous formula's reference outlines");
 require(previousReferencesRepainted,"nonformula selection must repaint and clear outlines outside the changed selection rectangles");
 require(!fixture.view.activeCellEditor()&&fixture.session.snapshot()==before,"clearing selection outlines leaves the workbook and editor state unchanged");
}
void selectedFormulaChangeRefreshesReferenceOutlines(){
 Fixture fixture;const auto formula=fixture.model.index(0,3);require(fixture.model.setData(formula,"=SUM(B1:B3)",Qt::EditRole),"seed selected-formula refresh fixture");fixture.view.setFocus();fixture.view.setCurrentIndex(formula);QApplication::processEvents();const auto first=fixture.view.visualRect(fixture.model.index(0,1)),last=fixture.view.visualRect(fixture.model.index(2,1)),nextFirst=fixture.view.visualRect(fixture.model.index(0,2)),nextLast=fixture.view.visualRect(fixture.model.index(2,2));PaintObserver paint;fixture.view.viewport()->installEventFilter(&paint);
 require(fixture.model.setData(formula,"=SUM(C1:C3)",Qt::EditRole),"replace the current cell's committed formula");QApplication::processEvents();const bool referencesRepainted=paint.painted.contains(first.united(nextLast));
 require(referencesRepainted,"changing a selected formula must repaint both old and new references outside the changed cell");const auto image=fixture.view.viewport()->grab().toImage();require(blueEdge(image,first,last)==0&&blueEdge(image,nextFirst,nextLast)==0,"committed formula replacement in Ready does not reveal old or new precedent outlines");
 paint.painted=QRegion();require(fixture.model.setData(formula,"plain text",Qt::EditRole),"replace selected formula with plain text");QApplication::processEvents();const bool clearedReferencesRepainted=paint.painted.contains(nextFirst.united(nextLast));
 require(clearedReferencesRepainted&&blueEdge(fixture.view.viewport()->grab().toImage(),nextFirst,nextLast)==0,"replacing a selected formula with plain text must repaint and clear its references");require(!fixture.view.activeCellEditor()&&fixture.view.currentIndex()==formula,"formula-change repaint preserves selection and does not open an editor");
}
void draftOutlinesOverrideSelectedFormula(){
 Fixture fixture;const auto formula=fixture.model.index(0,3);require(fixture.model.setData(formula,"=SUM(B1:B3)",Qt::EditRole),"seed selected formula for draft precedence");fixture.view.setCurrentIndex(formula);const auto before=fixture.session.snapshot();const auto first=fixture.view.visualRect(fixture.model.index(0,1)),last=fixture.view.visualRect(fixture.model.index(2,1)),draftFirst=fixture.view.visualRect(fixture.model.index(0,2)),draftLast=fixture.view.visualRect(fixture.model.index(2,2));
 fixture.bar.setFocus();fixture.bar.setText("=SUM(C1:C3)");QApplication::processEvents();const auto draft=fixture.view.viewport()->grab().toImage();require(blueEdge(draft,first,last)==0&&blueEdge(draft,draftFirst,draftLast)>20,"active formula-bar draft outlines replace the selected formula's committed references");
 fixture.bar.setText("plain text draft");QApplication::processEvents();const auto plain=fixture.view.viewport()->grab().toImage();require(blueEdge(plain,first,last)==0&&blueEdge(plain,draftFirst,draftLast)==0,"an active nonformula draft suppresses committed selected-formula outlines");
 fixture.view.cancelEntry();QApplication::processEvents();const auto cancelled=fixture.view.viewport()->grab().toImage();require(blueEdge(cancelled,first,last)==0&&blueEdge(cancelled,draftFirst,draftLast)==0,"cancelling a draft returns to Ready without committed precedent outlines");require(fixture.session.snapshot()==before&&!fixture.view.activeCellEditor()&&fixture.view.currentIndex()==formula,"outline precedence and cancellation preserve canonical data and selection");
}
void activeDraftHasReferenceOutline(){
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(0,3));fixture.bar.setFocus();const auto before=fixture.session.snapshot();QTest::keyClicks(&fixture.bar,"=SUM(B1:B3)");QApplication::processEvents();
 const auto first=fixture.view.visualRect(fixture.model.index(0,1)),last=fixture.view.visualRect(fixture.model.index(2,1));
 require(blueEdge(fixture.view.viewport()->grab().toImage(),first,last)>20,"active draft reference must have a visible blue range outline");
 require(fixture.session.snapshot()==before&&fixture.view.currentIndex()==fixture.model.index(0,3),"reference outlines do not mutate or navigate the workbook");
 fixture.view.cancelEntry();QApplication::processEvents();
 require(blueEdge(fixture.view.viewport()->grab().toImage(),first,last)==0,"cancelled formula drafts clear their reference outlines");
}
void keyboardPointingIsUncommittedAndCompletionFirst(){
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(0,3));fixture.bar.setFocus();QTest::keyClicks(&fixture.bar,"=SUM(");const auto before=fixture.session.snapshot();
 int completionArrows=0;fixture.view.beforeEditorKey=[&](QLineEdit*,QKeyEvent* key){if(key->key()==Qt::Key_Down){++completionArrows;return true;}return false;};
 QTest::keyClick(&fixture.bar,Qt::Key_Down);
 require(completionArrows==1&&fixture.bar.text()=="=SUM(","completion gets the first opportunity to consume formula arrows");fixture.view.beforeEditorKey={};
 QTest::keyClick(&fixture.bar,Qt::Key_Right);
 require(fixture.bar.text()=="=SUM(E1","arrow after a delimiter inserts a neighboring cell reference");
 QTest::keyClick(&fixture.bar,Qt::Key_Down,Qt::ShiftModifier);QTest::keyClick(&fixture.bar,Qt::Key_Right,Qt::ShiftModifier);
 require(fixture.bar.text()=="=SUM(E1:F2","Shift arrows extend the uncommitted pointed reference range");
 QTest::keyClick(&fixture.bar,Qt::Key_Left);
 require(fixture.bar.text()=="=SUM(E2","plain arrows move the pointed cell without duplicating the old range");
 require(fixture.view.currentIndex()==fixture.model.index(0,3)&&fixture.session.snapshot()==before,"keyboard reference picking preserves edit destination and canonical workbook");
 QTest::keyClick(&fixture.bar,Qt::Key_F2);const auto cursor=fixture.bar.cursorPosition();QTest::keyClick(&fixture.bar,Qt::Key_Left);
 require(fixture.bar.text()=="=SUM(E2"&&fixture.bar.cursorPosition()==cursor-1,"F2 switches arrows to editing the text cursor");
 QTest::keyClick(&fixture.bar,Qt::Key_End);QTest::keyClick(&fixture.bar,Qt::Key_F2);QTest::keyClick(&fixture.bar,Qt::Key_Right);
 require(fixture.bar.text()=="=SUM(F2","F2 returns to point mode and continues the current reference");
 fixture.view.cancelEntry();
 fixture.view.setFocus();fixture.view.edit(fixture.model.index(0,3));QApplication::processEvents();auto* editor=fixture.view.activeCellEditor();require(editor!=nullptr,"in-cell keyboard reference fixture opens");QTest::keyClicks(editor,"=");QTest::keyClick(editor,Qt::Key_Left);QTest::keyClick(editor,Qt::Key_Down,Qt::ShiftModifier);
 require(editor->text()=="=C1:C2"&&fixture.bar.text()==editor->text()&&fixture.session.snapshot()==before,"in-cell arrows and Shift ranges share formula-bar point behavior without committing");
}
void pivotPointingHook(){
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(0,3));fixture.bar.setFocus();const auto before=fixture.session.snapshot();int calls=0;fixture.view.pointedReference=[&](QModelIndex first,QModelIndex last){++calls;require(first==last,"custom references receive a single pointed cell");return first.column()==4?QString("GETPIVOTDATA(\"Revenue\",$A$5)"):QString();};
 QTest::keyClicks(&fixture.bar,"=");QTest::keyClick(&fixture.bar,Qt::Key_Right);require(fixture.bar.text()=="=GETPIVOTDATA(\"Revenue\",$A$5)"&&calls==1,"single-cell keyboard pointing uses report lookup reference");
 QTest::keyClick(&fixture.bar,Qt::Key_Down,Qt::ShiftModifier);require(fixture.bar.text()=="=E1:E2"&&calls==1,"extending a report lookup uses ordinary A1 range without invoking single-cell hook");
 QTest::keyClick(&fixture.bar,Qt::Key_Right);require(fixture.bar.text()=="=F2"&&calls==2&&fixture.session.snapshot()==before,"empty custom result falls back to A1 and pointing stays uncommitted");
}
void standaloneViewCanCloseWithAnActiveDraft(){
 for(const bool modified:{false,true}){
 WorkbookSession session;WorksheetModel model(session);QWidget host;QLineEdit bar(&host);auto* view=new WorksheetView(&host);auto* layout=new QVBoxLayout(&host);layout->addWidget(&bar);layout->addWidget(view);view->setModel(&model);view->setFormulaBar(&bar);host.resize(600,400);host.show();QApplication::processEvents();view->setCurrentIndex(model.index(0,0));view->edit(model.index(0,0));QApplication::processEvents();
 QPointer<QLineEdit> editor=view->activeCellEditor();require(editor!=nullptr,"standalone closing fixture opens a native editor");editor->setFocus();QApplication::processEvents();require(editor->hasFocus(),"standalone closing fixture focuses its active editor");QTest::keyClicks(editor,"=10+20");const auto before=session.snapshot();
 editor->setModified(modified);std::cerr<<"Destroying standalone WorksheetView with active draft\n";delete view;QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
 Fixture replacement;
 require(session.snapshot()==before,"destroying an active worksheet view and creating its replacement preserves canonical state without heap corruption");
 }
}
void activeCellHasSuiteBorder(){
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(1,1));fixture.view.setFocus();QApplication::processEvents();const auto cell=fixture.view.visualRect(fixture.model.index(1,1));const auto image=fixture.view.viewport()->grab().toImage();int forest=0;
 for(int y=cell.top()+3;y<cell.bottom()-2;++y)for(int x=cell.left()-2;x<=cell.left()-1;++x)if(image.pixelColor(x,y)==QColor("#217346"))++forest;
 require(forest>12,"current worksheet cell has the visible source green selection border");
}
void outlinesRespectSheetAndViewportBounds(){
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(0,3));fixture.view.setCurrentSheetName("Sheet1");fixture.bar.setFocus();const auto before=fixture.session.snapshot();const auto first=fixture.view.visualRect(fixture.model.index(0,1)),last=fixture.view.visualRect(fixture.model.index(2,1));
 for(const auto* formula:{"=Other!B1:B3","=[Budget.xlsx]Sheet1!B1:B3","=\"B1:B3\""}){fixture.bar.setText(formula);QApplication::processEvents();require(blueEdge(fixture.view.viewport()->grab().toImage(),first,last)==0,"other-sheet external and literal references have no current-sheet outline");}
 fixture.bar.setText("=sheet1!B1:B3");QApplication::processEvents();require(blueEdge(fixture.view.viewport()->grab().toImage(),first,last)>20,"qualified current-sheet references match worksheet names case-insensitively");
 fixture.bar.setText("=B:B");fixture.view.scrollTo(fixture.model.index(200000,1));QApplication::processEvents();const int x=fixture.view.columnViewportPosition(1),height=fixture.view.viewport()->height();
 require(blueEdge(fixture.view.viewport()->grab().toImage(),QRect(x,0,fixture.view.columnWidth(1),1),QRect(x,height-2,fixture.view.columnWidth(1),2))>40,"whole-column outlines remain visible and clipped after scrolling far down the sheet");
 fixture.view.scrollTo(fixture.model.index(0,0));fixture.bar.setText("=5:5");QApplication::processEvents();const auto image=fixture.view.viewport()->grab().toImage();const int top=fixture.view.rowViewportPosition(4);int blue=0;
 for(int y=std::max(0,top);y<=std::min(image.height()-1,top+2);++y)for(int column=20;column<image.width()-20;++column){const auto color=image.pixelColor(column,y);if(color.blue()>color.red()+40&&color.blue()>color.green()+20)++blue;}
 require(blue>40&&fixture.session.snapshot()==before,"whole-row outlines clip to the visible viewport without canonical mutation");
}
void formulaBarCanBeDestroyedBeforeTheView(){
 WorkbookSession session;WorksheetModel model(session);QWidget host;auto* bar=new QLineEdit(&host);auto* view=new WorksheetView(&host);auto* layout=new QVBoxLayout(&host);layout->addWidget(bar);layout->addWidget(view);view->setModel(&model);view->setFormulaBar(bar);host.resize(600,400);host.show();view->setCurrentIndex(model.index(0,0));bar->setFocus();QApplication::processEvents();QTest::keyClicks(bar,"=SUM(B1:B3)");const auto before=session.snapshot();
 delete bar;QApplication::processEvents();view->setFocus();delete view;
 require(session.snapshot()==before,"destroying the formula bar before its live worksheet view leaves the workbook unchanged");
 Fixture replacement;
}
void calculationPreservesUnmodifiedEditorSelection(){
 Fixture fixture;fixture.model.setData(fixture.model.index(0,0),"=1+2",Qt::EditRole);fixture.view.setFocus();fixture.view.edit(fixture.model.index(0,0));QApplication::processEvents();auto* editor=fixture.view.activeCellEditor();require(editor!=nullptr&&editor->text()=="=1+2"&&!editor->isModified(),"unmodified editor initially receives canonical formula text");
 editor->setSelection(1,2);const auto cursor=editor->cursorPosition(),selection=editor->selectionStart();const auto selected=editor->selectedText();const auto before=fixture.session.snapshot();
 fixture.model.setCalculated({{"revision",before["revision"]},{"values",Json::array({{{"sheet",fixture.model.sheetId().toStdString()},{"row",1},{"col",1},{"type","number"},{"value",3}}})}});QApplication::processEvents();
 require(editor->text()=="=1+2"&&!editor->isModified()&&editor->cursorPosition()==cursor&&editor->selectionStart()==selection&&editor->selectedText()==selected,"calculation preserves caret and selection before an editor's first keystroke");
 require(fixture.session.snapshot()==before,"unmodified editor calculation delivery preserves canonical state");
}
void activeCellIsAccessible(){
 Fixture fixture;fixture.model.setData(fixture.model.index(0,0),"=1+2",Qt::EditRole);fixture.view.setFocus();fixture.view.edit(fixture.model.index(0,0));QApplication::processEvents();auto* editor=fixture.view.activeCellEditor();require(editor!=nullptr,"accessibility fixture opens the cell editor");editor->setFocus();QApplication::processEvents();
 auto* accessible=QAccessible::queryAccessibleInterface(editor);require(accessible!=nullptr,"native cell editor exposes a Qt accessibility interface");
 require(accessible->role()==QAccessible::EditableText&&accessible->text(QAccessible::Name)=="Cell A1","native cell editor exposes its editable role and cell address");
 require(accessible->text(QAccessible::Value)=="=1+2"&&accessible->state().focusable,"native cell editor exposes its formula value and keyboard focus capability");
 QTest::keyClick(editor,Qt::Key_End);QTest::keyClicks(editor,"+4");require(accessible->text(QAccessible::Value)=="=1+2+4","accessible value follows keyboard formula editing");
 fixture.view.cancelEntry();require(fixture.model.raw(0,0)=="=1+2","cancelling the accessible editor preserves the committed formula");
}
void inlineFormulaTransfersToBarWithoutCommit(){
 for(bool accept:{false,true}){
  Fixture f;const auto target=f.model.index(26,3);require(f.model.setData(target,"=-D69/SUM(D40:D41)",Qt::EditRole),"seed the independently observed denominator formula");f.view.setCurrentIndex(target);f.view.scrollTo(target);f.view.setFocus();const auto before=f.session.snapshot();QTest::keyClick(&f.view,Qt::Key_F2);auto* inlineEditor=f.view.activeCellEditor();require(inlineEditor,"real F2 opens the original inline formula");QTest::keyClick(inlineEditor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(inlineEditor,"=-D69/SUM(");const auto first=f.model.index(39,3),last=f.model.index(40,3);f.view.scrollTo(last,QAbstractItemView::PositionAtCenter);QApplication::processEvents();const auto a=f.view.visualRect(first).center(),b=f.view.visualRect(last).center();QTest::mousePress(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,a);QMouseEvent move(QEvent::MouseMove,QPointF(b),QPointF(f.view.viewport()->mapToGlobal(b)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(f.view.viewport(),&move);QTest::mouseRelease(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,b);require(inlineEditor->text()=="=-D69/SUM(D40:D41","prepare the actual pointed unfinished range without modifying the model");if(accept){QTest::keyClick(inlineEditor,Qt::Key_End);for(int n=0;n<3;++n)QTest::keyClick(inlineEditor,Qt::Key_Left,Qt::ShiftModifier);}
  f.bar.setFocus(Qt::OtherFocusReason);QApplication::processEvents();
  require(f.session.snapshot()==before&&f.model.raw(26,3)=="=-D69/SUM(D40:D41)","moving an unfinished inline formula to the formula bar must not commit a partial formula or create history");
  require(!f.view.activeCellEditor()&&f.bar.hasFocus()&&f.bar.text()=="=-D69/SUM(D40:D41"&&(accept?f.bar.selectedText()=="D41":f.bar.cursorPosition()==f.bar.text().size()),"formula-bar transfer retains the actual unfinished text and native caret or selected endpoint");
  QTest::keyClick(&f.bar,Qt::Key_End);for(int n=0;n<3;++n)QTest::keyClick(&f.bar,Qt::Key_Left,Qt::ShiftModifier);require(f.bar.selectedText()=="D41","source53:44 selects only the denominator endpoint in the actual formula bar");
  QTest::keyClicks(&f.bar,"D4");require(f.bar.text()=="=-D69/SUM(D40:D4"&&f.session.snapshot()==before,"source53:45 keeps the independently observed partial endpoint without a transaction");QTest::keyClicks(&f.bar,"2)");
  QTest::keyClick(&f.bar,accept?Qt::Key_Return:Qt::Key_Escape);QApplication::processEvents();
  if(accept)require(f.model.raw(26,3)=="=-D69/SUM(D40:D42)"&&f.session.snapshotView().at("revision").get<std::uint64_t>()==before.at("revision").get<std::uint64_t>()+1&&f.session.undo()&&f.session.snapshotView().at("sheets")==before.at("sheets"),"only Enter commits the endpoint correction, in exactly one native Undo");
  else require(f.session.snapshot()==before,"Escape from the transferred corrected draft preserves the original cells and history");
 }
}
void partialColumnTokenCanBePointed(){
 for(bool inlineEdit:{false,true}){
  Fixture f;const auto target=f.model.index(72,5),reference=f.model.index(28,5);require(f.model.setData(target,"=-SUM(F$40:F$42)*F31",Qt::EditRole)&&f.model.setData(reference,"0.131",Qt::EditRole),"prepare independent source formula and pointed driver");f.view.setCurrentIndex(target);f.view.scrollTo(target);f.view.setFocus();const auto before=f.session.snapshot();QLineEdit* editor=nullptr;
  if(inlineEdit){QTest::keyClick(&f.view,Qt::Key_F2);editor=f.view.activeCellEditor();require(editor,"F2 opens the copied source formula");QTest::keyClick(editor,Qt::Key_End);QTest::keyClick(editor,Qt::Key_Backspace);QTest::keyClick(editor,Qt::Key_Backspace);}
  else{f.bar.setFocus();editor=&f.bar;QTest::keyClicks(editor,"=-SUM(F$40:F$42)*F");}
  require(editor->text()=="=-SUM(F$40:F$42)*F"&&f.session.snapshot()==before,"source55:56 leaves the actual partial F token without changing the workbook");if(inlineEdit)require(f.view.property("worksheet.entryMode").toString()=="Edit","fresh F2 remains in the source Edit mode when the native formula suffix is corrected");f.view.scrollTo(reference,QAbstractItemView::PositionAtCenter);QApplication::processEvents();QTest::mouseClick(f.view.viewport(),Qt::LeftButton,Qt::NoModifier,f.view.visualRect(reference).center());QApplication::processEvents();
  require(editor->text()=="=-SUM(F$40:F$42)*F29"&&f.bar.text()==editor->text()&&f.view.currentIndex()==target&&f.session.snapshot()==before,"actual pointing replaces the incomplete column token with F29, preserving the draft destination and history");
  require(f.view.property("worksheet.entryMode").toString()==(inlineEdit?"Edit":"Point"),"pointing preserves an existing formula's source Edit mode while a new draft still enters Point");
  const auto rect=f.view.visualRect(reference);const auto image=f.view.viewport()->grab().toImage();int red=0;for(int y=rect.top()+3;y<rect.bottom()-3;++y)for(int x=rect.left();x<=rect.left()+2;++x){const auto c=image.pixelColor(x,y);if(c.red()>140&&c.red()>c.green()+50&&c.red()>c.blue()+25)++red;}require(red>10,"source55:57 creates the native red second-reference rectangle on the actual F29 cell");
  QTest::keyClick(editor,Qt::Key_Return);QApplication::processEvents();require(f.model.raw(72,5)=="=-SUM(F$40:F$42)*F29"&&f.session.snapshotView().at("revision").get<std::uint64_t>()==before.at("revision").get<std::uint64_t>()+1&&f.session.undo()&&f.session.snapshotView().at("sheets")==before.at("sheets"),"pointing and Enter commit exactly one formula transaction and one Undo");
 }
}

void missingFunctionCloseOffersNativeCorrection(){
 for(const QString original:{QString("=-D69/SUM(D40:D41"),QString("=SUM(\"<b>\""),QString("=SUM('O''(Brien'!A1")})for(bool inlineEdit:{false,true})for(bool accept:{false,true}){
  Fixture f;const auto target=f.model.index(0,0);require(f.model.setData(target,"original",Qt::EditRole),"seed correction Undo boundary");f.view.setCurrentIndex(target);f.view.setFocus();QLineEdit* editor=&f.bar;if(inlineEdit){f.view.edit(target);QApplication::processEvents();editor=f.view.activeCellEditor();require(editor,"native inline correction editor opens");QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);}else{f.bar.setFocus();f.bar.clear();}
  const QString corrected=original+")";QTest::keyClicks(editor,original);const auto before=f.session.snapshot();const auto cursor=editor->cursorPosition();QList<int> buttons,icons;QStringList messages;bool plainText=true;QPointer<QMessageBox> last;QTimer timer;timer.setInterval(5);QObject::connect(&timer,&QTimer::timeout,[&]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!box||box==last)return;last=box;buttons.append(int(box->standardButtons()));icons.append(int(box->icon()));messages.append(box->text());if(box->standardButtons().testFlag(QMessageBox::Yes)){plainText=plainText&&box->textFormat()==Qt::PlainText;box->button(accept?QMessageBox::Yes:QMessageBox::No)->click();}else if(box->button(QMessageBox::Ok))box->button(QMessageBox::Ok)->click();else box->reject();});timer.start();QTest::keyClick(editor,Qt::Key_Return);timer.stop();
  require(!buttons.isEmpty()&&buttons.first()==int(QMessageBox::Yes|QMessageBox::No)&&icons.first()==int(QMessageBox::Information),"a single missing function close must offer a genuine Information Yes/No correction before the existing warning");require(plainText&&messages.first().contains(corrected)&&messages.first().contains("Do you want to accept this correction?"),"native correction describes the complete literal candidate rather than silently rewriting or rendering formula HTML");
  if(accept){require(buttons.size()==1&&f.model.raw(0,0)==corrected&&f.session.snapshotView().at("revision").get<std::uint64_t>()==before.at("revision").get<std::uint64_t>()+1,"Yes commits the corrected formula once");require(f.session.undo()&&f.session.snapshotView().at("sheets")==before.at("sheets"),"one Undo restores the original input after accepted correction");}
  else{require(buttons.size()==2&&buttons.last()==int(QMessageBox::Ok)&&icons.last()==int(QMessageBox::Warning)&&messages.last().contains("missing an opening or closing parenthesis"),"No retains the existing yellow missing-parenthesis warning");require(editor->text()==original&&editor->cursorPosition()==cursor&&f.view.currentIndex()==target&&f.session.snapshot()==before,"rejecting correction preserves native draft, caret, destination and history");f.view.cancelEntry();}
 }
}
void unsafeParenthesisCorrectionsRemainWarnings(){
 for(const QString original:{QString("=SUM(A1+"),QString("=SUM((A1"),QString("=(A1+1"),QString("=SUM(\"unterminated"),QString("=SUM(Table[Column"),QString("=A1)")}){
  Fixture f;f.bar.setFocus();QTest::keyClicks(&f.bar,original);const auto before=f.session.snapshot();QList<int> buttons;QPointer<QMessageBox> last;QTimer timer;timer.setInterval(5);QObject::connect(&timer,&QTimer::timeout,[&]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!box||box==last)return;last=box;buttons.append(int(box->standardButtons()));if(box->button(QMessageBox::Ok))box->button(QMessageBox::Ok)->click();else box->reject();});timer.start();QTest::keyClick(&f.bar,Qt::Key_Return);timer.stop();require(buttons==QList<int>{int(QMessageBox::Ok)}&&f.bar.text()==original&&f.session.snapshot()==before,"unsupported malformed expressions retain the ordinary warning without an invented candidate");f.view.cancelEntry();
 }
}
int main(int argc,char** argv){QApplication app(argc,argv);try{std::cout<<"Qt platform: "<<QApplication::platformName().toStdString()<<'\n';const auto selected=argc>1?std::string(argv[1]):std::string();for(auto test:{std::pair{"dirty",calculationPreservesDirtyEntry},std::pair{"selected_outline",selectedFormulaHasReferenceOutline},std::pair{"selected_refresh",selectedFormulaChangeRefreshesReferenceOutlines},std::pair{"selected_draft",draftOutlinesOverrideSelectedFormula},std::pair{"outline",activeDraftHasReferenceOutline},std::pair{"keyboard",keyboardPointingIsUncommittedAndCompletionFirst},std::pair{"pivot_point",pivotPointingHook},std::pair{"teardown",standaloneViewCanCloseWithAnActiveDraft},std::pair{"border",activeCellHasSuiteBorder},std::pair{"bounds",outlinesRespectSheetAndViewportBounds},std::pair{"bar_first",formulaBarCanBeDestroyedBeforeTheView},std::pair{"clean_selection",calculationPreservesUnmodifiedEditorSelection},std::pair{"accessible",activeCellIsAccessible},std::pair{"bar_transfer",inlineFormulaTransfersToBarWithoutCommit},std::pair{"partial_point",partialColumnTokenCanBePointed},std::pair{"correction",missingFunctionCloseOffersNativeCorrection},std::pair{"correction_controls",unsafeParenthesisCorrectionsRemainWarnings}})if(selected.empty()||selected==test.first)test.second();std::cout<<"worksheet entry tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
