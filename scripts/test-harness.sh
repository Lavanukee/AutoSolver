#!/bin/bash
# Test harness for the Autosolver mod.
#
# MODES:
#
# (1) "platinum" (default): full unattended cycle. Kills any running GD,
#     launches a fresh copy, drives the menu via mouse clicks to load
#     Platinum Adventure, captures a screenshot every second during play,
#     extracts [TEL] telemetry lines, then quits GD. Click coordinates
#     are calibrated for a 1500x1000 fullscreen GD window (matches the
#     user's setup); adjust the COORD_* variables if your window differs.
#
# (2) "observe": GD must already be running and on the level you want to
#     test. Harness just captures screenshots + extracts [TEL] lines from
#     the current log — no launch, no quit, no menu navigation. Use when
#     you need a level the platinum-mode coords don't reach.
#
# Outputs to /tmp/autosolver-test-<timestamp>/:
#   screenshot.png    - final screen capture
#   burst/<n>-<HHMMSS>.png  - one screenshot per second through the play window
#   telemetry.log     - filtered [TEL] lines for the run
#   geode-full.log    - full Geode log file (full context, for grepping)
#
# Usage:
#   ./scripts/test-harness.sh                     # platinum mode, 30s play
#   ./scripts/test-harness.sh 45                  # platinum mode, 45s play
#   ./scripts/test-harness.sh observe             # observe mode, immediate snapshot
#   ./scripts/test-harness.sh observe 30          # observe mode, capture for 30s
#
# Calibrated click coordinates for the menu nav. Recorded against a
# 1500x1000 fullscreen GD window. The user navigates:
#   click (1088,467) → wait 2s → click (250,250) → wait 2s →
#   click (1150,300) → wait 2s → press space → in level.
COORD_1_X=1088; COORD_1_Y=467
COORD_2_X=250;  COORD_2_Y=250
COORD_3_X=1150; COORD_3_Y=300

set -u

MODE="platinum"
WAIT_SECONDS="${1:-30}"
if [ "${1:-}" = "observe" ]; then
    MODE="observe"
    WAIT_SECONDS="${2:-0}"
fi

GD_LOG_DIR="/Users/jedd/Library/Application Support/Steam/steamapps/common/Geometry Dash/Geometry Dash.app/Contents/geode/logs"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CLICK="$SCRIPT_DIR/click"
OUT_DIR="/tmp/autosolver-test-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT_DIR"

echo "[harness] mode=$MODE output=$OUT_DIR"

if [ "$MODE" = "platinum" ] && [ ! -x "$CLICK" ]; then
    echo "[harness] click binary missing at $CLICK"
    echo "[harness]   build it: swiftc -O $SCRIPT_DIR/click.swift -o $CLICK"
    exit 2
fi

# Graceful → forceful → wait-until-gone kill cycle. Steam refuses to relaunch
# while a stale GD process exists ("Game already running"), so we have to be
# sure nothing's left before we open it again.
kill_gd() {
    osascript -e 'tell application "Geometry Dash" to quit' >/dev/null 2>&1 || true
    sleep 3
    pkill -9 -f "Geometry Dash" 2>/dev/null || true
    sleep 2
    local tries=0
    while pgrep -qf "Geometry Dash" && [ $tries -lt 10 ]; do
        sleep 1
        tries=$((tries + 1))
    done
    if pgrep -qf "Geometry Dash"; then
        echo "[harness] WARNING: GD still detected after kill attempts, launch may fail"
    fi
}

do_click() {
    local x=$1 y=$2
    echo "[harness]   click ($x, $y)"
    "$CLICK" "$x" "$y"
}

if [ "$MODE" = "platinum" ]; then
    # Always close GD first so we get a clean session + a fresh log file.
    # 2s post-kill buffer lets Steam and the audio subsystem fully release
    # the process before we relaunch.
    if pgrep -qf "Geometry Dash"; then
        echo "[harness] existing GD process detected, killing for fresh session"
        kill_gd
    fi
    sleep 2

    # Snapshot existing log files so we can detect the new one for this run.
    BEFORE_LOGS=$(ls -1 "$GD_LOG_DIR" 2>/dev/null | sort -u)

    echo "[harness] launching Geometry Dash..."
    open -a "Geometry Dash"
    echo "[harness] waiting 14s for GD to load to main menu..."
    sleep 14

    AFTER_LOGS=$(ls -1 "$GD_LOG_DIR" 2>/dev/null | sort -u)
    NEW_LOG=$(comm -13 <(echo "$BEFORE_LOGS") <(echo "$AFTER_LOGS") | head -1)
    if [ -n "$NEW_LOG" ]; then
        GEODE_LOG="$GD_LOG_DIR/$NEW_LOG"
        echo "[harness] tracking new log: $NEW_LOG"
    else
        GEODE_LOG=$(ls -t "$GD_LOG_DIR"/*.log 2>/dev/null | head -1)
        echo "[harness] no new log detected; falling back to: $(basename "$GEODE_LOG")"
    fi

    echo "[harness] activating GD..."
    osascript -e 'tell application "Geometry Dash" to activate' >/dev/null
    sleep 2
    # Force fullscreen via macOS Cmd+Ctrl+F. Steam relaunches of GD
    # sometimes start in windowed mode, which makes the user-calibrated
    # 1500x1000 click coordinates miss. Toggling fullscreen first
    # guarantees the click coordinates land inside the game window.
    # Key code 3 = F.
    echo "[harness] toggling fullscreen (Cmd+Ctrl+F)..."
    osascript -e 'tell application "System Events" to key code 3 using {command down, control down}' >/dev/null
    sleep 3  # fullscreen transition animation
    echo "[harness] running 3-click navigation..."
    do_click "$COORD_1_X" "$COORD_1_Y"
    sleep 2
    do_click "$COORD_2_X" "$COORD_2_Y"
    sleep 2
    do_click "$COORD_3_X" "$COORD_3_Y"
    sleep 2
    echo "[harness]   press Space"
    osascript -e 'tell application "System Events" to key code 49' >/dev/null
    # Extra settling time after Space — the level needs to load before we
    # start capturing, otherwise the first ~2-3 screenshots are the loading
    # transition (not actual gameplay).
    sleep 3
else
    # Observe mode — assume GD is already running and on the right level.
    if ! pgrep -qf "Geometry Dash"; then
        echo "[harness] ERROR: observe mode but no GD process found. Launch GD first."
        exit 1
    fi
    GEODE_LOG=$(ls -t "$GD_LOG_DIR"/*.log 2>/dev/null | head -1)
    echo "[harness] observing existing session, log: $(basename "$GEODE_LOG")"
fi

# Capture a SERIES of screenshots throughout the play window, so we have the
# pre-death moments preserved no matter when death happens during the wait.
# Filenames embed HH:MM:SS so we can correlate with [TEL] real_death events
# from the same wall clock. ~1 capture/sec (screencapture overhead is ~150ms
# on retina; faster than this stutters the game).
SHOT_DIR="$OUT_DIR/burst"
mkdir -p "$SHOT_DIR"
echo "[harness] capturing ~1 screenshot/sec for $WAIT_SECONDS s..."
for i in $(seq 1 "$WAIT_SECONDS"); do
    SHOT_TS=$(date +%H%M%S)
    screencapture -x "$SHOT_DIR/$(printf '%03d' $i)-$SHOT_TS.png"
    sleep 0.85
done

echo "[harness] capturing final screenshot..."
screencapture -x "$OUT_DIR/screenshot.png"

if [ -f "$GEODE_LOG" ]; then
    cp "$GEODE_LOG" "$OUT_DIR/geode-full.log"
    grep "\[TEL\]" "$GEODE_LOG" > "$OUT_DIR/telemetry.log" || true
    TEL_COUNT=$(wc -l < "$OUT_DIR/telemetry.log" | tr -d ' ')
    echo "[harness] telemetry lines captured: $TEL_COUNT"
else
    echo "[harness] WARNING: no Geode log file found at $GEODE_LOG"
fi

if [ "$MODE" = "platinum" ]; then
    echo "[harness] post-run buffer (2s) before quit..."
    sleep 2
    echo "[harness] quitting GD..."
    kill_gd
fi

echo "[harness] done. results in $OUT_DIR"
ls -la "$OUT_DIR"
