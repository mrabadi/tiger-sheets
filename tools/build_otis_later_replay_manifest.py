#!/usr/bin/env python3
"""Acceptance-load a genuinely UI-produced35:11 checkpoint and replay later UI actions.
Copies native saved files, not a semantic snapshot; every later cell is authored
by the recorded Qt actions. Final same-process full replay has no handoff loads.
"""
import argparse,json,shutil,tempfile,hashlib,subprocess,sys
from pathlib import Path
from otis_replay_timing import merge_timed_overlays
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'docs/validation/otis-90-minute'
p=argparse.ArgumentParser();p.add_argument('--checkpoint-dir',type=Path,required=True);p.add_argument('--workspace',type=Path);p.add_argument('--with-contexts',action='store_true');p.add_argument('--from-segment',choices=['income','balance','linking'],default='income');p.add_argument('--through',choices=['income','balance','linking'],default='linking');args=p.parse_args()
workspace=args.workspace or Path(tempfile.mkdtemp(prefix='tigersheets-otis-later-'));workspace.mkdir(parents=True,exist_ok=True)
names=['Otis-10-K-Raw.xls','101-01-90-Minute-Case-Study-Otis-Blank.xlsx'];provenance=[]
for name in names:
 src=args.checkpoint_dir/name;dst=workspace/name
 if dst.exists():raise SystemExit('Refusing to overwrite prior native replay files: '+str(dst))
 shutil.copyfile(src,dst);provenance.append({'source':str(src),'copy':str(dst),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()})
steps=[]
def prep(slug,action,expect=None):
 s={'id':'later-acceptance-'+slug,'videoTime':{'income':'35:10.9','balance':'50:11.9','linking':'67:11.9'}[args.from_segment],'evidence':{'kind':'acceptance','reference':'Load the actual UI-produced35:11 checkpoint; not a source gesture or cloned worksheet.','observed':'Explicit independent-segment preparation, omitted from final same-process replay.'},'action':action}
 if expect:s['expect']=expect
 steps.append(s);return s

def keys(slug,target,keys,expect=None):return prep(slug,{'type':'key_sequence','target':target,'keys':keys},expect)
def text(slug,target,value):return prep(slug,{'type':'text','target':target,'text':value})
def open_file(slug,path):
 parent=prep(slug,{'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+O']},'steps':[]});children=[]
 for suffix,action in [('select',{'type':'key_sequence','target':'fileNameEdit','keys':['Ctrl+A']}),('type',{'type':'text','target':'fileNameEdit','text':str(path)}),('confirm',{'type':'key_sequence','target':'fileNameEdit','keys':['Return']})]:
  item=prep(slug+'-'+suffix,action);steps.pop();children.append(item)
 parent['action']['steps']=children
open_file('open-native-source',workspace/names[0]);keys('first-source-sheet','workspace.sheets',['Home']);keys('restore-cashflows-tab','worksheet.grid',['Ctrl+PgDown']*36,{'sheetName':'cash flows'});keys('source-address-select','formula.name',['Ctrl+A']);text('source-address','formula.name','E51');keys('source-address-commit','formula.name',['Return'],{'row':51,'column':5,'cells':[{'address':'E51','value':1676}]});prep('source-viewport',{'type':'vertical_view','topRow':37});keys('collapse-source-ribbon','worksheet.grid',['Ctrl+F1']);keys('new-model-window','worksheet.grid',['Ctrl+N']);open_file('open-native-model',workspace/names[1]);keys('collapse-model-ribbon','worksheet.grid',['Ctrl+F1'])
if args.from_segment!='income':
 # Workbook files do not persist a window's Recent Colors palette. Recreate
 # that UI-only boundary with a temporary native format and one real Undo;
 # the original worksheet contents/styles are restored before replay starts.
 keys('temporary-color-address-select','formula.name',['Ctrl+A']);text('temporary-color-address','formula.name','A1');keys('temporary-color-address-commit','formula.name',['Return']);keys('open-color-palette','worksheet.grid',['Alt','1'])
 parent=prep('prime-recent-custom-blue',{'type':'modal','open':{'type':'mouse','target':'tutorial.font.moreColors'},'steps':[]});children=[]
 child=prep('prime-custom-tab',{'type':'mouse_tab','target':'colors.tabs','tab':'Custom'});steps.pop();children.append(child)
 for component,value in [('red','0'),('green','0'),('blue','255')]:
  for suffix,action in [('select',{'type':'key_sequence','target':'colors.'+component,'keys':['Ctrl+A']}),('type',{'type':'text','target':'colors.'+component,'text':value})]:
   child=prep('prime-'+component+'-'+suffix,action);steps.pop();children.append(child)
 child=prep('accept-temporary-blue',{'type':'key_sequence','target':'colors.dialog','keys':['Return']});steps.pop();children.append(child);parent['action']['steps']=children
 keys('undo-temporary-blue','worksheet.grid',['Ctrl+Z'],{'cells':[{'address':'A1','input':''}]})
keys('model-address-select','formula.name',['Ctrl+A']);text('model-address','formula.name',{'income':'B2','balance':'G33','linking':'G74'}[args.from_segment]);keys('model-address-commit','formula.name',['Return']);prep('model-viewport',{'type':'vertical_view','topRow':{'income':1,'balance':16,'linking':61}[args.from_segment]})
segments=[];order=['income','balance','linking'];first=order.index(args.from_segment);last=order.index(args.through)
if first>last:raise SystemExit('Incoming segment must precede the requested final segment.')
for name in order[first:last+1]:
 subprocess.run([sys.executable,str(ROOT/f'tools/build_otis_{name}_manifest.py')],cwd=ROOT,check=True)
 path=BASE/f'{name}-native-action-manifest.json';segment=json.loads(path.read_text());segments.append({'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()});steps.extend(segment['steps'])
 # Independent debug runs persist genuine native handoffs so later failures do
 # not erase the preceding successful segment; final same-process replay omits these.
 checkpoint=prep('persist-'+name+'-handoff',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+S']});checkpoint['videoTime']={'income':'50:12.5','balance':'67:12.5','linking':'84:27.5'}[name]
if args.with_contexts:
 subprocess.run([sys.executable,str(ROOT/'tools/build_otis_context_manifest.py')],cwd=ROOT,check=True);context=json.loads((BASE/'external-context-native-action-manifest.json').read_text());limit={'income':3012,'balance':4032,'linking':5066}[args.through]
 for item in context['steps']:
  time=sum(float(v)*60**n for n,v in enumerate(reversed(item['videoTime'].split(':'))))
  if {'income':2111,'balance':3012,'linking':4032}[args.from_segment]<=time<=limit:steps.append(item)
 subprocess.run([sys.executable,str(ROOT/'tools/build_otis_model_annotation_manifest.py')],cwd=ROOT,check=True)
 overlays=json.loads((BASE/'model-annotation-native-action-manifest.json').read_text())
 for item in overlays['steps']:
  time=sum(float(v)*60**n for n,v in enumerate(reversed(item['videoTime'].split(':'))))
  if {'income':2111,'balance':3012,'linking':4032}[args.from_segment]<=time<=limit:steps.append(item)
 def sortkey(s):
  action=s['action'];priority=-3 if action['type'] in ['annotation','document_image_selection'] and action.get('operation')=='clear' else -2 if action['type']=='document_context' and action.get('operation') in ['hide','close'] else 1 if action['type']=='annotation' else 0
  return (sum(float(v)*60**n for n,v in enumerate(reversed(s['videoTime'].split(':')))),priority)
 steps.sort(key=sortkey)
 subprocess.run([sys.executable,str(ROOT/'tools/build_otis_caption_manifest.py')],cwd=ROOT,check=True)
 captions=json.loads((BASE/'video-caption-native-action-manifest.json').read_text())
 caption_steps=[item for item in captions['steps'] if {'income':2111,'balance':3012,'linking':4032}[args.from_segment]<=sum(float(v)*60**n for n,v in enumerate(reversed(item['videoTime'].split(':'))))<=limit]
 steps=merge_timed_overlays(steps,caption_steps)
# An explicit delivery save persists actual UI edits, with qualified native caches.
prep('persist-model-after-replay',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+S']})['videoTime']={'income':'50:12.9','balance':'67:12.9','linking':'84:27.9'}[args.through]
manifest={'version':1,'sourceVideo':'https://www.youtube.com/watch?v=O2uPNfErmJo','windowSize':[1280,720],'sourceReviewComplete':False,'scope':'Independent later-segment native replay from an actual3264-action historical checkpoint. Acceptance fixture opening is explicit; full final replay must start from original untouched source.','workspace':str(workspace),'checkpointFiles':provenance,'segments':segments,'steps':steps,'withExternalContexts':args.with_contexts,'incomingSegment':args.from_segment}
out=workspace/'later-native-actions.json';out.write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({'manifest':str(out),'workspace':str(workspace),'steps':len(steps)}))
