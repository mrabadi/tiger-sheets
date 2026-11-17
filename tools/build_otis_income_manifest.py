import json
from decimal import Decimal
import re
from pathlib import Path
root=Path('docs/validation/otis-90-minute')
source=json.loads((root/'income-projections-visual-steps.json').read_text())
live_selections=json.loads((root/'income-live-selection-phases.json').read_text())
width_phases=json.loads((root/'income-width-modal-phases.json').read_text())
steps=[];mapped=set()
def event(t,kind=None):
 matches=[e for e in source['events'] if e['time']==t and (kind is None or e['kind']==kind)]
 assert len(matches)==1,(t,kind,len(matches));return matches[0]
def emit(t,slug,action,expect=None,kind=None,method=None):
 e=event(t,kind);mapped.add(e['id']);d={'id':f'{e["id"]}-{slug}','videoTime':t,'evidence':{'kind':'visual','ledger':'income-projections-visual-steps.json','eventId':e['id'],'frames':e['evidence_frames'],'observed':e['observed']},'action':action}
 if method:d['evidence']['executionMethod']=method
 if expect:d['expect']=expect
 steps.append(d)
def key(t,slug,keys,target='worksheet.grid',expect=None,kind=None):emit(t,slug,{'type':'key_sequence','target':target,'keys':keys},expect,kind)
def click(t,cell,slug='select',kind=None):
 emit(t,f'{slug}-expose-{cell}',{'type':'wheel_to_cell','address':cell},kind=kind)
 emit(t,f'{slug}-{cell}',{'type':'mouse_cell','address':cell,'gesture':'click'},kind=kind)
def text(t,slug,value,target='worksheet.cellEditor',expect=None,kind=None):
 if target=='worksheet.grid' and len(value)>1:
  emit(t,slug+'-first-key',{'type':'text','target':target,'text':value[0]},kind=kind)
  emit(t,slug+'-remaining-keys',{'type':'text','target':'worksheet.cellEditor','text':value[1:]},expect,kind)
 else:emit(t,slug,{'type':'text','target':target,'text':value},expect,kind)
def native_input(value):
 if not value.startswith('=') and value.endswith('%'):
  text=format(Decimal(value[:-1])/100,'f')
  return text.rstrip('0').rstrip('.') if '.' in text else text
 return value

def enter(t,cell,value,kind=None):
 click(t,cell,kind=kind);text(t,f'begin-{cell}',value[0],'worksheet.grid',kind=kind)
 if len(value)>1:text(t,f'type-{cell}',value[1:],kind=kind)
 key(t,f'commit-{cell}',['Return'],'worksheet.cellEditor',{'cells':[{'address':cell,'input':native_input(value)}]},kind)
def drag(t,a,b,slug='range',kind=None):
 emit(t,f'{slug}-expose-start-{a}',{'type':'wheel_to_cell','address':a},kind=kind)
 emit(t,f'{slug}-press-{a}',{'type':'mouse_cell','address':a,'gesture':'press'},kind=kind)
 emit(t,f'{slug}-expose-end-{b}',{'type':'wheel_to_cell','address':b},kind=kind)
 for gesture in ['move','release']:emit(t,f'{slug}-{gesture}-{b}',{'type':'mouse_cell','address':b,'gesture':gesture},kind=kind)

def live_selection(identifier):
 e=next(item for item in source['events'] if item['id']==identifier);mapped.add(identifier)
 for phase in live_selections['groups'][identifier]:
  record={'id':identifier+'-live-'+phase['id'],'videoTime':phase['videoTime'],'evidence':{'kind':'visual','ledger':'income-projections-visual-steps.json','eventId':identifier,'phaseLedger':'income-live-selection-phases.json','frames':[str(Path(phase['sourceFrame']).resolve())],'observed':e['observed'],'executionMethod':phase['executionMethod']},'action':phase['action']}
  if phase.get('expect'):record['expect']=phase['expect']
  steps.append(record)

def width_phase_records(identifier):
 e=next(item for item in source['events'] if item['id']==identifier);mapped.add(identifier)
 records=[]
 for phase in width_phases['groups'][identifier]:
  record={'id':identifier+'-'+phase['id'],'videoTime':phase['videoTime'],'evidence':{'kind':'visual','ledger':'income-projections-visual-steps.json','eventId':identifier,'phaseLedger':'income-width-modal-phases.json','frames':[phase['sourceFrame']],'observed':e['observed'],'executionMethod':phase['executionMethod']},'action':phase['action']}
  if phase.get('expect'):record['expect']=phase['expect']
  records.append(record)
 return records

def copy_paste(t,a,b,d,kind=None):
 drag(t,a,b,'copy-range',kind);key(t,'copy',['Ctrl+C'],kind=kind);click(t,d,'paste-destination',kind);key(t,'paste',['Ctrl+V'],kind=kind)
def inserts(t,row,count,kind=None):
 click(t,f'B{row}',kind=kind);key(t,'select-whole-row',['Shift+Space'],kind=kind)
 for n in range(count):key(t,f'insert-row-{n+1:02}',['Ctrl++'],kind=kind)
def pending(t,slug,description,kind=None):emit(t,slug,{'type':'pending_dependency','description':description},kind=kind)
key('35:11','incoming-checkpoint',[],expect={'row':2,'column':2,'sheetName':'Sheet1','showGridLines':False,'cells':[{'address':'B2','input':'Income Statement:'},{'address':'B24','input':'Balance Sheet:'}]})
click('38:09','B2');click('38:10','D8')
click('38:11','B2');key('38:11','whole-row-2',['Shift+Space'])
for n in range(6):key('38:11.7',f'insert-row-{n+1:02}',['Ctrl++'])
live_selection('income-047')
live_selection('income-048')
click('38:15','B3','intermediate');click('38:15','B2','return')
enter('38:16','B2','Drivers:')
click('38:17','B3');click('38:17','B4','next');key('38:17','whole-row-4',['Shift+Space'])
for n in range(9):key('38:17.7',f'insert-row-{n+1:02}',['Ctrl++'])
click('38:19','B5','intermediate');click('38:19','B4','return')
enter('38:20','B4','New Equipment Market Size:');enter('38:24','B5','Growth Rate:')
click('38:26','B7');text('38:27','begin-cancelled-label','N','worksheet.grid');text('38:27','type-cancelled-label','ew');key('38:27','cancel-uncommitted-label',['Escape'],'worksheet.cellEditor');click('38:27','B6','after-cancel')
copy_paste('38:29','B4','B4','B7');enter('38:31','B7','New Equipment Market Share:');click('38:31','D7','intermediate');click('38:31','C7','return')
emit('38:43','inspect-retained-raw-source',{'type':'activate_window','documentName':'Otis-10-K-Raw.xls'},{'sheetName':'cash flows','row':51,'column':5,'cells':[{'address':'E51','value':1676}]})
key('38:43','save-retained-raw-source',['Ctrl+S'],expect={'documentName':'Otis-10-K-Raw.xls','sheetName':'cash flows','row':51,'column':5,'topRow':37,'firstColumn':1,'nameBoxText':'E51','cells':[{'address':'E51','value':1676}]})
steps[-1]['evidence']['executionMethod']='Actual native source Save at the audited source-workbook persistence boundary. The physical shortcut is inferred; the observed cash flows E51/top37/firstA view is asserted without changing source cells or selection.'
emit('38:44','return-to-model',{'type':'activate_window','documentName':'101-01-90-Minute-Case-Study-Otis-Blank.xlsx'})
click('38:44','C7')
for c,v in [('C7','16%'),('D7','17%'),('E7','18%')]:enter('38:45',c,v)
drag('38:45','C7','E7','assumptions')
key('38:50','open-font-color-palette',['Alt','1'])
children=[]
def child(t,slug,action,expect=None,kind=None):
 before=len(steps);emit(t,slug,action,expect,kind);children.append(steps.pop());assert len(steps)==before
child('38:52','colors-standard-visible',{'type':'key_sequence','target':'colors.dialog','keys':[]},{'modal':'colors.dialog'})
child('38:52','colors-custom-tab',{'type':'mouse_tab','target':'colors.tabs','tab':'Custom'})
children[-1]['videoTime']='38:53'
child('38:54','select-blue-component',{'type':'key_sequence','target':'colors.blue','keys':['Ctrl+A']})
for digit,time,image in [('2','38:54.4','38-54-4-colors-rgb-blue2.jpg'),('5','38:54.5','38-54-5-colors-rgb-blue25.jpg'),('5','38:54.6','38-54-6-colors-rgb-blue255.jpg')]:
 child('38:54','type-blue-'+time.replace(':','-').replace('.','-'),{'type':'text','target':'colors.blue','text':digit})
 children[-1]['videoTime']=time;children[-1]['evidence']['frames']=[str(root/'video-ui-evidence'/image)]
child('38:54','accept-blue',{'type':'key_sequence','target':'colors.dialog','keys':['Return']})
children[-1]['videoTime']='38:54.900';children[-1]['evidence']['frames']=[str(root/'source-frames/income-colors-close/2334.900000.png')];children[-1]['evidence']['executionMethod']='Native Return is timed to the first observed dialog-close fade at2334.9 seconds. The physical acceptance key between the last fully visible2334.866667 frame and the fade is unobserved; see income-colors-accept-native-boundary.json. RGB2/25/255 actions and their actual order are unchanged.'
emit('38:52','more-colors-dialog',{'type':'modal','open':{'type':'mouse','target':'tutorial.font.moreColors','gesture':'click'},'steps':children},{'cells':[{'address':c,'style':{'color':'#0000ff'}} for c in ['C7','D7','E7']]})
click('38:55','B7')
def range_cells(r):
 return r.split(':')[0],r.split(':')[-1]
def selected(t,r,slug='select-range'):
 a,b=range_cells(r)
 if a==b:click(t,a,slug)
 else:drag(t,a,b,slug)
def fill(t,r,direction='right'):
 selected(t,r,'fill-'+direction+'-range');key(t,'fill-'+direction,['Ctrl+R' if direction=='right' else 'Ctrl+D'])
def pointer_formula(t,cell,formula,wrong_first=None,commit_at=None):
 click(t,cell);text(t,'begin-formula-'+cell,'=','worksheet.grid')
 position=1;refs=list(re.finditer(r'\$?[A-Z]{1,3}\$?[1-9][0-9]*(?::\$?[A-Z]{1,3}\$?[1-9][0-9]*)?',formula))
 for n,m in enumerate(refs):
  if m.start()>position:text(t,f'formula-{cell}-text-{n}',formula[position:m.start()])
  raw=m.group();plain=raw.replace('$','');a,b=range_cells(plain)
  if n==0 and wrong_first:
   for k,candidate in enumerate(wrong_first):click(t,candidate,f'formula-{cell}-wrong-pointer-{k}')
  if a==b:click(t,a,f'formula-{cell}-pointer-{n}')
  else:drag(t,a,b,f'formula-{cell}-range-{n}')
  first=raw.split(':')[0];absolute_column=first.startswith('$');absolute_row='$' in first.lstrip('$')
  presses=1 if absolute_column and absolute_row else 2 if absolute_row else 3 if absolute_column else 0
  for k in range(presses):key(t,f'formula-{cell}-anchor-{n}-{k}',['F4'],'worksheet.cellEditor')
  position=m.end()
 if position<len(formula):text(t,'formula-'+cell+'-tail',formula[position:])
 key(commit_at or t,'commit-formula-'+cell,['Return'],'worksheet.cellEditor',{'cells':[{'address':cell,'input':native_input(formula)}]})
def dialog(t,slug,opener,children,expect=None):emit(t,slug,{'type':'modal','open':opener,'steps':children},expect)
def nested(t,slug,action,expect=None):
 before=len(steps);emit(t,slug,action,expect);result=steps.pop();assert len(steps)==before;return result
def keys_child(t,slug,keys,target):return nested(t,slug,{'type':'key_sequence','target':target,'keys':keys})
def mouse_child(t,slug,target):return nested(t,slug,{'type':'mouse','target':target})
def tab_child(t,slug,tab):return nested(t,slug,{'type':'mouse_tab','target':'formatCells.tabs','tab':tab})
def paste_special(t,mode='formats'):
 children=[keys_child(t,'paste-special-'+mode,[{'formats':'T','formulas':'F','all':'A'}[mode]],'pasteSpecial.dialog'),keys_child(t,'accept-paste-special',['Return'],'pasteSpecial.dialog')]
 dialog(t,'paste-special-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+Alt+V']},children)
def format_copy(t,source,destination):
 selected(t,source,'format-source');key(t,'copy-format-source',['Ctrl+C']);selected(t,destination,'format-destination');paste_special(t,'formats')
def held_paste_special(identifier):
 phase_file=root/'income-paste-modal-count-phases.json'
 entry=next(group for group in json.loads(phase_file.read_text())['groups'] if group['id']==identifier)
 def record(phase):
  source_event=next(e for e in source['events'] if e['id']==phase['eventId']);mapped.add(source_event['id'])
  result={'id':phase['eventId']+'-'+phase['id'],'videoTime':phase['videoTime'],'evidence':{'kind':'visual','ledger':'income-projections-visual-steps.json','eventId':phase['eventId'],'phaseLedger':phase_file.name,'frames':[str((Path(__file__).resolve().parents[1]/phase['sourceFrame']['path']).resolve())],'observed':source_event['observed'],'executionMethod':entry['method']},'action':phase['action']}
  if phase.get('expect'):result['expect']=phase['expect']
  return result
 steps.extend(record(phase) for phase in entry['before'])
 children=[record(phase) for phase in entry['modal']]
 source_event=next(e for e in source['events'] if e['id']==identifier)
 emit(source_event['time'],'paste-special-dialog',{'type':'modal','open':{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+Alt+V']},'steps':children})
 steps[-1]['videoTime']=children[0]['videoTime'];steps[-1]['evidence'].update({'phaseLedger':phase_file.name,'executionMethod':entry['method'],'frames':children[0]['evidence']['frames']})
 steps.extend(record(phase) for phase in entry['after'])
def column_width(t,width):
 if t=='42:41':
  records=width_phase_records('income-151');children=records[:7]
  dialog(t,'column-width-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Alt','O','C','W']},children,{'nameBoxText':'1R x 4C'})
  steps[-1]['videoTime']=children[0]['videoTime'];steps[-1]['evidence'].update({'phaseLedger':'income-width-modal-phases.json','frames':[children[0]['evidence']['frames'][0]],'executionMethod':width_phases['qualification']})
  steps.extend(records[7:]);return
 children=[keys_child(t,'select-column-width',['Ctrl+A'],'qt_spinbox_lineedit'),nested(t,'type-column-width',{'type':'text','target':'qt_spinbox_lineedit','text':str(width)}),keys_child(t,'accept-column-width',['Return'],'qt_spinbox_lineedit')]
 dialog(t,'column-width-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Alt','O','C','W']},children)
def blue(t,r):
 selected(t,r);key(t,'apply-current-blue',['Alt','1']);emit(t,'choose-blue',{'type':'mouse','target':'tutorial.font.0000ff'})

pointer_formula('39:00','C4','=C19/C7');fill('39:09','C4:E4')
pointer_formula('39:10','D5','=D4/C4-1');click('39:13','D5');key('39:13','percentage-format',['Ctrl+Shift+5']);key('39:17','increase-decimals',['Alt','3'])
steps[-1]['evidence']['executionMethod']='Actual Alt,3 changes one decimal. The source decimal change is visible; its physical gesture falls between sampled frames.'
click('39:18','C5')
emit('39:18','held-three-columns',{'type':'key_sequence','target':'worksheet.grid','keys':['Shift+Right','Shift+Right'],'releaseModifiers':False},{'row':5,'column':3,'nameBoxText':'1R x 3C'})
steps[-1]['videoTime']='39:19';steps[-1]['evidence']['frames']=[str((root/'source-frames/later/income/frame-0249.jpg').resolve())];steps[-1]['evidence']['executionMethod']='Held Shift+Right is an inferred native input method for the source39:19 three-column selection. The visible1R x3C header remains held in the following Alignment modal; physical source keys are unobserved.'
alignment=tab_child('39:20','alignment-tab','Alignment');alignment['expect']={'nameBoxText':'1R x 3C'}
released=nested('39:20','release-held-columns',{'type':'modifier_release','target':'formatCells.dialog','keys':['Shift']},{'nameBoxText':'C5'})
dismissed=keys_child('39:20','dismiss-unchanged-dialog',['Escape'],'formatCells.dialog')
for child in [released,dismissed]:
 child['videoTime']='39:21';child['evidence']['frames']=[str((root/'source-frames/later/income/frame-0251.jpg').resolve())];child['evidence']['executionMethod']='Native modifier release and Escape are inferred completion/dismissal counterparts of the first retained39:21 dialog-gone/D5 view; individual physical source keys and their within-frame order are unobserved.'
dialog('39:20','alignment-dialog-cancel',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},[alignment,released,dismissed])
click('39:20','D5','after-dialog')
for step in steps[-2:]:step['videoTime']='39:21';step['evidence']['frames']=[str((root/'source-frames/later/income/frame-0251.jpg').resolve())]
children=[tab_child('39:22','number-tab','Number'),keys_child('39:22','percentage-category',['Home','Down','Down','Down','Down','Down','Down'],'formatCells.categoryList'),keys_child('39:24','custom-category',['End'],'formatCells.categoryList'),keys_child('39:28','select-type',['Ctrl+A'],'formatCells.numberFormat')]
for n,chunk in enumerate(['0','.0%',';(0.0%)']):children.append(nested('39:28',f'custom-format-part-{n}',{'type':'text','target':'formatCells.numberFormat','text':chunk}))
children.append(keys_child('39:34','accept-custom-format',['Return'],'formatCells.dialog'))
dialog('39:22','custom-percent-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},children,{'cells':[{'address':'D5','style':{'numberFormat':'0.0%;(0.0%)'}}]})
fill('39:34','D5:E5');click('39:37','D7','annotation-first');click('39:37','E11','annotation-second')
for step in steps[-2:]:step['videoTime']='39:39'
click('39:37','F5','annotation-cleared-next-assumption')
for step in steps[-2:]:step['videoTime']='39:46'

click('41:51','F5');enter('41:57','F5','5');click('41:58','E7','copy-source');key('41:58','copy',['Ctrl+C']);click('41:58','F5','paste-target');paste_special('41:59')
enter('42:00.667','F5','5%');steps[-1]['evidence']['executionMethod']='Native percentage entry adapts the observed committed5%value; no actual source typing draft or physical keystrokes are visible.';click('42:01.167','F5','observed-ready-decimal-target');key('42:01.167','observed-ready-increase-decimals',['Alt','3'],expect={'cells':[{'address':'F5','input':'0.05','display':'5.0%'}]});steps[-1]['evidence']['executionMethod']='Native IncreaseDecimals Alt3 is an explicitly inferred input-method counterpart of the source Ready5%toReady5.0%format transition at2521.166667; the physical source shortcut is not visible. No product inference rule or unobserved cell metadata rewrite.';click('42:02','E7');selected('42:02','C7:E7');key('42:03','increase-decimals',['Alt','3'])
emit('42:06','open-quick-analysis',{'type':'mouse','target':'worksheet.quickAnalysis'});key('42:06','quick-analysis-visible',[],target='quickAnalysis.popup')
click('42:08','H6','dismiss-quick-analysis');selected('42:08','C7:E7');key('42:10','home-keytips',['Alt','H']);key('42:10','dismiss-keytips',['Escape'])
click('42:14','F5');fill('42:14','F5:G5');enter('42:17','H5','4');held_paste_special('income-141')
for cell,value,at in [('H5','4%','42:20.633333'),('I5','4%','42:20.933333'),('J5','3%','42:21.433333')]:
 before=len(steps);enter('42:20' if cell!='J5' else '42:21',cell,value)
 for step in steps[before:]:step['videoTime']=at;step['evidence']['executionMethod']='Native percentage entry is an inferred counterpart of the observed4%/4%/3%drafts after the held PasteSpecial dialog; physical individual keys are unobserved.'
click('42:22','F4');pointer_formula('42:26','F4','=E4*(1+F5)');format_copy('42:29','E4','F4')
click('42:31','E4');key('42:31','copy-again',['Ctrl+C']);click('42:31','F4');column_width('42:33',11.14);fill('42:36','F4:J4')
steps.extend(width_phase_records('income-150'));column_width('42:41',11.14)
click('42:44','F7');held_paste_special('income-153');enter('42:51','F7','18%')
for cell,value in [('G7','18.2%'),('H7','18.4%'),('I7','18.5%'),('J7','18.5%')]:enter('42:57',cell,value)
for cell in ['G7','F7','F19']:click('43:02',cell)
pointer_formula('43:05','F19','=F4*F7')
held_paste_special('income-159')
for cell in ['F19','F32','B8','B9']:click('43:16',cell)
selected('43:25','C20:E21');click('43:25','B8');click('43:25','B9');click('43:50','B9');enter('43:51','B9','Service Units (Maintenance):')
inserts('43:55',11,2);click('43:57','B10');enter('44:06','B10','Growth Rate:');enter('44:09','B12','Revenue per Service Unit:')
click('44:15','B13');text('44:15','begin-B13','G','worksheet.grid')
for slug,at,chunk,draft,frame in [('type-B13','44:15','r','Gr','0545'),('draft-Gro','44:16','o','Gro','0546'),('draft-complete','44:17','wth Rate:','Growth Rate:','0547')]:
 text('44:15',slug,chunk,expect={'row':13,'column':2,'formulaDraft':draft,'cells':[{'address':'B13','input':''}]})
 steps[-1]['videoTime']=at;steps[-1]['evidence']['frames']=[str((root/f'source-frames/later/income/frame-{frame}.jpg').resolve())]
 steps[-1]['evidence']['executionMethod']='Native character entry is an inferred input-method counterpart of the observed Gr/Gro/full inline drafts. The retained one-second source frames establish these states, not individual physical-key timestamps; the label remains uncommitted until the following Home transition.'
key('44:18','peek-home',['Alt','H'],expect={'row':13,'column':2,'nameBoxText':'B13','formulaDraft':'Growth Rate:','cells':[{'address':'B13','input':'Growth Rate:'}]})
steps[-1]['evidence']['executionMethod']='The existing grid-targeted native Home adapter commits the real inline draft by focus loss and retains B13, matching the source Ready/Home state. Physical source commitment and ribbon-opening keys are unobservable and inferred; no Return or fabricated reselection is inserted.'
key('44:18','collapse-home',['Escape']);click('44:18','C9')
for step in steps[-3:]:
 step['videoTime']='44:19';step['evidence']['frames']=[str((root/'source-frames/later/income/frame-0549.jpg').resolve())];step['evidence']['executionMethod']='Inferred native dismissal/selection is timed to the first retained collapsed-ribbon C9 source view at44:19; the physical keys or pointer gesture between frames are unobserved.'
for cell,value in [('C9','2'),('D9','2.1'),('E9','2.15')]:enter('44:20',cell,value)
selected('44:20','C9:E9');emit('44:26','apply-current-blue',{'type':'mouse','target':'tutorial.qat.fontColor'})
click('44:26','C9','review-first');selected('44:26','C9:E9','reselect')
children=[tab_child('44:30','number-tab','Number'),keys_child('44:30','number-category',['Home','Down'],'formatCells.categoryList'),keys_child('44:30','select-decimals',['Ctrl+A'],'formatCells.decimalPlaces'),nested('44:30','two-decimals',{'type':'text','target':'formatCells.decimalPlaces','text':'2'}),mouse_child('44:30','toggle-thousands-on','formatCells.thousandsSeparator'),mouse_child('44:30','toggle-thousands-off','formatCells.thousandsSeparator'),keys_child('44:30','accept-number-format',['Return'],'formatCells.dialog')]
dialog('44:30','number-two-decimals-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+1']},children)
click('44:35','E9');click('44:35','D7');copy_paste('44:39','D5','J5','D10');click('44:43','E10');click('44:43','F10');enter('44:48','F10','3%');fill('44:48','F10:J10')
pointer_formula('44:50','F9','=E9*(1+F10)');format_copy('44:54','E9','F9');fill('44:56','F9:J9');click('44:59','C12')
pointer_formula('45:06','C12','=C22/C9',wrong_first=['C21'],commit_at='45:20')
steps[-1]['evidence'].update({'phaseLedger':'income-c12-native-boundaries.json','frames':[str(root/'source-frames/income-c12-boundaries/2721.400000.png')]});steps[-1]['videoTime']='45:21.400';steps[-1]['action']['keys']=['Ctrl+Return'];steps[-1]['expect'].update({'row':12,'column':3,'nameBoxText':'C12'});steps[-1]['expect']['cells'][0].update({'value':3735,'valueType':'number'});steps[-1]['evidence']['executionMethod']='Native CtrlEnter is an explicitly inferred retain-origin commit counterpart: source remains Point/NameBoxC9 through2721.366667 and first shows Ready/result3735.00 with C12 retained at2721.4. The actual physical acceptance key between native30fps frames is unobserved.'
key('45:06','uncommitted-before-context',[],expect={'formulaDraft':'=C22/C9','nameBoxText':'C9','cells':[{'address':'C12','input':''}]})
steps[-1]['videoTime']='45:12';steps[-1]['evidence']['frames']=[event('45:06')['evidence_frames'][-1]];steps[-1]['evidence']['executionMethod']='Read-only native observation of the source45:12 Point/inline formula, immediately before the45:13 PDF switch. It introduces no focus or worksheet mutation.'
key('45:20','return-model-checkpoint',[],expect={'formulaDraft':'=C22/C9','nameBoxText':'C9','cells':[{'address':'C12','input':''}]})
c12_boundaries=json.loads((root/'income-c12-native-boundaries.json').read_text())
for phase in c12_boundaries['selectionPhases']:
 emit('45:22',phase['slug'],phase['action'],phase.get('expect'))
 steps[-1]['videoTime']=phase['time'];steps[-1]['evidence'].update({'phaseLedger':'income-c12-native-boundaries.json','frames':[str(root/'source-frames/income-c12-boundaries'/(phase['frame']+'.png'))],'executionMethod':c12_boundaries['methodQualification']})
format_copy('45:25','C21:E21','C12:E12');selected('45:27','D10:E10','first-growth-range');copy_paste('45:27','D10','J10','D13');click('45:32','E14');click('45:39','H16');click('45:39','F13')
for cell,value in [('F13','4%'),('G13','3.5%'),('H13','3.5%')]:enter('45:40',cell,value)
for cell in ['I13','J13','F12']:click('45:40',cell)
pointer_formula('45:46','F12','=E12*(1+F13)');format_copy('45:50','E12','F12');fill('45:51','F12:J12');click('45:53','F13');click('45:53','F22')
click('45:55','F22');text('45:55','begin-service-revenue','=','worksheet.grid');click('45:55','F19','wrong-first-pointer');click('45:55','F9','correct-first-pointer');text('45:55','multiply-service-revenue','*');click('45:55','F13','wrong-price-pointer');click('45:55','F12','correct-price-pointer');key('45:55','commit-service-revenue',['Return'],'worksheet.cellEditor',{'cells':[{'address':'F22','input':'=F9*F12'}]})
pointer_formula('45:59','F23','=SUM(F21:F22)');selected('46:00','F21:F22','first-revenue-range');selected('46:00','F21:F23','three-revenue-rows');selected('46:05','F21:I23','intermediate-fill-extent');fill('46:05','F21:J23')
click('46:08','F29');click('46:10','F29','annotation-selection');pointer_formula('46:15','F24','=F23/E23-1');click('46:15','F24','select-growth-result-before-format');key('46:15','growth-check-percent',['Ctrl+Shift+5']);selected('46:19','F24:H24','intermediate-growth-range');fill('46:19','F24:J24');key('46:19','growth-one-decimal',['Alt','3']);click('46:21','H29');selected('46:37','F24:J24');key('46:37','clear-temporary-growth-checks',['Delete']);click('46:40','B15');inserts('46:41',15,7)
click('46:44','B34');click('46:44','B37');live_selection('income-217');live_selection('income-218');click('46:57','B15')
enter('46:59','B15','Product - COGS % Revenue:');enter('47:02','B16','Service - COGS % Revenue:');enter('47:07','B17','OpEx % Revenue:');enter('47:10','B19','Tax Rate:');enter('47:12','B20','NCI Net Income % Total:')
selected('47:18','B44:E44');selected('47:23','C26:E43','broad-history');click('47:23','E44');selected('47:23','C45:E45','nci-history');live_selection('income-227')
click('47:35','C15');text('47:35','begin-product-cogs-ratio','=-','worksheet.grid');click('47:35','C32','product-cogs');text('47:35','divide-product-cogs','/');click('47:35','C26','wrong-year-header-denominator');click('47:35','C28','correct-product-revenue-denominator');key('47:35','commit-product-cogs',['Return'],'worksheet.cellEditor',{'cells':[{'address':'C15','input':'=-C32/C28'}]})
pointer_formula('47:40','C16','=-C33/C29');pointer_formula('47:45','C17','=-C34/C30');click('47:49','C15');key('47:49','inspect-product-ratio',['F2']);key('47:49','leave-product-ratio',['Escape'],'worksheet.cellEditor');format_copy('47:51','D13','C15:C17');click('47:54','F13');selected('47:54','C15:C17');fill('47:56','C15:E17');click('47:58','F15')
click('48:02','F15');text('48:02','begin-average-autocomplete','=AV','worksheet.grid');text('48:02','complete-average','ERAGE(');click('48:02','E15','first-average-reference');drag('48:02','C15','E15','expand-average-range');text('48:02','close-average',')');key('48:02','commit-average',['Return'],'worksheet.cellEditor',{'cells':[{'address':'F15','input':'=AVERAGE(C15:E15)','valueType':'number','display':'82.4%'}]})
held_paste_special('income-237');held_paste_special('income-238');click('48:11','G15');selected('48:11','F15:G15');fill('48:13','F15:H15');selected('48:18','G15:H15');key('48:18','clear-moving-average',['Delete']);selected('48:20','F15:F17');key('48:20','bold-first-forecast-averages',['Ctrl+B']);pointer_formula('48:22','G15','=F15');fill('48:22','G15:G17','down');fill('48:22','G15:J17')
click('48:26','C19');pointer_formula('48:27','C19','=-C42/C40');pointer_formula('48:33','C20','=-C45/C44');format_copy('48:38','C17:J17','C19:J19');held_paste_special('income-249');fill('48:45','C19:E20')
for step in steps[-6:]:step['videoTime']='48:45.100000'
click('48:47','F17');click('48:47','B25');selected('48:51','F17:J17','forecast-link-source');key('48:51','copy-forecast-link-row',['Ctrl+C']);selected('48:51','F19:J20','tax-nci-destination');key('48:51','paste-forecast-averages-links',['Ctrl+V'])
click('48:54','F32');pointer_formula('48:56','F32','=-F28*F15');fill('49:14','F32:F33','down');click('49:14','F33');key('49:14','inspect-service-cogs',['F2']);key('49:14','leave-service-cogs',['Escape'],'worksheet.cellEditor');pointer_formula('49:16','F34','=-F30*F17',wrong_first=['F32']);pointer_formula('49:20','F35','=SUM(F32:F34)');click('49:20','F35','inspect-wrong-operating-income');key('49:20','edit-wrong-operating-income',['F2']);key('49:20','leave-wrong-operating-income',['Escape'],'worksheet.cellEditor');click('49:23','F35');key('49:23','clear-incorrect-operating-income',['Delete']);pointer_formula('49:24','F35','=F30+SUM(F32:F34)',wrong_first=['F32']);pointer_formula('49:29','F37','=E37')
click('49:35','F37','copy-source');key('49:35','copy-other-income',['Ctrl+C']);click('49:35','F38','paste-destination');paste_special('49:35','formulas');click('49:37','F38');key('49:37','pension-formula-review',[],expect={'cells':[{'address':'F38','input':'=E38'}]});click('49:44','F39');click('49:44','F40');copy_paste('49:46','E40','E40','F40');copy_paste('49:49','E42','E42','F42');key('49:49','wrong-external-tax-check',[],expect={'cells':[{'address':'F42','input':"=-'[Otis-10-K-Raw.xls]operations'!J22"}]})
click('49:49','F42','inspect-wrong-external-tax');key('49:49','edit-wrong-external-tax',['F2']);key('49:49','leave-wrong-external-tax',['Escape'],'worksheet.cellEditor');pointer_formula('49:55','F42','=-F40*F19',wrong_first=['F41']);copy_paste('50:00','E44','E44','F44');pointer_formula('50:02','F45','=-F44*F20');pointer_formula('50:05','F46','=SUM(F44:F45)');live_selection('income-269');click('50:11','H42');key('50:11','tax-formula-check',[],expect={'cells':[{'address':'H42','input':'=-H40*H19','valueType':'number','display':'(713)'}]});click('50:12','G33');key('50:12','income-projections-checkpoint',[],expect={'row':33,'column':7,'sheetName':'Sheet1','cells':[{'address':'G33','input':'=-G29*G16','valueType':'number','display':'(5,538)'},{'address':'B26','input':'Income Statement:'},{'address':'B48','input':'Balance Sheet:'}]})
from otis_late_commit_headers import apply_late_commit_headers
apply_late_commit_headers(steps, root)
steps.sort(key=lambda step:sum(float(v)*60**n for n,v in enumerate(reversed(step["videoTime"].split(":")))))
manifest={'version':1,'sourceVideo':'https://www.youtube.com/watch?v=O2uPNfErmJo','scope':'Native ordered replay in progress for35:11–50:12. Requires the actual native historical checkpoint from the preceding replay. No workbook metadata or cells are cloned.','sourceReviewComplete':False,'windowSize':[1280,720],'preparedThrough':'50:12','incomingCheckpoint':{'time':'35:11','sheet':'Sheet1','activeCell':'B2','incomeHeader':2,'balanceHeader':24,'note':'The preceding native replay must supply this workbook; this manifest has no synthetic fixture loader.'},'steps':steps,'externalContext':[e for e in source['events'] if e.get('application')!='Excel model'],'remainingSourceEvents':[e['id'] for e in source['events'] if e['id'] not in mapped and e.get('application')=='Excel model'],'knownDependencies':['Requires real incoming historical workbook and native execution; sourceReviewComplete stays false.'],'unreplayedVisualEffects':[e for e in source['events'] if e.get('application')=='Excel model' and e['kind']=='annotation'],'unresolvedSourceGestures':[e for e in source['events'] if e.get('caveat')],'executionStatus':'Prepared action definitions. Actual native execution evidence is retained separately in later-native-replay logs; this definition never claims completed source parity.'}
p=root/'income-native-action-manifest.json';p.write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n');print(len(steps),'native steps;',len(manifest['remainingSourceEvents']),'remaining source events')
