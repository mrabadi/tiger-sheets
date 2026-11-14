#include "tigersheets/worksheet_cell_format.hpp"
#include "tigersheets/grid.hpp"
#include <QApplication>
#include <QTableView>
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
Json fixture(){return Json::parse(R"({"revision":0,"sheets":[{"id":"s","name":"Data","cells":[{"row":1,"col":1,"input":"'00123","style":{"bold":true,"note":"Keep this note","fill":"#ffffff"}},{"row":1,"col":2,"input":"=1+2","style":{"italic":true}},{"row":2,"col":1,"input":"TRUE"},{"row":2,"col":2,"input":"#N/A"},{"row":3,"col":1,"input":"Outside"}]}]})");}
void select(QTableView& view,WorksheetModel& model,int top=0,int left=0,int bottom=1,int right=1){view.setCurrentIndex(model.index(top,left));view.selectionModel()->select(QItemSelection(model.index(top,left),model.index(bottom,right)),QItemSelectionModel::ClearAndSelect);}
void directFormatting(){WorkbookSession session(fixture());WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model);int changes=0;model.onChanged=[&]{++changes;};
 const Json style={{"numberFormat","0.00"},{"alignment","center"},{"fontFamily","DejaVu Sans"},{"fontSize",13},{"color","#123456"}};
 require(formatWorksheetCells(nullptr,model,view,{{"style",style}}),"direct cell formatting succeeds");
 for(int row=0;row<2;++row)for(int col=0;col<2;++col){const auto actual=model.cellStyle(row,col);for(const auto& [key,value]:style.items())require(actual.at(key)==value,"the complete selected rectangle receives formatting");}
 require(model.raw(0,0)=="'00123"&&model.raw(0,1)=="=1+2"&&model.raw(1,0)=="TRUE"&&model.raw(1,1)=="#N/A"&&model.cellStyle(0,0).at("note")=="Keep this note"&&model.cellStyle(0,0).at("bold")==true&&model.cellStyle(0,1).at("italic")==true,"formatting preserves typed inputs, formulas, notes and existing styles");
 require(model.cellStyle(2,0).empty()&&changes==1&&session.snapshotView().at("revision")==1,"formatting affects only the selection in one transaction");
 require(session.undo(),"formatting supports one undo");model.refresh();require(session.snapshotView().at("sheets")==fixture().at("sheets"),"one undo restores all original formatting and values");
}
void clearFormats(){auto source=fixture();source["sheets"][0]["cells"][0]["style"]["ooxmlStyleId"]=7;source["sheets"][0]["cells"][0]["style"]["wrap"]=true;WorkbookSession session(source);WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model);int changes=0;model.onChanged=[&]{++changes;};
 require(clearWorksheetFormats(model,view),"clear formats succeeds");require(model.cellStyle(0,0)==Json({{"note","Keep this note"}})&&model.cellStyle(0,1).empty(),"clear formats removes every visual style including imported style identity but retains the note");
 require(model.raw(0,0)=="'00123"&&model.raw(0,1)=="=1+2"&&model.raw(1,0)=="TRUE"&&model.raw(1,1)=="#N/A"&&changes==1,"clear formats preserves values and formulas in one observable transaction");
 require(session.undo(),"clear formats supports undo");model.refresh();require(session.snapshotView().at("sheets")==source.at("sheets"),"one undo restores imported and ordinary styles exactly");
}
void nativeDialog(){WorkbookSession session(fixture());WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model);QString failure;bool opened=false;
 QTimer::singleShot(0,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog){failure="Format Cells did not open a native modal dialog";return;}try{opened=true;require(dialog->objectName()=="formatCells.dialog"&&dialog->windowTitle()=="Format Cells","dialog identity and title match Format Cells");auto* tabs=dialog->findChild<QTabWidget*>("formatCells.tabs");require(tabs&&tabs->count()==4&&tabs->tabText(0)=="Number"&&tabs->tabText(1)=="Alignment"&&tabs->tabText(2)=="Font"&&tabs->tabText(3)=="Fill","all four formatting tabs are available");
 auto* number=dialog->findChild<QComboBox*>("formatCells.numberFormat");auto* alignment=dialog->findChild<QComboBox*>("formatCells.alignment");auto* bold=dialog->findChild<QCheckBox*>("formatCells.bold");auto* fill=dialog->findChild<QLineEdit*>("formatCells.fill");auto* size=dialog->findChild<QSpinBox*>("formatCells.fontSize");auto* buttons=dialog->findChild<QDialogButtonBox*>("formatCells.buttons");require(number&&alignment&&bold&&fill&&size&&buttons,"native formatting controls are discoverable");require(!number->accessibleName().isEmpty()&&!alignment->accessibleName().isEmpty()&&!bold->accessibleName().isEmpty()&&bold->isChecked()&&fill->text()=="#ffffff"&&number->currentText()=="General","controls have accessible names and current cell defaults");
 number->setEditText("$#,##0.00");alignment->setCurrentIndex(alignment->findData("right"));size->setValue(14);tabs->setCurrentIndex(3);fill->setFocus();fill->selectAll();QTest::keyClicks(fill,"#ffeecc");QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);
 }catch(const std::exception& error){failure=QString::fromUtf8(error.what());dialog->reject();}});
 require(formatWorksheetCells(nullptr,model,view),"accepting Format Cells applies the draft");require(failure.isEmpty(),failure.toUtf8().constData());require(opened,"a real dialog was shown");
 require(model.cellStyle(0,0).at("numberFormat")=="$#,##0.00"&&model.cellStyle(1,1).at("alignment")=="right"&&model.cellStyle(1,1).at("fontSize")==14&&model.cellStyle(1,1).at("fill")=="#ffeecc","accepted controls format every selected cell");
 require(!model.cellStyle(0,1).contains("bold")&&model.cellStyle(0,1).at("italic")==true,"unmodified controls preserve different existing styles across the selection");require(session.undo(),"dialog formatting is one undo");model.refresh();require(session.snapshotView().at("sheets")==fixture().at("sheets"),"undo restores dialog changes together");
 const auto before=session.snapshot();QTimer::singleShot(0,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog){failure="Cancel dialog missing";return;}dialog->findChild<QComboBox*>("formatCells.numberFormat")->setEditText("0%");QTest::keyClick(dialog,Qt::Key_Escape);});require(!formatWorksheetCells(nullptr,model,view)&&session.snapshot()==before,"Escape discards all formatting drafts without history");
}
void validation(){WorkbookSession session(fixture());WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model);const auto before=session.snapshot();
 for(const auto& style:Json::array({Json{{"unsupported",true}},Json{{"note","replace note"}},Json{{"alignment","diagonal"}},Json{{"color","not-a-color"}},Json{{"fill",7}},Json{{"fontSize",401}},Json{{"fontFamily",""}},Json{{"bold","yes"}}})){bool rejected=false;try{formatWorksheetCells(nullptr,model,view,{{"style",style}});}catch(const std::exception&){rejected=true;}require(rejected&&session.snapshot()==before&&!session.canUndo(),"unsupported or malformed style properties refuse atomically");}
 view.selectionModel()->select(QItemSelection(model.index(3,0),model.index(3,0)),QItemSelectionModel::Select);bool rejected=false;try{clearWorksheetFormats(model,view);}catch(const std::exception&){rejected=true;}require(rejected&&session.snapshot()==before,"discontinuous selections refuse without formatting gaps");
 select(view,model,0,0,100000,0);rejected=false;try{formatWorksheetCells(nullptr,model,view,{{"style",{{"bold",true}}}});}catch(const std::exception&){rejected=true;}require(rejected&&session.snapshot()==before,"formatting above the bounded selection limit refuses before allocation or mutation");
}
void noChanges(){WorkbookSession session(fixture());WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model);const auto before=session.snapshot();
 require(formatWorksheetCells(nullptr,model,view,{{"style",Json::object()}})&&session.snapshot()==before&&!session.canUndo(),"an empty formatting change creates no blank styles or undo entry");
 QTimer::singleShot(0,[]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->accept();});require(formatWorksheetCells(nullptr,model,view)&&session.snapshot()==before&&!session.canUndo(),"accepting unchanged dialog settings preserves mixed formatting and creates no history");
}
void clearUnformatted(){WorkbookSession session;WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model);const auto before=session.snapshot();require(clearWorksheetFormats(model,view)&&session.snapshot()==before&&!session.canUndo(),"clearing an unformatted range does not allocate blank cells or create history");}
void clearColors(){WorkbookSession session(fixture());WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model);
 require(formatWorksheetCells(nullptr,model,view,{{"style",{{"fill",""},{"color",""}}}}),"No Fill and Automatic font color accept explicit reset values");require(!model.data(model.index(0,0),Qt::BackgroundRole).isValid()&&!model.data(model.index(0,0),Qt::ForegroundRole).isValid(),"cleared colors use default grid brushes rather than invalid QColor brushes");
 require(model.cellStyle(0,0).at("fill")==""&&model.cellStyle(0,0).at("color")==""&&model.cellStyle(0,0).at("bold")==true&&model.cellStyle(0,0).at("note")=="Keep this note"&&model.raw(0,0)=="'00123","color reset sentinels preserve unrelated styles, notes and cell values");require(session.undo(),"color resets are one undo");model.refresh();require(session.snapshotView().at("sheets")==fixture().at("sheets"),"one undo restores color settings exactly");
}
void explicitLeft(){WorkbookSession session(Json::parse(R"({"revision":0,"sheets":[{"id":"s","name":"Numbers","cells":[{"row":1,"col":1,"input":"42"},{"row":1,"col":2,"input":"=40+2"}]}]})"));WorksheetModel model(session);QTableView view;view.setModel(&model);select(view,model,0,0,0,1);
 require(formatWorksheetCells(nullptr,model,view,{{"style",{{"alignment","left"}}}}),"Format Cells accepts explicit Left alignment");model.setCalculated({{"revision",1},{"status","ok"},{"values",Json::array({{{"sheet","s"},{"row",1},{"col",2},{"type","number"},{"value",42}}})}});
 for(int col=0;col<2;++col)require((model.data(model.index(0,col),Qt::TextAlignmentRole).toInt()&Qt::AlignHorizontal_Mask)==Qt::AlignLeft,"explicit Left aligns both numeric literals and calculated numeric formulas to the left");
}
int main(int argc,char** argv){QApplication app(argc,argv);try{directFormatting();clearFormats();nativeDialog();validation();noChanges();clearUnformatted();clearColors();explicitLeft();std::cout<<"worksheet cell formatting tests passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
