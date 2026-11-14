#pragma once
#include "tigersheets/session.hpp"
#include <QWidget>
#include <QFont>
#include <functional>
class QPainter;
namespace tigersheets {
Json chartData(const Json& workbook,const Json& calculation,const Json& specification);
void renderChart(QPainter& painter,const QRectF& bounds,const Json& specification,const Json& data,const QFont& font=QFont());
enum class ChartEditAction { General, Data, Type };
class ChartWidget:public QWidget {
public:
 explicit ChartWidget(QWidget* parent=nullptr);
 void setChart(Json specification,Json data);
 void setSelected(bool selected);
 bool isSelected() const { return selected_; }
 bool isInteracting() const { return dragging_; }
 std::function<void(bool)> selectionChanged;
 std::function<void(ChartEditAction)> editRequested;
 std::function<void()> deleteRequested;
 std::function<void(QRect before,QRect after)> geometryCommitted;
protected:
 bool event(QEvent*) override;
 void paintEvent(QPaintEvent*) override;
 void mousePressEvent(QMouseEvent*) override;
 void mouseDoubleClickEvent(QMouseEvent*) override;
 void contextMenuEvent(QContextMenuEvent*) override;
 void mouseMoveEvent(QMouseEvent*) override;
 void mouseReleaseEvent(QMouseEvent*) override;
 void keyPressEvent(QKeyEvent*) override;
private:
 Json specification_=Json::object(),data_=Json::object();
 int cornerAt(const QPoint&) const;
 void updateGesture(const QPoint& globalPosition);
 bool selected_=false,dragging_=false;
 int resizeCorner_=0;
 QPoint pressGlobal_;
 QRect gestureStart_;
};
}
