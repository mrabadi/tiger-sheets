#pragma once
#include <QRect>
#include <QStringList>
class QWidget;
namespace tigersheets::testing {
// Publisher shortcut artwork; separate from native workbook banners and cells.
QWidget* showVideoCaption(QWidget* surface,const QRect& bounds,const QStringList& lines,int fontPixelSize=42);
void clearVideoCaption(QWidget* surface);
}
