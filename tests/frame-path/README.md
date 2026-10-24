# Frame-path diagnostic rig

This Linux diagnostic branch measures the shipped Bino 2.8 stereo3d2 rendering path without changing its output algorithm.
It is not a release candidate.
The input is a generated 288-frame H.264 High-profile 1920×1080 half-SBS clip at 24000/1001 fps.
There is no film, host display, host audio session or physical display in the rig.

## Setup

Use an existing writable scratch directory with this checkout at `bino/`.
Mount that scratch directory at `/work` in the container.
Copy `build.sh`, `run-case.sh`, `session.sh` and `probe-matrix.sh` into its root and make them executable.
Create `home/` and `evidence/` there.
The Dockerfile builds on the existing ORDER 234 development image and requires the public `sparky-stereo-repo.asc` key in its build context.
Use the repository address and Docker socket supplied by the lane coordinator.
The signed edition repository has priority 1001 in the base image.
Record package versions after installation; the repository is moving, so the Dockerfile alone does not pin a repeat to the measured versions.

Run every container with `--platform linux/amd64 --cpus 4 --user 1000:1000`.
Expose only the AMD render node, with `--device /dev/dri/renderD128 --group-add 125`.
The GUI cases additionally require `--cap-add SYS_NICE` because the packaged KWin binary has that file capability.
Provide a passwd entry for UID 1000 inside the container; D-Bus and Xwayland local-user authentication require it.
Never mount host X11, Wayland, D-Bus or PipeWire sockets.

## Runs

1. Run `sh /work/build.sh` to build Bino, the probes and the generated fixture.
2. Run `/work/probe-matrix.sh` to measure FFmpeg software decode, retained VA-API frames and VA-API copy-back, followed by four real-time Qt sink cases.
3. Run `/work/run-case.sh sw-baseline ,` in a fresh container for software decoding.
4. Run `/work/run-case.sh vaapi-baseline vaapi` in another fresh container for VA-API.
5. Repeat a GUI case with `-e BINO_GPU_TRACE=1` for asynchronous OpenGL timestamp queries, and compare with the same case without it.

Each GUI case creates a fresh private D-Bus session and headless KWin with a 1920×1080 virtual output and Xwayland.
KWin launches the session script and supplies its isolated display environment.
The script checks `glxinfo -B`, launches Bino fullscreen and records its exit status.
Exit 124 is the expected 16-second timeout after the 12.012-second clip has finished; other statuses require investigation.
The application receives SIGTERM by PID from `timeout`, and KWin exits with the session script.

`BINO_FRAME_TRACE` enables line-buffered CPU event CSV output.
`BINO_GPU_TRACE` additionally records GPU intervals using paired `GL_TIMESTAMP` queries, collected only when the results become available.
GPU intervals can include stalls; they are not isolated shader execution times.
There is no `glFinish` and no blocking query-result fetch.
The last pending GPU queries may be absent when the process exits; analysis excludes startup and clip end.
The timestamp column for GPU events is the CPU submission time, so interval durations and CPU start times come from different clocks by design.

The preload library counts actual `av_hwframe_transfer_data` calls.
It supports this FFmpeg 9 rig (`libavutil.so.61`), including Qt's locally loaded plugin.
A passive sink receiving hardware frames is not a rendering/import test.
The mapped sink invokes Bino's real `VideoFrame::update()` method.
Its positive transfer count checks that a zero count in the passive sink is meaningful.

## Analysis and failing regression

Inside the container:

```sh
python3 /work/bino/tests/frame-path/analyze.py /work/evidence/sw-baseline
python3 /work/bino/tests/frame-path/analyze.py /work/evidence/vaapi-baseline
python3 /work/bino/tests/frame-path/analyze.py /work/evidence/vaapi-baseline --check-no-duplicate-views
```

The last command is deliberately red on the uncorrected renderer: the expected work is one view pass per eye, but it renders both views in each of the two stereo callbacks.
The analyzer also rejects missing, duplicated or reordered sink timestamps and insufficient stereo pairs.
This is a reproduction to turn green in the subsequent fix, not a claim that a fix has already passed.
The software and VA-API sink cases must each deliver the 288 distinct source timestamps.

The report includes callback counts and durations within source PTS 1–11 seconds.
KWin presentation rows are counted within monotonic timestamps taken at the corresponding sink frames.
KWin CSV rows, widget swaps, texture uploads and distinct decoded frames are separate measurements: none may be substituted for another.
In particular, Bino's periodic overlay redraw can yield more swaps and KWin rows than the clip's frame rate.

These results describe the host AMD GPU and the generated clip.
They do not emulate Carrizo/Hainan performance, cross-GPU bandwidth or a TV's frame-packing scanout.
