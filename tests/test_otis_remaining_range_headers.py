"""Source counts use real held selections rather than completed NameBox ranges."""
import json
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import os
import shutil
import hashlib

ROOT = Path(__file__).resolve().parents[1]
E = ROOT / 'docs/validation/otis-90-minute'
GENERATED = {}
WORKSPACES = {}
ANCHORS = [
    (2660, 2067, 'D27'), (2663, 2070, 'D27'), (2686, 2093, 'C33'),
    (2749, 2156, 'D40'), (2781, 2188, 'D42'), (2827, 2234, 'B2'),
    (3052, 2459, 'C7'), (3055, 2462, 'C7'), (3061, 2468, 'C9'),
    (3070, 2477, 'C10'), (3079, 2486, 'C16'), (3082, 2489, 'C16'),
    (3085, 2492, 'C16'), (3088, 2495, 'C24'), (3091, 2498, 'A28'),
    (3094, 2501, 'A28'), (3100, 2507, 'A29'), (3103, 2510, 'A29'),
    (3161, 2568, 'A54'), (3164, 2571, 'A54'), (4697, 4102, 'F2'),
    (3214, 2621, 'F2'), (4703, 4108, 'F24'), (3230, 2637, 'F24'),
    (4718, 4123, 'F24'), (4725, 4130, 'B24'), (3389, 2796, 'A16'),
    (4776, 4181, 'D38'), (4667,4072,'D59'), (4670,4075,'A53')]
ROWS = [72, 73, 76, 82, 84, 87, 99, 100, 101, 102, 104, 105, 106,
        107, 108, 109, 111, 112, 113, 114, 117, 118, 119, 120, 121, 122, 124, 132]


def generate(folder, mode):
    if mode in GENERATED:
        return copy.deepcopy(GENERATED[mode])
    output = Path(folder) / (mode + '.json')
    command = [sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'),
               '--workspace', str(Path(folder) / (mode + '-work')), '--output', str(output)]
    if mode == 'include-intro':
        command.append('--include-intro')
    result = subprocess.run(command, capture_output=True, timeout=180)
    if result.returncode:
        raise AssertionError('Source generator setup failed: ' + result.stderr.decode()[-4000:])
    GENERATED[mode] = json.loads(output.read_text())['steps']
    WORKSPACES[mode] = str(Path(folder) / (mode + '-work'))
    if os.environ.get('OTIS_REMAINING_RANGE_PROOF_DIR'):
        proof = Path(os.environ['OTIS_REMAINING_RANGE_PROOF_DIR'])
        proof.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(output, proof / ('current-' + mode + '.json'))
    return copy.deepcopy(GENERATED[mode])


def without_remaining(folder, mode):
    """Isolate own changes except the exact D42 prerequisite of a later ledger.

    This is a declared input-isolation adapter, not a substitute selection algorithm.
    The dense early ledger subsequently refines remaining-r084's anchor and phases;
    retaining that one byte-exact entry keeps its prerequisite real. All peer ledgers
    and the generator's actual __file__ remain unchanged.
    """
    empty = Path(folder) / 'isolated-input' / 'historical-remaining-range-header-phases.json'
    empty.parent.mkdir(exist_ok=True)
    own = json.loads((E / 'historical-remaining-range-header-phases.json').read_text())
    prerequisite = [entry for entry in own['entries'] if entry['id'] == 'remaining-r084']
    if len(prerequisite) != 1:
        raise AssertionError('Exactly one reviewed D42 prerequisite entry is required')
    empty.write_text(json.dumps({'entries': prerequisite}) + '\n')
    generator = ROOT / 'tools/build_otis_history_manifest.py'
    original = "remaining_range_path=EVIDENCE/'historical-remaining-range-header-phases.json'"
    source = generator.read_text()
    if source.count(original) != 1:
        raise AssertionError('The exact isolated ledger dependency must occur once')
    launcher = Path(folder) / 'isolated-generator.py'
    launcher.write_text('from pathlib import Path\n'
        + 'p=Path(' + repr(str(generator)) + ')\n'
        + 's=p.read_text()\n'
        + 'assert s.count(' + repr(original) + ')==1\n'
        + 's=s.replace(' + repr(original) + ',' + repr('remaining_range_path=Path(' + repr(str(empty)) + ')') + ')\n'
        + 'exec(compile(s,str(p),"exec"),{"__file__":str(p),"__name__":"__main__"})\n')
    workspace = str(Path(folder) / (mode + '-isolated-work'))
    output = Path(folder) / (mode + '-isolated.json')
    command = [sys.executable, str(launcher), '--workspace', workspace, '--output', str(output)]
    if mode == 'include-intro':
        command.append('--include-intro')
    result = subprocess.run(command, capture_output=True, timeout=180)
    if result.returncode:
        raise AssertionError('Isolated generator setup failed: ' + result.stderr.decode()[-4000:])
    if os.environ.get('OTIS_REMAINING_RANGE_PROOF_DIR'):
        proof = Path(os.environ['OTIS_REMAINING_RANGE_PROOF_DIR'])
        proof.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(output, proof / ('without-remaining-' + mode + '.json'))
        shutil.copyfile(launcher, proof / ('input-isolation-launcher-' + mode + '.py'))
    return json.loads(output.read_text())['steps'], workspace


def normalized(value, workspace):
    if isinstance(value, str):
        return value.replace(workspace, '<isolated-workspace>')
    if isinstance(value, list):
        return [normalized(item, workspace) for item in value]
    if isinstance(value, dict):
        return {key: normalized(item, workspace) for key, item in value.items()}
    return value


def unchanged_outside(before, after, workspace_before, workspace_after, approved):
    new = {f'historical-remaining-r{i:03d}-{suffix}' for i in ROWS for suffix in ['held', 'handoff']}
    # These two real source states occur identically in both projections because
    # the later dense D42 refinement requires their own prerequisite entry.
    new.difference_update({'historical-remaining-r084-held', 'historical-remaining-r084-handoff'})
    new.update('historical-remaining-d39-' + suffix for suffix in ['3r1c', '4r2c', 'release'])
    new.add('historical-remaining-d9-inspection')
    left = [normalized(s, workspace_before) for s in before if s['id'] not in approved]
    right = [normalized(s, workspace_after) for s in after if s['id'] not in approved and s['id'] not in new]
    if left != right:
        mismatch = next(((a['id'],b['id']) for a,b in zip(left,right) if a!=b), ('length',len(left),len(right)))
        raise AssertionError('An unapproved record, source assertion, action or order changed: ' + repr(mismatch))


class RemainingRangeHeaders(unittest.TestCase):
    def test_original_singleton_headers_replace_animated_range_inferences(self):
        with tempfile.TemporaryDirectory(prefix='otis-source-range-nav-') as folder:
            for mode in ['include-intro', 'default']:
                byid = {s['id']:s for s in generate(folder, mode)}
                with self.subTest(mode=mode, source='1518.500 D9'):
                    actual = byid.get('historical-remaining-d9-inspection')
                    self.assertIsNotNone(actual, 'Original1518.500 shows D9 before the later C9 held block')
                    self.assertEqual((actual['videoTime'], actual['action'], actual['expect']),
                        ('25:18.500', {'type':'mouse_cell', 'address':'D9', 'gesture':'click'},
                         {'row':9, 'column':4, 'nameBoxText':'D9'}))
                for full, default, address in [(4667,4072,'D59'), (4670,4075,'A53')]:
                    with self.subTest(mode=mode, source=address):
                        identifier = f'historical-{full if mode == "include-intro" else default:05d}-source-insert'
                        self.assertEqual(byid[identifier]['action'], {'type':'text', 'target':'formula.name', 'text':address},
                            'Original logical NameBox and formula input outrank the animated old range outline')
                        selected = byid[f'historical-{full+1 if mode == "include-intro" else default+1:05d}-source-insert']
                        self.assertEqual(selected['expect'], {'row':59 if address=='D59' else 53,
                            'column':4 if address=='D59' else 1, 'nameBoxText':address})

    def test_exact_bounded_changes_preserve_unrelated_objects_and_order(self):
        with tempfile.TemporaryDirectory(prefix='otis-remaining-outside-') as folder:
            for mode in ['include-intro', 'default']:
                after = generate(folder, mode)
                before, workspace = without_remaining(folder, mode)
                before_byid = {s['id']: s for s in before}
                after_byid = {s['id']: s for s in after}
                anchors = {}
                for full, default, anchor in ANCHORS:
                    serial = full if mode == 'include-intro' else default
                    identifier = f'historical-{serial:05d}-' + ('source-insert' if full >= 4000 else 'address')
                    anchors[identifier] = anchor
                retired_serials = ([2649, 2650, 2651, 2652, 2653, 2654, 4575, 4576, 4577, 2665, 2666, 2667]
                    if mode == 'include-intro' else [2056, 2057, 2058, 2059, 2060, 2061, 3980, 3981, 3982, 2072, 2073, 2074])
                retired = set()
                for serial in retired_serials:
                    matches = [s['id'] for s in before if s['id'].startswith(f'historical-{serial:05d}-')]
                    self.assertEqual(len(matches), 1)
                    retired.add(matches[0])
                self.assertEqual(set(before_byid) - set(after_byid), retired)
                for identifier, anchor in anchors.items():
                    with self.subTest(mode=mode, anchor=identifier):
                        actual = after_byid[identifier]
                        self.assertEqual(actual['action'], {'type': 'text', 'target': 'formula.name', 'text': anchor})
                        restored = copy.deepcopy(actual)
                        restored['action'] = before_byid[identifier]['action']
                        restored['evidence'] = before_byid[identifier]['evidence']
                        self.assertEqual(normalized(restored, WORKSPACES[mode]), normalized(before_byid[identifier], workspace))
                guard_serials = [4668,4671] if mode=='include-intro' else [4073,4076]
                guards = {f'historical-{n:05d}-source-insert' for n in guard_serials}
                for identifier in guards:
                    restored = copy.deepcopy(after_byid[identifier])
                    restored['evidence'] = before_byid[identifier]['evidence']
                    restored.pop('expect')
                    self.assertEqual(normalized(restored, WORKSPACES[mode]), normalized(before_byid[identifier], workspace))
                approved = set(anchors) | retired | guards
                unchanged_outside(before, after, workspace, WORKSPACES[mode], approved)
                damaged = copy.deepcopy(after)
                outside = [i for i,s in enumerate(damaged) if s['id'] not in approved and not s['id'].startswith('historical-remaining-')]
                damaged[outside[0]]['action'] = {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': ['Delete']}
                with self.assertRaisesRegex(AssertionError, 'unapproved'):
                    unchanged_outside(before, damaged, workspace, WORKSPACES[mode], approved)
                reordered = copy.deepcopy(after)
                first, second = outside[:2]
                reordered[first], reordered[second] = reordered[second], reordered[first]
                with self.assertRaisesRegex(AssertionError, 'unapproved'):
                    unchanged_outside(before, reordered, workspace, WORKSPACES[mode], approved)
                prerequisite_damage = copy.deepcopy(after)
                held = next(s for s in prerequisite_damage if s['id'] == 'historical-remaining-r084-held')
                held['action']['keys'] = ['Delete']
                with self.assertRaisesRegex(AssertionError, 'unapproved'):
                    unchanged_outside(before, prerequisite_damage, workspace, WORKSPACES[mode], approved)

    def test_later_verified_count_instants_use_native_range_extension(self):
        # These constants come from the frozen original-frame screen, not the phase ledger.
        expected = [(72, '22:51.500', '2R x 1C'), (73, '22:52.000', '7R x 2C'),
            (76, '22:58.000', '1R x 2C'), (82, '23:27.500', '2R x 2C'),
            (84, '23:35.400', '1R x 2C'), (87, '23:54.000', '1R x 9C'),
            (99, '25:17.000', '2R x 2C'), (100, '25:17.500', '4R x 3C'),
            (101, '25:19.000', '2R x 3C'), (102, '25:21.500', '2R x 3C'),
            (104, '25:25.000', '4R x 3C'), (105, '25:25.500', '5R x 3C'),
            (106, '25:26.000', '7R x 3C'), (107, '25:27.500', '2R x 3C'),
            (108, '25:30.000', '2R x 4C'), (109, '25:30.500', '1R x 5C'),
            (111, '25:31.500', '2R x 1C'), (112, '25:32.000', '2R x 5C'),
            (113, '25:59.500', '1R x 3C'),
            (114, '26:00.000', '2R x 5C'), (117, '26:43.500', '1R x 3C'),
            (118, '26:44.000', '1R x 5C'), (119, '26:52.500', '1R x 4C'),
            (120, '26:53.000', '1R x 5C'), (121, '26:59.500', '1R x 5C'),
            (122, '27:02.500', '2R x 8C'), (124, '28:16.000', '5R x 5C'),
            (132, '30:51.000', '4R x 1C')]
        with tempfile.TemporaryDirectory(prefix='otis-later-range-') as folder:
            for mode in ['include-intro', 'default']:
                steps = generate(folder, mode)
                byid = {s['id']: s for s in steps}
                for index, time, header in expected:
                    with self.subTest(mode=mode, sourceRow=index):
                        identifier = f'historical-remaining-r{index:03d}-held'
                        self.assertTrue(identifier in byid, f'Original source row{index} shows {header}: missing{identifier}')
                        phase = byid[identifier]
                        self.assertEqual((phase['videoTime'], phase['expect']['nameBoxText']), (time, header))
                        self.assertEqual(phase['action']['type'], 'key_sequence')
                        self.assertEqual(phase['action']['target'], 'worksheet.grid')
                        self.assertIs(phase['action']['releaseModifiers'], False)
                        self.assertTrue(all(key.startswith('Shift+') for key in phase['action']['keys']))
                        if index == 84:
                            self.assertEqual(phase['evidence']['phaseLedger'], 'historical-remaining-range-early-header-phases.json')
                            self.assertEqual(phase['evidence']['phaseGroup'], 'remaining-early-neighbor-d42-finite-range')
                            self.assertIn('unobservable', phase['evidence']['replayMethod'].lower())
                            self.assertIn('inferred', phase['evidence']['replayMethod'].lower())
                            anchor = byid['historical-02781-address' if mode == 'include-intro' else 'historical-02188-address']
                            self.assertEqual((anchor['videoTime'], anchor['action']),
                                ('23:35.200', {'type':'text', 'target':'formula.name', 'text':'D42'}))
                            release = byid['historical-remaining-r084-handoff']
                            self.assertEqual((release['videoTime'], release['action'], release['expect']),
                                ('23:35.667', {'type':'modifier_release', 'target':'worksheet.grid', 'keys':['Shift']},
                                 {'row':42, 'column':4, 'nameBoxText':'D42'}))
                            self.assertLess(steps.index(anchor), steps.index(phase))
                            self.assertLess(steps.index(phase), steps.index(release))
                            for actual, filename, digest in [
                                    (anchor, '0082.png', '008559694189f2af0665e91fe2ea04fe2c58c9295d51430a457a858c0a8fe9ea'),
                                    (phase, '0088.png', '150a51290daaf1ea4f9d3a857858f07f49658178d519d8c7494068842499aabc'),
                                    (release, '0096.png', '3b29f8a7eec1591b8d68f352fe3408885baaa310934214744cc19edd55710ba8')]:
                                frame = Path(actual['evidence']['sourceFrame'])
                                self.assertEqual(frame.name, filename)
                                self.assertEqual(hashlib.sha256(frame.read_bytes()).hexdigest(), digest)
                        else:
                            self.assertEqual(phase['evidence']['phaseLedger'], 'historical-remaining-range-header-phases.json')
                            self.assertIn('unobserved', phase['evidence']['replayMethod'].lower())

    def test_d39_observed_counts_and_released_fill_preserve_both_modes(self):
        with tempfile.TemporaryDirectory(prefix='otis-remaining-range-') as folder:
            for mode in ['include-intro', 'default']:
                with self.subTest(mode=mode):
                    steps = generate(folder, mode)
                    byid = {step['id']: step for step in steps}
                    first = byid.get('historical-remaining-d39-3r1c')
                    self.assertIsNotNone(first, 'Original1368.500 shows a held3Rx1C, not a completed NameBox range')
                    self.assertEqual((first['videoTime'], first['action'], first['expect']),
                        ('22:48.500', {'type': 'key_sequence', 'target': 'worksheet.grid',
                         'keys': ['Shift+Down', 'Shift+Down'], 'releaseModifiers': False},
                         {'row': 39, 'column': 4, 'nameBoxText': '3R x 1C'}))
                    second = byid['historical-remaining-d39-4r2c']
                    self.assertEqual((second['videoTime'], second['action']['keys'], second['expect']['nameBoxText']),
                        ('22:49.000', ['Shift+Down', 'Shift+Right'], '4R x 2C'))
                    self.assertIs(second['action']['releaseModifiers'], False)
                    release = byid['historical-remaining-d39-release']
                    self.assertEqual((release['videoTime'], release['action'], release['expect']),
                        ('22:49.500', {'type': 'modifier_release', 'target': 'worksheet.grid', 'keys': ['Shift']},
                         {'row': 39, 'column': 4, 'nameBoxText': 'D39'}))
                    fill = byid['historical-02655-fill-right' if mode == 'include-intro' else 'historical-02062-fill-right']
                    self.assertEqual(fill['action'], {'type': 'key_sequence', 'target': 'worksheet.grid', 'keys': ['Ctrl+R']})
                    self.assertLess(steps.index(release), steps.index(fill))
                    self.assertFalse(any(s['action'].get('target') == 'formula.name'
                        and s['action'].get('text') in ['D39:D41', 'D39:E42']
                        and s['videoTime'] in ['22:48.500', '22:49.000'] for s in steps))
                    for phase, frame in [(first, '1368.500.jpg'), (second, '1369.000.jpg'), (release, '1369.500.jpg')]:
                        self.assertEqual(Path(phase['evidence']['sourceFrame']).name, frame)
                        self.assertEqual(phase['evidence']['phaseLedger'], 'historical-remaining-range-header-phases.json')
                        self.assertIn('inferred', phase['evidence']['replayMethod'].lower())


if __name__ == '__main__':
    unittest.main()
