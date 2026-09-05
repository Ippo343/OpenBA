# Unported exe `.text` functions — shared brief

Session 2026-09-04, branch `wave/upgrade-path-complete`, follows `f119f3b`.
**Read this fully before touching your lane.** Your lane list is
`docs/waves/lanes/code_lane<N>.txt`.

## What this wave is

49 addresses in the exe's `.text` range that `ResolveAddr(Module::Exe, addr)`
still misses, so every call to them lands on `NotImplementedStub_impl` and
**returns 0**. Unlike the dispatch-slot band we just closed, these have no PE
import name — each one needs a Ghidra decompile to identify.

The list came from the union of `docs/waves/lanes/held_back_code_fns.txt` (49)
and the `BANISHED_AUTOLOAD=Auto` census (48).

**CORRECTION 1 (integrator error, found by lane 1).** The held-back file is a
stale pre-`a8a24ba` artifact and the census is authoritative. `0x1400192a0` is
ALREADY REGISTERED (`standalone/reg_unreg_lane1.cpp:220`, in `a8a24ba`, an
ancestor of `f119f3b`); the census correctly omitted it and the union wrongly
resurrected it. **The real total is 48.** Regenerate from the census alone.

**CORRECTION 2 (integrator error, found independently by lanes 1 and 6).** This
brief originally said LOAD-WINDOW rows "are the highest-value ones: a null
returned there can silently shorten the save stream." **That over-claims and is
withdrawn.** `NotImplementedStub_impl` is invoked from inside `ResolveAddr`
itself (`standalone/resolve_standalone.cpp:176`), on a resolution MISS — not from
a per-address thunk when the null is called. So:

- `LOAD-WINDOW` / `first=load` means only "the one-time `*_InitFuncs()` that
  resolves this address happened to run during load".
- `total=N` is a RESOLVE count, not a call count. `total=1` is the normal shape
  for a one-time init. A high `total` means the resolve sits in a loop.
- It is **not** evidence the resulting null is ever invoked, and **not** evidence
  the address is on a Serialize path. Check whether it actually is one.

Weight a row by what its CALL SITE does (unguarded call > null-guard-and-skip >
null-guard-and-fallback), never by its census tag.

## Your deliverable is a TRIAGE REPORT first, code second

The single most important lesson from the previous two waves on this list:

> Of 18 rows audited, only 6 were real gaps; **5 would have caused ACTIVE HARM
> if implemented as written.**

So do not assume an unregistered address is a bug. For each address in your lane,
decide and report one of:

- **`port`** — a real gap. Our callers want this function and get 0 instead.
- **`deliberate — leave`** — a caller null-guards and takes a fallback path on
  purpose, or the function is genuinely a no-op for us. Say which call site.
- **`dead`** — nothing actually calls it (see the `.pdata` trap below).
- **`needs a run to decide`** — you cannot settle it statically. Say what run.

A well-argued `leave` with the call site quoted is worth as much as a port. A
guessed port is worth less than nothing.

## Method, per address

1. `get_function_by_address(address="0x...")` then `decompile_function`.
   The exe is already the current Ghidra program — see the concurrency rule below.
2. `get_function_xrefs` / `get_xrefs_to` to find retail's callers. That tells you
   what the function is FOR, which is usually more informative than its body.
3. **Grep the ADDRESS across `code/`, `engine/`, `standalone/`, `runtime/`** to
   find OUR consumers. Read every one before deciding.
4. Classify the consumer:
   - **null-guard-and-SKIP** → registering RESTORES retail. Usually `port`.
   - **null-guard-and-FALLBACK** → registering CHANGES behaviour. Argue for it or
     leave it.
   - **unguarded call** → currently an EXEC-of-0 waiting to happen. High priority.
5. If you port it, write the reimpl in **your own file** (below) and say exactly
   which one-line `RegisterExeAddr` the integrator must add.

## Traps that have burned this project before

- **Grep the retail ADDRESS, not our `fn_*` name.** 8 of 18 of our own `fn_*`
  names were found to be misnomers. A name match is not evidence.
- **A lone `[DATA]` xref inside `.pdata` (`0x1401e1000`-`0x1401f33ff`) means DEAD
  CODE, not "called from a table".** That xref is the function's own unwind
  record. "It has a real body" is not evidence anything calls it.
- **An unregistered address is sometimes DELIBERATE.** Prior sessions left slots
  null so a dispatch-or-fallback path takes the fallback. Registering silently
  changes behaviour.
- **A skipped serializer call is a stream desync our own save/load round trip can
  NEVER detect**, because both legs skip the same bytes. If your function is
  reached from a Serialize path, treat a `leave` verdict with extra suspicion.
- **A stub's "why this is stubbed" comment outlives the blocker it names.** If you
  find a comment explaining why something is skipped, verify the claim still
  holds rather than trusting it. Two stale comments were corrected in this branch
  this session alone.
- **Do not "fix" a faithful crash with a null check.** If a byte-faithful port
  crashes on an unguarded deref, something upstream handed us a null — find that.
  See CLAUDE.md.

## Working rules — SIX LANES SHARE ONE TREE

- **Do NOT build.** One object dir; a parallel `mingw32-make` corrupts it. The
  integrator builds centrally.
- **Do NOT run the game.** One Banished directory, one process, shared logs.
- **Put all your code in your own new file** `code/Game/<Module>/unreg_lane<N>_*.cpp`
  or, for registration-only work, `standalone/reg_code_lane<N>.cpp`. Tell the
  integrator the one line needed to call your init.
- **Do NOT edit shared files** — `standalone/address_registry.cpp`,
  `engine/funcs/engine_funcs.{h,cpp}`, anything already under `code/Game/` that
  you did not create. If your fix needs one, describe the exact edit in your
  report and the integrator applies it serially.
- **Ghidra concurrency: do NOT call `switch_program`.** It changes global state
  for all six lanes at once. Pass `program="Runtime-x64.dll"` as a parameter on
  the individual call instead. The exe is the current program; leave it that way.
- Comment every registration the way the existing ones do: what was broken with
  it unregistered, and the evidence. A bare address+name line is not enough.

## Report back

A table: address, what the function IS (name it), verdict, the call site you
read, and what changes if it is registered. Then the diff summary of your own
file. **Flag anything you could not settle rather than guessing.**

---

# RESULTS — integrated and verified 2026-09-04

All six lanes reported. Integrated, built clean, and verified with
`BANISHED_AUTOLOAD=Auto BANISHED_UNREG_CENSUS=600`.

**Census: 85 -> 55 unregistered addresses. Load reached gameplay, no crash.**
(`docs/waves/unreg_census_20260904_postwave.txt`)
Exe `.text` band specifically: 48 -> 18.

## The premise was wrong, and that is the finding

This wave was framed as "49 unported functions to port". Of 48 real addresses
(see the list correction above), only **4 were genuine gaps**. The rest were
already ported and statically wired, or are deliberate leaves where registering
would cause ACTIVE HARM.

| verdict | count |
|---|---|
| real gap, fixed | 4 |
| dead / vestigial `fn_*` resolve | ~23 |
| deliberate leave (7+ would cause active harm) | ~18 |
| already registered before the wave | 1 |
| needs a run to decide | 1 |
| **identified but NOT fixed (see below)** | **1** |

Registering blind would have double-driven the camera every frame, double-built
the pathfinder and leaked it, double-written fog against the driver, deselected
the player's selection every frame, and double-inserted 8 tool-mode AVL nodes.

## Real gaps fixed
- **`0x14006bd80` `MapParamsDialog::MapParamsDialog`** (lane 2) — the in-game
  **Map button was dead**. `mainmenudialog.cpp:351` calls it UNGUARDED; the stub
  returned 0, so `AddDialogToUI` wrapped a null IBP and the button did nothing.
  The dialog was fully ported and had a correct open path with ZERO callers —
  only the headless harness reached it. An inverted null test: *the harness* was
  the null test while the real button constructed nothing.
- **`0x14001aea0` `MapGenerator::~MapGenerator`** (lane 1) — null-guard-and-SKIP.
  `MapGenerator_Cleanup` calls `Dtor(self,0)` then re-runs Init, so with the body
  skipped it **overwrote live array headers instead of releasing them**.
- **`0x1400a40e0` `MusicPlayer::Update`** (lane 5) — the only writer of music
  state and the only music-channel `PlayEffect` caller, so background music never
  played. Lane 5 did NOT dismiss the "MusicTrackList deferred" blocker comment at
  `gameupdateloop_load.cpp:1249` as stale; it performs the deferred
  `Find("Game/MusicTracks.rsc")` itself in `EnsureTrackList` and caches into
  `+0x08`. **Gated opt-in on `BANISHED_LIVE_MUSIC`** — registration is verified,
  but the feature is UNPROVEN and off by default. Needs an opt-in run.
- 10 register-only ports (lanes 4 and 6) that make existing reimpls reachable.

## `0x140095da0` — live crash GUARDED (not yet ported)
`GameUpdateLoop::TakeScreenshot`, **not** the "QuickSave handler" that
`reg_mods.cpp:30` named it (name now corrected). Integrator-verified chain:
`fn_QuickSave` resolves it and gets null; both call sites
(`gameupdateloop.cpp:2060`, `:2135`) are **UNGUARDED**; flags come from
`KeyMap::Pressed(0x21)`/`(0x22)`; `CHECK_FN` only traces.

**Pressing quicksave or quickload in-game was an EXEC-of-0 in the default build.**

**Interim guard applied** at both call sites (`gameupdateloop.cpp`). This is the
documented CLAUDE.md exception, not a papered-over faithful crash: retail
dereferences nothing here — its `.rdata` slot is loader-filled, so retail's
pointer is non-null and its unguarded call is correct. Ours is null solely
because the address is unregistered, i.e. our build genuinely differs from retail.
The guard also **clears both flags**, because retail's `TakeScreenshot` is their
only clearer (the 2-byte store to `self+0xC0`) and nothing else in the tree writes
them — verified: the only writers are the two `= 1` in `Update`. Left set, the
branch re-enters every frame forever and would fire a screenshot the instant the
real port lands, for a keypress made minutes earlier.

**Lane 4 deliberately declined to port it, and was right to.** `TakeScreenshot`
constructs a live GPU object, dispatches two virtuals through an exe vtable, walks
the filesystem and calls four unverified Runtime methods. Under the lane rules
(no build, no run) it could not compile or exercise a line of it, and binding an
unverified body there would replace a null-pointer crash with a wrong-ABI crash
inside D3D — strictly worse and harder to diagnose.

### Full port spec (lane 4) — every dependency is a named PE import
| piece | slot | symbol |
|---|---|---|
| ctor | `0x140151f40` | `??0FrameCopyImageBuffer@Graphics@@QEAA@XZ` |
| dtor | `0x140151f48` | `??1FrameCopyImageBuffer@Graphics@@UEAA@XZ` |
| vtable +0x48 | `0x1401520b0` | `Create@FrameCopyImageBuffer` |
| vtable +0x50 | `0x1401520b8` | `Generate@FrameCopyImageBuffer` |
| save dir | `0x1401520c0` | `GetSaveDirectory@FileSystem@IO` |
| exists | `0x1401520d0` | `Exists@FileSystem@IO` |
| write | `0x1401520e0` | `SaveImageToFile@ImageBuffer@Video` |

Object size `0x88` (`MemAlloc(0x88, 8)`). `Create` args from the call site:
`(0, 4, 5, 0x100, screenW @ singleton+4, screenH @ +8, 1,1,1,1, 0)`;
`Generate(videoCtx, 1)`; pixel buffer `MemAlloc(w*h*4, 4)` →
`ImageBuffer::CopyFromTexture` → `SaveImageToFile` → `MemFree`. Path is
`<GetSaveDirectory()>\Screenshots\Screenshot<N>.bmp` — **bmp, not png**
(`DAT_14015e518` = `62 6d 70 00`); N increments from 0 until `Exists` is false.
The exe vtable `0x14015e370` is 12 slots: slot 0 is exe code (deleting dtor),
slots 1-11 are import thunks to the Runtime's own virtuals, so the vptr overwrite
is cosmetic — `Create`/`Generate` can go through the Runtime-installed vptr or
`ResolveExport`. `minimap_paint.cpp:149-154` is the working precedent for the
whole alloc/ctor/vptr/`*(vt+0x48)` dance. **Delete the interim guard when this
lands.**

### Verification status of the guard
Compiles clean; the load path reruns to gameplay with no crash and the census is
unchanged at 55. The keypress path itself was **not** exercised — driving
`KeyMap::Pressed(0x21)` needs input injection the harness does not have. The
guard is correct by inspection (both call sites null-tested, flags cleared), but
"quicksave no longer crashes" is reasoned, not observed.

## Housekeeping applied
- Deleted **10 vestigial `fn_*` pointers** (8 lane 3 + 2 lane 1), each verified
  first: exactly 3 source mentions, no dereference, single `ResolveAddr` site.
- Integrator ruling for the tree: **zero consumers -> DELETE; any real consumer,
  including `_DEBUG`-only or feature-gated -> REGISTER.** Lanes 3 and 4 had
  recommended opposite treatments of the same shape.
- Added `MapParamsDialog_GetStandaloneVtable()` to `mainmenudialog.cpp` — the
  vtable is in an anonymous namespace, so it cannot be `extern`'d across TUs.

### Measurement note
Deleting `fn_CitizenList_ShuffleEntities` removed the only GUARANTEED census
signal for `0x140089470`. It is still (correctly) unregistered — the seat shuffle
is opt-in — but its other resolver (`town_workplace_registry.cpp:64`, `EnsureFns`)
is lazy and did not run in the verification window, so the address no longer
appears. Absence from the census is now weaker evidence for that one address.

## Still open
1. `0x140095da0` — the live quicksave/quickload crash above. **Highest priority.**
2. `0x140002280` `Entity::FindComponent<AmbientEmitterComponent>` — needs a run.
   The unguarded retail deref requires the description to declare **>= 2**
   emitters. Run `BANISHED_AMBIENT_TRACE=1` after raising the 16-line cap at
   `ambientemittercomponent.cpp:283`.
3. `BANISHED_LIVE_MUSIC=1` run to prove music actually plays.
4. 18 exe `.text` addresses remain, 6 in the load window.
5. Stale comment: `entity_finders_disaster.cpp:50` blames an
   `AmbientEmitterComponent::OnAdded` that has been live since `1b7a50b`.
6. Latent (unreachable today): `SubArrayDeepCopy`
   (`serialize_struct60b.cpp:76-82`) zeroes `dst->data`/`dst->count` but not
   `dst->capacity`; retail clears count AND capacity as one qword.

### No second consumer of `+0xC0`/`+0xC1` — verified at instruction level (lane 4)
Checked rather than asserted, because the interim guard clears both flags and a
hidden second consumer would be masked by that.

**Retail.** Disassembled `GameUpdateLoop::Draw` (`0x140096010`) and filtered for
accesses to those bytes. Exactly two touch `RDI` (the `GameUpdateLoop*`):
```
1400961b7: CMP byte ptr [RDI + 0xc0],0x0    -> guards the first TakeScreenshot call
140096341: CMP byte ptr [RDI + 0xc1],0x0    -> guards the second
```
Both are compares, not writes. **Trap to not trip over:**
`1400962c0: MOV EAX,dword ptr [RCX + 0xc0]` looks like a third access and is not —
`RCX` there is `[RDI + 0xe0]`, the camera/player-controller block being bulk-copied
to the stack. Different object, coincidental offset.

So retail's lifecycle is closed with three participants: `Update` sets (from
`KeyMap::Pressed` 0x21/0x22), `Draw` tests, `TakeScreenshot` clears.

**Our tree.** Exhaustive grep returns five sites and no others: two writes
(`:1645`, `:1648`), two reads (`:2058`, `:2133`), one read in a `TRACE` format
(`:1651`), plus the struct field and its `static_assert`. No raw-offset `+0xC0`
access on a `GameUpdateLoop*` — the other `0xc0` hits are a `frustum[0xC0]` array,
`mapGen+0xc0`, a `worldGeom+0xc0` dim field and a serialized geo float, all
unrelated objects.

**Scope of the claim (residual risk, stated honestly):** retail's `Draw` and our
tree were both checked exhaustively; all 10,476 retail functions were NOT scanned
for a stray write to some other path's `GameUpdateLoop*+0xC0`. The loop object is
not published anywhere that would reach in and set a screenshot flag, and Ghidra
shows `0x140095da0` with only those two callers.
