# Cutover boot attempt 08 — 2026-09-08 — THE ENGINE WALL FELL

Tip with lane ENG (`c1f006a`) merged, plus the ordinal bridge (`644b770`) and the
String allocator-door fix (`f2d65d1`). Runtime-x64.dll renamed aside, restored
after.

## Attempts 04–07 all stopped at the same line. This one did not.

```
[GameMain] Constructing Engine (storage=00000087ecbff130)...
[GameMain] Engine constructed.
```

That second line has never appeared in a DLL-less run before. **Distance
travelled moved for the first time in five attempts**, which is the runbook's
progress metric and the one that stayed flat while residue fell from 75 to 50.

## Lane ENG's prediction was confirmed in every particular

It committed, before any run, to all four of these:

| predicted | observed |
|---|---|
| the four `Engine` names leave `[RtUnresolved]` | they did |
| the log reaches `Engine constructed.` | it did |
| `Engine::Startup` does **not** finish | it did not |
| first fault: `?SetCommandLine@Configuration@System@@`, EXEC-of-0 symbolising **inside `RtEngine_Engine_Startup`** | exactly that |

```
[RtUnresolved] ?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z  (asked by 00007ff61d258405)
Fault: EXEC of 0000000000000000
  #1  reimpl-x64.exe + 0x2BB5CC   ->  RtEngine_Engine_Startup +0x17a
  #2                 + 0x19B9CD   ->  Game::RunGameMain +0x25d
  #3                 + 0x341013   ->  WinMain +0x20c
```

It reached that prediction by checking all 65 of `Startup`'s dependencies against
every `RegisterRuntimeExport` literal in the tree — 26 ported, 39 not — and naming
which one the control flow reaches first. **That is a genuinely predictive model
of the boot, not a post-hoc reading**, and it is the first time this wave has had
one.

## The blocker was stale AND inverted, and the inversion was the opposite of my guess

Four symbols had been held since Wave 8 under "allocator, NO-GO". Every fact in
that holdback is still true; the verdict was not. I had guessed the inversion was
*"a ported `Startup` will create the arena a SECOND time on top of an already-armed
allocator"*. **Wrong: arming does not create an arena at all.** `[RtAlloc] ARMED`
registers 27 export *names* and stores two function pointers. Nothing is
constructed, nothing reserved. So the port MUST create the arena and nothing else
does — established three ways (a measured `live=0 ranges=0` at `Startup` entry, a
rel32 scan finding zero callers of `MemoryAllocator::Startup`, and `pageSize==0`
after `Construct` making an earlier allocation structurally impossible).

## Also fixed on the way here, both found BY this boot and by no audit

* **The ordinal door had no bridge to the name registry** (`644b770`).
  `Device::Lock`/`Unlock` were ported, registered by name *and* address,
  census-counted and clean on every audit — and `ord:3214`/`ord:4462` were still
  null, because `ResolveOrdinal` consulted only its own registry.
* **`String`'s allocator door was pinned to the DLL** (`f2d65d1`). `RtMemFree`
  resolved through `RtOriginal<>()`, which skips the port registry by design, so
  every `String`-using port on the boot path freed through a null — on a tree where
  `MemFree` is ported. The mirror of this wave's usual failure: the port exists
  and the door refuses to use it.
* **`-lole32` was missing** from `SA_LIBS`. Lane ENG's `Startup` calls
  `CoInitializeEx` as retail's does, and **a lane verifying with `-fsyntax-only`
  cannot see a missing import library at all** — this failed only at link, in the
  integrator's build. That is a real limit of the no-build lane contract, not a
  lane error.

## Next, in order

1. `?SetCommandLine@Configuration@System@@` and the other 38 unported `Startup`
   dependencies. This is now a *known, enumerated* list rather than a search.
2. `CreateVideoContext` (0x1800dd500) — unported and unexported, downstream of
   `Startup`, deliberately left faithful by lane VID.
3. dx11's all-or-nothing import tables — 5 unsatisfied in `contextdata`, 17 in
   `devicedata`; one missing export disables every slot and faults on first draw.
