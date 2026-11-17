#!/usr/bin/env python3
"""Observed blue presenter overlays on actual native worksheet regions.
No cells or workbook metadata are assigned. Source rectangles target the native
counterpart cell regions; original screen pixels/physical drag samples differ.
"""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'docs/validation/otis-90-minute';events={}
for f in ['income-projections-visual-steps.json','balance-cashflow-visual-steps.json','linking-debt-checks-visual-steps.json']:
 for e in json.loads((BASE/f).read_text())['events']:events[e['id']]=(f,e)
steps=[];covered=set()
def add(id,time,cell_range=None):
 f,e=events[id];covered.add(id);action={'type':'annotation','target':'worksheet.grid','operation':'rectangle' if cell_range else 'clear'}
 if cell_range:action.update(cellRange=cell_range,color='#0000FF',lineWidth=3)
 seconds=sum(int(v)*60**n for n,v in enumerate(reversed(time.split(':'))));segment,start=('income',2110) if id.startswith('income') else ('balance-cashflow',3011) if id.startswith('balance') else ('final',4031);frame=f'/tmp/tigersheets-video-review/{segment}-frames/frame-{seconds-start:04d}.jpg'
 assert Path(frame).exists(),frame
 steps.append({'id':id+'-native-overlay-'+str(len(steps)+1),'videoTime':time,'evidence':{'kind':'visual','ledger':f,'eventId':id,'frames':[frame],'observed':e['observed'],'executionMethod':'Ephemeral blue presenter rectangle on the actual native counterpart cell range, drawn with genuine pointer events. It never formats cells. Original drag samples and exact pixel geometry are adapted; time denotes a source-visible state.'},'action':action})
add('income-081','39:37','C4:D5');add('income-081','39:39');add('income-081','39:39','D2:D5');add('income-081','39:42','E2:E5');add('income-081','39:46')
add('income-197','45:34','C12:E13');add('income-197','45:35','F13:J13');add('income-197','45:38')
add('income-209','46:10','I22:J23');add('income-209','46:12','J19');add('income-209','46:14')
add('income-212','46:21','F24:J24');add('income-212','46:36')
add('balance-cashflow-008','50:49','D82:E82');add('balance-cashflow-009','50:54','C82');add('balance-cashflow-009','50:58')
add('balance-cashflow-138','54:15','D22:E29');add('balance-cashflow-139','54:26')
for id,t,r in [('240','57:33','F90'),('241','57:35','G90:J90'),('242','57:36','D90:E90'),('243','57:47',None),('267','59:12','B2:J10'),('268','59:16',None)]:add('balance-cashflow-'+id,t,r)
for id,t,r in [('085','70:23','B99:J102'),('086','70:25',None),('162','73:02','B120:J120'),('163','73:09',None),('229','75:01','C119:D120'),('230','75:05','E119'),('230','75:09',None),('367','79:44','B4:J10'),('368','79:46',None),('383','80:16','B4'),('384','80:17','B9:B10'),('385','80:18','B9:J10'),('386','80:19',None)]:add('linking-debt-checks-'+id,t,r)
early_file=BASE/'early-history-context-visual-steps.json'
if early_file.exists():
 early=[event for event in json.loads(early_file.read_text())['events'] if event['kind'] in ['worksheet_annotation','worksheet_annotation_clear']]
 groups={}
 for event in early:
  if event['kind']=='worksheet_annotation':groups.setdefault(event['payload']['gesture_group'],[]).append(event)
 for event in early:
  payload=event['payload'];identifier=event['id'];covered.add(identifier);common={'type':'annotation','target':'@active_window'}
  def emit_early(slug,action):steps.append({'id':identifier+'-source-window-'+slug,'videoTime':event['time'],'evidence':{'kind':'visual','ledger':early_file.name,'eventId':identifier,'frames':event['evidence_frames'],'observed':event['observed'],'executionMethod':'Ephemeral blue source-coordinate presenter overlay on the actual1280x720 native window, including source chrome/header rectangles. Genuine held pointer counterparts preserve observed expansions; source rectangle measurement is qualified within2pixels. No cell content, style or view metadata is assigned.','semanticCounterpart':payload.get('cell_range')},'action':dict(common,**action)})
  if payload['operation']=='clear':emit_early('clear',{'operation':'clear'});continue
  x,y,w,h=payload['source_rect'];group=groups[payload['gesture_group']];has_resize=any(item['payload']['operation']=='resize' for item in group)
  if not has_resize:emit_early('rectangle',{'operation':'rectangle','rect':[x,y,w,h],'color':payload['color'],'lineWidth':payload['line_width']});continue
  if event is group[0]:emit_early('press',{'operation':'press','point':[x,y],'color':payload['color'],'lineWidth':payload['line_width']})
  emit_early('move',{'operation':'move','point':[x+w,y+h]})
  if event is group[-1]:emit_early('release',{'operation':'release','point':[x+w,y+h]})
intro_file=BASE/'early-intro-context-visual-steps.json'
if intro_file.exists():
 intro={event['id']:event for event in json.loads(intro_file.read_text())['events']}
 trajectories=json.loads((BASE/'early-intro-annotation-trajectories.json').read_text())
 tab_geometry=json.loads((BASE/'workbook-tab-annotation-geometry.json').read_text());x,y,w,h=tab_geometry['sourceTabRect']
 def tab_point(point):return [round((point[0]-x)/(w-1),10),round((point[1]-y)/(h-1),10)]
 def tab_overlay(state,slug,operation,point=None):
  identifier=state['eventId'];event=intro[identifier];covered.add(identifier)
  action={'type':'annotation','target':'workspace.sheets','tabName':tab_geometry['tabName'],'operation':operation}
  if point is not None:action.update(tabPoint=tab_point(point),color='#0000FF',lineWidth=3)
  steps.append({'id':identifier+'-native-tab-'+slug,'videoTime':state['videoTime'],'evidence':{'kind':'visual','ledger':intro_file.name,'eventId':identifier,'frames':[state['frame']],'observed':event['observed'],'executionMethod':'Source measured held rectangle mapped around the actual native tab geometry by normalized tab-relative pointer positions. Original source tab bounds are measured within2pixels; platform geometry and physical release timing are explicit adaptations. Temporary overlays never change the sheet selection, data, style or history.','sourceTabGeometry':'workbook-tab-annotation-geometry.json','legacyObservationId':event.get('sourceObservationId')},'action':action,'expect':{'documentName':'Otis-10-K-Raw.xls','sheetName':'operations','row':1,'column':1,'revision':0}})
 for trajectory in trajectories['trajectories']:
  if trajectory['document']!='workbook':continue
  for index,state in enumerate(trajectory['states']):
   if index==0:tab_overlay(state,'press','press',trajectory['sourceAnchor'])
   tab_overlay(state,'move','move',state['sourceEnd'])
   if index==len(trajectory['states'])-1:tab_overlay(state,'release','release',state['sourceEnd'])
 for state in trajectories['clearStates']:
  if state['document']=='workbook':tab_overlay(state,'clear','clear')
steps.sort(key=lambda s:sum(float(v)*60**n for n,v in enumerate(reversed(s['videoTime'].split(':')))))
manifest={'version':1,'scope':'Observed ephemeral worksheet annotations07:00–84:26, including the initial operations-tab held rectangle and71 early source/history annotation states. Requires the same-process native worksheet actions; contains no fixture or worksheet assignments.','sourceReviewComplete':False,'steps':steps,'coveredSourceEvents':sorted(covered),'methodCaveats':['Later native cell-range rectangles adapt source screen regions; early history uses independently measured original window-coordinate rectangles within2pixels; the initial operations-tab outline resolves actual native tab geometry. Physical drag samples remain unobservable.','Clear times inside a multi-second ledger event denote visibly cleared source states; gestures between samples are unobservable.']}
p=BASE/'model-annotation-native-action-manifest.json';p.write_text(json.dumps(manifest,indent=2)+'\n');print(len(steps),'native model overlay states;',len(covered),'source events')
