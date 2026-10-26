#!/bin/bash
set -eu
export CASE_DIR=/work/native-input-evidence/$1
mkdir -p "$CASE_DIR/home" "$CASE_DIR/runtime"
chmod 700 "$CASE_DIR/runtime"
export HOME="$CASE_DIR/home" XDG_RUNTIME_DIR="$CASE_DIR/runtime" LANG=C.UTF-8
export XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_DATA_HOME="$HOME/.local/share"
export QT_QPA_PLATFORM=offscreen KWIN_COMPOSE=O2 LP_NUM_THREADS=4
exec dbus-run-session -- kwin_wayland --virtual --width 800 --height 600 --xwayland \
    --socket bino-native-input --no-global-shortcuts --exit-with-session /work/native-input-bino/tests/native-input/session.sh > "$CASE_DIR/kwin.log" 2>&1
