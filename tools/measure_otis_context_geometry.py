#!/usr/bin/env python3
"""Read-only geometric evidence: align official PDF pixels to source video frames.
Candidates are explicitly unverified until a reviewer inspects the source frame.
"""
import json,subprocess,hashlib
from pathlib import Path
import cv2,numpy as np
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/validation/otis-90-minute'
TMP=Path('/tmp/tigersheets-video-review/context-matches');TMP.mkdir(parents=True,exist_ok=True)
DOCS={'investor':'101-01-Otis-Investor-Presentation.pdf','case':'101-01-Otis-90-Minute-Case-Study.pdf','10k':'101-01-Otis-10-K.pdf'}
SPECS={**{f'income-{n:03d}':('investor',p) for n,p in [(16,6),(17,6),(24,11),(38,24),(61,24),(98,31),(117,42),(118,42)]},'income-004':('case',1),**{f'balance-cashflow-{n:03d}':('investor',41) for n in [407,408,409,410,411,412,511,512]},**{f'linking-debt-checks-{n:03d}':('investor',41) for n in [115,116,117,119,391]},**{f'linking-debt-checks-{n:03d}':('10k',38) for n in [332,333,334]},'linking-debt-checks-465':('case',1)}
def boxes(image):
 b,g,r=cv2.split(image);mask=np.uint8((b>190)&(g<100)&(r<100))*255
 _,_,stats,_=cv2.connectedComponentsWithStats(mask)
 return [list(map(int,s[:4])) for s in stats[1:] if s[1]>=130 and s[2]>40 and s[3]>15 and s[4]>50]
def main():
 records=[];sift=cv2.SIFT_create(4000)
 for filename in ['income-projections-visual-steps.json','balance-cashflow-visual-steps.json','linking-debt-checks-visual-steps.json']:
  for event in json.loads((BASE/filename).read_text())['events']:
   if event['id'] not in SPECS:continue
   doc,page=SPECS[event['id']];pdf=BASE/'reference'/DOCS[doc];prefix=TMP/f'{doc}-{page}'
   if not prefix.with_suffix('.png').exists():subprocess.run(['/usr/bin/pdftoppm','-f',str(page),'-l',str(page),'-singlefile','-r','96','-png',str(pdf),str(prefix)],check=True)
   reference=cv2.imread(str(prefix)+'.png');ka,da=sift.detectAndCompute(cv2.cvtColor(reference,cv2.COLOR_BGR2GRAY),None)
   candidates=[]
   for frame in event['evidence_frames']:
    actual=cv2.imread(frame);rects=boxes(actual)
    if not rects:continue
    kb,db=sift.detectAndCompute(cv2.cvtColor(actual,cv2.COLOR_BGR2GRAY),None)
    if db is None:continue
    good=[m for m,n in cv2.BFMatcher().knnMatch(da,db,k=2) if m.distance<.7*n.distance]
    if len(good)<10:continue
    p=np.float32([ka[m.queryIdx].pt for m in good]);q=np.float32([kb[m.trainIdx].pt for m in good]);matrix,inliers=cv2.estimateAffinePartial2D(p,q,method=cv2.RANSAC,ransacReprojThreshold=2)
    if matrix is None or inliers.sum()<20:continue
    inverse=cv2.invertAffineTransform(matrix);mapped=[]
    for x,y,w,h in rects:
     points=cv2.transform(np.array([[[x,y],[x+w,y+h]]],dtype=np.float32),inverse)[0];mapped.append({'sourceRect':[x,y,w,h],'pdfRect96':[round(float(points[0,0]),3),round(float(points[0,1]),3),round(float(points[1,0]-points[0,0]),3),round(float(points[1,1]-points[0,1]),3)]})
    candidates.append({'frame':frame,'inliers':int(inliers.sum()),'pdf96ToVideo':matrix.tolist(),'rectangles':mapped})
   records.append({'eventId':event['id'],'videoTime':event['time'],'observed':event['observed'],'pdf':str(pdf),'page':page,'pdfSha256':hashlib.sha256(pdf.read_bytes()).hexdigest(),'pdf96Size':[reference.shape[1],reference.shape[0]],'status':'geometric candidates; visual verification required','frames':candidates})
 out=BASE/'context-geometry-candidates.json';out.write_text(json.dumps({'method':'SIFT actual official PDF96dpi pixels to actual source frame; RANSAC2px. Candidate blue components can overlap or contain pre-existing boxes. Do not treat every component as a new source gesture.','records':records},indent=2)+'\n');print(out,len(records))
if __name__=='__main__':main()
