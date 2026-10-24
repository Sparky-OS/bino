#!/usr/bin/env python3
"""Check demand-driven paints, source eyes, and the real overlay lifecycle."""
import argparse,json,subprocess,collections,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--clip',type=Path,required=True);p.add_argument('--swap-expected',action='store_true');a=p.parse_args()
rows=[json.loads(line) for line in (a.directory/'repaint.jsonl').read_text().splitlines()]
phases={r['phase']:r['ms'] for r in rows if r['event']=='phase'}
paints=[r for r in rows if r['event']=='paint']
assert (a.directory/'app-exit.txt').read_text().strip()=='0'
assert 'end' in phases and len(paints)>10
raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(a.clip),'-threads','4','-f','rawvideo','-pix_fmt','rgb24','pipe:1'])
source=np.frombuffer(raw,np.uint8).reshape(72,180,640,3)
if a.swap_expected: source=np.concatenate((source[:,:,320:],source[:,:,:320]),axis=2)
def pixels(r,kind='target'):
 return np.array(Image.open(a.directory/(r['stem']+'-'+kind+'.png')).convert('RGBA'))
def last_at(phase):
 t=phases[phase];found=[r for r in paints if r['ms']<t]
 assert found,phase
 return found[-1]
def phase_paints(name): return [r for r in paints if r['phase']==name]
channels=0;worst=0
for r in paints:
 if r['native']:
  actual=pixels(r,'video')[:,:,:3];expected=source[round(r['pts']*4/1e6)]
 else:
  if r['phase'] not in ('playing-hidden','settle','idle','hidden','expired','idle-again','expose'):continue
  actual=pixels(r)[:,:,:3];eye=r['buffer'] if r['stereo'] else 0
  expected=source[round(r['pts']*4/1e6),:,eye*320:(eye+1)*320]
 error=np.abs(actual.astype(int)-expected.astype(int));assert error.max()<=1,('source pixels',r,int(error.max()))
 channels+=actual.size;worst=max(worst,int(error.max()))
assert channels>0
native=paints[-1]['native']
if native:
 for name,count in (('mono',1),('stereo',2)):
  found=phase_paints(name);assert found and all(r['views']==count for r in found),(name,found)
 for name in ('show','lock','animate','pause','restore','show-again'):
  found=phase_paints(name);assert found,name
  assert any(np.count_nonzero(pixels(r)[:,:,3])>1000 for r in found),('overlay missing',name)
 assert np.count_nonzero(pixels(last_at('locked-expired'))[:,:,3])>1000,'locked overlay disappeared'
 for name in ('hidden','expired'):
  assert np.max(pixels(last_at(name))[:,:,3])==0,('overlay did not disappear',name)
 animation=phase_paints('animate')
 assert len(animation)>=5,('overlay animation stopped',len(animation))
 hashes={hashlib.sha256(pixels(r).tobytes()).hexdigest() for r in animation}
 assert len(hashes)>=3,('overlay progress stayed stale',len(hashes))
 paused=phase_paints('pause');assert paused and not paused[-1]['playing']
 # The pause/start icon itself must change, independently of the moving seek bar.
 h,w=pixels(paused[-1]).shape[:2];cx,cy=w//2,int(h-w/9);d=int(w/36)
 icon=lambda r:pixels(r)[cy-d:cy+d,cx-d:cx+d]
 assert not np.array_equal(icon(animation[-1]),icon(paused[-1])),'pause icon did not change'
 assert phase_paints('resize') and phase_paints('restore') and phase_paints('expose')
 assert phase_paints('resize')[-1]['width']*4==phase_paints('restore')[-1]['width']*3
# A repaint must include both toolkit buffers for the same source PTS.
if not native and paints[-1]['stereo']:
 assert len(paints)%2==0, 'incomplete stereo pair'
 for left,right in zip(paints[::2],paints[1::2]):
  assert (left['buffer'],right['buffer'])==(0,1) and left['pts']==right['pts'],('unpaired targets',left,right)
idle={name:len(phase_paints(name)) for name in ('idle','idle-again')}
low=collections.Counter(r['pts'] for r in phase_paints('playing-hidden') if 500000<=r['pts']<1500000)
allowed=1 if native or not paints[-1]['stereo'] else 2
excess=sum(max(0,n-allowed) for n in low.values())
result={'source_channels':channels,'maximum_error':worst,'idle_paints':idle,'low_rate_paints_by_pts':dict(low),'excess_low_rate_paints':excess,'native':native,'paint_count':len(paints)}
print(json.dumps(result,indent=2),flush=True)
assert all(n==0 for n in idle.values()),('unrequested idle repaints',idle)
assert set(low)=={500000,750000,1000000,1250000} and excess==0,('duplicate hidden-overlay playback paints',low)
