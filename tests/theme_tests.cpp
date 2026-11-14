#include "tigersheets/theme.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool result,const char* message){if(!result)throw std::runtime_error(message);}
double luminance(const QColor& color){const auto channel=[](double value){return value<=0.04045?value/12.92:std::pow((value+0.055)/1.055,2.4);};return 0.2126*channel(color.redF())+0.7152*channel(color.greenF())+0.0722*channel(color.blueF());}
double contrast(const QColor& first,const QColor& second){const auto a=luminance(first),b=luminance(second);return (std::max(a,b)+0.05)/(std::min(a,b)+0.05);}
void worksheetEditorStaysInsideItsCell(){
 QTemporaryDir state;qputenv("TIGERSHEETS_STATE_DIR",state.path().toUtf8());tigersheets::MainWindow window;window.resize(1100,720);window.show();QApplication::processEvents();
 auto* view=dynamic_cast<tigersheets::WorksheetView*>(window.findChild<QTableView*>("worksheet.grid"));require(view!=nullptr,"themed worksheet exposes its native cell view");auto* model=dynamic_cast<tigersheets::WorksheetModel*>(view->model());require(model!=nullptr,"themed worksheet exposes its cell model");
 const auto index=model->index(5,1);require(model->setData(index,"Gross Profit",Qt::EditRole),"seed the reported B6 editing example");view->setCurrentIndex(index);view->scrollTo(index);QApplication::processEvents();const auto before=window.currentSession().snapshot();
 QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,view->visualRect(index).center());QTest::keyClick(view,Qt::Key_F2);QApplication::processEvents();auto* editor=view->activeCellEditor();require(editor&&editor->isVisible()&&editor->text()=="Gross Profit","F2 opens the existing B6 text for in-place editing");
 const auto cell=view->visualRect(index);const QRect editingRect(editor->mapTo(view->viewport(),QPoint()),editor->size());
 const auto geometryMessage=QString("themed in-place editor must fit inside B6 instead of protruding like a separate control: cell %1,%2 %3x%4; editor %5,%6 %7x%8").arg(cell.x()).arg(cell.y()).arg(cell.width()).arg(cell.height()).arg(editingRect.x()).arg(editingRect.y()).arg(editingRect.width()).arg(editingRect.height()).toStdString();
 require(cell.contains(editingRect),geometryMessage.c_str());
 QTest::keyClick(editor,Qt::Key_End);require(editor->hasFocus()&&editor->cursorPosition()==editor->text().size(),"the cell-sized editor keeps its focused text caret usable");require(window.currentSession().snapshot()==before,"opening and positioning an editor keeps the cell uncommitted");view->cancelEntry();
 const auto checkBounds=[&](const QModelIndex& at){view->setCurrentIndex(at);view->scrollTo(at);view->setFocus();view->edit(at);QApplication::processEvents();auto* line=view->activeCellEditor();require(line&&line->isVisible(),"boundary fixture opens its native editor");const QRect rect(line->mapTo(view->viewport(),QPoint()),line->size());require(view->visualRect(at).contains(rect),"in-place editors remain inside narrow, scrolled and merged cells");view->cancelEntry();};
 view->setRowHeight(5,18);view->setColumnWidth(1,48);checkBounds(index);
 const auto scrolled=model->index(200000,20);require(model->setData(scrolled,"Gross Profit",Qt::EditRole),"seed a distant scrolled cell");checkBounds(scrolled);
 require(model->apply(tigersheets::Json::array({{{"type","set_sheet_layout"},{"sheet",model->sheetId().toStdString()},{"mergedRanges",tigersheets::Json::array({{{"row1",8},{"col1",2},{"row2",9},{"col2",4}}})}}})),"seed a merged editing area");checkBounds(model->index(7,1));
}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);if(argc>1&&std::string(argv[1])=="worksheet_editor"){try{worksheetEditorStaysInsideItsCell();std::cout<<"worksheet editor geometry tests passed\n";return 0;}catch(const std::exception& error){std::cerr<<"theme test failed: "<<error.what()<<'\n';return 1;}}
 QWidget root;root.setWindowTitle("Compact suite controls");root.setMinimumWidth(760);auto* layout=new QVBoxLayout(&root);layout->setContentsMargins(8,4,8,4);layout->setSpacing(4);
 auto* suiteMenu=new QMenuBar;suiteMenu->setNativeMenuBar(false);suiteMenu->setObjectName("workspace.menubar");auto* fileAction=suiteMenu->addMenu("File")->menuAction();auto* identity=new QWidget(suiteMenu);identity->setObjectName("workspace.identity");auto* identityLayout=new QHBoxLayout(identity);identityLayout->setContentsMargins(10,4,10,4);identityLayout->setSpacing(7);auto* mark=new QLabel("T");mark->setFixedSize(28,28);identityLayout->addWidget(mark);auto* brand=new QLabel("TigerSheets");brand->setObjectName("workspace.brand");identityLayout->addWidget(brand);suiteMenu->setCornerWidget(identity,Qt::TopLeftCorner);layout->addWidget(suiteMenu);
 auto* menu=new QMenuBar;menu->setNativeMenuBar(false);menu->addMenu("File");menu->addMenu("Edit");menu->addMenu("View");layout->addWidget(menu);
 auto* tabs=new QTabBar;tabs->setExpanding(false);for(const auto* title:{"Home","Insert","Data","Review"})tabs->addTab(title);layout->addWidget(tabs);
 auto* controls=new QHBoxLayout;controls->setSpacing(4);auto* tool=new QToolButton;tool->setText("Copy");tool->setAutoRaise(true);controls->addWidget(tool);auto* save=new QPushButton("Save");save->setProperty("role","primary");controls->addWidget(save);auto* combo=new QComboBox;combo->addItems({"General","Currency"});controls->addWidget(combo);auto* spin=new QSpinBox;spin->setRange(6,96);spin->setValue(11);controls->addWidget(spin);auto* number=new QDoubleSpinBox;number->setValue(12.5);controls->addWidget(number);controls->addStretch();layout->addLayout(controls);
 auto* formula=new QLineEdit("=SUM(A1:A8)");formula->setObjectName("formula.input");layout->addWidget(formula);auto* field=new QLineEdit("Workbook name");layout->addWidget(field);
 tigersheets::applyAppTheme(&root);root.adjustSize();root.show();formula->setFocus();app.processEvents();
 try{
  std::cout<<"control heights: tool="<<tool->sizeHint().height()<<" button="<<save->sizeHint().height()<<" line="<<field->sizeHint().height()<<" combo="<<combo->sizeHint().height()<<" spin="<<spin->sizeHint().height()<<" double="<<number->sizeHint().height()<<" tabs="<<tabs->sizeHint().height()<<" menu="<<menu->sizeHint().height()<<'\n';
  require(tool->sizeHint().height()<=26&&save->sizeHint().height()<=26,"suite toolbar buttons must fit a compact 26px row");
  require(field->sizeHint().height()<=26&&combo->sizeHint().height()<=26&&spin->sizeHint().height()<=26&&number->sizeHint().height()<=26,"native worksheet controls must fit compact 26px rows");
  require(tabs->sizeHint().height()<=28&&menu->sizeHint().height()<=32,"menu and ribbon tabs must not create tall chrome rows");
  const auto fileCenter=suiteMenu->mapTo(&root,suiteMenu->actionGeometry(fileAction).center()),brandCenter=brand->mapTo(&root,brand->rect().center());
  require(std::abs(fileCenter.y()-brandCenter.y())<=4,"File menu must share the identity row's visual baseline");
  require(root.font().pointSizeF()>=9.5,"compact controls must retain readable suite typography");
  const auto palette=root.palette();require(contrast(palette.color(QPalette::Text),palette.color(QPalette::Base))>=4.5,"normal worksheet text must retain accessible contrast");require(contrast(palette.color(QPalette::HighlightedText),palette.color(QPalette::Highlight))>=4.5,"forest selection text must retain accessible contrast");require(contrast(palette.color(QPalette::ButtonText),palette.color(QPalette::Button))>=4.5,"toolbar labels must retain accessible contrast");
  const auto inputHint=formula->sizeHint();field->setFocus();app.processEvents();const auto idleImage=formula->grab().toImage();const auto idleBorder=idleImage.pixelColor(idleImage.width()/2,0);
  formula->setFocus();app.processEvents();const auto focusedImage=formula->grab().toImage();const auto focusedBorder=focusedImage.pixelColor(focusedImage.width()/2,0);
  require(formula->hasFocus()&&contrast(focusedBorder,palette.color(QPalette::Base))>=3&&contrast(focusedBorder,idleBorder)>=1.5,"keyboard focus must remain visibly distinct on the flat formula field");
  require(formula->sizeHint()==inputHint,"keyboard focus must not resize dense input controls");
  worksheetEditorStaysInsideItsCell();
  std::cout<<"theme tests passed\n";
 }catch(const std::exception& error){std::cerr<<"theme test failed: "<<error.what()<<'\n';return 1;}
}
