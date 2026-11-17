#!/usr/bin/env python3
"""Build a native UI action manifest from retained visual observations.

This generator writes JSON and copies the untouched BIFF source into an isolated
working directory. It never constructs a workbook model or a final cell batch.
The replay uses actual editors, native commands, dialogs, clipboard and windows.
"""
from pathlib import Path
import argparse,json,shutil,tempfile
ROOT=Path(__file__).resolve().parents[1]
EVIDENCE=ROOT/'docs/validation/otis-90-minute'
parser=argparse.ArgumentParser();parser.add_argument('--workspace');parser.add_argument('--through',type=float,default=2110.5);parser.add_argument('--checkpoint-save',action='store_true');parser.add_argument('--include-intro',action='store_true');parser.add_argument('--output');args=parser.parse_args()
WORK=Path(args.workspace) if args.workspace else Path(tempfile.mkdtemp(prefix='tigersheets-otis-history-'))
WORK.mkdir(parents=True,exist_ok=True)
RAW=WORK/'Otis-10-K-Raw.xls';MODEL=WORK/'101-01-90-Minute-Case-Study-Otis-Blank.xlsx'
if not RAW.exists():shutil.copyfile(EVIDENCE/'reference/101-01-Otis-10-K-Raw.xls',RAW)
COMPLETE=WORK/'101-01-90-Minute-Case-Study-Otis.xlsx'
if args.include_intro and not COMPLETE.exists():shutil.copyfile(EVIDENCE/'reference/101-01-90-Minute-Case-Study-Otis-Complete.xlsx',COMPLETE)
initial=json.loads((EVIDENCE/'initial-model-visual-steps.json').read_text())['observations']
history=json.loads((EVIDENCE/'historical-statements-visual-steps.json').read_text())['observations']
context=json.loads((EVIDENCE/'intro-context-visual-steps.json').read_text())['observations']
observations=context+initial+history
steps=[];current_evidence=None;current_time=0;serial=0
text_phases={}
for phase_file in ['initial-model-title-phases.json','historical-text-phases-0001-0700.json','historical-text-phases-0701-1412.json','historical-formula-edit-phases.json']:
 path=EVIDENCE/phase_file
 if path.exists():
  for entry in json.loads(path.read_text())['entries']:text_phases[(entry['callSeconds'],entry['address'])]={**entry,'phaseLedger':phase_file}
local_formula_phases={}
for phase_file in ['historical-local-formula-phases-0001-0700.json','historical-local-formula-phases-0701-1412.json']:
 path=EVIDENCE/phase_file
 if path.exists():
  for entry in json.loads(path.read_text())['entries']:local_formula_phases[(entry['callSeconds'],entry['address'])]={**entry,'phaseLedger':phase_file}

def evidence(t,note=None,acceptance=False):
 global current_evidence,current_time
 current_time=t
 if acceptance:current_evidence={'kind':'acceptance','observed':note,'method':'Fixture preparation through native UI; source video starts with workbooks already open.'};return
 row=min(observations,key=lambda r:abs(r['videoSeconds']-t))
 ledger='initial-model-visual-steps.json' if row in initial else 'intro-context-visual-steps.json' if row in context else 'historical-statements-visual-steps.json'
 current_evidence={'kind':'visual','ledger':ledger,'eventId':row['id'],'sourceFrame':str(EVIDENCE/row['sourceFrame']),'observed':row['observation']}
 if note:current_evidence['replayMethod']=note

def step(action,expect=None,label='action'):
 global serial
 serial+=1;r={'id':f'historical-{serial:05d}-{label}','videoTime':f'{int(current_time//60):02d}:{current_time%60:06.3f}','evidence':dict(current_evidence),'action':action}
 if expect:r['expect']=expect
 steps.append(r);return r

def key(keys,target='worksheet.grid',expect=None,label='keys'):
 return step({'type':'key_sequence','target':target,'keys':keys},expect,label)
def text(value,target='worksheet.cellEditor',label='draft'):
 return step({'type':'text','target':target,'text':value},label=label)
def go(t,address):
 evidence(t,'Observed selection target. Replay uses the native Name Box because the physical navigation keys are not visible in the sampled source frames.')
 key(['Ctrl+A'],'formula.name');text(address,'formula.name','address');key(['Return'],'formula.name',label='select')
def window(t,name):
 evidence(t,'Observed workbook switch. Replay activates the actual retained native workbook window.')
 step({'type':'activate_window','documentName':name},label='window')
def cell(t,address,value,bold=False,commit='Return'):
 phase=text_phases.get((t,address));finish=t
 if phase:
  go(phase.get('selectSeconds',t),address);started=False
  if phase.get('editExisting'):
   if phase.get('editEventId'):current_evidence['eventId']=phase['editEventId']
   key(phase.get('editKeys',['F2','Home']));started=True
  if phase.get('boldAtStart'):
   evidence(phase.get('selectSeconds',t),'Source draft is already bold. Native Ctrl B before typing is an explicit visible-style adapter; its physical source trigger is not asserted.');key(['Ctrl+B'])
  for entry in phase['phases']:
   evidence(entry['seconds'],entry.get('observed','Observed native text phase. Prefixes grouped in one source observation retain their source order; individual physical key timings inside that group are not asserted.'))
   current_evidence['phaseLedger']=phase['phaseLedger']
   if entry.get('eventId'):current_evidence['eventId']=entry['eventId']
   if entry.get('sourceFrame'):
    source=Path(entry['sourceFrame']);current_evidence['sourceFrame']=str(source if source.is_absolute() else ROOT/source if entry['sourceFrame'].startswith('docs/') else EVIDENCE/source)
   operation=entry['operation']
   if operation=='keys':key(entry['keys'],'worksheet.cellEditor')
   elif operation=='replace':key(['Ctrl+A'],'worksheet.cellEditor');text(entry['text'])
   elif operation=='append':
    if not started:text(entry['text'][:1],'worksheet.grid');text(entry['text'][1:]) if len(entry['text'])>1 else None;started=True
    else:text(entry['text'])
   else:raise ValueError('Unknown source text phase '+operation)
  finish=phase['commitSeconds'];evidence(finish)
  if phase.get('commitEventId'):current_evidence['eventId']=phase['commitEventId']
 else:
  go(t,address);text(value[:1],'worksheet.grid')
  if len(value)>1:text(value[1:])
 expected={'cells':[{'address':address,'input':value}]}
 if phase:
  expected.update(phase.get('commitExpect',{}))
  if phase.get('commitSourceFrame'):current_evidence['sourceFrame']=str(ROOT/phase['commitSourceFrame'])
  if phase.get('commitMethod'):current_evidence['replayMethod']=phase['commitMethod']
 if phase and phase.get('retainCurrent'):
  import re
  col,row=re.fullmatch(r'([A-Z]+)([1-9][0-9]*)',address).groups();column=0
  for letter in col:column=column*26+ord(letter)-64
  expected.update(row=int(row),column=column);current_evidence['replayMethod']='Source Ready state retains the corrected current cell. Native formula checkmark adapts the unobserved physical acceptance gesture without an extra model edit.'
  step({'type':'mouse','target':'formula.accept','gesture':'click'},expected,'commit')
 else:key(phase.get('commitKeys',[commit]) if phase else [commit],'worksheet.cellEditor',expected,'commit')
 if bold and not (phase and phase.get('boldAtStart')):
  go(finish,address);key(['Ctrl+B'])
def edit(t,address,value):
 if (t,address) in text_phases:return cell(t,address,value)
 go(t,address);key(['F2']);key(['Ctrl+A'],'worksheet.cellEditor');text(value);key(['Return'],'worksheet.cellEditor',{'cells':[{'address':address,'input':value}]},'correct')
def cols(t,first,last=None):
 go(t,first+'1');key(['Ctrl+Space']+['Shift+Right']*(ord(last or first)-ord(first)),label='whole-columns')
def rows(t,first,last=None):
 go(t,'A'+str(first));key(['Shift+Space']+['Shift+Down']*((last or first)-first),label='whole-rows')
def modal(t,op,children,expect=None,label='dialog'):
 evidence(t);return step({'type':'modal','open':op,'steps':children},expect,label)
def child(action,label='dialog-action'):
 global serial
 serial+=1;return {'id':f'historical-{serial:05d}-{label}','videoTime':f'{int(current_time//60):02d}:{current_time%60:06.3f}','evidence':dict(current_evidence),'action':action}
def ck(keys,target):return child({'type':'key_sequence','target':target,'keys':keys})
def ct(value,target):return child({'type':'text','target':target,'text':value})
def cm(target):return child({'type':'mouse','target':target,'gesture':'click'})
def tab(name):return child({'type':'mouse_tab','target':'formatCells.tabs','tab':name})
def format_cell(t,address,category=None,code=None,border=None,unmerge=False,symbol=None,decimals=None):
 go(t,address);evidence(t,'Observed native Format Cells dialog. Controls use native clicks/keys; category navigation is an explicit replay adaptation.')
 children=[]
 if category is not None or code is not None:
  children.append(tab('Number'))
  if category is not None:children.append(ck(['Home']+['Down']*['General','Number','Currency','Accounting','Date','Time','Percentage','Fraction','Scientific','Text','Special','Custom'].index(category),'formatCells.categoryList'))
  if code is not None:children.extend([ck(['Ctrl+A'],'formatCells.numberFormat'),ct(code,'formatCells.numberFormat')])
  if decimals is not None:children.extend([ck(['Ctrl+A'],'formatCells.decimalPlaces'),ct(str(decimals),'formatCells.decimalPlaces')])
  if symbol is not None:children.append(ck(['Home']+(['Down'] if symbol=='$' else []),'formatCells.currencySymbol'))
 if unmerge:children.extend([tab('Alignment'),cm('formatCells.merge')])
 if border:children.extend([tab('Border'),cm('formatCells.border.'+border)])
 children.append(ck(['Return'],'formatCells.dialog'))
 return step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},'steps':children},label='format')
def width(t,column,value,legacy_started=False,edit_seconds=None,accept_seconds=None,expected_current=None):
 if not legacy_started:go(t,column+'1' if ':' not in column else ':'.join(c+'1' for c in column.split(':')))
 evidence(t,'Observed legacy Alt O C W and Column Width dialog; edit the real numeric input. An already visible legacy sequence continues without interrupting the existing whole-column selection.' if legacy_started else 'Observed legacy Alt O C W and Column Width dialog; edit the real numeric input.')
 children=[ck(['Ctrl+A'],'qt_spinbox_lineedit')]
 evidence(edit_seconds if edit_seconds is not None else t);children.append(ct(str(value),'qt_spinbox_lineedit'))
 evidence(accept_seconds if accept_seconds is not None else t);children.append(ck(['Return'],'qt_spinbox_lineedit'))
 evidence(t);expected={'row':expected_current[0],'column':expected_current[1]} if expected_current else None
 return step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['C','W'] if legacy_started else ['Alt','O','C','W']},'steps':children},expected,label='width')
def paste(t,source,dest,mode='formats',copy_seconds=None,accept_seconds=None):
 go(copy_seconds if copy_seconds is not None else t,source);key(['Ctrl+C']);go(t,dest);evidence(t,'Observed Paste Special '+mode+' operation; source and destination use native selection/clipboard.')
 children=[cm('pasteSpecial.'+mode)]
 evidence(accept_seconds if accept_seconds is not None else t);children.append(ck(['Return'],'pasteSpecial.dialog'));evidence(t)
 step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Alt','E','S']},'steps':children},label='paste-'+mode)
def fill(t,address):go(t,address);key(['Ctrl+R'],label='fill-right')
def indent(t,address):go(t,address);key(['Alt','H','6'],label='indent')
def visible_bold(t,address):
 go(t,address);evidence(t,'Visible source result is bold. Replay applies native Ctrl B explicitly because the sampled source does not expose the formatting gesture or Excel inherited-format mechanism. This is a replay adaptation, not an observed physical key claim.');key(['Ctrl+B'],label='visible-bold-adapter')
def border(t,address,side='top'):format_cell(t,address,border=side)
def deletecols(t,first,last=None):cols(t,first,last);key(['Ctrl+-'],label='delete-columns')
def cutcolumn(t,source,dest):cols(t,source);key(['Ctrl+X']);go(t,dest+'1');key(['Ctrl+V'],label='move-column')
def save(t):evidence(t,'Saved title observed; native Ctrl S replay method.');key(['Ctrl+S'],label='save')
def checkpoint(t,inputs):
 evidence(t,'Read-only checkpoint of cells visible in the retained source state; no workbook mutation.')
 key([],expect={'cells':[{'address':a,'input':v} for a,v in inputs.items()]},label='checkpoint')
def values(t,cells):
 evidence(t,'Read-only checkpoint of visible computed values, including deliberately wrong/intermediate results. The runner waits only for normal asynchronous calculation; it never changes the workbook or forces recalculation.')
 key([],expect={'cells':[{'address':a,'value':v,'valueType':'error' if isinstance(v,str) and v.startswith('#') else 'number'} for a,v in cells.items()]},label='computed-checkpoint')
def wheelto(t,address):
 evidence(t,'Observed source target; replay scrolls the actual viewport by wheel events until the cell is onscreen, retaining unfinished formula.')
 step({'type':'wheel_to_cell','target':'worksheet.grid','address':address},label='wheel-to-cell')
def pointer(t,address,gesture='click'):evidence(t);step({'type':'mouse_cell','address':address,'gesture':gesture},label='point')
def local_pointed_formula(t,address,phases=None,commit=None,formula=None):
 phase_ledger=None;draft_time=t
 if phases is None:
  source=local_formula_phases[(t,address)];phases=source['phases'];commit=source['commitSeconds'];formula=source['input'];phase_ledger=source['phaseLedger'];draft_time=source.get('draftSeconds',t)
 go(t,address);evidence(draft_time);text('=','worksheet.grid')
 for phase in phases:
  at,operation,value=(phase['seconds'],phase['operation'],phase['value']) if isinstance(phase,dict) else phase
  if operation in ['point','press','move','release']:wheelto(at,value)
  evidence(at,phase.get('observed') if isinstance(phase,dict) else None)
  if phase_ledger:current_evidence['phaseLedger']=phase_ledger
  if isinstance(phase,dict):
   if phase.get('eventId'):current_evidence['eventId']=phase['eventId']
   if phase.get('sourceFrame'):
    source_frame=Path(phase['sourceFrame']);current_evidence['sourceFrame']=str(source_frame if source_frame.is_absolute() else ROOT/source_frame if phase['sourceFrame'].startswith('docs/') else EVIDENCE/source_frame)
  if operation in ['point','press','move','release']:step({'type':'mouse_cell','address':value,'gesture':'click' if operation=='point' else operation},label='local-point')
  elif operation=='keys':key(value,'formula.input')
  else:text(value,'formula.input')
 evidence(commit);key(['Return'],'formula.input',{'cells':[{'address':address,'input':formula}]},'commit-local-pointed-formula')
def autosum(t,address,commit,formula,inspection=None):
 go(t,address)
 evidence(t,'Source SUM draft is visible. Native Alt Equals opens the same unfinished AutoSum proposal; the source physical shortcut is not visible and is an explicit input-method adaptation.')
 key(['Alt+='],label='autosum-draft')
 if inspection:
  raw_time,origin_time,source_cell=inspection
  window(raw_time,RAW.name);go(raw_time,source_cell);window(origin_time,MODEL.name)
 evidence(commit,'Source accepts the previously visible AutoSum draft as a separate native transaction.')
 key(['Return'],'worksheet.cellEditor',{'cells':[{'address':address,'input':formula}]},'commit-autosum')
def observed_header_anchors(first):
 for phase in json.loads((EVIDENCE/'historical-header-anchor-phases.json').read_text())['entries']:
  if (phase['address'][0] in 'CDE')!=first:continue
  go(phase['selectSeconds'],phase['address'])
  for at,frame,action in [('editSeconds','editFrame',{'type':'key_sequence','target':'worksheet.grid','keys':['F2']}),('anchorSeconds','anchorFrame',{'type':'key_sequence','target':'worksheet.cellEditor','keys':['F4']}),('commitSeconds','commitFrame',{'type':'mouse','target':'formula.accept','gesture':'click'})]:
   evidence(phase[at],'Source-retained native edit, F4 anchor and acceptance state. The no-focus formula checkmark adapts an unobservable physical acceptance trigger and retains the actual source active cell.')
   current_evidence.update(eventId=phase['eventId'],sourceFrame=str(ROOT/phase[frame]),phaseLedger='historical-header-anchor-phases.json')
   step(action,{'cells':[{'address':phase['address'],'input':phase['input']}]} if at=='commitSeconds' else None,'header-anchor')
# Source-visible draft phases override helper timing. Invisible physical keys
# remain disclosed adaptations; the intermediate selections are real UI events.
external_phases={
 (705.5,'C4'):{'switch':706.5,'references':[{'point':707.5,'f4':[709,709.5,710]}],'return':716.5,'commit':717},
 (722,'C8'):{'switch':724,'references':[{'point':725.5,'f4':[726,726.5,732.5]}],'commit':733},
 (736.5,'C10'):{'switch':738,'references':[{'point':739,'end':739.5,'f4':[740.833,741,741.5]}],'return':740.5,'anchorsAfterReturn':True,'endpointEdit':True,'inlineCommit':True,'commitControl':'formula.accept','commitCurrent':[10,3],'copyAfterCommit':True,'commit':743.5},
 (758,'C13'):{'switch':758.5,'references':[{'point':760,'f4':[761,761.5,762]}],'commit':762},
 (763.5,'C14'):{'switch':763.5,'references':[{'point':764.5,'f4':[765,765.5,766]}],'commit':766.5},
 (773.5,'C18'):{'switch':774,'references':[{'point':776,'f4':[776.5,777,778]}],'return':779,'originMouseResume':779,'inlineCommit':True,'commitControl':'formula.accept','commitCurrent':[18,3],'commit':779.5},
 (818,'C21'):{'switch':819.5,'references':[{'point':820.5,'f4':[821,821.25,821.5]}],'return':821.5,'originEdits':[(822,'keys',['F2'],'formula.input'),(824,'keys',['Home','Right'],'worksheet.cellEditor'),(824,'text','-','worksheet.cellEditor')],'finalPrefix':'=-','inlineCommit':True,'commitKeys':['Shift+Return'],'commitCurrent':[20,3],'commit':824.5},
 (1100.5,'C27'):{'switch':1102,'sourcePrefix':[(1102.5,'SUM'),(1103,'(')],'references':[{'point':1104,'end':1104,'f4':[1104.833,1105,1105.5]}],'commit':1106},
 (1106.5,'C28'):{'switch':1107,'references':[{'point':1108}],'return':1108.5,'originMouseResume':1108.5,'inlineCommit':True,'commit':1109},
 (1109.5,'C29'):{'switch':1111,'references':[{'point':1112.5,'rangePath':[(1113,'B12')],'f4':[1115,1115.25,1115.5]}],'commit':1116.5},
 (1118,'C30'):{'switch':1119.5,'references':[{'point':1122.5,'f4':[1122.667,1122.833,1123]},{'point':1126,'f4':[1126.333,1126.667,1127]},{'point':1128.5,'f4':[1128.667,1128.833,1129]}],'operators':[1123.5,1127.5],'commit':1129.5},
 (1132.5,'C31'):{'switch':1133,'references':[{'point':1134,'f4':[1134.167,1134.333,1134.5]}],'commit':1136},
 (1136.5,'C32'):{'switch':1137,'references':[{'point':1138.5,'f4':[1139,1139.25,1139.5]},{'point':1141.5,'f4':[1141.667,1141.833,1142]}],'operators':[1140],'commit':1143.5},
 (1315,'D36'):{'switch':1315.5,'references':[{'before':[(1319.5,'B24')],'point':1320,'f4':[1320.333,1320.667,1321]}],'commit':1322.5},
 (1328,'D39'):{'switch':1328.5,'references':[{'point':1329.5,'f4':[1329.667,1329.833,1330]},{'point':1332.5,'f4':[1333.5,1334,1334.5]}],'operators':[1330.5],'commit':1335.5},
 (1336.5,'D40'):{'switch':1337,'returnSwitches':[(1338,1339)],'references':[{'point':1340.5,'f4':[1340.833,1341.167,1341.5]}],'commit':1342},
 (1343,'D41'):{'switch':1343.5,'references':[{'point':1347.5,'f4':[1348,1349,1350]},{'point':1352,'f4':[1352.333,1352.667,1353]},{'point':1354.5,'f4':[1354.667,1354.833,1355]}],'operators':[1350.5,1353.5],'return':1355.5,'originEdits':[(1356,'keys',['F2'],'formula.input')],'inlineCommit':True,'commit':1357},
 (1381,'D44'):{'switch':1381.5,'references':[{'point':1384,'f4':[1385,1385.5,1386]}],'commit':1387},
 (1388.5,'D45'):{'switch':1389,'references':[{'point':1392.5,'f4':[1395,1395.5,1396]},{'point':1398.5,'f4':[1399,1399.5,1400]}],'operators':[1396.5],'commit':1401},
 (1813,'C56'):{'switch':1815.5,'references':[{'point':1817,'f4':[1818,1818.5,1819]}],'return':1820.5,'commit':1821},
 (1822,'C57'):{'switch':1826.5,'references':[{'point':1828,'f4':[1828.333,1828.667,1829]},{'point':1831,'f4':[1831.167,1831.333,1831.5]},{'point':1832.5,'f4':[1833,1833.25,1833.5]},{'point':1836.5,'f4':[1837.5,1838,1838.5]},{'point':1840.5,'f4':[1840.667,1840.833,1841]}],'operators':[1830,1832,1834,1839],'commit':1841.5},
 (1843,'C58'):{'switch':1845,'references':[{'point':1846.5,'rangePath':[(1846.5,'C19'),(1847,'C21')],'f4':[1847.167,1847.333,1847.5]}],'commit':1848.5},
 (1863.5,'C62'):{'switch':1866.633333,'originPeeks':[(1864.033333,1864.1)],'references':[{'point':1868,'f4':[1868.5,1869,1869.5]}],'commit':1870.5},
 (1871,'C63'):{'switch':1871.5,'sourcePrefix':[(1872.5,'S'),(1873,'UM'),(1874,'(')],'references':[{'point':1875.5,'rangePath':[(1875.5,'C30'),(1876.5,'C31'),(1877,'C32'),(1878,'C33')],'f4':[1878.167,1878.333,1878.5]}],'return':1879,'commit':1879.5},
 (1907,'C67'):{'switch':1908,'sourcePrefix':[(1909.5,'S'),(1910,'UM'),(1910.5,'(')],'references':[{'point':1911.5,'rangePath':[{'at':1911.5,'address':'C37','f4':[1912.5]},(1913,'C38'),(1913.5,'C39')],'f4':[1919,1919.5,1920]},{'point':1923,'f4':[1923.333,1923.667,1924]}],'operators':[1921.5,1921.5],'commit':1931.5},
 (1959.5,'C68'):{'switch':1960,'references':[{'point':1962.5,'f4':[1962.667,1962.833,1963]}],'commit':1963.5},
 (1964.5,'C69'):{'switch':1965,'references':[{'before':[(1966.5,'C45')],'point':1967,'f4':[1967.167,1967.333,1967.5]}],'commit':1968},
 (1969,'C70'):{'switch':1969.5,'references':[{'point':1971,'f4':[1971.5,1972,1972.5]}],'commit':1973.5},
 (1974.5,'C71'):{'switch':1975,'references':[{'before':[{'time':1977,'address':'C40','f4':[1977.5,1978,1978.5]},(1979.5,'C39')],'point':1980},{'point':1986.5,'f4':[1986.833,1987.167,1987.5]},{'point':1989,'f4':[1989.5,1990,1990.5]}],'operators':[1984,1988],'commit':1993},
 (2017.5,'C74'):{'switch':2017.5,'references':[{'point':2019,'f4':[2023,2023.5,2024]}],'return':2024,'commit':2024.5},
}
def external(t,address,parts,prefix='=',suffix='',anchor=False):
 # parts: source cell/range strings interleaved with arithmetic operator strings.
 phase=external_phases.get((t,address),{});reference_index=0;operator_index=0;deferred_anchors=[]
 go(t,address);text(prefix[:1],'worksheet.grid')
 if len(prefix)>1 and not phase.get('sourcePrefix'):text(prefix[1:])
 switch_editor='worksheet.cellEditor'
 if args.include_intro:
  for show,hide in phase.get('originPeeks',[]):
   evidence(show,'Dense source switcher selects the retained Complete workbook while origin bare= remains pending. Native Ctrl Tab reaches that actual workbook; source A100 display residue is unexplained and is not fabricated. Exact retained source boundary: c62-namebox-native-boundary.json.');key(['Ctrl+Tab'],switch_editor,{'windowCount':3,'documentName':COMPLETE.name,'formulaDraft':prefix},'attempt-complete-window')
   evidence(hide,'Source immediately returns to the origin worksheet with its unfinished formula. Native reverse window switch preserves the actual draft; no NameBox A100 input is invented.');key(['Ctrl+Shift+Tab'],'formula.input',{'windowCount':3,'documentName':MODEL.name,'formulaDraft':prefix},'return-from-complete-attempt');switch_editor='formula.input'
 evidence(phase.get('switch',t))
 key(['Ctrl+Shift+Tab'] if args.include_intro else ['Ctrl+Tab'],switch_editor,label='switch-source')
 for return_time,switch_time in phase.get('returnSwitches',[]):
  evidence(return_time,'Observed switch back to origin with unfinished draft.');key(['Ctrl+Tab'],'formula.input',label='temporary-origin-return');evidence(switch_time);key(['Ctrl+Shift+Tab'] if args.include_intro else ['Ctrl+Tab'],'formula.input',label='resume-source-pointing')
 for prefix_time,prefix_text in phase.get('sourcePrefix',[]):evidence(prefix_time);text(prefix_text,'formula.input',label='source-function-draft')
 for part in parts:
  if part in ['+','-',',',')','(']:
   operator_times=phase.get('operators',[])
   if operator_index<len(operator_times):evidence(operator_times[operator_index])
   text(part,'formula.input');operator_index+=1;continue
  reference_phases=phase.get('references',[])
  point_phase=reference_phases[reference_index] if reference_index<len(reference_phases) else {}
  reference_index+=1;point_time=point_phase.get('point',t)
  for draft_point in point_phase.get('before',[]):
   wrong_time,wrong_address=(draft_point['time'],draft_point['address']) if isinstance(draft_point,dict) else draft_point
   wheelto(wrong_time,wrong_address);pointer(wrong_time,wrong_address)
   if isinstance(draft_point,dict):
    for anchor_time in draft_point.get('f4',[]):evidence(anchor_time);key(['F4'],'formula.input',label='intermediate-relative-reference')
  start,_,end=part.partition(':');wheelto(point_time,start)
  if end:
   pointer(point_time,start,'press');path=point_phase.get('rangePath',[(point_phase.get('end',point_time),end)])
   for range_point in path:
    end_time,end=(range_point['at'],range_point['address']) if isinstance(range_point,dict) else range_point
    wheelto(end_time,end);pointer(end_time,end,'move')
    if isinstance(range_point,dict):
     for anchor_time in range_point.get('f4',[]):evidence(anchor_time);key(['F4'],'formula.input',label='intermediate-range-anchor')
   pointer(end_time,end,'release')
  else:pointer(point_time,start)
  if not anchor:
   if phase.get('anchorsAfterReturn'):deferred_anchors=point_phase.get('f4',[t,t,t])
   else:
    for anchor_time in point_phase.get('f4',[point_time]*3):evidence(anchor_time);key(['F4'],'formula.input',label='relative-reference')
 if suffix:text(suffix,'formula.input')
 if 'return' in phase:
  evidence(phase['return'],'Observed return to origin workbook before committing the unfinished formula; native Ctrl Tab preserves the draft.');key(['Ctrl+Tab'],'formula.input',label='return-origin-with-draft')
 if 'originMouseResume' in phase:
  evidence(phase['originMouseResume'],'Source retains a genuine inline editor and Point status after returning to the origin. Replay clicks the original cell using a verified native restoration path; the physical source return/click gesture is unobservable and this is an explicit input-method adaptation. See historical-inline-origin-surface-contract.json.')
  step({'type':'mouse_cell','address':address,'gesture':'click'},label='resume-origin-inline-point')
 if phase.get('endpointEdit'):
  evidence(740.75,'Source has an actual inline SUM editor and Enter status by741. Native F2 restores it, a second F2 cycles to Enter, and End/Left positions the final endpoint. These physical inputs/timing inside the sampled boundary are explicit adaptations; the native state and F4 endpoint cycles are verified.')
  key(['F2'],'formula.input',label='restore-inline-range');key(['F2'],'worksheet.cellEditor',label='inline-enter-mode');key(['End','Left'],'worksheet.cellEditor',label='caret-last-range-endpoint')
  draft_prefix="=-SUM('[Otis-10-K-Raw.xls]operations'!"
  for anchor_time,endpoint in zip(deferred_anchors,['G$15','$G15','G15']):evidence(anchor_time);key(['F4'],'worksheet.cellEditor',{'formulaDraft':draft_prefix+'$G$14:'+endpoint+')'},'cycle-last-endpoint')
  evidence(742,'Source changes from Enter to Edit mode before selecting the first range endpoint (Edit status visible by742.5). Replay uses native F2 followed by Home/Right/ShiftRight to select the same five-character token; exact source caret-input gesture is not visible.');key(['F2','Home']+['Right']*38+['Shift+Right']*5,'worksheet.cellEditor',label='select-first-range-endpoint')
  for anchor_time,endpoint in zip([742.5,742.8,743],['G$14','$G14','G14']):evidence(anchor_time);key(['F4'],'worksheet.cellEditor',{'formulaDraft':draft_prefix+endpoint+':G15)'},'cycle-first-endpoint')
 else:
  for anchor_time in deferred_anchors:evidence(anchor_time);key(['F4'],'formula.input',label='relative-reference')
 for at,operation,value,target in phase.get('originEdits',[]):
  evidence(at,'Returned source origin changes from Point to visible in-cell Edit. Native F2 restores the real inline editor; caret-navigation keys adapt an unobserved physical input. Dense C21 frames prove its reference stays positive until the literal minus insertion at824.0; the coarse negative-prefix narration is corrected by historical-positive-nci-boundary.json.')
  if address=='C21':current_evidence.update(phaseLedger='historical-positive-nci-boundary.json',sourceFrame=str(EVIDENCE/'source-frames/c21-sign-boundary'/f'{at:08.3f}.jpg'))
  key(value,target,label='origin-inline-edit') if operation=='keys' else text(value,target,label='origin-sign-insertion')
 sheet='operations' if t<900 else 'consolidated balance sheets' if t<1450 else 'cash flows'
 qualifier="'[Otis-10-K-Raw.xls]"+sheet+"'!"
 formula=phase.get('finalPrefix',prefix)
 import re
 for part in parts:
  if part in ['+','-',',',')','(']:formula+=part
  else:
   reference=re.sub(r'([A-Z]+)([1-9][0-9]*)',r'$\1$\2',part) if anchor else part
   formula+=qualifier+reference
 formula+=suffix
 evidence(phase.get('commit',t))
 expected={'cells':[{'address':address,'input':formula}]}
 if phase.get('commitCurrent'):
  expected.update(row=phase['commitCurrent'][0],column=phase['commitCurrent'][1]);current_evidence['replayMethod']='Source Ready state retains the observed active cell. Native checkmark or Shift Return adapts the unobservable physical acceptance/navigation gesture while preserving the actual cell and one formula transaction.'
 if phase.get('commitControl'):step({'type':'mouse','target':phase['commitControl'],'gesture':'click'},expected,'commit-reference')
 else:key(phase.get('commitKeys',['Return']),'worksheet.cellEditor' if phase.get('inlineCommit') else 'formula.input',expected,'commit-reference')
 if phase.get('copyAfterCommit'):
  current_evidence['replayMethod']='Source retained frame shows a native marching-ants copy outline after acceptance. Ctrl C is the disclosed native input-method counterpart; the physical source key is not visible.';key(['Ctrl+C'],label='copy-accepted-reference')
def openfile(t,path,shortcut='Ctrl+O',acceptance=True):
 evidence(t,'Open the untouched raw120-tab workbook in an isolated working copy.' if shortcut=='Ctrl+O' else 'Save the native blank model with the observed source filename.',acceptance)
 children=[ck(['Ctrl+A'],'fileNameEdit'),ct(str(path),'fileNameEdit'),ck(['Return'],'fileNameEdit')]
 step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':[shortcut]},'steps':children},label='open-file' if shortcut=='Ctrl+O' else 'save-as')

# The optional introductory inspection opens the original published completed
# workbook read-only in its own real window. Construction reuses the separate
# blank model. Nothing is copied from that demonstration into the model.
if args.include_intro:
 openfile(17.5,MODEL,'Ctrl+Shift+S');evidence(17.5,'The blank model is already open/collapsed in the video. Save/open and initial collapse are explicit native acceptance preparation.',True);key(['Ctrl+F1']);go(17.5,'B2')
 evidence(20,'Open the existing demonstration workbook in its own real native window. It remains open during construction as shown by the later three-workbook source switcher; its contents never enter the blank model.',True);key(['Ctrl+N']);openfile(20,COMPLETE)
 # The currently published completed workbook differs from the opening video
 # demonstration. Prepare only its isolated copy through ordinary native UI.
 # This variant is never used for constructing the fresh blank model.
 prep_start=len(steps)
 rows(20,29);key(['Ctrl++'],expect={'cells':[{'address':'B53','input':'Income Statement:'}]},label='demo-insert-blank-row');rows(20,38,39);key(['Ctrl++'])
 go(20,'B45:J46');key(['Ctrl+X']);go(20,'B38');key(['Ctrl+V']);rows(20,45,46);key(['Ctrl+-'])
 for address,label in {'B22':'A/R % Revenue:','B26':'AP % COGS:','B27':'AL % Total Expenses:','B28':'Cont. Liab. % Total Expenses:','B30':'Other Liabilities % Total Expenses:','B34':'Pensions & Other % Revenue:','B38':'Average Interest Rate on Debt:','B42':'% in Stock Repurchases:','B43':'% in Debt Repayment:','B45':'Changes in Debt:','B46':'Common Dividends % Net Income:','B86':'LIABILITIES & EQUITY:','B106':'(+/-) Net Income to NCI:','B108':'(+/-) Pensions & Other Items:','B118':'(+/-) Changes in Debt:'}.items():edit(20,address,label)
 for address,value in {'C47':'0','D47':'0','E47':'-725'}.items():cell(20,address,value)
 format_cell(20,'C47:E47',category='Accounting',decimals=0,symbol='');format_cell(20,'F41:J41',category='Accounting',decimals=0,symbol='')
 # These visible opening-demo formats differ from the published reference and
 # are confined to this isolated copy. JPEG colors imply the Excel pale-yellow
 # input palette with light-gray borders; exact source colors are not metadata.
 format_cell(20,'C9:J9',category='Custom',code='0.0')
 # Source22.0/22.5 shows plain first-forecast drivers. The published copy has
 # these20nonempty F cells bold; neighboringG:J and date headers differ.
 # All changes below are explicit isolated-demo acceptance preparation.
 plain_driver_ranges=['F15:F17','F19:F20','F22:F24','F26:F28','F30','F32:F34','F36','F46','F48','F50:F51']
 plain_driver_cells=[]
 for address in plain_driver_ranges:
  first,_,last=address.partition(':');cells=['F'+str(row) for row in range(int(first[1:]),int((last or first)[1:])+1)];plain_driver_cells.extend(cells)
  go(20,address);key(['Ctrl+B'],expect={'cells':[{'address':cell,'style':{'bold':False}} for cell in cells]},label='demo-source-plain-drivers')
 # Source24.5 has a blank row111, no currency on the row101 zero checks,
 # and bold NetIncomeToParent amounts. Preserve the real formulas elsewhere.
 go(20,'F111:H111');key(['Delete'],expect={'cells':[{'address':column+'111','input':'','display':''} for column in 'FGH']},label='demo-source-clear-scratch')
 zero_format=format_cell(20,'D101:J101',category='Accounting',decimals=0,symbol='');zero_format['id']=f'historical-{serial:05d}-demo-source-zero-format'
 no_symbol_accounting=r'_(* #,##0_);_(* \(#,##0\);_(* "-"_);_(@_)'
 zero_checks=[{'address':column+'101','input':'='+column+'84-'+column+'99','value':0,'style':{'numberFormat':no_symbol_accounting}} for column in 'DEFGHIJ']+[{'address':'C101','input':''}]
 zero_format['expect']={'cells':zero_checks}
 # Source cash-flow historical zero dashes are dark rather than the published
 # blue font. The native Font page resets only these three demo cells without
 # changing the remembered Quick Access font color used by the source chrome.
 dark_zero_checks=[]
 for address in ['C119','C120','D120']:
  expected={'address':address,'input':'0','value':0,'style':{'color':'','numberFormat':no_symbol_accounting}};dark_zero_checks.append(expected)
  go(20,address);children=[tab('Font'),ck(['Ctrl+A'],'formatCells.color'),ck(['Backspace'],'formatCells.color'),ck(['Return'],'formatCells.dialog')]
  step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},'steps':children},{'cells':[expected]},'demo-source-dark-zero')
 # Native30fps source24.8 exposes the full NetChangeinCash row with currency
 # symbols in all eight columns. Format only this isolated demo range; preserve
 # its real formulas and tiny residuals, including the unexplained G127 sign.
 net_cash_format=format_cell(20,'C127:J127',category='Accounting',decimals=0,symbol='$');net_cash_format['id']=f'historical-{serial:05d}-demo-source-net-cash-currency'
 dollar_accounting=r'_("$"* #,##0_);_("$"* \(#,##0\);_("$"* "-"_);_(@_)'
 net_cash_checks=[{'address':column+'127','input':'='+'+'.join(column+str(row) for row in [110,115,123,125]),'style':{'numberFormat':dollar_accounting}} for column in 'CDEFGHIJ']
 net_cash_format['expect']={'cells':net_cash_checks}
 go(20,'C105:J105');key(['Ctrl+B'],expect={'cells':[{'address':column+'105','input':'='+column+'73','style':{'bold':True}} for column in 'CDEFGHIJ']},label='demo-source-bold-net-income')
 for address in ['F5:J5','F7:J7','F10:J10','F13:J13','F15:J17','F19:J20','F22:J24','F26:J28','F30:J30','F32:J34','F36:J36','F38:J38','F46:J46','F48:J48','F50:J51','E40','E42:E43']:
  go(20,address);evidence(20,'Opening-demo visible pale-yellow assumption ranges and faint cell boundaries; source palette values are inferred from retained JPEG pixels.')
  children=[tab('Fill'),ck(['Ctrl+A'],'formatCells.fill'),ct('#FFFF99','formatCells.fill'),tab('Border'),ck(['Ctrl+A'],'formatCells.borderColorInput'),ct('#BFBFBF','formatCells.borderColorInput')]
  children.extend(cm('formatCells.border.'+side) for side in ['top','bottom','left','right']);children.append(ck(['Return'],'formatCells.dialog'))
  step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},'steps':children},label='demo-yellow-format')
 width(20,'B',34.57);width(20,'D:J',9.57);save(20);go(20,'B53')
 def mark_demo_acceptance(records):
  for record in records:
   record['evidence']={'kind':'acceptance','observed':'Prepare only the isolated published demonstration copy to match the opening video variant: blankrow29, interestdriverblockbeforeminimumcash, visible label differences, historical repurchase entries, plain first-forecast drivers, blank cash-flow scratch row, no-symbol balance-check zeros, three dark historical cash-flow zero dashes, currency on the source-visible Net Change in Cash row, bold cash-flow Net Income to Parent and visible column widths. Native UI only; no contents enter blank-model construction.','method':'Explicit demo-only preparation, not observed video editing. See native-history/intro-completed-reference-discrepancy.json and intro-demo-variant-audit.json. Source22.0/22.5/24.0/24.5 original frames distinguish these labels/styles from the published workbook; the separately constructed final liabilities spelling remains source-observed.'}
   if record['action'].get('type')=='modal':mark_demo_acceptance(record['action']['steps'])
 mark_demo_acceptance(steps[prep_start:])
 evidence(20,'The demonstrated completed-workbook window has a collapsed ribbon. Configure the same real native view.',True);key(['Ctrl+F1'])
 for t,rect in [(20,[1103,94,1280,720]),(20.5,[105,66,1280,720]),(21,[-18,10,1280,720]),(21.5,[0,0,1280,720])]:
  evidence(t,'Original source Windows outer-border positions are adapted to native Qt client geometry. First restored window right/bottom bounds are clipped;1280x720 retained size is inferred from the later source window. Qt offscreen frame insets remain explicit in actual capture, rather than claimed pixel equality.')
  step({'type':'window_geometry','rect':rect},label='intro-window-geometry')
 checkpoint(21.5,{'B53':'Income Statement:','B75':'Balance Sheet:','B103':'Cash Flow Statement:','B38':'Average Interest Rate on Debt:','B47':'Stock Repurchases:','C47':'0','D47':'0','E47':'-725'})
 demo_views={
  21.5:[{'address':'B53','input':'Income Statement:'},{'address':'C57','value':13118},{'address':'F57','display':'  15,180 '},{'address':'J71','display':'  1,717 '}],
  22:[{'address':'B38','input':'Average Interest Rate on Debt:'},{'address':'E40','value':3000},{'address':'E47','value':-725},{'address':'F41','display':'  1,655 '}],
  22.5:[{'address':'B22','input':'A/R % Revenue:'},{'address':'B26','input':'AP % COGS:'},{'address':'F15','display':'82.4%'}],
  23:[{'address':'B2','input':'Drivers:'},{'address':'C4','value':35300},{'address':'F5','display':'5.0%','style':{'fill':'#ffff99'}},{'address':'C9','display':'2.0','style':{'numberFormat':'0.0'}}],
  23.5:[{'address':'B53','input':'Income Statement:'},{'address':'F62','display':'  2,094 '},{'address':'F71','display':'  1,349 '}],
  24:[{'address':'B75','input':'Balance Sheet:'},{'address':'B86','input':'LIABILITIES & EQUITY:'},{'address':'D78','value':1799},{'address':'E78','value':3475},{'address':'J84','display':' $ 13,661 '}],
  24.5:[{'address':'B103','input':'Cash Flow Statement:'},{'address':'B106','input':'(+/-) Net Income to NCI:'},{'address':'B108','input':'(+/-) Pensions & Other Items:'},{'address':'C110','value':1469},{'address':'F110','display':'  2,020 '},{'address':'J110','display':'  2,243 '}],
  25:[{'address':'C62','value':1853},{'address':'E71','value':1420},{'address':'F71','display':'  1,349 '}],
  25.5:[{'address':'B30','input':'Other Liabilities % Total Expenses:'},{'address':'F19','display':'29.9%'},{'address':'F38','display':'2.5%'}],
  26:[{'address':'B2','input':'Drivers:'},{'address':'C4','value':35300},{'address':'F5','display':'5.0%'}]}
 demo_views[22.5].extend({'address':address,'style':{'bold':False}} for address in plain_driver_cells if int(address[1:])<=36)
 demo_views[22].extend({'address':address,'style':{'bold':False}} for address in plain_driver_cells if int(address[1:])>=34)
 demo_views[23].extend([{'address':'F2','style':{'bold':True}}]+[{'address':address,'style':{'bold':False}} for address in ['G15','J15','G46','J46']])
 demo_views[24.5].extend(zero_checks+dark_zero_checks+[{'address':column+'111','input':'','display':''} for column in 'FGH']+[{'address':column+'105','input':'='+column+'73','style':{'bold':True}} for column in 'CDEFGHIJ']+[{'address':address,'style':{'color':'#0000FF'}} for address in ['D119','E119','E120']])
 # Supersede coarse half-second viewport labels with every actual native30fps
 # first-row transition. No physical wheel/key input is claimed from pixels.
 inspection=json.loads((EVIDENCE/'intro-inspection-native-viewport-boundaries.json').read_text())
 coarse_checks={21.5:21.5,22.1:22,22.733333:22.5,23.033333:23,23.733333:23.5,24.066667:24,24.733333:24.5,25.233333:25,25.733333:25.5,25.966667:26}
 for observation in inspection['observations']:
  t=observation['seconds'];top=observation['topRow'];evidence(t,'Original native30fps first-visible row and retained B53 selection. Actual native wheel navigation adapts the unobservable source scroll gesture; no cell selection or workbook edit is synthesized.')
  current_evidence.update(ledger='intro-inspection-native-viewport-boundaries.json',eventId=observation['id'],sourceFrame=observation['sourceFrame'],sourceFrameSha256=observation['sourceFrameSha256'],sourceSeconds=t,observed='Top row '+str(top)+'; active B53 and Income Statement formula bar remain unchanged.')
  checks=list(demo_views[coarse_checks[t]]) if t in coarse_checks else []
  if top==106:checks.extend(net_cash_checks)
  step({'type':'vertical_view','topRow':top},{'topRow':top,'row':53,'column':2,'formulaDraft':'Income Statement:','cells':checks},'intro-inspection-viewport')
 evidence(26,'The source retains the final topRow1 view. This read-only empty key checkpoint preserves focus and does not add a scroll gesture.');key([],expect={'topRow':1,'row':53,'column':2,'formulaDraft':'Income Statement:','cells':demo_views[26]},label='intro-inspection-held-final')
 window(27,MODEL.name)
 evidence(397,'Create the already-open source workbook window before loading the untouched rawBIFF file. Native CtrlNis acceptance preparation; no source cells are cloned.',True);key(['Ctrl+N'])
# Native fixture opening precedes the observed raw-sheet traversal.
openfile(397,RAW)
if args.include_intro:steps[-1]['expect']={'windowCount':3,'documentName':RAW.name}
evidence(397,'The source raw workbook is already collapsed in the first visible06:37frame. Configure this real view before all38tab transitions; the physical collapse gesture is unobserved acceptance preparation.',True);key(['Ctrl+F1'])
intro=json.loads((EVIDENCE/'intro-visual-steps.json').read_text())
for r in intro['rawNavigation']['steps']:
 q=dict(r);q['expect']={'sheetName':r['observed']['sheetName'],'row':1,'column':1};steps.append(q)
for t,top in [(417.5,2),(418,7),(418.5,5),(419,1)]:
 evidence(t,'Observed operations viewport change with A1 retained. Replay uses actual native scrollbar/wheel events; the physical source scroll input is unobservable.');step({'type':'vertical_view','topRow':top},{'topRow':top,'row':1,'column':1},'intro-raw-viewport')
go(423,'D10')
intro_drag=json.loads((EVIDENCE/'intro-raw-drag-phases.json').read_text())
for phase in intro_drag['phases']:
 evidence(phase['seconds'],'Observed native pointer phase; the source Name Box shows '+phase['observedNameBox']+'. Original30fps frames distinguish held drag dimensions from partial repaint residue.')
 current_evidence.update(ledger='intro-context-visual-steps.json',eventId=phase['sourceEventId'],phaseLedger='intro-raw-drag-phases.json',sourceFrame=str(ROOT/phase['sourceFrame']))
 step({'type':'mouse_cell','address':phase['address'],'gesture':phase['gesture']},phase['nativeExpectedState'],'intro-raw-pointer')
phase=intro_drag['viewport'];evidence(phase['seconds'],'Observed source scroll retains the G8 anchor and G8:I23 selection; actual native scrollbar/wheel events adapt the unobservable scroll gesture.')
current_evidence.update(ledger='intro-context-visual-steps.json',eventId='context-0191',phaseLedger='intro-raw-drag-phases.json',sourceFrame=str(ROOT/phase['sourceFrame']))
step({'type':'vertical_view','topRow':phase['topRow']},phase['nativeExpectedState'],'intro-raw-pointer-viewport')
for t,top in [(428.5,2),(429,1)]:
 evidence(t,'Observed upward operations scroll retaining G16. Replay uses actual native scrollbar/wheel events.');step({'type':'vertical_view','topRow':top},{'topRow':top,'row':16,'column':7},'intro-raw-return-viewport')
# Video model already exists when first activated. Create and save through native
# UI unless the optional introductory inspection already prepared that window.
if args.include_intro:window(487.5,MODEL.name)
else:
 evidence(487.5,'Create the already-blank model as native UI preparation; no cells or metadata are cloned.',True);key(['Ctrl+N']);openfile(487.5,MODEL,'Ctrl+Shift+S')
 evidence(487.5,'Source model ribbon is already collapsed. Configure the real native view for the observed starting checkpoint.',True);key(['Ctrl+F1'])
go(488.5,'A4');evidence(491,'Observed temporary View page while gridlines remain visible. Native Alt W opens the real temporary ribbon; physical source opening method is unobservable.');key(['Alt','W'],expect={'showGridLines':True,'row':4,'column':1})
evidence(492.5,'Observed collapsed ribbon with gridlines still visible. Native Escape is a disclosed closing-method adapter.');key(['Escape'],expect={'showGridLines':True,'row':4,'column':1})
evidence(493,'Source gridlines first disappear; caption explicitly advertises Alt W V G.');key(['Alt','W','V','G'],expect={'showGridLines':False,'row':4,'column':1})
evidence(494.3);key(['Alt','O'],label='initial-legacy-banner');width(495,'A',2,legacy_started=True,edit_seconds=496,accept_seconds=496.5,expected_current=(4,1))
for t,a in [(497,'B3'),(497.5,'B2'),(498,'A2')]:go(t,a)
cell(502.533333,'B2','Income Statement:',True)
go(505,'B2');key(['Ctrl+A']);evidence(506.5);key(['Alt','H']);evidence(507);key(['Escape','Alt','8']);key(['Ctrl+A'],'tutorial.qat.fontSize');evidence(508);text('12','tutorial.qat.fontSize');evidence(508.5);key(['Return'],'tutorial.qat.fontSize')
for t,a in [(509,'B2'),(509.5,'D2'),(510,'E2'),(511.5,'B2')]:go(t,a)
window(513.5,RAW.name);go(514.5,'P7')
evidence(515,'The visible source pointer selects S6 while A remains the first column. Use an actual native click on the visible merged cell, avoiding Name Box navigation that scrolls its right-clipped S6:U6 span. The physical button transition is inferred from the source pointer/selection boundary.')
step({'type':'mouse_cell','address':'S6','gesture':'click'},{'row':6,'column':19,'firstColumn':1},'source-s6-pointer')
window(516,MODEL.name)
go(517.633,'B2');evidence(517.833,'Native30fps boundary review shows legacy Alt O C banner, then B AutoFit to139pixels without any width dialog. Final A key is inferred from native command/outcome rather than directly visible keypress.');key(['Alt','O','C','A'],label='autofit-column')
go(518.5,'C2');evidence(519);text('1','worksheet.grid');evidence(519.5);text('2/');evidence(520);text('20/');evidence(520.5);text('21');evidence(521);key(['Backspace']*4,'worksheet.cellEditor');evidence(522);key(['Backspace'],'worksheet.cellEditor');text('31/');evidence(523);text('2019');key(['Tab'],'worksheet.cellEditor')
evidence(524);text('=','worksheet.grid');text('E');evidence(525);text('OMONTH(C2');evidence(526);text('m');evidence(527);key(['Backspace'],'worksheet.cellEditor');text(',12)');key(['Return'],'worksheet.cellEditor')
fill(528,'D2:E2');paste(530,'C2','C2:E2',copy_seconds=529,accept_seconds=531);cols(532,'C','E');key(['Alt','O','C','A']);go(533,'C2');go(535,'C2:E2')
# Native 30fps review resolves the dialog's lifespan and each visible prefix.
# The source has a literal space in the code; the published workbook's escaped
# space is equivalent, but is not the raw code entered in this workflow.
first_format_children=[]
evidence(539.4,'Observed Format Cells opening with C2:E2 already selected. The Number-tab click makes the native starting page explicit; physical source opening keys are not visible.')
first_format_children.append(tab('Number'))
evidence(541.366667,'Observed Custom category selection; native category-list keys reproduce the visible result.')
first_format_children.append(ck(['Home']+['Down']*11,'formatCells.categoryList'))
evidence(543.366667,'Observed selected Type field before replacement; Ctrl A is the native replay method.')
first_format_children.append(ck(['Ctrl+A'],'formatCells.numberFormat'))
for t,prefix in [(544.266667,'"'),(544.566667,'F'),(544.833333,'Y'),(545.366667,'"'),(545.8,' '),(546.066667,'y'),(546.2,'y')]:
 evidence(t,'Individually retained native 30fps source frame shows this next Type-field character.')
 first_format_children.append(ct(prefix,'formatCells.numberFormat'))
evidence(547.166667,'Observed acceptance fade followed by FY19 headers; Return is a native replay adaptation because the physical acceptance gesture is not visible.')
first_format_children.append(ck(['Return'],'formatCells.dialog'))
first_format=modal(539.4,{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},first_format_children,label='format')
first_format['evidence']['boundaryLedger']='first-format-cells-native-boundaries.json'
visible_bold(548,'C2:E2');go(549,'C6');go(550,'B4')
window(553,RAW.name);go(554,'I9');go(555,'G6');go(556,'J6');go(557,'G6');go(558,'J6');deletecols(560,'J','L');go(561,'M6');deletecols(562,'M','O');go(563,'G6')
# Source operations cleaning: each unmerge/cut/delete is an actual native action.
go(564,'G6:L6');cols(564.5,'G','L');go(565,'G6:I6');format_cell(566,'G1:I1048576',unmerge=True)
go(571,'J5');format_cell(572,'J1:L1048576',unmerge=True);go(574,'K1');go(574.5,'M1');format_cell(575.5,'M1:O1048576',unmerge=True)
go(578,'I1');go(578.5,'F1');cutcolumn(579.5,'G','K');go(581,'K1');go(581.5,'M1');cutcolumn(582.5,'M','I');deletecols(585,'G','H')
checkpoint(585.5,{'G6':"'2019",'H6':"'2020",'I6':"'2021",'G8':'5648','H8':'5371','I8':'6428'})
for t,a in [(586,'A2:F2'),(586.5,'E3'),(587,'A11:C11'),(587.5,'A8:C8'),(588,'A8:C9'),(589,'A12:C12'),(589.5,'A12:C13'),(591,'A14:C14'),(591.5,'A14:I16'),(593,'A19:C19'),(593.5,'A17:H17'),(594,'A17:I17'),(595.5,'A17:C17'),(596.5,'A17:I17'),(597,'A18:G18'),(597.5,'A18:I19'),(598.5,'A7:I18'),(599,'A11:I18'),(599.5,'A12:I18')]:go(t,a)
# Remaining construction is appended below; no final workbook batch is used.
for t,a in [(600,'A18:I22'),(600.5,'A18:I21'),(601,'H20'),(601.5,'A20:C21'),(602,'A20:I24'),(602.5,'A21:C21'),(604,'A1:F1048576'),(606,'D10:F10')]:go(t,a)
go(607,'A1');key(['Ctrl+A']);go(607.5,'A10:C10');format_cell(608.5,'A1:F1048576',unmerge=True)
for t,a in [(610,'B10'),(610.5,'A9'),(611.5,'G9'),(612,'G8')]:go(t,a)
window(613,MODEL.name)
cell(613.5,'B4','Products:');cell(615,'B5','Services:');cell(616.5,'B6','Total Revenue:')
cell(619,'B8','Cost of Products:')
cell(621.5,'B9','Cost of Services:')
cell(624,'B10','Operating Expenses:')
cell(628.5,'B11','Operating Income:',True)
cell(633.5,'B13','Other Income / (Expense):')
window(637.5,RAW.name)
for t,a in [(639.5,'A20:F20'),(640,'A20:I21'),(641,'B17'),(641.5,'A19:C19'),(642,'A19:I19'),(643,'A19:F20'),(643.5,'A19:I20'),(645,'A17'),(645.5,'A17:I18'),(646.5,'A17:I17')]:go(t,a)
window(648,MODEL.name)
cell(650,'B14','Non-Service Pension Expense:');cell(655,'B15','Interest Income / (Expense):');cell(659,'B16','Pre-Tax Income:',True);cell(661.5,'B18','Income Taxes:')
cell(664.5,'B20','Net Income:',True)
cell(667.5,'B21','(-) NCI Net Income:');cell(673,'B22','Net Income to Parent:',True)
indent(677.5,'B21');go(678,'B16');indent(680,'B13:B15');indent(681.5,'B8:B10');indent(682.5,'B4:B5');go(683,'B5');go(683.5,'B6');key(['Ctrl+B'])
for t,a in [(684,'B6:E6'),(684.5,'B6:G6'),(685,'B6:I6'),(685.5,'B6:J6')]:go(t,a)
evidence(687.5,'Observed QAT Alt7 Borders popup, then Top Border.');key(['Alt','7']);key(['Down','Down','Return'],'tutorial.borderMenu')
go(690,'B10');go(690.5,'B11');go(691,'B11:H11');go(691.5,'B11:J12');border(692.5,'B11:J11');border(695,'B16:J16')
for t,a in [(699,'C19'),(699.5,'B21'),(702,'B2:B18'),(702.5,'B2:B22'),(703.5,'C2')]:go(t,a)
cols(704,'B');key(['Alt','O','C','A'])
external(705.5,'C4',['G8']);paste(718.5,'C4','C4:E5','formulas')
autosum(719.5,'C6',719.5,'=SUM(C4:C5)');visible_bold(719.5,'C6');fill(720.5,'C6:E6');go(721,'C8')
external(722,'C8',['G12'],prefix='=-');go(734,'C8:D9');go(734.5,'C8:F9');paste(735.5,'C8','C8:E9','formulas')
external(736.5,'C10',['G14:G15'],prefix='=-SUM(',suffix=')');fill(744.5,'C10:E10')
autosum(745.5,'C11',745.5,'=SUM(C8:C10)');visible_bold(745.5,'C11');fill(746.5,'C11:E11');values(746.5,{'C11':-11265})
format_cell(749.5,'C8:E11',category='Accounting',decimals=0,symbol='');format_cell(755.5,'C4:E6',category='Accounting',decimals=0,symbol='')
external(758,'C13',['G17']);external(763.5,'C14',['G19']);paste(767.5,'C14','C14:C15','formulas')
autosum(768.5,'C16',769,'=SUM(C13:C15)');values(769,{'C16':-86});visible_bold(769,'C16');paste(772,'C8:E11','C13:E16')
external(773.5,'C18',['G22']);edit(781.5,'C18',"=-'[Otis-10-K-Raw.xls]operations'!G22")
go(782.5,'C20');evidence(783);text('=','worksheet.grid');key(['Escape'],'worksheet.cellEditor');go(785,'C16');key(['F2']);key(['Escape'],'worksheet.cellEditor');go(787,'C11');key(['F2']);key(['Escape'],'worksheet.cellEditor')
local_pointed_formula(788.5,'C11');fill(794.5,'C11:E11');local_pointed_formula(796,'C16');local_pointed_formula(801,'C20')
window(804,RAW.name)
for t,a in [(806,'G23'),(806.5,'G26'),(809.5,'G25'),(810,'G24')]:go(t,a)
window(816,MODEL.name);external(818,'C21',['G24'])
go(826,'C18');go(826.5,'C14');key(['Ctrl+C']);go(827,'C18')
evidence(827.5,'Observed Paste Special Formats from copied C14 onto C18; source tax changes to parentheses.');step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Alt','E','S']},'steps':[cm('pasteSpecial.formats'),ck(['Return'],'pasteSpecial.dialog')]},label='paste-tax-format')
evidence(827.5,'Copied C18 has the actual external clipboard-ownership conflict visible at13:48. The scoped replay adapter replaces clipboard ownership at the external boundary; native CtrlC raises the application warning and retains the within-workbook fallback. This does not fabricate a worksheet mutation.');step({'type':'modal','clipboardOwnershipConflict':True,'open':{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+C']},'steps':[ck([],'@active_modal'),ck(['Return'],'@active_modal')]},label='clipboard-ownership-warning')
go(829,'C16');go(829.5,'C11');go(829.75,'C13');key(['Ctrl+C']);go(830,'C16');go(830.5,'C20');go(831,'C20:C22')
evidence(831.5,'Observed Paste Special Formats from single copied C13 onto C20:C22. C20 temporarily loses bold, then becomes bold again by832.5.');step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Alt','E','S']},'steps':[cm('pasteSpecial.formats'),ck(['Return'],'pasteSpecial.dialog')]},label='paste-net-income-format')
visible_bold(832.5,'C20');go(832.5,'C22')
go(833.5,'B22');go(834.5,'B22:E22');go(835,'B22:H22');go(835.5,'B22:J22');evidence(836.5);key(['Alt','7']);key(['Down','Down','Return'],'tutorial.borderMenu')
autosum(839.5,'C22',840,'=SUM(C20:C21)');visible_bold(840,'C22');go(841,'C13:D16');fill(842,'C13:E22')
window(847.5,RAW.name);go(849,'G23:I24');go(850,'G23:I23');window(851,MODEL.name);go(852,'C20:E20');window(853.5,RAW.name);window(856.5,MODEL.name)
for t,a in [(857.5,'E20'),(859,'C15'),(860,'C14'),(861.5,'C13'),(863,'C15')]:go(t,a)
window(864.5,RAW.name)
for t,a in [(865.5,'G19'),(866,'G20'),(866.5,'G22'),(868.5,'G21:I21')]:go(t,a)
window(869.5,MODEL.name);window(871.5,RAW.name);go(873,'G17:I17');go(879.5,'G19');go(880,'G19:I20');window(881,MODEL.name)
edit(884.5,'C14',"=-'[Otis-10-K-Raw.xls]operations'!G19");edit(887.5,'C15',"=-'[Otis-10-K-Raw.xls]operations'!G20");paste(890.5,'C14:C15','C14:E15','formulas')
values(891,{'C6':13118,'D6':12756,'E6':14298,'C20':1267,'D20':1056,'E20':1420,'C22':1116,'D22':906,'E22':1246})
go(891.5,'C20');go(892,'C22');window(893,RAW.name);go(894,'G23:I24');window(897.5,MODEL.name)
format_cell(900.5,'C4:E4',category='Accounting',decimals=0,symbol='$');format_cell(905,'C22:E22',category='Accounting',decimals=0,symbol='$')
for t,a in [(905.5,'C18'),(906.5,'C4:C5'),(907,'C4:E23'),(907.5,'C4:E22')]:go(t,a)
evidence(908.5);key(['Ctrl+C']);evidence(913.5);key(['Escape']);go(915.5,'C2:E3');go(916,'C2:E2');evidence(926.5);key(['Alt','5'])
cell(933.5,'B24','Balance Sheet:',True);local_pointed_formula(936.5,'C24');fill(939.5,'C24:E24');paste(942.5,'C2:E2','C24:E24');go(943,'B24');go(943.5,'B25');go(944,'B26');go(944.5,'B25')
window(946.5,RAW.name);go(946.5,'H23');evidence(948);key(['Ctrl+PgDown']);evidence(948.5);key(['Ctrl+PgDown']);go(951,'D5:F5')
format_cell(973,'A1:F1048576',unmerge=True);go(975,'E1');go(975.5,'I1');format_cell(977,'G1:I1048576',unmerge=True);go(978.5,'J1');format_cell(980,'J1:L1048576',unmerge=True);format_cell(983,'M1:O1048576',unmerge=True)
cutcolumn(985.5,'G','N')
for t,a in [(987.5,'J1'),(988,'G1'),(988.5,'E1')]:go(t,a)
cols(989,'E');key(['Shift+Right']);cols(990,'E','O');cols(990.5,'E','L');cols(991,'E','K');deletecols(992,'E','L');deletecols(992.5,'D')
cols(993,'A');evidence(993.5);key(['Alt','O','C','A']);width(995,'B',2)
cols(996.5,'C');evidence(997);key(['Alt','E','Escape']);cols(997.5,'B');key(['Shift+Right']);cols(998.5,'B','D');deletecols(999,'B','C')
checkpoint(999,{'B5':"'2020",'C5':"'2021",'B7':'1782','B8':'17','C7':'1565','C8':'1910'})
for t,a in [(999.5,'B4'),(1000,'B6'),(1000.5,'B7'),(1002,'A14'),(1003,'A18'),(1013.5,'A13'),(1014.5,'A19'),(1016,'A14:B19'),(1016.5,'A14:C19'),(1017.5,'A17'),(1018,'A20'),(1027,'A15')]:go(t,a)
evidence(1028);key(['Ctrl+PgDown']);evidence(1028.5);key(['Ctrl+PgDown'])
for t,a in [(1029.5,'A9:C9'),(1030.5,'A1:F1048576'),(1031.5,'A9:C9'),(1032,'A10:C10'),(1032.5,'A9:C9'),(1033.5,'D9:F9')]:go(t,a)
# First source cashflow label unmerge includes observed checkbox toggling off/on/off.
go(1034,'A1:F1048576');evidence(1034.5)
step({'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},'steps':[tab('Alignment'),cm('formatCells.merge'),cm('formatCells.merge'),cm('formatCells.merge'),ck(['Return'],'formatCells.dialog')]},label='cashflow-early-unmerge')
go(1037,'B9');evidence(1046,'Observed direct source tab click; native sheet tab pointer.');step({'type':'mouse_tab','target':'workspace.sheets','tab':'consolidated balance sheets'},label='source-tab')
for t,a in [(1047,'A15:A16'),(1047.5,'A15:A17'),(1048,'A18'),(1048.5,'A17:A18'),(1049.5,'A16'),(1050,'A16:B16'),(1050.5,'A14'),(1051,'A14:C15'),(1052,'A19'),(1052.5,'A19:D20'),(1053,'A19:C19'),(1056.5,'A16:A17'),(1057,'A16:B16'),(1058,'A16:C16'),(1062.5,'A7')]:go(t,a)
window(1065,MODEL.name)
cell(1067,'B26','ASSETS:',True);cell(1069.5,'B27','Cash:');cell(1070.5,'B28','Accounts Receivable:');cell(1073.5,'B29','Inventory & Other:');cell(1076,'B30','Net PP&E, Goodwill & Intangibles:');cell(1082,'B31','Op. Lease Assets:')
cell(1085,'B32','Other Assets:')
cell(1087.5,'B33','Total Assets:',True)
paste(1095.5,'B22:J22','B33:J33');indent(1099,'B27:B32')
external(1100.5,'C27',['B7:B8'],prefix='=SUM(',suffix=')');external(1106.5,'C28',['B9'],anchor=True);external(1109.5,'C29',['B10:B12'],prefix='=SUM(',suffix=')');external(1118,'C30',['B15','+','B17','+','B18']);go(1130.5,'C30');key(['F2'],expect={'cells':[{'address':'C29','style':{'numberFormat':'#,##0'}}]});key(['Escape'],'worksheet.cellEditor');external(1132.5,'C31',['B16']);external(1136.5,'C32',['B14','+','B19']);autosum(1144.5,'C33',1145,'=SUM(C27:C32)')
paste(1154.5,'C13','C27:C32');format_cell(1155.5,'C27',category='Accounting',decimals=0,symbol='$')
window(1164,RAW.name);go(1164,'B5:C5');window(1173,MODEL.name);go(1173.5,'C27:C33');key(['Ctrl+X']);go(1174.5,'D27');key(['Ctrl+V'],label='correct-history-year-column')
format_cell(1176.5,'C32',category='General');fill(1179,'D27:E33');values(1179.5,{'D33':10710,'E28':3148,'E33':12195})
window(1183,RAW.name);go(1183,'C20');window(1189,MODEL.name);go(1189,'E28');key(['F2']);key(['Escape'],'worksheet.cellEditor');edit(1191.5,'D28',"='[Otis-10-K-Raw.xls]consolidated balance sheets'!B9");paste(1194,'D28','D28:E28','formulas');values(1194.5,{'D28':3148,'E28':3232,'D33':10710,'E33':12279})
# Liability and equity labels and formulas, including initial corrected drafts.
window(1200,RAW.name)
for t,a in [(1201,'A22'),(1202,'A23:C25'),(1207,'A22:C32'),(1213,'A26:C26'),(1224,'A27:C28'),(1228,'A29:C32'),(1240,'A35:C44')]:go(t,a)
window(1245,MODEL.name)
cell(1248,'B35','LIABILITES & EQUITY:',True);cell(1252,'B36','Accounts Payable:');cell(1254,'B37','Accrued Liabilities:');cell(1258,'B38','Contract Liabilities:');cell(1261,'B39','Total Debt:');cell(1263.5,'B40','Op. Lease Liabilities:');cell(1267,'B41','Other Liabilities:');cell(1270,'B42','Total Liabilities:',True)
indent(1275,'B36:B41');go(1277,'B32:E33');paste(1278,'B33:J33','B42:J42');border(1280.5,'C41','bottom')
cell(1283,'B44',"Common Shareholders' Equity:");cell(1288,'B45','Noncontrolling Interests:');cell(1293,'B46','Total Equity:',True);cell(1298,'B48','TOTAL LIABILITIES + EQUITY:',True);cell(1302,'B50','Balance Check:');indent(1308,'B44:B45');paste(1312,'B42:J42','B46:J46')
external(1315,'D36',['B23']);evidence(1322.5);key(['Ctrl+C']);go(1323,'D36:D38')
evidence(1324,'Observed accidental blank native Find and Replace dialog. The opening chord is inferred; no search or workbook mutation is performed.');key(['Ctrl+F'])
evidence(1324.5);step({'type':'mouse','target':'findReplace.close','gesture':'click'},label='close-accidental-find');go(1324.5,'C37');go(1325,'D36');evidence(1325.5);key(['Ctrl+C']);go(1325.5,'D36:D37');go(1326,'D36:E38');paste(1326.5,'D36','D36:E38','formulas')
external(1328,'D39',['B22','+','B28']);external(1336.5,'D40',['B30']);external(1343,'D41',['B29','+','B31','+','B32']);autosum(1358,'D42',1358.5,'=SUM(D36:D41)');window(1359.5,RAW.name);go(1363.5,'B33');window(1366,MODEL.name);go(1367,'D41');go(1367.5,'D39');go(1368.5,'D39:D41');fill(1369.5,'D39:E42');go(1371,'D27');go(1371.5,'D27:D28');go(1372,'D27:E33');paste(1373.5,'D27:E33','D36:E42');go(1374,'D37:D39');go(1374.5,'D37');go(1375.5,'D44')
window(1377,RAW.name);go(1378,'B33:C33');go(1379,'B36');window(1380.5,MODEL.name);external(1381,'D44',['B43']);go(1387.5,'D44:E44');external(1388.5,'D45',['B35','+','B44']);autosum(1402,'D46',1402.5,'=SUM(D44:D45)');go(1403,'D44:D46');go(1403.5,'D44:F46');go(1404,'D44:E46');go(1404.5,'D45');go(1405,'D44');key(['Escape']);fill(1406.5,'D44:E46');go(1407.5,'D40:E41');paste(1408,'D40:E42','D44:E46');border(1410.5,'C45','bottom')
local_pointed_formula(1412.5,'D48',[(1413,'point','D44'),(1413.5,'point','D42'),(1413.5,'text','+'),(1414,'point','D46')],1414.5,'=D42+D46')
format_cell(1416,'D42:E42',category='Accounting',decimals=0,symbol='');format_cell(1419,'D48',category='Accounting',decimals=0,symbol='$');go(1419,'D48');key(['Ctrl+B']);fill(1420,'D48:E48')
local_pointed_formula(1425,'D50',[(1425,'point','D48'),(1425.5,'point','D44'),(1426,'point','D33'),(1426.5,'text','-'),(1427,'point','D48')],1427.5,'=D33-D48');fill(1428,'D50:E50')
go(1434,'B2:J2');evidence(1437,'Observed Alt6 fill palette and gray live preview; keyboard navigation is a native replay adaptation for the observed theme swatch.');key(['Alt','6']);key(['Down','Down'],'tutorial.fillPalette');evidence(1443);key(['Return'],'tutorial.fillPalette');border(1443.5,'B2:J2','bottom');paste(1448,'B2:J2','B24:J24');go(1449,'D25');go(1452.5,'B52');save(1454)
window(1457.5,RAW.name);go(1457.5,'B36');evidence(1459);key(['Ctrl+PgDown']);evidence(1459.5);key(['Ctrl+PgDown'])
go(1461,'G7:I7');go(1463,'I7');cols(1463.5,'G','I');go(1465,'G6:I6');format_cell(1466.5,'G1:I1048576',unmerge=True)
for t,a in [(1469,'B5'),(1469.5,'A5'),(1470,'C5'),(1470.5,'E5'),(1471,'H5')]:go(t,a)
cols(1471.5,'J','L');format_cell(1473,'J1:O1048576',unmerge=True);go(1475,'S5:U5');go(1475.5,'V5');go(1476,'P5:R5');format_cell(1477,'P1:U1048576',unmerge=True)
go(1479,'N5');go(1479.5,'E5');go(1480,'A5');cols(1480.5,'H','J');deletecols(1481.5,'H','L');cols(1482.5,'I','J');deletecols(1483.5,'I','M')
for t,a in [(1484,'A5'),(1484.5,'B5'),(1485,'D5'),(1485.5,'G5')]:go(t,a)
cols(1486,'G');go(1487,'H5');cutcolumn(1488,'H','J');cutcolumn(1490.5,'G','K')
for t,a in [(1492.5,'H3'),(1493,'G5'),(1493.5,'F3'),(1494,'D2'),(1494.5,'C2')]:go(t,a)
cols(1495,'A');key(['Alt','O','C','A']);cols(1497.5,'B');cols(1498,'B','D');cols(1498.5,'B','G');deletecols(1499.5,'B','H');go(1501,'A2');go(1503.5,'B5');cols(1506.5,'B');key(['Ctrl++'],label='insert-spacer-column');evidence(1508);key(['Alt','O'],label='legacy-width-banner');width(1508.5,'B',2,legacy_started=True);go(1509.5,'A5');go(1510.5,'C5')
checkpoint(1510.5,{'C5':"'2019",'D5':"'2020",'E5':"'2021",'C7':'1267','D7':'1056','E7':'1420'})
for t,a in [(1516,'A11'),(1517,'C7:C8'),(1517.5,'C7:E10'),(1518.5,'D9'),(1519,'C9:E10'),(1520.5,'D12'),(1521,'C10'),(1521.5,'C10:E11'),(1523,'D13'),(1524,'C12:F13'),(1525,'C16:E19'),(1525.5,'C16:E20'),(1526,'C16:E22'),(1527.5,'C24:E25'),(1530,'A28:D29'),(1530.5,'A28:E28'),(1531,'A29:A31'),(1531.5,'A29:A30'),(1532,'A29:E30'),(1538,'A36')]:go(t,a)
rows(1539.5,36);rows(1540,36,38);rows(1543,36,39);go(1545,'A38');go(1545.5,'A40');rows(1546,40,41);go(1547.5,'A41');go(1548,'A43');rows(1548.5,43,44);rows(1549.5,43,45);go(1551,'A45');rows(1551.5,46);go(1556.5,'A48');go(1558.5,'A50');go(1559,'A54');go(1559.5,'A54:C54');go(1560,'A54:E55')
rows(1563,50,53);key(['Ctrl+-'],label='remove-four-blank-rows');save(1565)
window(1589,MODEL.name)
observed_header_anchors(True)
local_pointed_formula(1598,'F2');paste(1603,'E2','F2');fill(1604.5,'F2:J2');local_pointed_formula(1607,'F24');go(1611,'F24');key(['Ctrl+B']);key(['Alt','5']);fill(1613.5,'F24:J24')
observed_header_anchors(False)
paste(1624.5,'B24:J25','B52:J53','all');cell(1625,'B52','Cash Flow Statement:');cell(1629,'B53','Cash Flow from Operations:',True);cell(1633,'B54','Net Income to Parent:');visible_bold(1647.5,'B54');indent(1648,'B54')
cell(1650,'B55','(+/-) Reversal of NCI Net Income:');indent(1661.5,'B55');go(1666,'B55')
autofit_b55_view={'row':55,'column':2,'nameBoxText':'B55','topRow':43,'topRowOffset':0}
evidence(1666,'Source1666.000 retains singletonB55/top43 while columnB fits the label. The native AutoFit shortcut is inferred; no fabricatedB1 navigation or whole-column selection is required.')
key([],expect=autofit_b55_view,label='whole-columns');key(['Alt','O','C','A'],expect=autofit_b55_view)
cell(1672,'B56','(+) D&A:');indent(1676.5,'B56')
cell(1681,'B57','Pensions/Other Items:');indent(1685.5,'B57');edit(1687,'B57','(+/-) Pensions/Other Items:')
window(1694,RAW.name);go(1694,'A17');go(1695,'A16');go(1696,'A16:E20');window(1715,MODEL.name)
cell(1716,'B58','(+/-) Change in WC & Leases:');indent(1724,'B58');cell(1725,'B59','Cash Flow from Operations:',True);border(1729.5,'B59:J59')
cell(1733,'B61','Cash Flow from Investing:',True);cell(1738,'B62','(-) CapEx:');cell(1741,'B63','(-) Acquisitions & Other:');indent(1747,'B62:B63');cell(1748,'B64','Cash Flow from Investing:',True);cell(1752,'B66','Cash Flow from Financing:',True);cell(1756,'B67','(+/-) Change in Debt:');cell(1766,'B68','(-) Common Dividends:');cell(1770,'B69','(-) Stock Repurchases:');cell(1773,'B70','(-) Dividends to NCI:');cell(1785,'B71','(+/-) Other Items:');cell(1791,'B72','Cash Flow from Financing:',True)
paste(1796.5,'B59:J59','B64:J64');go(1797.5,'B1048576');paste(1798.5,'B64:J64','B72:J72');indent(1801,'B67:B71')
local_pointed_formula(1803,'C54',[(1805,'point','C22')],1805.5,'=C22');local_pointed_formula(1807,'C55',[(1807,'text','-'),(1809,'point','C21')],1810,'=-C21');external(1813,'C56',['C9']);external(1822,'C57',['C10','+','C11','+','C12','+','C24','+','C25']);external(1843,'C58',['C16:C21'],prefix='=SUM(',suffix=')');autosum(1849.5,'C59',1849.5,'=SUM(C54:C58)');paste(1853,'D38:D42','C55:C59')
window(1856.5,RAW.name);go(1857,'C23');go(1861,'A29');go(1862,'A34');window(1863,MODEL.name);go(1863,'C61');external(1863.5,'C62',['C28']);external(1871,'C63',['C29:C33'],prefix='=SUM(',suffix=')');autosum(1881,'C64',1882.5,'=SUM(C62:C63)',inspection=(1882,1882.5,'A34'));go(1883,'C63');go(1883.5,'C62:C63');go(1884,'C62:E64');fill(1884.5,'C62:D64');fill(1885,'C62:E64')
window(1886,RAW.name);go(1886,'A34');window(1887,MODEL.name);window(1888.5,RAW.name);window(1889.5,MODEL.name);window(1891.5,RAW.name);go(1892,'B36');go(1899,'C34');go(1900,'B36');window(1901.5,MODEL.name);go(1901.5,'C67');window(1903.5,RAW.name);go(1905,'A42');window(1905.5,MODEL.name)
external(1907,'C67',['C36:C39',')','+','C41'],prefix='=SUM(');values(1931.5,{'C67':'#VALUE!'})
go(1932,'C68');go(1932.5,'C67');key(['F2']);key(['Escape'],'worksheet.cellEditor');window(1937.5,RAW.name)
# These are source value edits, preserving the original model formula and its
# preceding #VALUE! state. They are deliberately individual native transactions.
for t,a in [(1942.5,'C37'),(1942.5,'D37'),(1944,'C38'),(1944,'D38'),(1945,'C39'),(1946,'C40'),(1946.5,'C41')]:cell(t,a,'0')
go(1947,'D41');cell(1947.5,'E41','0');cell(1949,'C43','0');cell(1949.5,'C44','0');cell(1950,'D44','0');go(1951,'D45');go(1951.5,'E47');cell(1953,'E46','0');cell(1955.5,'C30','0');go(1956.5,'C35');window(1957.5,MODEL.name);values(1957.5,{'C67':6})
external(1959.5,'C68',['C43']);external(1964.5,'C69',['C44']);external(1969,'C70',['C45']);external(1974.5,'C71',['C40','+','C46','+','C47']);autosum(1993.5,'C72',1994,'=SUM(C67:C71)');window(1994.5,RAW.name);go(1994.5,'C35');window(1999,MODEL.name)
paste(2000.5,'C55:C59','C68:C72');paste(2001.5,'C68','C67');go(2003,'C56:C58');go(2003.5,'C57:C59');go(2004,'C62:F64');paste(2005,'C57:C59','C62:E64')
cell(2009.5,'B74','FX Rate Effects:');cell(2012.5,'B76','Net Change in Cash:',True);cell(2015.5,'C74','FX Rate Effects:',commit='Tab');go(2016.5,'C74');key(['Delete']);external(2017.5,'C74',['C50'])
go(2025.5,'C76');text('=','worksheet.grid')
for t,a in [(2026,'C67'),(2026.5,'C62'),(2027,'C59')]:wheelto(t,a);pointer(t,a)
evidence(2027.5);text('+','formula.input');wheelto(2027.5,'C72');pointer(2027.5,'C72');wheelto(2028,'C64');pointer(2028,'C64');evidence(2028.5);text('+','formula.input');pointer(2029,'C72');evidence(2029.5);text('+','formula.input');pointer(2030,'C74');evidence(2030.5);key(['Return'],'formula.input',{'cells':[{'address':'C76','input':'=C59+C64+C72+C74'}]},'commit-net-cash-corrected-draft');go(2031,'C76');key(['Ctrl+B'])
go(2033.5,'C54:D70');go(2034,'C54:D74');go(2034.5,'C54:D76');go(2035,'C54:C76');fill(2036.5,'C54:E76');values(2036.5,{'E57':'#VALUE!','E59':'#VALUE!','E76':'#VALUE!'});go(2038,'E76');go(2039,'D57');key(['F2']);key(['Escape'],'worksheet.cellEditor');go(2041,'E57');key(['F2']);key(['Escape'],'worksheet.cellEditor');go(2042,'D57');window(2042.5,RAW.name);go(2042.5,'D10');go(2045,'E12');cell(2049.5,'E12','0');window(2051.5,MODEL.name);values(2051.5,{'E57':-33,'E59':1750,'E76':1676})
go(2052.5,'E59');go(2054,'C76');paste(2056.5,'C62:E62','C74:E74');go(2057.5,'C76');save(2058);window(2059.5,RAW.name);go(2062.5,'A54');go(2063.5,'C51:E52');go(2064,'C51:E51');window(2069,MODEL.name);go(2073,'D76');window(2075.5,RAW.name);go(2076,'C52');go(2077,'D52');go(2077.5,'E51');evidence(2077.5,'Observed sourcecashflow viewport retainsE51 withtopRow37 beforehandoff. Replay reaches it with realnative scrollbar/wheel events.');step({'type':'vertical_view','topRow':37},{'topRow':37,'firstColumn':1,'row':51,'column':5,'sheetName':'cash flows','nameBoxText':'E51'},'source-handoff-viewport');window(2080,MODEL.name)
for t,a in [(2082.5,'C76'),(2083,'D76'),(2083.5,'E76')]:go(t,a);key(['F2']);key(['Escape'],'worksheet.cellEditor')
go(2084,'C76');go(2086,'C54');go(2092,'E61');go(2092.5,'E57');go(2094,'C54:E57')
for t,a in [(2094.5,'E56'),(2095.5,'D56'),(2096,'C56'),(2096.5,'D56'),(2097,'D54'),(2097.5,'E55'),(2098,'E57'),(2098.5,'E58'),(2100.5,'E67'),(2108,'D29'),(2109,'D9'),(2109.5,'B1'),(2110,'B2')]:go(t,a)
values(2110.5,{'D50':0,'E50':0,'C59':1469,'D59':1480,'E59':1750,'C64':-203,'D64':-353,'E64':-89,'C72':-1133,'D72':-844,'E72':58,'C74':-20,'D74':59,'E74':-43,'C76':113,'D76':342,'E76':1676})

def seconds_of(record):
 return sum(float(part)*60**i for i,part in enumerate(reversed(record['videoTime'].split(':'))))
def records_in(records):
 for index,record in enumerate(records):
  yield records,index,record
  yield from records_in(record['action'].get('steps',[]))
def source_match(record,match):
 if 'recordIdByMode' in match and record['id']!=match['recordIdByMode']['include-intro' if args.include_intro else 'default']:return False
 if 'videoSeconds' in match and abs(seconds_of(record)-match['videoSeconds'])>0.0005:return False
 if 'sourceEventId' in match and record['evidence'].get('eventId')!=match['sourceEventId']:return False
 for field,value in match.items():
  if field in ['videoSeconds','sourceEventId','occurrence','selectionAddress','recordIdByMode']:continue
  actual=record['action'].get('open',{}).get('keys') if field=='openKeys' else record['action'].get(field)
  if actual!=value:return False
 return True
def source_find(match,selection=False):
 matches=[]
 for records,index,record in records_in(steps):
  if not source_match(record,match):continue
  if selection:
   if record['action']!={'type':'text','target':'formula.name','text':match['selectionAddress']} or index==0 or index+1>=len(records):continue
   before,after=records[index-1],records[index+1]
   if before['action']!={'type':'key_sequence','target':'formula.name','keys':['Ctrl+A']} or after['action']!={'type':'key_sequence','target':'formula.name','keys':['Return']}:continue
   if any(abs(seconds_of(other)-seconds_of(record))>0.0005 for other in [before,after]):continue
   matches.append((records,index-1,[before,record,after]))
  else:matches.append((records,index,[record]))
 if 'occurrence' in match:
  occurrence=match['occurrence']
  if not isinstance(occurrence,int) or occurrence<0 or occurrence>=len(matches):raise ValueError('Source patch occurrence does not exist: '+str(match))
  return matches[occurrence]
 if len(matches)!=1:raise ValueError('Source patch must match exactly once, found '+str(len(matches))+': '+str(match))
 return matches[0]
def patch_evidence(record,patch):
 t=patch.get('seconds',seconds_of(record));evidence(t,patch.get('method','Source-refined native action timing; input method is adapted where physical keys are unobservable.'))
 source=next(row for row in initial+history if row['id']==patch['sourceEventId'])
 current_evidence.update(ledger='initial-model-visual-steps.json' if source in initial else 'historical-statements-visual-steps.json',eventId=source['id'],sourceFrame=str(EVIDENCE/source['sourceFrame']),observed=source['observation'])
 if patch.get('sourceFrame'):current_evidence['sourceFrame']=patch['sourceFrame']
 record['videoTime']=f'{int(t//60):02d}:{t%60:06.3f}';record['evidence']=dict(current_evidence)
 if 'expect' in patch:record['expect']=patch['expect']
applied_source_patches=[]
for name in ['historical-semantic-0001-0700-action-patches.json','historical-semantic-0701-1412-action-patches.json']:
 path=EVIDENCE/name
 if not path.exists():continue
 for patch in json.loads(path.read_text())['patches']:
  operation=patch['operation'];affected=[]
  if operation=='insert':
   parent=source_find(patch['parentMatch'])[2][0] if patch.get('parentMatch') else None
   if parent is not None and parent['action']['type']!='modal':raise ValueError('A source child insertion requires an actual modal')
   evidence(patch['seconds']);record=child(patch['action'],'source-insert');patch_evidence(record,patch);(parent['action']['steps'] if parent else steps).append(record);affected=[record['id']]
  else:
   records,index,matched=source_find(patch['match'],operation in ['retime_selection','remove_selection']);affected=[record['id'] for record in matched]
   if operation in ['remove','remove_selection']:del records[index:index+len(matched)]
   elif operation in ['retime','retime_selection','replace']:
    for record in matched:
     if operation=='replace':record['action']=patch['action']
     patch_evidence(record,patch)
   else:raise ValueError('Unknown source patch operation '+operation)
  applied_source_patches.append({'ledger':name,'operation':operation,'sourceEventId':patch['sourceEventId'],'actionIds':affected})
# A visible dimension header belongs to an actual held pointer/keyboard phase.
# Replace only independently reviewed NameBox triplets, never a time-window match.
live_selection_path=EVIDENCE/'historical-live-selection-phases.json'
if live_selection_path.exists():
 for group in json.loads(live_selection_path.read_text())['entries']:
  for match in group.get('removeActions',[]):
   records,index,matched=source_find(match)
   del records[index:index+len(matched)]
  for match in group.get('replacedSelections',[]):
   records,index,matched=source_find(match,True)
   del records[index:index+len(matched)]
  handoff=group.get('modalHandoff');parent=None
  if handoff:
   parent=source_find(handoff['match'])[2][0]
   if parent['action']['type']!='modal':raise ValueError('Held selection handoff requires an actual modal')
   if handoff.get('holdModifiers'):parent['action']['open']['releaseModifiers']=False
  for phase in group['phases']+(handoff.get('phases',[]) if handoff else []):
   evidence(phase['seconds'],phase.get('method',group['method']))
   source=next(row for row in history if row['id']==phase['sourceEventId'])
   current_evidence.update(ledger='historical-statements-visual-steps.json',eventId=source['id'],sourceObservationIds=group['sourceEventIds'],phaseLedger=live_selection_path.name,phaseGroup=group['id'],sourceFrame=str(EVIDENCE/phase.get('sourceFrame',source['sourceFrame'])),observed=source['observation'])
   if handoff and phase in handoff.get('phases',[]):
    record=child(phase['action'],'live-selection-modal')
    if phase.get('expect'):record['expect']=phase['expect']
    parent['action']['steps'].append(record)
   else:step(phase['action'],phase.get('expect'),'live-selection')
 # Native popup and external Point headers retain their independently observed
 # surface state; expectations inspect real controls without setting UI text.
 header_review=json.loads(live_selection_path.read_text())
 for check in header_review.get('pointRangeExpectations',[])+header_review.get('popupHeaderExpectations',[]):
  record=source_find(check['match'])[2][0]
  record.setdefault('expect',{}).update(check['expect'])
  record['evidence']['headerSurfaceLedger']=live_selection_path.name
def sort_source_children(records):
 records.sort(key=seconds_of)
 for record in records:sort_source_children(record['action'].get('steps',[]))
sort_source_children(steps)
# Source-only viewport changes are independent actions, including intermediate
# scroll positions inside a single observation. Do not reselect cells to scroll:
# the real scrollbar/wheel adapter preserves current selection and formula draft.
viewport_review=json.loads((EVIDENCE/'historical-viewport-visual-steps.json').read_text())
for viewport in viewport_review['events']:
 evidence(viewport['videoSeconds'],viewport['method'])
 current_evidence.update(ledger='historical-statements-visual-steps.json',eventId=viewport['sourceEventId'],sourceFrame=viewport['sourceFrame'],observed=viewport['observed'],viewportLedger='historical-viewport-visual-steps.json',viewportEventId=viewport['id'])
 document=RAW.name if viewport['document']=='raw' else MODEL.name
 step({'type':'activate_window','documentName':document},label='viewport-window')
 coordinate='topRow' if viewport['type']=='vertical_view' else 'firstColumn'
 expected=viewport[coordinate] if coordinate=='topRow' else sum((ord(c)-64)*26**i for i,c in enumerate(reversed(viewport[coordinate])))
 action={'type':viewport['type'],coordinate:viewport[coordinate]};check={coordinate:expected}
 if 'topRowOffset' in viewport:action['topRowOffset']=viewport['topRowOffset'];check['topRowOffset']=viewport['topRowOffset']
 step(action,check,label='source-viewport')
if args.checkpoint_save:
 evidence(2110.5,'Interim handoff persistence only: save the actual cleaned source and model through native Ctrl S. These saves are not asserted as source-video events and are omitted from the final same-process combined replay.',True)
 step({'type':'activate_window','documentName':RAW.name},label='checkpoint-source-window');key(['Ctrl+S']);step({'type':'activate_window','documentName':MODEL.name},label='checkpoint-model-window');key(['Ctrl+S'])

# Append independently reviewed observations after existing emission so adding
# a read-only checkpoint cannot renumber any previously authored action.
for check in header_review.get('readOnlyCheckpoints',[]):
 evidence(check['seconds'],check['method'])
 source=next(row for row in history if row['id']==check['sourceEventId'])
 current_evidence.update(ledger='historical-statements-visual-steps.json',eventId=source['id'],sourceFrame=str(EVIDENCE/check['sourceFrame']),observed=check['observed'],phaseLedger=live_selection_path.name,sourceCheckpointId=check['id'])
 step(check['action'],check['expect'],'source-checkpoint')

# A retained draft checkpoint can outlive its former coarse commit timestamp.
# Explicit identities appended here preserve every existing emitted action ID.
for phase in text_phases.values():
 for check in phase.get('readOnlyCheckpoints',[]):
  evidence(check['seconds'],check['method'])
  source=next(row for row in history if row['id']==check['sourceEventId'])
  current_evidence.update(ledger='historical-statements-visual-steps.json',eventId=source['id'],sourceFrame=str(ROOT/check['sourceFrame']),observed=check['observed'],phaseLedger=phase['phaseLedger'])
  assert not any(record['id']==check['id'] for record in steps),check['id']
  steps.append({'id':check['id'],'videoTime':f"{int(check['seconds']//60):02d}:{check['seconds']%60:06.3f}",'evidence':dict(current_evidence),'action':check['action'],'expect':check['expect']})

# Modal dimension headers require the real held selection to survive its opener.
# Apply these exact source patches after emission to preserve every unrelated ID.
modal_header_path=EVIDENCE/'historical-modal-header-phases.json'
if modal_header_path.exists():
 def modal_header_evidence(entry,event_id=None,source_frame=None):
  source=next(row for row in initial+history if row['id']==(event_id or entry['sourceEventId']))
  return {'kind':'visual','ledger':'initial-model-visual-steps.json' if source in initial else 'historical-statements-visual-steps.json','eventId':source['id'],'sourceFrame':str(EVIDENCE/(source_frame or source['sourceFrame'])),'observed':source['observation'],'phaseLedger':modal_header_path.name,'replayMethod':entry['method']}
 def modal_header_record(entry,suffix,seconds,action,expect,event_id=None,source_frame=None):
  return {'id':'historical-modal-header-'+entry['sourceEventId']+'-'+suffix,'videoTime':f'{int(seconds//60):02d}:{seconds%60:06.3f}','evidence':modal_header_evidence(entry,event_id,source_frame),'action':action,'expect':expect}
 for entry in json.loads(modal_header_path.read_text())['entries']:
  parent=source_find(entry['modalMatch'])[2][0]
  parent['action']['open']['releaseModifiers']=False
  children=parent['action']['steps']
  children[0].setdefault('expect',{})['nameBoxText']=entry['count']
  children[0]['evidence']['modalHeaderLedger']=modal_header_path.name
  for match in entry.get('removedSelections',[]):
   records,index,matched=source_find(match,True);del records[index:index+len(matched)]
  for match in entry.get('removeActions',[]):
   records,index,matched=source_find(match);del records[index:index+len(matched)]
  if entry.get('replacedSelection'):
   records,index,matched=source_find(entry['replacedSelection'],True)
   records[index:index+len(matched)]=[modal_header_record(entry,phase['suffix'],phase['seconds'],phase['action'],phase['expect'],phase.get('sourceEventId'),phase.get('sourceFrame',entry['sourceFrame'])) for phase in entry['selectionPhases']]
  release=entry['release']
  if release.get('moveMatch'):
   records,index,matched=source_find(release['moveMatch']);record=records.pop(index)
   record.update(videoTime=f"{int(release['seconds']//60):02d}:{release['seconds']%60:06.3f}",evidence=modal_header_evidence(entry,release['sourceEventId'],release['sourceFrame']),action={'type':'modifier_release','target':'pasteSpecial.dialog','keys':['Shift']})
   record['expect']['nameBoxText']=entry['anchor']
  else:record=modal_header_record(entry,'release',release['seconds'],{'type':'modifier_release','target':'pasteSpecial.dialog','keys':['Shift']},{'nameBoxText':entry['anchor']},release['sourceEventId'],release['sourceFrame'])
  acceptance=next(index for index,child_record in enumerate(children) if child_record['action'].get('keys')==['Return'])
  children.insert(acceptance,record)
 sort_source_children(steps)

# Dense source drafts and Ready headers are refined after existing emission so
# exact source clock corrections and stable additions cannot renumber old IDs.
zero_phase_path=EVIDENCE/'historical-cashflow-zero-phases.json'
selection_followup_path=EVIDENCE/'historical-final-selection-phases.json'
early_ready_path=EVIDENCE/'historical-early-ready-header-phases.json'
early_held_path=EVIDENCE/'historical-early-held-selection-phases.json'
remaining_range_path=EVIDENCE/'historical-remaining-range-header-phases.json'
remaining_early_range_path=EVIDENCE/'historical-remaining-range-early-header-phases.json'
bar_commit_path=EVIDENCE/'historical-bar-commit-header-phases.json'
for zero_phase_path in [zero_phase_path,selection_followup_path,early_ready_path,early_held_path,remaining_range_path,remaining_early_range_path,bar_commit_path]:
 if not zero_phase_path.exists():continue
 for group in json.loads(zero_phase_path.read_text())['entries']:
  def zero_evidence(record,phase):
   patch=dict(phase);patch.setdefault('method',group['method'])
   if patch.get('sourceFrame'):
    p=Path(patch['sourceFrame']);patch['sourceFrame']=str(p if p.is_absolute() else EVIDENCE/p)
   patch_evidence(record,patch)
   record['evidence'].update(phaseLedger=zero_phase_path.name,phaseGroup=group['id'],sourceObservationIds=group['sourceEventIds'])
   if phase.get('observed'):record['evidence']['observed']=phase['observed']
   if phase.get('sourceState'):record['evidence']['sourceState']=phase['sourceState']
  for patch in group['updates']:
   records,index,matched=source_find(patch['match'],patch['operation'] in ['retime_selection','remove_selection'])
   if patch['operation'] in ['remove','remove_selection']:
    del records[index:index+len(matched)];continue
   for record in matched:
    if patch.get('action'):record['action']=patch['action']
    zero_evidence(record,patch)
  for phase in group.get('additionalPhases',[]):
   assert not any(record['id']==phase['id'] for record in steps),phase['id']
   record={'id':phase['id'],'videoTime':f"{int(phase['seconds']//60):02d}:{phase['seconds']%60:06.3f}",'action':phase['action']}
   zero_evidence(record,phase)
   if phase.get('beforeActionByMode'):
    anchor=source_find({'recordIdByMode':phase['beforeActionByMode']})[2][0]
    steps.insert(steps.index(anchor),record)
   else:steps.append(record)
 sort_source_children(steps)

# Stable ordering keeps same-time native selections/edits ahead of their source
# viewport check while inserting a scroll between its actual neighboring edits.
steps.sort(key=lambda record:sum(float(part)*60**i for i,part in enumerate(reversed(record['videoTime'].split(':')))))

# Completion here means only the explicit replay segment. It cannot claim full
# source completion while fixture adapters, provisional widths, intro context,
# annotations and unsampled physical commands remain distinguished.
manifest={'version':1,'sourceVideo':'https://www.youtube.com/watch?v=O2uPNfErmJo','windowSize':[1280,720],
 'scope':('Native workbook inspection and historical construction replay0:20–35:11 in progress. ' if args.include_intro else 'Native ordered historical construction replay6:40–35:11 in progress. ')+'Every mutation goes through native UI; no final model cloning.',
 'sourceReviewComplete':False,'preparedThrough':f'{int(min(args.through,2110.5)//60):02d}:{min(args.through,2110.5)%60:06.3f}','workspace':str(WORK),
 'appliedSourcePatches':applied_source_patches,
 'sourceEvidence':['intro-visual-steps.json','intro-inspection-native-viewport-boundaries.json','intro-context-visual-steps.json','initial-model-visual-steps.json','initial-model-width-boundary.json','historical-statements-visual-steps.json','historical-viewport-visual-steps.json','historical-text-phases-0001-0700.json','historical-text-phases-0701-1412.json','historical-local-formula-phases-0001-0700.json','historical-local-formula-phases-0701-1412.json','historical-header-anchor-phases.json','historical-semantic-0001-0700-action-patches.json','historical-semantic-0701-1412-action-patches.json','c62-namebox-native-boundary.json','historical-formula-edit-phases.json','historical-positive-nci-boundary.json','initial-model-title-phases.json','historical-live-selection-phases.json','historical-modal-header-phases.json'],
 'knownNativeStateGaps':[{'sourceEventId':'history-1194','boundaryLedger':'c62-namebox-native-boundary.json','observed':'Name Box changes atomically SUM to A100 after an attempted Complete activation while the origin bare-equals draft and worksheet stay painted.','status':'The genuine three-window attempt and return execute; the unexplained Excel display residue is not fabricated or claimed reproduced.'}],
 'methodCaveats':['Native fixture opening and initial view configuration are explicit acceptance preparation.','The opening completed-file demonstration differs from the published completed reference; only its isolated copy receives disclosed native UI preparation, with no contents entering blank-model construction.','Name Box navigation and explicit dialog control clicks adapt visible targets when physical source keys are unobservable.','Initial B widening uses inferred legacy AutoFit command from thirtyfps banner/outcome evidence.','The actual clipboard warning uses a scoped external ownership conflict and the native Copy failure path. Presenter annotations and Word/PDF/slide switches have separate early-intro/early-history visual ledgers and context-generator actions; only the combined manifest establishes their integration.','This executable pass still requires comparison and refinement against every retained observation; it does not claim every source event executed.'],
 'steps':[s for s in steps if s['videoTime']=='synthetic acceptance' or float(s['videoTime'].split(':')[0])*60+float(s['videoTime'].split(':')[1])<=args.through]}
for phase_path in [EVIDENCE/'historical-cashflow-zero-phases.json',selection_followup_path,early_ready_path,early_held_path,remaining_range_path,remaining_early_range_path,bar_commit_path]:
 if phase_path.exists():manifest['sourceEvidence'].append(phase_path.name)
out=Path(args.output) if args.output else EVIDENCE/'historical-native-action-manifest.json';out.write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({'manifest':str(out),'workspace':str(WORK),'steps':len(manifest['steps'])}))
