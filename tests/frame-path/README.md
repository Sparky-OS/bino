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
The Dockerfile builds on Debian testing and requires the public `sparky-stereo-repo.asc` and `deb-multimedia-keyring.pgp` keys in its build context.
Use the repository address and Docker socket supplied by the lane coordinator.
The signed edition repository has priority 1001 above Debian and deb-multimedia.
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
After the current-target selection fix, the same command must pass with two view passes per stereo pair.
The analyzer also rejects missing, duplicated or reordered sink timestamps and insufficient stereo pairs.
Keep the failing baseline beside the corrected run so the gate's sensitivity remains reproducible.
The software and VA-API sink cases must each deliver the 288 distinct source timestamps.

The report includes callback counts and durations within source PTS 1–11 seconds.
KWin presentation rows are counted within monotonic timestamps taken at the corresponding sink frames.
KWin CSV rows, widget swaps, texture uploads and distinct decoded frames are separate measurements: none may be substituted for another.
In particular, Bino's periodic overlay redraw can yield more swaps and KWin rows than the clip's frame rate.

These results describe the host AMD GPU and the generated clip.
They do not emulate Carrizo/Hainan performance, cross-GPU bandwidth or a TV's frame-packing scanout.

## Current-target pixel regression

`widget-probe` links the real Bino player, sink, shaders and widget.
Its subclass reads the current target framebuffer after the real `Widget::paintGL` completes.
The probe does not replace rendering or inject private state.
It bypasses GUI controls to keep overlays away from the source pixels.
The generated 24 fps lossless RGB clips encode column, row, eye and frame identity, with 320×180 pixels per eye.
The widget has the same size as an eye, so this test isolates target selection from the separate native-resolution work.

```sh
python3 /work/bino/tests/frame-path/pixels.py make /work/clips/pixels
python3 /work/bino/tests/frame-path/pixels.py make /work/clips/pixels --mono
/work/bino/tests/frame-path/pixel-case.sh before-stereo stereo stereo
python3 /work/bino/tests/frame-path/pixels.py check /work/evidence/m2/before-stereo \
    --clip /work/clips/pixels/stereo.mkv --check-passes
```

Run each case in a fresh container and compositor session.
Use `mono stereo` for identical mono pixels in the two stereo targets, and `stereo fallback` for a normal non-stereo widget selecting the left view.
Pass the matching `--mono` or `--fallback` flag to the checker.
Repeat stereo with no render node and `LIBGL_ALWAYS_SOFTWARE=1` for the software renderer control.
Check the renderer in each case's `glxinfo.txt` and `app.log`.
The checker decodes the source independently with FFmpeg and compares every RGB channel in every capture within source PTS 0.5–1.5 seconds.
It allows one 8-bit code value for the linear/sRGB round trip and requires at least 20 distinct source timestamps per target, paired between eyes.
The separate pass assertion requires exactly the requested view in each callback, or view 0 for mono and non-stereo controls.
Omit `--check-passes` to verify pixel fidelity on the old renderer while retaining its failing pass count.
`--swap-expected` and `--stale-expected` must fail, demonstrating sensitivity to eye identity and frame freshness.
Readback is deliberately confined to this test executable and is not a performance measurement.

The llvmpipe control additionally uses 256×128 eyes (`--width 256 --height 128` in the fixture and checker, `PIXEL_WIDTH=256 PIXEL_HEIGHT=128 PIXEL_CLIP=/work/clips/pixels/stereo-256.mkv` in the probe).
The uncorrected renderer already blends sharp address transitions at 320×180 on llvmpipe; retain that failed source check and compare its pixels before and after separately.
The power-of-two software control and all 320×180 AMD controls must pass the source check.
This test does not relax pixel tolerance to accommodate the existing software sampling issue.

## Native sample preservation

At one stored pixel per target pixel, the view and display shaders use `texelFetch`.
This avoids filtered sampling across sharp source boundaries on llvmpipe at 320×180.
It does not change the scaling/filtering path when the target dimensions differ.
The 320×180 software case must now pass the same source check as the AMD case, with the same one-code-value tolerance and both eyes checked.
Keep a capture set from the preceding commit as a failing control.
Run `--swap-expected` and `--stale-expected` on the corrected software captures; both must still fail.
Compare all common AMD stereo, mono and non-stereo-target captures with the preceding commit byte for byte in RGB.
Passing this sampling test alone does not establish native-resolution delivery to the compositor.

## Native delivery regression

The probe queries the dimensions of the actual color attachment of each Qt target framebuffer, reads that full target, and records `target_geometry`.
This is separate from Bino's intermediate `geometry` trace.
Run a source eye at 320×180 with a 640×360 widget to expose enlargement:

```sh
CASE_ROOT=/work/evidence/m3 PIXEL_WIDTH=640 PIXEL_HEIGHT=360 \
    /work/bino/tests/frame-path/pixel-case.sh native-before stereo stereo
python3 /work/bino/tests/frame-path/pixels.py check /work/evidence/m3/native-before \
    --clip /work/clips/pixels/stereo.mkv --check-native-size --check-passes
```

The uncorrected native-delivery path fails with both intermediate and target dimensions 640×360 instead of the source eye's 320×180.
The new size assertion checks both stages before the independent source-pixel check.
An absent trace event also fails.
When rendering moves to a separate native layer, move the target readback to that submitted layer's framebuffer; reading the ordinary overlay framebuffer would no longer test the video handoff.
The final acceptance gate must additionally inspect the compositor surface declaration and buffer/destination sizes, check resizing and ordinary overlays in both eyes, and retain the mono and unsupported-transport controls.

## Native video surfaces

The native probe reads the EGL/GLX window framebuffer directly, asserts that its read framebuffer is zero, queries its drawable size and records its channel precision.
Qt's framebuffer wrapper redirects zero to the widget FBO, so a readback without that assertion can accidentally verify the overlay instead of the submitted video.
`--native-layer --check-native-size --check-passes` checks the submitted eyes against FFmpeg's decoded source pixels.
`--layout lr|rl|tb|bt` describes the fixture's stored layout, independently of Bino's `PIXEL_MODE` input option.
Use half-sized eye fixtures with the corresponding half-input modes and test odd eye dimensions as well.

`PIXEL_HOLD=1` pauses playback and records the final submitted timestamp and packed buffer, including frames already queued when pause was requested.
It captures the ordinary overlay framebuffer separately and obtains a compositor screen/window capture through ScreenShot2.
`PIXEL_SUBTITLE=1` enables the fixture subtitle track and saves the original subtitle renderer's display-resolution image for comparison.
The fixture text is `Native size: Aa Wm 0123456789`, covering the entire three-second clip.
`compositor.py CASE` independently reconstructs both eyes from the address formula with one pixel-center bilinear resize, checks the output pixels and verifies identical opaque subtitle pixels.
Its `--wrong-eye` control must fail.
`PIXEL_WRAPPED=1` additionally puts the video in an ordinary QMainWindow with a menu.

`PIXEL_LIFECYCLE=1 PIXEL_HOLD=1` retains native buffers across resize, hide/show, stereo-to-mono and mono-to-stereo stages.
`PIXEL_WARMUP=1` replays the same fixture three times after cold shader compilation; timestamp-based source expectations and the required capture count do not change.
This is a pixel-fidelity probe, not a throughput benchmark.
Use `LP_NUM_THREADS=4` for the software renderer, in addition to the container CPU quota.

`KWIN_BINARY` can select a locally built consumer; the default tests the installed compositor.
The X11 native path requires compositor support version 8 and libstereo-declare 1.7 for independent child placement below its ordinary parent.
The Wayland path uses the existing full-SBS surface declaration and viewport destination.
Both keep the stored eye raster unchanged, while the ordinary Qt window paints subtitles and controls at display resolution.

`PIXEL_UI=1 PIXEL_HOLD=1` checks the playback controls as a separate ordinary overlay.
Mount the published breeze-dark icon theme read-only at `/work/test-icons/breeze-dark` and provide the matching `qt6-svg-plugins` package; the probe rejects missing playback and seek icons.
The probe saves their original renderer's image from serialized player state; `overlay.py CASE ui-reference.png` compares alpha and premultiplied color at the display raster.
The same checker accepts `subtitle-reference.png` for subtitles.
`lifecycle.py CASE` checks every stored source sample and both buffer and window dimensions across the five lifecycle stages.
`PIXEL_CHECK_X11=1` inspects the live root capability, parent declaration, child layout, source raster and destination property independently through Xlib.
Adding `PIXEL_DROP_DECLARATION=1` removes only the child's layout before the compositor capture: the source gate must still pass while `compositor.py` fails.

The native layer prefers 10-bit RGB configurations, then uses an 8-bit window configuration when that is all the driver exposes.
The ordinary overlay keeps eight alpha bits independently of that choice.
Desktop OpenGL is requested explicitly; leaving the renderable type unspecified can select an incompatible EGL API on NVIDIA Wayland.
The actual video and overlay precision is recorded in the trace, so a software or NVIDIA result cannot be presented as proof of a different GPU's precision.

### Edition build gate and upstream output controls

Configure both the application and these probes with
`-DBINO_STEREO_DESKTOP=ON` or `-DBINO_STEREO_DESKTOP=OFF`.
ON is the edition default; the OFF application does not require libstereo-declare
or the native-surface development dependencies.
The manual and display shader resource follow the same option.

`output-modes-probe CLIP DIRECTORY MODE_NUMBER` uses the real `Gui` and its actions.
For OFF it verifies all 30 original output actions, triggers the chosen action,
checks its mode-string round trip, pauses the source, and captures the real widget
framebuffer twice (both target buffers for mode 2, consecutive frames for mode 3).
For ON it verifies that no output action is exposed.
Run each mode in a fresh isolated compositor session with a source containing
320x180 left RGB(49,97,151) and right RGB(211,173,67) eyes, lossless FFV1/bgr0 at
24 fps for three seconds.
Keep the source at `ROOT/source.mkv` and results at `ROOT/off-0` through
`ROOT/off-29`, with the process exit status in each `app-exit.txt`.
`output-modes.py ROOT` decodes and checks the fixture, then compares interior
pixels of each output to independently calculated packing, parity, luminance and
anaglyph expectations (maximum one 8-bit code value error).
The alternating case must contain both source eyes, and OpenGL stereo must have
the expected eye in each target buffer.
`output-modes.py ROOT --swap-expected` is a negative control and must fail.

For ON, also rerun `widget-probe` with the address-coded sources and the native
size/source gates above. The output-menu test alone does not prove native eye
geometry or the stereo declaration.

The restored checkerboard mode also has a retained parser regression: its old
`checkeroard` saved spelling did not round-trip. The corrected canonical spelling
is `checkerboard`; the old saved spelling remains accepted. The documented
`full-color` and `half-color` command-line names are accepted alongside the
original `fullcolor` and `halfcolor` names.
The interleaved-mode pixel checks exposed undefined implicit texture derivatives
in the original divergent eye-selection branches. Those three modes now use
explicit gradients calculated before the branch; their conversion and parity
rules are unchanged. Use `--modes 13 14 15` to repeat just that regression on a
second driver. The one-code-value tolerance is unchanged.

## Hardware texture import (milestone 4)

Build the edition with the private development headers matching Qt Multimedia
6.11.2-3+stereo3d9+lc1 (`qt6-multimedia-private-dev`). The OFF build does not
require them. `VideoFrame` retains supported frames unmapped until Bino has its
rendering context. `VideoTextureImporter` initializes `QHwVideoBuffer`'s converter
with a QRhi borrowing that context, calls `mapTextures`, validates the native GL
planes, and keeps the source frame and textures alive until a GL fence completes.
Unsupported imports use the existing map/upload path. The rendering and timer
policies are unchanged.

`import-fixture.py DIRECTORY` generates a 72-frame H.264 source with spatial and
temporal addresses in both eyes. `--color` adds fixed non-neutral chroma, and
`--ten-bit` generates a Main10 HEVC source. Raw input and encoded output both
specify BT.709 limited range. Fixed chroma avoids comparing different chroma
reconstruction filters at edges; the moving luma addresses still detect stale,
swapped, scaled or missing eyes.

Run `pixel-case.sh` in a fresh isolated compositor with `PIXEL_HW_DEVICE=vaapi`,
`PIXEL_TRACE_TRANSFER=1`, `PIXEL_CLIP` pointing at the fixture, and
`PROBE_BINARY` pointing at the candidate widget probe. `PIXEL_WARMUP=1` collects
across repeated playback when shader compilation or host load skips timestamps;
the source gate still requires at least 20 paired timestamps. For cold NVIDIA
startup, `PIXEL_TIMEOUT=40` bounds the whole process, including initialization
and teardown. The default remains 20 seconds.

Validate each run with both commands:

```
python3 pixels.py check CASE --clip CLIP --accurate-yuv --native-layer --check-native-size --check-passes
python3 import-path.py CASE --path import
```

The first checks every captured RGB channel against the independently decoded
source, at tolerance 1. `--accurate-yuv` requests FFmpeg's accurate rounding and
full chroma interpolation; the original RGB fixture/reference is unchanged.
The second requires imports and rejects every CPU map, application upload and
independently interposed `av_hwframe_transfer_data` call. Before the fix the
72-frame VA-API source records 72 maps and 72 hardware transfers and fails.

Run the same hardware decoder with `QT_DISABLE_HW_TEXTURES_CONVERSION=1` and
check `--path map`: CPU maps and independent transfers must occur, with no
imports. Set `PIXEL_HW_DEVICE=,` and check `--path software`: mapping must occur
but hardware transfers must not. On this Qt backend CUDA and the X11 GLX route
use mapping; their source pixels must pass too. Import failure is an ordinary
fallback, not a reason to discard the frame.

Run `deferred-probe` on the offscreen Qt platform to check metadata/re-update
without mapping, one-time mapping, unchanged serialization bytes, failed mapping,
and source ownership. Run `texture-lifetime-probe` with a real GL context to
check live native handles, source retention after the caller drops its frame,
bounded leases, unavailable/unsupported imports, current-surface restoration,
and context teardown/recreation. These are lifecycle tests with synthetic
textures; the VA-API runs supply the actual decoder/import proof.

A held hardware source with `PIXEL_HOLD=1 PIXEL_LIFECYCLE=1` checks resize,
hide/show, stereo-to-mono and mono-to-stereo while importing. Do not combine
lifecycle with playback warmup. Check it with `lifecycle.py CASE --clip CLIP
--accurate-yuv`, which validates the entire buffer at each stage's source PTS.

The Main10 test also exposes a pre-existing limited-range conversion error:
P010/P016 normalized R16 samples use denominator 65535, whereas the existing
8-bit matrix expects equivalent codes divided by 65280. Rescaling those samples
by 65535/65280 before the matrix corrects both import and mapping; 8-bit samples
keep scale 1. The retained pre-correction Main10 runs fail with maximum RGB
error 3 on both paths. This is an SDR BT.709 test, not HDR proof.

## Demand-driven repainting (milestone 5)

Create the low-rate fixture from the RGB pixel fixture without changing its pixels:

```
ffmpeg -v error -y -i pixels/stereo.mkv -vf setpts=6*PTS -r 4 -c:v ffv1 -pix_fmt bgr0 -threads 4 repaint.mkv
```

Run `pixel-case.sh` in a fresh isolated compositor with `PIXEL_REPAINT=1`,
`PIXEL_CLIP` pointing at that fixture, and `PIXEL_TIMEOUT=40`.
The native test uses a 640x360 parent and source-size 320x180 eyes.
Provide the Breeze playback icons at `/work/test-icons/breeze-dark` and a Qt SVG
icon engine matching the runtime Qt version; missing icons fail the probe.
Check with `repaint.py CASE --clip repaint.mkv`.

The observer records actual `paintGL` calls, their source PTS, the native pair,
and the ordinary overlay target. It never requests a repaint itself.
After observing a source frame, the timeline pauses, waits idle, reveals and
locks controls, resumes progress, pauses, changes stereo input to mono and back,
resizes, restores, unlocks, reveals controls again, waits for expiry, waits idle,
and hides/shows the window. Action timers use precise deadlines.

The checker compares both native eyes against independently decoded source RGB
at every captured timestamp with the existing tolerance of one code value.
For Qt stereo and ordinary fallback, it checks unobscured source-size targets;
the stereo path additionally requires consecutive left/right buffers at one PTS.
All four low-rate source timestamps must appear exactly once per target.
Both paused idle windows must have zero paints. Native controls must animate,
retain their lock, change the pause icon, expire, and survive resize/exposure.
`--swap-expected` must fail the source gate.

The pre-fix timer fails with repeated identical frames and paused idle paints,
after the source and overlay assertions pass. The fix keeps the 40 ms cadence
only while visible playback controls need progress animation, schedules the
remaining expiry deadline when paused, and uses frame/state/input notifications
for other changes. Static locked controls need no timer. The OFF build retains
its existing continuous update for alternating output.
