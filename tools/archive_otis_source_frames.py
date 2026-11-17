#!/usr/bin/env python3
"""Retain later-video evidence actually referenced anywhere by a replay manifest.
This copies existing local evidence bytes and rewrites paths; it never edits
images, source PDFs or workbook contents. Review crops are distinguished from
timestamped original frames. Existing unequal evidence is refused.
"""
import argparse,hashlib,json,re,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/validation/otis-90-minute'
PATTERN=re.compile(r'^/tmp/tigersheets-video-review/([A-Za-z0-9_-]+)/([A-Za-z0-9_.-]+\.(?:jpg|jpeg|png))$')

def archive_later_frames(manifest):
 out=BASE/'source-frames/later';records={};video_sha='05fffb181dcc0670de3a42cd1faa0554e1e01d3a7dd665857f71d5c808e81400'
 def walk(value,path='$'):
  if isinstance(value,dict):return {key:walk(item,path+'.'+key) for key,item in value.items()}
  if isinstance(value,list):return [walk(item,path+'['+str(n)+']') for n,item in enumerate(value)]
  if not isinstance(value,str):return value
  match=PATTERN.fullmatch(value)
  if not match:return value
  folder,name=match.groups();segment=folder.removesuffix('-frames') if folder in ['income-frames','balance-cashflow-frames','final-frames'] else folder;source=Path(value)
  if not source.is_file():raise FileNotFoundError('Referenced original source frame is unavailable: '+value)
  sha=hashlib.sha256(source.read_bytes()).hexdigest();target=out/segment/name;target.parent.mkdir(parents=True,exist_ok=True)
  if target.exists():
   if hashlib.sha256(target.read_bytes()).hexdigest()!=sha:raise ValueError('Refusing to replace unequal retained source evidence: '+str(target))
  else:shutil.copyfile(source,target)
  timed=segment in ['income','balance-cashflow','final'] and re.fullmatch(r'frame-\d+\.jpg',name);seconds={'income':2110,'balance-cashflow':3011,'final':4031}[segment]+int(name[6:-4]) if timed else None
  entry=records.setdefault(value,{'originalPath':value,'retainedPath':str(target.relative_to(ROOT)),'sha256':sha,'bytes':target.stat().st_size,'videoSeconds':seconds,'evidenceKind':'timestamped-original-frame' if timed else 'supplementary-review-evidence','timestampBasis':'Known one-second frame sequence' if timed else 'Preserve source ledger timing; this review file has no independently inferred timestamp','referencedBy':[]});entry['referencedBy'].append(path)
  return str(target)
 retained=walk(manifest);provenance={'sourceVideo':'https://www.youtube.com/watch?v=O2uPNfErmJo','sourceLocalVideoSha256':video_sha,'method':'Existing local image evidence referenced anywhere in this replay manifest, including nested segment qualifications, is byte-copied without image edits. Original paths and SHA256 remain explicit. Only independently known one-second sequences receive derived timestamps; supplementary frames/crops retain source-ledger timing rather than invented timestamps.','frames':sorted(records.values(),key=lambda r:(r['videoSeconds'] is None,r['videoSeconds'] or 0,r['originalPath']))}
 out.mkdir(parents=True,exist_ok=True);meta=out/'provenance.json';meta.write_text(json.dumps(provenance,indent=2)+'\n');retained['retainedLaterFrameProvenance']=str(meta);retained['retainedLaterFrameCount']=len(records);return retained

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('manifest',type=Path);p.add_argument('--output',type=Path);a=p.parse_args();result=archive_later_frames(json.loads(a.manifest.read_text()));target=a.output or a.manifest;target.write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n');print(json.dumps({'manifest':str(target),'retainedSourceFrames':result['retainedLaterFrameCount'],'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}))
