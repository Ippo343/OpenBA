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
#
# PREFER, and the reason it exists:
#
#   tools/game_dir_lock.sh run lane-FOO 900 -- make sa-run
#
# DEFEATED BY A PIPE -- this cost a real clobber on 2026-09-08. `acquire`
# returns 1 on timeout correctly, but in a pipeline the exit status is the LAST
# command's:
#
#   tools/game_dir_lock.sh acquire lane-FOO 540 2>&1 | tail -3   # <-- ALWAYS 0
#
# The lane read "exit code 0", deployed over another lane's exe mid-run, and
# rewrote its logs -- while the piped text it did not read said TIMEOUT in plain
# English. NEVER pipe `acquire` into head/tail/grep. Use `run` above, or call
# `acquire` bare, or `set -o pipefail`, or:
#
#   if ! tools/game_dir_lock.sh acquire lane-FOO 900; then exit 1; fi
#
# A granted acquire now also prints a line matching `^\[gamelock\] GRANT `, so a
# caller that must pipe can test for the POSITIVE rather than trusting a status
# that a pipe can swallow. And `verify <lane>` re-checks ownership immediately
# before launch.
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

# ---- FIFO ticketing ---------------------------------------------------------
#
# `acquire` used to be a poll-and-grab spin, which is NOT a queue: whoever
# happened to poll in the window after a release won, and a lane that had been
# waiting 29 minutes had no priority over one that arrived a second ago. Lane
# UI3 lost a slot that way on 2026-09-08 after waiting an hour. More timeout
# does not fix that -- it is unfairness, not impatience.
#
# So a waiter takes a TICKET (its lane name, containing the epoch it first
# asked) and may only attempt the grab when it is at the head. A waiter TOUCHES
# its ticket every poll, so a lane that dies while queued stops refreshing and
# its ticket is ignored after TICKET_STALE seconds -- otherwise one dead waiter
# would deadlock every other lane, which is worse than the unfairness it fixes.
#
# Lanes running an older copy of this script take no ticket and still grab as
# before. That is the pre-existing behaviour, not a regression.
QUEUE="$GAME/.gamedir_queue"
TICKET_STALE=60

ticket_take() {
    local who="$1"
    mkdir -p "$QUEUE" 2>/dev/null
    [ -f "$QUEUE/$who" ] || printf '%s' "$(now)" > "$QUEUE/$who"
}
ticket_touch() { touch "$QUEUE/$1" 2>/dev/null; }
ticket_drop()  { rm -f "$QUEUE/$1" 2>/dev/null; }

# Echo the lane name at the head of the queue: the LIVE ticket (refreshed within
# TICKET_STALE) with the earliest first-asked epoch. Empty if nobody is queued.
ticket_head() {
    local best="" best_at="" f name asked age
    [ -d "$QUEUE" ] || return 0
    for f in "$QUEUE"/*; do
        [ -f "$f" ] || continue
        name=$(basename "$f")
        age=$(( $(now) - $(date -r "$f" +%s 2>/dev/null || echo 0) ))
        [ "$age" -gt "$TICKET_STALE" ] && continue      # waiter died or left
        asked=$(cat "$f" 2>/dev/null || echo 0)
        if [ -z "$best_at" ] || [ "$asked" -lt "$best_at" ]; then
            best_at=$asked; best=$name
        fi
    done
    printf '%s' "$best"
}

cmd_acquire() {
    local who="${1:?lane name required}" timeout="${2:-900}" waited=0 head
    mkdir -p "$GAME" 2>/dev/null
    ticket_take "$who"
    while :; do
        ticket_touch "$who"
        # Wait your turn. An empty head means nobody else is queued.
        head=$(ticket_head)
        if [ -n "$head" ] && [ "$head" != "$who" ]; then
            if [ "$waited" -ge "$timeout" ]; then
                echo "[gamelock] DENIED -- TIMEOUT after ${timeout}s; queued behind '$head'."
                echo "[gamelock] TIMEOUT after ${timeout}s; queued behind '$head'." >&2
                ticket_drop "$who"
                return 1
            fi
            [ $(( waited % 30 )) -eq 0 ] &&
                echo "[gamelock] queued behind '$head' (${waited}s/${timeout}s)..." >&2
            sleep 5; waited=$(( waited + 5 ))
            continue
        fi
        # Take the lock only if nobody is mid-run. A lane that crashed without
        # releasing leaves no open handle, so this does not deadlock on it.
        if ! run_in_progress && mkdir "$LOCK" 2>/dev/null; then
            printf '%s' "$who"     > "$LOCK/owner"
            printf '%s' "$(now)"   > "$LOCK/started"
            ticket_drop "$who"
            echo "[gamelock] ACQUIRED by $who"
            echo "[gamelock] GRANT $who"
            return 0
        fi
        if [ "$(lock_owner)" = "$who" ]; then
            ticket_drop "$who"
            echo "[gamelock] already held by $who (re-entrant, not re-acquired)"
            return 0
        fi
        break_if_stale && continue
        if [ "$waited" -ge "$timeout" ]; then
            # Printed on BOTH streams on purpose. A caller that pipes this
            # command loses the exit status (see the DEFEATED BY A PIPE note in
            # the usage block), so the refusal has to be visible on stdout too.
            echo "[gamelock] DENIED -- TIMEOUT after ${timeout}s; '$(lock_owner)' still holds it."
            echo "[gamelock] Do NOT deploy anyway -- that is the clobber this prevents."
            echo "[gamelock] TIMEOUT after ${timeout}s; '$(lock_owner)' still holds it." >&2
            echo "[gamelock] Do NOT deploy anyway -- that is the clobber this prevents." >&2
            ticket_drop "$who"
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

    # ---- Idle advisory -------------------------------------------------------
    #
    # The third variant of the clobber class, raised by lane UI3 on 2026-09-08:
    # `verify` catches "I never got it" and the stdout refusal catches "I misread
    # the grant", but neither catches "a lane holds it and is no longer using
    # it". A queued lane then stares at a 30-minute hold with no way to tell a
    # dead holder from one legitimately analysing between launches -- and
    # guessing wrong IS the clobber this script exists to prevent.
    #
    # So this prints FACTS and refuses to draw the conclusion. It deliberately
    # does NOT auto-break: holding the lock across several launches with analysis
    # gaps between them is correct usage and looks identical from outside.
    [ -d "$LOCK" ] || return 0
    local proc idle newest
    proc=$(tasklist 2>/dev/null | grep -c -i 'reimpl-x64\.exe')
    newest=0
    for f in standalone_debug.log standalone_error.log standalone_crash.log reimpl-x64.exe; do
        [ -f "$GAME/$f" ] || continue
        local m; m=$(date -r "$GAME/$f" +%s 2>/dev/null) || continue
        [ "$m" -gt "$newest" ] && newest=$m
    done
    if [ "$newest" -gt 0 ]; then
        idle=$(( $(now) - newest ))
        echo "[gamelock] holder activity: reimpl-x64.exe processes=$proc, last game-dir write ${idle}s ago"
        if [ "$proc" -eq 0 ] && [ "$idle" -ge 300 ]; then
            echo "[gamelock] ADVISORY: no running exe and no write for ${idle}s. That is CONSISTENT WITH"
            echo "[gamelock]   (a) the holder analysing between launches -- correct usage, do not disturb -- or"
            echo "[gamelock]   (b) a holder that has finished or died. These look IDENTICAL from here."
            echo "[gamelock]   Do NOT break the lock on this line. ASK the holder ('$(lock_owner)'), and"
            echo "[gamelock]   escalate to a human with these numbers rather than with a guess."
        fi
    fi
}

# The safe path, made the easy one. Acquire, run, release -- with the acquire's
# exit status checked HERE, where no caller's pipe can discard it. Anything the
# caller wants to pipe is this command's output, not the acquire's.
#
#   tools/game_dir_lock.sh run lane-FOO 900 -- make sa-run
#
cmd_run() {
    local who="${1:?lane name required}" timeout="${2:-900}"
    shift 2
    [ "${1:-}" = "--" ] && shift
    [ $# -gt 0 ] || { echo "[gamelock] run: no command given" >&2; return 2; }

    if ! cmd_acquire "$who" "$timeout"; then
        echo "[gamelock] run: ABORTING, the lock was never granted to '$who'." >&2
        return 1
    fi
    # Release on INT/TERM/HUP as well as on the normal path. Without this, a
    # lane killed mid-run leaves the lock held until break_if_stale expires it,
    # and from every other lane's side that is indistinguishable from a lane
    # legitimately holding it for a very long time -- which is what tonight's
    # 1000-second holds looked like from the outside.
    trap 'cmd_release "$who" >/dev/null 2>&1; trap - INT TERM HUP; kill -INT $$' INT TERM HUP
    "$@"
    local rc=$?
    trap - INT TERM HUP
    cmd_release "$who"
    return $rc
}

# Returns 0 only if the lock is held BY YOU, right now. Cheap enough to call
# immediately before a launch: it answers "is the exe about to run still mine?"
# in the one way a discarded exit code cannot fake.
cmd_verify() {
    local who="${1:?lane name required}" owner
    if [ ! -d "$LOCK" ]; then
        echo "[gamelock] VERIFY FAILED: no lock is held at all (expected '$who')." >&2
        return 1
    fi
    owner=$(lock_owner)
    if [ "$owner" != "$who" ]; then
        echo "[gamelock] VERIFY FAILED: held by '$owner', not '$who'." >&2
        return 1
    fi
    echo "[gamelock] verified: still held by $who"
}

# The safe path, made the easy one. Acquire, run, release -- with the acquire's
# exit status checked HERE, where no caller's pipe can discard it. Anything the
# caller wants to pipe is this command's output, not the acquire's.
#
#   tools/game_dir_lock.sh run lane-FOO 900 -- make sa-run
#
cmd_run() {
    local who="${1:?lane name required}" timeout="${2:-900}"
    shift 2
    [ "${1:-}" = "--" ] && shift
    [ $# -gt 0 ] || { echo "[gamelock] run: no command given" >&2; return 2; }

    if ! cmd_acquire "$who" "$timeout"; then
        echo "[gamelock] run: ABORTING, the lock was never granted to '$who'." >&2
        return 1
    fi
    "$@"
    local rc=$?
    cmd_release "$who"
    return $rc
}

# Returns 0 only if the lock is held BY YOU, right now. Cheap enough to call
# immediately before a launch: it answers "is the exe about to run still mine?"
# in the one way a discarded exit code cannot fake.
cmd_verify() {
    local who="${1:?lane name required}" owner
    if [ ! -d "$LOCK" ]; then
        echo "[gamelock] VERIFY FAILED: no lock is held at all (expected '$who')." >&2
        return 1
    fi
    owner=$(lock_owner)
    if [ "$owner" != "$who" ]; then
        echo "[gamelock] VERIFY FAILED: held by '$owner', not '$who'." >&2
        return 1
    fi
    echo "[gamelock] verified: still held by $who"
}

case "${1:-}" in
    acquire) shift; cmd_acquire "$@" ;;
    run)     shift; cmd_run "$@" ;;
    verify)  shift; cmd_verify "$@" ;;
    release) shift; cmd_release "$@" ;;
    status)  cmd_status ;;
    *)       usage ;;
esac
