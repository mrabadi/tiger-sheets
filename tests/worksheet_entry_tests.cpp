#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QTest>
#include <QVBoxLayout>
#include <QPointer>
#include <QAccessible>
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
 Fixture fixture;fixture.view.setCurrentIndex(fixture.model.index(0,0));fixture.view.setFocus();QApplication::processEvents();const auto cell=fixture.view.visualRect(fixture.model.index(0,0));const auto image=fixture.view.viewport()->grab().toImage();int forest=0;
 for(int y=cell.top()+3;y<cell.bottom()-2;++y)for(int x=cell.left();x<=cell.left()+2;++x)if(image.pixelColor(x,y)==QColor("#245648"))++forest;
 require(forest>12,"current worksheet cell has a visible forest suite border");
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
int main(int argc,char** argv){QApplication app(argc,argv);try{std::cout<<"Qt platform: "<<QApplication::platformName().toStdString()<<'\n';const auto selected=argc>1?std::string(argv[1]):std::string();for(auto test:{std::pair{"dirty",calculationPreservesDirtyEntry},std::pair{"outline",activeDraftHasReferenceOutline},std::pair{"keyboard",keyboardPointingIsUncommittedAndCompletionFirst},std::pair{"pivot_point",pivotPointingHook},std::pair{"teardown",standaloneViewCanCloseWithAnActiveDraft},std::pair{"border",activeCellHasSuiteBorder},std::pair{"bounds",outlinesRespectSheetAndViewportBounds},std::pair{"bar_first",formulaBarCanBeDestroyedBeforeTheView},std::pair{"clean_selection",calculationPreservesUnmodifiedEditorSelection},std::pair{"accessible",activeCellIsAccessible}})if(selected.empty()||selected==test.first)test.second();std::cout<<"worksheet entry tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
