# Preview worksheet-tab isolation regression

The requested tdd:tdd workflow was followed. No Superpowers workflow was used.

The two-sheet model regression creates a private preview with different candidate values on both sheets, switches to the second sheet, attempts an edit and verifies the canonical workbook remains unchanged. It also switches back to the first candidate sheet, then explicitly discards and refreshes to restore live editable cells.

RED: `cmake --build /tmp/tigersheets-vba-build --target grid_tests -j2 && QT_QPA_PLATFORM=offscreen /tmp/tigersheets-vba-build/grid_tests` failed `preview sheet switching retains candidate values and read-only isolation`. The existing setSheetId called refresh, which reset the read-only state and loaded the live session.

The model now owns its preview snapshot. showPreview retains that candidate, and setSheetId reloads the retained candidate without enabling edits. Explicit refresh clears the preview and loads the live session for acceptance/discard transitions. This fix is confined to the model; the main-window preview controls remain their own integration layer.

GREEN: the same focused grid executable passed after the minimal fix (exit 0). Related vba_worker_tests also passed against the actual isolated helper after rebuilding all model consumers. No additional production refactor was needed.

Final related sanitizer verification also passed: rebuilt grid_tests and vba_worker_tests in `/tmp/tigersheets-vba-asan`, then ran both with AddressSanitizer+UBSan (`ASAN_OPTIONS=detect_leaks=0`, offscreen Qt). The GUI tests used the ordinary resource-limited helper executable, including the helper copy beside the sanitized host required by automatic editor resolution. Grid test exited 0; real helper/editor/recorder tests reported `vba worker tests passed`.
