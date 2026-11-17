#include "native_presentation_builds.hpp"
#include <QApplication>
#include <QEvent>
#include <QVariant>
#include <QKeyEvent>
#include <QPainter>
#include <QTest>
#include <QWidget>
#include <stdexcept>
namespace tigersheets::testing {
namespace {
class Builds final:public QWidget {
 QWidget* surface_;std::vector<std::vector<QRect>> stages_;std::size_t stage_=0;
 void publish(){surface_->setProperty("replay.presentationStage",static_cast<int>(stage_));surface_->setProperty("replay.presentationStageCount",static_cast<int>(stages_.size()));update();}
public:
 Builds(QWidget* surface,std::vector<std::vector<QRect>> stages):QWidget(surface),surface_(surface),stages_(std::move(stages)){setObjectName("replay.presentationBuilds");setAccessibleName("Read-only progressive slide reveal");setFocusPolicy(Qt::NoFocus);setAttribute(Qt::WA_ShowWithoutActivating);setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);setAttribute(Qt::WA_TransparentForMouseEvents);setGeometry(surface->rect());surface->installEventFilter(this);publish();show();raise();if(auto* annotation=surface->findChild<QWidget*>("replay.annotationOverlay",Qt::FindDirectChildrenOnly))annotation->raise();}
 ~Builds()override{surface_->setProperty("replay.presentationStage",QVariant());surface_->setProperty("replay.presentationStageCount",QVariant());}
protected:
 bool eventFilter(QObject* object,QEvent* event)override{
  if(object==surface_&&event->type()==QEvent::Resize)setGeometry(surface_->rect());
  if(object==surface_&&event->type()==QEvent::KeyPress){auto* key=static_cast<QKeyEvent*>(event);if(key->modifiers()!=Qt::NoModifier)return false;const bool next=key->key()==Qt::Key_Space||key->key()==Qt::Key_Right||key->key()==Qt::Key_Down||key->key()==Qt::Key_PageDown;const bool previous=key->key()==Qt::Key_Left||key->key()==Qt::Key_Up||key->key()==Qt::Key_PageUp||key->key()==Qt::Key_Backspace;if(next&&stage_+1<stages_.size()){++stage_;publish();return true;}if(previous&&stage_>0){--stage_;publish();return true;}}
  return QWidget::eventFilter(object,event);
 }
 void paintEvent(QPaintEvent*)override{QPainter painter(this);for(const auto& mask:stages_.at(stage_))painter.fillRect(mask,Qt::white);}
};
}
void setPresentationBuilds(QWidget* surface,const std::vector<std::vector<QRect>>& masksByStage){
 if(!surface||masksByStage.empty()||masksByStage.size()>1000)throw std::invalid_argument("Choose a source slide surface and its observed reveal stages");for(const auto& masks:masksByStage){if(masks.size()>1000)throw std::invalid_argument("Too many source slide masks");for(const auto& rect:masks)if(rect.isEmpty())throw std::invalid_argument("Choose nonempty source slide masks");}clearPresentationBuilds(surface);new Builds(surface,masksByStage);QApplication::processEvents();
}
void advancePresentationBuild(QWidget* surface,bool reverse){if(!surface||!surface->findChild<QWidget*>("replay.presentationBuilds",Qt::FindDirectChildrenOnly))throw std::invalid_argument("Open the observed slide build before advancing it");QTest::keyClick(surface,reverse?Qt::Key_Left:Qt::Key_Right);QApplication::processEvents();}
void clearPresentationBuilds(QWidget* surface){if(surface)if(auto* builds=surface->findChild<QWidget*>("replay.presentationBuilds",Qt::FindDirectChildrenOnly)){delete builds;surface->update();}}
}
