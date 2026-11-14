# Compact OwlDocs suite styling

The reference was inspected in `/workspace/owldocs/src/app/AppTheme.cpp`, `MainWindow.cpp` around line 3180, and `RibbonWidget.cpp`. OwlDocs places the identity in the menu bar's top-left corner and document actions in its top-right corner. Its identity uses a 28px logo, 7px gaps and small margins; its writing bar uses 8px horizontal/4px vertical margins and 4px gaps.

TigerSheets keeps the same Fusion style, Inter/Noto Sans/DejaVu Sans fallback at 10pt, forest accent `#245648`, workspace `#f4f5f3`, chrome `#fcfcfa`, borders `#dce2da`, dark text `#202c33`, and muted text `#65756d`. This change tightens control padding and corners rather than reducing the normal suite font.

Controls now use 2px corners; formula/name fields have square borders. Auto-raise toolbar buttons have transparent normal backgrounds with flat hover/checked states. Tabs use 4px vertical/10px horizontal padding, flat backgrounds, and the forest selection underline. Input controls fit approximately 24px rows, and dock/status chrome has smaller padding. The header and identity corner widgets have no separate border, leaving one menu-row separator. Existing focus/hover colors and contrast are retained; focus does not resize inputs.

The named `workspace.menubar` has 8px vertical item padding so File aligns with the 28px identity corner in the same 36px row. Generic menu bars remain compact. This scope is necessary because Qt otherwise top-aligns a 24px action within the taller corner-widget row.

MainWindow layout changes are owned separately: one 36px identity/menu/action row, a roughly 72px ribbon, 26px toolbar controls, and a 28px formula row. Ribbon page margins should stay at 2–3px vertically to accommodate its controls and caption without clipping. The theme does not impose a tall ribbon or replace the application's widget hierarchy.

## Executed geometry regression evidence

Tests create actual native Qt buttons, line edits, combo/spin controls, menus, tabs, and a menu bar with the real identity-corner geometry. They assert widget measurements and rendered focus behavior rather than matching stylesheet strings.

1. With the original theme, the first RED was `suite toolbar buttons must fit a compact 28px row`. Measured controls were tool 33px, push button 30px, inputs 29–30px, tabs 33px, and menu 43px. Compact padding passed this geometry check.
2. Tightening the shared toolbar-row contract to 26px produced a second RED with a remaining 27px auto-raise tool button. Reducing its vertical padding passed the 26px-row contract without changing the readable suite font.
3. The identity-corner fixture then produced RED: `File menu must share the identity row's visual baseline`. The scoped menu padding fixed it while retaining the same four-pixel center-alignment tolerance and the generic compact-menu test.

Final native size hints on the validated offscreen Qt host:

| Control | Original | Compact |
| --- | ---: | ---: |
| Auto-raise tool button | 33px | 25px |
| Push button | 30px | 24px |
| Line edit | 29px | 24px |
| Combo box | 30px | 24px |
| Integer/double spin box | 29px | 24px |
| Ribbon tab | 33px | 25px |
| Generic menu bar | 43px | 24px |

Additional passing checks retain a readable font and at least 4.5:1 normal/selection/button palette contrast. Actual focused formula-field border pixels have at least 3:1 contrast against the field and visibly differ from the unfocused border; focusing leaves the size hint unchanged.

## Isolated verification and rendering

No shared CMake build was run. The test and actual theme source were compiled directly with `-Wall -Wextra -Wpedantic`:

```sh
g++ -std=c++20 -fPIC -Wall -Wextra -Wpedantic -Iinclude \
  -I/usr/include/x86_64-linux-gnu/qt6 \
  -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets \
  -I/usr/include/x86_64-linux-gnu/qt6/QtGui \
  -I/usr/include/x86_64-linux-gnu/qt6/QtCore \
  tests/theme_tests.cpp src/theme.cpp \
  -lQt6Widgets -lQt6Gui -lQt6Core -o /workspace/build/tiger-theme-tests
QT_QPA_PLATFORM=offscreen /workspace/build/tiger-theme-tests
```

The focused checks pass. Before/after renders were also inspected locally during the original review; those historical images are not included in the repository or required to run the checks. The fixture deliberately displays both the suite menu row and a generic menu row to verify their separate sizing; it is not a mockup of the final application layout. Integrated MainWindow rendering and its layout regression are owned by the root agent. No MainWindow, grid, or CMake files were edited by this theme task.
