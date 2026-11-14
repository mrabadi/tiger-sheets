#include "tigersheets/calculation.hpp"
#include "tigersheets/formula_editor.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QAccessible>
#include <QApplication>
#include <QFile>
#include <QListWidget>
#include <QTest>
#include <QVBoxLayout>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void nativeEditorSmoke(){
 WorkbookSession session;WorksheetModel model(session);QWidget window;QVBoxLayout layout(&window);FormulaLineEdit bar;WorksheetView view;
 layout.addWidget(&bar);layout.addWidget(&view);view.setModel(&model);view.setFormulaBar(&bar);view.setCurrentIndex(model.index(0,0));view.beforeEditorKey=[&](QLineEdit* editor,QKeyEvent* event){return editor==&bar&&bar.handleFormulaKey(event);};
 QFile catalog(QStringLiteral(TIGERSHEETS_SOURCE_DIR "/compatibility/functions.json"));require(catalog.open(QIODevice::ReadOnly),"function catalog opens");CalculationEngine engine;bar.setFunctionCatalog(Json::parse(catalog.readAll().toStdString()),engine.functions());
 window.setWindowTitle("TigerSheets native keyboard validation");window.resize(820,560);window.show();require(QTest::qWaitForWindowExposed(&window,5000),"native test window is exposed");window.activateWindow();require(QTest::qWaitForWindowActive(&window,5000),"native test window is active");
 bar.setFocus();require(QTest::qWaitFor([&]{return bar.hasFocus();},1000),"formula bar receives native keyboard focus");QTest::keyClicks(&bar,"=sum");require(bar.completionPopup()->isVisible(),"native formula completion opens");require(!bar.completionPopup()->accessibleName().isEmpty(),"completion popup has an accessible name");QTest::keyClick(&bar,Qt::Key_Tab);require(bar.text()=="=SUM(","native Tab accepts completion without committing");
 QTest::keyClick(&bar,Qt::Key_Right);require(bar.text()=="=SUM(B1","native keyboard reference pointing retains formula draft");require(session.snapshotView()["sheets"][0]["cells"].empty(),"formula draft has not changed committed cells");view.cancelEntry();
 view.setCurrentIndex(model.index(0,0));view.setFocus();view.edit(model.index(0,0));QApplication::processEvents();auto* editor=view.activeCellEditor();require(editor!=nullptr,"native cell editor opens");editor->setFocus();require(QTest::qWaitFor([&]{return editor->hasFocus();},1000),"native cell editor receives focus");
 auto* accessible=QAccessible::queryAccessibleInterface(editor);require(accessible&&accessible->role()==QAccessible::EditableText&&accessible->text(QAccessible::Name)=="Cell A1","cell editor exposes its editable role and address");QTest::keyClicks(editor,"00123");require(accessible->text(QAccessible::Value)=="00123"&&accessible->state().focused,"accessible value and focus follow keyboard editing");QTest::keyClick(editor,Qt::Key_Escape);require(model.raw(0,0).isEmpty(),"native Escape cancels without writing the cell");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);std::cout<<"Qt platform: "<<app.platformName().toStdString()<<'\n';try{if(argc>1)require(app.platformName()==QString::fromUtf8(argv[1]),"requested native backend matches the actual Qt platform");nativeEditorSmoke();std::cout<<"native keyboard/accessibility interface smoke passed\n";return 0;}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
