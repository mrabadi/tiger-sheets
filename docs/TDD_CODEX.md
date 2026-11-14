# Codex integration evidence

TigerSheets has an optional native Qt `CodexClient` that launches the installed `codex app-server --listen stdio://` process, uses JSONL bidirectional RPC, and delegates four workbook tools to the application's host adapter. The host owns reads, search and isolated revision-checked previews; this transport never accepts edits or saves files.

The implementation was checked against the [official Codex App Server documentation](https://learn.chatgpt.com/docs/app-server) on 2026-11-11, the installed executable's help, and its locally generated experimental JSON schemas. Experimental dynamic tools require initialization opt-in. Models are discovered from the runtime, including catalog pagination. No model identifier or account entitlement is assumed.

## Public interface

`include/tigersheets/codex.hpp` defines a QtCore-only `CodexClient` without MOC requirements. `connectRuntime`, `send`, `cancel`, `isReady`, `isBusy`, `models`, `model`, `setModel` and `toolDefinitions` expose its lifecycle. `onText`, `onStatus`, `onError` and `onTool` callbacks run on the object's Qt thread. Use one client per active workbook context; create a fresh client when changing workbooks. Reconnecting the same client resumes its in-memory conversation identifier. Identifiers are not persisted to disk.

The published tool catalog is `schemas/workbook.v1.json`. It exactly matches the four registered flat function names: `workbook_v1_capabilities`, `workbook_v1_read`, `workbook_v1_search`, and `workbook_v1_preview`. The host must enforce the schemas and bound results. Server calls are only delegated when the name, namespace, thread, active turn and argument-object shape are valid. Replies preserve the server's original request identifier. Accept and save are absent from the agent interface.

The client uses a private temporary working directory, a small environment allowlist, read-only sandboxing and approval policy `never`. It reads effective configuration before authentication/model discovery and disables every inherited MCP server by name on both thread start and resume. Unrecognized MCP configuration fails closed. Thread configuration disables analytics and OpenTelemetry export. App-server stderr is discarded rather than leaking trace content into chat. The process has no TigerSheets workbook path; user messages carry only a bounded sheet/revision summary. Registered workbook tools access application state through the host adapter. Read-only sandboxing is defense in depth, not an OS-level prohibition on all filesystem reads.

## Observed test-first cycles

The focused build/run command was:

```sh
g++ -std=c++20 -fPIC -Iinclude \
  -I/usr/include/x86_64-linux-gnu/qt6 \
  -I/usr/include/x86_64-linux-gnu/qt6/QtCore \
  tests/codex_tests.cpp src/codex.cpp -lQt6Core \
  -o /tmp/tiger-codex-tests
/tmp/tiger-codex-tests
```

1. **RED:** the handshake test could not compile because the new public `tigersheets/codex.hpp` interface did not exist. **GREEN:** the real QProcess integration completed initialize → initialized → config/read → account/read → model/list with a deterministic local Python protocol peer and selected its advertised default model.
2. **RED:** sending the first prompt failed “sending must begin a turn.” **GREEN:** two messages streamed `Working done` twice on one thread, valid preview calls reached the host once per turn, and foreign namespaces/wrong-thread requests received failed responses without host invocation.
3. **RED:** a two-page model catalog failed “all model catalog pages must be discovered.” **GREEN:** both pages were returned and a default on the second page was selected.
4. **RED:** the catalog publication test failed “workbook.v1 tool schema must ship with TigerSheets.” **GREEN:** the published schema matches the runtime's dynamic tool definitions exactly.

Additional passing regression/characterization checks cover explicit stdio arguments, private cwd, exclusion of a sentinel environment secret, refusal of unknown effective MCP configuration before account access, turn cancellation, and thread resume retaining MCP isolation. The final executable printed `codex tests passed`.

## Verification limits

Tests use a real local subprocess with a deterministic, unauthenticated Python protocol peer. They require no network, credentials or Codex account and do not modify user configuration. Installed-runtime schema/help checks establish protocol shape; authenticated inference, managed account policy and future experimental protocol compatibility have not been tested. Missing runtimes, sign-in requirements, malformed frames, RPC failures and process exits report optional-AI errors without touching workbook state.
