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
#   tools/game_dir_lock.sh assert-held <lane-name>   # gate a step on ownership; see below
#   tools/game_dir_lock.sh release <lane-name>
#   tools/game_dir_lock.sh status
#   tools/game_dir_lock.sh orphans   # is the dir usable? names an UNLOCKED run
#
# "IS THE LOCK FREE?" AND "CAN I USE THE DIRECTORY?" ARE DIFFERENT QUESTIONS.
# A process that outlives its run keeps reimpl-x64.exe and standalone_*.log
# open, so deploys fail with "Device or resource busy" while the lock correctly
# reads free. If a deploy fails that way, do NOT wait for the lock -- run
# `orphans`. See the UNLOCKED RUNS AND ORPHANS block below.
#
#   # the whole point -- deploy and run under ONE lock, so nothing can land
#   # between the copy and the launch:
#   tools/game_dir_lock.sh acquire lane-FOO 900 || exit 1
#
# PREFER, and the reason it exists:
#
#   tools/game_dir_lock.sh run lane-FOO 900 -- make sa-run
#
# READ YOUR LOGS FROM A SNAPSHOT, NOT FROM THE GAME DIR:
#
#   tools/game_dir_lock.sh run lane-FOO 900 --snapshot ./mylogs -- make sa-run
#
# The lock covers deploy+run. It does NOT cover the window in which you sit and
# ANALYSE, and standalone_*.log is overwritten by whoever runs next. On
# 2026-09-09 three consecutive reads of "the" debug log were three different
# processes, and another lane's verification-mode banner was read as a
# default-on regression in the merged tree. `--snapshot` copies the logs while
# the lock is still held, so what you analyse is what you ran.
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
# SECOND COSTUME, found 2026-09-10 by Wave 18 lane VDATA, cost ~15 min: it is
# not only pipes. ANY compound command discards the refusal, because the caller
# (or the agent harness reporting the step) sees the status of the LAST element:
#
#   tools/game_dir_lock.sh acquire lane-FOO 540; echo "ACQUIRE_EXIT=$?"   # <-- reports 0
#
# Note what makes this one nastier than the pipe: the echoed text is CORRECT --
# `$?` there really is acquire's status -- so the human-readable output says
# ACQUIRE_EXIT=1 while the step's own exit status says 0. Two fields disagree
# and the authoritative-looking one is wrong. A wrapper added to make the status
# more visible is what hid it.
#
# So the rule is not "do not pipe". It is: DO NOT PUT `acquire` IN A COMPOUND
# COMMAND OF ANY KIND -- no pipe, no `;`, no trailing echo, no `&&` tail. Give
# it a statement of its own, and put `assert-held` in front of the dangerous
# action regardless (below). In that lane's case assert-held is what caught it
# before it deployed over another lane.
#
# A granted acquire now also prints a line matching `^\[gamelock\] GRANT `, so a
# caller that must pipe can test for the POSITIVE rather than trusting a status
# that a pipe can swallow. And `verify <lane>` (alias: `assert-held`)
# re-checks ownership immediately before launch.
#
# THE STRONGEST GUARD DOES NOT TRUST `acquire`'S EXIT CODE AT ALL -- a caller
# that forgets the `||` entirely (not even a pipe, just a bare call with no
# check) defeats every mitigation above, and that happened in Wave 11. Put
# `assert-held` directly in front of the dangerous action instead, as its own
# statement that fails the step on its own if ownership is not current:
#
#   tools/game_dir_lock.sh acquire lane-FOO 900
#   tools/game_dir_lock.sh assert-held lane-FOO || exit 1     # <-- copy this line
#   cp standalone/build/reimpl-x64.exe "$GAME/reimpl-x64.exe"
#
# This is what `run` already does internally, and it is why `run` is PREFERRED
# over hand-rolling acquire/deploy/release: there is no exit code to discard
# because there is no gap between the check and the action for a bad read of
# it to land in.
#
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
    # Test seam: GAMELOCK_FAKE_RUN=1 forces the "a run is live" answer so the
    # messages below can be exercised without stranding a real process on the
    # shared game directory. Never set in normal use.
    [ "${GAMELOCK_FAKE_RUN:-0}" = "1" ] && return 0
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

# ---- UNLOCKED RUNS AND ORPHANS ----------------------------------------------
#
# THE FOURTH VARIANT OF THE CLOBBER CLASS, 2026-09-09. `run_in_progress` has
# always been correct and has always gated the grab. What was missing is that
# NOTHING TOLD ANYONE ABOUT IT. The failure ran like this:
#
#   * the integrator's own run finished and reported completion, but its
#     process lingered ~50 minutes holding standalone_*.log and reimpl-x64.exe
#     open;
#   * `status` printed "free" -- TRUE, nobody held the lock -- and said nothing
#     about the live process, because its process advisory sat behind
#     `[ -d "$LOCK" ] || return 0` and so only ran when the lock was HELD;
#   * a lane's `acquire` could never grab (run_in_progress was true), fell
#     through to the generic wait, and printed `waiting for '(unknown)'`,
#     because with no lock directory `lock_owner` has nothing to read.
#
# Every individual component was working. The instrument's fault was reporting
# a TRUE statement ("free") that answered a different question from the one
# being asked ("can I use the directory?"). Those are two questions and this
# script now answers both, separately and by name.
#
# ---------------------------------------------------------------------------
# CORRECTION, 2026-09-09, AND IT IS THE INTEGRATOR'S OWN ERROR.
#
# This block originally continued: "the lane read that as lock contention,
# reported itself 'queued behind' another lane that in fact held nothing, and
# waited 15 minutes." THAT SENTENCE WAS FALSE, and the lane it described said
# so with evidence.
#
# The orphan was real and was demonstrated: a `cp` failed with "Device or
# resource busy" while `status` said free, and killing the stranded process
# fixed it. What was NOT established is that the orphan caused the OTHER lane's
# 15-minute wait. It did not. That lane's `acquire` printed
# `queued behind 'updateload'` and then `waiting for 'updateload'` -- both
# naming a real lane -- and this script can only produce those two strings when
# a live TICKET is ahead of you (line ~280) and when the lock is genuinely HELD
# (line ~323). In the orphan case there is no lock directory, so `lock_owner`
# has nothing to read and the string would have been `(unknown)`. The message
# the lane actually saw is therefore proof of ordinary, correct FIFO queueing
# behind a lane that really did hold the lock and really was running.
#
# So two conditions were conflated: the integrator's orphan (which blocked the
# INTEGRATOR'S deploy) and a genuine queue (which delayed the other lane). The
# fix below stands on its own evidence and is unaffected -- the silent-orphan
# gap is real whatever caused any particular wait -- but the attribution was
# wrong, and it was wrong in the most seductive direction: an explanation
# offered for SOMEONE ELSE'S anomaly that made the explainer the sole cause,
# tidily accounted for both lanes, and stopped the search. That is exactly
# `reference_a_reconciliation_retires_the_flag.md`. The tell was available at
# the time and was not looked at: the lane quoted a message naming a lane, and
# the orphan branch cannot produce a name.
#
# DIAGNOSTIC RULE THIS LEAVES BEHIND: the two conditions have DIFFERENT
# symptoms and must not be merged. An orphan fails a `cp` with "Device or
# resource busy" while `status` reads free, and never reaches a queue. A real
# queue never reaches a `cp` at all. Ask which one happened before explaining
# either.
# ---------------------------------------------------------------------------
#
# It still does NOT kill anything. A live exe nobody locked is either an orphan
# or a lane running unlocked, and from here those are indistinguishable --
# killing the second one destroys a real measurement. Facts, then a human.

# PIDs of every running reimpl-x64.exe, space separated. Empty if none.
live_exe_pids() {
    tasklist 2>/dev/null | grep -i 'reimpl-x64\.exe' | awk '{print $2}' | tr '\n' ' '
}

# Seconds since the game dir was last written, or -1 if it never was. Used to
# age a suspected orphan: a live run writes its logs continuously.
game_dir_idle() {
    local newest=0 f m
    for f in standalone_debug.log standalone_error.log standalone_crash.log reimpl-x64.exe; do
        [ -f "$GAME/$f" ] || continue
        m=$(date -r "$GAME/$f" +%s 2>/dev/null) || continue
        [ "$m" -gt "$newest" ] && newest=$m
    done
    [ "$newest" -eq 0 ] && { printf '%s' -1; return; }
    printf '%s' "$(( $(now) - newest ))"
}

# Print the facts about a live run that NO LANE HAS LOCKED. Callers decide what
# it means; this deliberately draws no conclusion and kills nothing.
report_unlocked_run() {
    local pids idle
    pids=$(live_exe_pids); idle=$(game_dir_idle)
    echo "[gamelock] UNLOCKED RUN: the game dir is IN USE but NO LANE HOLDS THE LOCK." >&2
    echo "[gamelock]   reimpl-x64.exe pid(s): ${pids:-<none visible>}" >&2
    echo "[gamelock]   last game-dir write: ${idle}s ago" >&2
    echo "[gamelock]   deployed exe: $(md5sum "$GAME/reimpl-x64.exe" 2>/dev/null | cut -c1-32)" >&2
    echo "[gamelock]   This is NOT lock contention -- nobody is holding the lock, so" >&2
    echo "[gamelock]   waiting will not clear it. It is one of exactly two things:" >&2
    echo "[gamelock]     (a) an ORPHAN: a finished run whose process outlived it. Windows" >&2
    echo "[gamelock]         keeps the exe and logs open for the life of the process, so" >&2
    echo "[gamelock]         every deploy fails with 'Device or resource busy'." >&2
    echo "[gamelock]     (b) somebody RUNNING UNLOCKED, mid-measurement." >&2
    echo "[gamelock]     (c) a THREAD-LESS HUSK: the process crashed or exited, every" >&2
    echo "[gamelock]         thread is gone and every HANDLE IS ALREADY RELEASED, but the" >&2
    echo "[gamelock]         pid lingers in the table because something still holds a" >&2
    echo "[gamelock]         handle to the process object. taskkill reports 'no running" >&2
    echo "[gamelock]         instance of the task' while tasklist still lists it." >&2
    echo "[gamelock]   (a) and (b) look identical from here. (c) DOES NOT -- measure it:" >&2
    # The (a)/(b)/(c) split is not a guess: the same non-destructive move-aside
    # test `run_in_progress` already runs on the logs answers the ONE question a
    # blocked lane actually has -- will my deploy fail? A live run (b), and an
    # orphan (a) whose threads are still alive, hold the exe open. A husk (c)
    # holds nothing.
    #
    # WAVE 19: this block previously asserted "Windows keeps the exe and logs
    # open for the life of the process, so EVERY DEPLOY FAILS". That premise is
    # FALSE for (c) -- handles die with the last thread, not with the pid. On
    # 2026-09-10 pid 17292 sat here for 22 minutes after an ACCESS_VIOLATION,
    # blocking three lanes, while the exe renamed freely the whole time. The
    # tool drew a conclusion its evidence (a pid in the table) did not support.
    # A PID IN THE PROCESS TABLE IS NOT A RUNNING PROGRAM.
    local held=""
    for f in reimpl-x64.exe standalone_debug.log standalone_error.log; do
        [ -f "$GAME/$f" ] || continue
        if mv "$GAME/$f" "$GAME/$f.holdtest" 2>/dev/null; then
            mv "$GAME/$f.holdtest" "$GAME/$f" 2>/dev/null
        else
            held="${held}${held:+ }$f"
        fi
    done
    if [ -n "$held" ]; then
        echo "[gamelock]   HANDLES HELD: $held" >&2
        echo "[gamelock]   -> a deploy WILL fail. This is (a) or (b), and they are" >&2
        echo "[gamelock]      indistinguishable from here. Do NOT kill a process you did" >&2
        echo "[gamelock]      not start -- killing (b) destroys a real run. If it is" >&2
        echo "[gamelock]      yours, kill it; otherwise ASK, and escalate with these" >&2
        echo "[gamelock]      numbers, not with a guess." >&2
    else
        echo "[gamelock]   HANDLES HELD: none (exe and logs both move aside freely)" >&2
        echo "[gamelock]   -> a deploy will SUCCEED. This is (c), a husk. It is not" >&2
        echo "[gamelock]      holding the game dir and waiting on it is waiting on" >&2
        echo "[gamelock]      nothing. PROCEED, and take the lock as usual." >&2
        echo "[gamelock]      (This does NOT prove no lane is mid-measurement -- it" >&2
        echo "[gamelock]      proves nothing is holding these three files right now.)" >&2
    fi
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
    ticket_sweep_garbage
    [ -f "$QUEUE/$who" ] || printf '%s' "$(now)" > "$QUEUE/$who"
}
ticket_touch() { touch "$QUEUE/$1" 2>/dev/null; }
ticket_drop()  { rm -f "$QUEUE/$1" 2>/dev/null; }

# A ticket a caller keeps ON TIMEOUT (see cmd_acquire below) so a requeue keeps
# its original epoch is, by design, no longer touched -- ticket_head ignores it
# once it passes TICKET_STALE, but nothing else ever removes the file. A lane
# that times out and never retries would otherwise leave it in $QUEUE forever.
# That is harmless to correctness (stale tickets are never counted) but is
# unbounded garbage, so sweep anything far past any realistic retry gap.
TICKET_GARBAGE_AFTER=$(( TICKET_STALE * 20 ))   # 20 min; no real retry waits that long
ticket_sweep_garbage() {
    local f age
    [ -d "$QUEUE" ] || return 0
    for f in "$QUEUE"/*; do
        [ -f "$f" ] || continue
        age=$(( $(now) - $(date -r "$f" +%s 2>/dev/null || echo 0) ))
        case "$age" in ''|*[!0-9]*) age=0 ;; esac
        [ "$age" -gt "$TICKET_GARBAGE_AFTER" ] && rm -f "$f" 2>/dev/null
    done
}

# Echo the lane name at the head of the queue: the LIVE ticket (refreshed within
# TICKET_STALE) with the earliest first-asked epoch. Empty if nobody is queued.
#
# DIGITS GUARD, 2026-09-10: `cmd || echo 0` only fires if `cmd` FAILS. A ticket
# file that EXISTS but is EMPTY (e.g. a `ticket_take` interrupted between
# `mkdir -p` and the `printf` that fills it in, or a filesystem hiccup) makes
# `cat` succeed with empty output, so `asked=""`. The numeric test below then
# THROWS ("integer expression expected"), the `if` reads as false, and that
# ticket is silently never chosen as head -- forever, while any other lane
# holds a readable ticket. Observed in Wave 11: a lane's own ticket went blind
# this way, `ticket_head` fell through to the wrong branch, and the lane's log
# read like queued-behind-nobody. `date -r` failing has the same shape (a
# successful-but-empty read), so both are guarded the same way: force to a
# sane integer before the numeric tests use it.
ticket_head() {
    local best="" best_at="" f name asked age
    [ -d "$QUEUE" ] || return 0
    for f in "$QUEUE"/*; do
        [ -f "$f" ] || continue
        name=$(basename "$f")
        age=$(( $(now) - $(date -r "$f" +%s 2>/dev/null || echo 0) ))
        case "$age" in ''|*[!0-9]*) age=0 ;; esac
        [ "$age" -gt "$TICKET_STALE" ] && continue      # waiter died or left
        asked=$(cat "$f" 2>/dev/null || echo 0)
        case "$asked" in ''|*[!0-9]*) asked=0 ;; esac
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
                # Do NOT ticket_drop here. Dropping would make a requeuing
                # `acquire` call ticket_take() fresh, which stamps a NEW asked
                # epoch and sends the longest-waiting lane to the BACK of the
                # queue -- exactly the unfairness FIFO ticketing exists to
                # prevent (see the block comment above). Leave the ticket in
                # place: a caller that retries soon keeps its original place;
                # a caller that never retries has its ticket age past
                # TICKET_STALE (ignored by ticket_head) and eventually swept
                # by ticket_sweep_garbage.
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
        # THE CASE THAT USED TO PRINT `waiting for '(unknown)'`: no lock
        # directory exists, so nobody holds it, yet the grab above was refused
        # -- which can only mean run_in_progress() said a run is live. Waiting
        # cannot help, so say so immediately and once, rather than spinning to
        # timeout against a phantom holder.
        if [ ! -d "$LOCK" ] && run_in_progress; then
            echo "[gamelock] DENIED -- the game dir is in use by an UNLOCKED run." >&2
            report_unlocked_run
            ticket_drop "$who"
            return 1
        fi
        if [ "$waited" -ge "$timeout" ]; then
            # Printed on BOTH streams on purpose. A caller that pipes this
            # command loses the exit status (see the DEFEATED BY A PIPE note in
            # the usage block), so the refusal has to be visible on stdout too.
            echo "[gamelock] DENIED -- TIMEOUT after ${timeout}s; '$(lock_owner)' still holds it."
            echo "[gamelock] Do NOT deploy anyway -- that is the clobber this prevents."
            echo "[gamelock] TIMEOUT after ${timeout}s; '$(lock_owner)' still holds it." >&2
            echo "[gamelock] Do NOT deploy anyway -- that is the clobber this prevents." >&2
            # Same reasoning as the other TIMEOUT branch above: keep the
            # ticket so a requeue preserves the original asked epoch instead
            # of restamping now() and losing FIFO priority.
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
    # The orphan is BORN HERE. On 2026-09-09 a run reported completion, its
    # holder released, and its process lived another 50 minutes holding the exe
    # open -- blocking every later deploy while the lock read "free". Releasing
    # while your own run is still live is always a mistake, and this is the last
    # moment anyone can cheaply notice. It warns rather than refuses: a lane that
    # cannot kill its process must still be able to give the lock back.
    if run_in_progress; then
        echo "[gamelock] WARNING: releasing while a run is STILL LIVE." >&2
        echo "[gamelock]   reimpl-x64.exe pid(s): $(live_exe_pids)" >&2
        echo "[gamelock]   Your process outliving your run is what creates an ORPHAN:" >&2
        echo "[gamelock]   the next lane's deploy fails with 'Device or resource busy'" >&2
        echo "[gamelock]   while this lock reads free, and it has no way to tell why." >&2
        echo "[gamelock]   Kill it before you walk away. Note BANISHED_KILL_AT_FRAME" >&2
        echo "[gamelock]   reports completion WITHOUT guaranteeing the process exited." >&2
    fi
    rm -rf "$LOCK"
    echo "[gamelock] released by $who"
}

cmd_orphans() {
    if ! run_in_progress; then
        echo "[gamelock] no live run: the game dir is writable."
        local pids; pids=$(live_exe_pids)
        [ -n "$pids" ] && echo "[gamelock] (note: reimpl-x64.exe pid(s) $pids exist but hold no game-dir handle)"
        return 0
    fi
    if [ -d "$LOCK" ]; then
        echo "[gamelock] a run is live and '$(lock_owner)' holds the lock -- NORMAL. Leave it alone."
        return 0
    fi
    report_unlocked_run
    return 1
}

cmd_status() {
    if [ -d "$LOCK" ]; then
        echo "[gamelock] HELD by '$(lock_owner)' for $(( $(now) - $(lock_started) ))s"
    elif run_in_progress; then
        # "free" alone was TRUE here and cost a lane 15 minutes on 2026-09-09,
        # because the honest answer to "is the lock free?" is not the answer to
        # "can I use the directory?". Both are printed, in that order.
        echo "[gamelock] free -- BUT THE GAME DIR IS IN USE (see below); a deploy will FAIL"
        report_unlocked_run
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
    local who="${1:?lane name required}" timeout="${2:-900}" snapdir=""
    shift 2
    if [ "${1:-}" = "--snapshot" ]; then snapdir="${2:?--snapshot needs a directory}"; shift 2; fi
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
    # Snapshot the logs BEFORE releasing. The lock protects the deploy+run
    # window; it says nothing about the ANALYSIS window, and reading
    # standalone_*.log after the release reads whichever lane ran next. That
    # fooled a whole analysis on 2026-09-09: three reads of "the" debug log were
    # three different processes, and another lane's verification-mode banner was
    # misread as a default-on regression in the merged tree. The tell -- object
    # pointers differing across reads -- was in the output and was walked past
    # twice.
    if [ -n "$snapdir" ]; then
        mkdir -p "$snapdir" 2>/dev/null
        local f
        for f in standalone_debug.log standalone_error.log standalone_crash.log                  standalone_boot_state.txt; do
            [ -f "$GAME/$f" ] && cp "$GAME/$f" "$snapdir/$f" 2>/dev/null
        done
        echo "[gamelock] logs snapshotted to $snapdir (taken BEFORE release, so they are yours)"
    fi
    cmd_release "$who"
    return $rc
}

# Returns 0 only if the lock is held BY YOU, right now. Cheap enough to call
# immediately before a launch: it answers "is the exe about to run still mine?"
# in the one way a discarded exit code cannot fake. Also reachable as
# `assert-held` (same command, different verb) for a caller that wants a name
# that cannot be misread as anything but a gate: `acquire` grants a lock,
# `assert-held` is the thing you put directly at a deploy step so the step
# itself refuses to proceed on a stolen or expired lock, regardless of what
# any earlier `acquire`'s exit code was read as.
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


# Test seam: GAMELOCK_NO_MAIN=1 sources this file for its functions (used by
# tools/game_dir_lock_selftest.sh to exercise ticket_head/ticket_take/etc.
# directly) without running the dispatch below. Never set in normal use. Same
# shape as the existing GAMELOCK_FAKE_RUN seam above.
if [ "${GAMELOCK_NO_MAIN:-0}" != "1" ]; then
case "${1:-}" in
    acquire) shift; cmd_acquire "$@" ;;
    run)     shift; cmd_run "$@" ;;
    verify|assert-held) shift; cmd_verify "$@" ;;
    release) shift; cmd_release "$@" ;;
    status)  cmd_status ;;
    orphans) cmd_orphans ;;
    *)       usage ;;
esac
fi
