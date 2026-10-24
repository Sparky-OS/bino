#!/bin/bash
set -eu
export CASE_DIR=/work/evidence/$1
test ! -e "$CASE_DIR"
mkdir -p "$CASE_DIR/home" "$CASE_DIR/runtime"
chmod 700 "$CASE_DIR/runtime"
export HOME="$CASE_DIR/home" XDG_RUNTIME_DIR="$CASE_DIR/runtime" LANG=C.UTF-8
export XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_DATA_HOME="$HOME/.local/share"
export DECODE="$2"
export QT_QPA_PLATFORM=offscreen
export KWIN_COMPOSE=O2 KWIN_LOG_PERFORMANCE_DATA=1
cd "$CASE_DIR"
exec dbus-run-session -- kwin_wayland --virtual --width 1920 --height 1080 --xwayland --socket bino-bench --no-lockscreen --no-global-shortcuts --no-kactivities --exit-with-session /work/session.sh > "$CASE_DIR/kwin.log" 2>&1
