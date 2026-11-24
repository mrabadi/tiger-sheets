#pragma once
#include <QItemSelection>
#include <QRegion>

namespace tigersheets {
// Qt can split a single merged-cell selection into adjacent cell ranges.
// Coalesce only a complete rectangle; preserve gaps between separate areas.
inline QItemSelection coalescedWorksheetSelection(const QItemSelection& selected){
 if(selected.size()<2)return selected;const auto* model=selected.front().model();QRegion area;
 for(const auto& range:selected){if(!range.isValid()||range.model()!=model)return selected;area+=QRect(range.left(),range.top(),range.width(),range.height());}
 const auto bounds=area.boundingRect();if(!area.isEmpty()&&area==QRegion(bounds))return QItemSelection(model->index(bounds.top(),bounds.left()),model->index(bounds.bottom(),bounds.right()));return selected;
}
}
