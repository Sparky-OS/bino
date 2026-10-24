#!/usr/bin/env python3
"""Generate address-coded lossless sources; check every captured RGB sample."""
import argparse
import collections
import csv
import json
from pathlib import Path
import subprocess
import numpy as np
from PIL import Image

p = argparse.ArgumentParser()
p.add_argument("action", choices=("make", "check"))
p.add_argument("directory", type=Path)
p.add_argument("--clip", type=Path)
p.add_argument("--width", type=int, default=320)
p.add_argument("--height", type=int, default=180)
p.add_argument("--mono", action="store_true")
p.add_argument("--fallback", action="store_true")
p.add_argument("--check-passes", action="store_true")
p.add_argument("--swap-expected", action="store_true")
p.add_argument("--stale-expected", action="store_true")
a = p.parse_args()
ew, h = a.width, a.height
w = ew if a.mono else 2 * ew
if a.action == "make":
    a.directory.mkdir(parents=True, exist_ok=True)
    path = a.clip or a.directory / ("mono.mkv" if a.mono else "stereo.mkv")
    command = ["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgb24",
               "-video_size", f"{w}x{h}", "-framerate", "24", "-i", "pipe:0", "-an",
               "-c:v", "ffv1", "-pix_fmt", "bgr0", "-threads", "4", str(path)]
    encoder = subprocess.Popen(command, stdin=subprocess.PIPE)
    y, x = np.indices((h, w))
    for frame in range(72):
        source = np.stack(((x % ew + frame * 13) % 256, (y + frame * 7) % 256,
                           (x // ew) * 128 + (x % ew // 256) * 16 + frame % 8), axis=2)
        encoder.stdin.write(source.astype(np.uint8).tobytes())
    encoder.stdin.close()
    assert encoder.wait() == 0
    raise SystemExit(0)

decoded = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(a.clip),
                                   "-threads", "4", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"])
frames = np.frombuffer(decoded, np.uint8).reshape(-1, h, w, 3)
assert len(frames) == 72
captures = sorted(a.directory.glob("*-*.png"))
seen = collections.defaultdict(set)
max_error = 0
samples = 0
for path in captures:
    pts, target = map(int, path.stem.split("-"))
    index = round(pts * 24 / 1000000)
    eye = 0 if a.mono or a.fallback else target
    if a.swap_expected:
        eye = 1 - eye
    if a.stale_expected:
        index -= 1
    expected = frames[index, :, eye * ew:(eye + 1) * ew]
    actual = np.array(Image.open(path).convert("RGB"))
    assert actual.shape == expected.shape, (path, actual.shape, expected.shape)
    error = np.abs(actual.astype(np.int16) - expected.astype(np.int16))
    worst = int(error.max())
    assert worst <= 1, (path.name, "source pixel mismatch", worst, int(np.sum(error > 1)))
    max_error = max(max_error, worst)
    samples += actual.size
    seen[target].add(pts)
assert set(seen) == ({0} if a.fallback else {0, 1}), seen.keys()
assert all(len(values) >= 20 for values in seen.values()), seen
if not a.fallback:
    assert seen[0] == seen[1], "unpaired eye captures"

rows = list(csv.DictReader((a.directory / "trace.csv").open()))
views = []
passes = collections.Counter()
bad = []
for row in rows:
    if row["event"] == "view":
        views.append(int(row["arg1"]))
    elif row["event"] == "paint":
        if 500000 <= int(row["pts_us"]) <= 1500000:
            target = int(row["arg1"])
            expected = [0 if a.mono or a.fallback else target]
            passes[str(views)] += 1
            if views != expected:
                bad.append((row["pts_us"], target, views.copy(), expected))
        views = []
assert passes, "no paint callbacks"
print(json.dumps({"captures": len(captures), "rgb_samples": samples,
                  "source_max_error": max_error, "views_per_callback": dict(passes),
                  "bad_callbacks": len(bad)}, indent=2), flush=True)
if a.check_passes:
    assert not bad, ("wrong views for target", bad[:4])
