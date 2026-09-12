# WAVE 26 KICKOFF

Base: `wave26/base` at `1337b417` (Wave 25 integration + cutover attempt 26).
Seven lanes, dispatched in parallel, each in its own private worktree.

## BOARD, RE-DERIVED ON THIS TIP

`tools/runtime_census.py` on `1337b417`, with `rt_mint_gen.py --check` reading
**UP TO DATE** (63 vtables, 104 BaseTypeInfo, 39 storage):

```
TOTAL   1333 called / 1185 ported / 88.9%      gap 148
```

Gap decomposition (the census prints this; do not re-derive it by hand):

```
  BODY 15 · VTABLE 54 · DATA 46 · UNKNOWN 33 (address-keyed)
```

**7 of the 15 BODY rows are the allocator, already WRITTEN and held behind
`BANISHED_RT_ALLOC`.** The real unwritten body surface is **8 functions**. A lane
scoped on "148 rows to port" is scoped against a population that does not exist.

Per-namespace gap: internal 33 · IO 19 · System 19 · Video 19 · Graphics 18 ·
Object 10 · Achievement 9 · Engine 8 · UI 6 · Audio 4 · Input 2 · Application 1.

## THE LARGEST UNTOUCHED LEVER

`PROMOTION BACKLOG`: **194 registered bodies across 49 unpromoted vtables, 29 of
them COMPLETE.** None of those 194 moves a column today — an unpromoted table is
published only under `BANISHED_RT_MINT=1`, `RT_DATA_SLOT` falls through, and
**retail's bodies run**.

COMPLETE is the STATIC half of readiness only. It says promoting would put no trap
in a live table. It does **not** say the row is promotable — that turns on owning
the function that STAMPS the table. A green readiness screen is not a route.

## STANDING WARNINGS FOR EVERY LANE

1. **THE GAME-DIR LOCK IS DEFECTIVE AND THE FIX IS NOT IN YET.** Lane LOCK is
   fixing it *in this wave, in parallel with you*. Until it merges:
   - `STALE_AFTER=1800` (`tools/game_dir_lock.sh:131`) is **shorter than a
     legitimate measurement**. Wave 25 lane ALLOC's lock was broken as stale at
     1802s **while its process was alive and had written to the directory 1s
     before**.
   - **The holder is never told.** ALLOC got no signal and lost a measurement pair.
   - **MITIGATION, MANDATORY:** md5 the deployed exe against your own build
     *immediately before every launch* and record it in your log. That per-run
     check is what turned "was my measurement contaminated?" from an argument into
     a measurement. Do not skip it because your run is short.
   - Never pipe `game_dir_lock.sh acquire` into anything — a pipeline exits with
     the LAST command's status, so a TIMEOUT reads as a GRANT.
   - Never background a blocking acquire: the lock is held from the moment it is
     GRANTED, not when you notice.

2. **Do not `find /`.** Three Wave 22 lanes did, burned ~4,500 CPU-seconds and
   contaminated a perf A/B. Grep/Glob scoped to your worktree.

3. **Report what you MEASURED, separately from what you INFER.** Every closed wave
   in this project has a defect of the form "an asserted number sat beside a
   measured one and inherited its credibility". If you did not run it, say so.

4. **A cheap-wins list is the stalest section of any handover and the first one
   acted on.** Wave 20 §4: three items named cheap were stale on contact while
   every *measured* section held. **Re-derive any figure below before acting on
   it** — each lane brief names the command.

5. **If you did not get to something, say so plainly.** A residual naming a
   condition but no addressee never fires.

## THE CUTOVER WALL, AS OF ATTEMPT 26

```
[RtMint] FATAL: minted BaseTypeInfo::factoryFn is not ported
  vtable : ?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A
  slot   : 0
  target : ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ
```

Not an EXEC-of-0 — the mint's own per-slot trap, naming itself. The boot now
reaches deep into game init (Engine::Startup complete, MainLoop constructed, Game
ctor at t=569ms, KeyMap, 50 input bindings) — well past the video and audio walls
of attempts 23/24/25.

**A cutover boot is a GATE, not a PLANNER.** It can only name what it reached
before faulting. Expect attempt 27 to find ONE new name, not a roadmap.

## AN OPEN DISCREPANCY, FOR LANE ATTEND

`tools/cutover_attempt26.sh:42` sets **`BANISHED_RT_MINT=1`** — the blanket switch
that Wave 25 lane MANIFEST measured as **publishing the WITHHELD row against the
manifest's own verdict**, and which integrator-queue item **C3 said not to use at
cutover**. So attempt 26's `published 44, withheld 1` line may describe the
manifest's *intent* rather than the process's *registration state*. This is
flagged as a question, not a finding. ATTEND settles it.

## LANES

| Lane | Branch | Scope |
|---|---|---|
| LOCK     | `wave26/lock`     | The game-dir lock's two defects; `stress_run.sh` machine load |
| FACTORY  | `wave26/factory`  | The attempt-26 wall, and the size of the factory-body family |
| PROMOIO  | `wave26/promoio`  | IO/Video/System promotion backlog + those namespaces' DATA/BODY rows |
| PROMOGFX | `wave26/promogfx` | Graphics/Engine/Achievement/Audio backlog + their BODY rows |
| INT33    | `wave26/int33`    | The 33 address-keyed UNKNOWN rows |
| DOORS    | `wave26/doors`    | Board honesty: wrapper doors, invisible registrations, phantom literals |
| ATTEND   | `wave26/attend`   | Two runs: W17 attendance (decides 7 rows), and a clean attempt 27 |

PROMOIO and PROMOGFX are split **by namespace** so they cannot collide on the
backlog table. Every gap bucket is owned by exactly one lane.

## DELIVERABLE

Each lane writes `docs/runtime_port/WAVE26_LANE_<NAME>.md` and commits on its own
branch. The integrator merges and runs attempt 27. Write your doc **as you go**,
not at the end — Wave 25's integrator queue exists because a residual written
after the fact names no addressee.
