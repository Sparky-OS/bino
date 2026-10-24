from pathlib import Path
import argparse,json
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('case',type=Path);p.add_argument('--wrong-eye',action='store_true');a=p.parse_args()
case=a.case
screen=np.array(Image.open(case/'screen.png').convert('RGB'))
overlay=np.array(Image.open(case/'overlay.png').convert('RGBA'))
h,w=overlay.shape[:2]
pts=int((case/'held').read_text());frame=round(pts*24/1000000)
y,x=np.indices((180,320)); references=[]
for eye in range(2):
    src=np.stack(((x+frame*13)%256,(y+frame*7)%256,eye*128+(x//256)*16+frame%8),axis=2).astype(float)
    # Pixel-center bilinear reconstruction, clamped independently at each eye's edges.
    xx=np.clip((np.arange(w)+.5)*320/w-.5,0,319); yy=np.clip((np.arange(h)+.5)*180/h-.5,0,179)
    x0=np.floor(xx).astype(int);y0=np.floor(yy).astype(int);x1=np.minimum(x0+1,319);y1=np.minimum(y0+1,179)
    wx=(xx-x0)[None,:,None];wy=(yy-y0)[:,None,None]
    ref=(src[y0[:,None],x0[None,:]]*(1-wx)+src[y0[:,None],x1[None,:]]*wx)*(1-wy)+(src[y1[:,None],x0[None,:]]*(1-wx)+src[y1[:,None],x1[None,:]]*wx)*wy
    references.append(ref)
# Locate the expected video using several independent interior source addresses.
left=screen[:,:screen.shape[1]//2]
positions=[]
for oy in range(left.shape[0]-h+1):
    for ox in range(left.shape[1]-w+1):
        if all(np.max(np.abs(left[oy+py,ox+px].astype(float)-references[0][py,px]))<=1.5 for py,px in ((32,32),(60,130),(120,350))):
            positions.append((ox,oy))
assert positions,'source addresses absent in compositor capture'
mask=overlay[:,:,3]==0
scores=[]
for ox,oy in positions:
    actual=left[oy:oy+h,ox:ox+w].astype(float)
    scores.append(np.mean((actual[mask]-references[0][mask])**2))
ox,oy=positions[int(np.argmin(scores))]
result={'video_origin':[ox,oy],'destination':[w,h],'source_eye':[320,180],'frame':frame,'eyes':[]}
crops=[]
for eye in range(2):
    actual=screen[oy:oy+h,ox+eye*screen.shape[1]//2:ox+eye*screen.shape[1]//2+w].astype(float)
    expected=references[1-eye if a.wrong_eye else eye]
    diff=np.abs(actual[mask]-expected[mask]);mse=np.mean(diff**2)
    assert diff.max()<=1.5,(eye,float(diff.max()),float(np.mean(diff>1.5)))
    result['eyes'].append({'checked_channels':int(diff.size),'max_error':float(diff.max()),'psnr_db':float(10*np.log10(255**2/mse))})
    crops.append(actual)
opaque=overlay[:,:,3]==255
assert np.sum(opaque)>500
assert np.array_equal(crops[0][opaque],crops[1][opaque]),'opaque subtitle pixels differ between eyes'
assert np.max(np.abs(crops[0][opaque]-overlay[:,:,:3][opaque].astype(float)))<=1
result['identical_opaque_subtitle_pixels']=int(np.sum(opaque))
(case/'compositor.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
