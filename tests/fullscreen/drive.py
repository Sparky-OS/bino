#!/usr/bin/env python3
"""Real KWin input; compare Bino's own F11 and double-click transitions."""
import json,os,subprocess,time,shutil
from pathlib import Path
import numpy as np
from PIL import Image
root=Path(os.environ['CASE_DIR'])
helper='/work/packaging/sparky-os/upstream/bino/proof/fakeinput'
def input(*args):subprocess.run([helper,*map(str,args)],check=True)
def compositor_geometry(name):
 script=root/(name+'.js');tag='fullscreen-'+name
 script.write_text('const w=workspace.activeWindow;const r=w.clientGeometry;print("BINO_GEOM_'+name+' "+JSON.stringify([r.x,r.y,r.width,r.height,w.fullScreen]));')
 qdbus=shutil.which('qdbus6') or '/usr/lib/qt6/bin/qdbus'
 def call(path,*args):return subprocess.check_output([qdbus,'org.kde.KWin',path,*args],text=True).strip()
 sid=call('/Scripting','org.kde.kwin.Scripting.loadScript',str(script),tag)
 call('/Scripting/Script'+sid,'org.kde.kwin.Script.run')
 call('/Scripting','org.kde.kwin.Scripting.unloadScript',tag)
 marker='BINO_GEOM_'+name+' '
 for _ in range(100):
  rows=[line.split(marker,1)[1] for line in (root/'kwin.log').read_text().splitlines() if marker in line]
  if rows:return json.loads(rows[-1])
  time.sleep(.02)
 raise RuntimeError('compositor geometry missing')
def snapshot(name):
 (root/'request').write_text(name+'\n')
 for _ in range(150):
  p=root/(name+'.json')
  if p.exists() and p.stat().st_size:break
  time.sleep(.05)
 state=json.loads(p.read_text())
 state['compositor_client']=compositor_geometry(name)
 env=dict(os.environ,QT_QPA_PLATFORM='offscreen')
 subprocess.run(['/work/probes/screen-capture','screen',str(root/(name+'-screen.png'))],env=env,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 image=np.asarray(Image.open(root/(name+'-screen.png')).convert('RGB'))
 state['capture_width']=image.shape[1]
 views=[image] if image.shape[1]==1280 else [image[:,:1280],image[:,1280:]]
 state['white_rects']=[]
 for eye in views:
  y,x=np.where(np.all(eye>240,axis=2))
  state['white_rects'].append([int(x.min()),int(y.min()),int(x.max()-x.min()+1),int(y.max()-y.min()+1)] if x.size else [])
 (root/(name+'.json')).write_text(json.dumps(state,indent=2)+'\n')
 return state
def click_at(state,overlay=False,double=False):
 x,y,w,h=state['widget']
 if os.environ.get('FULLSCREEN_PLATFORM','wayland')=='wayland':
  x+=state['compositor_client'][0]+state['margins'][0];y+=state['compositor_client'][1]+state['margins'][1]
 vh=min(h,round(w*9/16));vw=min(w,round(h*16/9));top=y+(h-vh)/2
 # The centre of the play/pause button, or the centre of the picture.
 pos=(x+w/2,top+vh-(2.75 if overlay=='panel' else 2)*vw/18) if overlay else (x+w/2,y+h/2)
 input('move',*pos,'click',*(['click'] if double else []),'sleep',850)

input('sleep',2000)
states={'normal':snapshot('normal')}
input('key',87,'sleep',850);states['f11-in']=snapshot('f11-in')
input('key',87,'sleep',850);states['f11-out']=snapshot('f11-out')
click_at(states['f11-out'],double=True);states['double-in']=snapshot('double-in')
click_at(states['double-in'],double=True);states['double-out']=snapshot('double-out')
# Keep the existing single-click play/pause control functional in each direction.
click_at(states['double-out'],overlay=True);states['single-pause']=snapshot('single-pause')
click_at(states['single-pause'],overlay=True);states['single-play']=snapshot('single-play')
# The painted overlay must not divert the double-click to window maximization.
click_at(states['single-play'],overlay='panel',double=True);states['overlay-in']=snapshot('overlay-in')
click_at(states['overlay-in'],overlay='panel',double=True);states['overlay-out']=snapshot('overlay-out')
checks={
 'f11_enters_fullscreen':states['f11-in']['fullscreen'],
 'f11_fills_both_eyes':states['f11-in']['white_rects']==[[0,0,1280,720]]*(states['f11-in']['capture_width']//1280),
 'f11_leaves_fullscreen':not states['f11-out']['fullscreen'],
 'double_state_equals_f11':states['double-in']['state']==states['f11-in']['state'],
 'double_video_equals_f11':states['double-in']['white_rects']==states['f11-in']['white_rects'],
 'double_returns_like_f11':all(states['double-out'][k]==states['f11-out'][k] for k in ('state','widget','white_rects')),
 'overlay_state_equals_f11':states['overlay-in']['state']==states['f11-in']['state'],
 'overlay_video_equals_f11':states['overlay-in']['white_rects']==states['f11-in']['white_rects'],
 'overlay_returns_like_f11':all(states['overlay-out'][k]==states['f11-out'][k] for k in ('state','widget','white_rects')),
 'single_pauses':states['double-out']['playing'] and not states['single-pause']['playing'],
 'single_plays':states['single-play']['playing'],
 'double_keeps_playback':states['double-in']['playing'] and states['double-out']['playing'] and states['overlay-in']['playing'] and states['overlay-out']['playing'],
}
result={'pass':all(checks.values()),'checks':checks,'states':states}
(root/'result.json').write_text(json.dumps(result,indent=2)+'\n')
(root/'request').write_text('quit\n')
print(json.dumps(result,indent=2))
raise SystemExit(0 if result['pass'] else 1)
