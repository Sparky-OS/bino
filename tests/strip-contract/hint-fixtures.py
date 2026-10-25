#!/usr/bin/env python3
"""Unmarked guesses and misleading names, with independent source-eye references."""
import json, shutil, subprocess, sys
from pathlib import Path
from PIL import Image
src, out = map(Path, sys.argv[1:]); out.mkdir(parents=True, exist_ok=True)
cases = [
    ('unmarked-SBS', 'case-00', None, '1/1', [320,180]),
    ('unmarked-RL', 'case-01', None, '1/1', [320,180]),
    ('unmarked-BT', 'case-03', None, '1/1', [320,90]),
    ('unmarked-FSBS', 'case-00', None, '1/1', [160,180]),
    ('unmarked-HSBS', 'case-00', None, '1/1', [320,180]),
    ('unmarked-TAB', 'case-02', None, '1/1', [320,180]),
    ('unmarked-FTAB', 'case-02', None, '1/1', [320,90]),
    ('unmarked-lrq', 'case-00', None, '1/1', [320,180]),
    ('unmarked-SBS-SAR', 'case-10', None, '2/1', [1920,804]),
    ('unmarked-HSBS-SAR', 'case-10', None, '2/1', [1920,804]),
    ('declared-TAB', 'case-10', 1, '2/1', [1920,804]),
    ('declared-HSBS', 'case-03', 2, '1/1', [320,90]),
    ('declared-mono-SBS', 'case-15', 0, '1/1', [3840,1080]),
    ('shape', 'case-15', None, '1/1', [1920,1080]),
    ('plain', 'case-11', None, '1/1', [960,804]),
    ('plain-360', 'case-11', None, '1/1', [960,804]),
]
manifest=[]
base={c['name']:c for c in json.loads((src/'manifest.json').read_text())}
for name,source,mark,sar,display in cases:
    frame=src/(source+'.png')
    # Extra fixtures keep source frames alongside the manifest too.
    if not frame.exists():
        left=Image.open(src/(source+'-left.png'));right=Image.open(src/(source+'-right.png'))
        image=Image.new('RGB',(left.width+right.width,left.height));image.paste(left);image.paste(right,(left.width,0))
        frame=out/(name+'.png');image.save(frame)
    cmd=['ffmpeg','-v','error','-y','-loop','1','-framerate','12','-i',str(frame),'-vf','setsar='+sar,
         '-frames:v','36','-c:v','ffv1','-pix_fmt','bgr0','-threads','4']
    if mark is not None:cmd+=['-metadata:s:v','stereo_mode='+str(mark)]
    subprocess.run(cmd+[str(out/(name+'.mkv'))],check=True)
    c=dict(base[source]);c.update(name=name,mark=mark,sar=sar,display=display,stereo=source!='case-11' and mark!=0)
    if c['stereo']:
        for eye in ('left','right'):shutil.copyfile(src/(source+'-'+eye+'.png'),out/(name+'-'+eye+'.png'))
    manifest.append(c)
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
