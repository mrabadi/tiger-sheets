#!/usr/bin/env python3
"""Assemble one ordered native replay, with a fresh untouched source copy.

This writes action JSON only. It never builds a workbook snapshot or modifies
application cells. The historical source/model are created by their UI actions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from archive_otis_source_frames import archive_later_frames
from otis_replay_timing import merge_timed_overlays

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'docs/validation/otis-90-minute'
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=EVIDENCE / 'full-native-action-manifest.json')
parser.add_argument('--workspace', type=Path, help='Fresh canonical replay/delivery directory; existing model output is refused.')
parser.add_argument('--history-manifest', type=Path, help='Use an existing historical manifest only after checking its source copy is untouched.')
parser.add_argument('--skip-intro', action='store_true', help='Debug only: omit the isolated opening demonstration inspection.')
args = parser.parse_args()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

if args.history_manifest:
    history_path = args.history_manifest.resolve()
    history = json.loads(history_path.read_text())
    source_path = Path(history['workspace']) / 'Otis-10-K-Raw.xls'
    if sha(source_path) != sha(EVIDENCE / 'reference/101-01-Otis-10-K-Raw.xls'):
        raise SystemExit('Historical source copy has already been edited; generate a fresh replay workspace.')
else:
    workspace = args.workspace.resolve() if args.workspace else Path(tempfile.mkdtemp(prefix='tigersheets-otis-full-'))
    workspace.mkdir(parents=True, exist_ok=True)
    if (workspace / '101-01-90-Minute-Case-Study-Otis-Blank.xlsx').exists():
        raise SystemExit('Replay workspace already contains a model; archive the previous run before a fresh replay.')
    raw = workspace / 'Otis-10-K-Raw.xls'
    if raw.exists() and sha(raw) != sha(EVIDENCE / 'reference/101-01-Otis-10-K-Raw.xls'):
        raise SystemExit('Replay workspace source is edited; start with the untouched source copy.')
    history_path = workspace / 'historical-native-actions.json'
    command=[sys.executable, str(ROOT / 'tools/build_otis_history_manifest.py'), '--workspace', str(workspace), '--output', str(history_path)]
    if not args.skip_intro:command.append('--include-intro')
    subprocess.run(command, cwd=ROOT, check=True)
    history = json.loads(history_path.read_text())

segments = [(history_path, history)]
for name in ['income', 'balance', 'linking']:
    subprocess.run([sys.executable,str(ROOT/f'tools/build_otis_{name}_manifest.py')],cwd=ROOT,check=True)
    path = EVIDENCE / f'{name}-native-action-manifest.json'
    segments.append((path, json.loads(path.read_text())))

steps = []
ids = set()

def validate_ids(records):
    for step in records:
        if step['id'] in ids:
            raise SystemExit(f'Duplicate replay step ID: {step["id"]}')
        ids.add(step['id'])
        validate_ids(step['action'].get('steps', []))

for _, segment in segments:
    records = [step for step in segment['steps'] if not step['evidence'].get('observed', '').startswith('Interim handoff persistence only:')]
    validate_ids(records)
    steps.extend(records)

# Read-only contexts and presenter overlays are real native actions in the same
# process. Source window returns precede worksheet actions at equal timestamps.
for tool,path in [('context','external-context-native-action-manifest.json'),('model_annotation','model-annotation-native-action-manifest.json')]:
    subprocess.run([sys.executable,str(ROOT/f'tools/build_otis_{tool}_manifest.py')],cwd=ROOT,check=True)
    source_path=EVIDENCE/path;segment=json.loads(source_path.read_text());segments.append((source_path,segment));validate_ids(segment['steps']);steps.extend(segment['steps'])

def time_value(step):
    return sum(float(value)*60**n for n,value in enumerate(reversed(step['videoTime'].split(':'))))

def priority(step):
    action=step['action']
    if action['type']=='document_context' and action.get('operation') in ['hide','close']:return -2
    if action['type'] in ['document_image_selection','annotation'] and action.get('operation')=='clear':return -3
    return 1 if action['type']=='annotation' else 0

steps.sort(key=lambda step:(time_value(step),priority(step)))
subprocess.run([sys.executable,str(ROOT/'tools/build_otis_caption_manifest.py')],cwd=ROOT,check=True)
caption_path=EVIDENCE/'video-caption-native-action-manifest.json';captions=json.loads(caption_path.read_text());segments.append((caption_path,captions));validate_ids(captions['steps'])
# Captions can transition while an actual native modal event loop is running.
# Execute those transitions among the source-timed modal children, preserving
# every worksheet action and retaining parent-modal opening/closing behavior.
steps=merge_timed_overlays(steps,captions['steps'])

for number,(action,description) in enumerate([
    ({'type':'activate_window','documentName':'Otis-10-K-Raw.xls'},'Activate the genuinely edited source workbook for portable delivery.'),
    ({'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+S']},'Save the actual edited source; no reconstructed source snapshot is substituted.'),
    ({'type':'activate_window','documentName':'101-01-90-Minute-Case-Study-Otis-Blank.xlsx'},'Return to the final native model with its observed B2 selection.'),
    ({'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+S']},'Save the final native model, caches and view after the last observed tutorial action.')]):
    record={'id':f'delivery-native-save-{number+1}','videoTime':'84:28.9','evidence':{'kind':'acceptance','observed':description,'method':'Explicit delivery action after source footage, not an observed tutorial gesture.'},'action':action}
    if number==3:record['expect']={'row':2,'column':2,'topRow':1,'firstColumn':1,'cells':[{'address':'B52','input':'Income Statement:'},{'address':'B74','input':'Balance Sheet:'},{'address':'B102','input':'Cash Flow Statement:'}]}
    validate_ids([record]);steps.append(record)

oracle_path=EVIDENCE/'final-native-acceptance-oracle.json'
oracle=json.loads(oracle_path.read_text());final_cells={cell['address']:dict(cell) for cell in steps[-1]['expect']['cells']}
for cell in oracle['exactFormulaChecks']:
    final_cells.setdefault(cell['address'],{'address':cell['address']})['input']=cell['input']
for cell in oracle['independentExactHistoricalValues']:
    check=final_cells.setdefault(cell['address'],{'address':cell['address']});check.update(value=cell['value'],valueType='number')
    if cell['value']!=0:check['valueTolerance']={'absolute':1e-8,'relative':1e-12}
for column in 'FGHIJ':final_cells.setdefault(column+'100',{'address':column+'100'}).update(value=0,valueType='number')
steps[-1]['expect']['cells']=list(final_cells.values())

def joined_source_coverage():
    mapped={};native_by_id={}
    def visit(records):
        for record in records:
            native_by_id[record['id']]=record
            evidence=record['evidence'];event_id=evidence.get('eventId');ledger=evidence.get('ledger')
            if event_id and ledger:mapped.setdefault((Path(ledger).name,event_id),[]).append(record)
            visit(record['action'].get('steps',[]))
    visit(steps);rows=[];reviewed={};seen=set()
    def action_semantics(action):
        value=dict(action)
        if 'steps' in value:value['steps']=[{'videoTime':child['videoTime'],'action':action_semantics(child['action'])} for child in value['steps']]
        return value
    alias_path=EVIDENCE/'joined-source-semantic-aliases.json'
    if alias_path.exists():
        for alias in json.loads(alias_path.read_text())['entries']:
            key=(alias['ledger'],alias['sourceEventId'])
            if key in reviewed:raise ValueError('Duplicate reviewed source-event alias: '+str(key))
            if not alias.get('reason') or not alias.get('classification') or alias['classification']=='unmapped-source-observation':raise ValueError('Source alias requires an explicit reviewed classification and reason: '+str(key))
            references=alias['actionReferences']
            if not references and alias['classification']!='editorial-nonapplication-boundary':raise ValueError('Reviewed source state requires an actual native chain: '+str(key))
            for reference in references:
                native=native_by_id.get(reference['id'])
                if native is None:raise ValueError('Reviewed source alias references an unavailable native action: '+reference['id'])
                digest=hashlib.sha256(json.dumps(action_semantics(native['action']),sort_keys=True,separators=(',',':')).encode()).hexdigest()
                if native['videoTime']!=reference['videoTime'] or native['action']['type']!=reference['type'] or digest!=reference['actionSha256']:raise ValueError('Reviewed native action semantics changed: '+reference['id'])
            reviewed[key]=alias
    ledgers=['intro-context-visual-steps.json','intro-inspection-native-viewport-boundaries.json','initial-model-visual-steps.json','historical-statements-visual-steps.json','early-intro-context-visual-steps.json','early-history-context-visual-steps.json','income-projections-visual-steps.json','balance-cashflow-visual-steps.json','linking-debt-checks-visual-steps.json','later-shortcut-caption-visual-steps.json']
    for ledger in ledgers:
        path=EVIDENCE/ledger
        if not path.exists():continue
        source=json.loads(path.read_text())
        for event in source.get('events',source.get('observations',[])):
            identifier=event.get('id');key=(ledger,identifier);seen.add(key);actions=mapped.get(key,[])
            methods=' '.join(str(value) for action in actions for key,value in action['evidence'].items() if key in ['executionMethod','replayMethod','method'])
            classification='unmapped-source-observation'
            if actions:classification='native-actions-with-input-method-adaptation' if methods else 'native-actions-linked-to-visual-evidence'
            row={'ledger':ledger,'sourceEventId':identifier,'videoTime':event.get('time',event.get('videoSeconds',event.get('seconds'))),'classification':classification,'actionIds':[action['id'] for action in actions],'adaptationEvidence':methods or None,'observed':event.get('observed',event.get('observation',''))}
            if not actions and key in reviewed:
                alias=reviewed[key];row['classification']=alias['classification'];row['actionIds']=[reference['id'] for reference in alias['actionReferences']];row['adaptationEvidence']=alias['reason'];row['semanticReview']={'sidecar':alias_path.name,'reason':alias['reason'],'report':alias.get('report'),'limitations':alias.get('limitations',[])}
            rows.append(row)
    if set(reviewed)-seen:raise ValueError('Reviewed alias references an unavailable source observation: '+str(sorted(set(reviewed)-seen)))
    counts={}
    for row in rows:counts[row['classification']]=counts.get(row['classification'],0)+1
    return {'method':'Join exact source-ledger/event IDs recursively, then apply only independently reviewed per-event semantic aliases with verified native IDs, source times and action hashes. Editorial boundaries and unreproduced displays remain explicit classifications. A link is preparation evidence, not successful execution; unreviewed observations remain unmapped.','counts':counts,'events':rows}

manifest = {
    'version': 1,
    'sourceVideo': history['sourceVideo'],
    'windowSize': [1280, 720],
    'workspace': history['workspace'],
    'scope': 'One process, ordered native worksheet/source replay and read-only external-document contexts from the untouched BIFF source through the last unobscured model view at84:26. Segment handoff saves are omitted; final delivery saves are acceptance actions. External-application chrome, animation masks and native cell-region annotation adaptations remain disclosed.',
    'sourceReviewComplete': False,
    'preparedThrough': '84:26',
    'segments': [{'manifest': str(path), 'sha256': sha(path), 'preparedThrough': segment.get('preparedThrough'), 'nativeSteps': len(segment['steps']), 'remainingSourceEvents': segment.get('remainingSourceEvents', []), 'unreplayedVisualEffects': segment.get('unreplayedVisualEffects', []), 'externalContext': segment.get('externalContext', []), 'methodCaveats': segment.get('methodCaveats', []), 'unresolvedSourceGestures': segment.get('unresolvedSourceGestures', []), 'knownNativeStateGaps': segment.get('knownNativeStateGaps', [])} for path, segment in segments],
    'steps': steps,
    'nativeFinalOracle':{'path':str(oracle_path),'sha256':sha(oracle_path),'exactFormulaChecks':len(oracle['exactFormulaChecks']),'independentHistoricalValues':len(oracle['independentExactHistoricalValues']),'valueTolerance':{'absolute':1e-8,'relative':1e-12},'exactZeroChecks':'Source-exact zeros omit tolerance; no values or formulas are modified.','roundedDisplayChecks':'All342 independently source-rounded displays are verified separately by tools/audit_otis_native_final.py against the actual delivered native file.'},
    'joinedSourceCoverage':joined_source_coverage(),
    'executionStatus': 'Prepared for execution. This manifest is not evidence of successful replay or pixel identity.',
}
args.output.parent.mkdir(parents=True, exist_ok=True)
manifest=archive_later_frames(manifest)
args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')
print(json.dumps({'manifest': str(args.output), 'workspace': history['workspace'], 'topLevelActions': len(steps), 'actionsIncludingModalChildren': len(ids), 'sha256': sha(args.output)}))
