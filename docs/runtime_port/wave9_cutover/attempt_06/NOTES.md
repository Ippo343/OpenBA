# Cutover boot attempt 06 — 2026-09-08, integrator

Exe built 03:56 from the tip with lane Y (`129b144`) and lane Z (`a3de8bd`)
merged; deployed 03:57; run 03:57. Runtime-x64.dll renamed to `.absent` for the
run and restored immediately after, per the runbook.

## Outcome: NO CRASH, and THE SAME WALL AS 04 AND 05

```
[Standalone] === INIT SUCCESS ===
[Standalone] === ENTERING GameMain ===
[GameMain] Engine ctor          = 0000000000000000
[GameMain] Engine dtor          = 0000000000000000
[GameMain] Engine::Startup      = 0000000000000000
[GameMain] Engine::Shutdown     = 0000000000000000
[GameMain] FATAL: Engine ctor/dtor not resolved
[Standalone] === GameMain returned 1 ===
```

`standalone_crash.log` is EMPTY and `[RtResolve] 0 misses`. The process boots,
registers 1617 addresses, builds all 345 type descriptions, installs the message
hook, enters `GameMain`, and returns 1 through its own guard.

## Two numbers that move in opposite directions, and the honest reading

**Residue fell 75 -> 61 distinct.** Lanes Y and Z closed fourteen rows between
them, and the two lane-Y predictions that could be checked here both held: the
`?_singleton@Factory@Object@@` `[RtResolveEarly] BYPASS?` line is GONE (only the
`??_7Thread@System@@6B@` one remains, which is not lane Y's), and the five names
it claimed would leave `[RtUnresolved]` did.

**DISTANCE TRAVELLED DID NOT MOVE AT ALL.** Attempts 04, 05 and 06 stop at the
identical line. The runbook names distance as the progress metric, and by that
metric this attempt scored ZERO — the fourteen rows it closed are all BEHIND the
wall or on paths the run never reaches.

That is worth stating plainly rather than reporting the residue drop and moving
on. **A falling residue count and a stationary boot are compatible**, because
residue records what a run ASKED FOR, and a run that stops at `GameMain` asks
for almost nothing. The 61 remaining are not 61 obstacles in a line; they are
*everything the boot would ask for next*, and the ones behind the wall have
never been exercised at all. Closing them is necessary and it is not progress
you can see from here.

## The wall is one row, and its blocker is a Wave 8 fact that may have inverted

Four symbols, all held back by `runtime/src/engine/engine.cpp` with the single
reason **"allocator, NO-GO"**:

| symbol | retail | ord |
|---|---|---|
| `??0Engine@0@QEAA@PEAX@Z` | 0x1800dbf00 | 174 |
| `??1Engine@0@QEAA@XZ` | 0x1800dbec0 | 586 |
| `?Startup@Engine@1@QEAAIPEBDII_N@Z` | 0x1800db8a0 | 4094 |
| `?Shutdown@Engine@1@QEAAXXZ` | 0x1800db4f0 | 4047 |

`standalone/rt_alloc_swap.cpp:55-80` already records that MSVC **inlined**
`MemoryAllocator::Startup` into `Engine::Startup`, and that 0x1800db905 is **the
only place the arena is ever created**. So the Wave 8 reason — do not take over
the allocator, there is no oracle for it — is a statement about ORACLE
AVAILABILITY, and at cutover our allocator is the only one there is. The
constraint plausibly **inverts** from "must not" to "must". Lane ENG owns
re-deriving that rather than inheriting it; this wave has now had four holdback
blockers turn out stale or already answered elsewhere in the tree, so it is
being checked, not assumed.

## Residue composition, for whoever reads this next

61 distinct. Roughly: 4 Engine (the wall), 24 UI frame-loop and widgets, 9
Graphics, 6 Video::Device/Context, 13 IO/Object/Audio/Input, 2 unidentified
ordinal-only rows (`ord:3214`, `ord:4462`) — an ordinal in this list is a symbol
**nobody has named yet**, which is why lane IOA was asked to name them first.
