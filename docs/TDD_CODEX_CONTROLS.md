# Codex controls: OwlDocs parity

Reference: the local OwlDocs `src/app/ChatDock.cpp` and `CodexController.cpp`, and the [official Codex app-server documentation](https://learn.chatgpt.com/docs/app-server), reviewed 2026-11-11. No live account or workbook was sent during tests.

The requested TDD skill was followed in focused steps:

| Behavior | RED observation | GREEN verification |
| --- | --- | --- |
| Always-visible Model / Reasoning / Speed row | `QT_QPA_PLATFORM=offscreen build/codex_workspace_tests` failed: `Codex must expose OwlDocs' Model, Reasoning, and Speed selectors` | Same command passed after adding the compact controls. |
| Discover advertised choices and show suite labels | After repairing a missing test-only JSON include, `cmake --build build --target codex_controls_tests -j2` failed because the new `setCatalog`, `setConnected` and selected-value APIs were absent. | Build and `QT_QPA_PLATFORM=offscreen build/codex_controls_tests` passed. Labels fit the narrow panel; Sol uses Light / Medium / High / XHigh / Ultra when advertised. |
| Model changes replace capabilities | `QT_QPA_PLATFORM=offscreen build/codex_controls_tests model` failed: `changing models replaces its capabilities and resets reasoning and speed to safe defaults` | Same command passed after connecting selection changes to capability refresh. Includes Fast-only legacy catalogs and absent capabilities. |
| Actual controls reach outgoing requests | `QT_QPA_PLATFORM=offscreen build/codex_panel_integration_tests selectors` failed: `codex.input` (the suite controls were not wired into the existing panel). | Same command passed through the actual main window and deterministic fake app-server. Verified selected model, effort and speed, retained conversation, busy states, cancellation and Disconnect. |
| Enter sends; Shift+Enter inserts a newline | `QT_QPA_PLATFORM=offscreen build/codex_panel_integration_tests keyboard` failed: `Enter did not send the Codex message`. | Same command passed after routing Enter to the existing Send action; Shift+Enter retains normal multiline editing. |

Controls retain the OwlDocs grid margins, spacing, labels, accessible names, low/Light preference and Standard speed fallback. Fast is an explicit choice. Busy responses leave next-turn selectors available; sending captures their values. Catalog updates are separate from response status, so completing a message does not reset the choices.

Protocol evidence is recorded separately in `TDD_CODEX_SETTINGS_PROTOCOL.md`. Final optimized, sanitizer and package validation records will be linked here when complete.

Final optimized verification: `ctest --test-dir build-release --output-on-failure`
passed 43/43 targets. Eight focused Codex/preview suites passed ASan/UBSan with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LeakSanitizer remains unavailable
under this environment's ptrace restrictions. Larger schemas exposed a test-only
partial log read; the fake-process reader now waits for a complete NDJSON line.
The final sanitizer run passes after that fixture repair.

The installed local Codex binary's experimental JSON schema was generated offline
and checked for the dynamic-tool fields, model catalog effort/service-tier metadata,
and `serviceTierForTurn: "default"` semantics. No account or inference request was
made. Source hashes, full test logs, screenshots and the financial XLSX fixture
are retained in `validation/codex-workflow/summary.json` and adjacent files.
