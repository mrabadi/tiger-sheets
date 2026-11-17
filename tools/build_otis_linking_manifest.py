import json
from decimal import Decimal
import re
from pathlib import Path
root=Path("docs/validation/otis-90-minute")
source_file=root/"linking-debt-checks-visual-steps.json"
source=json.loads(source_file.read_text())
steps=[];mapped=set()

def event(t,kind=None):
 matches=[e for e in source['events'] if e['time']==t and (kind is None or e['kind']==kind)]
 assert len(matches)==1,(t,kind,len(matches));return matches[0]

def emit(t,slug,action,expect=None,kind=None,method=None):
 e=event(t,kind);mapped.add(e['id']);d={'id':f'{e["id"]}-{slug}','videoTime':t,'evidence':{'kind':'visual','ledger':source_file.name,'eventId':e['id'],'frames':e['evidence_frames'],'observed':e['observed']},'action':action}
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

def copy_paste(t,a,b,d,kind=None):
 drag(t,a,b,'copy-range',kind);key(t,'copy',['Ctrl+C'],kind=kind);click(t,d,'paste-destination',kind);key(t,'paste',['Ctrl+V'],kind=kind)

def inserts(t,row,count,kind=None):
 click(t,f'B{row}',kind=kind);key(t,'select-whole-row',['Shift+Space'],kind=kind)
 for n in range(count):key(t,f'insert-row-{n+1:02}',['Ctrl++'],kind=kind)

def pending(t,slug,description,kind=None):emit(t,slug,{'type':'pending_dependency','description':description},kind=kind)

def child(t,slug,action,expect=None,kind=None):
 before=len(steps);emit(t,slug,action,expect,kind);children.append(steps.pop());assert len(steps)==before

def range_cells(r):
 return r.split(':')[0],r.split(':')[-1]

def selected(t,r,slug='select-range'):
 a,b=range_cells(r)
 if a==b:click(t,a,slug)
 else:drag(t,a,b,slug)

def fill(t,r,direction='right'):
 selected(t,r,'fill-'+direction+'-range');key(t,'fill-'+direction,['Ctrl+R' if direction=='right' else 'Ctrl+D'])

def pointer_formula(t,cell,formula,wrong_first=None):
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
 key(t,'commit-formula-'+cell,['Return'],'worksheet.cellEditor',{'cells':[{'address':cell,'input':native_input(formula)}]})

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

def held_paste_special(group):
 phase_file=root/'linking-modal-held-selection-phases.json'
 entry=next(value for value in json.loads(phase_file.read_text())['groups'] if value['id']==group)
 def qualify(record,phase):
  record['videoTime']=phase['videoTime']
  record['evidence'].update({'phaseLedger':phase_file.name,'phaseGroup':group,'frames':[str((Path(__file__).resolve().parents[1]/frame['path']).resolve()) for frame in phase['sourceFrames']],'executionMethod':entry['method']})
 before=len(steps);click(entry['selectionEventTime'],entry['selectionStartCell'])
 for record in steps[before:]:
  record['videoTime']=entry['selectionStartTime'];record['evidence'].update({'phaseLedger':phase_file.name,'phaseGroup':group,'executionMethod':entry['method'],'frames':[str((Path(__file__).resolve().parents[1]/frame['path']).resolve()) for frame in entry['selectionStartSourceFrames']]})
 for phase in entry['selectionPhases']:
  emit(entry['selectionEventTime'],phase['slug'],phase['action'],phase['expect']);qualify(steps[-1],phase)
 children=[]
 for phase in entry['modalPhases']:
  record=nested(entry['modalEventTime'],phase['slug'],phase['action'],phase['expect']);qualify(record,phase);children.append(record)
 dialog(entry['modalEventTime'],'paste-special-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+Alt+V']},children)
 steps[-1]['videoTime']=entry['modalPhases'][0]['videoTime']
 steps[-1]['evidence'].update({'phaseLedger':phase_file.name,'phaseGroup':group,'executionMethod':entry['method']})
 for phase in entry['afterPhases']:
  emit(entry['afterEventTime'],phase['slug'],phase['action'],phase['expect']);qualify(steps[-1],phase)

def column_width(t,width):
 children=[keys_child(t,'select-column-width',['Ctrl+A'],'qt_spinbox_lineedit'),nested(t,'type-column-width',{'type':'text','target':'qt_spinbox_lineedit','text':str(width)}),keys_child(t,'accept-column-width',['Return'],'qt_spinbox_lineedit')]
 dialog(t,'column-width-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Alt','O','C','W']},children)

def blue(t,r):
 selected(t,r);key(t,'apply-current-blue',['Alt','1']);emit(t,'choose-blue',{'type':'mouse','target':'tutorial.font.0000ff'})

def checkpoint(t,expect=None):key(t,'visible-checkpoint',[],expect=expect)

def whole_row(t,row):click(t,'B'+str(row));key(t,'whole-row',['Shift+Space'])

def external(t,operation):
 emit(t,'external-text-'+operation,{'type':'external_text_editor','operation':operation},method='QPlainTextEdit external-app adapter for the source Notepad window; actual typing/cut/copy uses the system clipboard, not worksheet mutation.')

def external_keys(t,slug,keys):key(t,slug,keys,'replay.externalTextEditor')

def source_to_external(t,cell):click(t,cell);key(t,'copy-source-label',['Ctrl+C']);external(t,'activate');external_keys(t,'paste-source-label',['Ctrl+V','Return'])

def return_model(t):emit(t,'return-model',{'type':'activate_window','documentName':'101-01-90-Minute-Case-Study-Otis-Blank.xlsx'})

def begin(t,cell,prefix='='):click(t,cell);text(t,'begin-'+cell,prefix,'worksheet.grid')

def point(t,cell,slug='point'):click(t,cell,slug)

def commit(t,cell,formula):key(t,'commit-'+cell,['Return'],'worksheet.cellEditor',{'cells':[{'address':cell,'input':native_input(formula)}]})

label_phase_file=root/'linking-visible-label-phases.json'
label_phases={entry['id']:entry for entry in json.loads(label_phase_file.read_text())['entries']}

def visible_label(group):
 entry=label_phases[group];cell=entry['cell'];current=''
 row=int(re.search(r'[0-9]+',cell).group());column=0
 for letter in re.match(r'[A-Z]+',cell).group():column=column*26+ord(letter)-ord('A')+1
 def qualify(before,phase):
  for step in steps[before:]:
   step['videoTime']=phase['videoTime']
   step['evidence'].update({'phaseLedger':label_phase_file.name,'phaseGroup':group,'frames':[str((Path(__file__).resolve().parents[1]/phase['sourceFrame']).resolve())],'executionMethod':entry['method']})
 for index,phase in enumerate(entry['phases']):
  before=len(steps);t=phase['sourceEventTime'];draft=phase['draft'];slug=f'label-{group}-phase-{index:02}'
  if index==0:
   click(t,cell,slug+'-select');text(t,slug,draft,'worksheet.grid')
  else:
   common=0
   while common<min(len(current),len(draft)) and current[common]==draft[common]:common+=1
   if len(current)>common:key(t,slug+'-remove',['Backspace']*(len(current)-common),'worksheet.cellEditor')
   if len(draft)>common:text(t,slug+'-append',draft[common:])
   if draft==current:key(t,slug+'-hold',[],'worksheet.cellEditor')
  steps[-1]['expect']={'row':row,'column':column,'formulaDraft':draft}
  qualify(before,phase);current=draft
 phase=entry['commit'];destination=phase['destination'];destination_row=int(re.search(r'[0-9]+',destination).group());destination_column=0
 for letter in re.match(r'[A-Z]+',destination).group():destination_column=destination_column*26+ord(letter)-ord('A')+1
 before=len(steps)
 key(phase['sourceEventTime'],f'label-{group}-commit',phase['keys'],'worksheet.cellEditor',{'row':destination_row,'column':destination_column,'cells':[{'address':cell,'input':phase['finalInput']}]})
 qualify(before,phase)

def allocation_selection():
 entry=json.loads(label_phase_file.read_text())['allocationSelection'];t=entry['sourceEventTime'];before=len(steps)
 emit(t,'allocation-held-range',entry['nativeHeldAction'],{'row':39,'column':2,'nameBoxText':'2R x 1C'})
 key(t,'allocation-held-range-checkpoint',[],expect={'row':39,'column':2,'nameBoxText':'2R x 1C'});steps[-1]['videoTime']=entry['lastVisibleCount']
 emit(t,'allocation-release-range',entry['nativeReleaseAction'],{'row':39,'column':2,'nameBoxText':'B39'});steps[-1]['videoTime']=entry['firstRestoredAnchor']
 for index,navigation in enumerate(entry['postReleaseNavigation']):
  key(t,f'allocation-post-release-navigation-{index}',['Down'],expect={'row':int(navigation['destination'][1:]),'column':2,'nameBoxText':navigation['destination']});steps[-1]['videoTime']=navigation['videoTime']
 for step in steps[before:]:
  seconds=sum(float(v)*60**n for n,v in enumerate(reversed(step['videoTime'].split(':'))))
  frame=min(entry['retainedOriginalFrames'],key=lambda frame:abs(frame['seconds']-seconds))
  step['evidence'].update({'phaseLedger':label_phase_file.name,'phaseGroup':'allocation-selection','frames':[str((Path(__file__).resolve().parents[1]/frame['sourceFrame']).resolve())],'executionMethod':entry['method']})

def scratch_inspection():
 entry=json.loads(label_phase_file.read_text())['scratchInspection']
 for video_time in entry['nativeCheckpoints']:
  seconds=sum(float(v)*60**n for n,v in enumerate(reversed(video_time.split(':'))))
  frame=min(entry['retainedOriginalFrames'],key=lambda frame:abs(frame['seconds']-seconds))
  key(entry['sourceEventTime'],'scratch-ready-inspection-'+video_time.replace(':','-'),[],expect={'row':111,'column':6,'nameBoxText':'F111','cells':[{'address':'F111','input':'=SUM(F118:H118)'}]})
  steps[-1]['videoTime']=video_time
  steps[-1]['evidence'].update({'phaseLedger':label_phase_file.name,'phaseGroup':'scratch-ready-inspection','frames':[str((Path(__file__).resolve().parents[1]/frame['sourceFrame']).resolve())],'observed':entry['sourceState'],'sourceLedgerCorrection':entry['originalLedgerError'],'executionMethod':entry['method']})
checkpoint('67:12',{'row':74,'column':7,'cells':[{'address':'B46','input':'Income Statement:'},{'address':'B68','input':'Balance Sheet:'},{'address':'B96','input':'Cash Flow Statement:'}]});click('67:37','G74');begin('67:38','F71');point('67:39','E71');text('67:39','add-net-cash','+');emit('67:40','scroll-cashflow-during-cash-entry',{'type':'wheel_to_cell','address':'F120'});point('67:41','F120');commit('67:42','F71','=E71+F120');selected('67:43','F71:J71');key('67:44','fill-cash-right',['Ctrl+R']);click('67:44','F75','after-fill');click('67:57','F74');begin('67:58','F74');point('67:58','E74');text('67:58','subtract-capex','-');emit('67:59','scroll-cashflow-ppe',{'type':'wheel_to_cell','address':'F106'});checkpoint('68:00');point('68:02','F106');text('68:05','subtract-da','-');point('68:07','F100');commit('68:14','F74','=E74-F106-F100');selected('68:15','F74:G74');fill('68:16','F74:J74');click('68:16','F75','after-fill');click('68:17','F76');click('68:24','F76');emit('68:25','scroll-balance-other-assets',{'type':'wheel_to_cell','address':'B76'});selected('68:26','B76:D76');begin('68:28','F76');point('68:29','E76');text('68:29','subtract-pensions','-');emit('68:30','scroll-cashflow-other-assets',{'type':'wheel_to_cell','address':'F101'});checkpoint('68:31');point('68:32','F101');text('68:33','subtract-acquisitions','-');point('68:34','F107');commit('68:36','F76','=E76-F101-F107');selected('68:36','F76:I76');fill('68:37','F76:J76');click('68:38','F77');key('68:38','autosum-draft',['Alt+='],expect={'formulaDraft':'=SUM(F71:F76)'});commit('68:39','F77','=SUM(F71:F76)');selected('68:39','F76:H77');fill('68:40','F77:J77');click('68:40','F78','after-fill');click('68:41','F83');emit('68:42','scroll-balance-liabilities',{'type':'wheel_to_cell','address':'B83'});begin('68:48','F83');point('68:49','E83');text('68:49','add-debt-change','+');emit('68:50','scroll-cashflow-debt-entry',{'type':'wheel_to_cell','address':'F111'});point('68:51','F111');commit('68:52','F83','=E83+F111');fill('68:53','F83:J83');click('68:54','F86');emit('68:55','scroll-balance-liabilities-subtotal',{'type':'wheel_to_cell','address':'F80'});pointer_formula('68:58','F86','=SUM(F80:F85)');fill('68:59','F86:J86');click('69:00','F88');click('69:35','F88');begin('69:36','F89');point('69:37','E89');text('69:37','add-nci-profit','+');point('69:38','F99');text('69:38','add-nci-dividend','+');emit('69:39','scroll-cashflow-nci-dividend',{'type':'wheel_to_cell','address':'F114'});point('69:40','F114');commit('69:41','F89','=E89+F99+F114');click('69:42','F88');click('70:00','F88');click('70:01','F98');emit('70:02','scroll-down-cashflow',{'type':'wheel_to_cell','address':'B103'});emit('70:04','scroll-financing',{'type':'wheel_to_cell','address':'B113'});whole_row('70:05',113);whole_row('70:06',118);emit('70:08','scroll-balance',{'type':'wheel_to_cell','address':'B87'});click('70:09','F87');click('70:10','F88');emit('70:11','adjust-common-equity-viewport',{'type':'wheel','target':'worksheet.grid','angleDelta':[0,-120]});begin('70:15','F88');point('70:16','E88');text('70:16','add-parent-profit','+');point('70:17','F98');text('70:18','add-common-dividends','+');emit('70:20','scroll-cashflow-common-equity',{'type':'wheel_to_cell','address':'B99'});checkpoint('70:23');checkpoint('70:25');emit('70:26','scroll-financing-during-equity-entry',{'type':'wheel_to_cell','address':'F112'});point('70:30','F112');text('70:31','add-stock-repurchases','+');point('70:32','F113');text('70:33','add-other-financing','+');point('70:34','F115');text('70:35','add-fx-effects','+');point('70:36','F118');commit('70:37','F88','=E88+F98+F112+F113+F115+F118');click('70:38','F90');pointer_formula('70:39','F90','=SUM(F88:F89)');click('70:40','E92');selected('70:41','E92:E94');fill('70:42','E92:F94');click('70:44','F94');selected('70:46','F88:H92');selected('70:47','F88:J94');key('70:48','fill-equity-nci-checks-right',['Ctrl+R']);click('70:48','F95','after-fill');key('70:50','native-save',['Ctrl+S']);emit('70:51','scroll-driver-labels',{'type':'wheel_to_cell','address':'B31'});selected('70:52','B31:B34');selected('70:53','B31:B35');key('70:53','remove-driver-bold',['Ctrl+B']);click('70:54','B41');key('70:54','remove-nci-label-bold',['Ctrl+B']);click('70:55','B44');key('70:55','remove-fx-label-bold',['Ctrl+B']);click('70:56','B40');click('72:01','B40');whole_row('72:03',37)
for n in range(2):key('72:04',f'insert-row-{n}',['Ctrl++'])
key('72:05','third-debt-driver-row',['Ctrl++']);visible_label('minimum-cash');visible_label('excess-cash');whole_row('72:12',40)
for n in range(3):key('72:13',f'insert-row-{n}',['Ctrl++'])
visible_label('stock-allocation');visible_label('debt-allocation');allocation_selection();whole_row('72:23',41);key('72:24','delete-separator-row',['Ctrl+-']);click('72:24','C38','after-delete');click('72:25','E37');click('72:28','E53');key('72:28','copy-min-cash-number-format',['Ctrl+C']);click('72:30','E37');paste_special('72:30');begin('72:31','E37','3');text('72:31','minimum-cash-partial','00');text('72:32','minimum-cash-final-zero','0');commit('72:33','E37','3000');click('72:35','F38');begin('72:38','F38');emit('72:39','scroll-prior-cash',{'type':'wheel_to_cell','address':'E76'});point('72:41','E76');text('72:43','add-cfo','+');emit('72:44','scroll-cashflow-excess-cash',{'type':'wheel_to_cell','address':'F108'});point('72:49','F108');text('72:50','add-cfi','+');point('72:51','F113');text('72:53','add-common-dividends','+');point('72:54','F117');text('72:55','add-nci-dividends','+');point('72:56','F119');text('72:57','add-fx-effects','+');point('72:58','F123');checkpoint('73:02');checkpoint('73:09')
for t,cell in [('73:19','B83'),('73:20','B51'),('73:21','B37')]:emit(t,'scroll-up-during-excess-cash-entry',{'type':'wheel_to_cell','address':cell})
text('73:23','subtract-minimum-cash','-');point('73:23','E37');key('73:25','anchor-minimum-cash',['F4'],'worksheet.cellEditor');commit('73:27','F38','=E76+F108+F113+F117+F119+F123-$E$37');fill('73:28','F38:J38');click('73:29','G38');click('73:30','I38');click('73:31','J38');key('73:31','inspect-last-excess-cash',['F2']);key('73:31','leave-last-excess-cash',['Escape'],'worksheet.cellEditor');click('73:32','F39');click('73:34','F42');begin('73:36','F42','=IF(');point('73:37','F38');text('73:39','negative-excess-cash-condition','<0');text('73:41','first-if-comma',',');text('73:45','negative-shortfall-branch','-');point('73:45','F39','wrong-shortfall-reference');point('73:46','F38','correct-shortfall-reference');text('73:47','second-if-comma',',');text('73:49','negative-debt-allocation-branch','-');point('73:49','F38');text('73:49','multiply-debt-allocation','*');point('73:54','E40');key('73:57.633','anchor-debt-percentage',['F4'],'worksheet.cellEditor');text('73:58.000','close-debt-allocation-if',')');commit('73:58.200','F42','=IF(F38<0,-F38,-F38*$E$40)');click('73:59','G44');key('73:59','copy-blue-percent-format',['Ctrl+C']);held_paste_special('allocation-formats');enter('74:01','E40','15%');click('74:02','E39');enter('74:03','E39','85%');checkpoint('74:04');click('74:09','E39');click('74:11','F38');click('74:13','E35');key('74:13','copy-debt-accounting-format',['Ctrl+C']);held_paste_special('debt-accounting-formats');fill('74:18','F42:J42');click('74:19','H42');click('74:20','F42');click('74:21','F45');begin('74:25','F45','=IF(');point('74:26','F44','wrong-stock-condition');point('74:27','F42','correct-stock-condition');text('74:29','stock-condition','<0,');text('74:33','negative-stock-allocation','-');point('74:34','F38');text('74:35','multiply-stock-allocation','*');point('74:36','E39');key('74:36','anchor-stock-percentage',['F4'],'worksheet.cellEditor');text('74:37','close-two-argument-if',')');commit('74:37','F45','=IF(F42<0,-F38*$E$39)');selected('74:38','F42:J42');key('74:38','copy-stock-accounting-formats',['Ctrl+C']);click('74:39','F45');paste_special('74:39');checkpoint('74:40');fill('74:41','F45:J45');click('74:42','F43');whole_row('74:43',43);key('74:44','delete-blank-stock-separator',['Ctrl+-']);click('74:45','F40');click('74:46','E47');visible_label('other-fees');click('74:57','E47');emit('74:58','scroll-balance-other-items',{'type':'wheel_to_cell','address':'B83'});emit('74:59','scroll-cashflow-other-items',{'type':'wheel_to_cell','address':'B119'});selected('75:00','B119:E120');click('75:01','D123');checkpoint('75:05');emit('75:06','scroll-balance-again',{'type':'wheel_to_cell','address':'B83'});emit('75:07','scroll-drivers-again',{'type':'wheel_to_cell','address':'B47'});click('75:08','E47');begin('75:09','E47','=-');emit('75:10','scroll-balance-during-fee-entry',{'type':'wheel_to_cell','address':'B83'});emit('75:11','scroll-cashflow-during-fee-entry',{'type':'wheel_to_cell','address':'E119'});point('75:12','E119');text('75:13','divide-debt-issuance','/');point('75:14','E115');commit('75:15','E47','=-E119/E115');click('75:17','E45');key('75:17','copy-nci-ratio-format',['Ctrl+C']);click('75:18','E47');paste_special('75:18');click('75:19','F47');selected('75:20','F43:J43');key('75:20','copy-payout-assumption-template',['Ctrl+C']);selected('75:21','F47:J47');key('75:21','paste-fee-assumption-template',['Ctrl+V']);enter('75:21','F47','2%');click('75:22','G47');enter('75:23','H47','2%');selected('75:24','G48:J48');key('75:24','copy-fx-links',['Ctrl+C']);enter('75:25','G47','2%');enter('75:25','I47','2%');enter('75:25','J47','2%')
steps[-1]['evidence']['executionMethod']='Source all forecast fee assumptions become2%; exact intervening clipboard gesture is unresolved at1fps. These are explicit native entries to preserve visible values; this is not a claim those physical entry gestures were observed.'
emit('75:27','scroll-drivers-up',{'type':'wheel_to_cell','address':'B37'});click('75:29','F44');emit('75:30','scroll-balance',{'type':'wheel_to_cell','address':'B83'});emit('75:31','scroll-cashflow',{'type':'wheel_to_cell','address':'F115'});click('75:32','F115');begin('75:33','F115');emit('75:34','scroll-debt-driver',{'type':'wheel_to_cell','address':'F42'});point('75:35','F42');commit('75:36','F115','=F42');click('75:37','F117');begin('75:38','F117');emit('75:39','scroll-stock-driver',{'type':'wheel_to_cell','address':'F44'});point('75:40','F44');commit('75:40','F117','=F44');click('75:41','F118');click('75:42','F119');begin('75:43','F119','=IF');text('75:44','open-if-attempt','(');key('75:56','cancel-if-attempt',['Escape'],'worksheet.cellEditor');begin('75:57','F119','=MAX');text('75:58','open-max','(');point('75:59','F115');text('76:00','max-comma',',');text('76:05','max-zero','0');text('76:06','close-max-multiply',')*');emit('76:07','scroll-fee-assumption',{'type':'wheel_to_cell','address':'F47'});point('76:08','F47');commit('76:09','F119','=MAX(F115,0)*F47');click('76:10','F117');selected('76:11','F115:F120');emit('76:12','accidental-horizontal-view',{'type':'horizontal_view','firstColumn':'K'},{'firstColumn':11},method='Actual native scrollbar/wheel events reproduce the observed K:AC viewport. The original physical navigation gesture is unobserved.');emit('76:13','return-left-viewport',{'type':'horizontal_view','edge':'home'});click('76:13','F115');selected('76:14','F115:F120');fill('76:15','F115:J120');click('76:16','A115');click('76:17','F116');emit('76:18','scroll-balance-check',{'type':'wheel_to_cell','address':'F97'});click('76:19','F97');selected('76:20','F97:J98');click('76:21','G95');emit('76:22','scroll-balance-up',{'type':'wheel_to_cell','address':'B72'});emit('76:23','scroll-income',{'type':'wheel_to_cell','address':'B63'});click('76:24','B63');selected('76:25','B63:J63');emit('76:27','scroll-driver-block',{'type':'wheel_to_cell','address':'B37'});click('76:29','B37')
click('76:30','B42');whole_row('76:31',42)
for n in range(2):key('76:31',f'insert-interest-driver-row-{n}',['Ctrl++'])
visible_label('interest-expense');click('76:37','E42');begin('76:38','E42','=-');emit('76:39','scroll-income-first-interest-ratio',{'type':'wheel_to_cell','address':'E65'});point('76:40','E65');text('76:41','divide-first-interest-ratio','/');emit('76:42','scroll-balance-first-interest-ratio',{'type':'wheel_to_cell','address':'E80'});emit('76:43','scroll-debt-first-interest-ratio',{'type':'wheel_to_cell','address':'E89'});key('76:46','cancel-first-interest-ratio',['Escape'],'worksheet.cellEditor');begin('76:47','E42','=-');emit('76:48','scroll-income-restarted-interest-ratio',{'type':'wheel_to_cell','address':'E65'});emit('76:49','scroll-balance-restarted-interest-ratio',{'type':'wheel_to_cell','address':'E89'});emit('76:50','scroll-income-again-interest-ratio',{'type':'wheel_to_cell','address':'E65'});point('76:51','E65');text('76:52','divide-restarted-interest-ratio','/');emit('76:52','scroll-debt-denominator',{'type':'wheel_to_cell','address':'E89'});point('76:53','E89');commit('76:55','E42','=-E65/E89');copy_paste('76:56','E42','E42','D42');steps[-1]['evidence']['executionMethod']='Actual relative formula copy to the left reproduces the observed D42 formula; source physical fill/copy gesture falls between frames.';selected('76:57','C45:E45');key('76:57','copy-interest-percentage-format',['Ctrl+C']);selected('76:58','C42:E42');paste_special('76:58');click('76:59','D42');checkpoint('76:59',{'cells':[{'address':'D42','input':'=-D65/D89'}]});click('77:00','E42');click('77:02','F42');click('78:08','F42');selected('78:11','F45:J45');key('78:11','copy-blue-rate-format',['Ctrl+C']);click('78:12','F42');selected('78:13','F42:J42');paste_special('78:15');begin('78:17','F42','2');text('78:17','first-forecast-interest-rate','.5%');commit('78:18','F42','2.5%');enter('78:19','G42','3%');enter('78:20','H42','3%');enter('78:21','I42','3.5%');begin('78:21','J42','3');text('78:21','last-forecast-interest-rate','.5%');commit('78:22','J42','3.5%');checkpoint('78:22',{'cells':[{'address':'C46','input':''},{'address':'D46','input':''},{'address':'E46','input':''}]});emit('78:34','scroll-income-interest-expense',{'type':'wheel_to_cell','address':'F64'});click('78:35','F64');begin('78:36','F65','=-');emit('78:37','scroll-interest-rates',{'type':'wheel_to_cell','address':'F42'});point('78:38','F42');text('78:39','multiply-prior-debt','*');emit('78:39','scroll-income-during-interest-entry',{'type':'wheel_to_cell','address':'F65'});emit('78:40','scroll-balance-during-interest-entry',{'type':'wheel_to_cell','address':'E89'});point('78:41','E89');commit('78:56','F65','=-F42*E89');selected('78:57','F65:H65');fill('78:58','F65:J65');click('78:59','F66');emit('79:00','scroll-balance-checks',{'type':'wheel_to_cell','address':'F100'});click('79:40','H90');emit('79:41','scroll-top-drivers',{'type':'wheel_to_cell','address':'B2'});click('79:42','E9');checkpoint('79:44');checkpoint('79:46');selected('79:47','B17:G19');selected('79:48','B17:J17');checkpoint('79:49');emit('79:50','scroll-top-revenue-drivers',{'type':'wheel_to_cell','address':'B4'});selected('79:51','B4:J11');selected('79:52','B4:J10');checkpoint('79:53');click('80:08','E13');selected('80:09','C9:J10');checkpoint('80:11');click('80:15','F15');checkpoint('80:16');checkpoint('80:17');checkpoint('80:18');checkpoint('80:19');click('80:37','F15');emit('80:38','scroll-driver-bottom-income',{'type':'wheel_to_cell','address':'B52'});emit('80:39','scroll-cashflow-checks',{'type':'wheel_to_cell','address':'F110'});click('80:40','F110');emit('80:41','scroll-net-cash-visible',{'type':'wheel_to_cell','address':'F126'});begin('80:43','F110');point('80:43','F109');text('80:45','add-cfi-fcf-check','+');point('80:46','F114');commit('80:47','F110','=F109+F114');fill('80:47','F110:H110');click('80:48','F111');selected('80:49','F110:G110');begin('80:50','F111','=S');text('80:51','complete-fcf-sum','UM(');drag('80:51','F110','H110','three-year-fcf-range');text('80:52','close-three-year-fcf-sum',')');commit('80:52','F111','=SUM(F110:H110)');click('80:55','F111');key('80:55','clear-cumulative-fcf-scratch',['Delete']);click('81:00','F111');begin('81:01','F111','=SU');text('81:02','finish-dividend-sum','M(');point('81:02','F114','wrong-cfi-sum-target');drag('81:03','F118','H118','correct-common-dividend-range');text('81:04','close-three-year-dividend-sum',')');commit('81:04','F111','=SUM(F118:H118)');click('81:06','F111');scratch_inspection();click('81:10','F111');key('81:10','clear-dividend-scratch',['Delete']);selected('81:11','F110:H110');key('81:11','clear-temporary-fcf-row',['Delete']);click('81:25','E110');click('81:26','C110');begin('81:27','C110','=(');point('81:28','C109');text('81:29','add-cfi-in-conversion','+');point('81:29','C114');text('81:30','close-conversion-numerator',')');text('81:31','divide-parent-net-income','/');emit('81:31','scroll-balance-during-conversion',{'type':'wheel_to_cell','address':'C80'});emit('81:32','scroll-parent-net-income',{'type':'wheel_to_cell','address':'C72'});point('81:33','C72');commit('81:34','C110','=(C109+C114)/C72');click('81:35','C110');key('81:35','conversion-percentage',['Ctrl+Shift+5']);selected('81:36','C110:E110');selected('81:37','C110:J110');key('81:38','fill-permanent-conversion-row',['Ctrl+R']);emit('81:39','scroll-cashflow-slightly',{'type':'wheel','target':'worksheet.grid','angleDelta':[0,-120]});selected('81:40','D109:J109');key('81:40','copy-cfo-subtotal',['Ctrl+C']);selected('81:42','C110:J110');selected('81:43','C110:D110');selected('81:44','C110:E110');checkpoint('81:54');emit('81:55','scroll-income-from-conversion',{'type':'wheel_to_cell','address':'B60'});emit('81:56','scroll-income-header',{'type':'wheel_to_cell','address':'B52'});selected('81:57','C56:J56');checkpoint('82:19');click('82:20','G59');emit('82:20','scroll-cfi',{'type':'wheel_to_cell','address':'F112'});selected('82:21','F112:J114');selected('82:22','G113:J114');selected('82:23','F113:J114');emit('82:25','scroll-balance-from-cfi',{'type':'wheel_to_cell','address':'B80'});emit('82:26','scroll-income-from-cfi',{'type':'wheel_to_cell','address':'B54'});selected('82:27','F54:J56');click('82:28','J56');selected('82:33','C52:E72');checkpoint('82:34');click('84:06','E60');checkpoint('84:06',{'documentName':'101-01-90-Minute-Case-Study-Otis-Blank.xlsx','cells':[{'address':'E60','input':"=-SUM('[Otis-10-K-Raw.xls]operations'!I14:I15)"},{'address':'F46','input':'=IF(F44<0,-F38*$E$39)'},{'address':'C110','input':'=(C109+C114)/C72'}]});emit('84:07','scroll-drivers-retained-external-formula',{'type':'wheel_to_cell','address':'B15'});emit('84:08','scroll-first-row-retained-external-formula',{'type':'wheel_to_cell','address':'B2'});click('84:09','B2');checkpoint('84:26',{'row':2,'column':2,'topRow':1,'firstColumn':1,'sheetName':'Sheet1','showGridLines':False,'cells':[{'address':'B2','input':'Drivers:'},{'address':'B52','input':'Income Statement:'},{'address':'B74','input':'Balance Sheet:'},{'address':'B102','input':'Cash Flow Statement:'}]+[{'address':c+'46','input':''} for c in 'CDE']+[{'address':c+'100','value':0} for c in 'FGHIJ']+[{'address':'C110','display':'113%'},{'address':'J110','display':'130%'}]});checkpoint('84:27')
from otis_late_commit_headers import apply_late_commit_headers
apply_late_commit_headers(steps, root)
steps.sort(key=lambda step:sum(float(v)*60**n for n,v in enumerate(reversed(step['videoTime'].split(':')))))
manifest={'version':1,'sourceVideo':'https://www.youtube.com/watch?v=O2uPNfErmJo','scope':'Ordered native linking/debt/final-check replay; requires actual67:12 incoming workbook, retains external historical formulas and visible wrong/corrected formulas. No final worksheet snapshot is cloned.','sourceReviewComplete':False,'windowSize':[1280,720],'preparedThrough':'84:26','steps':steps,'remainingSourceEvents':[e['id'] for e in source['events'] if e['id'] not in mapped and e.get('application')=='Excel model'],'externalContext':[e for e in source['events'] if e.get('application')!='Excel model'],'unreplayedVisualEffects':[e for e in source['events'] if 'annotation' in e['kind']],'executionStatus':'Prepared action definitions. Actual native execution evidence is retained separately in later-native-replay logs; this definition never claims completed source parity.'}
(root/'linking-native-action-manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n');print(len(steps),'native steps;',len(manifest['remainingSourceEvents']),'remaining model observations')
