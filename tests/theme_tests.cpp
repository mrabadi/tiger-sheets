#include "tigersheets/theme.hpp"
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
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool result,const char* message){if(!result)throw std::runtime_error(message);}
double luminance(const QColor& color){const auto channel=[](double value){return value<=0.04045?value/12.92:std::pow((value+0.055)/1.055,2.4);};return 0.2126*channel(color.redF())+0.7152*channel(color.greenF())+0.0722*channel(color.blueF());}
double contrast(const QColor& first,const QColor& second){const auto a=luminance(first),b=luminance(second);return (std::max(a,b)+0.05)/(std::min(a,b)+0.05);}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);QWidget root;root.setWindowTitle("Compact suite controls");root.setMinimumWidth(760);auto* layout=new QVBoxLayout(&root);layout->setContentsMargins(8,4,8,4);layout->setSpacing(4);
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
  std::cout<<"theme tests passed\n";
 }catch(const std::exception& error){std::cerr<<"theme test failed: "<<error.what()<<'\n';return 1;}
}
