# Wave 9, Lane C (prep) — Data harvest

Scope: harvest, from the still-present `C:\GOG Games\Banished\Runtime-x64.dll`,
what every exported DATA symbol Wave 9 lane C owns actually contains, before
cutover removes the only place this ground truth exists (WAVE9.md §2). No
build, no game run, no port. Tooling and CSV only.

**Tool**: `tools/rt_data_harvest.py`. **Data**: `docs/runtime_port/wave9_data_harvest.csv`
(271 rows — one row per vtable slot / one row per singleton / one row per
`BaseTypeInfo`).

## Counts harvested

| Kind | Count |
|---|---|
| `??_7` vtables | **28** |
| `?_singleton@…` | **18** |
| `?_typeInfo@…` (`BaseTypeInfo`) | **14** |
| **Total exported DATA** | **60** |
| Phantoms (named in the source list, absent from the DLL's export directory) | **0** |

**Correction to the counts this lane was handed.** The task brief and WAVE9.md
both say "17 singletons"; the CLAUDE.md index row says the same. The source
list (`docs/runtime_port/wave9_unported_20260907.txt`) actually names **18**
distinct `?_singleton@…` symbols with no duplicates — `IO::FileSystem`,
`Video::Configuration` and `Video::Device` are all present alongside the ones
a first manual grep found. 28 + 18 + 14 = 60, not 59. Every one of the 60
resolved to a real RVA in the DLL's export directory — **zero phantoms** in
this set (`export_name_audit.py`'s failure mode does not apply here: these are
resolved by the standalone address registry against a known DLL address /
export name, not typed out by hand at each call site the way `ResolveExport`
names are).

## Method and independent verification

Primary source: `pefile` reading `Runtime-x64.dll` **on disk**. Export
directory gives each symbol's RVA; a vtable's slots are qword pointers,
resolved back to an export name via the same directory, or left as
`FUN_<addr>` (lane D's pool) when unexported. Cross-referenced against
`docs/runtime_port/census_20260907_prewave9.csv` to mark each slot's target
`ported` / `not ported` / `untracked` (present in neither the called nor
ported columns — the tree has never touched it at all).

**Spot-checked 3 vtables against live Ghidra memory** (`Runtime-x64.dll` open
in the shared Ghidra project) rather than trusting the file read alone:
`??_7Achievement@0@6B@` (9 slots), `??_7Geometry@Graphics@@6B@` (9 slots),
`??_7DialogController@UI@@6B@` (11 slots). `read_memory` at each vtable's VA
returned bytes **identical, to the byte, to the static file read** in all
three cases — confirmed by hand for Achievement and Geometry. Two of the
resolved slot addresses were also independently checked with
`get_function_by_address`, which found real (if unnamed) functions at both,
matching the harvest's own classification.

**A vtable's bytes are meaningful in the file; a singleton's are not.**
`?_singleton@…` reads as **empty (`b''`) from `pe.get_data`** for every one of
the 18 — the address falls in an uninitialised region with no file backing,
because the pointer is only ever written at runtime. Do not read that as "the
singleton is null" or "not yet resolved"; it means "ask the live process, not
the file". `??_7` vtables and `?_typeInfo@…` BaseTypeInfo, by contrast, ARE
fully populated on disk, because every field the constructor stores
(string/function addresses) is a link-time constant with no runtime
computation — confirmed against `runtime/types/object/TypeInfo.h`'s existing
`BaseTypeInfo` layout (see below).

**One real finding inside the "verification" step itself, corrected once measured
properly.** `??_7Geometry` slots 1, 2 and 3 all point at the same address,
`0x18007ec28` — confirmed three times over in the live memory read, not a
harvest bug. This report first guessed that was compiler ICF/COMDAT folding of
three identical virtual-method bodies. **That guess was wrong, and checking it
against all 28 vtables (not just the one that prompted it) is what caught it:**
`0x18007ec28` is **`_purecall`**, the standard MSVC CRT pure-virtual-call
trap (`get_function_by_address` names it directly; decompiling it shows the
CRT's `DecodePointer` / `abort` sequence, not application code). A slot
pointing there is not "two methods sharing one body" — it is a **pure virtual
method the concrete class never overrides**, and retail relies on it never
being called on an instance of that type, exactly the same shape as
`reference_checkptr_is_a_ret_in_every_build.md` (a slot that "works" only
because nothing calls it). See the vtable-wide measurement below for how
common this is; it changes the count of real functions lane B still owes, not
just Geometry's.

## Vtable ranking — cheapest first (lane B's dependency-ordered work list)

Columns: `slots` = total decoded slots; `ported` = slots whose target is
already ported in our tree; `unp-named` = named export, called by our tree,
NOT yet ported; `untracked` = slot's target isn't in our `called` set at all
(neither called nor ported — an unknown quantity, could be trivial or could be
new work); `unnamed` = target has no export name and is NOT a known CRT thunk
(lane D's real `FUN_` pool); `thunk` = target is a known CRT trap
(`_purecall` — see below), already understood, not lane D work. The
"not yet ported" total driving the sort is `unp-named + untracked + unnamed`
— **`thunk` slots are excluded from the ranking**, which is the correction
this table went through once the shared-address finding was measured properly
(see below): they looked like `unnamed` work in this table's first draft and
are not.

| Rank | Vtable | slots | ported | unp-named | untracked | unnamed | thunk | not-ready |
|---|---|--:|--:|--:|--:|--:|--:|--:|
| 1 | `??_7Geometry@Graphics@@6B@` | 9 | 5 | 0 | 0 | 1 | 3 | 1 |
| 2 | `??_7ImageBufferData@Video@@6B@` | 7 | 0 | 0 | 0 | 1 | 6 | 1 |
| 3 | `??_7RenderStateData@Video@@6B@` | 3 | 0 | 0 | 0 | 1 | 2 | 1 |
| 4 | `??_7Thread@System@@6B@` | 2 | 0 | 0 | 1 | 0 | 1 | 1 |
| 5 | `??_7Base@Object@@6B@` | 3 | 0 | 0 | 1 | 1 | 1 | 2 |
| 6 | `??_7Stream@IO@@6B@` | 7 | 0 | 0 | 1 | 1 | 5 | 2 |
| 7 | `??_7Internal@Object@@6B@` | 4 | 1 | 0 | 2 | 1 | 0 | 3 |
| 8 | `??_7AchievementInstance@Achievement@@6B@` | 4 | 0 | 0 | 3 | 1 | 0 | 4 |
| 9 | `??_7AnimationEvent@Graphics@@6B@` | 9 | 0 | 1 | 2 | 1 | 5 | 4 |
| 10 | `??_7CompiledExternal@Object@@6B@` | 8 | 0 | 1 | 2 | 1 | 4 | 4 |
| 11 | `??_7External@Object@@6B@` | 8 | 0 | 1 | 2 | 1 | 4 | 4 |
| 12 | `??_7ParticleInstance@Graphics@@6B@` | 10 | 4 | 0 | 4 | 1 | 1 | 5 |
| 13 | `??_7Goal@Achievement@@6B@` | 10 | 1 | 2 | 3 | 1 | 3 | 6 |
| 14 | `??_7GoalInstance@Achievement@@6B@` | 7 | 1 | 1 | 4 | 1 | 0 | 6 |
| 15 | `??_7Particle@Graphics@@6B@` | 9 | 0 | 1 | 4 | 1 | 3 | 6 |
| 16 | `??_7ParticleSystemInstance@Graphics@@6B@` | 10 | 4 | 0 | 5 | 1 | 0 | 6 |
| 17 | `??_7DialogInstance@UI@@6B@` | 10 | 3 | 0 | 6 | 1 | 0 | 7 |
| 18 | `??_7UpdateLoop@Engine@@6B@` | 12 | 0 | 0 | 6 | 1 | 5 | 7 |
| 19 | `??_7Achievement@0@6B@` | 9 | 1 | 2 | 5 | 1 | 0 | 8 |
| 20 | `??_7CompressedMemoryStream@IO@@6B@` | 9 | 1 | 1 | 6 | 1 | 0 | 8 |
| 21 | `??_7DialogLoopT@Engine@@6B@` | 13 | 3 | 0 | 8 | 1 | 1 | 9 |
| 22 | `??_7Element@UI@@6B@` | 12 | 2 | 0 | 8 | 1 | 1 | 9 |
| 23 | `??_7ImageBuffer@Video@@6B@` | 10 | 1 | 1 | 7 | 1 | 0 | 9 |
| 24 | `??_7MaterialInstance@Graphics@@6B@` | 9 | 0 | 1 | 7 | 1 | 0 | 9 |
| 25 | `??_7SpriteSheet@UI@@6B@` | 9 | 0 | 0 | 8 | 1 | 0 | 9 |
| 26 | `??_7DialogController@UI@@6B@` | 11 | 1 | 0 | 9 | 1 | 0 | 10 |
| 27 | `??_7DialogLoaderLoopT@Engine@@6B@` | 13 | 2 | 0 | 9 | 1 | 1 | 10 |
| 28 | `??_7LoaderLoop@Engine@@6B@` | 12 | 0 | 0 | 11 | 1 | 0 | 12 |

**Reading this**: `Geometry` is now unambiguously rank 1, not rank 6 — 9
slots, 5 already ported, 0 untracked, 3 `_purecall` (understood, not work),
and exactly 1 slot that is genuinely unnamed and unmeasured. Excluding
`_purecall` from the "work remaining" count is what moves it, along with
`ImageBufferData` and `RenderStateData` (6 and 2 of their slots are
`_purecall` respectively) — all three looked far more expensive in this
table's first draft than they actually are. The Engine loop family
(`UpdateLoop`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT`) is still the
most expensive tier and should still be scheduled last — matching WAVE9.md's
own note that `Engine::MainLoop::Run` is allocator-gated (lane E) anyway —
though `UpdateLoop` (rank 18) is cheaper than the other three once its 5
`_purecall` slots are set aside.

**A caution about `untracked`**: it does not mean "harmless". It means the
census has never seen the tree call that address, so its size and difficulty
are unmeasured — some are trivial one-line accessors, some could be another
`Input::Device::Update`-shaped oracle problem. Treat `untracked` as
"unscoped", not "small".

## Pure-virtual (`_purecall`) slots, measured across all 28 vtables

The Geometry finding above generalises, and measuring it changes lane B's real
function count, not just Geometry's. Grouping every vtable's slots by target
address and looking for duplicates within the same vtable:

**10 of the 28 vtables have at least one repeated-address slot group; all 30
of the duplicate slots across all 10 point at the exact same address,
`0x18007ec28` — `_purecall`. There is no other folded/shared address anywhere
in the 28 vtables.** That single fact is what overturned the original ICF
guess: real ICF folding would produce several distinct shared addresses (one
per group of identical bodies); one address shared everywhere is the
signature of a language-level trap, not a code-size optimization.

**Full sweep, all 28 vtables** (`fold_group` column in the CSV; groups are
slot indices sharing one target address within that same vtable — every group
found resolves to `_purecall`, so the "slots" count that group occupies minus
1 is real work removed from the count, not hidden work):

| Vtable | total slots | fold groups (slot indices) |
|---|--:|---|
| `??_7AnimationEvent@Graphics@@6B@` | 9 | [3, 5, 6, 7, 8] |
| `??_7CompiledExternal@Object@@6B@` | 8 | [3, 5, 6, 7] |
| `??_7External@Object@@6B@` | 8 | [3, 5, 6, 7] |
| `??_7Geometry@Graphics@@6B@` | 9 | [1, 2, 3] |
| `??_7Goal@Achievement@@6B@` | 10 | [6, 7, 8] |
| `??_7ImageBufferData@Video@@6B@` | 7 | [1, 2, 3, 4, 5, 6] |
| `??_7Particle@Graphics@@6B@` | 9 | [6, 7, 8] |
| `??_7RenderStateData@Video@@6B@` | 3 | [1, 2] |
| `??_7Stream@IO@@6B@` | 7 | [2, 3, 4, 5, 6] |
| `??_7UpdateLoop@Engine@@6B@` | 12 | [1, 2, 3, 4, 6] |
| `??_7Achievement@0@6B@` | 9 | none |
| `??_7AchievementInstance@Achievement@@6B@` | 4 | none |
| `??_7Base@Object@@6B@` | 3 | none |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 | none |
| `??_7DialogController@UI@@6B@` | 11 | none |
| `??_7DialogInstance@UI@@6B@` | 10 | none |
| `??_7DialogLoaderLoopT@Engine@@6B@` | 13 | none |
| `??_7DialogLoopT@Engine@@6B@` | 13 | none |
| `??_7Element@UI@@6B@` | 12 | none |
| `??_7GoalInstance@Achievement@@6B@` | 7 | none |
| `??_7ImageBuffer@Video@@6B@` | 10 | none |
| `??_7Internal@Object@@6B@` | 4 | none |
| `??_7LoaderLoop@Engine@@6B@` | 12 | none |
| `??_7MaterialInstance@Graphics@@6B@` | 9 | none |
| `??_7ParticleInstance@Graphics@@6B@` | 10 | none |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 10 | none |
| `??_7SpriteSheet@UI@@6B@` | 9 | none |
| `??_7Thread@System@@6B@` | 2 | none |

**18 of the 28 have no folding at all.** Of the 10 that do, every single
fold group in the whole sweep resolves to `_purecall` — there is no case in
this DLL where two ORDINARY (non-thunk) functions were folded together by the
linker. That is itself worth recording: it means lane B/C do not need to
worry about the "port 3 functions, find they're bit-identical, and have to
decide whether that's one implementation or three" problem this report
originally worried about — it never occurs here. Every fold is the CRT trap,
already priced into the ranking table above.

**What this means for lane B/C's function count**: every `_purecall` slot is
NOT a function to port. It is retail's compiler-inserted proof that the
concrete class relies on that base-class virtual method never being invoked
on this type — calling it aborts the process (the decompile of
`0x18007ec28` shows the CRT `_purecall` handler: `DecodePointer` on a
`SetInvalidParameterHandler`-style hook, then an `abort`). Minting these 10
vtables needs the *same trap* in that slot, not a reimplementation, and
getting one wrong (pointing it at a real function, or at null) is invisible
until something actually calls through that slot — which, by definition,
retail never does. `tools/rt_data_harvest.py` now special-cases
`0x18007ec28` (`KNOWN_INTERNAL_THUNKS`) so the CSV's `slot_name` reads
`_purecall` directly instead of `FUN_18007ec28`, and the ranking table above
already has the correction folded in — a `thunk` column pulled out of
`unnamed`, which is why `Geometry`, `ImageBufferData` and `RenderStateData`
moved to ranks 1-3.

**No overlap with lane D**: `0x18007ec28` does not appear in
`wave9_internals.csv` or the unported census list, so `_purecall` is not
sitting in lane D's 66-function `FUN_` pool as a phantom porting job — it is
purely this lane's concern.

### Minting requirement: what those 30 slots must point at, and why

This is a decision for whoever mints the 28 vtables, not analysis, but it
needs to be made once here rather than rediscovered per-class: **`0x18007ec28`
is CRT (MSVC's Universal CRT pure-virtual-call handler), not Runtime.** We are
not porting the DLL's copy — MinGW/UCRT already ships its own `_purecall`
(declared in `<corecrt_startup.h>` / linked from UCRT), so the mechanical
option is to point the slot at that.

**Recommended instead: a deliberate trap of our own** — log the class name and
slot index, then abort — rather than silently reusing UCRT's anonymous one.
The reasoning is the same shape as the `next`-chain caveat above: retail's
guarantee is "this slot is never called", and that guarantee was true for
*retail's* code, not automatically for a young reimplementation still finding
its bearings. If a port bug ever calls through one of these 30 slots on a
type that should never take that path, a named trap ("pure-virtual call on
`Graphics::Geometry` slot 1 — this should be unreachable, the vtable was
mis-minted or a wrong-typed pointer was used") turns an anonymous abort into
a defect report that says exactly which vtable and which slot to fix, instead
of a bare crash address someone has to work backward from. Cost is one small
shared helper function, called from up to 30 slots across 10 vtables — far
cheaper than the alternative failure mode (silence, or a crash that has to be
re-diagnosed back to "which vtable, which slot" the hard way, which is
exactly what this harvest exists to avoid needing).

## Singletons — 8 already resolved, 10 new

PLAN §2 states 8 exported data singletons are already resolved: `MainLoop`,
`Video::Device`, `FileSystem`, `InternalFactory`, `SaveLoad`,
`AchievementManager`, `Input::Device`, `Input::Configuration`. Checked against
all 18 found here — **all 8 are present, confirmed**:

- `?_singleton@MainLoop@Engine@@1PEAV12@EA`
- `?_singleton@Device@Video@@0PEAV12@EA`
- `?_singleton@FileSystem@IO@@0PEAV12@EA`
- `?_singleton@InternalFactory@Object@@0PEAV12@EA`
- `?_singleton@SaveLoad@Save@@0PEAV12@EA`
- `?_singleton@AchievementManager@Achievement@@0PEAV12@EA`
- `?_singleton@Device@Input@@0PEAV12@EA`
- `?_singleton@Configuration@Input@@0PEAV12@EA`

**"Already resolved" means the pointer lookup works, not that the storage is
minted.** These 8 still need their backing object minted at cutover exactly
like the other 10 — PLAN §2 is about the resolve mechanism, not about the
data existing without a DLL.

**10 are genuinely new work**, not previously called out anywhere in PLAN or
WAVE9.md:

- `?_singleton@Application@1@0PEAV11@EA`
- `?_singleton@Configuration@Audio@@0PEAV12@EA`
- `?_singleton@Device@Audio@@0PEAV12@EA`
- `?_singleton@AnimationManager@Graphics@@0PEAV12@EA`
- `?_singleton@Modification@IO@@2PEAV12@EA`
- `?_singleton@Persistence@IO@@0PEAV12@EA`
- `?_singleton@ExternalFactory@Object@@0PEAV12@EA`
- `?_singleton@Factory@Object@@0PEAV12@EA`
- `?_singleton@MemoryAllocator@System@@0PEAV12@EA`
- `?_singleton@Configuration@Video@@0PEAV12@EA`

`Object::Factory`'s singleton is already documented in
`runtime/types/object/TypeInfo.h` (Runtime `0x1801fc6e8`) as being wrongly
described in PLAN §2.3 as having "no export" — that was fixed once already;
this harvest just re-confirms it is one of the 18 by its real export name.
`Video::Configuration`'s consumer list includes a `dx11/` source file, so it
is part of the cutover surface `dx11/` adds, not only the exe/runtime tree.

## `BaseTypeInfo` (14) — file bytes ARE meaningful except `next`

All 14 decode cleanly against the existing `RtObject::BaseTypeInfo` layout in
`runtime/types/object/TypeInfo.h` (0x38 bytes: `typeId`, `name`, `parentName`,
`fileExtension`, `factoryFn`, `inPlaceFactoryFn`, `next`). Example,
`?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A`:

```
typeId           = 0xFFFFFFFF   (ctor sentinel, matches TypeInfo.h's documented behaviour)
name             = "Achievement"        (resolved from the pointer, read as a C string)
parentName       = "Base"
fileExtension    = null
factoryFn        = <export name if the DLL exports it, else FUN_<addr>>
inPlaceFactoryFn = <same>
next             = 0x0          (NOT baked in -- see below)
```

**`typeId`, `name`, `parentName`, `fileExtension`, `factoryFn` and
`inPlaceFactoryFn` are link-time constants and read correctly straight out of
the file** — no DLL load is needed to see them, because they are string and
function addresses the linker already knows. **`next` is the one field that
is NOT in the file**: it reads `0x0` for every one of the 14, because the
intrusive `_firstTypeInfo` list is only built by running the constructor at
DLL load (head-insert order depends on static-initializer order, which is a
runtime fact). Minting a `BaseTypeInfo` at cutover can copy every field except
`next` straight from this harvest; `next` has to be rebuilt by our own
static-init order the same way retail's linker order built it — get that
order wrong and only `Factory::Startup`'s parent-chain walk (which follows
`next`) breaks, silently, at boot.

Full per-field dump for all 14 is in the CSV (`kind=typeinfo` rows,
`slot_name` column carries the decoded dict).

### Checked against the tree's own hand-declared struct — no live defect

The `next`-is-runtime-only finding matters more than a caveat: it means
minting `BaseTypeInfo` is a static-init ORDER problem in our exe, not a copy
job, and PLAN §2.2 B already has code walking
`?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` through a hand-declared
mirror of this struct at three call sites (current line numbers as of this
check — two other lanes are active on this branch and lines below the struct
declarations have shifted slightly from an earlier draft of this section):

- `standalone/main.cpp:66-79` — `struct ProbeBaseTypeInfo` declaration.
  Consumed at `standalone/main.cpp:545` (the `IsDerivedFrom` NULL-`this` crash
  handler) and `standalone/main.cpp:566` (the `External::Load` tree-walk crash
  handler) — both walk `p->next` to search the chain.
- `code/Game/Game/game_main.cpp:486-492` — a second, locally-scoped
  `struct Probe`, used by the post-`Engine::Startup` factory-tree dump right
  where it's declared.

Both structs declare the same eight fields in the same order as
`RtObject::BaseTypeInfo` (`runtime/types/object/TypeInfo.h`) and this
harvest's decode: `typeId(u32)+pad(u32)` at +0x00, `name` +0x08, `parent`
+0x10, `fileExtension` +0x18 (both comments already correctly call out that
this field is the file extension, not an "abstract" flag — that misnomer was
fixed by Wave 3 lane A and neither struct still uses it), `factoryFn` +0x20,
`inPlaceFn` +0x28, `next` +0x30, for a total 0x38 bytes. **All three consumer
sites agree with the DLL's actual layout, field for field and offset for
offset. This is not a live defect** — the existing code is correct today; the
risk this harvest surfaces is prospective (minting time, rebuilding `next`
from our own static-init order), not present.

## Phantom check

**Zero phantoms.** Every one of the 60 symbols in the source list resolved to
a real RVA in `Runtime-x64.dll`'s export directory. This differs from the
`ResolveExport("...")` phantom-name failure mode `export_name_audit.py`
guards against (a hand-typed mangled name that was never in the DLL) — these
60 names came from the DLL's own export table via the census in the first
place, so the interesting question was never "does the name exist" but
"does the census's source-scan agree with the export table's RVA", which it
does in all 60 cases.

## What contradicts WAVE9.md

1. **Singleton count is 18, not 17** (see above) — both WAVE9.md's table and
   the lane brief undercount by one. 28 + 18 + 14 = **60** exported DATA
   symbols, not 59. **Fixed in place**: WAVE9.md's counts table and lane-C
   row, and PLAN.md §4.2's Wave 9 row, now say 18/60 and name the fix.
2. **This report's own first-pass explanation of the Geometry shared-slot
   finding was wrong**, and is corrected above: not compiler ICF/COMDAT
   folding of identical bodies, but `_purecall` (MSVC's pure-virtual-call
   trap) — measured across all 28 vtables, not just the one that surfaced it.
   30 slots across 10 vtables are `_purecall`, all the same address, which is
   itself the tell that it is a language-level trap and not a code-folding
   artifact.
3. Everything else — the 28 vtable figure, the "PLAN §2's 8 already-resolved
   singletons" claim, the "no phantoms in this bucket" expectation, and the
   agreement of the tree's own `BaseTypeInfo` mirror struct with the DLL —
   checks out exactly as WAVE9.md states.

## Handoff to lane B / lane D

- Lane B: mint `Thread`, `Base`, `Internal`, `RenderStateData` and `Geometry`
  first (ranks 1-6) — cheapest, and `Geometry`'s already-ported 5-of-9 is the
  best proof this can work class-by-class as WAVE9.md prescribes. Defer the
  Engine loop family (`UpdateLoop`/`LoaderLoop`/`DialogLoopT`/`DialogLoaderLoopT`)
  to last — largest, most untracked, and gated on the allocator anyway.
- Lane D: every `unnamed` slot in `wave9_data_harvest.csv` (`slot_name` starts
  `FUN_`) is a named call site now — a vtable slot index is a much stronger
  clue to a function's purpose than an anonymous internal caller. **Skip the
  30 rows where `slot_name=_purecall`** (across 10 vtables, `fold_group`
  column non-empty) — that's a known CRT trap, not lane D work. See "Minting
  requirement" above for what those 30 slots should point at instead: not a
  reimplementation, and preferably not UCRT's own anonymous `_purecall`
  either — a named trap that logs the class/slot before aborting.
- Whoever mints `BaseTypeInfo` structs: copy the harvest's decoded fields
  verbatim except `next`, and re-derive `next` from OUR static-init order —
  do not copy the file's `0x0` and assume it will be fixed later.
