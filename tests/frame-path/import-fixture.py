#!/usr/bin/env python3
"""Time-varying source addresses accepted by VA-API H.264/HEVC decoders."""
import argparse
import subprocess
from pathlib import Path
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path)
parser.add_argument('--color', action='store_true')
parser.add_argument('--ten-bit', action='store_true')
a = parser.parse_args()
a.directory.mkdir(parents=True, exist_ok=True)
w, h = 640, 180
y, x = np.indices((h, w))
codec = ['-c:v', 'libx265', '-x265-params', 'pools=4:frame-threads=1:log-level=error'] if a.ten_bit else ['-c:v', 'libx264']
cmd = ['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt',
       'yuv420p10le' if a.ten_bit else 'yuv420p', '-s', f'{w}x{h}', '-r', '24',
       '-color_range', 'tv', '-colorspace', 'bt709', '-color_trc', 'bt709',
       '-color_primaries', 'bt709', '-i', 'pipe:0', '-an', *codec, '-preset', 'veryfast', '-crf', '18', '-threads', '4',
       '-color_range', 'tv', '-colorspace', 'bt709', '-color_trc', 'bt709',
       '-color_primaries', 'bt709', str(a.directory / 'import.mkv')]
p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
for frame in range(72):
    luma = 16 + ((x % 320 + 2 * y + frame * 13 + (x // 320) * 79) % 220)
    u, v = (96, 160) if a.color else (128, 128)
    planes = [luma, np.full((h // 2, w // 2), u), np.full((h // 2, w // 2), v)]
    for plane in planes:
        if a.ten_bit:
            p.stdin.write((plane * 4).astype('<u2').tobytes())
        else:
            p.stdin.write(plane.astype(np.uint8).tobytes())
p.stdin.close()
assert p.wait() == 0
