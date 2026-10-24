#!/usr/bin/env python3
"""Temporal pairs and HD half/full packings supplement the spatial matrix."""
import json,subprocess,sys
from pathlib import Path
import numpy as np
from PIL import Image
out=Path(sys.argv[1]);manifest=json.loads((out/'manifest.json').read_text())
def eye(e,w,h):
 y,x=np.indices((h,w));return np.stack((x&255,y&255,(e<<7)|((x>>8)<<3)|(y>>8)),axis=-1).astype('uint8')
for name,mark,w,h,sar in [('case-12',13,320,180,'1/1'),('case-13',14,320,180,'1/1'),('case-14',3,1920,540,'1/2'),('case-15',1,1920,1080,'1/1')]:
 left,right=eye(0,w,h),eye(1,w,h)
 for label,im in [('left',left),('right',right)]:Image.fromarray(im).save(out/(name+'-'+label+'.png'))
 if mark in (13,14):
  a,b=(left,right) if mark==13 else (right,left)
  frames=np.stack([a,b]*18);fw,fh=w,h
 else:
  a=np.concatenate((left,right),axis=0 if mark==3 else 1)
  frames=np.stack([a]*12);fh,fw=a.shape[:2]
 cmd=['ffmpeg','-v','error','-y','-f','rawvideo','-pixel_format','rgb24','-video_size',f'{fw}x{fh}','-framerate','12','-i','pipe:0','-vf','setsar='+sar,'-c:v','ffv1','-pix_fmt','bgr0','-threads','4','-metadata:s:v','stereo_mode='+str(mark),str(out/(name+'.mkv'))]
 subprocess.run(cmd,input=frames.tobytes(),check=True)
 probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-of','json',str(out/(name+'.mkv'))]))
 (out/(name+'-probe.json')).write_text(json.dumps(probe,indent=2))
 num,den=map(int,sar.split('/'))
 row=dict(name=name,mark=mark,codec='ffv1',sar=sar,eye=[w,h],strip=[2*w,h],display=[w*num,h*den],stereo=True)
 manifest=[r for r in manifest if r['name']!=name]+[row]
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
