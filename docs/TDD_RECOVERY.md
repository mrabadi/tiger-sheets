# Recovery integrity

Recovery schema v2 stores a SHA-256 digest of each checkpoint's exact serialized snapshot, original document bytes, and original path. Each field is prefixed with its unsigned 64-bit, big-endian byte length before hashing. This detects damaged fields without ambiguity between field boundaries. Reading a damaged checkpoint throws an error identifying corruption and the checkpoint ID; records are never silently filtered.

Existing schema v1 databases migrate in one SQLite transaction. Migration validates snapshot JSON and hashes the existing bytes without reserializing the snapshot or document. A failed migration rolls back its column, digests, and version update, retaining all legacy records for diagnosis. Unsupported future schema versions are refused without downgrading them. Legacy unversioned databases with the original four-column schema also migrate. Fresh stores start at v2. Checkpoints and replacements remain single SQLite statements with WAL journaling and FULL synchronous mode.

The public `RecoveryStore` API is unchanged. Binary document data, empty data, and embedded zero bytes in paths are retained through explicit-length SQLite bindings. Qt Core supplies `QCryptographicHash`; no external hash process runs.

## Executed TDD evidence

1. Re-executed the existing source-corruption regression against the old implementation: `/tmp/tiger-recovery-corrupt-red-owned` exited 1 with `FAILED: corrupt recovery source must be diagnosed`.
2. Added the migration test before changing production code. `/tmp/tiger-recovery-tests migration` exited 1 with `Legacy recovery schema must migrate to v2 integrity records`.
3. Implemented the digest, transactional migration, and verified reads. The migration and complete suite passed.
4. Added boundary and failure-path coverage: identical unframed concatenations with different source/path boundaries, embedded path zero bytes, invalid legacy JSON rolling back the complete migration, future-version preservation, and exact binary source/snapshot preservation. These checks also passed.

Verification:

```sh
g++ -std=c++20 -fPIC -Wall -Wextra -Wpedantic -Iinclude \
  -I/usr/include/x86_64-linux-gnu/qt6 \
  -I/usr/include/x86_64-linux-gnu/qt6/QtCore \
  tests/recovery_tests.cpp src/recovery.cpp -lsqlite3 -lQt6Core \
  -o /tmp/tiger-recovery-tests
/tmp/tiger-recovery-tests
```

The warning-enabled build and all six test groups passed. Hashes detect accidental corruption; they do not authenticate a database against someone who can also replace its digest. Migration establishes an integrity baseline for the legacy bytes present at migration time and cannot diagnose historical alteration of otherwise valid legacy data.
