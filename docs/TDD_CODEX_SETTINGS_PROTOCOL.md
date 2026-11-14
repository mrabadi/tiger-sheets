# Codex settings: native client evidence

Date: 2026-11-11. Reference: OwlDocs `src/app/ChatDock.cpp`,
`src/app/CodexController.cpp`, and `src/codex/client.cpp`, read locally without
modification. App-server wire fields were checked against the official
[app-server reference](https://learn.chatgpt.com/docs/app-server).

The deterministic Python fake process supplies the runtime catalog and records
every JSON request. No live account or workbook context was sent to an external
service during these checks. The existing private working directory, environment
allowlist, read-only sandbox, disabled inherited MCP servers, revision-bound tools,
streaming, cancellation, and new-workbook isolation checks remain in the suite.

## Runner

Configured `/tmp/tigersheets-codex-settings` with Ninja, the existing isolated
Cargo path and solver prefix, and `TIGERSHEETS_BUILD_APP=OFF`. Baseline:

```sh
cmake --build /tmp/tigersheets-codex-settings --target codex_tests -j2
/tmp/tigersheets-codex-settings/codex_tests
```

Baseline output: `codex tests passed`.

## Observed RED and GREEN steps

Each row used the runner above before its corresponding production change and
again afterward. These are observed failures, not predicted results.

| Test | Observed RED | GREEN change |
| --- | --- | --- |
| `suiteModelCatalog` | `hidden models must not appear in suite controls` | Filter hidden models, stable-sort runtime defaults, prefer advertised GPT-6.1 Sol. |
| `suiteSettingsSelection` | Compile error identifying missing new public `onModelsChanged`, `reasoningEffort`, `setReasoningEffort`, `serviceTier`, and `setServiceTier` API | Add validated runtime-discovered choices, OwlDocs defaults, model-specific resets, and one completed-catalog callback. |
| `turnSettingsSnapshot` | `thread uses settings captured before callbacks can change the controls` | Snapshot settings before callbacks; send that snapshot on thread creation and every turn. Apply reasoning as `effort`, speed as `serviceTierForTurn`, and explicit `default` for Standard. |
| `disconnectAndResumeSettings` | Compile error identifying missing new public `disconnectRuntime` API | Stop the worker and free busy state; retain only the current workbook's thread identity for restricted resume. |
| Reconnect assertion in `disconnectAndResumeSettings` | `reconnecting restores the same suite defaults as OwlDocs catalog discovery` | Reapply suite defaults on completed discovery, matching OwlDocs. |
| `removedCapabilitiesUseRuntimeDefaults` | `removed runtime capabilities never retain a previously selected Fast or reasoning override` | Clear stale model/effort/tier values before applying a refreshed catalog. |

Every GREEN run printed `codex tests passed`. The selection test also verifies
that changing selectors starts no thread and sends no workbook data, invalid
settings are ignored, and only catalog discovery republishes the model catalog.
The settings snapshot test changes controls from the synchronous Working callback
and verifies the in-flight request retains its original settings. Its next turn
uses the newly selected model, effort, and explicit Standard override. Resume
checks retain the disabled-MCP configuration and exercise cancellation.

## Refactor and verification

Factored process stopping into one helper shared by Disconnect and Reconnect,
then ran:

```sh
cmake --build /tmp/tigersheets-codex-settings --target codex_tests -j2
ctest --test-dir /tmp/tigersheets-codex-settings -R '^codex_tests$' --output-on-failure
```

Result: `1/1 Test #7: codex_tests ... Passed` and `100% tests passed` (0.34 s).
The root task performs the broader native, UI, sanitizer, and packaged-app checks.
Actual authenticated inference and account entitlement behavior are not exercised
by this deterministic protocol test.
