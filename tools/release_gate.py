#!/usr/bin/env python3
"""Fail closed unless the frozen release contract has executed passing evidence."""
import argparse
import json
from pathlib import Path

def missing_evidence(catalog, results, key):
    return [entry[key] for entry in catalog['entries']
            if entry.get('scope') == 'required' and not valid_evidence(results.get(entry[key], {}))]

def valid_evidence(result):
    return (isinstance(result, dict) and result.get('passed') is True
            and isinstance(result.get('test'), str) and bool(result['test'].strip())
            and isinstance(result.get('cases'), list) and bool(result['cases'])
            and all(isinstance(case, str) and case.strip() for case in result['cases']))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = json.loads(args.report.read_text()) if args.report and args.report.exists() else {}
    failed = []
    for name, key in [('functions', 'name'), ('shortcuts', 'id'), ('features', 'id')]:
        catalog = json.loads((args.root / 'compatibility' / (name + '.json')).read_text())
        missing = missing_evidence(catalog, report.get(name, {}), key)
        print(f'{name}: {len(missing)} required entries without passing evidence')
        failed.extend(f'{name}:{item}' for item in missing)
    if failed:
        print('RELEASE BLOCKED: ' + ', '.join(failed[:20]) + (' ...' if len(failed) > 20 else ''))
        return 1
    print('Release compatibility gate passed')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
