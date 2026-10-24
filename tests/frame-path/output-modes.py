#!/usr/bin/env python3
"""Check output conversions against independently decoded, constant eye colours."""
import argparse,json,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser(); p.add_argument('root',type=Path); p.add_argument('--swap-expected',action='store_true'); p.add_argument('--modes',nargs='+',type=int,default=list(range(30))); a=p.parse_args()
raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(a.root/'source.mkv'),'-frames:v','1','-f','rawvideo','-pix_fmt','rgb24','pipe:1'])
source=np.frombuffer(raw,np.uint8).reshape(180,640,3)
colours=np.array([[49,97,151],[211,173,67]])
for eye in range(2): assert np.all(source[:,eye*320:(eye+1)*320] == colours[eye])
if a.swap_expected: colours=colours[::-1]
srgb=colours/255.; linear=np.where(srgb<=.04045,srgb/12.92,((srgb+.055)/1.055)**2.4)
l,r=linear; lum=lambda c: c @ np.array([.212671,.715160,.072169])
# Row-major matrices from the original Dubois output definitions.
matrices={16: ([[.437,.449,.164],[-.062,-.062,-.024],[-.048,-.050,-.017]], [[-.011,-.032,-.007],[.377,.761,.009],[-.026,-.093,1.234]]),
20: ([[-.062,-.158,-.039],[.284,.668,.143],[-.015,-.027,.021]], [[.529,.705,.024],[-.016,-.015,-.065],[.009,.075,.937]]),
24: ([[1.062,-.205,.299],[-.026,.908,.068],[-.038,-.173,.022]], [[-.016,-.123,-.017],[.006,.062,-.017],[.094,.185,.911]])}
anaglyph={17:[l[0],r[1],r[2]],18:[lum(l),r[1],r[2]],19:[lum(l),lum(r),lum(r)],
21:[r[0],l[1],r[2]],22:[r[0],lum(l),r[2]],23:[lum(r),lum(l),lum(r)],
25:[l[0],l[1],r[2]],26:[lum(l),lum(l),r[2]],27:[lum(l),lum(l),lum(r)],28:[lum(l),lum(r),0],29:[lum(l),0,lum(r)]}
for k,(m0,m1) in matrices.items(): anaglyph[k]=np.array(m0)@l+np.array(m1)@r
for k,value in anaglyph.items():
 c=np.clip(value,0,1); anaglyph[k]=255*np.where(c<=.0031308,12.92*c,1.055*c**(1/2.4)-.055)
results=[]
for mode in a.modes:
 d=a.root/f'off-{mode}'; info=json.loads((d/'mode.json').read_text())
 assert len(info['outputActions'])==30 and (d/'app-exit.txt').read_text().strip()=='0'
 w,h=info['width'],info['height']; y,x=np.indices((h,w)); gx=x+info['x']; gy=info['screenHeight']-1-(y+info['y'])
 aspect=320/180
 if mode in (5,7): aspect*=2
 if mode in (4,9,11): aspect*=.5
 screen_aspect=w/(h-h/49) if mode==4 else w/h
 rw=min(1,aspect/screen_aspect); rh=min(1,screen_aspect/aspect)
 tx=((x+.5)/w-.5*(1-rw))/rw; ty=((h-y-.5)/h-.5*(1-rh))/rh
 mask=(tx>.03)&(tx<.97)&(ty>.03)&(ty<.97)
 eyes=np.zeros((h,w),np.int32)
 if mode==1: eyes[:]=1
 if mode in (5,6,7,8):
  eyes=(tx>=.5).astype(np.int32); mask &= abs(tx-.5)>.03
  if mode in (7,8): eyes=1-eyes
 if mode in (9,10,11,12):
  eyes=(ty<.5).astype(np.int32); mask &= abs(ty-.5)>.03
  if mode in (11,12): eyes=1-eyes
 if mode==4:
  eyes=(ty<.5).astype(np.int32); mask &= abs(ty-.5)>.05
 if mode==13: eyes=gy%2
 if mode==14: eyes=gx%2
 if mode==15: eyes=(gx%2 != gy%2).astype(np.int32)
 expected=colours[eyes].astype(float)
 if mode in anaglyph: expected[:]=anaglyph[mode]
 errors=[]; alternating=[]
 for n,name in enumerate(('first','second')):
  actual=np.array(Image.open(d/f'{name}.png').convert('RGB')); assert actual.shape==(h,w,3)
  if mode==2: expected[:]=colours[n]
  if mode==3:
   distances=[np.max(abs(actual[mask].astype(float)-c)) for c in colours]
   eye=int(np.argmin(distances)); alternating.append(eye); expected[:]=colours[eye]
  error=float(abs(actual[mask].astype(float)-expected[mask]).max())
  assert error<=1,(mode,name,error)
  errors.append(error)
 if mode==3: assert set(alternating)=={0,1},('alternating did not deliver both source eyes',alternating)
 results.append(dict(mode=mode,channels=int(mask.sum()*3*2),max_error=max(errors)))
print(json.dumps({'modes':results,'source_checked_channels':sum(v['channels'] for v in results),'maximum_error':max(v['max_error'] for v in results)},indent=2))
