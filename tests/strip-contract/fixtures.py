#!/usr/bin/env python3
"""Synthetic source-address fixtures. No filename supplies a packing hint."""
import json
import subprocess
import sys
from pathlib import Path
import numpy as np
from PIL import Image

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
def eye(e,w,h):
    y,x=np.indices((h,w))
    return np.stack((x&255,y&255,(e<<7)|((x>>8)<<3)|(y>>8)),axis=-1).astype('uint8')
cases=[]
for packing,mode in [('sbs',1),('tab',3),('rows',7),('columns',9),('checkerboard',5)]:
    for invert in [False,True]:
        w,h=(320,90) if packing in ('tab','rows') else (160,180)
        left,right=eye(0,w,h),eye(1,w,h)
        a,b=(right,left) if invert else (left,right)
        if packing=='sbs': frame=np.concatenate((a,b),axis=1)
        elif packing=='tab': frame=np.concatenate((a,b),axis=0)
        elif packing=='rows':
            frame=np.empty((2*h,w,3),dtype='uint8');frame[::2]=a;frame[1::2]=b
        else:
            frame=np.empty((h,2*w,3),dtype='uint8')
            for y in range(h):
                p=y%2 if packing=='checkerboard' else 0
                frame[y,p::2]=a[y];frame[y,1-p::2]=b[y]
        mark={1:11,3:2,7:6,9:8,5:4}[mode] if invert else mode
        cases.append((frame,left,right,mark,'ffv1','1/1'))
# Alita's dimensions and marks; synthetic sample addresses, no film bytes.
left,right=eye(0,960,804),eye(1,960,804)
cases.append((np.concatenate((left,right),axis=1),left,right,1,'libx264rgb','2/1'))
cases.append((left,left,None,None,'ffv1','1/1'))
manifest=[]
for i,(frame,left,right,mark,codec,sar) in enumerate(cases):
    name=f'case-{i:02}'
    Image.fromarray(frame).save(out/(name+'.png'))
    Image.fromarray(left).save(out/(name+'-left.png'))
    if right is not None: Image.fromarray(right).save(out/(name+'-right.png'))
    cmd=['ffmpeg','-v','error','-y','-loop','1','-framerate','12','-i',str(out/(name+'.png')),
         '-vf','setsar='+sar,'-frames:v','36','-c:v',codec,'-threads','4']
    cmd += ['-crf','0','-preset','ultrafast'] if codec=='libx264rgb' else ['-pix_fmt','bgr0']
    if mark is not None: cmd+=['-metadata:s:v','stereo_mode='+str(mark)]
    subprocess.run(cmd+[str(out/(name+'.mkv'))],check=True)
    probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-of','json',str(out/(name+'.mkv'))]))
    (out/(name+'-probe.json')).write_text(json.dumps(probe,indent=2))
    manifest.append(dict(name=name,mark=mark,codec=codec,sar=sar,eye=[left.shape[1],left.shape[0]],
                         strip=[left.shape[1]*(2 if right is not None else 1),left.shape[0]],
                         display=[left.shape[1]*(2 if sar=='2/1' else 1),left.shape[0]],stereo=right is not None))
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
