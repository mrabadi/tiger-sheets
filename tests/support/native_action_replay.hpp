#pragma once
#include "tigersheets/session.hpp"
#include <QString>
#include <QPointer>
#include <QSize>
#include <QRect>
#include <vector>

namespace tigersheets {
class MainWindow;
namespace testing {

// A UI driver, deliberately separate from WorkbookSession's mutation API.
// Source observations and synthetic acceptance examples remain distinguishable.
class NativeActionReplay {
public:
 NativeActionReplay(MainWindow& window, QString outputDirectory);
 Json run(const Json& manifest);
private:
 QPointer<MainWindow> window_;
 QString outputDirectory_;
 bool pointerPressed_=false;
 QSize replayWindowSize_;
 std::vector<std::pair<QPointer<MainWindow>,QRect>> windowGeometryOverrides_;
 void configureWindow(MainWindow* window);
 Json observe() const;
 Json dispatch(const Json& action);
 Json execute(const Json& step);
 void verify(const Json& expected) const;
};

}
}
