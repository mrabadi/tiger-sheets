#pragma once
#include <QColor>
#include <QRect>
class QWidget;
namespace tigersheets::testing {
// Ephemeral presenter marks, drawn through real mouse events over a surface.
QWidget* annotationOverlay(QWidget* surface);
void beginAnnotation(QWidget* surface,const QPoint& point,const QColor& color,int lineWidth);
void moveAnnotation(QWidget* surface,const QPoint& point);
void endAnnotation(QWidget* surface,const QPoint& point);
void annotateRectangle(QWidget* surface,const QRect& rectangle,const QColor& color,int lineWidth);
void clearAnnotations(QWidget* surface);
}
