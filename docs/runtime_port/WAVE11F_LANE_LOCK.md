# Wave 11 follow-up 4: `tools/game_dir_lock.sh` -- three FIFO/exit-code defects

Lane LOCK. Worktree `C:/code/GIT/wt-w11f-lock`, branch `wave11f/lock`. Subject
file: `tools/game_dir_lock.sh`. Selftest: `tools/game_dir_lock_selftest.sh`.

All three defects named in the brief were confirmed real by reproducing them
against the pre-fix code before writing the fix. Each is reported separately
below, per the brief's "score each control separately" instruction.

## Defect 1 -- empty-ticket starvation in `ticket_head()`

**Confirmed real.** `asked=$(cat "$f" 2>/dev/null || echo 0)` only falls back
to `0` when `cat` *fails*. A ticket file that exists but is **empty** makes
`cat` succeed with empty output, so `asked=""`. `[ "$asked" -lt "$best_at" ]`
then throws `integer expression expected`, the `if` reads as false, and that
ticket is silently never chosen as head -- permanently, while any other lane
holds a readable ticket.

Reproduction against the pre-fix file (isolated to its function library,
scratch queue dir, no dispatch): a queue with `laneB` asked far in the future
and `laneA` holding an **empty** ticket. Correct behaviour: an unreadable
"when did I ask" should default to "assume you were first" (epoch 0), so
`laneA` must win.

```
-- OLD ticket_head on an empty ticket --
laneB          <- WRONG: the empty ticket (laneA) was silently skipped
```

**Fix**: `tools/game_dir_lock.sh:295-308` (`ticket_head`) -- coerce both
`asked` and `age` to `0` via `case "$x" in ''|*[!0-9]*) x=0 ;; esac` before
the numeric tests, applied to both the `cat` read (the reported bug) and the
`date -r` read on the line above (same shape: a successful-but-empty read
bypassing its `|| echo 0` fallback).

**New behaviour** (`tools/game_dir_lock_selftest.sh::test_defect1`), same
scratch setup, run against the fixed file:

```
PASS -- defect 1: empty ticket (ticket_head selects it instead of going blind)
```

`ticket_head` now returns `laneA` for that setup, not `laneB`.

## Defect 2 -- timeout path restamps the ticket epoch on requeue

**Confirmed real.** Every timeout branch in the old `cmd_acquire` called
`ticket_drop "$who"` before returning 1. A caller that retries (calls
`acquire` again) hits `ticket_take()`, which only preserves an existing
ticket's epoch (`[ -f "$QUEUE/$who" ] || printf '%s' "$(now)" > ...`) -- since
the file was just dropped, it writes a **fresh** `now()`. The longest-waiting
lane requeues at the back, which is exactly the unfairness the FIFO ticket
system exists to prevent (its own header cites a lane losing a slot after
waiting an hour, Wave 10).

**Fix**: removed `ticket_drop "$who"` from both `TIMEOUT` branches in
`cmd_acquire` (`tools/game_dir_lock.sh`, the "queued behind" branch and the
"still holds it" branch). The ticket is left in place; a caller that retries
soon keeps its original epoch (its file still exists, so `ticket_take` is a
no-op on content); a caller that never retries has its ticket ignored by
`ticket_head` once it passes `TICKET_STALE` (60s, since `ticket_touch` is what
governs liveness via mtime, not the content epoch) and is later swept by a new
`ticket_sweep_garbage()` (added so abandoned tickets don't accumulate forever
in `$QUEUE` -- runs from `ticket_take()`, removes anything older than
`TICKET_STALE * 20` = 20 minutes, well past any realistic retry gap).

**Verification, two independent checks** (`test_defect2`,
`test_defect2_source`):

1. **Behavioural, with a control that proves the test discriminates.** The
   control replays the OLD sequence (`ticket_drop` then `ticket_take`) and
   asserts the epoch *does* change (it must, or the test can't tell old from
   new); the real check replays the NEW sequence (`ticket_take` alone, no
   drop) and asserts the epoch is unchanged:

   ```
   PASS -- defect 2: timeout+requeue preserves original asked epoch
           (old sequence gave 1789048917, new gives 1000000000)
   ```

2. **Source-level**, since driving a real 900s timeout isn't practical in a
   selftest: extracts both `TIMEOUT` branches of `cmd_acquire`, strips
   comments, and asserts neither contains a live `ticket_drop` call:

   ```
   PASS -- defect 2: source check (TIMEOUT branches no longer ticket_drop)
   ```

## Defect 3 -- the exit-code trap

**Partially pre-existing, gap confirmed and closed.** Wave 10 had already
added real mitigations to this file: a `^\[gamelock\] GRANT ` positive-match
line for pipe-safe callers, a documented `if ! acquire; then exit 1; fi`
idiom, and a `verify <lane>` command that returns non-zero unless that exact
lane holds the lock right now -- which is already, functionally, the
"assert-held" verb the brief describes. `grep -rn "game_dir_lock" tools/`
found at least one live caller (`tools/objext_run.sh:28`) that calls
`acquire` with **no check of any kind**, not even a bare `||` -- a gap none of
Wave 10's mitigations can close, since they all depend on the caller reading
*something*.

**Fix** (kept in-scope to this file, per the brief -- not every caller was
touched): added `assert-held` as an alias for the existing `verify` command
(`tools/game_dir_lock.sh` case dispatch), and rewrote the usage header to lead
with a copy-pasteable idiom that puts the check as its own statement directly
in front of the dangerous action, independent of whether `acquire`'s exit code
was ever read:

```sh
tools/game_dir_lock.sh acquire lane-FOO 900
tools/game_dir_lock.sh assert-held lane-FOO || exit 1     # <-- copy this line
cp standalone/build/reimpl-x64.exe "$GAME/reimpl-x64.exe"
```

and notes that `run` is preferred precisely because it has no such gap to
begin with (no exit code exists for a caller to discard between the check and
the action).

Confirmed the verb did not exist before this change: `grep -n "assert-held"`
on the pre-fix file returns nothing (exit 1).

**Verification** (`test_defect3`), exit status checked directly, never
piped, across three cases:

```
PASS -- defect 3: assert-held gates correctly
        (fail/succeed/fail across no-lock, true holder, wrong lane)
```
  - no lock held at all -> non-zero
  - `laneA` holds it, `assert-held laneA` -> zero
  - `laneA` holds it, `assert-held laneB` -> non-zero

## Full selftest output

```
== game_dir_lock.sh selftest ==
subject: /c/code/GIT/wt-w11f-lock/tools/game_dir_lock.sh

PASS -- defect 1: empty ticket (ticket_head selects it instead of going blind)
PASS -- defect 2: timeout+requeue preserves original asked epoch (old sequence gave 1789048917, new gives 1000000000)
PASS -- defect 2: source check (TIMEOUT branches no longer ticket_drop)
PASS -- defect 3: assert-held gates correctly (fail/succeed/fail across no-lock, true holder, wrong lane)

== summary (per-defect, not aggregate) ==
PASS=4 FAIL=0
```

(Defect 2 has two independent checks by design, one behavioural with a
control, one source-level -- both pass, counted separately above and in the
summary line.)

## What was NOT changed

- Lock semantics: advisory, `mkdir`-based atomic acquire, `run_in_progress`
  refusal, `break_if_stale`, refusal to release someone else's lock -- all
  unchanged.
- No existing caller (`run`, `acquire`, `release`, `status`, `orphans`,
  `verify`) had its interface changed; `assert-held` is a pure addition
  (alias), `ticket_head`/`ticket_take` keep their signatures.
- Did not touch `tools/objext_run.sh` or any other caller. It is a real gap
  (an unchecked `acquire` call) but out of this lane's scope per the brief
  ("you cannot fix every caller"); flagged here for whoever picks it up next.

## Deviation from the brief

None of the three defects turned out to be non-issues. Defect 3 was **more
than half already fixed** by Wave 10 (`verify`, the `GRANT` line, the
documented idiom) -- the actionable residue was narrower than the brief
implied: a discoverability/naming gap (no verb spelled `assert-held`) and at
least one caller skipping the check outright, not an absence of machinery.
