#include "tigersheets/paste_special.hpp"
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <map>
namespace tigersheets {
namespace {
class PasteSpecialDialog final:public QDialog {
public:
 std::map<int,QAbstractButton*> keys;
 explicit PasteSpecialDialog(QWidget* parent):QDialog(parent){}
protected:
 void keyPressEvent(QKeyEvent* event) override {
  if(event->modifiers()==Qt::NoModifier||event->modifiers()==Qt::AltModifier){const auto found=keys.find(event->key());if(found!=keys.end()){found->second->click();event->accept();return;}}
  QDialog::keyPressEvent(event);
 }
};
}
std::optional<PasteSpecialOptions> choosePasteSpecial(QWidget* parent){
 PasteSpecialDialog dialog(parent);dialog.setObjectName("pasteSpecial.dialog");dialog.setWindowTitle("Paste Special");dialog.setWindowFlag(Qt::WindowContextHelpButtonHint);dialog.setFixedSize(434,286);dialog.setStyleSheet("QDialog { background:#f0f0f0; } QLabel,QGroupBox,QRadioButton,QCheckBox,QPushButton { font:11px Calibri; color:#000000; } QGroupBox { border:1px solid #d0d0d0; margin-top:6px; padding-top:3px; } QGroupBox::title { subcontrol-origin:margin; left:4px; } QRadioButton:disabled,QGroupBox:disabled { color:#aaaaaa; } QPushButton { min-height:20px; padding:0px 6px; border:1px solid #aaaaaa; border-radius:0px; background:#e7e7e7; } QPushButton:default { border:2px solid #0078d7; } QPushButton:disabled { color:#aaaaaa; }");auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(8,7,8,7);layout->setSpacing(5);auto* paste=new QGroupBox("Paste",&dialog);paste->setObjectName("pasteSpecial.paste");auto* choices=new QGridLayout(paste);choices->setContentsMargins(8,8,8,5);choices->setHorizontalSpacing(22);choices->setVerticalSpacing(1);auto* group=new QButtonGroup(paste);
 const std::pair<const char*,const char*> modes[]={{"&All","all"},{"&Formulas","formulas"},{"&Values","values"},{"Forma&ts","formats"},{"&Comments and Notes","comments"},{"Validatio&n","validation"},{"All using Source t&heme","allSourceTheme"},{"All e&xcept borders","allExceptBorders"},{"Column &widths","columnWidths"},{"Formulas and numbe&r formats","formulasNumberFormats"},{"Val&ues and number formats","valuesNumberFormats"},{"All merging conditional formats","allConditionalFormats"}};const int keys[]={Qt::Key_A,Qt::Key_F,Qt::Key_V,Qt::Key_T,Qt::Key_C,Qt::Key_N,Qt::Key_H,Qt::Key_X,Qt::Key_W,Qt::Key_R,Qt::Key_U,Qt::Key_P};
 for(int index=0;index<12;++index){auto* radio=new QRadioButton(QString::fromLatin1(modes[index].first),paste);radio->setObjectName("pasteSpecial."+QString::fromLatin1(modes[index].second));radio->setAccessibleName(QString::fromLatin1(modes[index].first).remove('&'));group->addButton(radio,index);choices->addWidget(radio,index%6,index/6);dialog.keys[keys[index]]=radio;if(index==0)radio->setChecked(true);if(index==11)radio->setEnabled(false);}
 layout->addWidget(paste);auto* operation=new QGroupBox("Operation",&dialog);operation->setObjectName("pasteSpecial.operation");auto* operationLayout=new QGridLayout(operation);operationLayout->setContentsMargins(8,7,8,5);operationLayout->setVerticalSpacing(1);operationLayout->setHorizontalSpacing(22);auto* operations=new QButtonGroup(operation);const std::pair<const char*,const char*> kinds[]={{"N&one","none"},{"A&dd","add"},{"&Subtract","subtract"},{"&Multiply","multiply"},{"D&ivide","divide"}};const int operationKeys[]={Qt::Key_O,Qt::Key_D,Qt::Key_S,Qt::Key_M,Qt::Key_I};for(int index=0;index<5;++index){auto* radio=new QRadioButton(kinds[index].first,operation);radio->setObjectName("pasteSpecial.operation."+QString::fromLatin1(kinds[index].second));operations->addButton(radio,index);operationLayout->addWidget(radio,index<3?index:index-3,index<3?0:1);dialog.keys[operationKeys[index]]=radio;if(index==0)radio->setChecked(true);}QObject::connect(group,&QButtonGroup::idToggled,&dialog,[operation,operations](int index,bool checked){if(!checked)return;const bool arithmetic=index==0||index==1||index==2||index==6||index==7||index==9||index==10;operation->setEnabled(arithmetic);if(!arithmetic)operations->button(0)->setChecked(true);});layout->addWidget(operation);
 auto* flags=new QHBoxLayout;auto* skip=new QCheckBox("Skip &blanks",&dialog);skip->setObjectName("pasteSpecial.skipBlanks");auto* transpose=new QCheckBox("Transpos&e",&dialog);transpose->setObjectName("pasteSpecial.transpose");flags->addWidget(skip);flags->addWidget(transpose);layout->addLayout(flags);dialog.keys[Qt::Key_B]=skip;dialog.keys[Qt::Key_E]=transpose;
 auto* bottom=new QHBoxLayout;auto* link=new QPushButton("Paste &Link",&dialog);link->setObjectName("pasteSpecial.link");link->setFixedWidth(74);link->setEnabled(false);bottom->addWidget(link);bottom->addStretch();auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);buttons->setObjectName("pasteSpecial.buttons");bottom->addWidget(buttons);layout->addLayout(bottom);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
 if(dialog.exec()!=QDialog::Accepted)return std::nullopt;
 return PasteSpecialOptions{modes[group->checkedId()].second,skip->isChecked(),transpose->isChecked(),kinds[operations->checkedId()].second};
}
}
