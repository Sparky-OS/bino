#!/usr/bin/env python3
"""Summarize real traces; optionally enforce one rendering pass per eye."""
import argparse
import collections
import csv
import json
import statistics
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("case", type=Path)
parser.add_argument("--expect-frames", type=int, default=288)
parser.add_argument("--check-no-duplicate-views", action="store_true")
args = parser.parse_args()
rows = list(csv.DictReader((args.case / "trace.csv").open()))
assert rows, "empty trace"
sink = [r for r in rows if r["event"] == "sink" and int(r["pts_us"]) >= 0]
pts = [int(r["pts_us"]) for r in sink]
assert len(pts) == args.expect_frames, ("sink delivery", len(pts), args.expect_frames)
assert len(set(pts)) == len(pts) and pts == sorted(pts), "duplicate or reordered PTS"
active = [r for r in rows if 1_000_000 < int(r["pts_us"]) < 11_000_000]
stats = {}
for event in sorted({r["event"] for r in active}):
    durations = sorted(int(r["duration_ns"]) / 1e6 for r in active if r["event"] == event)
    stats[event] = {"count": len(durations), "median_ms": statistics.median(durations),
                    "p95_ms": durations[int((len(durations) - 1) * .95)],
                    "total_ms": sum(durations)}
passes = 0
left = None
pairs = []
for r in rows:
    if r["event"] == "view":
        passes += 1
    if r["event"] != "paint":
        continue
    if r["arg1"] == "0":
        left = (r["pts_us"], passes)
    elif left and left[0] == r["pts_us"] and 1_000_000 < int(r["pts_us"]) < 11_000_000:
        pairs.append(left[1] + passes)
        left = None
    passes = 0
transfer_path = args.case / "transfers.csv"
transfers = list(csv.reader(transfer_path.open())) if transfer_path.exists() else []
assert all(int(r[2]) == 0 for r in transfers), "failed hardware transfer"
result = {"case": args.case.name, "sink_frames": len(pts),
          "sink_elapsed_s": (int(sink[-1]["start_ns"]) - int(sink[0]["start_ns"])) / 1e9,
          "hardware_transfers": len(transfers),
          "view_passes_per_stereo_pair": dict(collections.Counter(pairs)),
          "geometry": dict(collections.Counter(f'{r["arg1"]}x{r["arg2"]}'
                           for r in active if r["event"] == "geometry")),
          "events": stats}
if pairs:
    # Use the same clock as KWin's frame CSV, and exclude startup and clip end.
    start = next(int(r["start_ns"]) for r in sink if int(r["pts_us"]) > 1_000_000)
    end = next(int(r["start_ns"]) for r in sink if int(r["pts_us"]) >= 11_000_000)
    duration = (end - start) / 1e9
    result["active_window_ns"] = [start, end]
    for event in ("swap", "upload_convert"):
        count = sum(r["event"] == event and start <= int(r["start_ns"]) < end for r in rows)
        result[f"{event}_per_second"] = count / duration
    result["kwin_frames_per_second"] = {}
    for path in args.case.glob("kwin perf statistics *.csv"):
        data = list(csv.DictReader(path.open()))
        count = sum(start <= int(r["pageflip timestamp"]) < end for r in data)
        result["kwin_frames_per_second"][path.name] = count / duration
print(json.dumps(result, indent=2))
if args.check_no_duplicate_views:
    assert len(pairs) >= 100, "insufficient stereo pairs"
    assert all(n == 2 for n in pairs), f"expected two view passes per stereo pair, got {collections.Counter(pairs)}"
