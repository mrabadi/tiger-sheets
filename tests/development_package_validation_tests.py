"""Read-only staged Debian payload validation contracts."""
import importlib.util
import copy
import io
import shutil
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('development_package_validation', ROOT / 'tools/verify_development_package.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='tiger-staged-manifest-')
        self.stage = Path(self.directory.name)
        self.fields = {'Package': 'tiger-sheets', 'Version': '0.3.0~dev.4', 'Architecture': 'amd64',
                       'Description': 'TigerSheets development build\n incomplete compatibility',
                       'Depends': 'qt6-qpa-plugins, qt6-wayland, libc6 (>= 2.34)'}
        for name in ('usr/bin/tiger-sheets', 'usr/libexec/tigersheets/tigersheets-report-worker',
                     'usr/libexec/tigersheets/tigersheets-vba-worker'):
            target = self.stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b'\x7fELFfixture')
            target.chmod(0o755)
        for source, target in [('packaging/tiger-sheets.desktop', 'usr/share/applications/tiger-sheets.desktop'),
                               ('packaging/tiger-sheets-mime.xml', 'usr/share/mime/packages/tiger-sheets-mime.xml')]:
            destination = self.stage / target
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / source, destination)
        shutil.copytree(ROOT / 'packaging/icons/hicolor', self.stage / 'usr/share/icons/hicolor')
        library = self.stage / 'usr/lib/tiger-sheets/libpagmo.so.10'
        library.parent.mkdir(parents=True)
        library.write_bytes(b'\x7fELFlibrary')

    def tearDown(self):
        self.directory.cleanup()

    def test_complete_development_manifest(self):
        self.assertEqual(MODULE.validate_manifest(self.stage, self.fields), [])

    def test_previous_development_version_requires_an_upgrade(self):
        self.fields['Version'] = '0.3.0~dev.3'
        errors = MODULE.validate_manifest(self.stage, self.fields)
        self.assertTrue(any('Version must be 0.3.0~dev.4' in error for error in errors))

    def test_cli_defaults_to_current_development_version(self):
        package = self.stage / 'fixture.deb'
        package.write_bytes(b'fixture')
        with patch('sys.argv', ['verify_development_package.py', str(package)]), \
                patch.object(MODULE, 'verify_package') as verify, patch('sys.stdout', io.StringIO()):
            self.assertEqual(MODULE.main(), 0)
        self.assertEqual(verify.call_args.args[2], '0.3.0~dev.4')

    def test_missing_worker_and_icon_are_actionable(self):
        (self.stage / 'usr/libexec/tigersheets/tigersheets-report-worker').unlink()
        (self.stage / 'usr/share/icons/hicolor/512x512/apps/tiger-sheets.png').unlink()
        errors = MODULE.validate_manifest(self.stage, self.fields)
        self.assertTrue(any('tigersheets-report-worker' in error for error in errors))
        self.assertTrue(any('512x512' in error for error in errors))

    def test_wrong_version_or_clean_install_plugin_dependencies_fail(self):
        self.fields['Version'] = '0.1.0~dev.2'
        self.fields['Depends'] = 'libc6'
        errors = MODULE.validate_manifest(self.stage, self.fields)
        self.assertTrue(any('Version' in error for error in errors))
        self.assertTrue(any('qt6-qpa-plugins' in error for error in errors))
        self.assertTrue(any('qt6-wayland' in error for error in errors))

    def test_loader_origin_paths_resolve_inside_the_staged_bundle(self):
        valid = f'libpagmo.so.9 => {self.stage}/usr/bin/../lib/tiger-sheets/libpagmo.so.9 (0x00000000)'
        MODULE.validate_pagmo_linkage(self.stage, valid)
        with self.assertRaises(MODULE.ValidationError):
            MODULE.validate_pagmo_linkage(self.stage, 'libpagmo.so.9 => /usr/lib/libpagmo.so.9 (0x00000000)')

    def test_report_worker_checks_its_own_relative_solver_loader_path(self):
        worker = MODULE.EXECUTABLES[1]
        linkage = f'libpagmo.so.9 => {self.stage}/usr/lib/tiger-sheets/libpagmo.so.9 (0x00000000)'
        dynamic = '0x (RUNPATH) Library runpath: [$ORIGIN/../../lib/tiger-sheets]'
        MODULE.validate_solver_runtime(self.stage, worker, dynamic, linkage)
        for invalid in ('', dynamic.replace('../../lib', '../lib'),
                        dynamic.replace(']', ':/developer/build/lib]')):
            with self.subTest(dynamic=invalid), self.assertRaises(MODULE.ValidationError):
                MODULE.validate_solver_runtime(self.stage, worker, invalid, linkage)
        with self.assertRaises(MODULE.ValidationError):
            MODULE.validate_solver_runtime(self.stage, worker, dynamic,
                                          'libpagmo.so.9 => /developer/build/lib/libpagmo.so.9 (0x0)')

    def test_packaged_financial_solver_requires_qualified_math_and_constant_changes(self):
        valid = {'status': 'ok', 'kind': 'financial_model',
                 'analysis': {'success': True, 'feasibility': True, 'objective': 40},
                 'operations': [{'type': 'save_financial_definition'},
                                {'type': 'set_cell', 'sheet': 'data', 'row': 2, 'col': 2, 'input': '20.0'}]}
        MODULE.validate_financial_smoke(valid)
        for path, value in [('success', False), ('feasibility', False), ('objective', 39)]:
            invalid = copy.deepcopy(valid)
            invalid['analysis'][path] = value
            with self.subTest(field=path), self.assertRaises(MODULE.ValidationError):
                MODULE.validate_financial_smoke(invalid)
        invalid = copy.deepcopy(valid)
        invalid['operations'][1]['input'] = '=20'
        with self.assertRaises(MODULE.ValidationError):
            MODULE.validate_financial_smoke(invalid)

    def test_payload_paths_cannot_resolve_outside_private_stage(self):
        worker = self.stage / 'usr/libexec/tigersheets/tigersheets-report-worker'
        worker.unlink()
        worker.symlink_to('/bin/true')
        self.assertTrue(any('outside' in error for error in MODULE.validate_manifest(self.stage, self.fields)))

if __name__ == '__main__':
    unittest.main()
