#!/usr/bin/env python3
"""Build explicit read-only PDF/slide context adapters from visually checked ledgers.
These actions never construct or modify worksheet cells. The actual reference
PDFs are displayed; application chrome and PowerPoint object animations are
explicitly adapted, not claimed to be the original external applications.
"""
import json,re,math,subprocess,hashlib,xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'docs/validation/otis-90-minute'
FILES=[name for name in ['early-intro-context-visual-steps.json','early-history-context-visual-steps.json'] if (BASE/name).exists()]+['income-projections-visual-steps.json','balance-cashflow-visual-steps.json','linking-debt-checks-visual-steps.json']
DOCS={'slides':'101-01-90-Minute-Blank-Sheet-Case-Study-Slides.pdf','case':'101-01-Otis-90-Minute-Case-Study.pdf','investor':'101-01-Otis-Investor-Presentation.pdf','10k':'101-01-Otis-10-K.pdf'}
# Stage counts are source-visible states. Where background advance gestures are
# unobservable (e.g. return to the completed slide16), they remain adaptations.
STAGES={'income-002':(10,2),'income-002a':(11,0),'income-040':(11,1),'income-041':(11,2),'income-042':(11,3),'income-062':(11,3),'balance-cashflow-002':(11,3),'balance-cashflow-003':(12,1),'balance-cashflow-004':(12,2),'balance-cashflow-246':(13,0),'balance-cashflow-247':(13,1),'balance-cashflow-248':(13,2),'balance-cashflow-249':(13,3),'linking-debt-checks-002':(14,0),'linking-debt-checks-003':(14,1),'linking-debt-checks-004':(14,2),'linking-debt-checks-013':(14,3),'linking-debt-checks-025':(14,4),'linking-debt-checks-044':(14,5),'linking-debt-checks-057':(15,1),'linking-debt-checks-058':(15,2),'linking-debt-checks-068':(15,3),'linking-debt-checks-069':(15,4),'linking-debt-checks-112':(16,0),'linking-debt-checks-113':(16,1),'linking-debt-checks-122':(16,2),'linking-debt-checks-360':(16,4),'linking-debt-checks-361':(17,0),'linking-debt-checks-362':(17,1),'linking-debt-checks-363':(17,2),'linking-debt-checks-377':(17,3),'linking-debt-checks-388':(18,0),'linking-debt-checks-389':(18,1),'linking-debt-checks-417':(18,2),'linking-debt-checks-443':(18,3),'linking-debt-checks-449':(18,4),'linking-debt-checks-462':(19,0),'linking-debt-checks-467':(19,1),'linking-debt-checks-468':(19,2),'linking-debt-checks-469':(19,3),'linking-debt-checks-470':(19,4),'linking-debt-checks-471':(19,5),'linking-debt-checks-472':(19,6),'linking-debt-checks-473':(19,7)}
QUERIES={'income-008':'units sold','income-084':'growth','linking-debt-checks-323':'long-','linking-debt-checks-324':'long-term debt'}
for filename in ['early-intro-context-stages.json','early-history-context-stages.json']:
 if (BASE/filename).exists():STAGES.update(json.loads((BASE/filename).read_text()))
steps=[];adaptations=[];unresolved=[];doc=None;active=False;pages={};zooms={};slide=10;slideStage={};configuredSlide=None;emission_counts={};presentation=False
geometry=json.loads((BASE/'verified-context-annotations.json').read_text())
regions={}
for region in geometry['regions']:regions.setdefault(region['eventId'],[]).append(region)
region_continuations={r['continuationEventId'] for r in geometry['regions'] if 'continuationEventId' in r}
clear_states={}
for state in geometry['clearStates']:clear_states.setdefault(state['eventId'],[]).append(state)
annotated=False;image_selected=False
image_geometry=json.loads((BASE/'native-history/native-document-selection-verification.json').read_text())['sourceGeometries']
early_geometry=json.loads((BASE/'early-intro-context-geometry.json').read_text()) if (BASE/'early-intro-context-geometry.json').exists() else {'trajectories':[],'clearStates':[]}
early_states={state['eventId']:(trajectory,index,state) for trajectory in early_geometry['trajectories'] for index,state in enumerate(trajectory['states'])}
early_clears={state['eventId']:state for state in early_geometry['clearStates']}
early_text=json.loads((BASE/'early-intro-text-selections.json').read_text()) if (BASE/'early-intro-text-selections.json').exists() else {'selections':[],'clearStates':[],'floatingToolbarStates':[]}
early_selections={state['eventId']:state for state in early_text['selections']}
early_text_clears={state['eventId']:state for state in early_text['clearStates']}
case_artwork=json.loads((BASE/'early-intro-case-image-geometry.json').read_text()) if (BASE/'early-intro-case-image-geometry.json').exists() else None
toolbar_geometry=json.loads((BASE/'early-intro-text-toolbar-geometry.json').read_text()) if (BASE/'early-intro-text-toolbar-geometry.json').exists() else {'states':[]}
toolbar_states={state['eventId']:state for state in toolbar_geometry['states']}
toolbar_visible=False
extracted_documents={}

def emit(event,slug,action,expect=None,method=None):
 base=event['id']+'-context-'+slug;emission_counts[base]=emission_counts.get(base,0)+1
 identifier=base if emission_counts[base]==1 else base+'-'+str(emission_counts[base])
 item={'id':identifier,'videoTime':event['time'],'evidence':{'kind':'visual','ledger':event['_ledger'],'eventId':event['id'],'frames':event['evidence_frames'],'observed':event['observed'],'executionMethod':method or 'Read-only native PDF adapter operating the actual official reference; external-application chrome is adapted.'},'action':action}
 if expect:item['expect']=expect
 steps.append(item)

def keys(e,slug,target,values,expect=None):emit(e,slug,{'type':'key_sequence','target':target,'keys':values},expect)
def value(e,target,value,field):
 keys(e,field+'-select',target,['Ctrl+A']);emit(e,field+'-type',{'type':'text','target':target,'text':str(value)});keys(e,field+'-commit',target,['Return'],{'externalDocument':{field:value}})
def ensure(e,wanted):
 global doc,active,configuredSlide,annotated,toolbar_visible,presentation
 if wanted!=doc:
  if toolbar_visible:emit(e,'clear-departed-text-toolbar',{'type':'document_text_toolbar','operation':'clear','target':'@context_window'});toolbar_visible=False
  if annotated:emit(e,'leave-annotated-page',{'type':'annotation','operation':'clear','target':'context.surface'});annotated=False
  emit(e,'open-'+wanted,{'type':'document_context','operation':'open','path':str(BASE/'reference'/DOCS[wanted])});doc=wanted;active=True;configuredSlide=None;presentation=False
  if wanted in pages and pages[wanted]!=1:value(e,'context.page',pages[wanted],'page')
  if wanted in zooms:value(e,'context.zoom',zooms[wanted],'zoomPercent')
 elif not active:emit(e,'activate-'+wanted,{'type':'document_context','operation':'activate'});active=True

def mapped(point,matrix,zoom):
 # Invert the independently measured PDF96dpi→source affine. This preserves
 # the source target region when the native reader uses a different viewport.
 a,b,tx=matrix[0];c,d,ty=matrix[1];det=a*d-b*c
 x,y=point[0]-tx,point[1]-ty
 return [round((d*x-b*y)/det*zoom/100),round((-c*x+a*y)/det*zoom/100)]

def annotations(e):
 global annotated,configuredSlide
 for region in regions.get(e['id'],[]):
  entry=dict(e,time=region['videoTime'],evidence_frames=[region['frame']]);wanted=region['document'];ensure(entry,wanted)
  page,zoom=region['page'],100 if wanted=='slides' else region['zoomPercent']
  if wanted=='slides':present(entry,page,slideStage.get(page,0))
  if pages.get(wanted,1)!=page:
   if annotated:emit(entry,'clear-before-page',{'type':'annotation','operation':'clear','target':'context.surface'});annotated=False
   value(entry,'context.page',page,'page');pages[wanted]=page;configuredSlide=None
  if zooms.get(wanted)!=zoom:value(entry,'context.zoom',zoom,'zoomPercent');zooms[wanted]=zoom
  if region.get('clearFirst'):emit(entry,'clear-prior-region',{'type':'annotation','operation':'clear','target':'context.surface'})
  matrix=region['pdf96ToVideo'];x,y,w,h=region['sourceRect'];prefix='region-'+str(region['part'])
  method='Visually verified source rectangle mapped from the actual official PDF into the native read-only surface; genuine native pointer counterpart. Original application chrome and exact physical drag samples are adapted.'
  if region['mode']=='held':
   anchor=mapped(region.get('sourceAnchor',[x,y]),matrix,zoom);end=mapped(region.get('sourceEnd',[x+w,y+h]),matrix,zoom)
   emit(entry,prefix+'-press',{'type':'annotation','operation':'press','target':'context.surface','point':anchor},method=method)
   emit(entry,prefix+'-partial',{'type':'annotation','operation':'move','target':'context.surface','point':end},method=method)
   expanded=dict(entry,time=region['expandedVideoTime'],evidence_frames=[region['expandedFrame']])
   if region.get('continuationEventId'):
    continuation=events_by_id[region['continuationEventId']]
    expanded.update(id=continuation['id'],observed=continuation['observed'])
   ex,ey,ew,eh=region['expandedSourceRect'];end=mapped(region.get('expandedSourceEnd',[ex+ew,ey+eh]),matrix,zoom)
   emit(expanded,prefix+'-expand',{'type':'annotation','operation':'move','target':'context.surface','point':end},method=method)
   emit(expanded,prefix+'-release',{'type':'annotation','operation':'release','target':'context.surface','point':end},method=method)
  else:
   start,end=mapped([x,y],matrix,zoom),mapped([x+w,y+h],matrix,zoom)
   emit(entry,prefix,{'type':'annotation','operation':'rectangle','target':'context.surface','rect':[start[0],start[1],end[0]-start[0],end[1]-start[1]],'color':'#0000FF','lineWidth':3},method=method)
  annotated=True
 for state in clear_states.get(e['id'],[]):
  entry=dict(e,time=state['videoTime']);emit(entry,'clear-source-regions',{'type':'annotation','operation':'clear','target':'context.surface'});annotated=False

def select_claims(e,fragment=False):
 keys(e,'company-claims-query-select','context.find',['Ctrl+A']);emit(e,'company-claims-query',{'type':'text','target':'context.find','text':'Company Claims'});keys(e,'company-claims-find','context.find',['Return'])
 actual=subprocess.check_output(['/usr/bin/pdftotext','-layout',str(BASE/'reference'/DOCS['case']),'-']).decode();start=actual.index('Claims –' if fragment else 'Company Claims –');tail='growth rates,' if fragment else 'plausible.';end=actual.index(tail,start)+len(tail);selected=actual[start:end]
 keys(e,'company-claims-select-start','context.text',['Left']+(['Right']*len('Company ') if fragment else []));keys(e,'company-claims-selected-words','context.text',['Shift+Right']*len(selected),{'externalDocument':{'selectedText':selected.replace('\n','\u2029')}})
 adaptations.append({'eventId':e['id'],'method':'Actual native keyboard selects the observed'+('21-word fragment' if fragment else '32-word paragraph')+' in the read-only official PDF text pane. Original Word floating formatting toolbar/chrome is adapted; the PDF source and workbook stay unchanged.'})

def select_original_span(e,selection):
 wanted=selection['document'];ensure(e,wanted);page=selection['page'];zoom=selection['zoomPercent']
 if pages.get(wanted,1)!=page:value(e,'context.page',page,'page');pages[wanted]=page
 if zooms.get(wanted)!=zoom:value(e,'context.zoom',zoom,'zoomPercent');zooms[wanted]=zoom
 if wanted not in extracted_documents:extracted_documents[wanted]=subprocess.check_output(['/usr/bin/pdftotext','-layout',str(BASE/'reference'/DOCS[wanted]),'-']).decode().split('\f')
 raw=extracted_documents[wanted][page-1];characters=[];offsets=[]
 for at,char in enumerate(raw):
  if char.isspace():
   if characters and characters[-1]!=' ':characters.append(' ');offsets.append(at)
  else:characters.append(char);offsets.append(at)
 normalized=''.join(characters);source_text=re.sub(r'\s+',' ',selection['selectedTextNormalized']).strip()
 start=normalized.find(source_text[:45]);assert start>=0,('Missing independently observed selection prefix',e['id'],source_text[:45])
 end=normalized.find(source_text[-60:],start);assert end>=0,('Missing independently observed selection suffix',e['id'],source_text[-60:]);end+=len(source_text[-60:])
 first,last=offsets[start],offsets[end-1]+1;selected=raw[first:last]
 query=re.match(r'[^\n\u0080-\uffff]{1,60}',selected).group().rstrip();assert query
 matches=[]
 for number,content in enumerate(extracted_documents[wanted]):
  at=0
  while True:
   found=content.lower().find(query.lower(),at)
   if found<0:break
   matches.append((number,found));at=found+len(query)
 ordinal=matches.index((page-1,first))+1
 keys(e,'original-span-query-select','context.find',['Ctrl+A']);emit(e,'original-span-query',{'type':'text','target':'context.find','text':query});keys(e,'original-span-find','context.find',['Return']*ordinal)
 keys(e,'original-span-start','context.text',['Left']);keys(e,'original-span-select','context.text',['Shift+Right']*len(selected),{'externalDocument':{'page':page,'selectedText':selected.replace('\n','\u2029')}})
 adaptations.append({'eventId':e['id'],'method':'Native keyboard selects the actual original PDF span between independently source-reviewed prefix/suffix. PDF wrapping, automatic list numbering, and qualified partial-word caret boundaries differ from Word; original PDF text is preserved. Floating-toolbar chrome is separately disclosed.','sourceSelectedTextNormalized':source_text,'actualSelectedPdfText':selected,'sourceIncludesImage':selection.get('includesImage',False)})

def source_toolbar(e):
 global toolbar_visible
 state=toolbar_states.get(e['id'])
 if not state:return
 ensure(e,'case' if state['kind']=='word' else '10k')
 action={'type':'document_text_toolbar','target':'@context_window','operation':'clear' if state['state']=='hidden' else 'show'}
 if action['operation']=='show':action.update(rect=state['sourceRect'],kind=state['kind'],opacity=state['opacity'])
 emit(e,'original-floating-text-toolbar',action,method='Explicit external-application chrome adaptation: native no-focus graphical controls at the measured source-window bounds with the observed show/fade/hide lifecycle. Original selected PDF text, raster and source bytes remain read-only; icon artwork and fade alpha are approximations, and unused Word/Acrobat commands are not claimed.')
 toolbar_visible=action['operation']=='show'

def mask_stages(page):
 xml=subprocess.check_output(['/usr/bin/pdftotext','-f',str(page),'-l',str(page),'-bbox-layout',str(BASE/'reference'/DOCS['slides']),'-']);tree=ET.fromstring(xml);ns={'x':'http://www.w3.org/1999/xhtml'};starts=[]
 for line in tree.findall('.//x:line',ns):
  words=' '.join(w.text or '' for w in line.findall('x:word',ns))
  if words.startswith('•'):starts.append(float(line.attrib['yMin']))
 # The source presentation fills1280x720. PDF960x540-point pages render at
 #96dpi/100%; masks cover original paragraph/bullet/icon pixels at that scale.
 width=1280;bottom=674;rects=[]
 if page in [2,3,4,5]:
  blocks=[]
  for block in tree.findall('.//x:block',ns):
   y=float(block.attrib['yMin'])
   if 70<y<515:blocks.append((y,float(block.attrib['yMax'])))
  assert len(blocks)==2,('Expected the two independently source-reviewed original paragraph blocks',page,blocks)
  bottom=math.ceil(max(end for _,end in blocks)*4/3)+4
  return [[[0,math.floor(start*4/3)-4,width,bottom-(math.floor(start*4/3)-4)]] for start,_ in blocks]+[[]]
 for start in starts:
  top=max(100,math.floor(start*4/3)-34);rects.append([[0,top,width,bottom-top]])
 return rects+[[]]

def present(e,page,stage):
 global configuredSlide,presentation
 ensure(e,'slides');
 if pages.get('slides',1)!=page:
  if presentation:keys(e,'presentation-page','@context_window',['PgDown' if page>pages.get('slides',1) else 'PgUp']*abs(page-pages.get('slides',1)),{'externalDocument':{'page':page,'presentationView':True}})
  else:value(e,'context.page',page,'page')
  pages['slides']=page;configuredSlide=None
 if not presentation:
  keys(e,'full-size-presentation','@context_window',['F5'],{'externalDocument':{'page':page,'zoomPercent':100,'presentationView':True}});presentation=True;zooms['slides']=100;configuredSlide=None
  steps[-1]['evidence']['executionMethod']='Native F5 presentation-view counterpart displays the actual official960x540-point PDF at96dpi/100% over the full1280x720 canvas. Reader chrome is hidden; the original PowerPoint switch gesture remains adapted, and no slide pixels are synthesized.'
 if configuredSlide!=page:
  emit(e,'configure-slide-'+str(page),{'type':'presentation_builds','operation':'set','target':'context.surface','masks':mask_stages(page)},method='Disclosed PowerPoint-animation adaptation: temporary white masks over the actual official PDF page; native Space/Right reveals source-verified bullet/icon regions. Source physical background advances are unobservable.');configuredSlide=page;slideStage[page]=0
 while slideStage.get(page,0)<stage:
  n=slideStage.get(page,0)+1;emit(e,'reveal-'+str(n),{'type':'presentation_builds','operation':'advance','target':'context.surface'});slideStage[page]=n
 while slideStage.get(page,0)>stage:
  n=slideStage[page]-1;emit(e,'back-'+str(n),{'type':'presentation_builds','operation':'advance','target':'context.surface','reverse':True});slideStage[page]=n

all_events=[]
for filename in FILES:
 for e in json.loads((BASE/filename).read_text())['events']:e['_ledger']=filename;all_events.append(e)
events_by_id={e['id']:e for e in all_events}
for e in sorted(all_events,key=lambda e:e['seconds']):
 observed=e['observed'];lower=observed.lower();kind=e['kind'];app=e.get('application','');payload=e.get('payload',{});wanted=None
 if e['id'] in early_selections:
  select_original_span(e,early_selections[e['id']]);source_toolbar(e)
  if case_artwork and e['id']==case_artwork['eventId']:
   emit(e,'select-original-case-artwork',{'type':'document_image_selection','operation':'artwork','target':'context.surface','rect':[round(value*zooms['case']/100) for value in case_artwork['pdf96ImageRect']]},method='Native pointer counterpart selects the actual original heading and both pie-chart images under the observed gray Word artwork region.354 original PDF/video feature correspondences map the independently reviewed source bounds; Word chrome, viewport and physical click gesture remain explicit adaptations. Original PDF pixels/text are preserved.');image_selected=True
  continue
 if e['id'] in toolbar_states:source_toolbar(e);continue
 if e['id'] in early_text_clears:
  ensure(e,early_text_clears[e['id']].get('document','case'))
  if image_selected:emit(e,'clear-original-case-artwork',{'type':'document_image_selection','operation':'clear','target':'context.surface'});image_selected=False
  if toolbar_visible:emit(e,'clear-selected-text-toolbar',{'type':'document_text_toolbar','operation':'clear','target':'@context_window'});toolbar_visible=False
  keys(e,'clear-original-text-selection','context.text',['Left'],{'externalDocument':{'selectedText':''}});continue
 if e['id'].startswith('early-history-context-') and kind=='annotation_clear' and 'acrobat' in app.lower():
  ensure(e,'10k');emit(e,'clear-carried-source-region',{'type':'annotation','operation':'clear','target':'context.surface'});annotated=False;continue
 if e['id'] in early_states:
  trajectory,index,state=early_states[e['id']];wanted=trajectory['document'];ensure(e,wanted);page=trajectory['page'];zoom=100 if wanted=='slides' else trajectory['zoomPercent']
  if wanted=='slides':present(e,page,slideStage.get(page,0))
  if pages.get(wanted,1)!=page:value(e,'context.page',page,'page');pages[wanted]=page;configuredSlide=None
  if zooms.get(wanted)!=zoom:value(e,'context.zoom',zoom,'zoomPercent');zooms[wanted]=zoom
  method='Original source blue trajectory mapped through the measured official-PDF affine; actual native held pointer events preserve every sampled partial/expanded state. Physical sample timing and external-application chrome remain adapted.'
  if index==0:emit(e,'early-region-press',{'type':'annotation','operation':'press','target':'context.surface','point':mapped(trajectory['sourceAnchor'],trajectory['pdf96ToVideo'],zoom)},method=method)
  end=mapped(state['sourceEnd'],trajectory['pdf96ToVideo'],zoom);emit(e,'early-region-move',{'type':'annotation','operation':'move','target':'context.surface','point':end},method=method)
  if index==len(trajectory['states'])-1:emit(e,'early-region-release',{'type':'annotation','operation':'release','target':'context.surface','point':end},method=method)
  annotated=True;continue
 if e['id'] in early_clears:
  ensure(e,early_clears[e['id']]['document']);emit(e,'clear-early-regions',{'type':'annotation','operation':'clear','target':'context.surface'});annotated=False;continue
 if e['id'] in region_continuations:continue
 if e['id'] in regions or e['id'] in clear_states:
  annotations(e);continue
 if e['id'] in ['linking-debt-checks-392','linking-debt-checks-421','linking-debt-checks-422']:
  ensure(e,'investor')
  if annotated:emit(e,'clear-source-annotation-for-image',{'type':'annotation','operation':'clear','target':'context.surface'});annotated=False
  if image_selected:emit(e,'clear-prior-image-selection',{'type':'document_image_selection','operation':'clear','target':'context.surface'});image_selected=False
  if e['id']=='linking-debt-checks-422':continue
  geometry=image_geometry[0 if e['id'].endswith('392') else 1];page=geometry['page'];zoom=80.6
  if pages.get('investor',1)!=page:value(e,'context.page',page,'page');pages['investor']=page
  if zooms.get('investor')!=zoom:value(e,'context.zoom',zoom,'zoomPercent');zooms['investor']=zoom
  def scaled(rect):return [round(v*zoom/100) for v in rect]
  emit(e,'select-actual-pdf-image',{'type':'document_image_selection','operation':'select','target':'context.surface','rect':scaled(geometry['pdf96Image']),'toolbarRect':scaled(geometry['pdf96Toolbar'])},method='Native pointer selects the actual official PDF image; verified inverse source geometry maps the gray region and five-icon floating toolbar into the reader. Adobe artwork/chrome and physical deselection gesture are adapted.');image_selected=True;continue
 if e['id']=='linking-debt-checks-466':
  ensure(e,'case')
  # The source selects21words beginning at Claims and ending after rates,
  # across a wrapped paragraph. Select that exact content through native keys
  # in the read-only extraction pane, keeping the official PDF untouched.
  select_claims(e,True);continue
 if 'notepad' in (app+' '+observed).lower():continue # executed by native text adapter in model segments
 if e['id'] in STAGES or 'powerpoint' in app.lower() or 'power point' in lower or 'powerpoint' in lower or kind=='slide_build':
  if e['id'] in STAGES:slide,stage=STAGES[e['id']]
  else:
   m=re.search(r'slide\s*(\d+)',observed,re.I)
   if m:slide=int(m.group(1))
   stage=slideStage.get(slide,0)
  present(e,slide,stage)
  if e['id']=='early-intro-context-079':
   keys(e,'show-carried-tooltip-tool','@context_window',['F8'])
   steps[-1]['evidence']['executionMethod']='Native F8 briefly exposes the real PDF tool for its observed carried hover label. This external-reader configuration is an explicit input-method adaptation; the following actual native QToolTip covers the tool at the measured source bounds.'
   emit(e,'restore-carried-tooltip-pointer',{'type':'mouse','target':'context.surface','gesture':'hover'},method='Native pointer reset after page/zoom keys, solely to restore the source-observed carried tooltip in the adapted reader. This is not claimed as an observed original pointer gesture.')
   emit(e,'carry-edit-pdf-tooltip',{'type':'mouse','target':'context.editPdf','gesture':'hover','dwellMs':1000},{'externalDocument':{'page':6,'editPdfToolTipVisible':True,'toolTipText':'Edit PDF'}},method='Actual native graphical Edit PDF tool displays its real QToolTip over the source-observed slide6 state. Re-hovering restores a tooltip that native page/zoom keys dismiss; original Acrobat-to-PowerPoint window chrome and physical continuation are adapted.')
  if e['id']=='early-intro-context-080':
   emit(e,'clear-carried-edit-pdf-tooltip',{'type':'key_sequence','target':'context.surface','keys':['F8']},{'externalDocument':{'page':6,'editPdfToolTipVisible':False,'toolTipText':''}},method='Native presentation tool-toggle counterpart clears the genuine tooltip and hides its tool rail, restoring the full original PDF slide. Source tooltip disappearance is observed; original physical clearing gesture is unobservable.')
  continue
 document_name=payload.get('document','').lower()
 if ('word' in app.lower() and 'acrobat' not in app.lower()) or ('word' in lower and kind=='application_switch') or 'casestudy' in document_name.replace('-','').replace(' ',''):wanted='case'
 elif '10-k' in document_name and ('pdf' in document_name or 'acrobat' in app.lower()):wanted='10k'
 elif 'investor' in lower or payload.get('document')=='Otis Investor Presentation':wanted='investor'
 elif (('10-k' in lower or '10-k' in payload.get('document','').lower() or '10-k' in observed.lower()) and ('.xls' not in lower) and ('pdf' in lower or 'cover' in lower or 'acrobat' in lower)):wanted='10k'
 elif kind.startswith('source_') and doc is not None:wanted=doc
 elif kind in ['annotation','annotation_clear'] and active and not e.get('selection'):wanted=doc
 if wanted:
  ensure(e,wanted)
  if e['id'] in ['linking-debt-checks-325','linking-debt-checks-326']:
   assert wanted=='10k',('Debt Find must remain in the actual10-K PDF',e['id'],wanted)
   page,index=(16,1) if e['id'].endswith('325') else (38,2)
   emit(e,'debt-first-result' if index==1 else 'debt-next-result',{'type':'key_sequence','target':'context.find','keys':[] if index==1 else ['Return']},{'externalDocument':{'page':page,'query':'long-term debt','matchIndex':index,'matchCount':40}},method='Actual native literal case-insensitive Find in the unchanged official10-K PDF. Extracted PDF text has40 hits versus source Acrobat39; this documented text-extraction/chrome adaptation does not fabricate the source counter. First/second real matches are on pages16/38.')
   pages[wanted]=page;continue
  if e['id']=='linking-debt-checks-331':
   emit(e,'focus-source-page-field',{'type':'mouse','target':'context.page','gesture':'click'},{'externalDocument':{'page':38}},method='Actual native pointer focuses the unchanged page38 field. Source focus is visible, while its exact physical click/key gesture is unobservable; original Acrobat chrome is adapted.');continue
  page=payload.get('page');zoom=payload.get('zoom_percent')
  if page is None:
   m=re.search(r'(?:page\s*|PDF\s*)(\d+)',observed,re.I)
   if m:page=int(m.group(1))
  if wanted=='case':page=1;zoom=120 if zoom is None else zoom
  if zoom is None:
   m=re.search(r'(\d+(?:\.\d+)?)\s*%',observed)
   if m and 'zoom' in lower:zoom=float(m.group(1))
   elif '80.6%' in lower:zoom=80.6
   elif '71.4%' in lower:zoom=71.4
  if page is not None and pages.get(wanted,1)!=page:
   if annotated:emit(e,'clear-before-page',{'type':'annotation','operation':'clear','target':'context.surface'});annotated=False
   value(e,'context.page',page,'page');pages[wanted]=page;configuredSlide=None
  if zoom is not None and zooms.get(wanted)!=zoom:value(e,'context.zoom',zoom,'zoomPercent');zooms[wanted]=zoom
  if e['id']=='early-intro-context-078':
   emit(e,'hover-edit-pdf-tooltip',{'type':'mouse','target':'context.editPdf','gesture':'hover','dwellMs':1000},{'externalDocument':{'page':60,'editPdfToolTipVisible':True,'toolTipText':'Edit PDF'}},method='Actual native pointer hover opens the graphical Edit PDF tool\'s real QToolTip without editing the official PDF. Original Acrobat sidebar chrome is adapted.');continue
  if e['id']=='income-091':
   method='Actual native query-field pointer reopens a real document-vocabulary suggestion panel while preserving investor page11. The original proprietary cloud suggestions and56/4/18/4/22 frequencies differ from this truthful literal PDF index; no source count or synonym is fabricated.'
   emit(e,'show-actual-find-suggestions',{'type':'mouse','target':'context.find','gesture':'click'},{'externalDocument':{'page':11,'query':'growth','findSuggestionsVisible':True}},method=method)
   end=dict(e,time=e['end_time']);keys(end,'dismiss-actual-find-suggestions','context.find',['Escape'],{'externalDocument':{'page':11,'query':'growth','findSuggestionsVisible':False}})
   steps[-1]['evidence']['executionMethod']='Actual native Escape counterpart dismisses the real suggestion panel at the source-observed closed state, preserving page11/querygrowth. The source52-hit semantic result and Accelerate highlight remain an explicit cloud-index/search-highlight difference, not a claimed native match.'
   continue
  if e['id']=='income-003':select_claims(e)
  if e['id'] in QUERIES:
   keys(e,'find-select','context.find',['Ctrl+A']);emit(e,'find-type',{'type':'text','target':'context.find','text':QUERIES[e['id']]});keys(e,'find-submit','context.find',['Return']);continue
  if kind=='source_scroll':
   upward='up' in lower and 'down' not in lower;emit(e,'native-scroll',{'type':'wheel','target':'context.scroll','angleDelta':[0,120 if upward else -120]},method='Actual native wheel counterpart of the observed scroll; the source physical delta is not measurable. Exact image-offset alignment is tracked separately.');continue
  if kind in ['source_find_close','source_find_dismiss']:
   keys(e,'leave-find','context.find',['Escape']);continue
  if 'annotation' in kind or 'selection' in kind:
   unresolved.append({'eventId':e['id'],'reason':'Source region/selection geometry integration follows the verified PDF-to-video alignment; no fabricated worksheet mutation.'});continue
  if page is None and kind.startswith('source_'):emit(e,'observed-state',{'type':'document_context','operation':'activate'})
  continue
 # A source-visible Excel event follows an external context. This boundary uses
 # the actual original event's evidence, not a guessed AltTab keystroke.
 if active and app in ['Excel model','Excel source']:
  if toolbar_visible:emit(e,'clear-departed-text-toolbar',{'type':'document_text_toolbar','operation':'clear','target':'@context_window'});toolbar_visible=False
  if annotated:emit(e,'clear-departed-page',{'type':'annotation','operation':'clear','target':'context.surface'},method='Read-only replay overlay cleanup when leaving the source document; original PDF pixels are untouched.');annotated=False
  if image_selected:emit(e,'clear-departed-image',{'type':'document_image_selection','operation':'clear','target':'context.surface'},method='Native Escape counterpart clears the read-only image overlay before returning to the model. Original physical deselection gesture is unobservable.');image_selected=False
  emit(e,'return-model',{'type':'document_context','operation':'hide'},method='Native window activation counterpart of the observed return to Excel; exact physical AltTab gesture is not visible.');active=False
if active:emit(all_events[-1],'close-delivery-context',{'type':'document_context','operation':'close'},method='Acceptance workspace cleanup after the last observed context; PDF source remains unchanged.')
steps.sort(key=lambda item:sum(float(value)*60**n for n,value in enumerate(reversed(item['videoTime'].split(':')))))
manifest={'version':1,'sourceVideo':'https://www.youtube.com/watch?v=O2uPNfErmJo','scope':'Read-only observed external-context adapters00:00–84:26, including the separately reviewed opening/history ledgers. Actual official PDF pages/text/native scrolling and progressively revealed PDF paragraph/bullet/icon regions. No worksheet construction.','sourceLedgers':FILES,'windowSize':[1280,720],'sourceReviewComplete':False,'steps':steps,'unresolvedSourceRegions':unresolved,'adaptations':adaptations,'methodCaveats':['PDF adapter is not Acrobat/Word UI. Its extracted literal long-term debt hit count40 differs from source Acrobat39.','Source-shaped Find suggestions use genuine document vocabulary/counts; source cloud-generated synonyms/frequencies and52-result growth/Accelerate highlighting remain unreproduced. See pdf-search-index-investigation.json; no forced duplicate multiplier is applied.','Official source slide PDF has no animation objects. Source-observed builds use temporary masks over actual PDF pixels; final slide is original PDF pixels.','Source exact external-app scroll deltas are unobservable; actual native wheel counterparts are recorded.','Word gray text selections use the actual read-only extracted PDF text pane; original Word floating formatting toolbar/chrome remains adapted.','No claim of exact external-application pixels or every physical keystroke is made.']}
manifest['presentationGeometryCorrection']={'sourceFrame':'source-frames/intro-context/000.000.jpg','sourceFrameSha256':hashlib.sha256((BASE/'source-frames/intro-context/000.000.jpg').read_bytes()).hexdigest(),'sourceCanvas':[1280,720],'officialPdfPagePoints':[960,540],'pdfRasterDpi':96,'nativePresentationZoomPercent':100,'supersededAdapterSetting':'Earlier slide payload zoom_percent75 described the old reader configuration, not a visible PowerPoint zoom field. It reduced the original1280x720 slide raster to960x540. The native presentation view now hides reader chrome and fits the full original page.','inputMethod':'F5, PageUp/PageDown and temporary F8 PDF-tool visibility are native read-only adapter counterparts; they are not asserted as the original PowerPoint physical keys.'}
out=BASE/'external-context-native-action-manifest.json';out.write_text(json.dumps(manifest,indent=2)+'\n');print(len(steps),'read-only native context actions;',len(unresolved),'region integration entries')
