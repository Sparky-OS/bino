#!/bin/bash
set -eu
export QT_QPA_PLATFORM=wayland QT_FFMPEG_DECODING_HW_DEVICE_TYPES=,
export BINO_FRAME_TRACE="$CASE_DIR/trace.csv"
mkdir -p "$CASE_DIR/buffers"
printf 'wait 5\npause\nwait 10\nquit\n' > "$CASE_DIR/control.txt"
args=()
if [ -n "${STRIP_FULLSCREEN:-}" ]; then args+=(-f); fi
if [ -n "${STRIP_INPUT:-}" ]; then args+=(-i "$STRIP_INPUT"); fi
player_path=$PATH
if [ -n "${STRIP_NO_FFPROBE:-}" ]; then player_path=/work/pact-tools/no-ffprobe; fi
PATH="$player_path" WLREC_DIR="$CASE_DIR/buffers" LD_PRELOAD=/work/pact-tools/wlrec.so WAYLAND_DEBUG=client \
    "${STRIP_BINARY:-/work/package-prefix/usr/bin/bino}" --log-level debug \
    --control-file "$CASE_DIR/control.txt" -l one "${args[@]}" "$STRIP_CLIP" > "$CASE_DIR/app.log" 2>&1 &
player_pid=$!
trap 'kill "$player_pid" 2>/dev/null || true' EXIT
for attempt in $(seq 1 150); do
    if test -s "$CASE_DIR/buffers/log.jsonl"; then break; fi
    if [ -n "${STRIP_MONO:-}" ] && grep -Eq '^sink,[0-9]+,[0-9]+,[0-9]+,1,' "$CASE_DIR/trace.csv" 2>/dev/null; then break; fi
    kill -0 "$player_pid"
    sleep 0.1
done
sleep 1
QT_QPA_PLATFORM=offscreen /work/probes/screen-capture screen "$CASE_DIR/screen.png" > "$CASE_DIR/capture.log" 2>&1
QT_QPA_PLATFORM=offscreen /work/probes/screen-capture window "$CASE_DIR/window.png" >> "$CASE_DIR/capture.log" 2>&1
wait "$player_pid"
