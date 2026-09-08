# Wave 9, Lane C2 — Minting the exported DATA symbols

Follows [WAVE9_LANE_C.md](WAVE9_LANE_C.md) (the round-1 harvest) and
[WAVE9.md](WAVE9.md) §2. No build and no game run in this lane: static tools,
`pefile` against the shipped DLL, and a `-fsyntax-only` check of every file
written. The integrator owns the build.

**All counts in this document are as of 2026-09-07, round 3, census
843 called / 585 ported.** Read §1 before comparing them with any other
document's — the number is supposed to move.

**Delivered**

| File | What it is |
|---|---|
| `tools/rt_mint_gen.py` | Generator. **Runs `runtime_census.py` itself**, decodes the DLL, cross-checks the round-1 harvest, emits the tables. |
| `standalone/rt_data_mint_tables.inc` | Generated. 38 vtables (313 slots), 14 `BaseTypeInfo`, 28 storage symbols. |
| `standalone/rt_data_mint.{h,cpp}` | The mint: runtime fill, named traps, the type chain, and the shadow check. |
| `standalone/resolve{.h,_standalone.cpp}` | `RtPortLookupExport` / `RtPortLookupAddr` — residue-free registry queries (§4). |
| `standalone/main.cpp` | One line: a second shadow pass at exit. |

Registration is **off by default**. `BANISHED_RT_MINT=1` makes the minted data
live; nothing else in the tree changes behaviour.

---

## 1. The denominator is a moving target, and it tracks porting progress

| Measured | Exported DATA | Why it differs |
|---|--:|---|
| Round 1 (harvest, `wave9_data_harvest.csv`) | 60 | 28 `??_7` + 18 `?_singleton@` + 14 `?_typeInfo@`, filtered by **name prefix** |
| This lane, first pass | 70 | same set re-derived by **PE section** — +10 `.data` statics with ordinary names |
| This lane, round 3 | **80** | +10 `??_7*Data@Video` vtables **that our tree did not reference until lane B2 ported the twelve companion classes** |

**"60" was never the DLL's total.** It is the DLL data our tree *asks for*, at
the moment it was measured. Porting grows that set: a ported class references
its own vtable, so completing a class can hand this lane new rows —
`reference_porting_can_grow_the_cutover_surface.md`, measured three times in
this wave (`KeyMap::RegisterInput` +2, lane Q's `Array` cluster `internal`
64→69 while retiring 27, and now lane B2's +11 called for +29 ported, ten of
them mine).

The fix is structural, not a one-off recount: **`tools/rt_mint_gen.py` shells
out to `tools/runtime_census.py` on every run** and takes its symbol set from
that fresh output. It no longer reads the frozen `wave9_unported_20260907.txt`
at all. A generator driven by a hard-coded list would under-report silently for
the rest of the wave; this one cannot. The round-1 harvest CSV is still read,
but only as an independent **cross-check** — 253 rows (239 vtable slots + 14
`BaseTypeInfo` field sets) re-decoded straight from the DLL and compared,
**0 disagreements**.

The ten `.data` statics the prefix filter could not see are still worth naming,
because one of them is load-bearing:

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
| `?_loader@LoaderLoop@Engine@@1VLoader@2@A` | 56 | an `Engine::Loader` **object** — §6.1 |
| `?_rng@Particle@Graphics@@1VRandom@System@@A` | 2500 | a `System::Random` **object** — §6.1 |

`_loader`'s size is corroborated twice: `static_assert(sizeof(Loader) == 0x38)`
in `runtime/types/engine/UpdateLoop.h`, and the DLL's export table puts the next
symbol exactly 56 bytes later.

## 2. `_purecall` — 110 slots across 26 of the 38 vtables

Lane C reported "30 duplicate slots across 10 vtables". That counts **fold-group
duplicates** (10 groups, 40 slots, minus one representative each). The number
that matters for minting is how many slots must receive the trap: every slot
pointing at `0x18007ec28`. On lane C's own 28-vtable set that is **46 across
16** — the six it missed (`Base@Object`, `Thread@System`, `Element@UI`,
`ParticleInstance@Graphics`, `DialogLoopT`, `DialogLoaderLoopT`) each have
exactly one such slot and so formed no fold group. Lane C's ranking *table* was
already right; only the prose under it undercounts.

With lane B2's ten `*Data` vtables the total is **110 slots across 26
vtables** — because in those ten, *every slot except slot 0 is `_purecall`*
(`ContextData` and `DeviceData` are 19 of 20 each). This is all in lane B's
favour: 110 slots are not functions anyone has to port.

## 3. Slot counts: verified two independent ways, zero suspects

A minted vtable that is one slot short does not fail to link — it dispatches
whatever follows in `.rdata`. So every count is re-established:

* **23 of 38 by adjacency** — the last slot ends exactly where the next
  exported symbol begins. Exact, no inference. (All ten new `*Data` vtables are
  in this group; they are packed contiguously.)
* **15 of 38 by tail probe** — the qword one past the last slot is not a valid
  address anywhere in the DLL image, so the decode stopped on its own and not
  at the bound.
* **0 suspect.** The generator refuses to emit a table whose count it cannot
  establish either way, and the shadow check re-runs the tail probe live.

## 4. How the mint works, and why the tables are filled at runtime

The obvious design — bake `&OurGeometryClean` into a generated table — is wrong
here: **not one of the 38 vtables can be completely filled today** (§5), so a
baked table would need regenerating after every lane-B port, and every
regeneration is a chance to bake a stale address.

Instead the generated `.inc` records only the DLL's ground truth per slot
(*this slot holds function X, named or unnamed or `_purecall`*), and
`RtDataMintInit()` fills the actual table from the **port registry** after every
`*_PortInit()` has run. A slot becomes our code the moment lane B registers it,
with no source change and no regeneration.

**The rule this enforces: a minted slot holds our port or a named trap, never a
DLL address.** A slot quietly holding a DLL address works perfectly right up to
cutover and then faults with nothing to say. The trap prints the vtable, the
slot index and the mangled name of the function that should have been there —
which is why lane C recommended a named trap over UCRT's anonymous `_purecall`,
and it costs one generated stub per slot.

`BaseTypeInfo` is minted the same way: every field is a link-time constant from
the DLL **except `next`**, which reads `0x0` on disk for all 14 because the
chain is built by running constructors. Ours is built from our own order — and
not arbitrarily: `Factory::Startup` resolves each type's parent by name as it
walks, so the chain is built with **every parent ahead of its children in walk
order** (a topological pass; parents outside the set make a type a root). The
mint does **not** head-insert into the DLL's live `_firstTypeInfo`: the DLL's
static initialisers already registered these 14 types and duplicates would give
`Factory::Startup` two nodes per name. Our chain is separate until registration
mode makes our `_firstTypeInfo` storage the only one there is.

Singletons and the other `.data` statics get **no contents**. All 28 are BSS in
the DLL — the harvest read `b''` from every one — so minting them means owning
zeroed bytes and letting the existing runtime writer fill them.

### The trap this lane nearly walked into

The shadow check asks the registry and the DLL about ~360 symbols per pass. The
first draft did that through `ResolveExport` / `ResolveExportRaw` /
`ResolveRuntimeRaw` — all three of which **record into the `BANISHED_RT_STRICT`
residue census**. It would have manufactured several hundred `[RtResidue]` /
`[RtRawResidue]` entries and destroyed the one measurement Wave 9's cutover gate
depends on: the same shape as
`reference_truncated_tool_output_manufactures_calls_zero.md`, a tool inventing
the signal it exists to read. Hence `RtPortLookupExport` / `RtPortLookupAddr`:
registry queries that answer "is this ported" without falling through to the
DLL and without recording anything. Anything that wants to *inspect* the
registry rather than call through it should use them.

## 5. Per-symbol status

### The highest-leverage finding: slot 0 is `??_G`, not `??1`

Lane B2 ported `??1DeviceData@Video@@UEAA@XZ` and its eleven siblings — the
**virtual destructors**, at `0x1800f8cf0`+. Vtable slot 0 does not hold those.
It holds the compiler-generated **scalar deleting destructor** at
`0x180058590 + 0x80·k`, which is a different function, unnamed in the export
directory, and still unported. That is why lane B2's +29 ported functions moved
this lane's filled-slot count by **zero**.

All twelve are byte-identical apart from one rip-relative `LEA` displacement and
the `CALL` target (verified from the DLL's bytes): the standard
`test dl,2` deleting-destructor thunk that calls the real `??1` and conditionally
frees. **Porting those twelve thunks — or one shared body, twelve
registrations — would take twelve vtables from "1 trap" to COMPLETE**, because
every other slot in them is `_purecall`. It is the cheapest 12 complete vtables
available in the whole lane, and nothing else in this document comes close.

| Vtable | slot-0 `??_G` | slots | purecall |
|---|---|--:|--:|
| `??_7RenderBufferData@Video@@6B@` | `0x180058590` | 4 | 3 |
| `??_7VideoProgramData@Video@@6B@` | `0x180058610` | 3 | 2 |
| `??_7RenderStateData@Video@@6B@` | `0x180058690` | 3 | 2 |
| `??_7PixelProgramData@Video@@6B@` | `0x180058710` | 3 | 2 |
| `??_7VertexProgramData@Video@@6B@` | `0x180058790` | 3 | 2 |
| `??_7VertexLayoutData@Video@@6B@` | `0x180058810` | 3 | 2 |
| `??_7VertexBufferData@Video@@6B@` | `0x180058890` | 7 | 6 |
| `??_7IndexBufferData@Video@@6B@` | `0x180058910` | 7 | 6 |
| `??_7ConstantBufferData@Video@@6B@` | `0x180058990` | 4 | 3 |
| `??_7ImageBufferData@Video@@6B@` | `0x180058a10` | 7 | 6 |
| `??_7ContextData@Video@@6B@` | `0x180058a90` | 20 | 19 |
| `??_7DeviceData@Video@@6B@` | `0x180058b10` | 20 | 19 |

The same shape holds more weakly elsewhere: slot 0 is an unnamed deleting
destructor in **36 of the 38** vtables (`Thread@System` is `_purecall`;
`ImageBufferData`/`RenderStateData` are in the table above).

### Vtables — 38 minted, 0 complete

"ported" is what the registry holds today; it rises on its own as lane B lands
ports. "trap" is lane B's remaining worklist for that class.

| Vtable | slots | ported | trap | purecall |
|---|--:|--:|--:|--:|
| `??_7Geometry@Graphics@@6B@` | 9 | 5 | 1 | 3 |
| `??_7ConstantBufferData@Video@@6B@` | 4 | 0 | 1 | 3 |
| `??_7ContextData@Video@@6B@` | 20 | 0 | 1 | 19 |
| `??_7DeviceData@Video@@6B@` | 20 | 0 | 1 | 19 |
| `??_7ImageBufferData@Video@@6B@` | 7 | 0 | 1 | 6 |
| `??_7IndexBufferData@Video@@6B@` | 7 | 0 | 1 | 6 |
| `??_7PixelProgramData@Video@@6B@` | 3 | 0 | 1 | 2 |
| `??_7RenderBufferData@Video@@6B@` | 4 | 0 | 1 | 3 |
| `??_7RenderStateData@Video@@6B@` | 3 | 0 | 1 | 2 |
| `??_7Thread@System@@6B@` | 2 | 0 | 1 | 1 |
| `??_7VertexBufferData@Video@@6B@` | 7 | 0 | 1 | 6 |
| `??_7VertexLayoutData@Video@@6B@` | 3 | 0 | 1 | 2 |
| `??_7VertexProgramData@Video@@6B@` | 3 | 0 | 1 | 2 |
| `??_7VideoProgramData@Video@@6B@` | 3 | 0 | 1 | 2 |
| `??_7Base@Object@@6B@` | 3 | 0 | 2 | 1 |
| `??_7Stream@IO@@6B@` | 7 | 0 | 2 | 5 |
| `??_7Internal@Object@@6B@` | 4 | 1 | 3 | 0 |
| `??_7AchievementInstance@Achievement@@6B@` | 4 | 0 | 4 | 0 |
| `??_7AnimationEvent@Graphics@@6B@` | 9 | 0 | 4 | 5 |
| `??_7CompiledExternal@Object@@6B@` | 8 | 0 | 4 | 4 |
| `??_7External@Object@@6B@` | 8 | 0 | 4 | 4 |
| `??_7ParticleInstance@Graphics@@6B@` | 10 | 4 | 5 | 1 |
| `??_7Goal@Achievement@@6B@` | 10 | 2 | 5 | 3 |
| `??_7GoalInstance@Achievement@@6B@` | 7 | 2 | 5 | 0 |
| `??_7ParticleSystemInstance@Graphics@@6B@` | 10 | 4 | 6 | 0 |
| `??_7Particle@Graphics@@6B@` | 9 | 0 | 6 | 3 |
| `??_7DialogInstance@UI@@6B@` | 10 | 3 | 7 | 0 |
| `??_7UpdateLoop@Engine@@6B@` | 12 | 0 | 7 | 5 |
| `??_7Achievement@0@6B@` | 9 | 1 | 8 | 0 |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 | 1 | 8 | 0 |
| `??_7DialogLoopT@Engine@@6B@` | 13 | 3 | 9 | 1 |
| `??_7Element@UI@@6B@` | 12 | 2 | 9 | 1 |
| `??_7ImageBuffer@Video@@6B@` | 10 | 1 | 9 | 0 |
| `??_7MaterialInstance@Graphics@@6B@` | 9 | 0 | 9 | 0 |
| `??_7SpriteSheet@UI@@6B@` | 9 | 0 | 9 | 0 |
| `??_7DialogLoaderLoopT@Engine@@6B@` | 13 | 2 | 10 | 1 |
| `??_7DialogController@UI@@6B@` | 11 | 1 | 10 | 0 |
| `??_7LoaderLoop@Engine@@6B@` | 12 | 0 | 12 | 0 |
| **total** | **313** | **32** | **171** | **110** |

Minting a table with unfilled slots does not contradict "do not mint a vtable
whose slots you cannot fill" — it is that rule applied per slot instead of per
class: an unfilled slot is a trap that names itself, so a partially-minted table
cannot silently dispatch into the wrong body. The alternative reading (mint
nothing until a class is complete) would deliver **zero** vtables today and
nothing for lane B to measure against.

### `BaseTypeInfo` — 14 minted, all with trapped factories

All 14 are minted with `name` / `parentName` (stripped after the last `:`,
exactly as the ported ctor does) / `fileExtension` / `typeId = 0xFFFFFFFF` from
the DLL, and `next` from our topological chain.

**All 28 factory pointers (14 × `CreateNewObject` + `CreateInPlaceObject`) are
traps**: not one is ported. That is this lane's second-biggest blocker —
`Factory::CreateObject` tail-jumps straight to `+0x20`/`+0x28`, so at cutover a
type whose factory is a trap cannot be constructed at all. Types:
`Achievement`, `AchievementInstance`, `GoalInstance`, `GraphicsMesh`,
`Material`, `MaterialInstance`, `Particle`, `External`, `ExternalList`,
`ClipRegionElement`, `Dialog`, `ImageElement`, `NumberEditElement`,
`ScrollElement`.

### Storage — 28 minted (18 singletons + 10 `.data` statics)

Zeroed, 16-byte-aligned storage. Contents never compared against the DLL (§7).
Two need more than storage — §6.1.

## 6. Deferred, blocked, and what is *not* covered

1. **`?_loader@LoaderLoop` and `?_rng@Particle` need a CONSTRUCTOR at startup,
   not just storage.** Both are BSS in the *file*, which is all the harvest can
   see, but the DLL's CRT runs their constructors at load: `Loader`'s stores
   `??_7Loader@Engine@@6B@` into `+0x00`, `Random`'s seeds 624 words of MT19937
   state. Minting zeroed bytes reproduces the file, not the loaded DLL. After
   cutover the first virtual call on `_loader` dispatches through a null vtable
   and `Particle::_rng` returns zeros forever. **Open — needs an owner.** No
   other storage symbol has this shape (singletons are pointers written on
   first use).
2. **The twelve `??_G` deleting-destructor thunks** (§5) — blocked on lane B,
   and the cheapest 12 complete vtables in the wave.
3. **The 28 factory pointers** — blocked on lane B porting `CreateNewObject` /
   `CreateInPlaceObject` for the 14 types.
4. **The 14 `BaseTypeInfo` are the 14 our tree CALLS**, not every type the DLL
   registers. The rest of the chain has no consumer in the census, so it is
   outside the "called" denominator — but if DLL-internal code that survives to
   cutover walks the chain expecting a type nobody in our tree names, it will
   not be there. Recorded, not solved.
5. **The chain order is reasoned, not measured.** Parents precede children,
   which is what `Factory::Startup`'s by-name parent resolution needs. Retail's
   order is a linker artefact and not recoverable; if `Startup` turns out to be
   order-insensitive (a second pass), this is simply harmless.
6. **Registration is off.** `BANISHED_RT_MINT=1` while the DLL is loaded splits
   the process's state: the DLL's own code keeps using the DLL's singletons
   while our callers get ours, empty. It is wired now so the last commit of the
   wave is a flag flip, not new code — but it must not be flipped before the
   DLL goes.
7. **The set will grow again.** Every class a later lane completes can add
   vtables here (§1). Re-run the generator before closing the wave; it re-runs
   the census for you and will fail loudly on any new exported `.data` static
   whose size nobody has pinned.

## 7. The shadow check — what it compares, and what it refuses to

`BANISHED_RT_MINT_SHADOW=1`. Runs twice: at `RtDataMintInit` (`phase=init`) and
at process exit from `standalone/main.cpp` (`phase=exit`), because which
singletons the process actually populates is itself ground truth that dies with
the DLL.

**Compared**

| | |
|---|---|
| every vtable's exported address | vs the recorded VA |
| all 313 slots, live | DLL's qword vs the recorded target |
| every filled slot | ours == the registry's port of *that same function*, and **≠ the DLL's address** |
| slot count, live | the qword past the last slot must not be a DLL pointer |
| `BaseTypeInfo` `name`/`parentName`/`fileExtension` | by `strcmp`, not by pointer |
| `BaseTypeInfo` `factoryFn`/`inPlaceFactoryFn` | DLL's vs the recorded VA |

**Deliberately not compared**

* **The minted vtable's bytes against the DLL's.** A filled slot holds *our*
  function's address, a different number by construction. A byte comparison
  fails on exactly the slots that represent progress and passes on the ones
  that do not. The comparison that means something is the *mapping*.
* **Any storage symbol's contents.** All 28 are BSS the DLL only writes at
  runtime — comparing them is
  `reference_oracle_must_not_compare_unwritten_bytes.md` verbatim, returning
  100% green or 100% red while meaning neither. The report prints instead which
  of the 28 each side has actually written.
* **`BaseTypeInfo::next`.** The DLL's is built by its static-init order, ours by
  ours; they cannot agree and a mismatch would be noise.
* **`BaseTypeInfo::typeId` after `Factory::Startup`.** Startup overwrites the
  DLL's sentinel with the real CRC and never touches ours, so at `phase=exit` a
  difference is expected. Reported as `INFO`, never counted as a mismatch.

**Effectiveness control.** The summary separates *ground truth* comparisons
(recorded vs DLL — these prove the harvest, not the port) from *minted from OUR
ported code* (the only number that measures the port). If the second is zero it
says so in four lines of asterisks, because 313 green ground-truth rows with
none of our own code in the tables would otherwise read as a finished lane.

### Expected result, to be checked against the run

Predicted from the registry at census 843/585, so a difference is itself
informative — most likely another lane landing ports between now and the run,
which should move `minted from OUR` **up** and `traps` down by the same amount:

```
[RtMint] minted 38 vtables (313 slots: 32 ported, 171 unported-trap, 110 purecall-trap),
         14 BaseTypeInfo, 28 storage symbols (2992 bytes)
[RtMint] 0 of 38 vtables are COMPLETE ...
[RtMintShadow] ground truth       : 313 slot comparisons vs the live DLL, 0 MISMATCH
[RtMintShadow] minted from OUR    : 32 slots, 0 MISMATCH
[RtMintShadow] minted as traps    : 281 (110 purecall by design, 171 not yet ported)
[RtMintShadow] complete vtables   : 0 of 38
[RtMintShadow] slot-count suspects: 0
[RtMintShadow] BaseTypeInfo       : 70 field comparisons, 0 MISMATCH, 14 typeId INFO (phase=exit)
[RtMintShadow] storage            : 28 symbols, <n> written by the DLL, ...
[RtMintShadow] symbols absent from the DLL: 0
[RtMintShadow] ==== clean: 0 problem(s) ====
```

**Any non-zero `MISMATCH`, `slot-count suspects` or `absent` is a real finding**
and the run should be treated as failed. `typeId INFO` at `phase=exit` is
expected (14 of them); at `phase=init` it should be 0, and 14 there would mean
`Factory::Startup` ran before `RuntimePortInit`, which would be news.

## 8. How to run it

No new build target; the files are picked up by `standalone/*.cpp`.

```bash
BANISHED_RT_MINT_SHADOW=1 mingw32-make sa-run
grep -E "^\[RtMint" "C:/GOG Games/Banished/standalone_debug.log"
```

`BANISHED_RT_MINT_SHADOW=1` is the only variable needed. **Do not set
`BANISHED_RT_MINT=1`** — that registers the minted data over the DLL's and is
for cutover only (§6.6).

Regenerating the tables (do this whenever another lane lands ports, and before
the wave closes):

```bash
tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_mint_gen.py
```

It re-runs the census, re-decodes the DLL, cross-checks the round-1 harvest, and
exits non-zero writing nothing if any slot disagrees, any slot count cannot be
established, or any new exported `.data` static has no pinned size.
