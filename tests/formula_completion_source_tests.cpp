#include "tigersheets/formula_editor.hpp"
#include "tigersheets/calculation.hpp"
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QScreen>
#include <QSignalSpy>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTemporaryFile>
#include <QTest>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void configure(FormulaLineEdit& editor){
    QFile file(QStringLiteral(TIGERSHEETS_SOURCE_DIR "/compatibility/functions.json"));
    require(file.open(QIODevice::ReadOnly),"source completion fixture opens the frozen supported-function catalog");
    CalculationEngine engine;editor.setFunctionCatalog(Json::parse(file.readAll().toStdString()),engine.functions());
    editor.resize(620,24);editor.move(100,100);editor.show();editor.setFocus();QApplication::processEvents();
}
QStringList names(const FormulaLineEdit& editor){QStringList result;for(int i=0;i<editor.completionPopup()->count();++i)result<<editor.completionPopup()->item(i)->data(Qt::UserRole).toString();return result;}
void retain(FormulaLineEdit& editor,const QString& label){const auto directory=qEnvironmentVariable("TIGERSHEETS_COMPLETION_SOURCE_CAPTURE");if(directory.isEmpty())return;require(QDir().mkpath(directory),"native completion capture directory opens");require(editor.grab().save(directory+"/"+label+"-editor.png"),"retain the actual native formula editor");require(editor.completionPopup()->grab().save(directory+"/"+label+"-list.png"),"retain the actual native completion list");if(auto* help=editor.findChild<QLabel*>("formula.argumentHelp");help&&help->isVisible())require(help->grab().save(directory+"/"+label+"-help.png"),"retain the actual native completion help");}
void sourceMatching(){
    FormulaLineEdit editor;configure(editor);QSignalSpy finished(&editor,&QLineEdit::editingFinished);
    QTest::keyClicks(&editor,"=-D69/SUM");
    const QStringList expected={"SUM","SUMIF","SUMIFS","SUMPRODUCT","SUMSQ","SUMX2MY2","SUMX2PY2","SUMXMY2","DSUM","IMSUM","SERIESSUM"};
    require(names(editor)==expected,"source SUM completion includes the three real contains matches after its alphabetic prefix family");
    editor.setWorkbookCompletions({"MyTotal","TotalRevenue"},{"ComputeTotal","TotalFunction"});editor.setText("=tOtAl");
    require(names(editor)==QStringList{"TotalFunction","TotalRevenue","ComputeTotal","MyTotal","SUBTOTAL"},"case-insensitive matching ranks all prefixes before all contains matches for ordinary workbook identifiers too");
    QTest::keyClick(&editor,Qt::Key_Down);QTest::keyClick(&editor,Qt::Key_Down);QTest::keyClick(&editor,Qt::Key_Tab);
    require(editor.text()=="=ComputeTotal("&&finished.isEmpty(),"a contains-matched workbook function uses genuine native selection and Tab insertion without committing");
}
void sourceListGeometry(){
    FormulaLineEdit editor;configure(editor);QTest::keyClicks(&editor,"=-D69/SUM");auto* popup=editor.completionPopup();
    std::cout<<"SUM list "<<popup->width()<<"x"<<popup->height()<<" count="<<popup->count()<<"\n";
    require(popup->width()>=104&&popup->width()<=128,"source SUM list is content-sized around 116 pixels rather than a 280-pixel panel");
    require(popup->count()==11&&popup->visualItemRect(popup->item(10)).bottom()<popup->viewport()->height(),"all eleven source SUM rows are actually visible, including SERIESSUM");
    require(popup->height()>=184&&popup->height()<=196,"the eleven SUM suggestions retain the source compact approximately 17-pixel row pitch");
    retain(editor,"sum");
    editor.setText("=A");QApplication::processEvents();
    const QStringList first={"ABS","ACCRINT","ACCRINTM","ACOS","ACOSH","ACOT","ACOTH","ADDRESS","AGGREGATE","AMORDEGRC","AMORLINC","AND"};
    require(names(editor).mid(0,12)==first,"source A suggestions preserve the actual twelve visible canonical functions");
    std::cout<<"A list "<<popup->width()<<"x"<<popup->height()<<" count="<<popup->count()<<"\n";
    require(popup->width()>=116&&popup->width()<=144,"source A list remains content-sized around 128 pixels");
    require(popup->height()>=200&&popup->height()<=212&&popup->visualItemRect(popup->item(11)).bottom()<popup->viewport()->height(),"source A list shows twelve complete compact rows before scrolling");
    retain(editor,"abs");
    editor.setText("=-SUM(F$40:F$42)*F");
    std::cout<<"F list "<<popup->width()<<"x"<<popup->height()<<" count="<<popup->count()<<"\n";
    require(popup->width()>=180&&popup->width()<=232,"the source F family uses a content-sized roughly 200-pixel menu with bounded missing-font metric adaptation");
    std::cout<<"F first rows "<<names(editor).mid(0,12).join(",").toStdString()<<"\n";retain(editor,"fdist");
    const auto longName=QString("Long_")+QString(240,'n');editor.setWorkbookCompletions({longName});editor.setText("=Long_");
    require(popup->width()<=qMin(520,editor.screen()->availableGeometry().width())&&popup->textElideMode()==Qt::ElideRight,"long identifiers remain bounded and elided while ordinary function menus are narrow");
    QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()=="="+longName,"content sizing preserves the complete underlying accepted identifier");
}
void sourceCompletionHelp(){
    FormulaLineEdit editor;configure(editor);QSignalSpy finished(&editor,&QLineEdit::editingFinished);QTest::keyClicks(&editor,"=-D69/SUM");
    auto* popup=editor.completionPopup();auto* help=editor.findChild<QLabel*>("formula.argumentHelp");
    require(help&&help->isVisible()&&help->text()=="Adds all the numbers in a range of cells","source completion help displays only the selected function description");
    require(help->frameGeometry().left()>=popup->frameGeometry().right()+1&&std::abs(help->frameGeometry().top()-popup->frameGeometry().top())<=1,"source completion description is beside the first row to the right of the list");
    require(help->height()<=22&&!help->wordWrap(),"the source autocomplete description is a compact one-line native tooltip");
    require(editor.argumentHelpText().contains("SUM(number1")&&editor.argumentHelpText().contains("Adds all the numbers"),"the full assistance API retains the real signature and description independently of the compact visible text");
    editor.setText("=A");require(help->text()=="Returns the absolute value of a number, a number without its sign","source ABS description is complete and excludes its signature while the list is visible");
    std::cout<<"ABS description "<<help->width()<<"x"<<help->height()<<" font="<<help->font().family().toStdString()<<"\n";
    require(help->height()<=22&&help->width()<=380,"source ABS description remains a roughly 360-pixel one-line tooltip");
    QTest::keyClick(&editor,Qt::Key_Tab);require(editor.text()=="=ABS("&&!popup->isVisible()&&help->text().startsWith("ABS(")&&!help->text().contains("Returns the absolute"),"accepting the function changes to the separate compact signature calltip");
    require(editor.argumentHelpText().contains("Returns the absolute")&&finished.isEmpty(),"the calltip retains full accessible help and stays inside the uncommitted native edit");
    editor.setText("=-SUM(F$40:F$42)*F");require(popup->currentItem()->data(Qt::UserRole).toString()=="F.DIST","source partial-column expression selects the first F function");
    require(help->text()=="Returns the (left-tailed) F probability distribution (degree of diversity) for two data sets."&&!help->text().contains("deg_freedom"),"source F.DIST autocomplete shows its real description without an invented visible parameter line");
}
void sourceAnchoredHelp(){
    FormulaLineEdit editor;configure(editor);editor.move(684,530);editor.setText("=-SUM(F$40:F$42)*F");QApplication::processEvents();
    auto* popup=editor.completionPopup();auto* help=editor.findChild<QLabel*>("formula.argumentHelp");
    const int tokenX=editor.mapToGlobal(QPoint()).x()+editor.fontMetrics().horizontalAdvance("=-SUM(F$40:F$42)*");
    require(std::abs(popup->frameGeometry().left()-tokenX)<=6,"right-edge source descriptions do not displace the F list from its actual completion-token anchor");
    require(help->frameGeometry().left()>=popup->frameGeometry().right()+1&&editor.screen()->availableGeometry().contains(help->frameGeometry()),"long source descriptions fit the available space beside their anchored list without covering it");
    require(help->text().endsWith("for two data sets.")&&editor.argumentHelpText().contains("deg_freedom1"),"visible tail clipping retains the full description and parameter metadata for assistive access");
}
void completionTokenAnchor(){
    FormulaLineEdit editor;configure(editor);QTest::keyClicks(&editor,"=-D69/SUM");
    const int tokenX=editor.mapToGlobal(QPoint()).x()+editor.fontMetrics().horizontalAdvance("=-D69/");
    require(std::abs(editor.completionPopup()->frameGeometry().left()-tokenX)<=6,"source SUM popup starts below the SUM token, after the actual preceding reference expression");
}
class CompletionDelegate final:public QStyledItemDelegate {
public:
    mutable FormulaLineEdit* editor{};
    QWidget* createEditor(QWidget* parent,const QStyleOptionViewItem&,const QModelIndex&)const override {
        editor=new FormulaLineEdit(parent);QFile file(QStringLiteral(TIGERSHEETS_SOURCE_DIR "/compatibility/functions.json"));require(file.open(QIODevice::ReadOnly),"real table editor reads its catalog");CalculationEngine engine;editor->setFunctionCatalog(Json::parse(file.readAll().toStdString()),engine.functions());return editor;
    }
};
void offscreenCompletionRetainsVisibleAnchor(){
    QStandardItemModel model(100,1);const auto index=model.index(18,0);model.setData(index,"original");
    QTableView view;CompletionDelegate delegate;view.setModel(&model);view.setItemDelegate(&delegate);view.setColumnWidth(0,610);view.resize(660,300);view.move(100,100);view.show();view.scrollTo(index,QAbstractItemView::PositionAtCenter);view.edit(index);QApplication::processEvents();
    auto* editor=delegate.editor;require(editor&&editor->hasFocus(),"source offscreen fixture starts with a genuine focused table editor");QSignalSpy finished(editor,&QLineEdit::editingFinished);editor->selectAll();QTest::keyClicks(editor,"=-D69/SU");
    auto* popup=editor->completionPopup();const auto visibleAnchor=popup->frameGeometry().topLeft();require(!editor->visibleRegion().isEmpty()&&popup->isVisible(),"the completion fixture records a genuinely visible editor and native suggestion anchor");
    view.verticalScrollBar()->setValue(view.verticalScrollBar()->maximum());QApplication::processEvents();require(editor->geometry().bottom()<0&&editor->visibleRegion().isEmpty(),"native table scrolling really moves the editor outside its viewport");
    QTest::keyClicks(editor,"M");QApplication::processEvents();
    require(popup->isVisible()&&popup->frameGeometry().topLeft()==visibleAnchor,"typing SUM after its editor scrolls offscreen retains the last visible native assistance anchor");
    require(editor->text()=="=-D69/SUM"&&editor->hasFocus()&&finished.isEmpty()&&model.data(index).toString()=="original","offscreen assistance does not move the editor, commit the draft, or change the cell model");
}
}
int main(int argc,char** argv){QTemporaryFile screen;require(screen.open(),"source-size offscreen screen configuration opens");screen.write("{\"screens\":[{\"width\":1280,\"height\":800}]}");screen.flush();if(qgetenv("QT_QPA_PLATFORM").startsWith("offscreen"))qputenv("QT_QPA_PLATFORM",QByteArray("offscreen:configfile=")+screen.fileName().toUtf8());QApplication app(argc,argv);try{const auto mode=argc>1?QString::fromLocal8Bit(argv[1]):QString();if(mode.isEmpty()||mode=="matching")sourceMatching();if(mode.isEmpty()||mode=="geometry")sourceListGeometry();if(mode.isEmpty()||mode=="help")sourceCompletionHelp();if(mode.isEmpty()||mode=="anchor")sourceAnchoredHelp();if(mode.isEmpty()||mode=="token")completionTokenAnchor();if(mode.isEmpty()||mode=="offscreen")offscreenCompletionRetainsVisibleAnchor();std::cout<<"Source completion matching and geometry passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
