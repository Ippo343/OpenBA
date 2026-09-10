# WAVE 15 KICKOFF — the 20 `_singleton` data rows

**Scoped 2026-09-09, not dispatched.** Written by the Wave 14 integrator off the
binaries and the merged tree. Read this before scoping the lane; it exists so
the next lane does not re-derive what Wave 12 and Wave 14 already established.

## The finding that motivates the wave

There are **20 `?_singleton@` exports** in Runtime-x64.dll. All 20 are minted in
`standalone/rt_data_mint_tables.inc`. Until today **ZERO were promoted**.

That number was worth checking twice: a naive grep returns 1, because
`reg_mint_promoted.cpp` contains a COMMENTED-OUT registration left behind as a
standing case. Comment-stripped, the live count was 0. It is now 1.

| state | count | rows |
|---|---|---|
| promoted | **1** | `SaveLoad@Save` (Wave 14, at the ceiling) |
| formal verdict, never promotable | **1** | `Application@1` — `UNPROMOTABLE \| HARMFUL`, Wave 12 |
| deferred by a different mechanism | **1** | `MemoryAllocator@System` — behind the opt-in `BANISHED_RT_ALLOC` gate |
| **no written verdict at all** | **17** | everything else |

The 17 are the actual scope. They are not "deferred" — nobody has ruled on them.

## Why this matters at cutover, in one paragraph

These rows are the exe's doors to engine state. The exe imports **15 of the 20**
and reaches them through its import table — e.g. it reads `SaveLoad`'s at 20
`.text` sites, every one through slot `0x140151610`. An imported row therefore
HAS a door we can intercept. The other 5 are read only by in-DLL RIP-relative
loads, which are **not** doors and can never be redirected. At cutover every one
of these 20 needs a decision, and getting one wrong is not a census cosmetic: a
row is a LIVE MUTABLE POINTER, so a wrong promotion strands real readers.

## Per-row size, calibrated

The table below is a crude upper-bound scan (every disp32 at every byte offset,
5 instruction lengths, so it overcounts) **calibrated against two exact counts**:

* `SaveLoad` — scan 20, true **5** (lane SINGLETON, two independent routes)
* `Application` — scan 130, true **26** (Wave 12)

Both give a factor of 4–5. Treat `~real` as an estimate, and **never** quote it
as a measurement — the point of the column is the RANKING, not the value.

| row | scan | ~real | exe door | note |
|---|---|---|---|---|
| `MemoryAllocator@System` | 10935 | ~2700 | **no** | ~500x the next tier; behind the alloc gate |
| `Device@Video` | 825 | ~206 | yes | |
| `ExternalFactory@Object` | 755 | ~188 | yes | |
| `FileSystem@IO` | 455 | ~113 | yes | |
| `Configuration@Video` | 305 | ~76 | yes | |
| `Application@1` | 130 | 26 exact | yes | **UNPROMOTABLE \| HARMFUL** |
| `Persistence@IO` | 120 | ~30 | yes | |
| `Configuration@Input` | 110 | ~27 | yes | |
| `Factory@Object` | 100 | ~25 | yes | |
| `MainLoop@Engine` | 100 | ~25 | yes | |
| `AnimationManager@Graphics` | 90 | ~22 | **no** | |
| `Device@Input` | 85 | ~21 | yes | |
| `Modification@IO` | 45 | ~11 | yes | |
| `Device@Audio` | 40 | ~10 | **no** | |
| `Configuration@Audio` | 35 | ~8 | yes | |
| `Configuration@System` | 30 | ~7 | **no** | |
| `Engine@1` | 30 | ~7 | **no** | |
| `InternalFactory@Object` | 30 | ~7 | yes | |
| `AchievementManager@Achievement` | 20 | ~5 | yes | joint smallest |
| `SaveLoad@Save` | 20 | 5 exact | yes | **DONE — promoted** |

**Wave 14 did the joint-smallest row in the set.** That is worth saying plainly
so nobody reads its cost as typical. `AchievementManager` is the only other row
of that size; the next tier up is 4–6x, and `MemoryAllocator` is ~500x.

## The triage question is NOT the reference count

Count is a proxy for effort. The question that decides PROMOTABLE is:

> Of this row's in-DLL RIP-relative references, how many are reads in bodies we
> have NOT ported?

Because a ported body reaches the slot through `RT_DATA_SLOT`/`ResolveExport`
— the door — and moves with the promotion. An unported body's RIP-relative load
cannot move, and promoting strands it on a slot nothing writes.

That is exactly the difference between the two rows that already have verdicts:

    Application  26 refs, 24 of them READS IN UNPORTED BODIES   -> HARMFUL
    SaveLoad      5 refs, 4 in ported bodies + 1 in a DEAD one  -> SAFE

> **CORRECTION 2026-09-09 (Wave 15 lane INSTR, measured).** The Application line
> above is WRONG and it was wrong the day it was written. `WAVE12_APPLICATION.md`
> says *"26 in-DLL RIP-relative references, 2 WRITE and 24 READ"* — a count of
> KINDS. This kickoff transcribed it as "24 of them READS IN UNPORTED BODIES",
> silently converting a kind-count into a PORT-STATE count. Wave 12 never made
> that claim; grep it and you will not find it.
>
> Measured, the split is **9 reads in unported bodies, not 24.** The other 15
> owners are ported AND registered — 14 of them on 2026-09-08 (`7ded21a`,
> `070b546`, `a3de8bd`), the day BEFORE Wave 12 closed (`06624c0`), and the 15th
> (`?Get@Application@1@`) by Wave 12's own lane A (`74110c0`). The nine are
> `Update@DialogLoaderLoopT`, `Update@LoaderLoop`, `PlatformShutdown@Device@Input`
> (x3), `PlatformGetKeyboardState@Device@Input`, and
> `PlatformGetMouseState@Device@Input` (x3).
>
> **The HARMFUL verdict SURVIVES; only this stated reason for it does.** Wave 12's
> actual argument never rested on the reader split — it is that we intercept only
> at DOORS, so all 26 in-DLL RIP-relative sites keep reading the DLL slot no
> matter what is ported, because a ported body's copy INSIDE the DLL still holds
> its own load. That argument is independent of port state and still holds.
>
> The lesson is the one the reference count made easy to miss: a measured number
> (26/2/24, which reproduces exactly) sat beside an asserted one, and lent it its
> credibility. Phase 2 classifies from measurement and quotes this line from
> nobody.

So the lane's real unit of work is **classifying each row's readers**, not
counting them.

## The method, already proven — reuse it, do not reinvent

Lane SINGLETON's Wave 14 enumeration is the template. Per row:

1. **Enumerate references by TWO routes that could disagree** — Ghidra
   `get_xrefs_to`, and a decoder-independent disp32 scan. When they disagree,
   find out why before proceeding: theirs disagreed 4-vs-5 because
   `MOV qword ptr [rip+d],0` measures RIP past the imm32. A second route only
   helps if it can actually disagree, and that one did.
2. **Classify every reference** R/W, and the owning body as ported+registered,
   unported, or dead.
3. For any reference in an UNPORTED body, the row is HARMFUL unless that body is
   proved dead. Prove deadness FROM THE BINARIES, closing all four routes:
   in-DLL call sites; import by name in the exe and both video DLLs; **import by
   ORDINAL** (note: no binary imports anything from Runtime by ordinal at all —
   this closes the route rather than failing to find an instance);
   `GetProcAddress` by name (check whether the name STRING exists anywhere
   outside the export directory).
4. **Name the route you cannot enumerate.** `GetProcAddress(h, MAKEINTRESOURCE(n))`
   has no string to find. Say so; do not pretend it is closed.
5. **Check pointer-valued references to the ACCESSOR too**, not only to the slot.
   Wave 14's lane had not done this until asked, and it was the right challenge.
6. **Do not accept a comfortable explanation.** "Dead in shipping, live in the
   ToolKit build" was offered to lane SINGLETON by the integrator and it went and
   DISPROVED it — zero references in the profile DLL either.

## Ordering is load-bearing, and it is not luck

Everything that resolves these names does so after `RtDataMintInit()` (inside
`RuntimePortInit()`); `AddressRegistryInit()` and `EngineFuncsInit()` both run
later. A promotion landing after either would strand the exe's readers on a slot
nothing writes. Verify per row rather than assuming — lane SINGLETON verified it
by MEASUREMENT (two independent sites agreeing on our storage), not by reading
`main.cpp`.

Related hazard, already documented in `runtime/funcs/rt_resolve_cache.cpp` §5:
a function-local `static T* p = ResolveExport(...)` freezes the FIRST answer,
**including a non-null one**, so any site whose first resolve runs before
`RuntimePortInit()` keeps the DLL's address forever while everyone else moves.
`SaveLoadSingletonSlot()` is still a plain frozen static rather than
`RtResolveCached` — harmless today, and a natural pickup for this lane.

## Suggested shape

* **Do NOT scope this as "promote the 17."** Scope a DECOMPOSITION lane first
  that classifies readers for the cheap tier and reports which rows are even
  candidates. Wave 11's kickoff makes the same recommendation for the object
  creation path and it was right.
* Cheap tier, all with exe doors and ~5–27 estimated refs: `AchievementManager`,
  `InternalFactory`, `Configuration@Audio`, `Modification`, `Device@Input`,
  `MainLoop`, `Factory@Object`, `Configuration@Input`, `Persistence`.
* The 4 no-door rows (`AnimationManager`, `Configuration@System`, `Engine`,
  `Device@Audio`) need a different argument entirely — with no import slot there
  may be nothing to promote INTO. Establish that before spending a lane on them.
* `MemoryAllocator` is its own wave and interacts with the allocator gate. Do
  not fold it in.

## What Wave 14 leaves you

* One row promoted and MEASURED with the DLL present (`SaveLoad`), including the
  exe-side door demonstrably following it.
* One row with a written `HARMFUL` verdict (`Application`) — the worked example
  of a row that must never move.
* The `RT_UNPROMOTABLE` block in `reg_mint_promoted.cpp`, which is the census's
  source of truth and already distinguishes `NO-OP` from `HARMFUL`.
* **The DLL-ABSENT boot is still unmeasured for every one of these rows**,
  including the promoted one. That is the configuration in which all of this
  actually matters, and it is the strongest argument for doing the boot BEFORE
  promoting anything else.
