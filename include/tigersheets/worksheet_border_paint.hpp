#pragma once
#include <QColor>
#include <QPainter>
#include <QRect>
#include <QLineF>
#include <nlohmann/json.hpp>
#include <string_view>

namespace tigersheets {
inline QPen worksheetBorderPen(std::string_view style,const QColor& color){
 const qreal width=style=="thick"?3:style=="medium"||style=="mediumDashed"||style=="mediumDashDot"||style=="mediumDashDotDot"||style=="slantDashDot"?2:style=="hair"?.5:1;
 QPen pen(color,width,Qt::SolidLine,Qt::FlatCap);
 if(style=="dashed"||style=="mediumDashed")pen.setStyle(Qt::DashLine);
 else if(style=="dotted")pen.setStyle(Qt::DotLine);
 else if(style=="dashDot"||style=="mediumDashDot"||style=="slantDashDot")pen.setStyle(Qt::DashDotLine);
 else if(style=="dashDotDot"||style=="mediumDashDotDot")pen.setStyle(Qt::DashDotDotLine);
 return pen;
}
inline qreal worksheetBorderStrength(std::string_view style){
 if(style.empty()||style=="none")return 0;
 return style=="double"?4:worksheetBorderPen(style,Qt::black).widthF();
}
// The first edge is the lower/right cell's top/left edge, so it wins ties.
inline nlohmann::json effectiveWorksheetBorderEdge(const nlohmann::json& preferred,const nlohmann::json& opposite){
 auto strength=[](const nlohmann::json& edge){return edge.is_object()&&edge.contains("style")&&edge.at("style").is_string()?worksheetBorderStrength(edge.at("style").get_ref<const std::string&>()):qreal(0);};
 const auto first=strength(preferred),second=strength(opposite);
 return first>0&&first>=second?preferred:second>0?opposite:nlohmann::json();
}
inline void paintWorksheetBorderLine(QPainter& painter,QLineF line,std::string_view style,const QColor& color){
 if(!worksheetBorderStrength(style)||line.isNull())return;
 painter.save();painter.setRenderHint(QPainter::Antialiasing,style=="hair");painter.setPen(worksheetBorderPen(style,color));
 // Flat caps exclude the terminal pixel. Shared bounds are inclusive.
 const auto direction=line.unitVector();line.setP2(line.p2()+direction.p2()-direction.p1());
 if(style=="double"){auto normal=line.normalVector();normal.setLength(1);const auto offset=normal.p2()-normal.p1();painter.drawLine(line.translated(offset));painter.drawLine(line.translated(-offset));}
 else painter.drawLine(line);
 painter.restore();
}
inline void paintWorksheetBorder(QPainter& painter,const QRect& rect,std::string_view side,std::string_view style,const QColor& color){
 if(style.empty()||style=="none"||rect.isEmpty())return;
 painter.save();painter.setClipRect(rect,Qt::IntersectClip);painter.setRenderHint(QPainter::Antialiasing,style=="hair");const auto pen=worksheetBorderPen(style,color);painter.setPen(pen);
 if(side=="diagonalUp"||side=="diagonalDown"){
  const QLineF line(side=="diagonalUp"?rect.bottomLeft():rect.topLeft(),side=="diagonalUp"?rect.topRight():rect.bottomRight());
  if(style=="double"){auto normal=line.normalVector();normal.setLength(1.5);const auto offset=normal.p2()-normal.p1();painter.drawLine(line.translated(offset));painter.drawLine(line.translated(-offset));}else painter.drawLine(line);
  painter.restore();return;
 }
 auto draw=[&](int inset){
  if(side=="top")painter.drawLine(rect.left(),rect.top()+inset,rect.right(),rect.top()+inset);
  else if(side=="bottom")painter.drawLine(rect.left(),rect.bottom()-inset,rect.right(),rect.bottom()-inset);
  else if(side=="left")painter.drawLine(rect.left()+inset,rect.top(),rect.left()+inset,rect.bottom());
  else if(side=="right")painter.drawLine(rect.right()-inset,rect.top(),rect.right()-inset,rect.bottom());
 };
 if(style=="double"){draw(0);draw(2);}else{const int width=std::max(1,static_cast<int>(pen.widthF()));draw(side=="bottom"||side=="right"?(width-1)/2:width/2);}
 painter.restore();
}
}
