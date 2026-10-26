#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reference decoder samples, centre-sited bilinear chroma, BT.709 equations.

Each decoded view is converted independently. A joined chroma plane may not
borrow samples from the other eye at its edge. No implementation calls/shaders
supply the expected image. Pixel tolerance stays at one 8-bit channel value.
"""
import json
import sys
from pathlib import Path
import numpy as np
case,root=map(Path,sys.argv[1:3]);w,h=320,240

def interpolate(a):
    x=(np.arange(w)+.5)/2-.5;y=(np.arange(h)+.5)/2-.5
    ix=np.floor(x).astype(int);iy=np.floor(y).astype(int);fx=x-ix;fy=y-iy
    first=a[:,np.clip(ix,0,w//2-1)]*(1-fx)+a[:,np.clip(ix+1,0,w//2-1)]*fx
    return first[:, :][np.clip(iy,0,h//2-1)]*(1-fy[:,None])+first[np.clip(iy+1,0,h//2-1)]*fy[:,None]

def expected(data):
    y=data[:w*h].reshape(h,w).astype(float)
    u=interpolate(data[w*h:w*h*5//4].reshape(h//2,w//2).astype(float))
    v=interpolate(data[w*h*5//4:].reshape(h//2,w//2).astype(float))
    yy=(y-16)/219;cb=(u-128)/224;cr=(v-128)/224
    kr,kb=.2126,.0722;kg=1-kr-kb
    rgb=np.stack((yy+2*(1-kr)*cr,yy-2*kb*(1-kb)/kg*cb-2*kr*(1-kr)/kg*cr,yy+2*(1-kb)*cb),axis=-1)
    return np.rint(np.clip(rgb,0,1)*255).astype('int16')
refs=[np.fromfile(root/f'view{e}.yuv',dtype='uint8').reshape(96,-1) for e in range(2)]
frames=[np.concatenate([expected(refs[e][n]) for e in range(2)],axis=1) for n in range(96)]
records=[json.loads(l) for l in (case/'buffers/log.jsonl').read_text().splitlines()]
checked=0;fail=[]
for item in records:
    assert (item['width'],item['height'])==(2*w,h)
    got=np.fromfile(case/'buffers'/item['file'],dtype='uint8').reshape(h,item['stride'])[:,:8*w].reshape(h,2*w,4)[::-1,:,:3].astype('int16')
    # Identify one source timestamp from both eyes' interiors, then check ALL
    # samples against that same pair, including both edges and the centre seam.
    scores=[np.abs(got[:,8:2*w-8]-f[:,8:2*w-8]).sum() for f in frames]
    n=int(np.argmin(scores));err=np.abs(got-frames[n]);bad=err>1
    if bad.any():fail.append({'buffer':item['file'],'source_frame':n,'bad_channels':int(bad.sum()),'max_error':int(err.max()),'bad_columns':np.where(bad.any(axis=(0,2)))[0].tolist()})
    checked+=got.size
print(json.dumps({'checked_channels':checked,'frames':len(records),'failures':fail},indent=2))
raise SystemExit(not records or bool(fail))
