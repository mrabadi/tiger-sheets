#pragma once
#include "tigersheets/references.hpp"
#include <QColor>
#include <QString>
#include <map>
#include <vector>

namespace tigersheets {
// Approximate the blue/red/purple reference hues visible in the compressed
// tutorial. These colors describe editing state, never stored cell styling.
inline QColor formulaReferenceColor(unsigned index){
 static const QColor colors[]={QColor("#3f6db5"),QColor("#b43b56"),QColor("#8064a2"),QColor("#339966"),QColor("#cc6633")};
 return colors[index%5];
}
inline std::vector<unsigned> formulaReferenceColorIndexes(std::string_view formula,const std::vector<FormulaReference>& references){
 std::map<QString,unsigned> assigned;std::vector<unsigned> colors;colors.reserve(references.size());
 for(const auto& reference:references){
  QString qualifier=QString::fromStdString(reference.sheet).toCaseFolded();
  if(reference.external){const auto fragment=QString::fromUtf8(formula.data()+reference.start,reference.length);qualifier=fragment.left(fragment.lastIndexOf('!')).toCaseFolded();}
  const auto key=qualifier+QString("/%1/%2/%3/%4").arg(reference.row1).arg(reference.col1).arg(reference.row2).arg(reference.col2);
  const auto [found,inserted]=assigned.emplace(key,static_cast<unsigned>(assigned.size()));colors.push_back(found->second);
 }
 return colors;
}
}
