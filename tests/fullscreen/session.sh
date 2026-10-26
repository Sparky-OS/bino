#!/bin/bash
set -eu
script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
probes=${FULLSCREEN_PROBES:-/work/fullscreen-probes}
export QT_QPA_PLATFORM=${FULLSCREEN_PLATFORM:-wayland} QT_FFMPEG_DECODING_HW_DEVICE_TYPES=,
if [[ -n ${FULLSCREEN_BINARY:-} ]]; then
 WAYLAND_DEBUG=client LD_PRELOAD="$probes/libfullscreen-observer.so" "$FULLSCREEN_BINARY" --log-level debug -l one "${FULLSCREEN_CLIP:-/work/fullscreen-fixtures/white-sar.mkv}" > "$CASE_DIR/app.log" 2>&1 &
else
 WAYLAND_DEBUG=client "$probes/fullscreen-probe" "${FULLSCREEN_CLIP:-/work/fullscreen-fixtures/white-sar.mkv}" "$CASE_DIR" > "$CASE_DIR/app.log" 2>&1 &
fi
player=$!
trap 'kill "$player" 2>/dev/null || true' EXIT
python3 "$script_dir/drive.py" > "$CASE_DIR/driver.log" 2>&1
wait "$player"
