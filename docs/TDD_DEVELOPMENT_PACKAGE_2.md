# Development installer revision 2

The updated application needs an installer that APT recognizes as newer than the installed development revision 1. The development package now uses `0.1.0~dev.2`; the default full-release gate and development compatibility description remain unchanged.

RED: `python3 tests/packaging_tests.py PackagingTests.test_development_deb_is_installable_and_clearly_labeled` built a real minimal Debian archive through the production CPack gate and failed because its control fields still reported `Version: 0.1.0~dev.1` instead of `Version: 0.1.0~dev.2`.

GREEN: changing the development suffix in `cmake/CPackGate.cmake.in` to `~dev.2` made that same test pass. The test checks the actual Debian control version, filename, extracted payload, incomplete development description, and that `dpkg --compare-versions` orders the resulting version after `0.1.0~dev.1`.

VERIFY: `python3 tests/packaging_tests.py` passes all five packaging tests. `python3 tests/release_gate_tests.py` passes all four release-gate tests. The installation instructions name the new installer and describe upgrading from revision 1. This focused packaging check uses a minimal payload; validation of the complete application installer is recorded separately after its build. No system-wide installation or upgrade was performed by this change.
