#!/usr/bin/env python3
"""Replay source publisher shortcut artwork separately from native app banners."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/validation/otis-90-minute'
steps=[]
def emit(identifier,time,ledger,event_id,frames,observed,action):
    steps.append({'id':identifier,'videoTime':time,'evidence':{'kind':'visual','ledger':ledger,'eventId':event_id,'frames':frames,'observed':observed,'executionMethod':'Explicit publisher-artwork adapter: actual no-focus native tooltip with source-measured bounds/text. Separate native caption and modal grabs are retained and composed at real global offsets. Its editorial/macro/tool origin and physical trigger are unproven.'},'action':action})
intro=json.loads((BASE/'early-intro-shortcut-caption.json').read_text())
for operation,seconds in [('show',401.633333),('clear',419.866667)]:
    time=f'{int(seconds//60):02}:{seconds%60:09.6f}'
    action={'type':'video_caption','target':'@active_window','operation':operation}
    if operation=='show':action.update(rect=intro['sourceBounds'],lines=intro['lines'],fontPixelSize=intro['fontPixelSize'])
    emit('intro-video-caption-'+operation,time,'early-intro-context-visual-steps.json',intro['eventId'],[str(BASE/intro['sourceFrame']) if not Path(intro['sourceFrame']).is_absolute() and not intro['sourceFrame'].startswith('docs/') else str(ROOT/intro['sourceFrame']) if not Path(intro['sourceFrame']).is_absolute() else intro['sourceFrame']]+[frame['file'] for frame in intro['retainedBoundaryFrames']],f'Original source Move to Next Sheet caption {operation}; native30fps boundary ±one frame.',action)
for name in ['early-history-context-visual-steps.json','later-shortcut-caption-visual-steps.json']:
    for event in json.loads((BASE/name).read_text())['events']:
        if event['kind'] not in ['shortcut_feedback','shortcut_feedback_clear']:continue
        payload=event['payload'];operation='show' if event['kind']=='shortcut_feedback' else 'clear'
        action={'type':'video_caption','target':'@active_window','operation':operation}
        if operation=='show':action.update(rect=payload['source_rect'],lines=payload.get('lines',payload.get('text','').split('\n')),fontPixelSize=payload.get('font_pixel_size',payload.get('appearance',{}).get('font_height_px',42)))
        emit(event['id']+'-native-caption',event['time'],name,event['id'],event['evidence_frames'],event['observed'],action)
steps.sort(key=lambda step:sum(float(value)*60**n for n,value in enumerate(reversed(step['videoTime'].split(':')))))
out=BASE/'video-caption-native-action-manifest.json'
out.write_text(json.dumps({'version':1,'sourceReviewComplete':False,'scope':'Source publisher shortcut panel show/hide states; genuine native Office warnings, banners, selection affordances and keytips are separate.','steps':steps,'methodCaveats':['Source panel origin is unproven; it is replayed as explicit publisher artwork, not represented as native Excel UI.','Dense original30fps interval boundaries are retained for opening/later panels; early historical panel bounds are halfsecond observations with ±oneframe/±2px measurement limits.','Exact physical trigger and animation interpolation between sampled states remain unobservable.']},indent=2,ensure_ascii=False)+'\n')
print(len(steps),'native source-caption transitions')
