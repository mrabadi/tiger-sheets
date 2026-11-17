#pragma once
#include <QRect>
#include <vector>
class QWidget;
namespace tigersheets::testing {
// Explicit adaptation of progressive slide reveals over the original PDF page.
void setPresentationBuilds(QWidget* surface,const std::vector<std::vector<QRect>>& masksByStage);
void advancePresentationBuild(QWidget* surface,bool reverse=false);
void clearPresentationBuilds(QWidget* surface);
}
