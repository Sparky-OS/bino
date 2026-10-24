import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('case',type=Path);p.add_argument('reference');a=p.parse_args()
x=np.array(Image.open(a.case/'overlay.png').convert('RGBA')).astype(int)
y=np.array(Image.open(a.case/a.reference).convert('RGBA')).astype(int)
assert x.shape==y.shape==(360,640,4),(x.shape,y.shape)
assert np.sum(y[:,:,3]>0)>500
# QImage is straight RGBA; OpenGL is premultiplied. Alpha directly measures edge quality.
alpha=np.abs(x[:,:,3]-y[:,:,3]);assert alpha.max()<=1,(alpha.max(),np.sum(alpha>1))
rgb=np.abs(x[:,:,:3]-y[:,:,:3]*(y[:,:,3,None]/255));assert rgb.max()<=1.5,rgb.max()
(a.case/'quality.json').write_text(json.dumps({'display_raster':[640,360],'nonzero_alpha_pixels':int(np.sum(y[:,:,3]>0)),'max_alpha_error':int(alpha.max()),'max_premultiplied_rgb_error':float(rgb.max())},indent=2)+'\n')
