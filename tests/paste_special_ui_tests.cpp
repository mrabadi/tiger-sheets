#include "tigersheets/paste_special.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 bool native=false;QTimer::singleShot(0,[&]{if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget())){native=dialog->windowTitle()=="Paste Special";for(const auto* name:{"all","formulas","values","formats","comments","validation","allSourceTheme","allExceptBorders","columnWidths","formulasNumberFormats","valuesNumberFormats","allConditionalFormats"})native&=dialog->findChild<QRadioButton*>("pasteSpecial."+QString::fromLatin1(name))!=nullptr;auto* operation=dialog->findChild<QGroupBox*>("pasteSpecial.operation");native&=operation!=nullptr;for(const auto* name:{"none","add","subtract","multiply","divide"})native&=dialog->findChild<QRadioButton*>("pasteSpecial.operation."+QString::fromLatin1(name))!=nullptr;auto* conditional=dialog->findChild<QRadioButton*>("pasteSpecial.allConditionalFormats");native&=conditional&&!conditional->isEnabled();auto* formats=dialog->findChild<QRadioButton*>("pasteSpecial.formats");if(formats)formats->click();native&=operation&&!operation->isEnabled();auto* link=dialog->findChild<QPushButton*>("pasteSpecial.link");native&=link&&!link->isEnabled();auto* skip=dialog->findChild<QCheckBox*>("pasteSpecial.skipBlanks");auto* transpose=dialog->findChild<QCheckBox*>("pasteSpecial.transpose");native&=skip&&transpose;if(const auto capture=qEnvironmentVariable("TIGERSHEETS_PASTE_CAPTURE");!capture.isEmpty())dialog->grab().save(capture);if(skip)skip->setChecked(true);if(transpose)transpose->setChecked(true);dialog->accept();}});
 const auto options=choosePasteSpecial(nullptr);require(native,"Paste Special reproduces both observed Paste columns, the five Operation choices, disabled conditional merge/link options, and Formats disabling Operation");require(options&&options->mode=="formats"&&options->skipBlanks&&options->transpose&&options->operation=="none","Transpose remains independent of the selected Formats paste mode and Skip blanks");std::cout<<"Native Paste Special dialog passed\n";
 }catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
