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
#   4  STALE           the state file does not describe a live, publishing process.
#                     TWO ways to earn it:
#                       (a) its pid is GONE          -- a previous run's leftovers
#                       (b) its pid is ALIVE but the FILE HAS STOPPED ADVANCING
#                           -- the watchdog thread is dead while the process lingers
#
# (b) IS THE ONE THIS SCRIPT USED TO MISS, AND IT COST A 24-MINUTE OUTAGE.
# The old script checked pid liveness and NEVER file freshness, so a crashed run
# that had not exited -- still holding the exe and both logs -- read as RUNNING
# and a lane reported BOOTSTATE_RC=0 for a process that was already dead.
# (WAVE20_GUIDE.md 0b. Note the corrected attribution there: this tool was a
# CONTRIBUTING cause, not the proximate one. A correct answer here would have
# shortened that outage; it would NOT have released the lock or reaped the
# orphan, which is what actually prolonged it.)
#
# The state file is rewritten on a fixed interval by BootState::Start
# (standalone/boot_state_watchdog.cpp), so its MTIME is a heartbeat. The
# staleness threshold is DERIVED from that publisher's own Sleep() below rather
# than picked, and is printed beside every verdict -- a hard-coded number here
# would go stale the moment the watchdog's interval changed, with nothing to
# notice (reference_a_check_is_unaimable_only_without_a_free_parameter.md).

set -u

GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
STATE_FILE="$GAME_DIR/standalone_boot_state.txt"

# ---------------------------------------------------------------------------
# DERIVE the publish interval from the publisher, do not restate it.
#
# BootState::WatchdogThread ends its loop with `Sleep(<ms>)`. Read that number
# out of the source so this check tracks the watchdog automatically. If the
# source is not next to us (this script copied elsewhere), fall back and SAY SO
# -- an undeclared fallback is how a derived number quietly becomes a guess.
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WATCHDOG_SRC="$SCRIPT_DIR/../standalone/boot_state_watchdog.cpp"

PUBLISH_MS=""
INTERVAL_SRC=""
if [ -f "$WATCHDOG_SRC" ]; then
    PUBLISH_MS="$(sed -n 's/^ *Sleep(\([0-9][0-9]*\));.*/\1/p' "$WATCHDOG_SRC" | head -1)"
fi
if [ -n "$PUBLISH_MS" ]; then
    INTERVAL_SRC="derived from $(basename "$WATCHDOG_SRC") Sleep(${PUBLISH_MS})"
else
    PUBLISH_MS=1000
    INTERVAL_SRC="FALLBACK 1000ms -- could NOT read Sleep() from $WATCHDOG_SRC"
fi
PUBLISH_SECS=$(( (PUBLISH_MS + 999) / 1000 ))
[ "$PUBLISH_SECS" -lt 1 ] && PUBLISH_SECS=1

# How many missed heartbeats before we call it dead. A publish is Sleep(interval)
# PLUS an EnumWindows sweep and a file rewrite, and this machine runs several
# lanes and a game at once, so a couple of late ticks are normal and must not
# read as a crash. Ten consecutive misses is not lateness.
STALE_FACTOR="${BANISHED_BOOTSTATE_STALE_FACTOR:-10}"
STALE_SECS=$(( PUBLISH_SECS * STALE_FACTOR ))

# Age of the state file in seconds, or "" if it cannot be determined.
file_age() {
    local mtime now
    mtime="$(stat -c %Y "$STATE_FILE" 2>/dev/null)" || return 1
    [ -n "$mtime" ] || return 1
    now="$(date +%s)"
    # Clock skew (or a file stamped in the future) must never manufacture a
    # STALE. Clamp at 0 and let it read fresh.
    if [ "$mtime" -gt "$now" ]; then echo 0; else echo $(( now - mtime )); fi
}

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

    local line state pid detail age fresh
    line="$(cat "$STATE_FILE" 2>/dev/null)"
    state="$(printf '%s' "$line" | sed -n 's/.*STATE=\([A-Z_]*\).*/\1/p')"
    pid="$(printf '%s' "$line" | sed -n 's/.*pid=\([0-9]*\).*/\1/p')"
    detail="$(printf '%s' "$line" | sed -n 's/.*detail=\(.*\)$/\1/p')"

    # A file whose pid is gone describes a previous run. Never report its state as live.
    if [ -n "$pid" ] && ! tasklist 2>/dev/null | grep -q "[^0-9]$pid[^0-9]"; then
        echo "STALE  (state file says $state from pid $pid, which is no longer running)"
        return 4
    fi

    # ---- FRESHNESS -- the check this script was missing -------------------
    # APPLICABLE ONLY to the three states a LIVE watchdog republishes every
    # tick. EXITED is written once by Stop() and is SUPPOSED to sit frozen, so
    # ageing it would turn every clean shutdown into a false STALE.
    #
    # MEASURED WHILE WRITING THIS, and it makes the freshness check MORE
    # load-bearing rather than less: `BootState::Stop()` HAS NO CALLER anywhere
    # in the tree (only `Start()` at main.cpp:1639 and `MarkStartupComplete()`
    # at game_main.cpp:478). So STATE=EXITED is never actually written, and
    # EVERY run -- including a clean self-quit, verified twice today -- leaves
    # STATE=RUNNING behind with a dead pid. The pid check catches that today;
    # freshness catches it even if the pid were recycled. The EXITED arm below
    # is kept because it is the correct rule for the state the header
    # documents, but nothing in the shipped exe currently produces it.
    case "$state" in
        BOOTING|RUNNING|BLOCKED_DIALOG)
            age="$(file_age)"
            if [ -n "$age" ] && [ "$age" -gt "$STALE_SECS" ]; then
                echo "STALE  (file says $state from pid $pid, and that pid is still alive,"
                echo "        but the file has not advanced for ${age}s > ${STALE_SECS}s"
                echo "        [freshness: publish interval ${PUBLISH_SECS}s x factor ${STALE_FACTOR}; ${INTERVAL_SRC}])"
                echo "        The watchdog thread is dead while the process lingers -- typically a"
                echo "        crashed run that never exited. It is STILL HOLDING the exe and both"
                echo "        logs. Read standalone_crash.log, then reap the orphan; do not wait."
                return 4
            fi
            ;;
    esac

    # Print the freshness that was actually applied beside the verdict, so a
    # green is quotable with the thing that made it green.
    fresh="age=${age:-?}s/${STALE_SECS}s"

    case "$state" in
        RUNNING)        echo "RUNNING  [$fresh] ($line)"; return 0 ;;
        BLOCKED_DIALOG) echo "BLOCKED_DIALOG  a modal dialog is open and boot cannot proceed."
                        echo "                title: $detail"
                        echo "                Click it, or relaunch -- an unattended run will never clear it."
                        return 2 ;;
        BOOTING)        echo "BOOTING  [$fresh] ($line)"; return 3 ;;
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
