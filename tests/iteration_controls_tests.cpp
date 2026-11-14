#include "tigersheets/iteration_controls.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace tigersheets;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){QApplication application(argc,argv);try{
 IterationControlsWidget controls;const Json original={{"iterationEnabled",false},{"maxIterations",200},{"maxChange",0.00001},{"calculationMode","manual"}};controls.setSettings(original);controls.show();
 auto* enabled=controls.findChild<QCheckBox*>("iteration.enabled");auto* passes=controls.findChild<QSpinBox*>("iteration.maxIterations");auto* change=controls.findChild<QLineEdit*>("iteration.maxChange");auto* apply=controls.findChild<QPushButton*>("iteration.apply");auto* example=controls.findChild<QPushButton*>("iteration.example");require(enabled&&passes&&change&&apply&&example,"Iteration controls expose accessible native controls");
 require(controls.settings()==original&&!enabled->isChecked(),"Loading settings retains exact controls and unrelated workbook settings");
 Json requested;int examples=0;controls.applyRequested=[&](const Json& value){requested=value;};controls.circularExampleRequested=[&]{++examples;};
 enabled->setFocus();QTest::keyClick(enabled,Qt::Key_Space);passes->setValue(250);change->setFocus();change->selectAll();QTest::keyClicks(change,"1e-8");apply->setFocus();QTest::keyClick(apply,Qt::Key_Space);
 require(requested.at("iterationEnabled")==true&&requested.at("maxIterations")==250&&requested.at("maxChange")==1e-8&&requested.at("calculationMode")=="manual"&&original.at("iterationEnabled")==false,"Keyboard changes produce a reviewable draft without mutating source settings");
 const auto good=requested;for(const auto* invalid:{"nan","inf","-0.1","not a number"}){change->setText(invalid);requested=Json();QTest::mouseClick(apply,Qt::LeftButton);require(requested.is_null()&&!controls.findChild<QLabel*>("iteration.error")->text().isEmpty(),"Invalid/nonfinite convergence values cannot emit settings proposals");}
 change->setText("0");QTest::mouseClick(apply,Qt::LeftButton);require(requested.at("maxChange")==0,"Exact convergence zero remains a supported engine setting");
 controls.setSettings(good);for(const auto* invalid:{"passes","fraction","change","enabled"}){auto bad=good;if(std::string(invalid)=="passes")bad["maxIterations"]=100001;if(std::string(invalid)=="fraction")bad["maxIterations"]=1.5;if(std::string(invalid)=="change")bad["maxChange"]=-1;if(std::string(invalid)=="enabled")bad["iterationEnabled"]="yes";bool refused=false;try{controls.setSettings(bad);}catch(const std::exception&){refused=true;}require(refused&&controls.settings()==good,"Invalid persisted settings refuse without destroying an existing valid draft");}
 QTest::mouseClick(example,Qt::LeftButton);require(examples==1&&controls.settings()==good,"Circular finance example is an explicit callback without changing iteration settings");
 std::cout<<"iteration controls passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
