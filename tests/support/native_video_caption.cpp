#include "native_video_caption.hpp"
#include <QPainter>
#include <QVariant>
#include <QWidget>
#include <stdexcept>
namespace tigersheets::testing {
namespace {
class Caption final:public QWidget {
 QStringList lines_;int size_;
public:
 Caption(QWidget*surface,const QRect&bounds,const QStringList&lines,int size):QWidget(surface,Qt::ToolTip|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus|Qt::WindowTransparentForInput),lines_(lines),size_(size){
  setObjectName("replay.videoCaption");setAccessibleName("Video shortcut caption");setFocusPolicy(Qt::NoFocus);
  setAttribute(Qt::WA_ShowWithoutActivating);setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);setAttribute(Qt::WA_TransparentForMouseEvents);
  setProperty("caption.lines",lines);setProperty("caption.fontPixelSize",size);setGeometry(QRect(surface->mapToGlobal(bounds.topLeft()),bounds.size()));show();raise();
 }
protected:
 void paintEvent(QPaintEvent*)override{
  QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(Qt::NoPen);painter.setBrush(QColor(0,0,0,192));painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),8,8);
  QFont font("Arial");font.setPixelSize(size_);painter.setFont(font);painter.setPen(Qt::white);const int height=(this->height()-16)/lines_.size();
  for(int i=0;i<lines_.size();++i)painter.drawText(QRect(8,8+i*height,width()-16,height),Qt::AlignLeft|Qt::AlignVCenter,lines_[i]);
 }
};
}
QWidget* showVideoCaption(QWidget*surface,const QRect&bounds,const QStringList&lines,int fontPixelSize){
 if(!surface||bounds.isEmpty()||lines.isEmpty()||fontPixelSize<8||fontPixelSize>96)throw std::invalid_argument("Choose a caption surface, bounds, text and valid source font size");
 clearVideoCaption(surface);return new Caption(surface,bounds,lines,fontPixelSize);
}
void clearVideoCaption(QWidget*surface){if(surface)if(auto*caption=surface->findChild<QWidget*>("replay.videoCaption",Qt::FindDirectChildrenOnly)){delete caption;surface->update();}}
}
