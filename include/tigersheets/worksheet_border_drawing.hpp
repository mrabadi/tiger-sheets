#pragma once
#include <QString>
namespace tigersheets {
class WorksheetView;
class WorksheetModel;
bool beginWorksheetBorderDrawing(WorksheetView& view,WorksheetModel& model,const QString& mode,const QString& style,const QString& color);
}
