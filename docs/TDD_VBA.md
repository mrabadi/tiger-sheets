# Native VBA execution evidence and current limits

The requested tdd:tdd workflow is used. No Superpowers workflow is loaded.

The initial native macro test failed because the deliberately new public vba.hpp interface did not exist. The implementation then passed disabled/no-execution, Sub selection, Dim, arithmetic variables, Range.Value and Range.Formula writes and live-workbook isolation.

The next executable test failed with `For and If native control flow`. Native counted For/Next and block If/Then/Else/End If were implemented; the same executable then passed its branches, an unsupported Shell statement rolling back earlier writes, and deterministic cancellation rolling back the complete candidate workbook.

Command: g++ -std=c++20 -Iinclude tests/vba_tests.cpp src/vba.cpp src/session.cpp -o /tmp/tigersheets-vba-tests && /tmp/tigersheets-vba-tests.

Execution is a bounded native interpreter, never shell or JavaScript eval. Statements operate on a private WorkbookSession and success returns one candidate; failure/disabled/cancellation returns the original workbook. Range assignments are bounded at 100,000 cells and macros at 100,000 execution steps.

Current supported subset: parameterless Subs, Dim of simple scalar variables, arithmetic/comparison/concatenation expressions, strings with doubled quotes, selected scalar intrinsic functions, named worksheet Range/Cells assignments, Value/Value2/Formula and basic formatting properties, counted For/Next and block or single-line If. Unsupported syntax is diagnosed and rolls back the candidate.

This is not complete VBA7 or the complete Excel object model. Class modules, events, UserForms, external libraries, Windows COM/ActiveX, structured errors, arrays and general procedure calls are not implemented. Formula-cell reads require a calculation-host adapter; the interpreter diagnoses their absence. Current execution is in-process; an OS worker lifecycle is a further integration gate. Existing vbaProject.bin bytes are preserved by the OOXML layer; native edited VBA project authoring has not yet been established. Private app metadata must not be represented as an Excel-executable VBA project.

Read-only scalar UDF API first failed on the deliberately missing evaluateVbaFunction symbol. Implementation passed Interest(1000, 0.05)=50 and rejected a function attempting a Range mutation. The calculation adapter receives evaluated scalar arguments; explicit Range reads of formula cells still require a host value/dependency adapter. Exact entry-point validation had an executed RED showing `.*` could match a Sub; valid identifier enforcement then passed. Optional cancellation is available on UDF execution too.
