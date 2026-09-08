# Cutover boot attempt 07 — 2026-09-08, integrator

Tip with lanes VID (`104beaf`), UIF (`739d0ab`) and IOA (`3a8ca7f`) merged.
Lanes ENG and GFX were still running, so **the Engine wall was expected and is
not this attempt's result.**

## Outcome

| | 06 | 07 | 07b (after the ordinal fix) |
|---|---|---|---|
| residue, distinct | 61 | 52 | **50** |
| crash | none | none | none |
| distance | `GameMain` returns 1 | same | same |

`standalone_crash.log` is zero bytes in all three. The wall is still the four
`Engine` symbols, which is exactly right — lane ENG had not landed.

## What this attempt was FOR, and it delivered something no audit could

Three lanes made falsifiable residue predictions. All held **except two rows**,
and those two are the finding.

**`ord:3214` and `ord:4462` were still null on a tree where the functions they
name are PORTED.** Lane VID ported `?Lock@Device@Video@@` and
`?Unlock@Device@Video@@` and registered both by name and by address. Lane IOA had
identified those two ordinals — and corrected my premise while doing so: an
`ord:` row does not mean *unidentified*, it means *resolved by ordinal*, and both
names were already sitting in `engine/funcs/engine_funcs.h:148` and `:153` as
comments.

The cause is structural: **`ResolveOrdinal` consulted only `RtOrdinalReg()`**, a
registry of its own. A port registered by NAME is invisible to an ordinal caller.
So the two functions were registered, census-counted, `rawdoor_audit`-clean and
`port_bypass_audit`-clean, and still a null call the moment the DLL is gone.

**Two lanes were each right and the combination was wrong.** One handed the
ordinals on as unported; the other ported the names and predicted they would
leave the residue list. Neither could see the door between them. Only running the
boot could, which is the argument for running it before it is expected to succeed.

Fixed (`644b770`) with a link-time ordinal→name map — seven rows, every name read
out of the shipped export directory with pefile, covering all six ordinals this
tree passes to `ResolveOrdinal`. A map rather than "every lane must also call
`RegisterRuntimeOrdinal`", because that is a rule people forget exactly once and
the failure is silent. An ordinal with no row now warns, once, by name.

## The residue arithmetic, since it does not add up naively

61 → 50 is 11 rows, against 20 predicted across three lanes. The difference is
not missing work: several predicted names (the eight Video list walkers,
`PlatformUpdate`) were never IN the 61, because they are unexported internals or
were reached by address and so never printed an `[RtUnresolved]` row to begin
with. **Residue counts what a run ASKED FOR BY NAME**, and a port can close a
real dependency without moving that number at all — the mirror of the
already-recorded fact that porting can GROW the `called` column.

## Still ahead, in order

1. `Engine` ctor/dtor/`Startup`/`Shutdown` — the wall (lane ENG).
2. `CreateVideoContext` (0x1800dd500) — unported AND unexported, a genuine
   EXEC-of-0 in the `MainLoop` ctor, deliberately left faithful by lane VID.
   Lane ENG has been told not to read a fault there as its own defect.
3. dx11's all-or-nothing import tables — 5 unsatisfied in `contextdata`, 17 in
   `devicedata`; one missing export disables every slot and faults on the first
   draw.
