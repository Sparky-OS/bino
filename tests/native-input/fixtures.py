#!/usr/bin/env python3
"""Independent encoded-channel fixtures, never photographs or film bytes."""
import json
import subprocess
import sys
from pathlib import Path
import numpy as np
from PIL import Image
out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
y, x = np.indices((90, 160))
source = np.stack(((3*x+y)%256, (x+5*y+31)%256, (7*x+3*y+79)%256), axis=-1).astype('uint8')
Image.fromarray(source).save(out/'encoded.png')
for name, mark in [('red-cyan',10), ('green-magenta',12), ('mono',0), ('unmarked',None)]:
    args = ['ffmpeg','-nostdin','-v','error','-y','-loop','1','-framerate','12','-i',str(out/'encoded.png'),
            '-vf','setsar=2/1','-frames:v','36','-c:v','ffv1','-pix_fmt','bgr0','-threads','4']
    if mark is not None: args += ['-metadata:s:v','stereo_mode='+str(mark)]
    subprocess.run(args+[str(out/(name+'.mkv'))],check=True)
    probe = subprocess.check_output(['ffprobe','-v','error','-show_streams','-of','json',str(out/(name+'.mkv'))])
    (out/(name+'.json')).write_bytes(probe)
# Expectations are equations transcribed independently from the accepted policy,
# not calls to the library or the shader under test. Integer division truncates.
r,g,b = source.astype('uint16').transpose(2,0,1)
for name,l,rgt in [('red-cyan',(r,(r+g)//2,(r+b)//2),((r+g)//2,g,b)),
                   ('green-magenta',((r+g)//2,g,(g+b)//2),(r,(r+g+b)//3,b)),
                   ('amber-blue',(r,g,(g+b)//2),((r+b)//2,(g+b)//2,b))]:
    for eye, channels in [('left',l),('right',rgt)]:
        Image.fromarray(np.stack(channels,axis=-1).astype('uint8')).save(out/(name+'-'+eye+'.png'))
