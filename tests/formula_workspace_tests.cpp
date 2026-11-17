#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/formula_editor.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
WorksheetView* worksheet(MainWindow& window){
    auto* view=dynamic_cast<WorksheetView*>(window.findChild<QTableView*>("worksheet.grid"));
    require(view,"MainWindow exposes its native worksheet view");return view;
}
FormulaLineEdit* formulaBar(MainWindow& window){
    auto* editor=dynamic_cast<FormulaLineEdit*>(window.findChild<QLineEdit*>("formula.input"));
    require(editor,"MainWindow formula bar uses the native completion editor");return editor;
}
void prepareWindow(MainWindow& window){
    window.show();QApplication::processEvents();auto* grid=worksheet(window);
    grid->setCurrentIndex(grid->model()->index(0,0));
}
void bareEqualsShowsFunctionSelector() {
    for(const bool inCell:{false,true}) {
        MainWindow window;prepareWindow(window);auto* grid=worksheet(window);auto* bar=formulaBar(window);
        const auto origin=grid->model()->index(6,3);grid->setCurrentIndex(origin);grid->setFocus();
        auto* name=window.findChild<QLineEdit*>("formula.name");auto* state=window.findChild<QLabel*>("status.saveState");
        const auto before=window.currentSession().snapshot();const auto history=window.currentSession().historyBytes();
        if(!inCell)bar->setFocus();QTest::keyClicks(inCell?static_cast<QWidget*>(grid):static_cast<QWidget*>(bar),"=");
        auto* editor=inCell?grid->activeCellEditor():bar;require(editor&&editor->text()=="=","typing equals opens the actual uncommitted formula buffer");
        require(name->text()=="SUM"&&state->text()=="Enter"&&grid->property("worksheet.entryMode").toString()=="Enter","a fresh bare equals draft shows the SUM function selector in Enter mode");
        QTest::keyClicks(editor,"1");require(name->text()=="D7"&&editor->text()=="=1","ordinary formula entry retains its cell address after the bare-equals selector");
        QTest::keyClick(editor,Qt::Key_Backspace);require(name->text()=="SUM"&&editor->text()=="=","returning to a bare equals buffer restores its function selector");
        grid->cancelEntry();require(name->text()=="D7"&&state->text()=="Ready"&&!grid->activeCellEditor()&&!bar->isModified(),"cancelling restores the origin address and Ready state");
        require(grid->currentIndex()==origin&&window.currentSession().snapshot()==before&&window.currentSession().historyBytes()==history,"function-selector feedback and cancellation preserve the workbook, history and active cell");
    }
}
void cellCompletionTabDoesNotCommitOrMove() {
    MainWindow window;window.show();QApplication::processEvents();auto* grid=worksheet(window);
    const auto active=grid->model()->index(0,0);grid->setCurrentIndex(active);grid->setFocus();
    const auto before=window.currentSession().snapshot();
    QTest::keyClick(grid,Qt::Key_F2);QApplication::processEvents();
    QPointer<QLineEdit> editor=grid->activeCellEditor();require(editor,"F2 starts a real cell editor");
    QTest::keyClicks(editor,"=SUM");QTest::keyClick(editor,Qt::Key_Tab);QApplication::processEvents();
    require(window.currentSession().snapshot()==before,"autocomplete Tab must not commit incomplete =SUM to the canonical workbook");
    require(grid->currentIndex()==active&&editor&&grid->activeCellEditor()==editor,"autocomplete Tab must keep the current cell and its editor open");
    require(editor->text()=="=SUM(","MainWindow cell completion inserts the selected function call");
    require(dynamic_cast<FormulaLineEdit*>(editor.data()),"the real cell editor uses FormulaLineEdit");
}
void workbookMetadataSuppliesNamesAndEnabledFunctions() {
    MainWindow window;prepareWindow(window);auto* grid=worksheet(window);
    auto* model=dynamic_cast<WorksheetModel*>(grid->model());require(model,"worksheet exposes the canonical model");
    const Json names={{"DiscountRate","'Sheet1'!$B$2"},{"AddTax","=LAMBDA(x,x*1.2)"}};
    const Json vba={{"enabled",true},{"modules",Json::array({{{"name","Module1"},{"source","Public Function NetCash(ByVal amount As Double) As Double\nNetCash = amount + 1\nEnd Function"}}})}};
    require(model->apply(Json::array({{{"type","set_metadata"},{"key","names"},{"value",names}},{{"type","set_metadata"},{"key","vba"},{"value",vba}}})),"set_metadata establishes workbook completion context");
    window.recalculateNow();grid->setCurrentIndex(grid->model()->index(0,0));const auto before=window.currentSession().snapshot();
    auto* bar=formulaBar(window);bar->setFocus();QTest::keyClicks(bar,"=IF(1,disco");QTest::keyClick(bar,Qt::Key_Tab);
    require(bar->text()=="=IF(1,DiscountRate","set_metadata names must appear in nested formula-bar completion without a function parenthesis");
    require(window.currentSession().snapshot()==before,"defined-name completion cannot mutate canonical workbook metadata or cells");
    grid->cancelEntry();bar->setFocus();QTest::keyClicks(bar,"=AddT");QTest::keyClick(bar,Qt::Key_Tab);
    require(bar->text()=="=AddTax(","workbook LAMBDA names are classified as functions in the formula bar");
    grid->cancelEntry();bar->setFocus();QTest::keyClicks(bar,"=NetC");QTest::keyClick(bar,Qt::Key_Tab);
    require(bar->text()=="=NetCash(","enabled VBA Function definitions supply formula-bar completion");
    grid->cancelEntry();grid->setFocus();QTest::keyClick(grid,Qt::Key_F2);QApplication::processEvents();
    auto* cell=dynamic_cast<FormulaLineEdit*>(grid->activeCellEditor());require(cell,"cell metadata completion uses FormulaLineEdit");
    QTest::keyClicks(cell,"=NetC");QTest::keyClick(cell,Qt::Key_Tab);
    require(cell->text()=="=NetCash("&&window.currentSession().snapshot()==before,"new cell editors receive workbook functions without committing a partial entry");
    grid->cancelEntry();auto disabled=vba;disabled["enabled"]=false;
    require(model->apply(Json::array({{{"type","set_metadata"},{"key","vba"},{"value",disabled}}})),"VBA can be disabled through metadata");
    window.recalculateNow();bar->setFocus();QTest::keyClicks(bar,"=NetC");
    require(!bar->completionPopup()->isVisible(),"disabling VBA removes stale macro-function suggestions");
}
void formulaBarKeyboardSelectionPreservesTheEdit() {
    MainWindow window;prepareWindow(window);auto* grid=worksheet(window);auto* bar=formulaBar(window);
    const auto before=window.currentSession().snapshot();const auto active=grid->currentIndex();
    bar->setFocus();QTest::keyClicks(bar,"=sum");
    require(bar->completionPopup()->isVisible(),"the actual formula bar offers a native completion list");
    QTest::keyClick(bar,Qt::Key_Down);
    require(bar->completionPopup()->currentItem()->data(Qt::UserRole).toString()=="SUMIF","Down selects the next canonical function in the formula-bar popup");
    QTest::keyClick(bar,Qt::Key_Tab);
    require(bar->text()=="=SUMIF("&&bar->hasFocus(),"formula-bar Tab inserts the keyboard-selected function and retains focus");
    require(window.currentSession().snapshot()==before&&grid->currentIndex()==active,"formula-bar completion cannot commit a partial formula or move the cell");
}
void formulaBarMouseCompletionPreservesTheEdit() {
    MainWindow window;prepareWindow(window);auto* bar=formulaBar(window);const auto before=window.currentSession().snapshot();
    bar->setFocus();QTest::keyClicks(bar,"=aver");auto* popup=bar->completionPopup();
    require(popup->isVisible()&&popup->count(),"formula-bar mouse fixture shows native suggestions");
    const auto position=popup->visualItemRect(popup->item(0)).center();
    QTest::mouseClick(popup->viewport(),Qt::LeftButton,Qt::NoModifier,position);
    QTest::mouseDClick(popup->viewport(),Qt::LeftButton,Qt::NoModifier,position);QApplication::processEvents();
    require(bar->text()=="=AVERAGE("&&bar->hasFocus(),"double-click completion in MainWindow keeps formula-bar editing active");
    require(window.currentSession().snapshot()==before,"mouse completion must not commit the partial formula");
}
void nestedArgumentHintsWorkInBarAndCell() {
    for(const bool inCell:{false,true}) {
        MainWindow window;prepareWindow(window);auto* grid=worksheet(window);auto* bar=formulaBar(window);
        const auto before=window.currentSession().snapshot();FormulaLineEdit* editor=bar;
        if(inCell){grid->setFocus();QTest::keyClick(grid,Qt::Key_F2);QApplication::processEvents();editor=dynamic_cast<FormulaLineEdit*>(grid->activeCellEditor());}
        require(editor,"nested-argument fixture uses the native formula editor");editor->setFocus();
        QTest::keyClicks(editor,"=IF(1,\"a,b\",XNPV(.1,{1,2;3,4},A1:A4");
        require(editor->currentFunction()=="XNPV"&&editor->currentArgument()==2&&editor->argumentHelpText().contains("<b>dates</b>"),"MainWindow editor tracks nested financial arguments while ignoring quoted and array separators");
        auto* help=editor->findChild<QWidget*>("formula.argumentHelp");require(help&&help->isVisible(),"the active MainWindow editor displays its argument hint");
        if(inCell)require(bar->text()==editor->text(),"cell formula text stays synchronized with the formula bar");
        QTest::keyClicks(editor,")");
        require(editor->currentFunction()=="IF"&&editor->currentArgument()==2,"closing the nested function restores the outer argument hint");
        require(window.currentSession().snapshot()==before,"viewing argument hints never commits an incomplete expression");
    }
}
void firstEscapeDismissesThenSecondCancels() {
    MainWindow window;prepareWindow(window);auto* grid=worksheet(window);auto* bar=formulaBar(window);
    require(grid->model()->setData(grid->model()->index(0,0),"17",Qt::EditRole),"Escape fixture seeds the original cell value");
    window.recalculateNow();grid->setCurrentIndex(grid->model()->index(0,0));const auto before=window.currentSession().snapshot();
    grid->setFocus();QTest::keyClick(grid,Qt::Key_F2);QApplication::processEvents();
    QPointer<FormulaLineEdit> editor=dynamic_cast<FormulaLineEdit*>(grid->activeCellEditor());require(editor,"Escape fixture starts a completion-capable cell editor");
    editor->selectAll();QTest::keyClicks(editor,"=su");require(editor->completionPopup()->isVisible(),"Escape fixture opens completion suggestions");
    QTest::keyClick(editor,Qt::Key_Escape);QApplication::processEvents();
    require(editor&&grid->activeCellEditor()==editor&&!editor->completionPopup()->isVisible()&&editor->text()=="=su","first Escape dismisses suggestions and preserves the current cell edit");
    require(window.currentSession().snapshot()==before,"dismissing suggestions does not commit or discard canonical data");
    QTest::keyClick(editor,Qt::Key_Escape);QApplication::processEvents();
    require(!grid->activeCellEditor()&&bar->text()=="17"&&window.currentSession().snapshot()==before,"second Escape cancels the cell entry and restores the original value without a transaction");
}
void f4CyclesUnicodeQualifiedRangesInBarAndCell() {
    const auto original=QString::fromUtf8("=SUM('FY''26 虎'!a1 : b$3)+10");
    const auto absolute=QString::fromUtf8("=SUM('FY''26 虎'!$a$1 : $b$3)+10");
    const auto rowLocked=QString::fromUtf8("=SUM('FY''26 虎'!a$1 : b$3)+10");
    for(const bool inCell:{false,true}) {
        MainWindow window;prepareWindow(window);auto* grid=worksheet(window);auto* model=dynamic_cast<WorksheetModel*>(grid->model());
        require(model&&model->apply(Json::array({{{"type","add_sheet"},{"id","unicode"},{"name","FY'26 虎"}}})),"F4 fixture supplies an actual Unicode-named worksheet");
        grid->setCurrentIndex(grid->model()->index(0,0));const auto before=window.currentSession().snapshot();auto* bar=formulaBar(window);FormulaLineEdit* editor=bar;
        if(inCell){grid->setFocus();QTest::keyClick(grid,Qt::Key_F2);QApplication::processEvents();editor=dynamic_cast<FormulaLineEdit*>(grid->activeCellEditor());}
        require(editor,"F4 fixture uses a native formula editor");editor->setFocus();editor->setText(original);editor->setSelection(original.indexOf("a1"),original.indexOf(")+10")-original.indexOf("a1"));
        QTest::keyClick(editor,Qt::Key_F4);
        require(editor->text()==absolute&&editor->text().mid(editor->cursorPosition())==")+10","F4 on the selected whole range locks both endpoints while preserving Unicode qualifier, apostrophes, case, spaces and cursor suffix");
        QTest::keyClick(editor,Qt::Key_F4);
        require(editor->text()==rowLocked&&editor->text().mid(editor->cursorPosition())==")+10","a second F4 cycles the same Unicode-qualified range to row locks");
        if(inCell)require(bar->text()==editor->text(),"F4 changes in a cell editor synchronize to the formula bar");
        require(window.currentSession().snapshot()==before,"F4 edits only pending formula text until explicit commit");
    }
}
}
int main(int argc,char**argv) {
    QApplication app(argc,argv);QTemporaryDir state;
    qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());
    try{
        const QString selected=argc>1?QString::fromLocal8Bit(argv[1]):QString();
        bool ran=false;
        for(const auto& [name,test]:std::initializer_list<std::pair<const char*,void(*)()>>{
            {"bare_equals",bareEqualsShowsFunctionSelector},
            {"cell_completion",cellCompletionTabDoesNotCommitOrMove},
            {"workbook_context",workbookMetadataSuppliesNamesAndEnabledFunctions},
            {"bar_keyboard",formulaBarKeyboardSelectionPreservesTheEdit},
            {"bar_mouse",formulaBarMouseCompletionPreservesTheEdit},
            {"nested_hints",nestedArgumentHintsWorkInBarAndCell},
            {"escape",firstEscapeDismissesThenSecondCancels},
            {"unicode_f4",f4CyclesUnicodeQualifiedRangesInBarAndCell}})
            if(selected.isEmpty()||selected==name){test();ran=true;std::cout<<"PASS "<<name<<'\n';}
        require(ran,"unknown formula workspace test selection");
    }
    catch(const std::exception& error){std::cerr<<"formula workspace test failed: "<<error.what()<<'\n';return 1;}
    std::cout<<"formula workspace tests passed\n";
}
