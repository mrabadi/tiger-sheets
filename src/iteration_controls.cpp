#include "tigersheets/iteration_controls.hpp"
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>
#include <stdexcept>
namespace tigersheets {
namespace {
Json normalized(const Json& settings){
 if(!settings.is_object())throw std::invalid_argument("Workbook settings must be an object");
 const auto enabled=settings.value("iterationEnabled",Json(false));const auto iterations=settings.value("maxIterations",Json(100));const auto change=settings.value("maxChange",Json(0.001));
 if(!enabled.is_boolean())throw std::invalid_argument("Iterative calculation must be enabled or disabled");
 if(!iterations.is_number_integer()||iterations.get<std::int64_t>()<1||iterations.get<std::int64_t>()>100000)throw std::invalid_argument("Iteration limit must be an integer between1 and100000");
 if(!change.is_number()||!std::isfinite(change.get<double>())||change.get<double>()<0)throw std::invalid_argument("Maximum change must be a finite nonnegative number");
 auto result=settings;result["iterationEnabled"]=enabled;result["maxIterations"]=iterations;result["maxChange"]=change;return result;
}
}
struct IterationControlsWidget::State {
 Json base=Json::object();QCheckBox* enabled{};QSpinBox* iterations{};QLineEdit* change{};QLabel* error{};
};
IterationControlsWidget::IterationControlsWidget(QWidget* parent):QWidget(parent),state_(std::make_unique<State>()){
 setObjectName("iteration.controls");setAccessibleName("Circular calculation settings");auto& s=*state_;auto* layout=new QVBoxLayout(this);
 auto* description=new QLabel("Circular models can calculate repeatedly until every formula changes by no more than the convergence threshold. A result that reaches the iteration limit remains unaccepted.",this);description->setWordWrap(true);layout->addWidget(description);
 s.enabled=new QCheckBox("Enable iterative calculation",this);s.enabled->setObjectName("iteration.enabled");s.enabled->setAccessibleName("Enable iterative calculation");layout->addWidget(s.enabled);
 auto* form=new QFormLayout;layout->addLayout(form);s.iterations=new QSpinBox(this);s.iterations->setObjectName("iteration.maxIterations");s.iterations->setAccessibleName("Maximum iterations");s.iterations->setRange(1,100000);form->addRow("Maximum &iterations",s.iterations);
 s.change=new QLineEdit(this);s.change->setObjectName("iteration.maxChange");s.change->setAccessibleName("Maximum change for convergence");s.change->setPlaceholderText("0.001");form->addRow("Maximum &change",s.change);
 s.error=new QLabel(this);s.error->setObjectName("iteration.error");s.error->setAccessibleName("Iteration settings validation");s.error->setWordWrap(true);layout->addWidget(s.error);
 auto* apply=new QPushButton("Review iteration settings",this);apply->setObjectName("iteration.apply");apply->setAccessibleName("Review iteration settings");layout->addWidget(apply);
 auto* example=new QPushButton("Circular financing example",this);example->setObjectName("iteration.example");example->setAccessibleName("Create a circular financing example draft");layout->addWidget(example);
 connect(s.enabled,&QCheckBox::toggled,this,[this](bool enabled){state_->iterations->setEnabled(enabled);state_->change->setEnabled(enabled);});
 connect(apply,&QPushButton::clicked,this,[this]{try{const auto draft=settings();state_->error->clear();if(applyRequested)applyRequested(draft);}catch(const std::exception& error){state_->error->setText(QString::fromUtf8(error.what()));}});
 connect(example,&QPushButton::clicked,this,[this]{if(circularExampleRequested)circularExampleRequested();});
 setSettings(Json::object());
}
IterationControlsWidget::~IterationControlsWidget()=default;
void IterationControlsWidget::setSettings(const Json& settings){
 const auto parsed=normalized(settings);auto& s=*state_;const QSignalBlocker enabled(s.enabled),iterations(s.iterations),change(s.change);s.base=parsed;s.enabled->setChecked(parsed.at("iterationEnabled"));s.iterations->setValue(parsed.at("maxIterations"));s.change->setText(QString::number(parsed.at("maxChange").get<double>(),'g',17));s.iterations->setEnabled(s.enabled->isChecked());s.change->setEnabled(s.enabled->isChecked());s.error->clear();
}
Json IterationControlsWidget::settings()const{
 const auto& s=*state_;bool valid=false;const auto change=s.change->text().trimmed().toDouble(&valid);if(!valid||!std::isfinite(change)||change<0)throw std::invalid_argument("Maximum change must be a finite nonnegative number (for example,0.001 or1e-8)");auto result=s.base;result["iterationEnabled"]=s.enabled->isChecked();result["maxIterations"]=s.iterations->value();result["maxChange"]=change;return normalized(result);
}
}
