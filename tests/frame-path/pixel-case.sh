#!/bin/bash
set -eu
export CASE_DIR=/work/evidence/m2/$1
export PIXEL_INPUT=$2 PIXEL_TARGET=$3
export PROBE_BINARY=${PROBE_BINARY:-/work/probes/widget-probe}
test ! -e "$CASE_DIR"
mkdir -p "$CASE_DIR/home" "$CASE_DIR/runtime"
chmod 700 "$CASE_DIR/runtime"
export HOME="$CASE_DIR/home" XDG_RUNTIME_DIR="$CASE_DIR/runtime" LANG=C.UTF-8
export XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_DATA_HOME="$HOME/.local/share"
export QT_QPA_PLATFORM=offscreen KWIN_COMPOSE=O2
cd "$CASE_DIR"
exec dbus-run-session -- kwin_wayland --virtual --width 800 --height 600 --xwayland \
    --socket bino-pixels --no-lockscreen --no-global-shortcuts --no-kactivities \
    --exit-with-session /work/bino/tests/frame-path/pixel-session.sh > kwin.log 2>&1
