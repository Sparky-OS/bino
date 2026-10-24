#!/usr/bin/env python3
"""Check actual overlay captures and composition over independent video pixels."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image
p=Path(sys.argv[1]);fail=[]
for hover in range(-1,9):
    views=[np.asarray(Image.open(p/f'{s}-{hover}.png').convert('RGBA')) for s in (0,1)]
    assert np.array_equal(*views),'2D and 3D controls differ'
    for i in range(9):
        x,y=20+i*80,300
        for dx,dy in [(1,1),(38,1),(1,38),(38,38)]:
            pixel=views[0][y+dy,x+dx]
            if pixel[3]: fail.append((hover,i,dx,dy,pixel.tolist()))
        # Outside the icon, inside the round backing: unchanged 70% alpha.
        px=views[0][y+20,x+2]
        assert 177<=px[3]<=180,(hover,i,px.tolist())
        assert tuple(px[:3])==((255,0,0) if hover==i else (0,0,0)),(hover,i,px.tolist())
    # Composite actual overlay over a distinct ordinary video witness.
    y,x=np.indices((400,720)); video=np.stack((x%256,y%256,np.full_like(x,91)),axis=-1).astype('uint8')
    alpha=views[0][:,:,3,None]/255
    composed=np.rint(views[0][:,:,:3]*alpha+video*(1-alpha)).astype('uint8')
    Image.fromarray(composed).save(p/f'composed-{hover}.png')
print('opaque button corner samples:',len(fail),'first:',fail[:2])
assert not fail,'button corners cover the video'
print('nine buttons: transparent corners, round normal/hover backing, identical 2D/3D')
