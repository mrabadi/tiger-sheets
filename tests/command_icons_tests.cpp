#include "tigersheets/command_icons.hpp"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QStringList>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition,const QString& message){
 if(!condition)throw std::runtime_error(message.toStdString());
}
void distinctCommands(const QStringList& labels){
 const auto fallback=tigersheets::spreadsheetCommandIcon("unknown command").pixmap(18,18).toImage();
 std::vector<std::pair<QString,QImage>> images;
 QStringList errors;
 for(const auto& label:labels){
  const auto icon=tigersheets::spreadsheetCommandIcon(label);
  require(!icon.isNull(),label+" has an icon");
  const auto image=icon.pixmap(18,18).toImage();
  int ink=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)ink+=qAlpha(image.pixel(x,y))>32;
  require(ink>=20,label+" remains legible at an18pixel toolbar size");
  if(image==fallback){errors<<label+" needs a command-specific graphical icon";continue;}
  for(const auto& [other,pixels]:images)if(image==pixels)errors<<label+" must be distinguishable from "+other;
  images.emplace_back(label,image);
 }
 require(errors.isEmpty(),errors.join('\n'));
}
void formulas(){distinctCommands({"Calculate now","Insert function","Define name","GETPIVOTDATA"});}
void familiar(){distinctCommands({"Copy","Paste","Cut","B","I","U","A","$","%","Left","Center","Right","Wrap","↶","↷"});}
const QStringList bannerLabels={"Insert chart","Remove charts","New sheet","Rename","Print settings","Print","Export PDF","A → Z","Z → A","Data Validation","Forecast","Assumptions","Compare scenarios","Goal Seek","Solver","Scenarios","PivotTable","Convert summary…","Data table","Refresh links","Cell note","Show formulas","Freeze top row","Visual Basic","Record macro","Open","Save","Find","Codex","Accept","Discard","ruler","formulaBar","gridlines","headings","Σ","Fill ↓","Format…","▾"};
void banners(){distinctCommands(bannerLabels);}
void aliases(){
 for(const auto& [alias,label]:std::vector<std::pair<QString,QString>>{{"Σ","AutoSum"},{"Fill ↓","Fill down"},{"ƒx","Insert function"},{"Format…","Format cells"},{"▾","Paste special"},{"gridlines","Gridlines"},{"formulaBar","Formula Bar"},{"headings","Headings"},{"ruler","Ruler"}})
  require(tigersheets::spreadsheetCommandIcon(alias).pixmap(18,18).toImage()==tigersheets::spreadsheetCommandIcon(label).pixmap(18,18).toImage(),alias+" resolves to the semantic "+label+" glyph");
}
void capture(){
 const auto path=qEnvironmentVariable("TIGERSHEETS_COMMAND_ICON_CAPTURE");if(path.isEmpty())return;
 const QStringList labels=QStringList{"Calculate now","Insert function","Define name","GETPIVOTDATA"}+bannerLabels;
 QImage image(900,((labels.size()+5)/6)*72,QImage::Format_ARGB32);image.fill(Qt::white);QPainter painter(&image);painter.setFont(QFont("DejaVu Sans",9));painter.setPen(QColor("#20352d"));
 for(int i=0;i<labels.size();++i){const QPoint at((i%6)*150,(i/6)*72);tigersheets::spreadsheetCommandIcon(labels[i]).paint(&painter,QRect(at+QPoint(60,4),QSize(28,28)));painter.drawText(QRect(at+QPoint(2,36),QSize(146,32)),Qt::AlignHCenter|Qt::TextWordWrap,labels[i]);}
 painter.end();require(image.save(path),"save native icon contact sheet");
}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);QIcon::setThemeSearchPaths({});QIcon::setFallbackSearchPaths({});QIcon::setThemeName("tigersheets-test-no-system-theme");
 try{const QString mode=argc>1?argv[1]:"";require(mode.isEmpty()||QStringList{"formulas","banner","aliases","familiar"}.contains(mode),"unknown icon test mode");if(mode.isEmpty()||mode=="formulas")formulas();if(mode.isEmpty()||mode=="banner")banners();if(mode.isEmpty()||mode=="aliases")aliases();if(mode.isEmpty()||mode=="familiar")familiar();capture();std::cout<<"command icons passed\n";}
 catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
