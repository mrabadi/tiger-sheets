#pragma once
class QTableView;
namespace tigersheets {
// Paint source-style headers without replacing Qt's native section controls.
void installWorksheetHeaders(QTableView& view);
}
