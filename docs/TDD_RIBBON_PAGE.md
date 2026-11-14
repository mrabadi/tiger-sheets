# Native ribbon overflow

The compact Home ribbon previously compressed controls at an 820px window width: font inputs became nearly blank and Fill/Center labels clipped. `RibbonPage` preserves the layout's preferred content width inside a frameless native `QScrollArea`. A horizontal scrollbar appears only when that width exceeds the viewport. Native Qt Tab navigation scrolls the focused control into view.

`contentLayout()` replaces the former page's `QHBoxLayout`. `hasOverflow()`, `overflowHeight()`, and the optional `overflowChanged(bool)` callback let the host reserve the native scrollbar height only on the current overflowing tab. Use a base ribbon height of 72px and add the current page's `overflowHeight()`. Recalculate this when changing tabs; inactive overflowing pages must not enlarge a narrow tab that fits.

## Executed TDD evidence

1. RED: wrote a representative real Home ribbon fixture with native buttons, font combo, font size, alignment, formats, and editing controls. The isolated compile failed because the new public header did not exist: `fatal error: tigersheets/ribbon_page.hpp: No such file or directory`.
2. GREEN: implemented the page and reran the same acceptance case with the new source linked. At 820px the window retained its width, native overflow appeared, and every control retained at least its preferred width bounded by its explicit maximum.
3. Added real Tab traversal to the last initially offscreen control. It passed using Qt's existing scroll-area focus handling; no custom global focus filter was needed. Added resize/callback/height checks and vertical clipping checks.
4. The first wide-window test assumed 1024px was sufficient. Actual native preferred content measured 1093px, with 69px legitimate overflow at 1024px. The earlier non-scroll layout had compressed inputs at that width. Corrected the acceptance fixture's wide window to 1280px, keeping the natural-width assertions unchanged. This was a corrected test assumption, not an implementation RED/GREEN cycle.

The final warning-enabled isolated compile and offscreen executable pass. No shared build directory, production main window, or project CMake file was changed for this helper.

```sh
g++ -std=c++20 -Wall -Wextra -fPIC \
  -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
  -Iinclude -I/usr/include/x86_64-linux-gnu/qt6 \
  -I/usr/include/x86_64-linux-gnu/qt6/QtCore \
  -I/usr/include/x86_64-linux-gnu/qt6/QtGui \
  -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets \
  -I/usr/include/x86_64-linux-gnu/qt6/QtTest \
  tests/ribbon_page_tests.cpp src/ribbon_page.cpp src/theme.cpp \
  -lQt6Test -lQt6Widgets -lQt6Gui -lQt6Core \
  -o /tmp/tiger-ribbon-page-tests
QT_QPA_PLATFORM=offscreen /tmp/tiger-ribbon-page-tests
```

The expected offscreen plugin `propagateSizeHints()` notices are not test failures. The executable checks natural widths at 820px, keyboard visibility, hidden overflow at 1280px, return to 72px height, host notifications in both directions, and no vertically clipped controls after adding native scrollbar space. Integrated main-window rendering and tab-switch integration belong to the host verification.
