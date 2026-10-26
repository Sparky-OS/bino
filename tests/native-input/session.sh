#!/bin/bash
set -eu
export QT_QPA_PLATFORM=wayland QT_FFMPEG_DECODING_HW_DEVICE_TYPES=,
export BINO_FRAME_TRACE="$CASE_DIR/trace.csv"
mkdir -p "$CASE_DIR/buffers"
printf 'wait 3\nquit\n' > "$CASE_DIR/control.txt"
args=()
if [ -n "${STRIP_INPUT:-}" ]; then args+=(-i "$STRIP_INPUT"); fi
WLREC_DIR="$CASE_DIR/buffers" LD_PRELOAD=/work/pact-tools/wlrec.so WAYLAND_DEBUG=client \
    "${STRIP_BINARY:-/usr/bin/bino}" --log-level debug --control-file "$CASE_DIR/control.txt" \
    -l one "${args[@]}" "$STRIP_CLIP" > "$CASE_DIR/app.log" 2>&1
