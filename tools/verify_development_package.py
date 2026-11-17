#!/usr/bin/env python3
"""Verify a development .deb in a private stage without installing it."""
import argparse
import configparser
import email.parser
import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

EXECUTABLES = ('usr/bin/tiger-sheets',
               'usr/libexec/tigersheets/tigersheets-report-worker',
               'usr/libexec/tigersheets/tigersheets-vba-worker')
ICON_SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
MIMES = ('application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
         'application/vnd.ms-excel.sheet.macroEnabled.12', 'text/csv')


def validate_manifest(stage, fields, expected_version='0.3.0~dev.4'):
    stage = Path(stage).resolve()
    errors = []
    for key, expected in [('Package', 'tiger-sheets'), ('Version', expected_version), ('Architecture', 'amd64')]:
        if fields.get(key) != expected:
            errors.append(f'{key} must be {expected}, received {fields.get(key)!r}')
    description = fields.get('Description', '').lower()
    if 'development' not in description or 'incomplete' not in description:
        errors.append('Description must identify development status and incomplete compatibility')
    dependencies = {re.split(r'[\s:(]', item.strip())[0] for item in fields.get('Depends', '').split(',')}
    for name in ('qt6-qpa-plugins', 'qt6-wayland'):
        if name not in dependencies:
            errors.append(f'Clean-install dependency missing: {name}')
    for name in EXECUTABLES:
        path = stage / name
        if not path.is_file() or not os.access(path, os.X_OK):
            errors.append(f'Missing executable: {name}')
        elif not path.resolve().is_relative_to(stage):
            errors.append(f'Executable resolves outside private stage: {name}')
    libraries = list((stage / 'usr/lib/tiger-sheets').glob('libpagmo.so*'))
    if not libraries:
        errors.append('Bundled pagmo runtime is missing from usr/lib/tiger-sheets')
    for path in libraries:
        if not path.resolve().is_relative_to(stage):
            errors.append(f'Library resolves outside private stage: {path.relative_to(stage)}')
    desktop = stage / 'usr/share/applications/tiger-sheets.desktop'
    try:
        parser = configparser.ConfigParser(interpolation=None)
        parser.read(desktop)
        entry = parser['Desktop Entry']
        for key, expected in [('Name', 'TigerSheets'), ('Exec', 'tiger-sheets %F'),
                              ('Icon', 'tiger-sheets'), ('StartupWMClass', 'tiger-sheets')]:
            if entry.get(key) != expected:
                errors.append(f'Desktop {key} must be {expected}')
        for mime in MIMES:
            if mime not in entry.get('MimeType', '').split(';'):
                errors.append(f'Desktop document association missing: {mime}')
    except (OSError, KeyError, configparser.Error) as error:
        errors.append(f'Invalid desktop entry: {error}')
    try:
        mime = ET.parse(stage / 'usr/share/mime/packages/tiger-sheets-mime.xml')
        namespace = '{http://www.freedesktop.org/standards/shared-mime-info}'
        if mime.getroot().tag != namespace + 'mime-info':
            raise ValueError('incorrect MIME XML namespace')
        macro = mime.find(f"{namespace}mime-type[@type='{MIMES[1]}']/{namespace}glob[@pattern='*.xlsm']")
        if macro is None:
            raise ValueError('missing *.xlsm macro workbook registration')
    except (OSError, ET.ParseError, ValueError) as error:
        errors.append(f'Invalid MIME registration: {error}')
    for size in ICON_SIZES:
        path = stage / f'usr/share/icons/hicolor/{size}x{size}/apps/tiger-sheets.png'
        try:
            data = path.read_bytes()
            if data[:8] != b'\x89PNG\r\n\x1a\n' or len(data) < 24 or struct.unpack('>II', data[16:24]) != (size, size):
                raise ValueError('incorrect PNG dimensions')
        except (OSError, ValueError) as error:
            errors.append(f'Invalid {size}x{size} launcher icon: {error}')
    return errors


class ValidationError(RuntimeError):
    pass


def validate_pagmo_linkage(stage, linkage):
    bundle = (Path(stage) / 'usr/lib/tiger-sheets').resolve()
    resolved = []
    for line in linkage.splitlines():
        name, separator, target = line.partition('=>')
        if separator and name.strip().startswith('libpagmo.so'):
            resolved.append(Path(target.strip().split()[0]).resolve())
    if not resolved or any(path.parent != bundle for path in resolved):
        raise ValidationError('Staged app resolved pagmo outside its bundled runtime directory')


def validate_solver_runtime(stage, executable, dynamic, linkage):
    expected = ('$ORIGIN/../lib/tiger-sheets' if executable == EXECUTABLES[0]
                else '$ORIGIN/../../lib/tiger-sheets')
    paths = []
    for line in dynamic.splitlines():
        if '(RPATH)' in line or '(RUNPATH)' in line:
            match = re.search(r'\[([^\]]*)\]', line)
            if match:
                paths.extend(match.group(1).split(':'))
    if expected not in paths or any(path.startswith('/') for path in paths):
        raise ValidationError(f'Staged executable has no clean relative bundled solver RPATH: {executable}')
    validate_pagmo_linkage(stage, linkage)


def validate_financial_smoke(response):
    try:
        analysis = response['analysis']
        changes = [operation for operation in response['operations'] if operation.get('type') == 'set_cell']
        if (response.get('status') != 'ok' or response.get('kind') != 'financial_model'
                or analysis.get('success') is not True or analysis.get('feasibility') is not True
                or not math.isclose(float(analysis['objective']), 40, abs_tol=1e-6)
                or len(changes) != 1 or changes[0].get('sheet') != 'data'
                or changes[0].get('row') != 2 or changes[0].get('col') != 2
                or not math.isclose(float(changes[0]['input']), 20, abs_tol=1e-6)):
            raise ValidationError('Staged financial Solver did not return the qualified40 objective and20 input')
    except (KeyError, TypeError, ValueError) as error:
        raise ValidationError(f'Invalid staged financial Solver reply: {error}') from error


def verify_package(package, report, expected_version):
    def run(arguments, *, allowed=(0,), input_text=None, env=None, timeout=60):
        result = subprocess.run([str(arg) for arg in arguments], input=input_text, text=True,
                                capture_output=True, timeout=timeout, env=env)
        report['commands'].append({'argv': [str(arg) for arg in arguments],
                                   'returncode': result.returncode,
                                   'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode not in allowed:
            raise ValidationError(f'{arguments[0]} failed: {(result.stdout + result.stderr)[-3000:]}')
        return result

    report['package'] = str(package)
    report['sha256'] = hashlib.sha256(package.read_bytes()).hexdigest()
    fields = dict(email.parser.Parser().parsestr(run(['dpkg-deb', '-f', package]).stdout).items())
    report['control'] = fields
    with tempfile.TemporaryDirectory(prefix='tigersheets-package-verify-') as temporary:
        private = Path(temporary)
        stage = private / 'stage'
        run(['dpkg-deb', '-x', package, stage])
        errors = validate_manifest(stage, fields, expected_version)
        if errors:
            raise ValidationError('; '.join(errors))
        report['manifest'] = 'passed'
        runtime_env = os.environ.copy()
        runtime_env.pop('LD_LIBRARY_PATH', None)
        runtime_env['LC_ALL'] = 'C'
        runtime_env['QT_QPA_PLATFORM'] = 'offscreen'
        for kind in ('CONFIG', 'CACHE', 'DATA'):
            runtime_env[f'XDG_{kind}_HOME'] = str(private / kind.lower())
        report['binaries'] = {}
        for name in EXECUTABLES:
            executable = stage / name
            report['binaries'][name] = hashlib.sha256(executable.read_bytes()).hexdigest()
            dynamic = run(['readelf', '-d', executable]).stdout
            linkage = run(['ldd', executable], env=runtime_env).stdout
            if 'not found' in linkage:
                raise ValidationError(f'Unresolved staged ELF dependencies: {name}')
            if name in EXECUTABLES[:2]:
                validate_solver_runtime(stage, name, dynamic, linkage)
        desktop = stage / 'usr/share/applications/tiger-sheets.desktop'
        run(['desktop-file-validate', desktop])
        plugins = Path(run(['qmake6', '-query', 'QT_INSTALL_PLUGINS']).stdout.strip()) / 'platforms'
        report['qt_platform_plugins'] = []
        for pattern in ('libqxcb.so', 'libqwayland-*.so', 'libqoffscreen.so'):
            matches = list(plugins.glob(pattern))
            if not matches:
                raise ValidationError(f'Installed clean-install Qt platform plugin is missing: {pattern}')
            for plugin in matches:
                dependencies = run(['ldd', plugin], env=runtime_env).stdout
                if 'not found' in dependencies:
                    raise ValidationError(f'Qt platform plugin has missing dependencies: {plugin}')
                report['qt_platform_plugins'].append(str(plugin))
        version = run([stage / EXECUTABLES[0], '--version'], env=runtime_env).stdout
        if expected_version not in version:
            raise ValidationError('Staged desktop binary reports the wrong application version')
        workbook = {'revision': 0, 'sheets': [
            {'id': 'data', 'name': 'Data', 'cells': [
                {'row': 1, 'col': 1, 'input': 'Region'}, {'row': 1, 'col': 2, 'input': 'Sales'},
                {'row': 2, 'col': 1, 'input': 'East'}, {'row': 2, 'col': 2, 'input': '10'}]},
            {'id': 'report', 'name': 'Report', 'cells': []}]}
        definition = {'id': 'package-smoke', 'name': 'PackageSmoke',
                      'source': {'sheet': 'data', 'row1': 1, 'col1': 1, 'row2': 2, 'col2': 2},
                      'output': {'sheet': 'report', 'row1': 1, 'col1': 1},
                      'rows': [{'field': 'Region'}], 'columns': [], 'filters': [],
                      'values': [{'field': 'Sales', 'aggregate': 'sum', 'name': 'Sales'}]}
        request = private / 'request.json'
        response = private / 'response.json'
        request.write_text(json.dumps({'version': 1, 'kind': 'pivot', 'workbook': workbook,
                                       'retainedLinks': [], 'contextGeneration': 1,
                                       'definition': definition, 'allowCachedLinks': False}))
        request.chmod(0o600)
        run([stage / EXECUTABLES[1], '--request', request, '--response', response], env=runtime_env)
        generated = json.loads(response.read_text())
        if generated.get('status') != 'ok' or len(generated['definition']['cache']['records']) != 1:
            raise ValidationError('Staged report worker did not generate its native reporting result')
        if response.stat().st_mode & 0o077:
            raise ValidationError('Report worker response is not private to its owner')
        financial_book = json.loads(json.dumps(workbook))
        financial_book['sheets'][0]['cells'].append({'row': 2, 'col': 3, 'input': '=B2*2'})
        request.write_text(json.dumps({'version': 1, 'kind': 'financial_model', 'action': 'run',
            'workbook': financial_book, 'retainedLinks': [], 'contextGeneration': 2,
            'definition': {'version': 1, 'id': 'package-solver', 'name': 'Package capacity',
                'kind': 'solver', 'specification': {
                    'objective': {'sheet': 'data', 'row': 2, 'col': 3}, 'goal': 'max', 'method': 'linear',
                    'variables': [{'sheet': 'data', 'row': 2, 'col': 2, 'lower': 0, 'upper': 20}],
                    'constraints': []}}}))
        run([stage / EXECUTABLES[1], '--request', request, '--response', response], env=runtime_env)
        validate_financial_smoke(json.loads(response.read_text()))
        report['staged_financial_solver'] = 'passed (objective40, changing constant20)'
        macro_request = {'version': 1, 'snapshot': workbook, 'source': '',
                         'procedure': 'Smoke', 'enabled': False}
        macro = json.loads(run([stage / EXECUTABLES[2], '--stdio'],
                               input_text=json.dumps(macro_request) + '\n', env=runtime_env).stdout)
        if macro.get('status') != 'disabled' or macro.get('version') != 1:
            raise ValidationError('Staged macro worker failed the disabled-macro protocol')
        installed = run(['dpkg-query', '-W', '-f=${Version}', fields['Package']], allowed=(0, 1))
        report['installed_version'] = installed.stdout.strip() if installed.returncode == 0 else None
        if report['installed_version']:
            run(['dpkg', '--compare-versions', fields['Version'], 'ge', report['installed_version']])
        simulation = run(['apt-get', '--simulate', '--reinstall', '-o', 'Debug::NoLocking=1', 'install', package],
                         env=runtime_env)
        if any(line.startswith('Remv ') for line in simulation.stdout.splitlines()):
            raise ValidationError('Read-only upgrade simulation proposed removing installed packages')
        report['upgrade_simulation'] = 'passed (no package changes performed)'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expect-version', default='0.3.0~dev.4')
    args = parser.parse_args()
    report = {'status': 'failed', 'commands': [], 'limitations': [
        'Private extraction uses this Ubuntu host and its installed runtime libraries; it is not a clean VM install.',
        'The desktop version probe uses Qt offscreen. Native keyboard and accessibility evidence is recorded separately.']}
    try:
        verify_package(args.package.resolve(strict=True), report, args.expect_version)
        report['status'] = 'passed'
    except (OSError, ValueError, subprocess.TimeoutExpired, ValidationError) as error:
        report['error'] = str(error)
    output = json.dumps(report, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output + '\n')
    print(json.dumps({key: report[key] for key in ('status', 'package', 'installed_version', 'error') if key in report}))
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
