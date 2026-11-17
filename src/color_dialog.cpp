#include "tigersheets/color_dialog.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
namespace tigersheets {
namespace {
class ColorSurface final:public QWidget {
public:
 enum Kind{Plane,Luminance,Sample};Kind kind;QColor chosen,current;std::function<void(QPoint)> pick;
 ColorSurface(Kind type,const QColor& color,QWidget* parent):QWidget(parent),kind(type),chosen(color),current(color){setMouseTracking(true);}
protected:
 void paintEvent(QPaintEvent*)override{
  QPainter painter(this);if(kind==Sample){painter.fillRect(QRect(0,0,width(),height()/2),chosen);painter.fillRect(QRect(0,height()/2,width(),height()-height()/2),current);painter.setPen(QColor("#888888"));painter.drawRect(rect().adjusted(0,0,-1,-1));return;}
  const double hue=chosen.hslHueF()<0?0.0:chosen.hslHueF();const double saturation=chosen.hslSaturationF();QImage pixels(size(),QImage::Format_RGB32);
  for(int y=0;y<height();++y)for(int x=0;x<width();++x){const auto color=kind==Plane?QColor::fromHslF(static_cast<double>(x)/std::max(1,width()-1),1.0-static_cast<double>(y)/std::max(1,height()-1),0.5):QColor::fromHslF(hue,saturation,1.0-static_cast<double>(y)/std::max(1,height()-1));pixels.setPixelColor(x,y,color);}
  painter.drawImage(0,0,pixels);painter.setPen(Qt::white);if(kind==Plane){const QPoint point(qRound(hue*(width()-1)),qRound((1.0-saturation)*(height()-1)));painter.drawLine(point-QPoint(4,0),point+QPoint(4,0));painter.drawLine(point-QPoint(0,4),point+QPoint(0,4));}else{const int y=qRound((1.0-chosen.lightnessF())*(height()-1));painter.drawLine(0,y,width(),y);}
 }
 void mousePressEvent(QMouseEvent* event)override{if(event->button()==Qt::LeftButton&&pick)pick(event->position().toPoint());}
 void mouseMoveEvent(QMouseEvent* event)override{if((event->buttons()&Qt::LeftButton)&&pick)pick(event->position().toPoint());}
};
class StandardColors final:public QWidget {
 struct Swatch{QPolygonF shape;QColor color;};std::vector<Swatch> swatches_;
public:
 std::function<void(QColor)> choose;
 explicit StandardColors(QWidget* parent):QWidget(parent){setFixedSize(210,180);setAccessibleName("Standard color honeycomb");constexpr int radius=6;const double cell=8.0;for(int row=-radius;row<=radius;++row)for(int column=std::max(-radius,-row-radius);column<=std::min(radius,-row+radius);++column){const double cx=105+std::sqrt(3.0)*cell*(column+row/2.0),cy=83+1.5*cell*row;QPolygonF hex;for(int corner=0;corner<6;++corner){const double angle=(60*corner-30)*3.14159265358979323846/180;hex<<QPointF(cx+cell*std::cos(angle),cy+cell*std::sin(angle));}const double hue=std::fmod(std::atan2(cy-83,cx-105)/(2*3.14159265358979323846)+1,1);const double distance=std::min(1.0,std::hypot(cx-105,cy-83)/83);swatches_.push_back({hex,QColor::fromHslF(hue,distance,0.5+0.48*(1-distance))});}}
protected:
 void paintEvent(QPaintEvent*)override{QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(Qt::white,0.5));for(const auto& swatch:swatches_){painter.setBrush(swatch.color);painter.drawPolygon(swatch.shape);}}
 void mousePressEvent(QMouseEvent* event)override{if(event->button()!=Qt::LeftButton)return;for(const auto& swatch:swatches_)if(swatch.shape.containsPoint(event->position(),Qt::OddEvenFill)){if(choose)choose(swatch.color);break;}}
};
}
QColor chooseWorksheetColor(QWidget* parent,const QColor& initial){
 QColor chosen=initial.isValid()?initial:QColor(Qt::black);QDialog dialog(parent);dialog.setObjectName("colors.dialog");dialog.setWindowTitle("Colors");dialog.setAccessibleName("Colors");dialog.setFixedSize(326,362);dialog.setStyleSheet("QDialog { background:#f0f0f0; } QLabel,QAbstractSpinBox,QComboBox,QLineEdit,QTabBar,QPushButton { font:11px Calibri; } QLineEdit,QSpinBox,QComboBox { min-height:18px; padding:0px; } QPushButton { min-height:20px; padding:0px 8px; }");
 auto* root=new QHBoxLayout(&dialog);root->setContentsMargins(8,8,8,8);root->setSpacing(10);auto* tabs=new QTabWidget(&dialog);tabs->setObjectName("colors.tabs");root->addWidget(tabs,1);auto* standard=new QWidget(tabs);auto* standardLayout=new QVBoxLayout(standard);standardLayout->setContentsMargins(5,5,5,5);standardLayout->setSpacing(4);standardLayout->addWidget(new QLabel("Colors:",standard));auto* honeycomb=new StandardColors(standard);standardLayout->addWidget(honeycomb,0,Qt::AlignHCenter);auto* grays=new QHBoxLayout;grays->setSpacing(1);for(int index=0;index<13;++index){const int value=qRound(index*255.0/12);const QColor color(value,value,value);auto* swatch=new QToolButton(standard);swatch->setFixedSize(14,18);swatch->setAccessibleName("Standard color "+color.name().toUpper());swatch->setToolTip(color.name().toUpper());swatch->setProperty("color",color);swatch->setStyleSheet("background:"+color.name()+";border:1px solid #aaaaaa;");grays->addWidget(swatch);}standardLayout->addLayout(grays);standardLayout->addStretch();tabs->addTab(standard,"Standard");
 auto* custom=new QWidget(tabs);auto* customLayout=new QVBoxLayout(custom);customLayout->setContentsMargins(5,5,5,5);customLayout->setSpacing(3);customLayout->addWidget(new QLabel("Colors:",custom));auto* selectors=new QHBoxLayout;selectors->setSpacing(9);auto* plane=new ColorSurface(ColorSurface::Plane,chosen,custom);plane->setObjectName("colors.spectrum");plane->setFixedSize(175,140);selectors->addWidget(plane);auto* luminance=new ColorSurface(ColorSurface::Luminance,chosen,custom);luminance->setObjectName("colors.luminance");luminance->setFixedSize(12,140);selectors->addWidget(luminance);selectors->addStretch();customLayout->addLayout(selectors);auto* form=new QFormLayout;form->setContentsMargins(0,2,0,0);form->setVerticalSpacing(2);auto* model=new QComboBox(custom);model->setObjectName("colors.model");model->addItems({"RGB","HSL"});model->setFixedWidth(88);form->addRow("Color model:",model);QSpinBox* components[3];QLabel* labels[3];const char* names[]={"red","green","blue"};for(int index=0;index<3;++index){auto* spin=new QSpinBox(custom);spin->setObjectName("colors."+QString::fromLatin1(names[index]));spin->setAccessibleName(QString::fromLatin1(names[index]));spin->setRange(0,255);spin->setFixedWidth(48);components[index]=spin;labels[index]=new QLabel(custom);form->addRow(labels[index],spin);}auto* hex=new QLineEdit(custom);hex->setObjectName("colors.hex");hex->setAccessibleName("Hex");hex->setFixedWidth(62);hex->setMaxLength(7);form->addRow("Hex:",hex);customLayout->addLayout(form);customLayout->addStretch();tabs->addTab(custom,"Custom");
 auto* side=new QVBoxLayout;side->setContentsMargins(0,0,0,0);side->setSpacing(7);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,Qt::Vertical,&dialog);buttons->setObjectName("colors.buttons");buttons->button(QDialogButtonBox::Ok)->setDefault(true);buttons->setFixedWidth(73);side->addWidget(buttons);side->addStretch();auto* newLabel=new QLabel("New",&dialog);newLabel->setAlignment(Qt::AlignCenter);side->addWidget(newLabel);auto* sample=new ColorSurface(ColorSurface::Sample,chosen,&dialog);sample->setObjectName("colors.preview");sample->setFixedSize(48,48);side->addWidget(sample,0,Qt::AlignHCenter);auto* currentLabel=new QLabel("Current",&dialog);currentLabel->setAlignment(Qt::AlignCenter);side->addWidget(currentLabel);root->addLayout(side);
 std::function<void()> sync=[&]{const bool hsl=model->currentIndex()==1;const int values[]={hsl?qRound(std::max(0.0f,chosen.hslHueF())*255):chosen.red(),hsl?qRound(chosen.hslSaturationF()*255):chosen.green(),hsl?qRound(chosen.lightnessF()*255):chosen.blue()};const char* rgbLabels[]={"Red:","Green:","Blue:"};const char* hslLabels[]={"Hue:","Sat:","Lum:"};for(int index=0;index<3;++index){const QSignalBlocker blocker(components[index]);components[index]->setValue(values[index]);labels[index]->setText(hsl?hslLabels[index]:rgbLabels[index]);}const QSignalBlocker blocker(hex);hex->setText(chosen.name().toUpper());for(auto* surface:{plane,luminance,sample}){surface->chosen=chosen;surface->update();}sample->setProperty("newColor",chosen);sample->setProperty("currentColor",initial);};
 auto select=[&](const QColor& color){chosen=color;sync();};honeycomb->choose=select;for(auto* swatch:standard->findChildren<QToolButton*>())QObject::connect(swatch,&QToolButton::clicked,&dialog,[&,swatch]{select(swatch->property("color").value<QColor>());});for(auto* spin:components)QObject::connect(spin,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{chosen=model->currentIndex()==0?QColor(components[0]->value(),components[1]->value(),components[2]->value()):QColor::fromHslF(components[0]->value()/255.0,components[1]->value()/255.0,components[2]->value()/255.0);sync();});QObject::connect(model,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{sync();});QObject::connect(hex,&QLineEdit::textEdited,&dialog,[&]{if(hex->text().size()==7){const QColor color(hex->text());if(color.isValid())select(color);}});plane->pick=[&](QPoint point){select(QColor::fromHslF(std::clamp(point.x()/174.0,0.0,1.0),1.0-std::clamp(point.y()/139.0,0.0,1.0),chosen.lightnessF()));};luminance->pick=[&](QPoint point){select(QColor::fromHslF(std::max(0.0f,chosen.hslHueF()),chosen.hslSaturationF(),1.0-std::clamp(point.y()/139.0,0.0,1.0)));};QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);sync();return dialog.exec()==QDialog::Accepted?chosen:QColor();
}
}
