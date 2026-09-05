# Unregistered dispatch-slot wave — shared brief

Session 2026-09-04, branch `wave/upgrade-path-complete`. Read this before
touching your lane.

## The core discovery

`CLAUDE.md` claims Application-x64.exe has no PE import table and uses "custom
runtime dispatch". **That is wrong.** The exe has a completely normal PE import
table: 833 imports from Runtime-x64.dll (slots `0x140151218`-`0x140152c18`) and
66 from KERNEL32.dll (`0x140151000`-`0x140151208`). Our
`RegisterDispatchSlot(addr, ResolveExport(name))` pairs ARE that table.

So for any unregistered address in that span the correct name is already known —
no Ghidra archaeology needed:

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py 0x140151458
tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py            # dump all
tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py --check    # validate our registrations
```

`--check` validated 59 of our 62 existing registrations. Use it after you add
yours; it must stay clean.

## The rule that makes this NOT a blanket sweep

An unregistered address is **sometimes deliberate**. Prior sessions left slots
unregistered on purpose so a dispatch-or-fallback path would take the fallback
(e.g. `Vector2::Max` 0x140152780 in `reg_component_economy.cpp`,
`reg_destroy_workers.cpp`). Registering one of those silently changes behaviour.

**Before registering anything, read our call site** and answer in your report:
does the consumer null-guard-and-fall-back (registering CHANGES behaviour — argue
for it or leave it), or null-guard-and-skip (registering RESTORES retail — safe)?
A skipped serializer call is a stream desync that our own save/load round trip
can never detect, because both legs skip the same bytes.

## Measurement available to you

`BANISHED_UNREG_CENSUS=<frames>` (added this session,
`standalone/resolve_standalone.cpp`) records per-address, per-phase hit counts
and dumps a sorted table. Phases: `boot` -> `load` -> `postload` -> `gameplay`,
set from `gameupdateloop_load.cpp` and `gameupdateloop.cpp`.

Latest run (autotutorial load of tutorial 1): 157 distinct unregistered
addresses; **only 34 are resolved inside the load/postload window.** That set is
in `docs/waves/unreg_census_20260904.txt`; regenerate the view with
`tools/ghidra_mcp/.venv/Scripts/python.exe tools/unreg_loadpath.py docs/waves/unreg_census_20260904.txt`.

Known limit: the census records where an address is *resolved*, not where the
resulting (null) pointer is *called*. A slot lazily resolved at boot and called
during the load shows as `first=boot`. So the 34 is a floor, not a ceiling.

## Working rules for this wave

- **Do not build.** All four lanes share one working tree and one object dir; a
  parallel `mingw32-make` will corrupt the build. I build and verify centrally.
- **Do not run the game.** One Banished directory, one process, shared logs.
- **Put all your code in your own new file** `standalone/reg_unreg_<lane>.cpp`
  (registered from wherever the other `reg_*.cpp` waves are invoked — follow the
  existing pattern and tell me the one line I need to add to call your init).
- **Do not edit shared files** (`standalone/address_registry.cpp`,
  `engine/funcs/engine_funcs.{h,cpp}`, anything under `code/Game/`). If your fix
  needs one, describe the exact edit in your report and I apply it serially.
- Comment every registration the way the existing ones do: what was broken with
  it unregistered, and the evidence. A bare address+name line is not enough.
- Ghidra MCP is available; the exe is the current program, `program=
  "Runtime-x64.dll"` for engine functions.

## Report back

For each address: name, verdict (`register` / `deliberate — leave` / `needs a
run to decide`), the call site you read, and what changes if it is registered.
Flag anything you could not settle rather than guessing.

---

## A second defect class found mid-wave: a slot read under the WRONG NAME

Lane 3 turned this up and it changes the register/leave calculus. Some sites read
an import slot into a variable named for a *different* function. They work today
**only because the slot is unregistered**, so the read yields null and control
falls through to a `ResolveExport`-by-name fallback. Register the slot and the
consumer is silently rebound to an unrelated function.

Confirmed cases (all verified against the PE import table and by reading the code):

| site | reads slot | into | PE table says that slot really is |
|---|---|---|---|
| `engine_funcs.cpp` ~3313 | `0x1401527f8` | `fn_DialogInstance_GetRoot` | `ParticleInstance::SetParticle` (real GetRoot is `0x1401517a8`) |
| `engine_funcs.cpp` ~3325 | `0x140152830` | `fn_AudioScene_PlayEffect` | `Achievement::Goal::Goal()` (variable has zero callers - dead) |
| `engine_funcs.cpp` 3486 | `0x140151a60` | `AddController` fallback | `Element::AddChild` (dead branch) |
| `address_registry.cpp` | `0x140152400` / `0x140152618` | registered SWAPPED | see below |
| `address_registry.cpp` | `0x1401518f0` | wrong `GetAlignedPosition` overload | instance, not static |

The two swapped serializer slots are NOT harmless: Ghidra shows both are thin
forwarders to the Encoding object (`serializer+8`) but to **different vtable
slots** - `Serialize(int,uchar*,name)` -> `+0x60`, `SerializeBytes(int,void*,name)`
-> `+0xC8`.

**Consequence for this wave: before registering any slot, grep its address across
`code/`, `engine/` and `standalone/` and confirm every consumer wants the same
function.** One consumer wanting it is not enough.

## Tooling status - what to trust

- `tools/dispatch_slot_map.py` - the slot -> PE symbol map. Trustworthy.
- `tools/dispatch_slot_map.py --check` - **exact** checker for the
  `RegisterDispatchSlot(addr, ResolveExport("name"))` form, because the name is
  literally in the source next to the address. This is what found the swap. Keep
  it clean.
- `tools/slot_name_audit.py` - a **REPORT, not a checker**: the 196 distinct PE
  import slots our port reads, each with the exe's symbol and the files reading
  it. Automatic name-vs-address matching was attempted and **abandoned** - our
  target names are hand-abbreviated (`iatIBPSer` for `InternalBasePtr::Serialize`,
  `s_fn_Vec3Ctor` for `??0Vector3`) and intent often sits in a comment lines away
  from the call. Every heuristic tried (context words, assignment target, prefix
  matching, ctor/dtor decoding) produced heavy false positives AND still missed
  all five known cases. Do not reintroduce a scoring heuristic here; review the
  list instead.

---

## The save/load desync -- CLOSED 2026-09-04 (`63a0f76`)

**Cause: a vtable slot-3 (Serialize) wrapper that never called its
`*_InitFuncs` resolver.** `InternalFactory::PersistObject` dispatches slot 3
immediately on create, so on a load it is the FIRST call into that class in the
process. Every `s_fn_*` the body null-guards is still null, every stream call is
skipped, and the record consumes ZERO bytes past its 13-byte `InternalBasePtr`
header -- while the save leg, hundreds of frames old with the resolver long
since run, wrote the full record. Symmetric code, asymmetric STATE, which is why
grepping for an asymmetric branch found nothing for days.

Measured on `ParticleComponent`: **save 71 bytes, load 13.** Four occurrences
found in total (`StateMachineComponent` 2026-08-28, `ParticleComponent`,
`CemeteryComponent`, `WellComponent`); the last two were latent. Audit with
`tools/ghidra_mcp/.venv/Scripts/python.exe tools/slot3_resolver_audit.py`
(exit 1 on a hit) and keep it clean.

The method that found it: `+0x1C` cursor diff per component on BOTH legs of a
MATCHED build pair, `BANISHED_SERIALIZE_TRACE=1`, correlated in ONE log. Two
runs against two different saves mis-index and send you after the wrong
component.

### Reproduction (still valid for regressions)

```bash
BANISHED_AUTOSTART=1 BANISHED_SAVE_TEST=1200      # writes ~/Documents/Banished/Save/Auto.sav
BANISHED_AUTOLOAD=Auto BANISHED_PERSIST_TRACE=1
```

`BANISHED_AUTOTUTORIAL=1` is NOT a repro: it loads the retail-authored tutorial
`.sav`, which loads cleanly, and it also refuses the autosave trigger
(`GetOperation()=0`). A whole census was collected against that working path
before this was noticed. `BANISHED_SAVE_TEST` OVERWRITES `Auto.sav`.

### Hypotheses eliminated on the way (keep this; it is the expensive part)

1. **A skipped serializer call because slot 3 was not INSTALLED.** Dead for
   `ParticleComponent` -- slot 3 is installed unconditionally outside the live
   gate, into one shared static array reached by both factory entry points,
   since `a11d083`. The defect was one level down: installed, but reaching a
   body whose resolver never ran.
2. **The two swapped Serializer slots (`0x140152400` / `0x140152618`).** Dead.
   Both forward to the same Encoding object; `+0x60` is an N-iteration one-byte
   loop and `+0xC8` is one N-byte call. Wire-identical under the binary encoding.
   (Still worth un-swapping for clarity.)
3. **The crop-field slot (P3).** Real defect, but symmetric -- skipped on BOTH
   legs -- so invisible to a same-build round trip. Tutorial 1 has no crop fields.
4. **A stale save / cross-build mismatch.** Dead. Regenerating the save with the
   current binary and loading it with that same binary reproduced the failure
   BYTE-IDENTICALLY, in a different world of a different size.
5. **A CompressedStream window that is never refilled.** Dead. The load stream is
   an `IO::CompressedMemoryStream`, and `CompressedStream::Read` (Runtime
   `0x1800aa780`) calls `SetWindow` on EVERY loop iteration, so a boundary
   crossing refills by construction.
6. **The save leg writing from an uninitialised component slot.** Dead, and it
   cost the most: the claim rested on a bad measurement ("per-component byte
   counts on load match the save leg exactly"). The header of
   `standalone/entity_comp_validator.cpp` asserted this as established fact and
   was believed by two independent lanes. It has been corrected in place.

## A principle this wave established

A loader must CONSUME the bytes the writer emitted, whether or not the data is
reconstructible. "It gets rebuilt during startup anyway" justifies DISCARDING a
value after reading it; it never justifies SKIPPING the read. A stale comment in
`particlecomponent.cpp` made exactly that error and was removed.

## Related open item

8 components install slot 3 INSIDE a `LiveEnabled("BANISHED_LIVE_*")` gate.
Serializer wiring is not a feature flag; see the hoist item in the wave opens.

---

## Re-census 2026-09-04 on the AUTOLOAD path (85 addresses)

The earlier 157/84 censuses were taken on `BANISHED_AUTOTUTORIAL`, which loads a
retail-authored save. This one is `BANISHED_AUTOLOAD=Auto` + `BANISHED_UNREG_CENSUS=600`
on a save this build wrote -- i.e. the path the desync lived on. It also served as
the regression test for the slot-3 hoist below: save leg wrote, load leg reached
gameplay clean.

Raw dump: `docs/waves/unreg_census_20260904_load.txt`
Annotated: `docs/waves/unreg_census_20260904_load_annotated.txt`
(regenerate the annotated view with `tools/unreg_full.py <dump>`; `unreg_loadpath.py`
prints ONLY the load-window rows, which is why it looks so much shorter)

| category | count | in load window |
|---|---|---|
| exe `.text` functions | 48 | 13 |
| PE import slots | 20 | 2 |
| `.rdata` vtable/const | 12 | 3 |
| `.data` globals | 5 | 4 |

**The import-slot band is effectively CLOSED.** Of the 21 import slots the first
run showed, 17 were already audited by lane 2 as no-op leaves (every consumer
resolves the same function via a `ResolveExport` / Runtime-address fallback), 2
are the documented read-under-the-WRONG-name leaves (`0x1401527f8`,
`0x140152830`), and the remaining 2 were settled here:

- **`0x1401514a0` `MeshInstance::MeshInstance(const&)` -- REGISTERED.** A real
  gap, not a deliberate leave. Its default-ctor (`0x140151490`) and dtor
  (`0x1401514a8`) siblings sit adjacent in `address_registry.cpp` and were both
  registered; the copy-ctor was simply missed. Its only consumer,
  `StorageDisplayComponent_MeshArrayResize`, null-guard-and-SKIPs: with the slot
  null it reallocates the 0x128-byte MeshInstance array and copy-constructs
  nothing into the entries below `oldCount`, hand-copying only `sceneId` at
  `+0x120`, so every surviving entry is raw `MemAlloc` memory wearing a garbage
  vtable. Census confirms it drops out (86 -> 85) and the load still reaches
  gameplay.
- **`0x140152970` `GraphicsMesh::GetNodeIndexFromName` -- LEAVE (verified no-op).**
  Two consumers, and they do NOT conflict. `showtool.cpp` reads it under the
  correct name and already falls back to Runtime `0x1800fee90`, which the export
  table confirms IS `GetNodeIndexFromName` -- so registering changes nothing.
  `action_base_complete.cpp` carries a `fn_Vector4Ctor_t` typedef against this
  address and a "do NOT register" warning, but it hard-assigns
  `s_Vector4Ctor = nullptr` and never reads the slot, so its stated danger is
  stale; registering could not reach it. Left unregistered to honour the existing
  decision, since the gain is zero either way.

What remains is the expensive band: **48 exe `.text` functions** (each needs a
Ghidra decompile; 13 are touched during load), 12 `.rdata` consts and 5 `.data`
globals. Every row is `total=1` -- the census records where an address is
RESOLVED, not where the resulting null is CALLED, so these counts are a floor.

## Slot-3 hoist out of the LiveEnabled gate -- DONE 2026-09-04

The standing note claimed 8 components gate slot 3 behind `LiveEnabled`. **Five
of the eight did not**: `animal`, `herd`, `population`, `statemachine` and
`storage` already install slot 3 below their gate, unconditionally (storage's
even carries a comment saying why). Only the three that use the
`if (!LiveEnabled(...)) return;` early-return shape actually gated it:

| component | flag | was it inert? |
|---|---|---|
| `DiseaseComponent` | `BANISHED_LIVE_CROPDISEASE` | **YES** -- not in `kDefaultOnFlags`, so slot 3 was inert on every default run |
| `FireComponent` | `BANISHED_LIVE_FIRE` | no (flag default-on) -- but any `=0` A/B changed the stream shape |
| `TornadoComponent` | `BANISHED_LIVE_TORNADO` | no (same) |

All three now wire slot 3 above the gate. `tools/slot3_resolver_audit.py` stays
clean. This removes the A/B trap: a `BANISHED_LIVE_*=0` run no longer changes the
save-stream shape, so such a run can no longer "prove" something that was really
a desync.
