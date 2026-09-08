# Wave 9, Lane C2 — Minting the exported DATA symbols

Follows [WAVE9_LANE_C.md](WAVE9_LANE_C.md) (the harvest) and
[WAVE9.md](WAVE9.md) §2. No build and no game run in this lane: static tools,
`pefile` against the shipped DLL, and a `-fsyntax-only` check of every file
written. The integrator owns the build.

**Delivered**

| File | What it is |
|---|---|
| `tools/rt_mint_gen.py` | Generator. Reads the harvest CSV, **re-verifies it against the DLL**, emits the tables. |
| `standalone/rt_data_mint_tables.inc` | Generated. 28 vtables (239 slots), 14 `BaseTypeInfo`, 28 storage symbols. |
| `standalone/rt_data_mint.{h,cpp}` | The mint itself: runtime fill, named traps, the type chain, and the shadow check. |
| `standalone/resolve{.h,_standalone.cpp}` | `RtPortLookupExport` / `RtPortLookupAddr` — residue-free registry queries (see "the trap this lane nearly walked into"). |
| `standalone/main.cpp` | One line: a second shadow pass at exit. |

Registration is **off by default**. `BANISHED_RT_MINT=1` makes the minted data
live; nothing else in the tree changes behaviour.

---

## 1. The exported-DATA surface is 70 symbols, not 60

Lane C's harvest filtered the census dump on three **name prefixes** — `??_7`,
`?_singleton@`, `?_typeInfo@` — and got 28 + 18 + 14 = 60. Re-deriving the same
set by **PE section** instead (every unported export whose RVA is not in
`.text`) finds **70**. The ten the prefix filter could not see are all `.data`
statics with ordinary names:

| Symbol | Size | Why it matters |
|---|--:|---|
| `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` | 8 | **The head of the chain the 14 `BaseTypeInfo` mints live on.** Minting the nodes and not the head mints a list nobody can find. |
| `?_ticksPerSecond@Timer@System@@0_JA` | 8 | `System::Timer`'s calibration, written by `Timer::Startup`, read by every |
| `?_secondsPerTick@Timer@System@@0NA` | 8 | tick→seconds conversion in the engine. Four symbols, one mechanism. |
| `?_systemTicksPerSecond@Timer@System@@0_JA` | 8 | |
| `?_systemInitialTick@Timer@System@@0_JA` | 8 | |
| `?_errorString@Error@System@@0PADA` | 8 | `char*`, not a buffer — the 8192-byte gap to the next export is other, unexported statics. |
| `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` | 1 | already consumed by `code/Game/Component/citizenai_executor.cpp` |
| `?_frameIndex@ParticleInstance@Graphics@@1HA` | 4 | |
| `?_loader@LoaderLoop@Engine@@1VLoader@2@A` | 56 | an `Engine::Loader` **object** — see §6 |
| `?_rng@Particle@Graphics@@1VRandom@System@@A` | 2500 | a `System::Random` **object** — see §6 |

`_loader`'s size is corroborated twice over: `static_assert(sizeof(Loader) ==
0x38)` in `runtime/types/engine/UpdateLoop.h`, and the DLL's own export table
puts the next symbol exactly 56 bytes later.

This is not a criticism of the harvest — its brief was the three prefixes — but
"60" is the count of three prefixes and was being read as the count of exported
DATA. All 70 are minted here.

## 2. `_purecall` is 46 slots across 16 vtables, not 30 across 10

Lane C reported "30 of the duplicate slots across all 10 [vtables] point at
`0x18007ec28`". That counts **fold-group duplicates**: 10 groups holding 40
slots, minus one representative each. The number that matters for minting is
how many slots must receive the trap, and that is every slot pointing at
`0x18007ec28`: **46, across 16 vtables**.

The six missing from lane C's list each have exactly **one** `_purecall` slot,
so they formed no fold group and its duplicate-scan could not see them:
`Base@Object`, `Thread@System`, `Element@UI`, `ParticleInstance@Graphics`,
`DialogLoopT@Engine`, `DialogLoaderLoopT@Engine`. Lane C's ranking **table** is
unaffected — its `thunk` column already shows `1` for each of those six; only
the prose sentence under it undercounts. The consequence is in lane B's
favour: 46 slots are not functions anyone has to port, not 30.

## 3. Slot counts: verified two independent ways, zero suspects

A minted vtable that is one slot short does not fail to link — it dispatches
whatever follows in `.rdata`. The harvest decoded slot counts by stopping at a
zero qword or at the next exported symbol, which is a heuristic, so the
generator re-establishes every count:

* **13 of 28 by adjacency** — the last slot ends exactly where the next
  exported symbol begins. Exact, no inference.
* **15 of 28 by tail probe** — the qword one past the last slot is not a valid
  address anywhere in the DLL image (it is string data or zero), so the decode
  stopped on its own and not at the bound.
* **0 suspect.** `rt_mint_gen.py` refuses to emit a table whose count it cannot
  establish either way, and the shadow check re-runs the tail probe live.

The generator also re-reads all 239 slot target addresses from the DLL and
fails if any disagrees with the CSV. None do.

## 4. How the mint works, and why the tables are filled at runtime

The obvious design — bake `&OurGeometryClean` into a generated table — is
wrong here, for a reason worth stating once: **not one of the 28 vtables can be
completely filled today** (§5), so a baked table would need regenerating after
every lane-B port, and every regeneration is a chance to bake a stale address.

Instead the generated `.inc` records only the DLL's ground truth per slot
(*this slot holds function X, named or unnamed or `_purecall`*), and
`RtDataMintInit()` fills the actual table from the **port registry** after every
`*_PortInit()` has run. A slot becomes our code the moment lane B registers it,
with no source change and no regeneration.

**The rule this enforces: a minted slot holds our port or a named trap, never a
DLL address.** A slot quietly holding a DLL address works perfectly right up to
cutover and then faults with nothing to say. The trap prints the vtable, the
slot index and the mangled name of the function that should have been there —
which is the whole reason lane C recommended a named trap over UCRT's anonymous
`_purecall`, and it costs one generated stub per slot.

`BaseTypeInfo` is minted the same way: every field is a link-time constant
copied from the harvest **except `next`**, which reads `0x0` on disk for all 14
because the chain is built by running constructors. Ours is built from our own
order — and not arbitrarily: `Factory::Startup` resolves each type's parent by
name as it walks, so the chain is built so that **every parent precedes its
children in walk order** (a topological pass over the 14; parents outside the
set make a type a root). The mint does **not** head-insert into the DLL's live
`_firstTypeInfo`: the DLL's own static initialisers already registered these 14
types, and inserting duplicates would give `Factory::Startup` two nodes per
name. Our chain is separate until registration mode makes our `_firstTypeInfo`
storage the only one there is.

Singletons and the other `.data` statics are **not** given contents. All 28 are
BSS in the DLL — the harvest read `b''` from every one — so minting them means
owning zeroed bytes and letting the existing runtime writer fill them.

### The trap this lane nearly walked into

The shadow check asks the registry and the DLL about ~250 symbols per pass. The
first draft did that through `ResolveExport` / `ResolveExportRaw` /
`ResolveRuntimeRaw` — all three of which **record into the `BANISHED_RT_STRICT`
residue census**. It would have manufactured several hundred `[RtResidue]` and
`[RtRawResidue]` entries and destroyed the one measurement Wave 9's cutover
gate depends on: same shape as
`reference_truncated_tool_output_manufactures_calls_zero.md`, a tool inventing
the signal it is there to read. Hence `RtPortLookupExport` /
`RtPortLookupAddr`: registry queries that answer "is this ported" without
falling through to the DLL and without recording anything. Anything that wants
to *inspect* the registry rather than call through it should use them.

## 5. Per-symbol status

### Vtables — 28 minted, 0 complete

"ported" is what the registry holds **today** (cross-checked against
`tools/runtime_census.py`, 2026-09-07, post-round-2); it rises on its own as
lane B lands ports. "trap" is lane B's remaining worklist for that class.

| Vtable | slots | ported | trap | purecall | depends on |
|---|--:|--:|--:|--:|---|
| `??_7Geometry@Graphics@@6B@` | 9 | 5 | 1 | 3 | `Graphics::Geometry` dtor |
| `??_7ImageBufferData@Video@@6B@` | 7 | 0 | 1 | 6 | `Video::ImageBufferData` dtor |
| `??_7RenderStateData@Video@@6B@` | 3 | 0 | 1 | 2 | `Video::RenderStateData` dtor |
| `??_7Thread@System@@6B@` | 2 | 0 | 1 | 1 | `Thread::OnCompleted` |
| `??_7Base@Object@@6B@` | 3 | 0 | 2 | 1 | `Object::Base` |
| `??_7Stream@IO@@6B@` | 7 | 0 | 2 | 5 | `IO::Stream` |
| `??_7Internal@Object@@6B@` | 4 | 1 | 3 | 0 | `Object::Internal` |
| `??_7AchievementInstance@Achievement@@6B@` | 4 | 0 | 4 | 0 | `Achievement::AchievementInstance` |
| `??_7AnimationEvent@Graphics@@6B@` | 9 | 0 | 4 | 5 | `Graphics::AnimationEvent`, `External::Compile` |
| `??_7CompiledExternal@Object@@6B@` | 8 | 0 | 4 | 4 | `Object::CompiledExternal`, `External::Compile` |
| `??_7External@Object@@6B@` | 8 | 0 | 4 | 4 | `Object::External` |
| `??_7Goal@Achievement@@6B@` | 10 | 2 | 5 | 3 | `Achievement::Goal` |
| `??_7GoalInstance@Achievement@@6B@` | 7 | 2 | 5 | 0 | `Achievement::GoalInstance` |
| `??_7ParticleInstance@Graphics@@6B@` | 10 | 4 | 5 | 1 | `Graphics::ParticleInstance` |
| `??_7Particle@Graphics@@6B@` | 9 | 0 | 6 | 3 | `Graphics::Particle` |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 10 | 4 | 6 | 0 | `Graphics::ParticleSystemInstance` |
| `??_7DialogInstance@UI@@6B@` | 10 | 3 | 7 | 0 | `UI::DialogInstance` |
| `??_7UpdateLoop@Engine@@6B@` | 12 | 0 | 7 | 5 | `Engine::UpdateLoop` |
| `??_7Achievement@0@6B@` | 9 | 1 | 8 | 0 | `Achievement::Achievement` |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 | 1 | 8 | 0 | `IO::CompressedStream` |
| `??_7DialogLoopT@Engine@@6B@` | 13 | 3 | 9 | 1 | `Engine::DialogLoopT` + `UpdateLoop` |
| `??_7Element@UI@@6B@` | 12 | 2 | 9 | 1 | `UI::Element` |
| `??_7ImageBuffer@Video@@6B@` | 10 | 1 | 9 | 0 | `Video::ImageBuffer` |
| `??_7MaterialInstance@Graphics@@6B@` | 9 | 0 | 9 | 0 | `Graphics::MaterialInstance` |
| `??_7SpriteSheet@UI@@6B@` | 9 | 0 | 9 | 0 | `UI::SpriteSheet` |
| `??_7DialogController@UI@@6B@` | 11 | 1 | 10 | 0 | `UI::DialogController` |
| `??_7DialogLoaderLoopT@Engine@@6B@` | 13 | 2 | 10 | 1 | `Engine::DialogLoaderLoopT` + `UpdateLoop` |
| `??_7LoaderLoop@Engine@@6B@` | 12 | 0 | 12 | 0 | `Engine::LoaderLoop` |
| **total** | **239** | **32** | **161** | **46** | |

**Zero of the 28 are complete**, and the reason is uniform: **slot 0 is the
scalar deleting destructor in 27 of them** (`Thread` is the exception, whose
slot 0 is `_purecall`), it is unnamed in the export directory, and not one has
been ported. Ordinary `TypeName` / `Type` accessors are the other bulk. This
does not contradict the brief's "do not mint a vtable whose slots you cannot
fill" — it is that rule implemented per slot instead of per class: an unfilled
slot is a trap that names itself, so a partially-minted table cannot silently
dispatch into the wrong body. The alternative reading of the rule (mint nothing
until a class is complete) would have delivered **zero** vtables today and
nothing for lane B to measure against.

### `BaseTypeInfo` — 14 minted, all with trapped factories

All 14 decode cleanly and all 14 are minted with `name` / `parentName` (stripped
after the last `:`, exactly as the ported ctor does) / `fileExtension` /
`typeId = 0xFFFFFFFF` from the harvest, and `next` from our topological chain.

**All 28 factory pointers (14 × `CreateNewObject` + `CreateInPlaceObject`) are
traps**: not one is ported. That is the single biggest blocker in this lane —
`Factory::CreateObject` tail-jumps straight to `+0x20`/`+0x28`, so at cutover a
type whose factory is a trap cannot be constructed at all.

`Achievement`, `AchievementInstance`, `GoalInstance`, `GraphicsMesh`,
`Material`, `MaterialInstance`, `Particle`, `External`, `ExternalList`,
`ClipRegionElement`, `Dialog`, `ImageElement`, `NumberEditElement`,
`ScrollElement`.

### Storage — 28 minted (18 singletons + 10 `.data` statics)

All minted as zeroed, 16-byte-aligned storage. Contents are never compared
against the DLL (§7). Two need more than storage — §6.

## 6. Deferred, blocked, and what is *not* covered

1. **`?_loader@LoaderLoop@Engine@@1VLoader@2@A` and
   `?_rng@Particle@Graphics@@1VRandom@System@@A` need a CONSTRUCTOR at startup,
   not just storage.** Both are BSS in the *file*, which is what the harvest
   sees, but the DLL's CRT runs their constructors at load: `Loader`'s stores
   `??_7Loader@Engine@@6B@` into `+0x00`, `Random`'s seeds 624 words of MT19937
   state. Minting zeroed bytes reproduces the file, not the loaded DLL. After
   cutover, the first virtual call on `_loader` dispatches through a null
   vtable and `Particle::_rng` returns zeros forever. **Open — needs an owner.**
   No other storage symbol has this shape (singletons are pointers written on
   first use).
2. **Factory pointers, above.** Blocked on lane B porting `CreateNewObject` /
   `CreateInPlaceObject` for the 14 types.
3. **The 14 `BaseTypeInfo` are the 14 our tree CALLS, not every type the DLL
   registers.** The rest of the DLL's chain has no consumer in the census, so it
   is out of the "called" denominator — but if any DLL-internal code that
   survives to cutover walks the chain expecting to find a type nobody in our
   tree names, it will not be there. Recorded, not solved.
4. **The chain order is reasoned, not measured.** Parents precede children,
   which is what `Factory::Startup`'s by-name parent resolution needs. Retail's
   order is a linker artefact and is not recoverable; if `Startup` turns out to
   be order-insensitive (a second pass), this is simply harmless.
5. **Registration is off.** Turning `BANISHED_RT_MINT=1` on while the DLL is
   loaded splits the process's state: the DLL's own code keeps using the DLL's
   singletons while our callers get ours, empty. It is wired now so the last
   commit of the wave is a flag flip, not new code — but it must not be flipped
   before the DLL goes.

## 7. The shadow check — what it compares, and what it refuses to

`BANISHED_RT_MINT_SHADOW=1`. Runs twice: at `RtDataMintInit` (`phase=init`) and
at process exit from `standalone/main.cpp` (`phase=exit`), because which
singletons the process actually populates is itself ground truth that dies with
the DLL.

**Compared**

| | |
|---|---|
| every vtable's exported address | vs the harvest's VA |
| all 239 slots, live | DLL's qword vs the harvest's recorded target |
| every filled slot | ours == the registry's port of *that same function*, and **≠ the DLL's address** |
| slot count, live | the qword past the last slot must not be a DLL pointer |
| `BaseTypeInfo` `name`/`parentName`/`fileExtension` | by `strcmp`, not by pointer |
| `BaseTypeInfo` `factoryFn`/`inPlaceFactoryFn` | DLL's vs the harvest's VA |

**Deliberately not compared**

* **The minted vtable's bytes against the DLL's.** A filled slot holds *our*
  function's address, a different number by construction. A byte comparison
  fails on exactly the slots that represent progress and passes on the ones
  that do not. The comparison that means something is the *mapping*.
* **Any storage symbol's contents.** All 28 are BSS the DLL only writes at
  runtime — comparing them is
  `reference_oracle_must_not_compare_unwritten_bytes.md` verbatim, and returns
  100% green or 100% red while meaning neither. What the report prints instead
  is which of the 28 the running process has actually written, on both sides.
* **`BaseTypeInfo::next`.** The DLL's is built by its static-init order, ours by
  ours; they cannot agree and a mismatch would be noise.
* **`BaseTypeInfo::typeId` after `Factory::Startup`.** Startup overwrites the
  DLL's sentinel with the real CRC and never touches ours, so at `phase=exit` a
  difference is expected. Reported as `INFO`, never counted as a mismatch.

**Effectiveness control.** The summary separates *ground truth* comparisons
(harvest vs DLL — these prove the harvest, not the port) from *minted from OUR
ported code* (the only number that measures the port). If the second is zero it
says so in four lines of asterisks, because 239 green ground-truth rows with
zero of our own code in the table would otherwise read as a finished lane.

### Expected result, to be checked against the run

Predicted from the registry as of 2026-09-07 (post round 2), so a difference is
itself informative — most likely another lane landing ports between now and the
run, which should move `minted from OUR` **up** and `traps` down by the same
amount:

```
[RtMint] minted 28 vtables (239 slots: 32 ported, 161 unported-trap, 46 purecall-trap),
         14 BaseTypeInfo, 28 storage symbols (2992 bytes)
[RtMint] 0 of 28 vtables are COMPLETE ...
[RtMintShadow] ground truth       : 239 slot comparisons vs the live DLL, 0 MISMATCH
[RtMintShadow] minted from OUR    : 32 slots, 0 MISMATCH
[RtMintShadow] minted as traps    : 207 (46 purecall by design, 161 not yet ported)
[RtMintShadow] complete vtables   : 0 of 28
[RtMintShadow] slot-count suspects: 0
[RtMintShadow] BaseTypeInfo       : 70 field comparisons, 0 MISMATCH, 14 typeId INFO (phase=exit)
[RtMintShadow] storage            : 28 symbols, <n> written by the DLL, ...
[RtMintShadow] symbols absent from the DLL: 0
[RtMintShadow] ==== clean: 0 problem(s) ====
```

**Any non-zero `MISMATCH`, `slot-count suspects` or `absent` is a real finding**
and the run should be treated as failed. `typeId INFO` at `phase=exit` is
expected (14 of them); at `phase=init` it should be 0, and 14 there would mean
`Factory::Startup` had already run before `RuntimePortInit`, which would be
news.

## 8. How to run it

No new build target; the files are picked up by `standalone/*.cpp`.

```bash
mingw32-make sa-deploy
cd "C:/GOG Games/Banished"
BANISHED_AUTOSTART=1 BANISHED_RT_MINT_SHADOW=1 ./reimpl-x64.exe
grep -E "^\[RtMint" standalone_debug.log
```

or, from the repo, the usual driver with the env var set:
`BANISHED_RT_MINT_SHADOW=1 mingw32-make sa-run`.

`BANISHED_RT_MINT_SHADOW=1` is the only variable needed. **Do not set
`BANISHED_RT_MINT=1`** — that registers the minted data over the DLL's and is
for cutover only (§6.5).

Regenerating the tables (only needed if the harvest changes):

```bash
tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_mint_gen.py
```

It exits non-zero and writes nothing if any slot disagrees with the DLL or any
slot count cannot be established.
