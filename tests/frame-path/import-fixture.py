#!/usr/bin/env python3
"""A time-varying luma-address source accepted by the VA-API H.264 decoder."""
import subprocess,sys
from pathlib import Path
import numpy as np
root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
w,h=640,180;y,x=np.indices((h,w))
cmd=['ffmpeg','-v','error','-y','-f','rawvideo','-pix_fmt','yuv420p','-s',f'{w}x{h}','-r','24','-i','pipe:0','-an','-c:v','libx264','-preset','veryfast','-crf','18','-threads','4','-color_range','tv','-colorspace','bt709','-color_trc','bt709','-color_primaries','bt709',str(root/'import.mkv')]
p=subprocess.Popen(cmd,stdin=subprocess.PIPE)
for frame in range(72):
    luma=16+((x%320+2*y+frame*13+(x//320)*79)%220)
    p.stdin.write(luma.astype(np.uint8).tobytes())
    p.stdin.write(bytes([128])*(w*h//2))
p.stdin.close();assert p.wait()==0
