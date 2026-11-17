"""Read-only staged Debian payload validation contracts."""
import importlib.util
import copy
import io
import json
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('development_package_validation', ROOT / 'tools/verify_development_package.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

class DocumentationInstallTests(unittest.TestCase):
    def test_install_keeps_user_docs_but_excludes_validation_archives(self):
        with tempfile.TemporaryDirectory(prefix='tiger-docs-install-') as directory:
            source = Path(directory) / 'source'
            build = Path(directory) / 'build'
            stage = Path(directory) / 'stage'
            user_docs = {
                'USER_GUIDE.md': b'Normal user documentation\n',
                'images/example.png': b'fixture-image',
                'validation-notes.md': b'User-facing validation notes\n',
                'reference/validation/input-rules.md': b'Normal input rules\n',
            }
            files = {
                'README.md': b'Package README\n',
                'main.c': b'int main(void) { return 0; }\n',
                'packaging/tiger-sheets.desktop': b'[Desktop Entry]\n',
                'packaging/tiger-sheets-mime.xml': b'<mime-info/>\n',
                'packaging/icons/hicolor/fixture.png': b'icon',
                'compatibility/README.md': b'compatibility',
                'schemas/fixture.json': b'{}',
                'third_party/ironcalc/LICENSE-MIT': b'MIT',
                'third_party/ironcalc/LICENSE-Apache-2.0': b'Apache',
                'third_party/ironcalc/PINNED.md': b'pinned',
                'third_party/SOLVER_DEPS.json': b'{}',
                'third_party/rust-vendor/fixture.txt': b'vendor',
                'docs/validation/README.md': b'Generated evidence, not user docs',
                'docs/validation/nested/result.json': b'{}',
                'docs/validation/nested/archive.zip': b'archive',
                'docs/validation/.hidden/extensionless': b'\x00\x01',
                'docs/validation/empty.png': b'',
                **{'docs/' + name: data for name, data in user_docs.items()},
            }
            for name, data in files.items():
                target = source / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            (source / 'cmake').mkdir()
            for name in ('CPackGate.cmake.in', 'CPackDevelopmentConfig.cmake.in'):
                shutil.copyfile(ROOT / 'cmake' / name, source / 'cmake' / name)
            (source / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.16)\n'
                'project(PackagingDocsFixture VERSION 0.1 LANGUAGES C)\n'
                'set(TIGERSHEETS_SOLVER_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/solver-fixture")\n'
                'foreach(target tiger-sheets tigersheets-vba-worker tigersheets-report-worker)\n'
                '  add_executable(${target} main.c)\n'
                'endforeach()\n'
                f'include("{(ROOT / "cmake/Packaging.cmake").as_posix()}")\n')
            for command in (
                ['cmake', '-S', str(source), '-B', str(build)],
                ['cmake', '--build', str(build)],
                ['cmake', '--install', str(build), '--prefix', str(stage)],
            ):
                completed = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            installed = stage / 'share/doc/tiger-sheets'
            self.assertEqual((installed / 'README.md').read_bytes(), files['README.md'])
            self.assertFalse((installed / 'docs/validation').exists(),
                             'Generated validation archives must not enter installer payloads')
            self.assertEqual(
                {str(path.relative_to(installed / 'docs')): path.read_bytes()
                 for path in (installed / 'docs').rglob('*') if path.is_file()},
                user_docs)

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

    def verify_with_installed_version(self, installed_version):
        """Keep Debian ordering real; isolate extraction/runtime/apt boundaries."""
        package = self.stage / 'fixture.deb'
        package.write_bytes(b'fixture')
        plugins = self.stage / 'qt-plugins'
        for name in ('libqxcb.so', 'libqwayland-egl.so', 'libqoffscreen.so'):
            target = plugins / 'platforms' / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b'plugin')
        original_run = subprocess.run
        extracted = None

        def external_run(argv, **kwargs):
            nonlocal extracted
            stdout = ''
            if argv[0] == 'dpkg':
                return original_run(argv, **kwargs)
            if argv[:2] == ['dpkg-deb', '-f']:
                stdout = '\n'.join(f'{key}: {value}' for key, value in self.fields.items())
            elif argv[:2] == ['dpkg-deb', '-x']:
                extracted = Path(argv[-1])
                shutil.copytree(self.stage, extracted)
            elif argv[0] == 'readelf':
                prefix = '../' if argv[-1].endswith('/bin/tiger-sheets') else '../../'
                stdout = f'0x (RUNPATH) Library runpath: [$ORIGIN/{prefix}lib/tiger-sheets]'
            elif argv[0] == 'ldd':
                stdout = f'libpagmo.so.10 => {extracted}/usr/lib/tiger-sheets/libpagmo.so.10 (0x0)'
            elif argv[0] == 'qmake6':
                stdout = str(plugins)
            elif argv[0] == 'dpkg-query':
                return subprocess.CompletedProcess(argv, 0 if installed_version else 1,
                                                   installed_version or '', '')
            elif argv[0] in ('desktop-file-validate', 'apt-get'):
                pass
            elif argv[-1] == '--version':
                stdout = self.fields['Version']
            elif '--request' in argv:
                request = json.loads(Path(argv[argv.index('--request') + 1]).read_text())
                response = Path(argv[argv.index('--response') + 1])
                result = ({'status': 'ok', 'definition': {'cache': {'records': [{}]}}}
                          if request['kind'] == 'pivot' else
                          {'status': 'ok', 'kind': 'financial_model',
                           'analysis': {'success': True, 'feasibility': True, 'objective': 40},
                           'operations': [{'type': 'set_cell', 'sheet': 'data',
                                           'row': 2, 'col': 2, 'input': '20.0'}]})
                response.write_text(json.dumps(result))
                response.chmod(0o600)
            elif argv[-1] == '--stdio':
                stdout = '{"status":"disabled","version":1}'
            else:
                raise AssertionError(f'Unexpected external command: {argv}')
            return subprocess.CompletedProcess(argv, 0, stdout, '')

        report = {'commands': []}
        with patch.object(MODULE.subprocess, 'run', side_effect=external_run):
            MODULE.verify_package(package, report, self.fields['Version'])
        return report

    def test_same_version_reinstall_is_accepted_and_simulated_read_only(self):
        report = self.verify_with_installed_version(self.fields['Version'])
        self.assertEqual(report['installed_version'], self.fields['Version'])
        simulations = [command['argv'] for command in report['commands']
                       if command['argv'][0] == 'apt-get']
        self.assertEqual(len(simulations), 1)
        self.assertIn('--simulate', simulations[0])
        self.assertIn('--reinstall', simulations[0])
        self.assertIn('Debug::NoLocking=1', simulations[0])
        self.assertNotIn('--allow-downgrades', simulations[0])

    def test_installed_newer_versions_still_reject_candidate_downgrade(self):
        for installed in ('0.3.0~dev.5', '0.3.0', '1:0.3.0~dev.4'):
            with self.subTest(installed=installed), self.assertRaisesRegex(MODULE.ValidationError, 'dpkg failed'):
                self.verify_with_installed_version(installed)

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
