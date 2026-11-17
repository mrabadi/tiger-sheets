#pragma once
#include <QRect>
#include <QString>
class QWidget;
namespace tigersheets::testing {
// Read-only external-application chrome adapter. The source only shows, fades
// and dismisses these controls; unused Word/Acrobat commands are not emulated.
QWidget* showDocumentTextToolbar(QWidget* surface,const QRect& bounds,const QString& kind,qreal opacity=1.0);
void clearDocumentTextToolbar(QWidget* surface);
}
