#include "tigersheets/formula_editor.hpp"
#include "tigersheets/calculation.hpp"
#include <QApplication>
#include <QFile>
#include <QListWidget>
#include <QScreen>
#include <QSignalSpy>
#include <QStyledItemDelegate>
#include <QStandardItemModel>
#include <QTableView>
#include <QTest>
#include <QVBoxLayout>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }
Json frozenCatalog() {
    QFile file(QStringLiteral(TIGERSHEETS_SOURCE_DIR "/compatibility/functions.json"));
    require(file.open(QIODevice::ReadOnly),"frozen function catalog fixture opens");
    return Json::parse(file.readAll().toStdString());
}
void configure(FormulaLineEdit& editor) {
    CalculationEngine engine;
    editor.setFunctionCatalog(frozenCatalog(),engine.functions());
    editor.resize(620,30);editor.show();editor.setFocus();QApplication::processEvents();
}
QStringList suggestions(const FormulaLineEdit& editor) {
    QStringList result;
    auto* popup=editor.completionPopup();
    for(int i=0;i<popup->count();++i)result<<popup->item(i)->data(Qt::UserRole).toString();
    return result;
}
void typingShowsAvailableCanonicalFunctions() {
    FormulaLineEdit editor;configure(editor);
    QTest::keyClicks(&editor,"=su");
    QApplication::processEvents();
    require(editor.completionPopup()->isVisible(),"typing =su must show a native function popup");
    require(suggestions(editor).contains("SUM"),"case-insensitive completion includes SUM");
    require(suggestions(editor).contains("SUMIF"),"completion offers the matching function family");
    require(!suggestions(editor).contains("AVERAGE"),"completion narrows to the typed prefix");
    require(!editor.completionPopup()->accessibleName().isEmpty(),"completion popup has an accessible name");
}
void tabAcceptsCompletionWithoutCommitting() {
    FormulaLineEdit editor;configure(editor);
    QSignalSpy returned(&editor,&QLineEdit::returnPressed),finished(&editor,&QLineEdit::editingFinished),edited(&editor,&QLineEdit::textEdited);
    QTest::keyClicks(&editor,"=aver");
    const auto before=edited.count();
    QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=AVERAGE(","Tab must insert the selected canonical function and opening parenthesis");
    require(editor.cursorPosition()==9&&editor.isModified(),"completion retains the insertion point and modified edit flag");
    require(returned.isEmpty()&&finished.isEmpty(),"accepting a completion must not commit or finish editing");
    require(edited.count()>before,"accepted completion preserves the inherited textEdited signal for live synchronization");
    require(!editor.completionPopup()->isVisible(),"completion closes after acceptance");
}
void arrowsNavigateSuggestionsWithoutEditing() {
    FormulaLineEdit editor;configure(editor);QTest::keyClicks(&editor,"=sum");
    QTest::keyClick(&editor,Qt::Key_Down);
    require(editor.completionPopup()->currentRow()==1&&editor.text()=="=sum","Down must select a suggestion without moving or editing the formula");
    QTest::keyClick(&editor,Qt::Key_Up);
    require(editor.completionPopup()->currentRow()==0,"Up selects the preceding function");
    QTest::keyClick(&editor,Qt::Key_PageDown);
    require(editor.completionPopup()->currentRow()==qMin(8,editor.completionPopup()->count()-1),"PageDown advances through suggestions");
    QTest::keyClick(&editor,Qt::Key_PageUp);
    require(editor.completionPopup()->currentRow()==0,"PageUp returns through suggestions");
    QTest::keyClick(&editor,Qt::Key_End);
    require(editor.completionPopup()->currentRow()==editor.completionPopup()->count()-1,"End selects the last suggestion");
    QTest::keyClick(&editor,Qt::Key_Home);
    require(editor.completionPopup()->currentRow()==0,"Home selects the first suggestion");
    QTest::keyClick(&editor,Qt::Key_Down);QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=SUMIF(","Tab accepts the keyboard-selected function");
}
void nestedCompletionPreservesSuffixAndExistingParenthesis() {
    FormulaLineEdit editor;configure(editor);
    QTest::keyClicks(&editor,"=IF(A1,averAGEX(B1:B3),0)");
    editor.setCursorPosition(editor.text().indexOf("aver")+4);
    require(editor.completionPopup()->isVisible(),"function completion must work inside an existing nested formula");
    QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=IF(A1,AVERAGE(B1:B3),0)","completion replaces the function token and preserves the existing parenthesis and formula suffix");
    if(editor.cursorPosition()!=15)std::cerr<<"Nested completion cursor="<<editor.cursorPosition()<<" text="<<editor.text().toStdString()<<'\n';
    require(editor.cursorPosition()==15,"completion places the cursor inside the existing function call");
    editor.setText("=IF(A1,\"su\",0)");editor.setCursorPosition(10);
    require(!editor.completionPopup()->isVisible(),"completion stays hidden inside a quoted string");
    editor.setText("=SUM(A1)");editor.setCursorPosition(6);
    require(!editor.completionPopup()->isVisible(),"completion stays hidden inside an A1 cell reference");
}
void nestedArgumentsIgnoreStringsArraysAndCompletedCalls() {
    FormulaLineEdit editor;configure(editor);
    QTest::keyClicks(&editor,"=IF(A1,\"a,b\",XNPV(0.1,{1,2;3,4},A1:A4");
    require(editor.currentFunction()=="XNPV"&&editor.currentArgument()==2,"argument help must track the innermost function and ignore string and array separators");
    require(editor.argumentHelpText().contains("<b>dates</b>")&&editor.argumentHelpText().contains("rate"),"argument help highlights dates and retains the financial function signature");
    auto* help=editor.findChild<QWidget*>("formula.argumentHelp");
    require(help&&help->isVisible()&&!help->accessibleName().isEmpty(),"argument help is an accessible visible native tooltip");
    QTest::keyClicks(&editor,")");
    require(editor.currentFunction()=="IF"&&editor.currentArgument()==2,"closing a nested call restores the outer function argument");
    QTest::keyClicks(&editor,")");
    require(editor.currentFunction().isEmpty()&&editor.argumentHelpText().isEmpty(),"closing the outer call clears argument help");
}
void escapeDismissesWithoutReopeningOnCursorMoves() {
    FormulaLineEdit editor;configure(editor);QTest::keyClicks(&editor,"=su");
    QTest::keyClick(&editor,Qt::Key_Escape);
    require(!editor.completionPopup()->isVisible(),"Escape must dismiss formula suggestions");
    QTest::keyClick(&editor,Qt::Key_Left);QTest::keyClick(&editor,Qt::Key_Right);
    require(!editor.completionPopup()->isVisible()&&editor.argumentHelpText().isEmpty(),"cursor movement must not immediately reopen dismissed formula assistance");
    require(editor.text()=="=su","Escape only dismisses assistance without changing formula text");
    QTest::keyClicks(&editor,"m");
    require(editor.completionPopup()->isVisible(),"a subsequent text edit enables fresh suggestions");
    QTest::keyClick(&editor,Qt::Key_Down,Qt::AltModifier);
    require(!editor.completionPopup()->isVisible(),"Alt+Down toggles the autocomplete list off");
    QTest::keyClick(&editor,Qt::Key_Down,Qt::AltModifier);
    require(editor.completionPopup()->isVisible(),"Alt+Down explicitly restores a dismissed completion list");
}
void assistanceRequiresAnEditableFocusedEditor() {
    QWidget host;QVBoxLayout layout(&host);FormulaLineEdit editor;QLineEdit other;
    layout.addWidget(&editor);layout.addWidget(&other);host.show();configure(editor);
    QTest::keyClicks(&editor,"=sum");
    other.setFocus();QApplication::processEvents();
    require(!editor.completionPopup()->isVisible()&&editor.argumentHelpText().isEmpty(),"formula assistance must close when focus leaves the editor");
    editor.setFocus();QTest::keyClicks(&editor,"i");
    require(editor.completionPopup()->isVisible(),"editing after restoring focus can show suggestions");
    editor.setReadOnly(true);QApplication::processEvents();
    require(!editor.completionPopup()->isVisible()&&!editor.acceptCompletion(),"read-only editors cannot offer or accept formula completions");
    const auto before=editor.text();QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()==before,"read-only completion handling cannot alter text");
}
void coreFunctionHintsUseRealSignatures() {
    FormulaLineEdit editor;configure(editor);
    const QList<QPair<QString,QString>> cases={
        {"SUM","number1"},{"AVERAGE","number1"},{"IF","logical_test"},{"SUMIF","criteria"},
        {"SUMIFS","sum_range"},{"COUNTIF","criteria"},{"COUNTIFS","criteria_range1"},
        {"XLOOKUP","return_array"},{"VLOOKUP","col_index_num"},{"INDEX","row_num"},{"MATCH","match_type"},
        {"LET","name_value1"},{"LAMBDA","calculation"},{"FILTER","include"},{"SORT","sort_index"},
        {"SORTBY","by_array1"},{"UNIQUE","exactly_once"},{"SEQUENCE","rows"},{"CHOOSE","index_num"},
        {"ROWS","array"},{"COLUMNS","array"},{"TRANSPOSE","array"},{"AVERAGEIFS","average_range"},
        {"IFS","logical_test1"},{"IFNA","value_if_na"},{"NOT","logical"},{"SUBTOTAL","function_num"},
        {"SUMPRODUCT","array1"},{"PRODUCT","number1"},{"ROUNDUP","num_digits"},{"ROUNDDOWN","num_digits"},
        {"INT","number"},{"MOD","divisor"},{"POWER","power"},{"SQRT","number"},
        {"TODAY","current date"},{"NOW","current date and time"},{"TIME","hour"},{"WEEKDAY","return_type"},
        {"DATEVALUE","date_text"},{"WORKDAY","holidays"},{"NETWORKDAYS","holidays"},{"DAYS","start_date"},
        {"TEXT","format_text"},{"VALUE","text"},{"LEN","text"},{"LEFT","num_chars"},{"RIGHT","num_chars"},
        {"MID","start_num"},{"TRIM","text"},{"UPPER","text"},{"LOWER","text"},{"SUBSTITUTE","instance_num"},
        {"REPLACE","old_text"},{"FIND","find_text"},{"SEARCH","find_text"},{"CONCAT","text1"},
        {"CONCATENATE","text1"},{"EXACT","text1"},{"XIRR","dates"},{"XNPV","rate"},{"PMT","nper"},{"DATE","year"}
    };
    for(const auto& [name,parameter]:cases) {
        editor.setText("="+name+"(");editor.setCursorPosition(editor.text().size());
        if(editor.currentFunction()!=name||!editor.argumentHelpText().contains(parameter)||editor.argumentHelpText().contains("Argument details are not included"))
            throw std::runtime_error(("common function requires a concrete signature and description: "+name).toStdString());
    }
}
void mouseAcceptanceKeepsEditing() {
    FormulaLineEdit editor;configure(editor);QSignalSpy finished(&editor,&QLineEdit::editingFinished);
    QTest::keyClicks(&editor,"=sum");
    auto* popup=editor.completionPopup();const auto index=popup->model()->index(0,0);
    QTest::mouseClick(popup->viewport(),Qt::LeftButton,Qt::NoModifier,popup->visualRect(index).center());
    QTest::mouseDClick(popup->viewport(),Qt::LeftButton,Qt::NoModifier,popup->visualRect(index).center());
    require(editor.text()=="=SUM(","double-clicking a native suggestion accepts that function");
    require(editor.hasFocus()&&finished.isEmpty(),"mouse completion does not take focus or commit the edit");
}
void referencesAndEscapedTextSuppressCompletion() {
    FormulaLineEdit editor;configure(editor);
    const QList<QPair<QString,int>> cases={
        {"=SUM(A$1)",6},{"=SUM(A:A)",6},{"=SUM!A1",3},
        {"=SUM($SU)",8},{"=SUM(A1:SU)",10},{"=SUM(Table[SU])",13},
        {"=IF(1,\"a \"\"su\"\" b\",0)",14},{"='su, sheet'!A1",4}
    };
    for(const auto& [formula,cursor]:cases) {
        editor.setText(formula);editor.setCursorPosition(cursor);
        if(editor.completionPopup()->isVisible())throw std::runtime_error(("references and escaped text must suppress completion: "+formula).toStdString());
    }
    editor.setText("=CUBEVALUE");require(!editor.completionPopup()->isVisible(),"unavailable functions cannot be offered as executable completions");
    editor.setText("=ACOT(");
    require(editor.argumentHelpText().contains("Argument details are not included"),"generic available-function help honestly labels missing argument details");
}
void avgOffersAnExplicitCanonicalSuggestion() {
    FormulaLineEdit editor;configure(editor);QTest::keyClicks(&editor,"=AVG");
    require(editor.completionPopup()->isVisible(),"AVG must offer a did-you-mean canonical AVERAGE suggestion");
    require(editor.text()=="=AVG"&&suggestions(editor).contains("AVERAGE")&&!suggestions(editor).contains("AVG"),"the AVG convenience never rewrites text or invents a function before acceptance");
    require(editor.completionPopup()->currentItem()->text().contains("canonical")&&editor.argumentHelpText().contains("not an Excel function"),"the fallback visibly explains the canonical function name");
    QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=AVERAGE(","accepting the AVG suggestion inserts canonical AVERAGE");
    editor.setFunctionCatalog(frozenCatalog(),{{"functions",Json::array({"AVERAGEIF"})}});
    editor.setText("=avg");
    require(suggestions(editor)==QStringList{"AVERAGEIF"},"AVG fallback only offers available canonical functions");
    editor.setText("=AVER");
    require(!editor.completionPopup()->currentItem()->text().contains("canonical")&&!editor.argumentHelpText().contains("not an Excel function"),"real prefix matches use ordinary completion rather than the AVG convenience fallback");
}
void workbookNamesInsertWithoutParentheses() {
    FormulaLineEdit editor;configure(editor);
    editor.setWorkbookCompletions({"DiscountRate","discountrate","Bad Name","A1","R1C1","C","1Invalid","Bad()"});
    QSignalSpy finished(&editor,&QLineEdit::editingFinished);
    QTest::keyClicks(&editor,"=IF(A1,discoZZ+1,0)");editor.setCursorPosition(editor.text().indexOf("disco")+5);
    require(suggestions(editor)==QStringList{"DiscountRate"},"workbook defined names are matched and deduplicated without losing their spelling");
    require(editor.completionPopup()->currentItem()->text().contains("Defined name")&&editor.argumentHelpText().contains("without parentheses"),"defined-name suggestions visibly explain their insertion kind");
    QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=IF(A1,DiscountRate+1,0)","Tab inserts a workbook name in a nested formula without a function parenthesis");
    require(editor.text().mid(editor.cursorPosition())=="+1,0)"&&finished.isEmpty(),"defined-name completion preserves suffix and continues editing");
    editor.setText("=Bad");require(!editor.completionPopup()->isVisible(),"invalid identifiers are not offered as workbook names");
    editor.setWorkbookCompletions({"_Tax","\\Assumption","Sales.Q1"});
    editor.setText("=_Ta");QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=_Tax","valid underscore-prefixed names can be completed");
    editor.setText("=\\Ass");QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=\\Assumption","valid leading-backslash names can be completed");
}
void workbookFunctionsInsertCallsAndKeepTheirKind() {
    FormulaLineEdit editor;configure(editor);
    editor.setWorkbookCompletions({"NetReturn"},{"AdjustedValue","adjustedvalue","NetReturn","Bad Function"});
    QTest::keyClicks(&editor,"=IF(A1,adjuZZ(B1,B2),0)");editor.setCursorPosition(editor.text().indexOf("adju")+4);
    require(suggestions(editor)==QStringList{"AdjustedValue"},"registered workbook functions are offered and deduplicated case-insensitively");
    require(editor.completionPopup()->currentItem()->text().contains("Workbook function")&&editor.argumentHelpText().contains("Parameter details"),"workbook functions show their actual kind and honestly unavailable signature details");
    QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=IF(A1,AdjustedValue(B1,B2),0)","UDF completion preserves a nested formula suffix and existing opening parenthesis");
    require(editor.currentFunction()=="AdjustedValue"&&editor.currentArgument()==0,"argument context recognizes an explicitly supplied workbook function");
    QTest::keyClicks(&editor,"C1,");
    require(editor.currentFunction()=="AdjustedValue"&&editor.currentArgument()==1,"workbook function argument context tracks separators without inventing parameter names");
    editor.setText("=netre");QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=NetReturn(","a workbook function takes precedence over its ordinary defined-name entry");
    editor.setWorkbookCompletions({});editor.setText("=Adju");
    require(!editor.completionPopup()->isVisible(),"replacing workbook completion context removes stale function names");
}
void workbookEntriesCannotImpersonateBuiltins() {
    FormulaLineEdit editor;configure(editor);
    editor.setFunctionCatalog(frozenCatalog(),{{"functions",Json::array({"SUM"})}});
    editor.setWorkbookCompletions({"SUM","GETPIVOTDATA","Revenue"},{"sum","getpivotdata","AdjustedValue"});
    editor.setText("=GETPIV");
    require(!editor.completionPopup()->isVisible(),"an unavailable builtin cannot be introduced as a workbook function");
    editor.setText("=sum");
    require(suggestions(editor)==QStringList{"SUM"},"builtins and workbook identifiers cannot produce duplicate completions");
    require(!editor.completionPopup()->currentItem()->text().contains("Workbook function")&&editor.argumentHelpText().contains("number1"),"a builtin keeps its canonical kind and accurate signature");
    QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()=="=SUM(","builtin acceptance preserves normal function insertion");
    FormulaLineEdit beforeCatalog;beforeCatalog.setWorkbookCompletions({}, {"CUBEVALUE","GETPIVOTDATA","AdjustedValue"});configure(beforeCatalog);
    beforeCatalog.setText("=CUBEVALUE");require(!beforeCatalog.completionPopup()->isVisible(),"unavailable builtin reservation also applies before the engine catalog arrives");
    beforeCatalog.setText("=GETPIV");require(suggestions(beforeCatalog)==QStringList{"GETPIVOTDATA"}&&!beforeCatalog.completionPopup()->currentItem()->text().contains("Workbook function"),"available GETPIVOTDATA keeps its builtin identity when workbook context arrives first");
}
void workbookIdentifierBoundariesStayDistinctFromCells() {
    FormulaLineEdit editor;configure(editor);
    const auto longest=QString(255,'N');
    editor.setWorkbookCompletions({"Sales.Q1","XFE1","A1048577",longest,QString(256,'M'),"XFD1048576","R1C1","R","C","Bad Name","Bad\\Middle"},{"_AfterTax"});
    editor.setText("=Sales.Q1");
    require(suggestions(editor).contains("Sales.Q1"),"a complete dotted workbook identifier remains distinct from a cell-reference suffix");
    editor.setText("=Sales.Q");
    require(suggestions(editor)==QStringList{"Sales.Q1"},"a dotted workbook name must not be mistaken for its cell-looking suffix");
    QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()=="=Sales.Q1","dotted defined-name completion inserts the original identifier");
    for(const auto& name:{QString("XFE1"),QString("A1048577"),longest}) {
        editor.setText("="+name);
        require(suggestions(editor).contains(name),"valid non-cell identifiers and the 255-character boundary can be completed");
    }
    for(const auto& prefix:{QString("XFD"),QString("R1C"),QString("Bad"),QString("MMMM")}) {
        editor.setText("="+prefix);
        require(!editor.completionPopup()->isVisible(),"actual cell addresses, malformed names, and overlength identifiers remain excluded");
    }
    editor.setText("=_Aft");QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=_AfterTax("&&editor.currentFunction()=="_AfterTax","underscore-prefixed workbook functions retain function context");
}
class FormulaDelegate final:public QStyledItemDelegate {
public:
    FormulaDelegate():catalog_(frozenCatalog()) {CalculationEngine engine;availability_=engine.functions();}
    mutable FormulaLineEdit* editor{};
    QStringList names,functions;
    QWidget* createEditor(QWidget* parent,const QStyleOptionViewItem&,const QModelIndex&)const override {
        editor=new FormulaLineEdit(parent);editor->setFunctionCatalog(catalog_,availability_);editor->setWorkbookCompletions(names,functions);return editor;
    }
    bool eventFilter(QObject* object,QEvent* event)override {
        if(auto* line=dynamic_cast<FormulaLineEdit*>(object);line&&event->type()==QEvent::KeyPress&&line->handleFormulaKey(static_cast<QKeyEvent*>(event)))return true;
        return QStyledItemDelegate::eventFilter(object,event);
    }
private:Json catalog_,availability_;
};
void delegateTabDoesNotCommitAndF4PassesThrough() {
    QTableView view;QStandardItemModel model(1,1);FormulaDelegate delegate;
    view.setModel(&model);view.setItemDelegate(&delegate);view.resize(640,100);view.show();
    view.edit(model.index(0,0));QApplication::processEvents();
    require(delegate.editor,"real cell delegate creates the formula editor");
    QSignalSpy committed(&delegate,&QAbstractItemDelegate::commitData),closed(&delegate,&QAbstractItemDelegate::closeEditor);
    QTest::keyClicks(delegate.editor,"=sum");QTest::keyClick(delegate.editor,Qt::Key_Tab);
    require(delegate.editor->text()=="=SUM("&&committed.isEmpty()&&closed.isEmpty(),"delegate routes completion Tab before Qt can commit or close the cell editor");
    QKeyEvent f4(QEvent::KeyPress,Qt::Key_F4,Qt::NoModifier);
    require(!delegate.editor->handleFormulaKey(&f4),"formula completion must leave F4 available to reference cycling");
    QTest::keyClick(delegate.editor,Qt::Key_F4);
    require(delegate.editor->text()=="=SUM(","unhandled F4 does not corrupt formula text");
    QTest::keyClicks(delegate.editor,"A1)");
    QTest::keyClick(delegate.editor,Qt::Key_Tab);QApplication::processEvents();
    require(committed.count()==1&&model.data(model.index(0,0)).toString()=="=SUM(A1)","Tab without a completion retains normal cell commit behavior");
}
void workbookCompletionsDoNotWriteTheCellModel() {
    QTableView view;QStandardItemModel model(1,1);FormulaDelegate delegate;
    model.setData(model.index(0,0),"Original value");delegate.names={"DiscountRate"};delegate.functions={"NetCash"};
    view.setModel(&model);view.setItemDelegate(&delegate);view.resize(640,100);view.show();view.edit(model.index(0,0));QApplication::processEvents();
    require(delegate.editor,"workbook completion model fixture creates an editor");
    QSignalSpy committed(&delegate,&QAbstractItemDelegate::commitData);
    delegate.editor->selectAll();QTest::keyClicks(delegate.editor,"=IF(1,disco");QTest::keyClick(delegate.editor,Qt::Key_Tab);
    require(delegate.editor->text()=="=IF(1,DiscountRate"&&committed.isEmpty()&&model.data(model.index(0,0)).toString()=="Original value","named-range completion changes only editor text until explicit commit");
    delegate.editor->selectAll();QTest::keyClicks(delegate.editor,"=NetC");QTest::keyClick(delegate.editor,Qt::Key_Tab);
    require(delegate.editor->text()=="=NetCash("&&committed.isEmpty()&&model.data(model.index(0,0)).toString()=="Original value","workbook-function completion cannot mutate the cell model or commit it");
}
void wideEditorsKeepCompletionCompactAndElideLongNames() {
    FormulaLineEdit editor;configure(editor);editor.resize(1230,30);
    QTest::keyClicks(&editor,"=SUM");auto* popup=editor.completionPopup();
    require(popup->isVisible(),"wide-editor fixture displays native completion suggestions");
    const auto available=editor.screen()->availableGeometry();
    require(popup->width()>=qMin(280,available.width())&&popup->width()<=qMin(520,available.width()),"a wide formula bar must use a compact adaptive completion width rather than its full width");
    const auto longName=QString("Long_")+QString(240,'n');editor.setWorkbookCompletions({longName});editor.setText("=Long_");
    require(popup->isVisible()&&popup->width()<=qMin(520,available.width()),"long workbook identifiers cannot make the native completion list excessively wide");
    require(popup->textElideMode()==Qt::ElideRight&&popup->horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff,"long suggestion captions elide instead of adding a wide horizontal scrolling surface");
    require(popup->currentItem()->data(Qt::UserRole).toString()==longName,"visual elision retains the complete identifier for acceptance and accessibility");
    QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()=="="+longName,"elided defined-name completion inserts the complete original identifier");
}
void popupAndHelpStayInsideTheAvailableScreen() {
    FormulaLineEdit editor;configure(editor);const auto available=editor.screen()->availableGeometry();
    editor.resize(180,30);editor.move(available.right()-editor.width()-10,available.bottom()-editor.height()-10);QApplication::processEvents();
    QTest::keyClicks(&editor,"=AVER");auto* popup=editor.completionPopup();auto* help=editor.findChild<QWidget*>("formula.argumentHelp");
    require(popup->isVisible()&&help&&help->isVisible(),"screen-edge fixture displays both completion list and help");
    require(available.contains(popup->frameGeometry()),"completion popup must remain inside the editor screen near its bottom-right corner");
    const auto helpRect=help->frameGeometry();
    require(available.contains(helpRect),("argument help must remain inside the editor screen when completion is visible: "+QString("available=%1,%2 %3x%4 help=%5,%6 %7x%8").arg(available.x()).arg(available.y()).arg(available.width()).arg(available.height()).arg(helpRect.x()).arg(helpRect.y()).arg(helpRect.width()).arg(helpRect.height()).toStdString()).c_str());
    require(!popup->frameGeometry().intersects(help->frameGeometry()),"screen-bound completion and help must not cover one another");
    require(editor.hasFocus(),"positioning native formula assistance cannot take editor focus");
    QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()=="=AVERAGE(","repositioned completion retains Tab acceptance");
    editor.setText("=XLOOKUP(");
    require(!popup->isVisible()&&help->isVisible()&&available.contains(help->frameGeometry()),"standalone function argument help also fits the available screen near an edge");
    const auto text=editor.text();QTest::keyClick(&editor,Qt::Key_Escape);
    require(!help->isVisible()&&editor.text()==text&&editor.hasFocus(),"repositioned help retains Escape dismissal without altering the edit");
}
}
int main(int argc,char**argv) {
    QApplication app(argc,argv);
    try { typingShowsAvailableCanonicalFunctions();tabAcceptsCompletionWithoutCommitting();arrowsNavigateSuggestionsWithoutEditing();nestedCompletionPreservesSuffixAndExistingParenthesis();nestedArgumentsIgnoreStringsArraysAndCompletedCalls();escapeDismissesWithoutReopeningOnCursorMoves();assistanceRequiresAnEditableFocusedEditor();coreFunctionHintsUseRealSignatures();mouseAcceptanceKeepsEditing();referencesAndEscapedTextSuppressCompletion();delegateTabDoesNotCommitAndF4PassesThrough();avgOffersAnExplicitCanonicalSuggestion();workbookNamesInsertWithoutParentheses();workbookFunctionsInsertCallsAndKeepTheirKind();workbookEntriesCannotImpersonateBuiltins();workbookIdentifierBoundariesStayDistinctFromCells();workbookCompletionsDoNotWriteTheCellModel();wideEditorsKeepCompletionCompactAndElideLongNames();popupAndHelpStayInsideTheAvailableScreen(); }
    catch(const std::exception& e) { std::cerr<<"formula editor test failed: "<<e.what()<<'\n';return 1; }
    std::cout<<"formula editor tests passed\n";
}
