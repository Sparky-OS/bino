#!/bin/bash
set -eu
export QT_QPA_PLATFORM=xcb QT_FFMPEG_DECODING_HW_DEVICE_TYPES=,
export BINO_FRAME_TRACE="$CASE_DIR/trace.csv"
glxinfo -B > "$CASE_DIR/glxinfo.txt" 2>&1
set +e
timeout 20 "$PROBE_BINARY" "${PIXEL_CLIP:-/work/clips/pixels/$PIXEL_INPUT.mkv}" "$CASE_DIR" "$PIXEL_INPUT" "$PIXEL_TARGET" > "$CASE_DIR/app.log" 2>&1
rc=$?
printf '%s\n' "$rc" > "$CASE_DIR/app-exit.txt"
exit "$rc"
