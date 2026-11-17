#pragma once
#include <QScrollArea>
#include <functional>

class QHBoxLayout;
class QResizeEvent;
class QStackedWidget;
class QTabWidget;
class QToolButton;
class QWheelEvent;

namespace tigersheets {
void anchorRibbonPopup(QTabWidget&,QStackedWidget&);
// Natural-width ribbon groups inside a horizontally scrollable tab page.
class RibbonPage final : public QScrollArea {
public:
 explicit RibbonPage(QWidget* parent=nullptr);
 QHBoxLayout* contentLayout() const noexcept;
 void compactControls();
 bool hasOverflow() const noexcept;
 int overflowHeight() const;
 QSize sizeHint() const override;
 QSize minimumSizeHint() const override;
 std::function<void(bool)> overflowChanged;
protected:
 void resizeEvent(QResizeEvent* event) override;
 void wheelEvent(QWheelEvent* event) override;
private:
 void updateOverflow();
 QHBoxLayout* contentLayout_{};
 bool overflow_{};
 QToolButton *previous_{},*next_{};
};
}
