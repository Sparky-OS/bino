#!/usr/bin/env python3
"""A native strip must stay inside the application's ordinary window."""
import json,re,sys
from pathlib import Path
from PIL import Image
case=Path(sys.argv[1]);app=(case/'app.log').read_text()
logical=re.findall(r'xdg_surface#\d+\.set_window_geometry\((-?\d+), (-?\d+), (\d+), (\d+)\)',app)
assert logical,'no committed top-level window geometry'
w,h=map(int,logical[-1][2:]); actual=Image.open(case/'window.png').size
# ScreenShot2 includes both views and may include server decoration margins.
report={'window_geometry':[w,h],'window_capture':list(actual),'pass':actual[0]<=2*(w+16) and actual[1]<=h+64}
(case/'containment.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
assert report['pass'],'native video escaped the ordinary window bounds'
