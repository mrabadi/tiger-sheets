# Installable development package

The user explicitly requested a package to install. The prior unconditional compatibility gate also blocked local development installers. The new named development configuration produces an installable, honestly labeled prerelease; default full-release packaging remains gated.

1. RED: `python3 tests/packaging_tests.py PackagingTests.test_development_deb_is_installable_and_clearly_labeled` invoked real CPack against a small payload and failed with RELEASE BLOCKED. GREEN: the explicit development channel produces a real Debian archive with package tiger-sheets, version 0.1.0~dev.1 and an incomplete-compatibility description; dpkg-deb extracts the expected payload.
2. RED: the named development configuration was absent. GREEN: its public configuration includes the normal config and selects development while preserving the project gate entry point.
3. RED: dpkg-deb showed the development summary twice. GREEN: the Debian description delegates the summary line to CPack, and the extracted description contains it once.

All five packaging tests and the four release-gate unit tests pass. Raw RED/GREEN outputs are in `validation/development-package`. The actual application package is then checked separately: control fields, dependencies, payload, relative runtime paths, extracted native launch, packaged macro worker and APT dependency simulation. The full Excel release gate remains blocked. No native application behavior changed, so the preceding 37 native test targets and six sanitizer suites remain the runtime validation baseline.
