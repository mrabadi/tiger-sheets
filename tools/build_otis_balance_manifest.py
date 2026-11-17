import json
from decimal import Decimal
import re
from pathlib import Path
root=Path("docs/validation/otis-90-minute")
source_file=root/"balance-cashflow-visual-steps.json"
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
def source_clipboard_pastes():
 phase_file=root/'balance-clipboard-paste-phases.json';phases=json.loads(phase_file.read_text())
 def record(phase,event_id):
  e=next(item for item in source['events'] if item['id']==event_id);mapped.add(event_id)
  action=dict(phase['action'])
  if action['type']=='modal':action['steps']=[record(child,event_id) for child in action['steps']]
  result={'id':event_id+'-'+phase['id'],'videoTime':phase['videoTime'],'evidence':{'kind':'visual','ledger':source_file.name,'eventId':event_id,'phaseLedger':phase_file.name,'frames':[str((Path(__file__).resolve().parents[1]/phase['sourceFrame']['path']).resolve())],'observed':e['observed'],'executionMethod':phases['qualification']},'action':action}
  if phase.get('expect'):result['expect']=phase['expect']
  return result
 steps.extend(record(phase,phase['eventId']) for phase in phases['steps'])

def column_width(t,width):
 children=[keys_child(t,'select-column-width',['Ctrl+A'],'qt_spinbox_lineedit'),nested(t,'type-column-width',{'type':'text','target':'qt_spinbox_lineedit','text':str(width)}),keys_child(t,'accept-column-width',['Return'],'qt_spinbox_lineedit')]
 dialog(t,'column-width-dialog',{'type':'key_sequence','target':'worksheet.grid','keys':['Alt','O','C','W']},children)

def blue(t,r):
 selected(t,r);key(t,'apply-current-blue',['Alt','1']);emit(t,'choose-blue',{'type':'mouse','target':'tutorial.font.0000ff'})
def checkpoint(t,expect=None):key(t,'visible-checkpoint',[],expect=expect)
def whole_row(t,row):click(t,'B'+str(row));key(t,'whole-row',['Shift+Space'])
def external(t,operation):
 emit(t,'external-text-'+operation,{'type':'external_text_editor','operation':operation},method='QPlainTextEdit external-app adapter for the source Notepad window; actual typing/cut/copy uses the system clipboard, not worksheet mutation.')
def external_keys(t,slug,keys,expect=None):key(t,slug,keys,'replay.externalTextEditor',expect)
def source_to_external(t,cell):click(t,cell);key(t,'copy-source-label',['Ctrl+C']);external(t,'activate');external_keys(t,'paste-source-label',['Ctrl+V'])
def return_model(t):emit(t,'return-model',{'type':'activate_window','documentName':'101-01-90-Minute-Case-Study-Otis-Blank.xlsx'})
def begin(t,cell,prefix='='):click(t,cell);text(t,'begin-'+cell,prefix,'worksheet.grid')
def point(t,cell,slug='point'):click(t,cell,slug)
def commit(t,cell,formula):key(t,'commit-'+cell,['Return'],'worksheet.cellEditor',{'cells':[{'address':cell,'input':native_input(formula)}]})
checkpoint('50:12',{'row':33,'column':7,'cells':[{'address':'B26','input':'Income Statement:'},{'address':'B48','input':'Balance Sheet:'}]})
click('50:46','G33');emit('50:47','expose-cash-flow-header',{'type':'wheel_to_cell','address':'B76'});click('50:48','D87');checkpoint('50:49');checkpoint('50:54');emit('50:58','scroll-back-balance',{'type':'wheel_to_cell','address':'B55'});click('50:59','F52');click('51:00','B22');whole_row('51:03',22)
for n in range(3):key('51:04',f'insert-row-{n}',['Ctrl++'])
for n in range(4):key('51:05',f'insert-row-{n}',['Ctrl++'])
external('51:07','open');return_model('51:08');emit('51:09','expose-balance-sheet',{'type':'wheel_to_cell','address':'B60'});click('51:10','B60');source_to_external('51:11','B59');external_keys('51:13','visible-notepad-checkpoint',[],{'externalText':'Accounts Receivable:\n'});return_model('51:14');click('51:14','B60');source_to_external('51:15','B60');return_model('51:16');source_to_external('51:16','B62');external_keys('51:17','visible-notepad-checkpoint',[],{'externalText':'Accounts Receivable:\nInventory & Other:\nOp. Lease Assets:\n'});return_model('51:18');whole_row('51:19',63);whole_row('51:21',61);click('51:23','B58');whole_row('51:24',58);click('51:25','B67');selected('51:26','B67:B69');key('51:26','copy-liability-labels',['Ctrl+C']);external('51:28','activate');external_keys('51:29','append-liability-labels',['Ctrl+V']);return_model('51:30');click('51:30','B70');whole_row('51:31',70);click('51:32','B71');whole_row('51:33',71);click('51:36','B72');source_to_external('51:38','B72');external_keys('51:39','visible-notepad-checkpoint',[],{'externalText':'Accounts Receivable:\nInventory & Other:\nOp. Lease Assets:\nAccounts Payable:\nAccrued Liabilities:\nContract Liabilities:\nOther Liabilities:\n'});return_model('51:40')
for t,cell in [('51:41','B83'),('51:42','B100'),('51:44','B55')]:emit(t,'observed-scroll',{'type':'wheel_to_cell','address':cell})
click('51:45','B22');external('51:46','activate');external_keys('51:48','cut-seven-label-list',['Ctrl+A','Ctrl+X']);return_model('51:49');click('51:49','B22');key('51:49','paste-seven-labels',['Ctrl+V'],expect={'cells':[{'address':'B'+str(22+n),'input':v} for n,v in enumerate(['Accounts Receivable:','Inventory & Other:','Op. Lease Assets:','Accounts Payable:','Accrued Liabilities:','Contract Liabilities:','Other Liabilities:'])]+[{'address':'B33','input':'Income Statement:'}]});external('51:50','activate');return_model('51:51');selected('51:51','B22:B28');click('51:52','B25');whole_row('51:53',25);key('51:54','insert-liability-separator',['Ctrl++']);click('51:55','B27');selected('51:56','B26:B29');click('51:57','B23')
for t,cell,value in [('51:58','B22','Accounts Receivable % Revenue:'),('52:01','B23','Inventory & Other % COGS:'),('52:04','B24','Op. Lease Assets % OpEx:'),('52:10','B26','Accounts Payable % COGS:'),('52:18','B27','Accrued Liabilities % Total Expenses:')]:enter(t,cell,value)
click('52:08','B26');click('52:13','B27');click('52:21','B28');click('52:23','B27');whole_row('52:24',27);emit('52:25','scroll-income-under-selection',{'type':'wheel_to_cell','address':'B40'});selected('52:27','B40:J40');selected('52:28','B40:J41');emit('52:32','scroll-upper-drivers',{'type':'wheel_to_cell','address':'B22'});click('52:33','B27');click('52:34','B28');enter('52:35','B28','Contract Liabilities % Total Expenses:');click('52:39','B29');click('52:42','B28');enter('52:44','B29','Other Liabilities % Total Expenses:');click('52:46','B22');selected('52:47','B22:B34');selected('52:48','B22:B29');click('52:49','C22');begin('52:51','C22');point('52:51','C60','blank-uncommitted-pointer');key('52:51','cancel-first-ratio',['Escape'],'worksheet.cellEditor')
begin('52:56','D22');point('52:58','D60');text('52:59','division','/');point('53:00','D38');commit('53:01','D22','=D60/D38')
begin('53:02','D23','=-');point('53:04','D61');text('53:05','divide-sum','/SUM(');drag('53:07','D40','D41','cogs-denominator');text('53:11','close-sum',')');commit('53:12','D23','=-D61/SUM(D40:D41)')
begin('53:13','D24','=-');point('53:16','D63');text('53:19','divide-lease-assets','/');point('53:19','D42');commit('53:22','D24','=-D63/D42');click('53:23','D26')
begin('53:25','D26','=-');point('53:27','D68');text('53:28','divide-sum','/SUM(');point('53:30','D40');drag('53:31','D40','D41','cogs-denominator');text('53:32','close-sum',')');commit('53:32','D26','=-D68/SUM(D40:D41)')
begin('53:33','D27','=-');point('53:35','D69');text('53:36','division','/');text('53:38','sum-autocomplete','SU');text('53:40','finish-incomplete-sum','M(');drag('53:40','D40','D41','incomplete-denominator')
dialog('53:42','parenthesis-warning-modal',{'type':'key_sequence','target':'worksheet.cellEditor','keys':['Return']},[keys_child('53:42','warning-visible',[],'@active_modal'),keys_child('53:43','dismiss-parenthesis-warning',['Return'],'@active_modal')],{'formulaDraft':'=-D69/SUM(D40:D41'})
# Original53:44 selects onlyD41 in the formula bar;53:45 retains the partialD4 endpoint.
endpoint_method='Native End/Shift selection and endpoint typing adapt the sampled visible buffer states; exact physical editing keys are not observable. Original frame0213 selects onlyD41 and frame0214 endsD4.'
emit('53:44','select-denominator-endpoint',{'type':'key_sequence','target':'formula.input','keys':['End','Shift+Left','Shift+Left','Shift+Left']},{'formulaDraft':'=-D69/SUM(D40:D41'},method=endpoint_method)
emit('53:45','partial-denominator-endpoint',{'type':'text','target':'formula.input','text':'D4'},{'formulaDraft':'=-D69/SUM(D40:D4'},method=endpoint_method)
emit('53:46','complete-denominator-endpoint',{'type':'text','target':'formula.input','text':'2)'},{'formulaDraft':'=-D69/SUM(D40:D42)'},method=endpoint_method)
emit('53:46','commit-D27',{'type':'key_sequence','target':'formula.input','keys':['Ctrl+Return']},{'cells':[{'address':'D27','input':'=-D69/SUM(D40:D42)'}],'row':27,'column':4,'nameBoxText':'D27'},method='Native Ctrl+Return adapts the observed Ready state retaining D27 in original frame0215; the exact physical commit keys are not observable.');click('53:47','D28');begin('53:48','D28','=-S');key('53:49','remove-partial-sum',['Backspace'],'worksheet.cellEditor');point('53:52','D70');text('53:53','divide-sum','/SUM(');drag('53:55','D40','D42','total-expenses');text('53:56','close-sum',')');commit('53:56','D28','=-D70/SUM(D40:D42)');begin('53:57','D29','=-');point('53:58','D73');text('54:01','divide-sum','/SUM(');drag('54:01','D40','D42','total-expenses');text('54:01','close-sum',')');commit('54:02','D29','=-D73/SUM(D40:D42)');click('54:02','D28','after-commit');emit('54:03','scroll-drivers',{'type':'wheel_to_cell','address':'B20'});selected('54:04','C20:I20');selected('54:05','C20:J20');key('54:05','copy-ratio-formats',['Ctrl+C']);click('54:06','C22');selected('54:07','C22:C25');selected('54:08','C22:F29');selected('54:09','C22:J29');paste_special('54:10');selected('54:10','D22:D23');selected('54:11','D22:D29');fill('54:12','D22:E29');click('54:13','G22');click('54:14','F32');checkpoint('54:15');checkpoint('54:26');click('54:27','G20');key('54:27','inspect-nci-average',['F2']);key('54:27','leave-nci-average',['Escape'],'worksheet.cellEditor');pointer_formula('54:28','G20','=F20');click('54:29','H20')
selected('54:30','F20:J20');key('54:30','copy-average-link-template',['Ctrl+C']);click('54:31','F22');key('54:31','paste-average-link-template',['Ctrl+V']);fill('54:32','F22:J24','down');click('54:33','F26');key('54:34','paste-liability-template',['Ctrl+V']);fill('54:34','F26:J29','down');click('54:35','F28');checkpoint('54:35',{'cells':[{'address':'F28','input':'=AVERAGE(C28:E28)'}]});click('54:36','G28');checkpoint('54:36',{'cells':[{'address':'G28','input':'=F28'}]});click('54:37','F25');click('54:38','F24');key('54:38','inspect-lease-average',['F2']);key('54:43','commit-unchanged-lease-average',['Return'],'worksheet.cellEditor');click('54:44','F23');key('54:44','inspect-inventory-average',['F2']);key('54:44','commit-unchanged-inventory-average',['Return'],'worksheet.cellEditor');click('54:47','F26');click('54:48','F29');click('54:49','F30');click('54:50','F26');emit('54:52','scroll-balance-forecasts',{'type':'wheel_to_cell','address':'F60'});click('54:54','F60');begin('54:55','F60');point('55:02','F38');text('55:05','multiply-ar-driver','*');point('55:05','F22');commit('55:06','F60','=F38*F22');click('55:07','F61');begin('55:08','F61','=-S');text('55:09','sum-inventory','UM(');drag('55:10','F40','F41','inventory-cogs');text('55:12','multiply-inventory-driver',')*');point('55:12','F23');commit('55:13','F61','=-SUM(F40:F41)*F23');click('55:14','F62');click('55:16','F63');begin('55:18','F63','=-');point('55:18','F42');text('55:18','multiply-lease-driver','*');point('55:20','F24');commit('55:21','F63','=-F42*F24');click('55:22','F68');begin('55:23','F68','=-S');text('55:23','payable-sum','UM(');drag('55:25','F40','F41','payable-cogs');text('55:28','multiply-payable-driver',')*');point('55:28','F26');commit('55:29','F68','=-SUM(F40:F41)*F26');click('55:30','F69');begin('55:31','F69','=-');text('55:32','accrued-sum','SUM(');drag('55:35','F40','F42','total-expenses');key('55:36','anchor-expenses-both',['F4'],'worksheet.cellEditor');key('55:36','anchor-expenses-rows',['F4'],'worksheet.cellEditor');text('55:45','multiply-accrued-driver',')*');point('55:45','F27');commit('55:48','F69','=-SUM(F$40:F$42)*F27')
source_clipboard_pastes();other_method='Native suffix deletion adapts the visible55:56partialF token; exact physical edit keys are not observable. The55:57F29 reference is created by an actual native worksheet pointer click, not formula replacement.'
emit('55:56','expose-other-driver-while-editing',{'type':'vertical_view','topRow':22},{'formulaDraft':'=-SUM(F$40:F$42)*F31','topRow':22},method=other_method)
emit('55:56','partial-other-driver',{'type':'key_sequence','target':'worksheet.cellEditor','keys':['End','Backspace','Backspace']},{'formulaDraft':'=-SUM(F$40:F$42)*F'},method=other_method)
for slug,action in [('point-other-driver-F29',{'type':'mouse_cell','address':'F29','gesture':'click'})]:
 emit('55:56',slug,action,{'formulaDraft':'=-SUM(F$40:F$42)*F29'} if action['type']=='mouse_cell' else None,method=other_method)
 steps[-1]['videoTime']='55:57';steps[-1]['evidence']['frames']=['docs/validation/otis-90-minute/source-frames/later/balance-cashflow/frame-0346.jpg']
emit('55:58','commit-F73',{'type':'key_sequence','target':'worksheet.cellEditor','keys':['Ctrl+Return']},{'cells':[{'address':'F73','input':'=-SUM(F$40:F$42)*F29'}],'row':73,'column':6,'nameBoxText':'F73'},method='Native Ctrl+Return adapts the observed Ready state retaining F73 in original frame0347; the exact physical commit keys are not observable.');selected('56:00','E58:E64');selected('56:01','E58:E74');selected('56:02','E58:E64');key('56:02','copy-seven-history-rows',['Ctrl+C']);selected('56:03','E59:E82');key('56:03','copy-historical-balance-region',['Ctrl+C']);selected('56:04','E59:G82');selected('56:05','E59:J82');paste_special('56:05');checkpoint('56:06');selected('56:07','F60:H63');fill('56:08','F60:J63');selected('56:09','F68:G69');fill('56:10','F68:J73');click('56:11','F72');begin('56:18','F72');point('56:18','E72');text('56:18','add-open-group','+(');point('56:25','F63');text('56:26','subtract-prior-lease','-');point('56:28','E63');text('56:29','close-lease-group',')');commit('56:30','F72','=E72+(F63-E63)');selected('56:31','F72:G72');fill('56:32','F72:J72');click('56:34','F73');emit('56:42','scroll-cashflow',{'type':'wheel_to_cell','address':'F90'});click('56:43','F90');begin('56:44','F90');emit('56:49','scroll-up-during-working-capital-formula',{'type':'wheel_to_cell','address':'E60'});point('56:52','E60');text('56:53','subtract-ar','-');point('56:54','F60');text('56:55','add-inventory','+');point('56:56','E61');text('56:57','subtract-inventory','-');point('56:57','F61');text('56:59','add-lease-assets','+');point('56:59','E63');text('57:01','subtract-lease-assets','-');point('57:01','F63');text('57:15','add-liability-sum','+SUM(');drag('57:17','F68','F70','current-operating-liabilities');text('57:19','subtract-prior-liability-sum',')-SUM(');drag('57:21','E68','E70','prior-operating-liabilities');text('57:22','add-lease-other-sum',')+SUM(');drag('57:24','F72','F73','current-lease-other');text('57:25','subtract-prior-lease-other-sum',')-SUM(');drag('57:26','E72','E73','prior-lease-other');text('57:26','close-working-capital',')');commit('57:27','F90','=E60-F60+E61-F61+E63-F63+SUM(F68:F70)-SUM(E68:E70)+SUM(F72:F73)-SUM(E72:E73)');emit('57:28','scroll-cashflow-fully',{'type':'wheel_to_cell','address':'B90'});fill('57:29','F90:J90');checkpoint('57:30');click('57:32','F92');checkpoint('57:33');checkpoint('57:35');checkpoint('57:36');click('57:47','F90');emit('57:51','scroll-balance',{'type':'wheel_to_cell','address':'B55'});emit('57:53','scroll-cashflow-again',{'type':'wheel_to_cell','address':'B90'})
click('58:53','H94')
for t,cell in [('58:54','B56'),('58:55','B34'),('58:56','B22')]:emit(t,'scroll-upward',{'type':'wheel_to_cell','address':cell})
click('58:57','B31');whole_row('58:58',31)
for n in range(5):key('58:59',f'insert-row-{n}',['Ctrl++'])
click('59:00','B31');key('59:00','whole-column-b',['Ctrl+Space']);whole_row('59:01',31)
key('59:02','review-comments-onboarding',['Alt','R'],expect={'ribbon':'Review','row':31})
emit('59:03','dismiss-comments-onboarding',{'type':'key_sequence','target':'worksheet.grid','keys':['Escape']},{'row':31},method='Actual Escape dismisses the observed onboarding; the source physical dismissal gesture is not visible between frames.')
for n in range(4):key('59:04',f'insert-row-{n}',['Ctrl++'])
begin('59:05','B31','C');text('59:05','type-capex-label','apEx % R');text('59:07','finish-capex-label','evenue:');commit('59:07','B31','CapEx % Revenue:');click('59:08','B32');whole_row('59:10',31);emit('59:11','scroll-top-drivers',{'type':'wheel_to_cell','address':'B2'});checkpoint('59:12');checkpoint('59:16');emit('59:17','scroll-new-driver-block',{'type':'wheel_to_cell','address':'B31'});click('59:19','B32');begin('59:20','B32','D');text('59:20','partial-da-label','&A Reve');text('59:22','temporary-da-label','nue:');key('59:24','select-da-correction',['Ctrl+A'],'worksheet.cellEditor');text('59:24','correct-da-label','D&A % Revenue:');commit('59:24','B32','D&A % Revenue:');click('59:25','B33')
for t,cell in [('59:26','B43'),('59:27','B65'),('59:28','B98')]:emit(t,'scroll-cashflow-labels',{'type':'wheel_to_cell','address':cell})
click('59:29','B98');key('59:29','copy-pension-label',['Ctrl+C'])
external('59:32','activate');external_keys('59:32','paste-pension-label',['Ctrl+V']);return_model('59:33');click('59:33','B103');click('59:34','B104');key('59:34','copy-acquisitions-label',['Ctrl+C']);external('59:35','activate');external_keys('59:36','append-acquisition-label',['Ctrl+V','Return']);return_model('59:37');click('59:37','B107');selected('59:38','B108:B111');selected('59:39','B108:B112');key('59:39','copy-financing-labels',['Ctrl+C']);external('59:39','activate');external_keys('59:40','append-financing-labels',['Ctrl+V','Return']);return_model('59:41');click('59:41','B115');key('59:41','copy-fx-label',['Ctrl+C']);external('59:42','activate');external_keys('59:42','append-fx-label',['Ctrl+V'],{'externalText':'(+/-) Pensions/Other Items:\n(-) Acquisitions & Other:\n\n(+/-) Change in Debt:\n(-) Common Dividends:\n(-) Stock Repurchases:\n(-) Dividends to NCI:\n(+/-) Other Items:\n\nFX Rate Effects:\n'});return_model('59:43');emit('59:43','scroll-up-from-cashflow',{'type':'wheel_to_cell','address':'B65'});emit('59:44','scroll-new-driver-area',{'type':'wheel_to_cell','address':'B33'});click('59:45','B33');whole_row('59:46',34)
for n in range(3):key('59:48',f'insert-row-{n}',['Ctrl++'])
external('59:49','activate');external_keys('59:50','cut-full-driver-list',['Ctrl+A','Ctrl+X']);return_model('59:51')
dialog('59:51','paste-size-warning',{'type':'key_sequence','target':'worksheet.grid','keys':['Ctrl+V']},[keys_child('59:51','paste-warning-visible',[],'@active_modal'),keys_child('59:52','cancel-paste-size-warning',['Escape'],'@active_modal')])
steps[-1]['evidence']['executionMethod']='Actual native paste warning and Cancel outcome; Escape invokes Cancel because the source cursor is over Cancel but its physical dismissal gesture falls between sampled frames.'
click('59:53','B34');key('59:54','retry-driver-list-paste',['Ctrl+V'],expect={'cells':[{'address':'B34','input':'(+/-) Pensions/Other Items:'},{'address':'B35','input':'(-) Acquisitions & Other:'},{'address':'B37','input':'(+/-) Change in Debt:'},{'address':'B43','input':'FX Rate Effects:'},{'address':'B46','input':'Income Statement:'}]});external('59:54','activate');external_keys('59:55','empty-notepad-checkpoint',[],{'externalText':''});return_model('59:56');whole_row('59:56',35);click('59:57','B34','pension-cut-source');key('59:57','cut-pension-label',['Ctrl+X']);click('59:57','B33','pension-move-destination');key('59:57','move-pension-label',['Ctrl+V']);click('59:58','B34');begin('59:59','B33','P');text('59:59','remove-pension-prefix','ensions/Other Items:');commit('60:01','B33','Pensions/Other Items:');click('60:02','D31');click('60:03','D32');click('60:04','B32');click('60:05','B37');begin('60:07','B35','A');text('60:07','remove-acquisition-prefix','cquisitions & Other:');commit('60:09','B35','Acquisitions & Other:');click('60:10','B33');selected('60:11','B31:B33');key('60:11','bold-three-driver-labels',['Ctrl+B']);click('60:12','B37');begin('60:13','B37','C');text('60:13','remove-debt-prefix','hange in Debt:');commit('60:14','B37','Change in Debt:');whole_row('60:15',38);selected('60:16','B38:B43','labels-to-move');key('60:16','cut-label-block',['Ctrl+X']);click('60:16','B39','shifted-label-destination');key('60:16','move-labels-down',['Ctrl+V']);click('60:17','B39');begin('60:18','B39','C');text('60:20','remove-dividend-prefix','ommon Dividends:');key('60:22','dividend-label-before-colon',['Left'],'worksheet.cellEditor');text('60:22','append-net-income-suffix',' % Net Income');key('60:26','select-dividend-label-correction',['Ctrl+A'],'worksheet.cellEditor');text('60:26','duplicate-net-label','Common Dividends % Net Net Income:');commit('60:28','B39','Common Dividends % Net Net Income:');begin('60:30','B40','S');text('60:30','remove-repurchase-prefix','tock Repurchases:');commit('60:31','B40','Stock Repurchases:');begin('60:32','B41','D');text('60:32','remove-nci-dividend-prefix','ividends to NCI:');key('60:35','nci-label-before-colon',['Left'],'worksheet.cellEditor');text('60:35','append-nci-net-income',' % NCI Net Income');commit('60:40','B41','Dividends to NCI % NCI Net Income:');click('60:41','B42');enter('60:42','B42','Other Items:');click('60:42','B42','observed-other-items-cut-source');steps[-1]['evidence']['executionMethod']='Native pointer counterpart restores the source-observed B42 cut anchor after this adapter commits with Enter. The physical source commit/reselection gesture falls between frames; no claim of an observed extra click.';key('60:42','cut-other-label',['Ctrl+X']);click('60:43','B43');key('60:43','move-other-label',['Ctrl+V'],expect={'cells':[{'address':'B42','input':''},{'address':'B43','input':'Other Items:'}]});click('60:44','B44');whole_row('60:45',45);click('60:47','B44');enter('60:48','B44','FX Rate Effects % Revenue:');checkpoint('60:49');whole_row('60:50',43);emit('60:52','scroll-down-slightly',{'type':'wheel','target':'worksheet.grid','angleDelta':[0,-120]});emit('60:55','scroll-up-slightly',{'type':'wheel','target':'worksheet.grid','angleDelta':[0,120]});selected('60:58','B36:B37');selected('60:59','B37:B38');selected('61:00','B37:C38');click('61:01','B43');click('61:03','C31');begin('61:04','C31','=-');emit('61:05','scroll-income-during-capex-entry',{'type':'wheel_to_cell','address':'C50'});emit('61:06','scroll-cashflow-during-capex-entry',{'type':'wheel_to_cell','address':'C106'});point('61:07','C106');text('61:08','divide-capex','/');emit('61:09','scroll-revenue',{'type':'wheel_to_cell','address':'C50'});point('61:10','C49','wrong-service-denominator');point('61:11','C50','correct-total-revenue-denominator');commit('61:11','C31','=-C106/C50');selected('61:14','C26:F26');selected('61:15','C26:J26');key('61:15','copy-percent-format-row',['Ctrl+C']);selected('61:16','C31:J31');paste_special('61:16');click('61:17','C32');begin('61:18','C32');emit('61:19','scroll-balance-during-da-entry',{'type':'wheel_to_cell','address':'C70'});emit('61:20','scroll-up-during-da-entry',{'type':'wheel_to_cell','address':'C50'});emit('61:21','scroll-cashflow-during-da-entry',{'type':'wheel_to_cell','address':'C100'});point('61:22','C100');text('61:23','divide-da','/');emit('61:24','scroll-revenue-during-da-entry',{'type':'wheel_to_cell','address':'C50'});point('61:25','C50');commit('61:26','C32','=C100/C50');begin('61:27','C33');key('61:27','cancel-first-pension-draft',['Escape'],'worksheet.cellEditor');click('61:29','B32');click('61:31','B33');enter('61:32','B33','Pensions/Other Items % Revenue:');checkpoint('61:33');begin('61:34','C33');emit('61:35','scroll-balance-during-pension-entry',{'type':'wheel_to_cell','address':'C70'});emit('61:36','scroll-cashflow-during-pension-entry',{'type':'wheel_to_cell','address':'C101'});point('61:37','C101');text('61:37','divide-pension','/');emit('61:38','scroll-up-revenue',{'type':'wheel_to_cell','address':'C50'});point('61:39','C50');commit('61:40','C33','=C101/C50');begin('61:41','C35');emit('61:42','scroll-balance-during-acquisition-entry',{'type':'wheel_to_cell','address':'C70'});emit('61:43','scroll-cashflow-during-acquisition-entry',{'type':'wheel_to_cell','address':'C107'});point('61:44','C107');commit('61:45','C35','=C107');selected('61:46','C35:E35');fill('61:47','C35:E35');click('61:49','F35');click('62:38','F35');begin('62:42','F35','-');text('62:42','acquisition-assumption','75');commit('62:43','F35','-75');pointer_formula('62:44','G35','=F35');fill('62:45','G35:J35');selected('62:47','C49:J49');click('62:48','C35');selected('62:49','C52:J52');key('62:49','copy-accounting-row',['Ctrl+C']);click('62:50','C35');key('62:50','legacy-edit-prefix',['Alt','E']);dialog('62:51','legacy-paste-special',{'type':'key_sequence','target':'worksheet.grid','keys':['S']},[keys_child('62:51','formats-choice',['T'],'pasteSpecial.dialog'),keys_child('62:51','accept-accounting-formats',['Return'],'pasteSpecial.dialog')]);click('62:52','F36');click('62:53','D39');click('62:54','C39');begin('62:56','B39','D');text('62:56','remove-common-word','ividends % Net Net Income:');commit('62:58','B39','Dividends % Net Net Income:');begin('62:59','C39','=-');emit('63:00','scroll-balance-during-dividend-entry',{'type':'wheel_to_cell','address':'C70'});emit('63:01','scroll-cashflow-during-dividend-entry',{'type':'wheel_to_cell','address':'C112'});point('63:03','C112');text('63:03','divide-common-dividends','/');emit('63:04','scroll-income-during-dividend-entry',{'type':'wheel_to_cell','address':'C66'});point('63:06','C66');commit('63:07','C39','=-C112/C66');click('63:09','C31');key('63:09','copy-payout-format',['Ctrl+C']);click('63:10','C39');paste_special('63:10');checkpoint('63:11');selected('63:12','C31:F31','partial-percent-row');selected('63:12','C31:I31','extend-percent-row');selected('63:12','C31:J31');key('63:13.100','copy-one-percent-format-row',['Ctrl+C']);selected('63:13.400','C31:J32');selected('63:14','C31:J33');key('63:14','legacy-edit-prefix',['Alt','E']);dialog('63:15','legacy-paste-percent-formats',{'type':'key_sequence','target':'worksheet.grid','keys':['S']},[keys_child('63:15','formats-choice',['T'],'pasteSpecial.dialog'),keys_child('63:15','accept-three-percent-rows',['Return'],'pasteSpecial.dialog')]);checkpoint('63:15',{'cells':[{'address':'C32','display':'1.4%'},{'address':'C33','display':'0.4%'}]});selected('63:16','C33:F33');selected('63:17','C33:J33');key('63:17','copy-pension-format-row',['Ctrl+C']);click('63:18','C39');selected('63:19','C39:J39');paste_special('63:19');click('63:20','B41');begin('63:23','B41','N');text('63:23','temporary-nci-label','CI % NCI Net Income:');key('63:24','select-final-nci-label',['Ctrl+A'],'worksheet.cellEditor');text('63:24','finish-nci-label','NCI Dividends % NCI Net Income:');commit('63:24','B41','NCI Dividends % NCI Net Income:');click('63:25','C41');whole_row('63:26',39);emit('63:26','far-right-viewport',{'type':'horizontal_view','firstColumn':'XEL'},{'firstColumn':16366},method='Actual native scrollbar End and wheel events reproduce the observed XEL:XFD viewport while preserving the whole-row selection. The original physical navigation gesture is unobserved.');emit('63:27','return-left-viewport',{'type':'horizontal_view','edge':'home'});click('63:27','D39');selected('63:28','C39:F39');selected('63:29','C39:J39');key('63:29','copy-nci-dividend-format-row',['Ctrl+C'])
click('63:30','C41','single-format-paste-anchor');paste_special('63:30.333');
for t,cell,direction in [('63:30.500','C42','Down'),('63:30.600','C43','Down'),('63:30.767','C44','Down'),('63:31.100','C43','Up'),('63:31.267','C42','Up'),('63:31.433','C41','Up')]:
 emit(t,'observed-active-'+cell,{'type':'key_sequence','target':'worksheet.grid','keys':[direction]},{'row':int(cell[1:]),'column':3},method='Actual native arrow navigation reproduces the densely observed single-cell sequence; the original physical keys are unobserved.')
click('63:32','C39');key('63:32','inspect-existing-common-payout',['F2']);key('63:32','leave-existing-common-payout',['Escape'],'worksheet.cellEditor');begin('63:34','C41','=-');emit('63:34','scroll-income-during-nci-entry',{'type':'wheel_to_cell','address':'C66'});emit('63:35','scroll-cashflow-during-nci-entry',{'type':'wheel_to_cell','address':'C114'});point('63:37','C114');text('63:38','divide-nci-dividends','/');emit('63:39','scroll-nci-income-reversal',{'type':'wheel_to_cell','address':'C99'});point('63:40','C99');commit('63:42','C41','=-C114/C99');click('63:43','C44');begin('63:44','C44');emit('63:44','scroll-balance-during-fx-entry',{'type':'wheel_to_cell','address':'C85'});emit('63:45','scroll-cashflow-during-fx-entry',{'type':'wheel_to_cell','address':'C118'});point('63:46','C118');text('63:47','divide-fx','/');emit('63:48','scroll-revenue-during-fx-entry',{'type':'wheel_to_cell','address':'C50'});point('63:50','C50');commit('63:52','C44','=C118/C50');click('63:53','C41');selected('63:54','C41:E41');selected('63:55','C41:J41');key('63:55','copy-fx-percent-formats',['Ctrl+C']);selected('63:56','C44:J44');paste_special('63:56');checkpoint('63:57');fill('63:58','C31:E33');selected('63:59','C39:E41');fill('64:00','C39:E41');fill('64:01','C44:E44');click('64:02','F31');emit('64:03','scroll-upper-drivers',{'type':'wheel_to_cell','address':'B22'});selected('64:04','C31:E33');checkpoint('64:05');selected('64:06','C39:E41');click('64:07','E41');click('64:08','C41');click('64:12','F29');selected('64:16','F29:J29');key('64:16','copy-average-link-template',['Ctrl+C'])
for t,cell in [('64:17','F31'),('64:18','F32'),('64:19','F33')]:click(t,cell);key(t,'paste-average-link-template',['Ctrl+V'])
click('64:20','G33');click('64:21','F33');selected('64:23','F33:J33');key('64:23','copy-pension-average-template',['Ctrl+C']);click('64:24','F40');click('64:25','F41');key('64:26','paste-nci-template',['Ctrl+V']);click('64:27','G41');click('64:28','F41');selected('64:30','F41:J41');key('64:30','copy-nci-template',['Ctrl+C']);click('64:31','F44');key('64:31','paste-fx-template',['Ctrl+V']);key('64:31','inspect-fx-average',['F2']);key('64:31','leave-fx-average',['Escape'],'worksheet.cellEditor');click('64:32','J44');click('64:33','G43');click('64:34','F39');begin('64:38','B39','D');text('64:38','remove-duplicated-net','ividends % Net Income:');commit('64:41','B39','Dividends % Net Income:');click('64:43','F39');click('64:51','F39');begin('64:54','F39','3');text('64:54','common-payout-35','5%');commit('64:55','F39','35%');begin('64:56','G39','3');text('64:56','common-payout-375','7.5%');commit('64:57','G39','37.5%');enter('64:58','H39','40%');enter('64:58','I39','40%');begin('64:59','J39','4');text('64:59','common-payout-40','0%');commit('65:00','J39','40%');selected('65:01','G39:J39');key('65:01','open-recent-font-color',['Alt','1']);emit('65:02','recent-blue-swatch',{'type':'mouse','target':'tutorial.fontPalette','childAccessibleName':'Font color #0000FF'});click('65:04','G41');click('65:05','G39');key('65:05','copy-blue-common-assumption',['Ctrl+C']);click('65:06','F39');paste_special('65:06');checkpoint('65:07');click('65:08','G35');click('65:09','F35');key('65:09','open-acquisition-font-color',['Alt','1']);emit('65:10','recent-blue-swatch',{'type':'mouse','target':'tutorial.fontPalette','childAccessibleName':'Font color #0000FF'});click('65:11','E33');click('65:13','F44');emit('65:15','scroll-balance',{'type':'wheel_to_cell','address':'B68'});emit('65:17','scroll-cashflow',{'type':'wheel_to_cell','address':'B98'});click('65:18','F98');begin('65:19','F98');emit('65:20','scroll-income-parent-profit',{'type':'wheel_to_cell','address':'F66'});point('65:22','F66');commit('65:23','F98','=F66');click('65:24','E99');begin('65:25','F99','=-');point('65:25','F65');commit('65:26','F99','=-F65');click('65:27','F100');begin('65:28','F100');emit('65:29','scroll-revenue',{'type':'wheel_to_cell','address':'F50'});point('65:30','F50');text('65:31','multiply-da-ratio','*');point('65:32','F32');commit('65:33','F100','=F50*F32');checkpoint('65:34');selected('65:35','E98:E100');selected('65:36','E98:E103');selected('65:37','E98:E106');selected('65:38','E98:E119');selected('65:39','E98:E120');key('65:40','copy-full-cashflow-history-formats',['Ctrl+C']);selected('65:41','E98:J120');paste_special('65:42');click('65:43','F101');begin('65:44','F101');emit('65:45','scroll-income-pension-entry',{'type':'wheel_to_cell','address':'F50'});point('65:46','F50');text('65:47','multiply-pension-ratio','*');emit('65:47','scroll-pension-driver',{'type':'wheel_to_cell','address':'F33'});point('65:49','F33');commit('65:50','F101','=F50*F33');click('65:51','F102');checkpoint('65:51',{'cells':[{'address':'F102','input':'=E72-F72+E73-F73+E75-F75+SUM(F80:F82)-SUM(E80:E82)+SUM(F84:F85)-SUM(E84:E85)'}]});copy_paste('65:52','E103','E103','F103');click('65:54','F106');begin('65:55','F106','=-');point('65:57','F50');text('65:58','multiply-capex-ratio','*');emit('65:58','scroll-capex-driver',{'type':'wheel_to_cell','address':'F31'});point('66:00','F31');commit('66:01','F106','=-F50*F31');click('66:02','F107');begin('66:03','F107');emit('66:04','scroll-acquisition-driver',{'type':'wheel_to_cell','address':'F35'});point('66:05','F35');commit('66:06','F107','=F35');pointer_formula('66:07','F108','=SUM(F106:F107)');checkpoint('66:08');whole_row('66:09',111);click('66:10','F112');begin('66:11','F112','=-');point('66:13','F66');text('66:14','multiply-common-payout','*');emit('66:14','scroll-common-payout-driver',{'type':'wheel_to_cell','address':'F39'});point('66:16','F39');commit('66:17','F112','=-F66*F39');click('66:18','F113');whole_row('66:19',113);click('66:20','F114');begin('66:21','F114','=-');point('66:22','F99');text('66:23','multiply-nci-payout','*');emit('66:24','scroll-nci-payout-driver',{'type':'wheel_to_cell','address':'F41'});point('66:26','F41');commit('66:27','F114','=-F99*F41');click('66:28','F118');begin('66:29','F118');point('66:31','F50');text('66:32','multiply-fx-ratio','*');point('66:33','F44');commit('66:33','F118','=F50*F44');click('66:34','F119');click('66:35','E120');key('66:36','inspect-net-cash-precedents',['F2']);key('66:38','commit-net-cash-unchanged',['Return'],'worksheet.cellEditor');copy_paste('66:39','E120','E120','F120');click('66:40','E116');fill('66:41','E116:F116');selected('66:43','F98:F104');selected('66:44','F98:F122');selected('66:45','F98:J119');selected('66:46','F98:J120');key('66:47','fill-all-cashflow-forecasts',['Ctrl+R']);click('66:48','G112')
for t,cell in [('66:49','B115'),('66:50','B96'),('66:51','B85'),('66:53','B71')]:emit(t,'scroll-to-balance',{'type':'wheel_to_cell','address':cell})
selected('66:54','F71:J76');click('66:57','F75');emit('66:58','scroll-cashflow-debt',{'type':'wheel_to_cell','address':'B111'});whole_row('67:00',111);selected('67:02','E112:G112');whole_row('67:03',113);whole_row('67:04',115);emit('67:06','scroll-cashflow-lower',{'type':'wheel','target':'worksheet.grid','angleDelta':[0,-120]});emit('67:07','scroll-balance-lower',{'type':'wheel_to_cell','address':'B87'});click('67:08','F87');click('67:09','F83');emit('67:10','scroll-balance-assets',{'type':'wheel_to_cell','address':'B71'});click('67:11','G74');checkpoint('67:12',{'row':74,'column':7,'cells':[{'address':'B46','input':'Income Statement:'},{'address':'B68','input':'Balance Sheet:'},{'address':'B96','input':'Cash Flow Statement:'}]})
# Original65:02/frame0891 retains the selectedG39 percentage in the Ready
# formula bar after choosing blue. This is an observation, not another action.
next(step for step in steps if step['id']=='balance-cashflow-523-recent-blue-swatch').setdefault('expect',{})['formulaDraft']='37.5%'
# Dense originals retain dimensions through Paste Special. The native held-Shift
# counterpart resets the adapter's anchor; no source keypress is claimed.
modal_count_file=root/'balance-modal-count-phases.json'
for phase in json.loads(modal_count_file.read_text())['cases']:
 selection=[step for step in steps if step['evidence']['eventId']==phase['selectionEvent'] and step['action']['type'] in ['wheel_to_cell','mouse_cell']]
 assert len(selection)==5,(phase['selectionEvent'],len(selection))
 position=steps.index(selection[0]);anchor=phase['anchor'];evidence=dict(selection[0]['evidence'])
 evidence.update(frames=[str((root/frame).resolve()) for frame in phase['countFrames']],sourcePhaseLedger=modal_count_file.name,executionMethod='Inferred native anchor reset and held-Shift range counterpart for the observed dimensions. Physical source keys and intermediate adapter selections are unobserved.')
 replacements=[]
 for suffix,action in [('anchor-expose',{'type':'wheel_to_cell','address':anchor}),('anchor-click',{'type':'mouse_cell','address':anchor,'gesture':'click'}),('held-count',{'type':'key_sequence','target':'worksheet.grid','keys':phase['heldKeys'],'releaseModifiers':False})]:
  record={'id':phase['selectionEvent']+'-'+suffix,'videoTime':selection[0]['videoTime'],'evidence':evidence,'action':action}
  if suffix=='held-count':record['expect']={'nameBoxText':phase['count']}
  replacements.append(record)
 steps[position:position+len(selection)]=replacements
 modal=next(step for step in steps if step['evidence']['eventId']==phase['modalEvent'] and step['action']['type']=='modal')
 children=modal['action']['steps'];assert len(children)==2 and children[-1]['action']['keys']==['Return']
 modal['videoTime']=phase['firstVisibleModal'];children[0]['videoTime']=phase['firstVisibleModal']
 children[0].setdefault('expect',{})['nameBoxText']=phase['count']
 for record in [modal,children[0]]:
  record['evidence']=dict(record['evidence'],frames=[str((root/frame).resolve()) for frame in phase['countFrames']],sourcePhaseLedger=modal_count_file.name)
 released_evidence=dict(modal['evidence'],frames=[str((root/phase['closedFrame']).resolve())],executionMethod='Native modifier release and acceptance adapt the first observed closed-modal anchor. Physical source keys and their within-frame order are unobserved.')
 release={'id':phase['modalEvent']+'-release-held-count','videoTime':phase['firstClosedModal'],'evidence':released_evidence,'action':{'type':'modifier_release','target':'pasteSpecial.dialog','keys':['Shift']},'expect':{'nameBoxText':anchor}}
 children.insert(-1,release);children[-1]['videoTime']=phase['firstClosedModal'];children[-1]['evidence']=released_evidence
 modal.setdefault('expect',{})['nameBoxText']=anchor
 for observed in phase['postCloseAnchors']:
  for suffix,action in [('expose',{'type':'wheel_to_cell','address':observed['address']}),('select',{'type':'mouse_cell','address':observed['address'],'gesture':'click'})]:
   record={'id':phase['modalEvent']+'-post-close-'+observed['address']+'-'+suffix,'videoTime':observed['time'],'evidence':dict(released_evidence,frames=[str((root/observed['frame']).resolve())],observed='Dialog gone; '+observed['address']+' is the visible Name Box/active anchor.'),'action':action}
   if suffix=='select':record['expect']={'nameBoxText':observed['address']}
   steps.append(record)
 if phase['retimedCheckpoint']:
  checkpoint=next(step for step in steps if step['id']==phase['retimedCheckpoint']['id']);checkpoint['videoTime']=phase['retimedCheckpoint']['time']
# The source first declines the real correction prompt, then acknowledges the
# ordinary parenthesis warning. Each child runs after its dialog event loop.
correction_file=root/'balance-formula-correction-phases.json'
correction=json.loads(correction_file.read_text())
modal=next(step for step in steps if step['evidence']['eventId']==correction['eventId'])
old_children=modal['action']['steps'];assert len(old_children)==2
method='Native Escape rejection (the real Yes/No dialog returns No) and OK adapt the observed rejected-correction/warning sequence; physical source keys are unobserved. No formula or header text is substituted.'
def correction_evidence(frames):
 return dict(modal['evidence'],frames=[str((root/frame).resolve()) for frame in frames],sourcePhaseLedger=correction_file.name,executionMethod=method)
modal['videoTime']=correction['firstInformation'];modal['action']['dialogSequence']=True
old_children[0]['videoTime']=correction['firstInformation'];old_children[0]['evidence']=correction_evidence(correction['informationFrames']);old_children[0]['expect']={'modal':'worksheet.formulaCorrection'}
decline={'id':correction['eventId']+'-decline-formula-correction','videoTime':correction['firstWarning'],'evidence':correction_evidence(correction['warningFrames']),'action':{'type':'key_sequence','target':'@active_modal','keys':['Escape']}}
warning={'id':correction['eventId']+'-ordinary-warning-visible','videoTime':correction['firstWarning'],'evidence':correction_evidence(correction['warningFrames']),'action':{'type':'key_sequence','target':'@active_modal','keys':[]}}
old_children[-1]['videoTime']=correction['firstClosed'];old_children[-1]['evidence']=correction_evidence(correction['closedFrames'])
modal['action']['steps']=[old_children[0],decline,warning,old_children[-1]]
modal['evidence']=correction_evidence(correction['informationFrames']+correction['warningFrames'])
# Preserve the source's intermediate dimensions rather than completing each
# selected range before its first observable checkpoint.
held_file=root/'balance-held-selection-phases.json'
held=json.loads(held_file.read_text());by_id={step['id']:step for step in steps}
assert len(by_id)==len(steps)
for identifier in held['removeIds']:
 assert identifier in by_id,identifier
 steps.remove(by_id[identifier])
def held_evidence(event_id,frames,observed=None):
 observed_event=next(e for e in source['events'] if e['id']==event_id)
 return {'kind':'visual','ledger':source_file.name,'eventId':event_id,'frames':[str((root/frame).resolve()) for frame in frames],'observed':observed or observed_event['observed'],'sourcePhaseLedger':held_file.name,'executionMethod':held['qualification']}
for patch in held['updates']:
 record=by_id[patch['id']]
 assert record['action']==patch['beforeAction'] and record['videoTime']==patch['beforeVideoTime'],patch['id']
 record['videoTime']=patch['videoTime'];record['evidence']=held_evidence(record['evidence']['eventId'],patch['frames'])
 if 'action' in patch:record['action']=patch['action']
 if 'expect' in patch:record.setdefault('expect',{}).update(patch['expect'])
for phase in held['records']:
 assert phase['id'] not in by_id,phase['id']
 record={key:phase[key] for key in ['id','videoTime','action']}
 record['evidence']=held_evidence(phase['eventId'],phase['frames'],phase['observed'])
 if 'expect' in phase:record['expect']=phase['expect']
 steps.append(record);by_id[phase['id']]=record
from otis_late_commit_headers import apply_late_commit_headers
apply_late_commit_headers(steps, root)
steps.sort(key=lambda step:sum(float(v)*60**n for n,v in enumerate(reversed(step["videoTime"].split(":")))))
manifest={'version':1,'sourceVideo':'https://www.youtube.com/watch?v=O2uPNfErmJo','scope':'Ordered native balance/cash-flow replay; requires actual50:12 incoming workbook. External Notepad actions use an explicitly labeled QPlainTextEdit adapter and real system clipboard. No worksheet metadata is cloned.','sourceReviewComplete':False,'windowSize':[1280,720],'preparedThrough':'67:12','steps':steps,'remainingSourceEvents':[e['id'] for e in source['events'] if e['id'] not in mapped and e.get('application')=='Excel model'],'externalContext':[e for e in source['events'] if e.get('application')!='Excel model'],'unreplayedVisualEffects':[e for e in source['events'] if 'annotation' in e['kind']],'executionStatus':'Prepared action definitions. Actual native execution evidence is retained separately in later-native-replay logs; this definition never claims completed source parity.'}
(root/'balance-native-action-manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n');print(len(steps),'native steps;',len(manifest['remainingSourceEvents']),'remaining model observations')
