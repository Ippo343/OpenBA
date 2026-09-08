#!/usr/bin/env bash
# =============================================================================
# tools/game_dir_lock.sh -- a mutex over the ONE shared game directory
# =============================================================================
#
# WHY THIS EXISTS
#
# Lanes work in isolated git worktrees, so their SOURCE never collides. But every
# lane deploys the same `reimpl-x64.exe` into `C:/GOG Games/Banished` and every
# lane's run writes the same `standalone_{debug,error,crash}.log` there. The
# worktree isolation makes that easy to forget, because everything else about a
# lane is private.
#
# During Wave 9's eight-lane session this cost real work:
#
#   * three consecutive deploy+run attempts by one lane were clobbered by another
#     lane overwriting the exe in the seconds between `cp` and launch,
#   * a `cp` failed outright with "Device or resource busy" because another lane
#     had the exe running,
#   * one lane's first measurement was silently taken against a DIFFERENT lane's
#     binary and had to be discovered and redone.
#
# The last one is the dangerous class. A clobbered run does not announce itself:
# you get a plausible log, from a plausible exe, that is measuring somebody
# else's code. That is the same shape as every other defect this wave has paid
# for -- an instrument reporting a confident number about the wrong thing.
#
# WHAT IT DOES NOT DO
#
# This is advisory. It cannot stop a lane that does not call it, and it does not
# protect the game data files (`WinData/`, the `.pkm` set). A lane that mounts or
# unmounts mods changes the INPUT of every other lane's run, and no lock over the
# exe will catch that -- announce those changes to the integrator instead.
#
# USAGE
#
#   tools/game_dir_lock.sh acquire <lane-name> [timeout-seconds]   # default 900
#   tools/game_dir_lock.sh release <lane-name>
#   tools/game_dir_lock.sh status
#
#   # the whole point -- deploy and run under ONE lock, so nothing can land
#   # between the copy and the launch:
#   tools/game_dir_lock.sh acquire lane-FOO 900 || exit 1
#   trap 'tools/game_dir_lock.sh release lane-FOO' EXIT
#   cp standalone/build/reimpl-x64.exe "$GAME/reimpl-x64.exe"
#   ( cd "$GAME" && BANISHED_AUTOSTART=1 ./reimpl-x64.exe )
#   cp "$GAME"/standalone_*.log ./my-evidence/     # copy out BEFORE releasing
#
# Releasing before copying the logs out reintroduces the race you just avoided.
# =============================================================================

set -u

GAME="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
LOCK="$GAME/.lane_lock"          # a DIRECTORY: mkdir is atomic on every fs here
STALE_AFTER=1800                 # 30 min; a lane run that long is already wrong

usage() { sed -n '/^# USAGE/,/^# ===/p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

now() { date +%s; }

lock_owner()  { cat "$LOCK/owner" 2>/dev/null || echo "(unknown)"; }
lock_started(){ cat "$LOCK/started" 2>/dev/null || echo 0; }

# A lock older than STALE_AFTER is assumed to belong to a lane that died without
# releasing. Say so LOUDLY when breaking one -- a silently stolen lock would
# recreate the exact failure this script exists to prevent.
break_if_stale() {
    local started age
    started=$(lock_started)
    [ "$started" -eq 0 ] 2>/dev/null && started=$(now)
    age=$(( $(now) - started ))
    if [ "$age" -gt "$STALE_AFTER" ]; then
        echo "[gamelock] WARNING: breaking a STALE lock held by '$(lock_owner)'" >&2
        echo "[gamelock]   held for ${age}s (limit ${STALE_AFTER}s). If that lane is" >&2
        echo "[gamelock]   still alive, BOTH runs are now unreliable -- discard them." >&2
        rm -rf "$LOCK"
        return 0
    fi
    return 1
}

# Is a run IN PROGRESS right now? Lane UIW established the reliable test, after
# three clobbered runs and one COMPLETE, PLAUSIBLE, WRONG RtDiff summary produced
# from another lane's binary:
#
#   `tasklist`-then-launch is NOT sufficient. It is a check followed by an
#   action with a gap in between, and another lane starts in the gap. That is a
#   TOCTOU race, and it failed three times in one session.
#
# The dependable test is whether the shared logs can be DELETED. Windows keeps
# them open for the life of the process, so a failed delete means a live run --
# and unlike tasklist it cannot go stale between the check and the answer,
# because it IS the action.
run_in_progress() {
    local f
    for f in standalone_debug.log standalone_error.log; do
        [ -f "$GAME/$f" ] || continue
        # Try to move it aside and back: same open-handle test as a delete, but
        # non-destructive if it succeeds.
        if mv "$GAME/$f" "$GAME/$f.locktest" 2>/dev/null; then
            mv "$GAME/$f.locktest" "$GAME/$f" 2>/dev/null
        else
            return 0        # held open => a run is live
        fi
    done
    return 1
}

cmd_acquire() {
    local who="${1:?lane name required}" timeout="${2:-900}" waited=0
    mkdir -p "$GAME" 2>/dev/null
    while :; do
        # Take the lock only if nobody is mid-run. A lane that crashed without
        # releasing leaves no open handle, so this does not deadlock on it.
        if ! run_in_progress && mkdir "$LOCK" 2>/dev/null; then
            printf '%s' "$who"     > "$LOCK/owner"
            printf '%s' "$(now)"   > "$LOCK/started"
            echo "[gamelock] ACQUIRED by $who"
            return 0
        fi
        if [ "$(lock_owner)" = "$who" ]; then
            echo "[gamelock] already held by $who (re-entrant, not re-acquired)"
            return 0
        fi
        break_if_stale && continue
        if [ "$waited" -ge "$timeout" ]; then
            echo "[gamelock] TIMEOUT after ${timeout}s; '$(lock_owner)' still holds it." >&2
            echo "[gamelock] Do NOT deploy anyway -- that is the clobber this prevents." >&2
            return 1
        fi
        [ $(( waited % 30 )) -eq 0 ] &&
            echo "[gamelock] waiting for '$(lock_owner)' (${waited}s/${timeout}s)..." >&2
        sleep 5; waited=$(( waited + 5 ))
    done
}

cmd_release() {
    local who="${1:?lane name required}"
    if [ ! -d "$LOCK" ]; then
        echo "[gamelock] not held; nothing to release"
        return 0
    fi
    local owner; owner=$(lock_owner)
    if [ "$owner" != "$who" ]; then
        # Refuse rather than steal. Releasing someone else's lock would let two
        # lanes into the directory at once believing they were serialized.
        echo "[gamelock] REFUSING: lock is held by '$owner', not '$who'." >&2
        return 1
    fi
    rm -rf "$LOCK"
    echo "[gamelock] released by $who"
}

cmd_status() {
    if [ -d "$LOCK" ]; then
        echo "[gamelock] HELD by '$(lock_owner)' for $(( $(now) - $(lock_started) ))s"
    else
        echo "[gamelock] free"
    fi
    if [ -f "$GAME/reimpl-x64.exe" ]; then
        echo "[gamelock] deployed exe: $(md5sum "$GAME/reimpl-x64.exe" | cut -c1-32)  $(date -r "$GAME/reimpl-x64.exe" '+%Y-%m-%d %H:%M:%S')"
        echo "[gamelock]   compare against your own build before believing any run."
    fi
    local n
    n=$(ls "$GAME"/WinData/*.pkm 2>/dev/null | wc -l)
    echo "[gamelock] mounted .pkm mods: $n  (changes the INPUT of every run; not covered by this lock)"
}

case "${1:-}" in
    acquire) shift; cmd_acquire "$@" ;;
    release) shift; cmd_release "$@" ;;
    status)  cmd_status ;;
    *)       usage ;;
esac
