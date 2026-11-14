"""Run the real reporting pipeline and sample its complete process-tree RSS."""
import argparse
import json
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import time


def rss_tree(pid):
    pending, seen, total = [pid], set(), 0
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        try:
            status = Path(f'/proc/{current}/status').read_text()
            total += next((int(line.split()[1]) for line in status.splitlines()
                           if line.startswith('VmRSS:')), 0)
            children = Path(f'/proc/{current}/task/{current}/children').read_text()
            pending.extend(map(int, children.split()))
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            pass
    return total


def run_sampled(command, log):
    maximum, started, last_progress = 0, time.monotonic(), 0
    with Path(log).open('w') as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
        while process.poll() is None:
            maximum = max(maximum, rss_tree(process.pid))
            elapsed = time.monotonic() - started
            if elapsed - last_progress >= 30:
                print(json.dumps({'running': Path(command[0]).name,
                                  'elapsedSeconds': round(elapsed, 1),
                                  'sampledProcessTreePeakRssKiB': maximum}), flush=True)
                last_progress = elapsed
            time.sleep(.025)
    if process.returncode:
        raise RuntimeError(f'{command[0]} exited {process.returncode}; inspect {log}')
    return {'wallSeconds': time.monotonic() - started,
            'sampledProcessTreePeakRssKiB': maximum, 'sampleIntervalMs': 25}


def hardware():
    result = {'availableLogicalCpus': len(os.sched_getaffinity(0)),
              'physicalMemoryGiB': os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES') / 2**30}
    try:
        allowed = os.sched_getaffinity(0)
        cpus = [dict(line.split(':', 1) for line in block.splitlines() if ':' in line)
                for block in Path('/proc/cpuinfo').read_text().split('\n\n') if block.strip()]
        cpus = [{key.strip(): value.strip() for key, value in cpu.items()} for cpu in cpus]
        selected = [cpu for cpu in cpus if int(cpu.get('processor', -1)) in allowed]
        result['cpuModel'] = selected[0].get('model name') if selected else None
        result['availablePhysicalCores'] = (len({(cpu['physical id'], cpu['core id']) for cpu in selected})
                                            if selected and all('physical id' in cpu and 'core id' in cpu for cpu in selected)
                                            else None)
    except (OSError, ValueError):
        result['availablePhysicalCores'] = None
    for path, key in [('/sys/fs/cgroup/memory.max', 'cgroupMemoryLimitBytes'),
                      ('/sys/fs/cgroup/cpu.max', 'cgroupCpuMax')]:
        try:
            text = Path(path).read_text().strip()
            result[key] = int(text) if text.isdecimal() else text
        except OSError:
            result[key] = None
    return result


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', default='build-release/reporting_capacity_benchmark')
    parser.add_argument('--worker', default='build-release/tigersheets-report-worker')
    parser.add_argument('--records', type=int, default=100000)
    parser.add_argument('--computed-results', action='store_true')
    parser.add_argument('--calculation-only', action='store_true')
    parser.add_argument('--output', default='docs/validation/reporting-capacity')
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    identities = {'benchmarkBinarySha256': digest(args.binary), 'workerBinarySha256': digest(args.worker)}
    measurement = run_sampled([str(Path(args.binary).resolve()), str(args.records),
                               str(output), str(Path(args.worker).resolve())] +
                              (['--computed-results' if args.computed_results else '--full-results',
                                '--calculation-only'] if args.calculation_only else
                               (['--computed-results'] if args.computed_results else [])),
                              output / 'benchmark.log')
    result_path = output / 'reporting-capacity.json'
    result = json.loads(result_path.read_text())
    financial = result.get('financialContextAcceptance', {})
    if 'udfCalls' in financial:
        financial['udfFormulaCells'] = financial.pop('udfCalls')
    if 'frozenRetainedLinkUses' in financial:
        financial['retainedLinkFormulaCells'] = financial.pop('frozenRetainedLinkUses')
    result['binaryIdentity'] = identities
    result['pipelineProcessTree'] = measurement
    result['hardware'] = hardware()
    result['referenceMachineMatch'] = False
    result['hardwareQualification'] = ('Measured host configuration is recorded above; this run '
                                       'does not qualify a 4-core, 16-GiB reference host.')
    inspector = Path(__file__).with_name('native_pivot_inspector.py').resolve()
    for suffix in ('', '-wal', '-shm'):
        (output / ('capacity-recovery.sqlite' + suffix)).unlink(missing_ok=True)
    result['recoveryDatabaseRetained'] = False
    result['independentInspection'] = []
    for index, fixture in enumerate(result.get('fixtureFiles', [])):
        log = output / f'independent-native-{index + 1}.log'
        inspected = run_sampled([sys.executable, str(inspector), fixture], log)
        inspected['result'] = json.loads(log.read_text().splitlines()[-1])
        result['independentInspection'].append(inspected)
    result_path.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
