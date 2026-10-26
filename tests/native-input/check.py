#!/usr/bin/env python3
"""Require native left-first samples; expectations never use the implementation."""
import json
import sys
from pathlib import Path
import numpy as np
from PIL import Image
case, fixtures, packing = map(Path,sys.argv[1:4])
log = case/'buffers/log.jsonl'
assert log.exists(), 'missing declared stereo strip (marked input was shown as mono)'
left = np.array(Image.open(fixtures/(str(packing)+'-left.png')).convert('RGB'))
right = np.array(Image.open(fixtures/(str(packing)+'-right.png')).convert('RGB'))
expected = np.concatenate((left,right),axis=1).astype(int)
checked = 0
for line in log.read_text().splitlines():
    item = json.loads(line)
    assert (item['width'],item['height']) == (expected.shape[1],expected.shape[0]), item
    got = np.fromfile(case/'buffers'/item['file'],dtype='uint8').reshape(item['height'],item['stride'])
    got = got[:,:item['width']*4].reshape(item['height'],item['width'],4)[::-1,:,:3].astype(int)
    error = np.abs(got-expected)
    assert error.max() <= 1, (item, int(error.max()), int((error>1).sum()))
    checked += got.size
assert checked
print(f'PASS {checked} source-checked channels; both eyes, native raster, tolerance 1')
