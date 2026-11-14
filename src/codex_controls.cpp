#include "tigersheets/codex_controls.hpp"
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <algorithm>
namespace tigersheets {
namespace {
using Json=nlohmann::json;
QString string(const Json& object,const char* key){const auto found=object.find(key);return found!=object.end()&&found->is_string()?QString::fromStdString(found->get<std::string>()):QString();}
QString wireModel(const Json& model){const auto wire=string(model,"model");return wire.isEmpty()?string(model,"id"):wire;}
void fitChoiceLabels(QComboBox* choice){
 QStyleOptionComboBox option;option.initFrom(choice);option.rect=QRect(0,0,200,choice->sizeHint().height());
 const int chrome=option.rect.width()-choice->style()->subControlRect(QStyle::CC_ComboBox,&option,QStyle::SC_ComboBoxEditField,choice).width();
 int width=0;for(int index=0;index<choice->count();++index)width=qMax(width,choice->fontMetrics().horizontalAdvance(choice->itemText(index)));
 choice->setMinimumWidth(width+chrome);
}
}
CodexControls::CodexControls(QWidget* parent):QWidget(parent){
 auto* controls=new QGridLayout(this);
 controls->setContentsMargins(0,0,0,0);
 controls->setHorizontalSpacing(6);
 controls->setVerticalSpacing(3);
 models_=new QComboBox(this);
 models_->setObjectName("codex.model");
 models_->setAccessibleName(tr("Codex model"));
 models_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
 models_->setMinimumContentsLength(8);
 models_->setToolTip(tr("Model (discovered from Codex)"));
 efforts_=new QComboBox(this);
 efforts_->setObjectName("codex.effort");
 efforts_->setAccessibleName(tr("Reasoning effort"));
 efforts_->setToolTip(tr("Reasoning effort"));
 tiers_=new QComboBox(this);
 tiers_->setObjectName("codex.speed");
 tiers_->setAccessibleName(tr("Response speed"));
 tiers_->setToolTip(tr("Speed / service tier"));
 controls->addWidget(new QLabel(tr("Model"),this),0,0);
 controls->addWidget(new QLabel(tr("Reasoning"),this),0,1);
 controls->addWidget(new QLabel(tr("Speed"),this),0,2);
 controls->addWidget(models_,1,0);
 controls->addWidget(efforts_,1,1);
 controls->addWidget(tiers_,1,2);
 controls->setColumnStretch(0,2);
 controls->setColumnStretch(1,1);
 controls->setColumnStretch(2,1);
 models_->setEnabled(false);
 efforts_->setEnabled(false);
 tiers_->setEnabled(false);
 connect(models_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){if(index>=0)refreshCapabilities();});
}
void CodexControls::setCatalog(const Json& catalog){
 const QSignalBlocker blocker(models_);
 catalog_=Json::array();
 if(catalog.is_array())for(const auto& item:catalog)if(item.is_object()&&!item.value("hidden",false)&&!wireModel(item).isEmpty())catalog_.push_back(item);
 std::stable_sort(catalog_.begin(),catalog_.end(),[](const auto& a,const auto& b){return a.value("isDefault",false)&&!b.value("isDefault",false);});
 models_->clear();
 for(const auto& item:catalog_){const auto id=wireModel(item);models_->addItem(id=="gpt-6.1-sol"?tr("GPT-6.1 Sol"):id,id);}
 const int preferred=models_->findData("gpt-6.1-sol");if(preferred>=0)models_->setCurrentIndex(preferred);
 refreshCapabilities();
}
void CodexControls::refreshCapabilities(){
 Json selected=Json::object();for(const auto& item:catalog_)if(wireModel(item)==model()){selected=item;break;}
 QStringList efforts;const auto advertised=selected.value("supportedReasoningEfforts",Json::array());
 if(advertised.is_array())for(const auto& entry:advertised){const auto id=entry.is_string()?QString::fromStdString(entry.get<std::string>()):string(entry,"reasoningEffort");if(!id.isEmpty()&&!efforts.contains(id))efforts<<id;}
 if(model()=="gpt-6.1-sol"){QStringList ordered;for(const auto* id:{"low","medium","high","xhigh","ultra"})if(efforts.contains(id))ordered<<id;efforts=ordered;}
 efforts_->clear();
 if(efforts.isEmpty())efforts_->addItem(tr("Default"),QString());
 for(const auto& id:efforts){const auto label=id=="low"?tr("Light"):id=="medium"?tr("Medium"):id=="high"?tr("High"):id=="xhigh"?tr("XHigh"):id=="ultra"?tr("Ultra"):id;efforts_->addItem(label,id);}
 const auto light=efforts_->findData("low");if(light>=0)efforts_->setCurrentIndex(light);
 fitChoiceLabels(efforts_);
 tiers_->clear();QStringList descriptions;
 auto tiers=selected.value("serviceTiers",Json::array());
 if(!tiers.is_array()||tiers.empty())tiers=selected.value("additionalSpeedTiers",Json::array());
 if(tiers.is_array())for(const auto& entry:tiers){
  const auto id=entry.is_string()?QString::fromStdString(entry.get<std::string>()):string(entry,"id");if(id.isEmpty())continue;
  const auto label=id.compare("default",Qt::CaseInsensitive)==0||id.compare("standard",Qt::CaseInsensitive)==0?tr("Standard"):id.compare("fast",Qt::CaseInsensitive)==0?tr("Fast"):id;
  tiers_->addItem(label,id);const auto description=string(entry,"description");if(!description.isEmpty()){const auto name=string(entry,"name");descriptions<<tr("%1: %2").arg(name.isEmpty()?id:name,description);}
 }
 auto tierIndex=[this](const QString& id){for(int i=0;i<tiers_->count();++i)if(tiers_->itemData(i).toString().compare(id,Qt::CaseInsensitive)==0)return i;return -1;};
 int preferred=tierIndex("default");if(preferred<0)preferred=tierIndex("standard");
 const auto defaultTier=string(selected,"defaultServiceTier");if(preferred<0&&!defaultTier.isEmpty()&&defaultTier.compare("fast",Qt::CaseInsensitive)!=0)preferred=tierIndex(defaultTier);
 if(preferred<0&&tiers_->count()>0){tiers_->insertItem(0,tr("Standard"),QString());preferred=0;}
 if(preferred>=0)tiers_->setCurrentIndex(preferred);
 if(descriptions.isEmpty()&&tierIndex("fast")>=0)descriptions<<tr("Fast mode can use additional ChatGPT credits.");
 tiers_->setToolTip(descriptions.isEmpty()?tr("Speed / service tier"):descriptions.join('\n'));
 fitChoiceLabels(tiers_);
 setConnected(connected_);
}
void CodexControls::setConnected(bool connected){connected_=connected;models_->setEnabled(connected);efforts_->setEnabled(connected);tiers_->setEnabled(connected&&tiers_->count()>0);}
QString CodexControls::model()const{return models_->currentData().toString();}
QString CodexControls::reasoningEffort()const{return efforts_->currentData().toString();}
QString CodexControls::serviceTier()const{return tiers_->currentData().toString();}
}
