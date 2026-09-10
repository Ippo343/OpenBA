#!/usr/bin/env bash
# =============================================================================
# tools/game_dir_lock_selftest.sh -- reproduces and verifies the three Wave 11
# follow-up defects fixed in tools/game_dir_lock.sh (2026-09-10):
#
#   1. empty-ticket starvation in ticket_head()
#   2. a timeout+requeue restamping the ticket epoch, losing FIFO priority
#   3. a caller reading a refusal as a grant (the "exit-code trap")
#
# Each defect gets its OWN pass/fail, reported separately -- never an
# aggregate. An aggregate "3/3" cannot distinguish "all three fixed" from "one
# test carrying the others".
#
# Runs entirely against a SCRATCH directory via BANISHED_GAME_DIR. Never runs
# against the real game directory. Uses `GAMELOCK_NO_MAIN=1` to source the
# subject file for its internal functions (ticket_head, ticket_take, ...)
# without triggering its command dispatch.
#
# `acquire`'s exit status is always checked directly ( `if cmd; then` / `cmd;
# rc=$?` ), never through a pipe -- piping the very command whose exit-code
# handling is under test would be its own punchline.
# =============================================================================
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCK_SH="$HERE/game_dir_lock.sh"

PASS=0
FAIL=0
report() {
    local label="$1" ok="$2" detail="$3"
    if [ "$ok" = "0" ]; then
        echo "PASS -- $label"
        PASS=$((PASS + 1))
    else
        echo "FAIL -- $label"
        echo "       $detail"
        FAIL=$((FAIL + 1))
    fi
}

new_scratch() {
    local d
    d="$(mktemp -d "${TMPDIR:-/tmp}/gdl_selftest.XXXXXX")"
    printf '%s' "$d"
}

# =============================================================================
# DEFECT 1 -- empty-ticket starvation
# =============================================================================
# An empty ticket file (asked="") must not make ticket_head() blind to that
# waiter. Reproduce by hand: create a queue dir with two tickets, one of them
# empty and the other with a LATER (larger) asked epoch. The empty one is the
# true earliest waiter (it has been sitting there, we just never recorded
# when) -- what we actually assert is narrower and unambiguous: ticket_head
# must not throw and must not silently skip the file to the point of always
# preferring the other ticket. We assert the two required properties:
#   (a) ticket_head does not error out / abort the loop on an empty ticket
#   (b) an empty ticket IS eligible to be chosen as head (asked coerces to 0,
#       the lowest possible epoch, which is also the conservative choice: an
#       unreadable "when did I ask" defaults to "assume you were first" rather
#       than "assume you don't exist")
test_defect1() {
    local scratch queue rc out
    scratch="$(new_scratch)"
    queue="$scratch/.gamedir_queue"
    mkdir -p "$queue"

    # laneB: a normal ticket, asked far in the future relative to epoch 0, so
    # if the empty ticket is correctly treated as asked=0 it MUST win.
    printf '%s' "9999999999" > "$queue/laneB"

    # laneA: an EMPTY ticket (the reproduction of the defect) with a fresh
    # mtime so it is not filtered out as stale.
    : > "$queue/laneA"
    touch "$queue/laneA"
    touch "$queue/laneB"

    out=$(
        GAMELOCK_NO_MAIN=1 BANISHED_GAME_DIR="$scratch" bash -c '
            source "'"$LOCK_SH"'"
            ticket_head
        ' 2>&1
    )
    rc=$?

    rm -rf "$scratch"

    if [ "$rc" -ne 0 ]; then
        report "defect 1: empty ticket (ticket_head must not error)" 1 \
            "ticket_head exited $rc, output: $out"
        return
    fi
    if [ "$out" != "laneA" ]; then
        report "defect 1: empty ticket (ticket_head must select it, asked coerced to 0)" 1 \
            "expected head='laneA' (asked coerced to 0, earliest), got '$out'"
        return
    fi
    report "defect 1: empty ticket (ticket_head selects it instead of going blind)" 0 ""
}

# =============================================================================
# DEFECT 2 -- timeout+requeue must preserve the original asked epoch
# =============================================================================
# Simulate: laneA takes a ticket at an old epoch (as if it had been queued a
# long time), then "times out" (cmd_acquire's timeout path, which must no
# longer ticket_drop), then calls ticket_take() again (as a retrying caller
# would). Assert the epoch recorded in the ticket file is UNCHANGED -- i.e.
# ticket_take() did not restamp now() because the file still existed.
test_defect2() {
    local scratch queue old_epoch rc out kept

    scratch="$(new_scratch)"
    queue="$scratch/.gamedir_queue"
    mkdir -p "$queue"

    old_epoch=1000000000   # a fixed, obviously-old epoch
    printf '%s' "$old_epoch" > "$queue/laneA"

    # Reproduce the OLD, buggy behaviour first, to prove the test actually
    # discriminates: if the timeout path called ticket_drop and then
    # ticket_take() ran, the epoch would change. We check that emulating the
    # OLD sequence (explicit drop + retake) DOES change the epoch, and that
    # the NEW code path (retake WITHOUT a drop, as cmd_acquire now does on
    # timeout) does NOT.
    old_behaviour_epoch=$(
        GAMELOCK_NO_MAIN=1 BANISHED_GAME_DIR="$scratch" bash -c '
            source "'"$LOCK_SH"'"
            ticket_drop laneA      # what the OLD cmd_acquire did on timeout
            ticket_take laneA      # what a requeuing acquire call does
            cat "$QUEUE/laneA"
        ' 2>&1
    )

    # restore the original ticket for the real (new-code) check
    printf '%s' "$old_epoch" > "$queue/laneA"

    kept=$(
        GAMELOCK_NO_MAIN=1 BANISHED_GAME_DIR="$scratch" bash -c '
            source "'"$LOCK_SH"'"
            # NEW behaviour: cmd_acquire timeout no longer drops the ticket,
            # so a requeuing acquire just calls ticket_take() against the
            # still-present file.
            ticket_take laneA
            cat "$QUEUE/laneA"
        ' 2>&1
    )

    rm -rf "$scratch"

    if [ "$old_behaviour_epoch" = "$old_epoch" ]; then
        report "defect 2: control (old drop-then-retake must lose the epoch)" 1 \
            "expected the OLD sequence to restamp away from $old_epoch, but it stayed $old_behaviour_epoch -- test does not discriminate"
        return
    fi
    if [ "$kept" != "$old_epoch" ]; then
        report "defect 2: timeout+requeue preserves original asked epoch" 1 \
            "expected epoch to stay $old_epoch, got '$kept' (old buggy sequence gave '$old_behaviour_epoch')"
        return
    fi
    report "defect 2: timeout+requeue preserves original asked epoch (old sequence gave $old_behaviour_epoch, new gives $kept)" 0 ""
}

# Companion check: cmd_acquire's TIMEOUT branches must not call ticket_drop
# any more (source-level check, since driving a real 900s timeout is not
# practical in a selftest). This directly checks the fix landed, not just a
# hand-simulation of its effect.
test_defect2_source() {
    local timeout_block rc
    # Extract the two TIMEOUT branches of cmd_acquire and confirm neither
    # calls ticket_drop.
    timeout_block=$(awk '
        /DENIED -- TIMEOUT after/{grab=1}
        grab{print}
        grab && /return 1/{grab=0}
    ' "$LOCK_SH")
    # Strip comments (this file's own explanation of the fix legitimately
    # mentions ticket_drop by name) before looking for a live call.
    if printf '%s' "$timeout_block" | grep -v '^[[:space:]]*#' | grep -q 'ticket_drop'; then
        report "defect 2: source check (TIMEOUT branches must not ticket_drop)" 1 \
            "found ticket_drop still present in a TIMEOUT branch:
$timeout_block"
        return
    fi
    report "defect 2: source check (TIMEOUT branches no longer ticket_drop)" 0 ""
}

# =============================================================================
# DEFECT 3 -- the exit-code trap: a caller must have an unmissable verb
# =============================================================================
# `assert-held <lane>` (alias for `verify`) must exit non-zero unless that
# exact lane currently owns the lock, and 0 when it does. This is the "point
# of use" gate the brief asks for -- checked directly, never through a pipe.
test_defect3() {
    local scratch rc

    scratch="$(new_scratch)"

    # Case A: nobody holds the lock at all.
    BANISHED_GAME_DIR="$scratch" bash "$LOCK_SH" assert-held laneA >/dev/null 2>&1
    rc=$?
    if [ "$rc" -eq 0 ]; then
        rm -rf "$scratch"
        report "defect 3: assert-held with no lock held must fail" 1 "exit code was 0"
        return
    fi

    # Case B: laneA legitimately holds the lock -- must succeed.
    BANISHED_GAME_DIR="$scratch" bash "$LOCK_SH" acquire laneA 5 >/dev/null 2>&1
    if ! BANISHED_GAME_DIR="$scratch" bash "$LOCK_SH" assert-held laneA >/dev/null 2>&1; then
        rc=$?
        rm -rf "$scratch"
        report "defect 3: assert-held for the true holder must succeed" 1 "exit code was $rc"
        return
    fi

    # Case C: laneB is not the holder -- must fail (this is the exact shape
    # of the exit-code trap: a caller who assumes ownership without checking).
    BANISHED_GAME_DIR="$scratch" bash "$LOCK_SH" assert-held laneB >/dev/null 2>&1
    rc=$?
    BANISHED_GAME_DIR="$scratch" bash "$LOCK_SH" release laneA >/dev/null 2>&1
    rm -rf "$scratch"
    if [ "$rc" -eq 0 ]; then
        report "defect 3: assert-held for a non-holder must fail" 1 "exit code was 0"
        return
    fi

    report "defect 3: assert-held gates correctly (fail/succeed/fail across no-lock, true holder, wrong lane)" 0 ""
}

echo "== game_dir_lock.sh selftest =="
echo "subject: $LOCK_SH"
echo

test_defect1
test_defect2
test_defect2_source
test_defect3

echo
echo "== summary (per-defect, not aggregate) =="
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
