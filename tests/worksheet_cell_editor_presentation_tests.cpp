#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QImage>
#include <QFontMetricsF>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
int textLeft(const QImage& image,const QRect& area){for(int x=area.left();x<=area.right();++x)for(int y=area.top();y<=area.bottom();++y){const auto c=image.pixelColor(x,y);if(c.red()<100&&c.green()<100&&c.blue()<100)return x;}throw std::runtime_error("the native cell image contains visible text");}
struct Fixture {
 MainWindow window;WorksheetView* view;WorksheetModel* model;QModelIndex cell;
 Fixture(int indent,const std::string& alignment="left"){window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();view=window.findChild<WorksheetView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(view->model());cell=model->index(2,2);const auto sheet=model->sheetId().toStdString();require(model->apply(Json::array({{{"type","set_cell"},{"sheet",sheet},{"row",3},{"col",3},{"input","MMMM"},{"style",{{"fontFamily","DejaVu Sans"},{"fontSize",12},{"indent",indent},{"alignment",alignment}}}}})),"prepare a styled synthetic cell");view->setColumnWidth(2,300);view->setCurrentIndex(cell);view->scrollTo(cell);view->setFocus();QApplication::processEvents();}
 QLineEdit* edit(const QString& expected="MMMM"){QTest::keyClick(view,Qt::Key_F2);QApplication::processEvents();auto* editor=view->activeCellEditor();require(editor&&editor->text()==expected,"F2 opens the actual native in-cell editor");editor->deselect();editor->setCursorPosition(editor->text().size());QApplication::processEvents();return editor;}
 ~Fixture(){view->cancelEntry();}
};
void indentation(){Fixture f(2);const auto before=f.window.currentSession().snapshot();const auto rect=f.view->visualRect(f.cell),area=rect.adjusted(2,2,-2,-2);const int resting=textLeft(f.view->viewport()->grab().toImage(),area);auto* editor=f.edit();const int editing=textLeft(f.view->viewport()->grab().toImage(),area);std::cout<<"indented resting text x="<<resting<<" editing x="<<editing<<" editor x="<<editor->x()<<" cell x="<<rect.x()<<'\n';require(std::abs(resting-editing)<=1,"editing an indented cell keeps its text at the same visible indentation");require(f.window.currentSession().snapshot()==before,"presenting indentation never changes cell input, style or Undo history");}
void alignment(const std::string& horizontal="left",int indent=0){Fixture f(indent,horizontal);const auto rect=f.view->visualRect(f.cell),area=rect.adjusted(2,2,-2,-2);const int resting=textLeft(f.view->viewport()->grab().toImage(),area);auto* editor=f.edit();const int editing=textLeft(f.view->viewport()->grab().toImage(),area);std::cout<<"plain resting text x="<<resting<<" editing x="<<editing<<" editor="<<editor->x()<<','<<editor->y()<<','<<editor->width()<<','<<editor->height()<<" cell="<<rect.x()<<','<<rect.y()<<','<<rect.width()<<','<<rect.height()<<'\n';require(std::abs(resting-editing)<=1,"starting an in-cell edit does not shift the visible text horizontally");require(editor->geometry().contains(rect.adjusted(1,1,-1,-1)),"the editor covers the cell interior so committed glyph edges cannot show underneath");}
void formulaIndentation(bool wrap){
 Fixture f(2);const QString formula=wrap?"=SUM("+QString("A1+").repeated(100)+"A1)":"=SUM(A1:A6)+SUM(B1:B6)+SUM(C1:C6)";require(f.model->setData(f.cell,formula,Qt::EditRole),"prepare an indented formula");const auto before=f.window.currentSession().snapshot();auto* editor=f.edit(formula);const auto margins=editor->textMargins();require(margins.left()>0&&editor->alignment().testFlag(Qt::AlignLeft),"formula editing retains indentation and starts from the left");
 if(wrap){require(editor->property("formula.softWrap").toBool(),"a long indented formula wraps inside the viewport");QTest::mouseClick(editor,Qt::LeftButton,Qt::NoModifier,{margins.left()+2,editor->property("formula.linePitch").toInt()/2});require(editor->cursorPosition()==0,"clicking the visible start of an indented wrapped formula puts the caret before its first character");}
 else require(editor->width()-margins.left()-margins.right()>=std::ceil(QFontMetricsF(editor->font()).horizontalAdvance(formula))+2,"expansion leaves room for the complete formula as well as its indentation");
 require(f.window.currentSession().snapshot()==before,"formula presentation and caret pointing leave the canonical workbook untouched");QTest::keyClick(editor,Qt::Key_Escape);QApplication::processEvents();require(!f.view->activeCellEditor()&&f.window.currentSession().snapshot()==before,"Escape cancels an indented formula edit without changing its content or formatting");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const std::string mode=argc>1?argv[1]:"all";if(mode=="all"||mode=="indent")indentation();if(mode=="all"||mode=="alignment")alignment();if(mode=="all"||mode=="aligned"){alignment("right",2);alignment("center",0);}if(mode=="all"||mode=="formula")formulaIndentation(false);if(mode=="all"||mode=="wrap")formulaIndentation(true);std::cout<<"cell editor presentation tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
