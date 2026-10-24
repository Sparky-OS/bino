# Native strip contract regression

Run in an isolated edition container, with the public stereo-proof-rig `wlrec.c` built as `/work/pact-tools/wlrec.so`.
`/work` is the lane's mounted scratch directory.
The real player is exercised without a command-line input mode.
`STRIP_NO_FFPROBE=1` gives the player an empty executable search path, reproducing installations without the optional ffprobe program.
It does not alter the clip or the player's linked decoder libraries.

```sh
python3 tests/strip-contract/fixtures.py /work/pact-fixtures
STRIP_CLIP=/work/pact-fixtures/case-10.mkv STRIP_NO_FFPROBE=1 \
  STRIP_BINARY=/work/pact-build/bino tests/strip-contract/case.sh native-sar
python3 tests/strip-contract/check.py /work/pact-fixtures case-10 /work/pact-evidence/native-sar
python3 tests/strip-contract/containment.py /work/pact-evidence/native-sar
```

Run one fresh container session per case, four CPUs, UID 1000, with only the AMD render node exposed.
KWin's virtual backend needs that render node for its OpenGL screenshot path; without it the compositor falls back and cancels stereo captures.
Set `STRIP_BINARY` to the extracted shipped Bino executable for the negative control.
The public recorder retains the real submitted strip and its viewport destination.
The tests compare every RGB channel in both views, with the previously retained tolerance of one level.
`--swap-expected` must fail on a successful stereo capture.
The containment test independently rejects a surface which extends beyond the ordinary top-level window; a correct native strip alone is insufficient.

The first twelve fixtures cover side by side, top and bottom, rows, columns and checkerboard in both orders, the 1920x804 H.264 SAR 2:1 StereoMode 1 case, and an unmarked mono control.
H.264 uses the lossless RGB encoder so source samples remain independently identifiable.
The original film is never opened or copied.
These fixtures do not yet cover all temporal, multiview, anaglyph and quincunx playback paths.
