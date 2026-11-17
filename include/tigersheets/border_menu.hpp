#pragma once
#include "tigersheets/session.hpp"
#include <functional>
class QMenu;
class QWidget;
namespace tigersheets {
QMenu* createWorksheetBorderMenu(QWidget* parent,const std::function<void(const Json&)>& apply,const std::function<void()>& showMore);
}
