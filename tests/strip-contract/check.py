#!/usr/bin/env python3
"""Check the actual declared surface against independently authored source samples."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('fixtures',type=Path);p.add_argument('case');p.add_argument('capture',type=Path)
p.add_argument('--swap-expected',action='store_true')
a=p.parse_args();case=next(c for c in json.loads((a.fixtures/'manifest.json').read_text()) if c['name']==a.case)
log=a.capture/'buffers/log.jsonl'
rows=[json.loads(x) for x in log.read_text().splitlines()] if log.exists() else []
report=dict(case=a.case,declared_buffers=len(rows),checks={})
checks=report['checks']; checks['declared_stereo']=bool(rows)==case['stereo']
if case['stereo'] and rows:
    expected=np.concatenate([np.asarray(Image.open(a.fixtures/(a.case+'-'+eye+'.png')).convert('RGB')) for eye in (('right','left') if a.swap_expected else ('left','right'))],axis=1)
    checks['native_size']=all([r['width'],r['height']]==case['strip'] for r in rows)
    checks['source_display_size']=all(r['destination']==case['display'] for r in rows)
    errors=[]
    if checks['native_size']:
        for r in rows:
            raw=np.fromfile(a.capture/'buffers'/r['file'],dtype=np.uint8).reshape(r['height'],r['width'],4)[::-1,:,:3]
            errors.append(int(np.abs(raw.astype('int16')-expected.astype('int16')).max()))
        checks['source_samples']=max(errors)<=1 # retained Bino source gate, unchanged
        report['max_channel_error']=max(errors)
        report['channels_checked']=expected.size*len(rows)
    else: checks['source_samples']=False
    report['destinations']=[list(x) for x in sorted({tuple(r['destination']) for r in rows})]
report['pass']=all(checks.values());print(json.dumps(report,indent=2))
(a.capture/('swapped-result.json' if a.swap_expected else 'result.json')).write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if report['pass'] else 1)
