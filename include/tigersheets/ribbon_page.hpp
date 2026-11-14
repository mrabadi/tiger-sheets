#pragma once
#include <QScrollArea>
#include <functional>

class QHBoxLayout;
class QResizeEvent;

namespace tigersheets {
// Natural-width ribbon groups inside a horizontally scrollable tab page.
class RibbonPage final : public QScrollArea {
public:
 explicit RibbonPage(QWidget* parent=nullptr);
 QHBoxLayout* contentLayout() const noexcept;
 bool hasOverflow() const noexcept;
 int overflowHeight() const;
 QSize sizeHint() const override;
 QSize minimumSizeHint() const override;
 std::function<void(bool)> overflowChanged;
protected:
 void resizeEvent(QResizeEvent* event) override;
private:
 void updateOverflow();
 QHBoxLayout* contentLayout_{};
 bool overflow_{};
};
}
