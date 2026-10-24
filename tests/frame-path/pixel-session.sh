#!/bin/bash
set -eu
export QT_QPA_PLATFORM=${PIXEL_PLATFORM:-xcb} QT_FFMPEG_DECODING_HW_DEVICE_TYPES=${PIXEL_HW_DEVICE:-,}
if [ -n "${PIXEL_SOFTWARE:-}" ]; then export LIBGL_ALWAYS_SOFTWARE=1; fi
export BINO_FRAME_TRACE="$CASE_DIR/trace.csv"
if [ -n "${PIXEL_TRACE_TRANSFER:-}" ]; then
    export LD_PRELOAD=/work/probes-on/libtransfer-trace.so
    export BINO_TRANSFER_TRACE="$CASE_DIR/transfers.csv"
fi
glxinfo -B > "$CASE_DIR/glxinfo.txt" 2>&1
if [ -n "${PIXEL_HOLD:-}" ]; then
    "$PROBE_BINARY" "${PIXEL_CLIP:-/work/clips/pixels/$PIXEL_INPUT.mkv}" "$CASE_DIR" "$PIXEL_INPUT" "$PIXEL_TARGET" > "$CASE_DIR/app.log" 2>&1 &
    probe_pid=$!
    trap 'kill "$probe_pid" 2>/dev/null || true' EXIT
    for attempt in $(seq 1 100); do
        test ! -e "$CASE_DIR/held" || break
        sleep 0.1
    done
    test -e "$CASE_DIR/held"
    if [ -n "${PIXEL_CHECK_X11:-}" ]; then
        python3 /work/bino/tests/frame-path/x11-declaration.py
    fi
    sleep 0.3
    QT_QPA_PLATFORM=offscreen /work/probes/screen-capture screen "$CASE_DIR/screen.png" > "$CASE_DIR/capture.log" 2>&1
    QT_QPA_PLATFORM=offscreen /work/probes/screen-capture window "$CASE_DIR/window.png" >> "$CASE_DIR/capture.log" 2>&1
    wait "$probe_pid"
    printf '0\n' > "$CASE_DIR/app-exit.txt"
    exit 0
fi
set +e
timeout 20 "$PROBE_BINARY" "${PIXEL_CLIP:-/work/clips/pixels/$PIXEL_INPUT.mkv}" "$CASE_DIR" "$PIXEL_INPUT" "$PIXEL_TARGET" > "$CASE_DIR/app.log" 2>&1
rc=$?
printf '%s\n' "$rc" > "$CASE_DIR/app-exit.txt"
exit "$rc"
