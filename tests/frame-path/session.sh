#!/bin/bash
set -eu
export QT_QPA_PLATFORM=xcb
python3 -c 'import time; print(time.monotonic_ns())' > "$CASE_DIR/start-ns.txt"
export QT_FFMPEG_DECODING_HW_DEVICE_TYPES="$DECODE"
export QT_LOGGING_RULES='qt.multimedia.ffmpeg.hwaccel=true;qt.multimedia.ffmpeg.videodecoder=true;qt.multimedia.ffmpeg.*=true'
glxinfo -B > "$CASE_DIR/glxinfo.txt" 2>&1
export BINO_FRAME_TRACE="$CASE_DIR/trace.csv"
export BINO_TRANSFER_TRACE="$CASE_DIR/transfers.csv"
export LD_PRELOAD=/work/probes/libtransfer-trace.so
set +e
timeout --signal=TERM 16 /work/build/bino --log-level debug --log-file "$CASE_DIR/bino.log" -i left-right-half -s off -f /work/clips/test-hsbs.mkv > "$CASE_DIR/app.log" 2>&1
rc=$?
python3 -c 'import time; print(time.monotonic_ns())' > "$CASE_DIR/end-ns.txt"
printf '%s\n' "$rc" > "$CASE_DIR/app-exit.txt"
exit 0
