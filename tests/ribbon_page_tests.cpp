#include "tigersheets/ribbon_page.hpp"
#include "tigersheets/theme.hpp"
#include <QApplication>
#include <QComboBox>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool result,const char* message){if(!result)throw std::runtime_error(message);}
struct Fixture {
 QWidget window;
 QTabWidget* tabs{};
 tigersheets::RibbonPage* page{};
 std::vector<QWidget*> controls;
 QToolButton* last{};
 Fixture(){
  tigersheets::applyAppTheme(&window);
  auto* layout=new QVBoxLayout(&window);layout->setContentsMargins(0,0,0,0);
  tabs=new QTabWidget;tabs->setDocumentMode(true);tabs->setFixedHeight(72);layout->addWidget(tabs);
  page=new tigersheets::RibbonPage;tabs->addTab(page,"Home");
  page->overflowChanged=[this](bool){tabs->setFixedHeight(72+page->overflowHeight());};
  auto group=[&](const QString& title){auto* container=new QWidget;auto* column=new QVBoxLayout(container);column->setContentsMargins(3,0,7,0);column->setSpacing(1);auto* row=new QHBoxLayout;row->setSpacing(3);column->addLayout(row);auto* caption=new QLabel(title);caption->setStyleSheet("font-size:10px;color:#65756d");caption->setAlignment(Qt::AlignHCenter);column->addWidget(caption);page->contentLayout()->addWidget(container);return row;};
  auto add=[&](QHBoxLayout* row,const QString& label){auto* button=new QToolButton;button->setText(label);button->setAutoRaise(true);button->setMinimumHeight(24);row->addWidget(button);controls.push_back(button);last=button;};
  auto* clip=group("Clipboard");for(const auto* text:{"Paste","Copy","Cut","▾"})add(clip,text);
  auto* font=group("Font");auto* family=new QFontComboBox;family->setMaximumWidth(140);font->addWidget(family);controls.push_back(family);auto* size=new QSpinBox;size->setRange(6,96);size->setValue(11);size->setMaximumWidth(60);font->addWidget(size);controls.push_back(size);for(const auto* text:{"B","I","U","A","Fill"})add(font,text);
  auto* alignment=group("Alignment");for(const auto* text:{"Left","Center","Right","Wrap"})add(alignment,text);
  auto* number=group("Number");auto* formats=new QComboBox;formats->addItems({"General","Number","Currency","Accounting","Percent","Date","Custom…"});number->addWidget(formats);controls.push_back(formats);
  auto* editing=group("Editing");for(const auto* text:{"Σ","Fill ↓"})add(editing,text);page->contentLayout()->addStretch();
  window.resize(820,100);window.show();QApplication::processEvents();
 }
};
}
int main(int argc,char** argv){
 QApplication app(argc,argv);
 try{
  Fixture fixture;
  require(fixture.window.width()==820,"ribbon must not force a narrow window wider");
  require(fixture.page->hasOverflow()&&fixture.page->horizontalScrollBar()->isVisible(),"820px Home ribbon must expose native horizontal overflow");
  for(auto* control:fixture.controls)require(control->width()>=std::min(control->sizeHint().width(),control->maximumWidth()),"narrow ribbon must retain each control's natural width");
  require(fixture.page->overflowHeight()>0,"overflow must expose its native extra height for the ribbon host");
  fixture.controls.front()->setFocus();QApplication::processEvents();
  for(int tabs=0;tabs<40&&!fixture.last->hasFocus();++tabs){auto* focus=QApplication::focusWidget();require(focus!=nullptr,"ribbon keyboard focus must remain on a control");QTest::keyClick(focus,Qt::Key_Tab);QApplication::processEvents();}
  require(fixture.last->hasFocus(),"Tab must reach the last ribbon control");
  const auto visible=fixture.page->viewport()->rect();const auto lastRect=QRect(fixture.last->mapTo(fixture.page->viewport(),QPoint{}),fixture.last->size());
  require(visible.contains(lastRect),"keyboard focus must scroll an offscreen ribbon control fully into view");
  std::vector<bool> changes;const auto adjustHeight=fixture.page->overflowChanged;
  fixture.page->overflowChanged=[&](bool overflow){adjustHeight(overflow);changes.push_back(overflow);};
  fixture.window.resize(1280,100);QApplication::processEvents();
  require(!fixture.page->hasOverflow()&&!fixture.page->horizontalScrollBar()->isVisible(),"wide Home ribbon must hide unnecessary overflow");
  require(fixture.tabs->height()==72&&fixture.page->overflowHeight()==0,"wide ribbon must keep its compact 72px height");
  require(!changes.empty()&&!changes.back(),"host must be notified when overflow clears");
  fixture.window.resize(820,100);QApplication::processEvents();
  require(fixture.page->hasOverflow()&&changes.back(),"host must be notified when overflow returns");
  require(fixture.tabs->height()==72+fixture.page->overflowHeight(),"narrow host must reserve only the native scrollbar's height");
  for(auto* control:fixture.controls){const auto rect=QRect(control->mapTo(fixture.page->viewport(),QPoint{}),control->size());require(rect.top()>=0&&rect.bottom()<fixture.page->viewport()->height(),"overflow must not clip the control row vertically");}
  std::cout<<"ribbon page tests passed\n";
 }catch(const std::exception& error){std::cerr<<"ribbon page test failed: "<<error.what()<<'\n';return 1;}
}
