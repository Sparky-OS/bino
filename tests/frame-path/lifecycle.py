from pathlib import Path
import json
import numpy as np
from PIL import Image
import argparse, subprocess
parser=argparse.ArgumentParser()
parser.add_argument('directory',type=Path)
parser.add_argument('--clip',type=Path)
parser.add_argument('--accurate-yuv',action='store_true')
a=parser.parse_args();p=a.directory
frames=None
if a.clip:
 decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(a.clip),'-threads','4',
  *(['-sws_flags','accurate_rnd+full_chroma_int'] if a.accurate_yuv else []),
  '-f','rawvideo','-pix_fmt','rgb24','pipe:1'])
 frames=np.frombuffer(decoded,np.uint8).reshape(72,180,640,3)
results=[]
for stage in range(5):
 info=json.loads((p/f'stage{stage}.json').read_text())
 assert (info['width'],info['height'],info['views'])==(640,180,1 if stage==3 else 2),info
 assert (info['windowWidth'],info['windowHeight'])==((640,360) if stage==0 else (480,270)),info
 n=round(info['pts']*24/1e6);y,x=np.indices((180,640))
 expected=np.stack(((x%320+n*13)%256,(y+n*7)%256,(x//320)*128+(x%320//256)*16+n%8),axis=2)
 if frames is not None:
  assert 0<=n<len(frames)
  expected=frames[n].astype(int)
 actual=np.array(Image.open(p/f'stage{stage}.png').convert('RGB')).astype(int)
 error=np.abs(actual-expected)
 assert error.max()<=1,(stage,error.max())
 results.append(dict(stage=stage,views=info['views'],source_max_error=int(error.max()),channels=actual.size))
(p/'lifecycle.json').write_text(json.dumps(results,indent=2)+'\n')
