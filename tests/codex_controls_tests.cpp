#include "tigersheets/codex_controls.hpp"
#include "tigersheets/theme.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QTest>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
using Json=nlohmann::json;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json catalog(){return Json::array({
 {{"id","hidden"},{"hidden",true},{"isDefault",true}},
 {{"id","other"},{"isDefault",true},{"supportedReasoningEfforts",{"medium","high"}},{"serviceTiers",{"standard"}}},
 {{"id","sol"},{"model","gpt-6.1-sol"},{"supportedReasoningEfforts",Json::array({{{"reasoningEffort","ultra"}},{{"reasoningEffort","max"}},"high","low","medium","xhigh"})},{"serviceTiers",Json::array({{{"id","fast"},{"name","Fast"},{"description","Extra credits"}},{{"id","default"},{"name","Standard"}}})},{"defaultServiceTier","fast"}}
 });}
void discoveredChoices(){
 CodexControls controls;applyAppTheme(&controls);controls.resize(304,70);controls.setCatalog(catalog());controls.setConnected(true);controls.show();QApplication::processEvents();
 auto* model=controls.findChild<QComboBox*>("codex.model");auto* effort=controls.findChild<QComboBox*>("codex.effort");auto* speed=controls.findChild<QComboBox*>("codex.speed");
 require(model->count()==2&&model->itemData(0)=="other","hidden models are omitted and server defaults sorted first");
 require(model->currentText()=="GPT-6.1 Sol"&&controls.model()=="gpt-6.1-sol","the suite model label preserves the actual wire ID");
 QStringList labels;for(int i=0;i<effort->count();++i)labels<<effort->itemText(i);
 require(labels==QStringList{"Light","Medium","High","XHigh","Ultra"}&&controls.reasoningEffort()=="low","reasoning choices match OwlDocs' advertised ordering and Light default");
 require(speed->count()==2&&speed->itemText(0)=="Fast"&&speed->currentText()=="Standard"&&controls.serviceTier()=="default","runtime speed choices default to Standard even when Fast is advertised first");
 require(speed->toolTip()=="Fast: Extra credits","speed descriptions are available as a tooltip");
 for(auto* choice:{effort,speed}){QStyleOptionComboBox option;option.initFrom(choice);option.rect=choice->rect();const auto field=choice->style()->subControlRect(QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxEditField,choice);for(int i=0;i<choice->count();++i)require(field.width()>=choice->fontMetrics().horizontalAdvance(choice->itemText(i)),"reasoning and speed labels fit at OwlDocs' dock width");}
}
void changingModels(){
 CodexControls controls;controls.setCatalog(catalog());controls.setConnected(true);
 auto* model=controls.findChild<QComboBox*>("codex.model");auto* effort=controls.findChild<QComboBox*>("codex.effort");auto* speed=controls.findChild<QComboBox*>("codex.speed");
 effort->setCurrentIndex(effort->findData("ultra"));speed->setCurrentIndex(speed->findData("fast"));model->setCurrentIndex(model->findData("other"));
 require(effort->count()==2&&controls.reasoningEffort()=="medium"&&speed->count()==1&&controls.serviceTier()=="standard","changing models replaces its capabilities and resets reasoning and speed to safe defaults");
 controls.setCatalog(Json::array({{{"id","legacy"},{"supportedReasoningEfforts",{"max","minimal"}},{"additionalSpeedTiers",{"fast"}},{"defaultServiceTier","fast"}}}));
 require(effort->itemText(0)=="max"&&controls.reasoningEffort()=="max","other models retain their discovered effort choices and order");
 require(speed->count()==2&&speed->currentText()=="Standard"&&controls.serviceTier().isEmpty()&&speed->itemData(1)=="fast","a legacy Fast-only catalog includes an ordinary Standard choice without opting in to Fast");
 require(speed->toolTip()=="Fast mode can use additional ChatGPT credits.","the suite Fast tooltip is present without runtime descriptions");
 controls.setCatalog(Json::array({{{"id","basic"}}}));
 require(effort->currentText()=="Default"&&controls.reasoningEffort().isEmpty()&&speed->count()==0&&!speed->isEnabled(),"older runtimes without effort or speed metadata keep default reasoning and disable unavailable speed choices");
 controls.setConnected(false);require(!model->isEnabled()&&!effort->isEnabled()&&!speed->isEnabled(),"disconnect disables every selector");
}
}
int main(int argc,char** argv){QApplication app(argc,argv);try{const auto selected=argc>1?std::string(argv[1]):std::string();if(selected.empty()||selected=="catalog")discoveredChoices();if(selected.empty()||selected=="model")changingModels();std::cout<<"Codex control tests passed\n";}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}}
