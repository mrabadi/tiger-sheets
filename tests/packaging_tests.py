"""Ubuntu identity and fail-closed release packaging checks."""
import configparser
import re
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class PackagingTests(unittest.TestCase):
    def test_financial_worker_resolves_bundled_solver_libraries(self):
        packaging = (ROOT / 'cmake/Packaging.cmake').read_text()
        self.assertRegex(packaging, r'set_target_properties\(tigersheets-report-worker\s+PROPERTIES\s+INSTALL_RPATH\s+"\$ORIGIN/../../lib/tiger-sheets"\)',
                         'the isolated financial worker must resolve bundled solver libraries from its installed libexec location')

    def test_named_development_config_keeps_the_release_config_separate(self):
        template = ROOT / 'cmake/CPackDevelopmentConfig.cmake.in'
        self.assertTrue(template.is_file(), 'provide a named development-package configuration for local installers')
        with tempfile.TemporaryDirectory(prefix='tiger-development-config-') as directory:
            target = Path(directory)
            (target / 'CPackConfig.cmake').write_text('set(CPACK_TIGERSHEETS_CHANNEL release)\nset(CPACK_PROJECT_CONFIG_FILE release-gate.cmake)\n')
            development = target / 'CPackDevelopmentConfig.cmake'
            development.write_text(template.read_text().replace('@CMAKE_BINARY_DIR@', str(target)))
            probe = target / 'probe.cmake'
            probe.write_text(f'include("{development}")\nfile(WRITE "{target / "selected.txt"}" "${{CPACK_TIGERSHEETS_CHANNEL}};${{CPACK_PROJECT_CONFIG_FILE}}")\n')
            subprocess.run(['cmake', '-P', str(probe)], check=True, capture_output=True, text=True)
            self.assertEqual((target / 'selected.txt').read_text(), 'development;release-gate.cmake')

    def test_development_deb_is_installable_and_clearly_labeled(self):
        with tempfile.TemporaryDirectory(prefix='tiger-development-package-') as directory:
            target = Path(directory)
            payload = target / 'payload'
            payload.mkdir()
            (payload / 'development.txt').write_text('Local development build\n')
            gate = (ROOT / 'cmake/CPackGate.cmake.in').read_text()
            gate = gate.replace('@TIGERSHEETS_SOURCE_DIR@', str(ROOT)).replace('@TIGERSHEETS_RELEASE_REPORT@', '')
            (target / 'gate.cmake').write_text(gate)
            config = target / 'CPackConfig.cmake'
            config.write_text(f'''set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME tiger-sheets)
set(CPACK_PACKAGE_VERSION 0.3.0)
set(CPACK_PACKAGE_FILE_NAME tiger-sheets_0.3.0_amd64)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native spreadsheet")
set(CPACK_PACKAGE_DESCRIPTION "Local spreadsheet application")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "TigerSheets contributors")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
set(CPACK_INSTALLED_DIRECTORIES "{payload};/usr/share/tiger-sheets")
set(CPACK_PROJECT_CONFIG_FILE "{target / 'gate.cmake'}")
set(CPACK_TIGERSHEETS_CHANNEL development)
''')
            result = subprocess.run(['cpack', '--config', str(config), '-B', str(target / 'packages')],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            packages = list((target / 'packages').glob('*.deb'))
            self.assertEqual(len(packages), 1, 'development packaging produces one installer')
            package = packages[0]
            fields = subprocess.check_output(['dpkg-deb', '-f', str(package), 'Package', 'Version', 'Description'], text=True)
            self.assertIn('Package: tiger-sheets', fields)
            version = subprocess.check_output(['dpkg-deb', '-f', str(package), 'Version'], text=True).strip()
            with self.subTest(contract='current development version'):
                self.assertEqual(version, '0.3.0~dev.4')
            with self.subTest(contract='upgrades the previous development package'):
                comparison = subprocess.run(['dpkg', '--compare-versions', version, 'gt', '0.3.0~dev.3'])
                self.assertEqual(comparison.returncode, 0, 'the new installer must upgrade dev.3')
            self.assertIn('development', fields.lower())
            self.assertIn('incomplete', fields.lower())
            description = subprocess.check_output(['dpkg-deb', '-f', str(package), 'Description'], text=True)
            self.assertEqual(sum(line.strip() == 'TigerSheets development build' for line in description.splitlines()),
                             1, 'the installer description must contain its summary only once')
            self.assertEqual(package.name, 'tiger-sheets_0.3.0~dev.4_amd64.deb')
            subprocess.run(['dpkg-deb', '-x', str(package), str(target / 'installed')], check=True)
            self.assertEqual((target / 'installed/usr/share/tiger-sheets/development.txt').read_text(),
                             'Local development build\n')

    def test_native_report_worker_is_installed_and_discoverable(self):
        packaging = (ROOT / 'cmake/Packaging.cmake').read_text()
        self.assertIn('install(TARGETS tigersheets-report-worker RUNTIME DESTINATION libexec/tigersheets)', packaging)
        source = (ROOT / 'src/report_worker.cpp').read_text()
        self.assertIn('/../libexec/tigersheets/tigersheets-report-worker', source)

    def test_ubuntu_runtime_declares_qt_platform_plugins(self):
        packaging = (ROOT / 'cmake/Packaging.cmake').read_text()
        declaration = re.search(r'set\(CPACK_DEBIAN_PACKAGE_DEPENDS\s+"([^"]*)"\s*\)', packaging)
        self.assertIsNotNone(declaration, 'Debian runtime dependencies must explicitly include Qt platform plugins')
        dependencies = {item.strip().split()[0] for item in declaration.group(1).split(',') if item.strip()}
        self.assertTrue({'qt6-qpa-plugins', 'qt6-wayland'} <= dependencies,
                        'Clean Ubuntu installs need both XCB and Wayland Qt platform plugin packages')

    def test_launcher_and_document_associations(self):
        desktop = ROOT / 'packaging/tiger-sheets.desktop'
        self.assertTrue(desktop.is_file(), 'Ubuntu launcher must have its own application identity')
        parser = configparser.ConfigParser(interpolation=None)
        parser.read(desktop)
        entry = parser['Desktop Entry']
        self.assertEqual(entry['Name'], 'TigerSheets')
        self.assertEqual(entry['Exec'], 'tiger-sheets %F')
        self.assertEqual(entry['Icon'], 'tiger-sheets')
        self.assertEqual(entry['StartupWMClass'], 'tiger-sheets')
        self.assertIn('application/vnd.openxmlformats-officedocument.spreadsheetml.sheet;', entry['MimeType'])
        self.assertIn('application/vnd.ms-excel.sheet.macroEnabled.12;', entry['MimeType'])
        self.assertIn('text/csv;', entry['MimeType'])
        tree = ET.parse(ROOT / 'packaging/tiger-sheets-mime.xml')
        self.assertEqual(tree.getroot().tag, '{http://www.freedesktop.org/standards/shared-mime-info}mime-info')

    def test_packaging_refuses_uncovered_compatibility_contract(self):
        gate = ROOT / 'cmake/ReleasePackageGate.cmake'
        self.assertTrue(gate.is_file(), 'release packages require the compatibility gate')
        result = subprocess.run(['cmake', f'-DTIGERSHEETS_SOURCE_DIR={ROOT}', '-P', str(gate)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0, 'an uncovered release must never produce a release package')
        self.assertIn('RELEASE BLOCKED', result.stdout + result.stderr)

if __name__ == '__main__':
    unittest.main()
