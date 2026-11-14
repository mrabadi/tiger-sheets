# Ubuntu Qt platform plugin dependencies

The Debian package must install Qt's dynamically loaded platform plugins as well as its ELF-linked Qt libraries. `CPACK_DEBIAN_PACKAGE_SHLIBDEPS` detects linked libraries but cannot infer plugins loaded by Qt at runtime. On the validated Ubuntu 22.04 host, `libqt6gui6`, `libqt6widgets6`, and `libqt6printsupport6` do not depend on the platform plugin packages.

`cmake/Packaging.cmake` now declares `CPACK_DEBIAN_PACKAGE_DEPENDS` as `qt6-qpa-plugins, qt6-wayland`, retaining automatic shared-library dependency discovery. The former provides the XCB plugin; the latter provides the Wayland platform plugin.

Names were verified with `apt-cache show qt6-qpa-plugins qt6-wayland`, the installed file owner of `plugins/platforms/libqxcb.so`, and the official Ubuntu package records: [Jammy Qt QPA plugins](https://packages.ubuntu.com/jammy/qt6-qpa-plugins) and [Jammy Qt Wayland](https://packages.ubuntu.com/jammy/amd64/qt6-wayland). Local metadata identifies QPA version `6.2.4+dfsg-2ubuntu1.1` and Wayland version `6.2.4-1`; package dependencies are intentionally named without fixing those patch versions so Ubuntu can resolve its compatible updates.

## Executed RED → GREEN

Added `PackagingTests.test_ubuntu_runtime_declares_qt_platform_plugins` before changing the package configuration:

```sh
python3 tests/packaging_tests.py PackagingTests.test_ubuntu_runtime_declares_qt_platform_plugins
```

Observed RED: exit 1, `Debian runtime dependencies must explicitly include Qt platform plugins`. Added the explicit runtime dependency declaration; the same focused command passed. `python3 tests/packaging_tests.py` then passed all three tests, including launcher/MIME identity and fail-closed release gating.

No configure, build, CPack, package install, or system configuration change was run for this correction. Root owns regeneration and staging. This static dependency contract check does not establish clean-install, X11, or Wayland runtime qualification; those acceptance checks remain open, and the full compatibility gate continues to block release packaging.
