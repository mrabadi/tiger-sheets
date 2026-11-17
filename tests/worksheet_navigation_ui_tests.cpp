#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QScrollBar>
#include <QLineEdit>
#include <QLabel>
#include <QMouseEvent>
#include <QTabBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>
#include <utility>
using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
 MainWindow window;QTableView* grid;QTabBar* tabs;WorksheetModel* model;
 Fixture(){window.resize(1400,850);window.show();QApplication::processEvents();grid=window.findChild<QTableView*>("worksheet.grid");tabs=window.findChild<QTabBar*>("workspace.sheets");model=dynamic_cast<WorksheetModel*>(grid->model());require(grid&&tabs&&model,"worksheet navigation controls exist");require(window.executeCommand("sheet.add")&&window.executeCommand("sheet.add"),"create three worksheets");clickTab(0);}
 void clickTab(int index){QTest::mouseClick(tabs,Qt::LeftButton,Qt::NoModifier,tabs->tabRect(index).center());QApplication::processEvents();}
 void key(Qt::Key key){grid->setFocus();QTest::keyClick(grid,key,Qt::ControlModifier);QApplication::processEvents();}
};
void sourceFarRightViewport(){
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();auto* grid=window.findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());const auto before=window.currentSession().snapshot();grid->setCurrentIndex(model->index(24,0));grid->selectionModel()->select(QItemSelection(model->index(24,0),model->index(24,model->columnCount()-1)),QItemSelectionModel::ClearAndSelect);auto* scroll=grid->horizontalScrollBar();scroll->setFocus();QTest::keyClick(scroll,Qt::Key_End);QApplication::processEvents();std::cout<<"end first="<<grid->columnAt(0)+1<<" position="<<grid->columnViewportPosition(16365)<<" last="<<grid->columnViewportPosition(16383)+grid->columnWidth(16383)<<" viewport="<<grid->viewport()->width()<<std::endl;require(grid->columnAt(0)==16365&&grid->columnViewportPosition(16365)==0,"source63:26 horizontal End begins with full XEL rather than partially clipped XEK");require(grid->columnViewportPosition(16383)+grid->columnWidth(16383)<=grid->viewport()->width(),"aligned far-right viewport still shows XFD fully");require(grid->currentIndex()==model->index(24,0)&&grid->selectionModel()->selection().front().width()==model->columnCount()&&window.currentSession().snapshot()==before,"far-right scrolling retains the whole-row selection, active anchor and workbook");QTest::keyClick(scroll,Qt::Key_Home);QApplication::processEvents();require(grid->columnAt(0)==0&&grid->columnViewportPosition(0)==0,"horizontal Home snaps back to full column A");
}
void nativeEntryContextIndicators(){
 Fixture f;
 auto* view=dynamic_cast<WorksheetView*>(f.grid);
 auto* name=f.window.findChild<QLineEdit*>("formula.name");
 auto* bar=f.window.findChild<QLineEdit*>("formula.input");
 auto* state=f.window.findChild<QLabel*>("status.saveState");
 require(view&&name&&bar&&state,"native entry surface controls exist");
 const auto origin=f.model->index(0,0),pointed=f.model->index(1,2);
 const auto before=f.window.currentSession().snapshot();
 const auto selection=f.grid->selectionModel()->selection();
 const auto history=f.window.currentSession().historyBytes();
 const bool undo=f.window.currentSession().canUndo(),redo=f.window.currentSession().canRedo();
 const auto unchanged=[&]{require(f.window.currentSession().snapshot()==before&&
   f.window.currentSession().historyBytes()==history&&f.window.currentSession().canUndo()==undo&&
   f.window.currentSession().canRedo()==redo&&f.grid->currentIndex()==origin&&
   f.grid->selectionModel()->selection()==selection&&!view->activeCellEditor()&&!view->hasReferenceSheetEntry(),
   "entry-mode changes and local reference pointing preserve the origin, selection, workbook and undo/redo history");};
 const auto mode=[&](const char* value){require(state->text()==value&&view->property("worksheet.entryMode").toString()==value,
   "visible entry status agrees with the native worksheet entry mode");};
 require(state->text()=="Ready"&&name->text()=="A1","an idle worksheet displays Ready and its original address");
 bar->setFocus();QTest::keyClicks(bar,"=EOMONTH(");
 mode("Enter");require(name->text()=="A1"&&bar->text()=="=EOMONTH("&&bar->isModified()&&bar->hasFocus(),
   "new formula-bar Enter retains origin A1 even when the draft contains a function");unchanged();
 QTest::keyClick(bar,Qt::Key_F2);
 mode("Edit");require(name->text().isEmpty()&&bar->text()=="=EOMONTH("&&bar->isModified()&&bar->hasFocus(),
   "formula-bar Edit has an empty Name Box without changing its draft");unchanged();
 QTest::keyClick(bar,Qt::Key_F2);
 mode("Enter");require(name->text()=="A1"&&bar->text()=="=EOMONTH(",
   "F2 returns the new formula-bar draft to Enter and restores origin A1");unchanged();
 QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(pointed).center());
 mode("Point");require(name->text()=="C2"&&bar->text()=="=EOMONTH(C2"&&bar->isModified(),
   "a newly pointed single local cell exposes endpoint C2 in the Name Box and real formula buffer");unchanged();
 const auto draft=bar->text();QTest::keyClick(bar,Qt::Key_Escape);
 mode("Point");require(name->text()=="C2"&&bar->text()==draft&&bar->isModified(),
   "the first Escape dismisses formula assistance while retaining the Point endpoint and exact draft");unchanged();
 QTest::keyClick(bar,Qt::Key_Escape);
 mode("Ready");require(name->text()=="A1"&&bar->text().isEmpty()&&!bar->isModified()&&f.grid->hasFocus(),
   "the second Escape cancels entry and restores the original native address, bar and grid focus");unchanged();
}
void draggedSelectionKeepsActiveAnchor(){
 Fixture f;auto* name=f.window.findChild<QLineEdit*>("formula.name");const auto first=f.model->index(1,1),last=f.model->index(2,3);require(f.model->setData(first,"Anchor",Qt::EditRole)&&f.model->setData(last,"Endpoint",Qt::EditRole),"seed visible selection endpoints");const auto before=f.window.currentSession().snapshot();QTest::mousePress(f.grid->viewport(),Qt::LeftButton,Qt::NoModifier,f.grid->visualRect(first).center());{const auto position=f.grid->visualRect(last).center();QMouseEvent move(QEvent::MouseMove,position,f.grid->viewport()->mapToGlobal(position),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(f.grid->viewport(),&move);}QApplication::processEvents();require(name->text()=="2R x 3C","dragging a selection displays its row and column counts in the name box");QTest::mouseRelease(f.grid->viewport(),Qt::LeftButton,Qt::NoModifier,f.grid->visualRect(last).center());QApplication::processEvents();require(f.grid->currentIndex()==first&&name->text()=="B2"&&f.window.findChild<QLineEdit*>("formula.input")->text()=="Anchor","releasing a range retains its original active cell, address and formula-bar content");const auto selection=f.grid->selectionModel()->selection();require(selection.size()==1&&selection.front().topLeft()==first&&selection.front().bottomRight()==last&&f.window.currentSession().snapshot()==before,"the full rectangular selection survives with no workbook edit");
}
void keyboardRangeKeepsActiveAnchor(){
 Fixture f;auto* name=f.window.findChild<QLineEdit*>("formula.name");const auto anchor=f.model->index(1,1);for(int column=1;column<4;++column)require(f.model->setData(f.model->index(1,column),QString::number(column),Qt::EditRole),"seed a contiguous keyboard navigation row");const auto before=f.window.currentSession().snapshot();f.grid->setCurrentIndex(anchor);f.grid->setFocus();for(const auto key:{Qt::Key_Down,Qt::Key_Down,Qt::Key_Right,Qt::Key_Right})QTest::keyClick(f.grid,key,Qt::ShiftModifier);auto range=f.grid->selectionModel()->selection().front();require(f.grid->currentIndex()==anchor&&range.topLeft()==anchor&&range.bottomRight()==f.model->index(3,3)&&name->text()=="B2","Shift arrows extend the selected rectangle while retaining active B2 and its address after Shift release");QTest::keyClick(f.grid,Qt::Key_Right);require(f.grid->currentIndex()==f.model->index(1,2)&&f.grid->selectionModel()->selection().front().width()==1,"an ordinary arrow leaves the rectangle from its active anchor");f.grid->setCurrentIndex(anchor);QTest::keyClick(f.grid,Qt::Key_Right,Qt::ControlModifier|Qt::ShiftModifier);range=f.grid->selectionModel()->selection().front();require(f.grid->currentIndex()==anchor&&range.bottomRight()==f.model->index(1,3),"CtrlShiftRight reaches the contiguous edge while retaining its active anchor");QTest::keyClick(f.grid,Qt::Key_Down,Qt::ShiftModifier);range=f.grid->selectionModel()->selection().front();require(f.grid->currentIndex()==anchor&&range.bottomRight()==f.model->index(2,3),"ShiftDown continues from the selection extent after CtrlShiftRight");require(f.window.currentSession().snapshot()==before,"keyboard range extension never changes values, formatting or history");
}

void tabTraversesSelectedRectangle(){
 Fixture f;auto* view=dynamic_cast<WorksheetView*>(f.grid);auto* name=f.window.findChild<QLineEdit*>("formula.name");auto* bar=f.window.findChild<QLineEdit*>("formula.input");require(view&&name&&bar,"rectangle Tab controls exist");
 for(int row=35;row<38;++row)require(f.model->setData(f.model->index(row,3),QString::number(row-34),Qt::EditRole),"seed distinct source-column values");
 const auto before=f.window.currentSession().snapshot();const auto history=f.window.currentSession().historyBytes();const bool undo=f.window.currentSession().canUndo(),redo=f.window.currentSession().canRedo();
 const auto origin=f.model->index(35,3);f.grid->setCurrentIndex(origin);f.grid->setFocus();QTest::keyClick(f.grid,Qt::Key_C,Qt::ControlModifier);QApplication::processEvents();const auto copied=QApplication::clipboard()->mimeData()->data("application/x-tigersheets-cells+json");const auto copiedText=QApplication::clipboard()->text();require(!copied.isEmpty(),"real source-cell copy populates native clipboard");auto* copyBorder=view->findChild<QWidget*>("worksheet.copyBorder");auto* copyState=f.window.findChild<QLabel*>("status.saveState");require(copyBorder&&copyState,"actual copy outline and status exist");
 QTest::keyClick(f.grid,Qt::Key_Down,Qt::ShiftModifier);QTest::keyClick(f.grid,Qt::Key_Down,Qt::ShiftModifier);QApplication::processEvents();const auto sourceRange=f.grid->selectionModel()->selection();require(sourceRange.size()==1&&sourceRange.front().topLeft()==origin&&sourceRange.front().bottomRight()==f.model->index(37,3),"real ShiftDown selects source D36:D38");
 const auto at=[&](int row,int col,const QItemSelection& selected){require(f.grid->currentIndex()==f.model->index(row,col)&&f.grid->selectionModel()->selection()==selected,"Tab changes only active cell within the selected rectangle");require(!view->activeCellEditor()&&name->text()==QString::fromStdString(columnName(col+1))+QString::number(row+1)&&bar->text()==f.model->data(f.model->index(row,col),Qt::EditRole).toString(),"Tab updates native address/bar without opening an editor");require(f.window.currentSession().snapshot()==before&&f.window.currentSession().historyBytes()==history&&f.window.currentSession().canUndo()==undo&&f.window.currentSession().canRedo()==redo,"rectangle traversal preserves workbook and Undo/Redo history");require(QApplication::clipboard()->mimeData()->data("application/x-tigersheets-cells+json")==copied&&QApplication::clipboard()->text()==copiedText,"rectangle traversal preserves the actual copied source");require(copyState->text()=="Select destination and press ENTER or choose Paste","Tab preserves actual source copy-mode status");if(row>=35&&row<=37)require(copyBorder->isVisible()&&copyBorder->property("sourceRect").toRect().contains(view->visualRect(origin).center()),"Tab keeps real D36 copied-cell outline while traversing its destination range");};
 QTest::keyClick(f.grid,Qt::Key_Tab);QApplication::processEvents();at(36,3,sourceRange);QTest::keyClick(f.grid,Qt::Key_Tab);at(37,3,sourceRange);QTest::keyClick(f.grid,Qt::Key_Tab);at(35,3,sourceRange);QTest::keyClick(f.grid,Qt::Key_Tab,Qt::ShiftModifier);at(37,3,sourceRange);QTest::keyClick(f.grid,Qt::Key_Backtab,Qt::ShiftModifier);at(36,3,sourceRange);
 f.grid->setCurrentIndex(f.model->index(1,1));QTest::keyClick(f.grid,Qt::Key_Down,Qt::ShiftModifier);QTest::keyClick(f.grid,Qt::Key_Right,Qt::ShiftModifier);QTest::keyClick(f.grid,Qt::Key_Right,Qt::ShiftModifier);const auto rectangle=f.grid->selectionModel()->selection();require(rectangle.size()==1&&rectangle.front().topLeft()==f.model->index(1,1)&&rectangle.front().bottomRight()==f.model->index(2,3),"real arrows select two-by-three traversal control");
 for(const auto target:{std::pair{1,2},std::pair{1,3},std::pair{2,1},std::pair{2,2},std::pair{2,3},std::pair{1,1}}){QTest::keyClick(f.grid,Qt::Key_Tab);at(target.first,target.second,rectangle);}QTest::keyClick(f.grid,Qt::Key_Tab,Qt::ShiftModifier);at(2,3,rectangle);
 f.grid->setCurrentIndex(f.model->index(7,6));f.grid->selectionModel()->select(QItemSelection(f.model->index(7,6),f.model->index(7,6)),QItemSelectionModel::ClearAndSelect);QTest::keyClick(f.grid,Qt::Key_Tab);QApplication::processEvents();require(f.grid->currentIndex()==f.model->index(7,7)&&f.grid->selectionModel()->selection().size()==1&&f.grid->selectionModel()->selection().front().width()==1,"collapsed native Tab still moves one column normally");require(f.window.currentSession().snapshot()==before&&f.window.currentSession().historyBytes()==history,"collapsed Tab also preserves workbook/history");
}

void returnTraversesSelectedRectangle(){
 {
  Fixture f;auto* view=dynamic_cast<WorksheetView*>(f.grid);require(view,"Return editor controls exist");
  const auto single=f.model->index(7,6);require(f.model->setData(single,"7",Qt::EditRole),"seed ordinary Return control");f.grid->setCurrentIndex(single);f.grid->setFocus();const auto before=f.window.currentSession().snapshot();
  QTest::keyClick(f.grid,Qt::Key_Return);QApplication::processEvents();require(!view->activeCellEditor()&&f.grid->currentIndex()==single&&f.window.currentSession().snapshot()==before,"ordinary single-cell Return retains the existing idle cell without editing or moving it");QTest::keyClick(f.grid,Qt::Key_F2);require(view->activeCellEditor(),"explicit F2 still opens an editor for the ordinary commit control");auto* editor=view->activeCellEditor();editor->selectAll();QTest::keyClicks(editor,"17");QTest::keyClick(editor,Qt::Key_Return);require(!view->activeCellEditor()&&f.grid->currentIndex()==f.model->index(8,6)&&f.model->data(single,Qt::EditRole).toString()=="17","an actual editor Return still commits and moves down normally");
  const auto retained=f.model->index(7,7);require(f.model->setData(retained,"11",Qt::EditRole),"seed retained editor control");f.grid->setCurrentIndex(retained);f.grid->setFocus();QTest::keyClick(f.grid,Qt::Key_F2);require(view->activeCellEditor(),"F2 still opens an actual editor");editor=view->activeCellEditor();editor->selectAll();QTest::keyClicks(editor,"23");QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);require(!view->activeCellEditor()&&f.grid->currentIndex()==retained&&f.model->data(retained,Qt::EditRole).toString()=="23","editor CtrlReturn still commits while retaining its origin");
 }
 Fixture f;auto* view=dynamic_cast<WorksheetView*>(f.grid);auto* name=f.window.findChild<QLineEdit*>("formula.name");auto* bar=f.window.findChild<QLineEdit*>("formula.input");auto* state=f.window.findChild<QLabel*>("status.saveState");require(view&&name&&bar&&state,"selected-range Return controls exist");
 for(int row=3;row<=4;++row)for(int column=2;column<=4;++column)require(f.model->setData(f.model->index(row,column),QString::number(row*10+column),Qt::EditRole),"seed distinct rectangular navigation cells");
 for(int row=35;row<38;++row)require(f.model->setData(f.model->index(row,3),QString::number(row),Qt::EditRole),"seed distinct vertical navigation cells");
 const auto before=f.window.currentSession().snapshot();const auto history=f.window.currentSession().historyBytes();const bool undo=f.window.currentSession().canUndo(),redo=f.window.currentSession().canRedo();
 const auto copy=[&]{QTest::keyClick(f.grid,Qt::Key_C,Qt::ControlModifier);QApplication::processEvents();return std::pair{QApplication::clipboard()->mimeData()->data("application/x-tigersheets-cells+json"),QApplication::clipboard()->text()};};
 f.grid->setCurrentIndex(f.model->index(3,2));f.grid->setFocus();auto copied=copy();QTest::keyClick(f.grid,Qt::Key_Down,Qt::ShiftModifier);QTest::keyClick(f.grid,Qt::Key_Right,Qt::ShiftModifier);QTest::keyClick(f.grid,Qt::Key_Right,Qt::ShiftModifier);auto selected=f.grid->selectionModel()->selection();require(selected.size()==1&&selected.front().topLeft()==f.model->index(3,2)&&selected.front().bottomRight()==f.model->index(4,4),"real Shift arrows select C4:E5");
 QTest::keyClick(f.grid,Qt::Key_Return);QApplication::processEvents();require(!view->activeCellEditor()&&f.grid->currentIndex()==f.model->index(3,2)&&f.grid->selectionModel()->selection()==selected&&f.window.currentSession().snapshot()==before&&state->text()=="Select destination and press ENTER or choose Paste","copy-active Return retains the preexisting idle selection and copy mode");QTest::keyClick(f.grid,Qt::Key_Escape);QApplication::processEvents();require(state->text()=="Ready","actual Escape leaves copy mode before the independent no-copy navigation test");
 const auto at=[&](int row,int column){QApplication::processEvents();require(!view->activeCellEditor()&&f.grid->currentIndex()==f.model->index(row,column)&&f.grid->selectionModel()->selection()==selected,"Ready Return traverses the selected rectangle columnwise without opening an editor or changing its range");require(name->text()==QString::fromStdString(columnName(column+1))+QString::number(row+1)&&bar->text()==f.model->data(f.model->index(row,column),Qt::EditRole).toString(),"selected-range Return updates the actual Name Box and formula bar");require(f.window.currentSession().snapshot()==before&&f.window.currentSession().historyBytes()==history&&f.window.currentSession().canUndo()==undo&&f.window.currentSession().canRedo()==redo,"selected-range Return preserves all workbook data and Undo/Redo history");require(QApplication::clipboard()->mimeData()->data("application/x-tigersheets-cells+json")==copied.first&&QApplication::clipboard()->text()==copied.second&&state->text()=="Ready","no-copy selected-range Return retains clipboard contents and the actual Ready footer");};
 for(const auto target:{std::pair{4,2},std::pair{3,3},std::pair{4,3},std::pair{3,4},std::pair{4,4},std::pair{3,2}}){QTest::keyClick(f.grid,Qt::Key_Return);at(target.first,target.second);}QTest::keyClick(f.grid,Qt::Key_Return,Qt::ShiftModifier);at(4,4);QTest::keyClick(f.grid,Qt::Key_Enter,Qt::ShiftModifier);at(3,4);QTest::keyClick(f.grid,Qt::Key_Enter);at(4,4);
 f.grid->setCurrentIndex(f.model->index(35,3));copied=copy();QTest::keyClick(f.grid,Qt::Key_Escape);require(state->text()=="Ready","vertical range navigation also begins outside copy mode");QTest::keyClick(f.grid,Qt::Key_Down,Qt::ShiftModifier);QTest::keyClick(f.grid,Qt::Key_Down,Qt::ShiftModifier);selected=f.grid->selectionModel()->selection();require(selected.size()==1&&selected.front().topLeft()==f.model->index(35,3)&&selected.front().bottomRight()==f.model->index(37,3),"real native arrows select copied D36:D38");
 QTest::keyClick(f.grid,Qt::Key_Return);at(36,3);QTest::keyClick(f.grid,Qt::Key_Enter);at(37,3);QTest::keyClick(f.grid,Qt::Key_Return);at(35,3);QTest::keyClick(f.grid,Qt::Key_Return,Qt::ShiftModifier);at(37,3);auto* outline=view->findChild<QWidget*>("worksheet.copyBorder");require(outline&&!outline->isVisible(),"no-copy Return never resurrects the cleared source copy outline");
}

void keyboardSwitchesAdjacentSheets(){
 Fixture f;const auto before=f.window.currentSession().snapshot();
 f.key(Qt::Key_PageUp);require(f.tabs->currentIndex()==0,"Ctrl+PageUp stays at the first worksheet");
 f.key(Qt::Key_PageDown);require(f.tabs->currentIndex()==1&&f.model->sheetId()==f.tabs->tabData(1).toString(),"Ctrl+PageDown activates the next worksheet");
 f.key(Qt::Key_PageDown);require(f.tabs->currentIndex()==2,"Ctrl+PageDown reaches the last worksheet");
 f.key(Qt::Key_PageDown);require(f.tabs->currentIndex()==2,"Ctrl+PageDown stays at the last worksheet");
 f.key(Qt::Key_PageUp);require(f.tabs->currentIndex()==1&&f.model->sheetId()==f.tabs->tabData(1).toString(),"Ctrl+PageUp activates the previous worksheet");
 require(f.grid->hasFocus(),"worksheet keyboard navigation retains grid focus");
 require(f.window.currentSession().snapshot()==before,"worksheet navigation does not modify workbook data or history");
}
void sheetsRememberTheirSelectionAndScroll(){
 Fixture f;const auto before=f.window.currentSession().snapshot();
 const auto first=f.model->index(45,18),last=f.model->index(48,21);
 f.grid->setCurrentIndex(first);f.grid->selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);
 f.grid->horizontalScrollBar()->setValue(1400);f.grid->verticalScrollBar()->setValue(800);QApplication::processEvents();
 const auto firstSelection=f.grid->selectionModel()->selection();const auto firstHorizontal=f.grid->horizontalScrollBar()->value(),firstVertical=f.grid->verticalScrollBar()->value();
 require(firstHorizontal>0&&firstVertical>0,"selection fixture scrolls both worksheet axes");
 f.clickTab(1);require(f.grid->currentIndex()==f.model->index(0,0),"a previously unvisited worksheet starts at A1");
 const auto second=f.model->index(20,8);f.grid->setCurrentIndex(second);f.grid->horizontalScrollBar()->setValue(600);f.grid->verticalScrollBar()->setValue(400);QApplication::processEvents();
 const auto secondSelection=f.grid->selectionModel()->selection();const auto secondHorizontal=f.grid->horizontalScrollBar()->value(),secondVertical=f.grid->verticalScrollBar()->value();
 f.clickTab(0);
 require(f.grid->currentIndex()==first,"clicking a worksheet tab restores its active cell");
 require(f.grid->selectionModel()->selection()==firstSelection,"clicking a worksheet tab restores its selected range");
 require(f.grid->horizontalScrollBar()->value()==firstHorizontal&&f.grid->verticalScrollBar()->value()==firstVertical,"clicking a worksheet tab restores both scroll positions");
 f.key(Qt::Key_PageDown);
 require(f.grid->currentIndex()==second&&f.grid->selectionModel()->selection()==secondSelection,"keyboard worksheet switching restores the destination selection");
 require(f.grid->horizontalScrollBar()->value()==secondHorizontal&&f.grid->verticalScrollBar()->value()==secondVertical,"keyboard worksheet switching restores destination scroll positions");
 require(f.window.currentSession().snapshot()==before,"remembering worksheet view positions does not edit workbook data or history");
}
void addingSheetRetainsThePreviousView(){
 Fixture f;const auto original=f.model->index(35,14);f.grid->setCurrentIndex(original);f.grid->horizontalScrollBar()->setValue(1000);f.grid->verticalScrollBar()->setValue(600);QApplication::processEvents();const auto horizontal=f.grid->horizontalScrollBar()->value(),vertical=f.grid->verticalScrollBar()->value();
 require(f.window.executeCommand("sheet.add"),"add a worksheet while viewing distant cells");
 require(f.grid->currentIndex()==f.model->index(0,0)&&f.grid->horizontalScrollBar()->value()==0&&f.grid->verticalScrollBar()->value()==0,"a new worksheet starts at A1 with an unscrolled view");
 f.clickTab(0);require(f.grid->currentIndex()==original&&f.grid->horizontalScrollBar()->value()==horizontal&&f.grid->verticalScrollBar()->value()==vertical,"adding a worksheet retains the previous worksheet's active cell and scroll");
}
void nativeWorkbookViewSurvivesSave(){
 Fixture f;QTemporaryDir files;const auto path=files.filePath("view-state.xlsx");
 f.grid->setCurrentIndex(f.model->index(1,1));f.grid->selectionModel()->select(QItemSelection(f.model->index(1,1),f.model->index(3,4)),QItemSelectionModel::ClearAndSelect);
 f.clickTab(1);f.grid->setCurrentIndex(f.model->index(45,18));f.grid->selectionModel()->select(QItemSelection(f.model->index(45,18),f.model->index(47,20)),QItemSelectionModel::ClearAndSelect);f.grid->scrollTo(f.model->index(39,15),QAbstractItemView::PositionAtTop);QApplication::processEvents();
 const auto topRow=f.grid->rowAt(0),topColumn=f.grid->columnAt(0);const auto before=f.window.currentSession().snapshot();require(f.window.saveWorkbook(path),"save current worksheet view");require(f.window.currentSession().snapshot()==before,"saving selection and viewport does not create workbook cell edits or history");
 const auto package=readWorkbook(path.toStdString());const auto& native=package.entries.at(package.sheetParts.at(package.workbook.at("sheets")[1].at("id").get<std::string>()));require(native.find("activeCell=\"S46\"")!=std::string::npos&&native.find("sqref=\"S46:U48\"")!=std::string::npos&&native.find("topLeftCell=\"")!=std::string::npos,"selection and top-left cell are native OOXML rather than application-only metadata");
 require(f.window.openWorkbook(path),"reopen saved native worksheet view");QApplication::processEvents();require(f.tabs->currentIndex()==1&&f.grid->currentIndex()==f.model->index(45,18),"native workbook active tab and active cell survive reopen");const auto selection=f.grid->selectionModel()->selection();require(selection.size()==1&&selection.front().top()==45&&selection.front().left()==18&&selection.front().bottom()==47&&selection.front().right()==20,"native selected rectangle survives reopen");require(f.grid->rowAt(0)==topRow&&f.grid->columnAt(0)==topColumn,"native top-left visible cell survives reopen");
 f.clickTab(0);require(f.grid->currentIndex()==f.model->index(1,1)&&f.grid->selectionModel()->selection().front().bottom()==3&&f.grid->selectionModel()->selection().front().right()==4,"other worksheet retains its saved active B2 and selected range");
}
void openingWorkbookClearsPreviousViewPositions(){
 Fixture f;QTemporaryDir files;const auto path=files.filePath("reopened.xlsx");
 require(f.window.saveWorkbook(path)&&f.window.openWorkbook(path),"open the saved workbook with its imported worksheet identities");
 f.clickTab(1);f.grid->setCurrentIndex(f.model->index(30,12));f.grid->horizontalScrollBar()->setValue(900);f.grid->verticalScrollBar()->setValue(500);f.clickTab(0);
 require(f.window.openWorkbook(path),"reopen a multi-sheet workbook with the same worksheet identities");
 f.clickTab(1);require(f.grid->currentIndex()==f.model->index(0,0)&&f.grid->horizontalScrollBar()->value()==0&&f.grid->verticalScrollBar()->value()==0,"opening a workbook clears view positions from the previous workbook context");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{if(argc>1&&std::string(argv[1])=="range_return"){returnTraversesSelectedRectangle();std::cout<<"rectangle Return tests passed\n";return 0;}if(argc>1&&std::string(argv[1])=="range_tab"){tabTraversesSelectedRectangle();std::cout<<"rectangle Tab tests passed\n";return 0;}returnTraversesSelectedRectangle();tabTraversesSelectedRectangle();sourceFarRightViewport();keyboardRangeKeepsActiveAnchor();nativeWorkbookViewSurvivesSave();nativeEntryContextIndicators();draggedSelectionKeepsActiveAnchor();keyboardSwitchesAdjacentSheets();sheetsRememberTheirSelectionAndScroll();addingSheetRetainsThePreviousView();openingWorkbookClearsPreviousViewPositions();std::cout<<"worksheet navigation UI tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
