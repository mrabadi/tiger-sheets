#pragma once
#include <QColor>
class QWidget;
namespace tigersheets {
// Invalid QColor means Cancel; editing the picker never mutates a workbook.
QColor chooseWorksheetColor(QWidget* parent,const QColor& current);
}
