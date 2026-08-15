#!/usr/bin/env bash
# tools/boot_state.sh -- one-shot boot-state probe for the standalone exe.
#
# WHY: a modal Win32 dialog (usually the launcher, armed when the previous run did not
# exit cleanly) leaves the process ALIVE and pumping messages while standalone_error.log
# stops mid-boot. To a log tail that is indistinguishable from a slow load, so an agent
# polling for `[HuntScan]` sits there until its timeout expires. Call this instead of
# assuming a quiet log means "still working".
#
# USAGE
#   tools/boot_state.sh              # print one status line
#   tools/boot_state.sh --wait 300   # block until RUNNING, or fail fast if blocked
#
# EXIT CODES (so a script can branch without parsing)
#   0  RUNNING         normal operation
#   1  NOT_RUNNING     no process, and no state file
#   2  BLOCKED_DIALOG  modal dialog open -- boot will NEVER proceed unattended
#   3  BOOTING         started, Engine::Startup has not returned yet
#   4  STALE           state file exists but its pid is gone (previous run's leftovers)
#
# The state file is written every second by BootState::Start (standalone/
# boot_state_watchdog.cpp). Its pid field is what makes a leftover file detectable.

set -u

GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
STATE_FILE="$GAME_DIR/standalone_boot_state.txt"

WAIT_SECS=0
if [ "${1:-}" = "--wait" ]; then
    WAIT_SECS="${2:-300}"
fi

# Is a reimpl-x64.exe alive at all? tasklist is authoritative here; the state file is not,
# because a killed process leaves its last line behind.
proc_alive() {
    tasklist 2>/dev/null | grep -qi 'reimpl-x64\.exe'
}

probe_once() {
    if [ ! -f "$STATE_FILE" ]; then
        if proc_alive; then
            # Running but nothing published yet: either a pre-watchdog build or the
            # first second of boot. Report BOOTING rather than inventing a failure.
            echo "BOOTING  (no state file yet -- process alive, watchdog not up)"
            return 3
        fi
        echo "NOT_RUNNING  (no process, no state file at $STATE_FILE)"
        return 1
    fi

    local line state pid detail
    line="$(cat "$STATE_FILE" 2>/dev/null)"
    state="$(printf '%s' "$line" | sed -n 's/.*STATE=\([A-Z_]*\).*/\1/p')"
    pid="$(printf '%s' "$line" | sed -n 's/.*pid=\([0-9]*\).*/\1/p')"
    detail="$(printf '%s' "$line" | sed -n 's/.*detail=\(.*\)$/\1/p')"

    # A file whose pid is gone describes a previous run. Never report its state as live.
    if [ -n "$pid" ] && ! tasklist 2>/dev/null | grep -q "[^0-9]$pid[^0-9]"; then
        echo "STALE  (state file says $state from pid $pid, which is no longer running)"
        return 4
    fi

    case "$state" in
        RUNNING)        echo "RUNNING  ($line)"; return 0 ;;
        BLOCKED_DIALOG) echo "BLOCKED_DIALOG  a modal dialog is open and boot cannot proceed."
                        echo "                title: $detail"
                        echo "                Click it, or relaunch -- an unattended run will never clear it."
                        return 2 ;;
        BOOTING)        echo "BOOTING  ($line)"; return 3 ;;
        EXITED)         echo "NOT_RUNNING  (clean shutdown: $line)"; return 1 ;;
        *)              echo "NOT_RUNNING  (unrecognised state file: $line)"; return 1 ;;
    esac
}

if [ "$WAIT_SECS" -le 0 ]; then
    probe_once
    exit $?
fi

# --wait: poll until RUNNING, but bail immediately on BLOCKED_DIALOG. Waiting out a modal
# dialog is exactly the wasted timeout this script exists to prevent.
deadline=$(( $(date +%s) + WAIT_SECS ))
while :; do
    out="$(probe_once)"; rc=$?
    case $rc in
        0) echo "$out"; exit 0 ;;
        2) echo "$out"; exit 2 ;;
        4) echo "$out"; exit 4 ;;
    esac
    if [ "$(date +%s)" -ge "$deadline" ]; then
        echo "TIMEOUT after ${WAIT_SECS}s -- last: $out"
        exit 3
    fi
    sleep 3
done
