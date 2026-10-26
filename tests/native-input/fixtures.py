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
# An unspecified/custom declaration on a wide raster is NOT a pair. Keep mdat
# before moov so inserting a visual sample-entry box cannot shift chunk offsets.
import struct
wide=np.tile(source,(2,4,1))
Image.fromarray(wide).save(out/'custom-source.png')
subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-loop','1','-framerate','12','-i',str(out/'custom-source.png'),
                '-frames:v','36','-c:v','libx264rgb','-crf','0','-preset','ultrafast','-threads','4',str(out/'wide-unmarked.mp4')],check=True)
containers={b'moov':0,b'trak':0,b'mdia':0,b'minf':0,b'stbl':0,b'stsd':8,b'avc1':78}
def custom_boxes(data):
    offset=0; result=b''
    while offset<len(data):
        size,tag=struct.unpack_from('>I4s',data,offset)
        assert 8<=size<=len(data)-offset
        payload=data[offset+8:offset+size]; offset+=size
        if tag in containers:
            skip=containers[tag];payload=payload[:skip]+custom_boxes(payload[skip:])
        if tag==b'avc1': payload+=struct.pack('>I4sIB',13,b'st3d',0,3)
        result+=struct.pack('>I4s',len(payload)+8,tag)+payload
    return result
(out/'wide-custom.mp4').write_bytes(custom_boxes((out/'wide-unmarked.mp4').read_bytes()))
# Container SAR on actual MV-HEVC views; compressed samples and associations stay
# byte-identical. This fixture has mdat before moov, so chunk offsets stay valid.
if len(sys.argv)>2:
    native=Path(sys.argv[2]).read_bytes()
    assert native.index(b'mdat')<native.index(b'moov')
    native_containers=dict(containers);native_containers[b'hvc1']=78
    def native_sar(data):
        offset=0;result=b''
        while offset<len(data):
            size,tag=struct.unpack_from('>I4s',data,offset)
            assert 8<=size<=len(data)-offset
            payload=data[offset+8:offset+size];offset+=size
            if tag==b'pasp':continue
            if tag in native_containers:
                skip=native_containers[tag];payload=payload[:skip]+native_sar(payload[skip:])
            if tag==b'hvc1':payload+=struct.pack('>I4sII',16,b'pasp',2,1)
            result+=struct.pack('>I4s',len(payload)+8,tag)+payload
        return result
    (out/'native-sar2.mp4').write_bytes(native_sar(native))
