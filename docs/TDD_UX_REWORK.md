# Native workspace UX revision

The reported defects were exercised in the actual MainWindow, using Qt Test on Ubuntu, before implementation. Tests use offscreen Qt Widgets; this does not qualify X11 or Wayland interaction.

## Root RED / GREEN sequence

Each row was developed as a focused failing behavior followed by its minimal implementation. Raw command output is retained under `docs/validation/ux-rework/`.

| Behavior | Observed failure | Implementation / verification |
|---|---|---|
| File beside suite identity | File geometry was on a separate row | Left menu corner identity and right document actions, same-row geometry test |
| Compact chrome | Worksheet began too far down | 72 px ribbon, compact controls, 155 px maximum worksheet origin |
| Live cell entry | Formula bar did not follow cell typing | Shared worksheet delegate mirrors the draft without canonical edits |
| Escape | Cell draft left stale formula-bar text | Cancel restores the original input and leaves workbook revision unchanged |
| Enter / Tab | Formula-bar Enter did not move down | One acceptance path, Enter down / Tab right, Shift reverses |
| Point references | Clicking cells left the formula instead of inserting a reference | Mouse press/drag/release inserts a relative A1 range, preserving the origin cell |
| Reference outlines | Draft references had no colored worksheet outlines | Parsed reference spans painted by WorksheetView; independent parser tests cover literals, bounds, sheets and external references |
| F4 | setText cleared the modified flag and lost the draft on focus change | QLineEdit insert preserves dirty state and local undo; caret converted to UTF-8 for the reference parser |
| Narrow windows | Home controls shrank below their readable widths | RibbonPage preserves natural widths with native horizontal overflow |
| Bar completion | =AVG had no suggestions | Available canonical AVERAGE suggestions; Tab completes without accepting the cell; accepted AVERAGE(10,20) evaluates to 15 |
| In-cell completion | Tab committed incomplete =SUM | FormulaLineEdit delegate handles completion before cell navigation |
| Workbook context | Defined-name and enabled UDF suggestions were absent | Revision-sourced completion context shared by bar and new cell editors |
| Chart editing | Double click did nothing | Native chart dialog updates the existing chart in one undo transaction |
| Chart deletion | Delete cleared worksheet cells | Chart owns Delete while selected; removes only its chart metadata |
| Chart movement | Dragging did not update workbook metadata | Exact cell anchor and pixel offsets stored transactionally, XLSX reopen verified |
| Chart recalculation | Every calculation destroyed selection / active widget | Reconcile native charts by identity and retain active gestures |
| Chart anchors | Column resize detached the chart visually | Reposition charts from cached anchors on header geometry changes |
| Workbook switch | Previous charts stayed interactive until asynchronous calculation | Clear prior chart widgets and discard entry state during context reset |
| Chart selection | Multiple figures retained handles and cell clicks left stale handles | Exclusive chart selection and explicit handoff back to worksheet cells |
| Read-only F4 | Programmatic insert modified a preview and crashed on focus loss | Ignore F4 on disabled or read-only editors |
| Read-only function insertion | A writable function dialog opened during preview | Reject insertion before opening the dialog |
| Formula fx control | Static label did not open Insert Function | Accessible native button creates a modified, uncommitted function draft |

The initial live-entry test incorrectly sent all subsequent characters to the grid instead of the newly created editor. It was corrected before establishing RED by temporarily disabling the draft observer, then restoring the minimal implementation. Initial standalone test compile/link failures were setup errors, not RED evidence. A context build made while a temporary JSON iteration was being corrected crashed; a fresh build with the named JSON object retained throughout iteration passed. The stale passing context-red log is a setup artifact and is explicitly excluded from RED evidence in TDD_FORMULA_WORKSPACE.md.

## Related focused evidence

- `TDD_WORKSHEET_ENTRY.md`: dirty editor preservation, colored outlines, keyboard point mode, active-view teardown, current-cell border and large-reference bounds.
- `TDD_FORMULA_WORKSPACE.md`: actual MainWindow completion, names/UDFs, mouse/keyboard popup routing and readonly-safe cancellation evidence.
- `TDD_FORMULA_EDITOR.md`: native completion, exact signatures, nested arguments, quoting, popup routing and typed workbook suggestions.
- `TDD_FORMULA_REFERENCES.md`: parsed references and coherent F4 range cycling.
- `TDD_CHART_EDITING.md`: chart selection, gestures, context commands and isolated editing dialog.
- `TDD_CHART_ANCHORS.md`: DrawingML offset preservation and independent package verification.
- `TDD_COMPACT_THEME.md`, `TDD_RIBBON_PAGE.md`: suite density and readable overflow.

## Qualification boundary

These changes improve the reported native interactions. They do not certify the frozen Excel function/shortcut catalogs, complete VBA, all native Excel chart families, or the full release contract. Packaging remains subject to the existing release gate. Actual Microsoft Excel execution and reopen behavior remain unverified.


## Final integrated validation

The final optimized CTest run passes 37/37 targets. Six focused native C++ suites pass with address and undefined-behavior sanitizers, including all eight worksheet entry cases and the 19 formula editor scenarios. Leak checking is disabled in that run; it does not qualify leaks, all third-party code or all release behavior. `validation/ux-rework/summary.json` and its XML/log records identify the exact binaries and sources.

The full-app sanitizer run first exposed a real sibling-destruction case omitted by the standalone fixture: the formula bar's QWidget base destructor delivered an event while a typed QPointer comparison assumed it was still a QLineEdit. A focused bar-first deletion test reproduced the downcast error. Inspecting the event object's live Qt type before comparing the typed pointer fixed it. A separate calculation-before-first-keystroke regression exposed caret/selection resets; identical source text now avoids a delegate reset. Both regressions pass normally and under sanitizers.

The first integrated development run also caught an old branding test assumption that no resource prefix other than icons could exist. The test now selects the dedicated icon resource and retains every original launcher-size/path/content assertion, allowing the new embedded function catalog. The current-cell-border failure in that run came from an object compiled before the final agent edit; a fresh build passes. The failed initial run is retained as `ctest-development-initial.log`.

The temporary-prefix install validates the desktop entry, generates a temporary MIME cache, resolves its libraries and launches the actual installed demo. It does not install into the user's desktop or qualify clean-machine installation/upgrade. The native 820 px render was allowed to settle before capture; initial CLI geometry overrides can briefly precede native scrollbar visibility. The actual settled MainWindow exposes its native ribbon scrollbar and keyboard access to the last control.

The independent financial fixture passes 13/13 openpyxl/Decimal/Poppler checks, retaining the existing limits: no Microsoft Excel run, and the PDF still omits chart graphics. The release compatibility gate remains blocked; this UX revision is not the requested full Excel/VBA release.
