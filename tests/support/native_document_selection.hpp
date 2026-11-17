#pragma once
#include <QRect>
class QWidget;
namespace tigersheets::testing {
// External viewer boundary adapter: actual native click and ephemeral image
// selection over original read-only PDF pixels. No image/file/model rewrite.
void selectDocumentImage(QWidget* surface,const QRect& image,const QRect& toolbar);
// Word's selected artwork has a gray image region and no Acrobat image toolbar.
void selectDocumentArtwork(QWidget* surface,const QRect& image);
void clearDocumentSelection(QWidget* surface);
}
