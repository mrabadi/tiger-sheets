#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/worksheet_cell_format.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QHeaderView>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QTabWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {
void require(bool okay,const char* text){if(!okay)throw std::runtime_error(text);}
const std::vector<std::string> styles={"none","thin","medium","dashed","dotted","thick","double","hair","mediumDashed","dashDot","mediumDashDot","dashDotDot","mediumDashDotDot","slantDashDot"};
Json edge(const std::string& style){return {{"style",style},{"color","#a31515"}};}
void validation(){
 for(const auto& style:styles){const Json patch={{"border",{{"top",edge(style)}}}};require(validatedWorksheetCellStyle(patch)==patch,"every native standard border style must pass the same UI validation as Session and OOXML");}
 bool rejected=false;try{validatedWorksheetCellStyle({{"border",{{"top",edge("invalid-style")}}}});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"unknown border styles remain rejected");
 rejected=false;try{validatedWorksheetCellStyle({{"border",{{"top",{{"style",4},{"color","#000000"}}}}}});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"non-text line styles remain rejected");
}
void rendering(){
 WorkbookSession session;WorksheetModel model(session);WorksheetView view;view.setModel(&model);view.resize(700,400);view.setShowGrid(false);view.horizontalHeader()->setDefaultSectionSize(180);view.verticalHeader()->setDefaultSectionSize(40);view.setCurrentIndex(model.index(7,3));view.show();
 for(int row=2;row<=5;++row)require(model.apply(Json::array({{{"type","set_cell"},{"sheet",model.sheetId().toStdString()},{"row",row},{"col",2},{"input",""},{"style",{{"border",{{"top",edge(std::vector<std::string>{"thin","thick","double","dashed"}[row-2])}}}}}}})),"seed actual imported native border style");
 QApplication::processEvents();const auto before=session.snapshot();const auto image=view.viewport()->grab().toImage();auto colored=[&](int row){const auto rect=view.visualRect(model.index(row-1,1));int count=0;for(int y=rect.top();y<rect.top()+5;++y)if(image.pixelColor(rect.center().x(),y)==QColor("#a31515"))++count;return count;};
 require(colored(2)==1&&colored(3)==3,"actual grid rendering distinguishes the thin one-pixel border from the three-pixel thick border");const auto doubleRect=view.visualRect(model.index(3,1));require(colored(4)==2&&image.pixelColor(doubleRect.center().x(),doubleRect.top()+1)!=QColor("#a31515"),"actual double border has two lines separated by a visible gap");const auto dashedRect=view.visualRect(model.index(4,1));int marks=0;for(int x=dashedRect.left()+5;x<dashedRect.right()-5;++x)if(image.pixelColor(x,dashedRect.top())==QColor("#a31515"))++marks;require(marks>30&&marks<dashedRect.width()-25,"native dashed borders have visible runs and gaps rather than a solid thin substitute");require(session.snapshot()==before,"painting native border weights and patterns never changes workbook data/history");
}
void axes(){
 WorkbookSession session;WorksheetModel model(session);WorksheetView view;view.setModel(&model);view.resize(700,300);view.setShowGrid(false);view.horizontalHeader()->setDefaultSectionSize(180);view.verticalHeader()->setDefaultSectionSize(40);view.setCurrentIndex(model.index(5,3));view.show();
 for(const auto* kind:{"medium","thick","double"}){Json border=Json::object();for(const auto* side:{"top","bottom","left","right"})border[side]=edge(kind);require(model.apply(Json::array({{{"type","set_cell"},{"sheet",model.sheetId().toStdString()},{"row",2},{"col",2},{"input",""},{"style",{{"border",border}}}}})),"seed four native border axes");QApplication::processEvents();const auto rect=view.visualRect(model.index(1,1));const auto image=view.viewport()->grab().toImage();const auto ink=QColor("#a31515");require(image.pixelColor(rect.center().x(),rect.top())==ink&&image.pixelColor(rect.center().x(),rect.bottom())==ink&&image.pixelColor(rect.left(),rect.center().y())==ink&&image.pixelColor(rect.right(),rect.center().y())==ink,"every native weight must reach the true outer edge on all four axes, including even-width medium bottom/right");}
}
struct Fixture {
 MainWindow window;WorksheetView* view;WorksheetModel* model;QMenu* menu;
 Fixture(){window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();view=window.findChild<WorksheetView*>("worksheet.grid");model=dynamic_cast<WorksheetModel*>(view->model());menu=window.findChild<QMenu*>("tutorial.borderMenu");require(view&&model&&menu,"actual native worksheet and Borders menu exist");model->setData(model->index(1,1),"100",Qt::EditRole);model->setData(model->index(2,2),"=1+2",Qt::EditRole);view->setCurrentIndex(model->index(1,1));view->selectionModel()->select(QItemSelection(model->index(1,1),model->index(2,2)),QItemSelectionModel::ClearAndSelect);view->setFocus();}
 void command(char letter){QTest::keyClick(view,Qt::Key_7,Qt::AltModifier);QTest::keyRelease(view,Qt::Key_Alt);QApplication::processEvents();auto* action=static_cast<QAction*>(nullptr);for(auto* candidate:menu->actions())if(candidate->property("ribbon.keyTip").toString()==QString(letter))action=candidate;require(action&&action->isEnabled(),"source-visible common Border commands must be genuinely enabled");QTest::keyClick(menu,Qt::Key(letter));QApplication::processEvents();require(!menu->isVisible(),"actual Border command applies and closes the native popup");}
};
void commands(){
 for(const auto key:std::string("ASTBHDCU")){Fixture f;const auto before=f.window.currentSession().snapshotView().at("sheets");const auto selected=f.view->selectionModel()->selection();f.command(key);for(int row=1;row<=2;++row)for(int col=1;col<=2;++col){const auto border=f.model->cellStyle(row,col).value("border",Json::object());const bool outside=key=='S'||key=='T',all=key=='A',top=key=='D'||key=='C'||key=='U',bottom=key=='B'||key=='H'||top;require(border.contains("top")==bool(all||(outside&&row==1)||(top&&row==1)),"common Border command applies the top edge only at its proper range boundary");require(border.contains("bottom")==bool(all||(outside&&row==2)||(bottom&&row==2)),"common Border command applies the bottom edge only at its proper range boundary");require(border.contains("left")==bool(all||(outside&&col==1))&&border.contains("right")==bool(all||(outside&&col==2)),"outside borders do not invent internal cell edges");for(const auto& [side,value]:border.items()){const std::string expected=(key=='T'||(side=="bottom"&&(key=='H'||key=='C')))?"thick":side=="bottom"&&(key=='B'||key=='U')?"double":"thin";require(value.at("style")==expected,"native common commands retain the requested thin, thick or double line identity");}}require(f.model->raw(1,1)=="100"&&f.model->raw(2,2)=="=1+2","border presets preserve input values and formulas");require(f.window.executeCommand("undo")&&f.window.currentSession().snapshotView().at("sheets")==before&&f.view->selectionModel()->selection()==selected,"each actual common Border command is exactly one Undo and retains the selection");}
}
void page(){
 Fixture f;std::exception_ptr error;bool opened=false;QTimer::singleShot(0,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());try{require(dialog,"actual Ctrl1 opens Format Cells");opened=true;auto* tabs=dialog->findChild<QTabWidget*>("formatCells.tabs");QTest::mouseClick(tabs->tabBar(),Qt::LeftButton,Qt::NoModifier,tabs->tabBar()->tabRect(3).center());auto* list=dialog->findChild<QListWidget*>("formatCells.borderStyleList");require(list&&list->count()==14,"source Border page has all fourteen native line samples");for(int i=0;i<list->count();++i)require(list->item(i)->flags()&Qt::ItemIsEnabled,"source line samples must support their native border styles rather than appear as disabled substitutes");QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(12)).center());QTest::mouseClick(dialog->findChild<QCheckBox*>("formatCells.border.top"),Qt::LeftButton);QTest::keyClick(dialog,Qt::Key_Return);}catch(...){error=std::current_exception();if(dialog)dialog->reject();}});QTest::keyClick(f.view,Qt::Key_1,Qt::ControlModifier);QTest::keyRelease(f.view,Qt::Key_Control);require(opened,"native source Border page was inspected");if(error)std::rethrow_exception(error);require(f.model->cellStyle(1,1).at("border").at("top").at("style")=="thick","genuine graphical Thick sample followed by Top applies a thick native edge");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());try{const std::string selected=argc>1?argv[1]:"validation";if(selected=="render")rendering();else if(selected=="axes")axes();else if(selected=="commands")commands();else if(selected=="page")page();else validation();std::cout<<"Native standard border commands passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
