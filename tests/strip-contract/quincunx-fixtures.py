#!/usr/bin/env python3
"""Set H.264 frame-packing SEI's quincunx bit without touching coded samples."""
import json,re,subprocess,sys
from pathlib import Path
import numpy as np
from PIL import Image
root=Path(sys.argv[1]);rows=json.loads((root/'manifest.json').read_text())
def unescape(n):return n.replace(b'\x00\x00\x03',b'\x00\x00')
def escape(n):
 o=bytearray();z=0
 for b in n:
  if z>=2 and b<=3:o.append(3);z=0
  o.append(b);z=z+1 if b==0 else 0
 return bytes(o)
for index,invert in [(16,False),(17,True)]:
 name=f'case-{index}';left=np.asarray(Image.open(root/'case-00-left.png'));right=np.asarray(Image.open(root/'case-00-right.png'))
 frame=np.concatenate((right,left) if invert else (left,right),axis=1)
 for label,a in [('',frame),('-left',left),('-right',right)]:Image.fromarray(a).save(root/(name+label+'.png'))
 raw=root/(name+'-base.h264')
 subprocess.run(['ffmpeg','-v','error','-y','-loop','1','-framerate','12','-i',str(root/(name+'.png')),'-frames:v','36','-c:v','libx264rgb','-threads','4','-crf','0','-preset','ultrafast','-x264-params','frame-packing=3',str(raw)],check=True)
 result=bytearray();changed=0
 for nal in re.split(b'\x00\x00(?:\x00)?\x01',raw.read_bytes()):
  if not nal:continue
  if nal[0]&31==6:
   data=unescape(nal[1:]);pos=0;rebuilt=bytearray()
   while pos<len(data) and data[pos]!=128:
    start=pos;kind=0;size=0
    while data[pos]==255:kind+=255;pos+=1
    kind+=data[pos];pos+=1
    while data[pos]==255:size+=255;pos+=1
    size+=data[pos];pos+=1
    payload=data[pos:pos+size];pos+=size
    if kind==45:
     bits=''.join(f'{b:08b}' for b in payload)
     assert bits[:10]=='1000000110',bits
     # ue(v) id=0, cancel=0, arrangement=3, quincunx=1;
     # content interpretation 1=left first, 2=right first; omit grid positions.
     bits=bits[:9]+'1'+f'{2 if invert else 1:06b}'+bits[16:22]+bits[38:]
     payload=int(bits,2).to_bytes(len(bits)//8,'big')
     assert len(payload)<255
     rebuilt+=bytes([45,len(payload)])+payload;changed+=1
    else:rebuilt+=data[start:pos]
   nal=nal[:1]+escape(bytes(rebuilt)+data[pos:])
  result+=b'\x00\x00\x00\x01'+nal
 assert changed
 marked=root/(name+'.h264');marked.write_bytes(result)
 subprocess.run(['ffmpeg','-v','error','-y','-r','12','-i',str(marked),'-c','copy',str(root/(name+'.mkv'))],check=True)
 probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_frames','-read_intervals','%+#1','-of','json',str(root/(name+'.mkv'))]))
 sd=[s for f in probe['frames'] for s in f.get('side_data_list',[]) if s.get('side_data_type')=='Stereo 3D']
 assert sd,probe
 detected=json.loads(subprocess.check_output([sys.argv[2],'--base','video/x-matroska','--json',str(root/(name+'.mkv'))]))
 assert (detected['layout'],detected['order'])==(7,int(invert)),detected
 (root/(name+'-probe.json')).write_text(json.dumps(probe,indent=2))
 rows=[r for r in rows if r['name']!=name]+[dict(name=name,mark='avc-quincunx',codec='libx264rgb',sar='1/1',eye=[160,180],strip=[320,180],display=[160,180],stereo=True)]
(root/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
