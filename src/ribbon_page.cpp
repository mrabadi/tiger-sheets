#include "tigersheets/ribbon_page.hpp"
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QScrollBar>

namespace tigersheets {
RibbonPage::RibbonPage(QWidget* parent):QScrollArea(parent){
 setFrameShape(QFrame::NoFrame);
 setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
 setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
 setWidgetResizable(true);
 setAlignment(Qt::AlignLeft|Qt::AlignTop);
 setFocusPolicy(Qt::NoFocus);
 auto* contents=new QWidget;
 contentLayout_=new QHBoxLayout(contents);
 contentLayout_->setContentsMargins(8,3,8,3);
 contentLayout_->setSpacing(5);
 // A group's minimum size may permit labels and combo boxes to collapse. Its
 // preferred size is the usable ribbon width; scroll instead of compressing it.
 contentLayout_->setSizeConstraint(QLayout::SetFixedSize);
 setWidget(contents);
 connect(horizontalScrollBar(),&QScrollBar::rangeChanged,this,[this]{updateOverflow();});
}
QHBoxLayout* RibbonPage::contentLayout() const noexcept{return contentLayout_;}
bool RibbonPage::hasOverflow() const noexcept{return overflow_;}
int RibbonPage::overflowHeight() const{return overflow_?horizontalScrollBar()->sizeHint().height():0;}
QSize RibbonPage::sizeHint() const{return {0,widget()->sizeHint().height()+overflowHeight()};}
QSize RibbonPage::minimumSizeHint() const{return {0,0};}
void RibbonPage::resizeEvent(QResizeEvent* event){QScrollArea::resizeEvent(event);updateOverflow();}
void RibbonPage::updateOverflow(){const bool next=horizontalScrollBar()->maximum()>horizontalScrollBar()->minimum();if(next==overflow_)return;overflow_=next;if(overflowChanged)overflowChanged(overflow_);}
}
