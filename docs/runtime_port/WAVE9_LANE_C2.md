# Wave 9, Lane C2 — Minting the exported DATA symbols

Follows [WAVE9_LANE_C.md](WAVE9_LANE_C.md) (the round-1 harvest) and
[WAVE9.md](WAVE9.md) §2. No build and no game run in this lane: static tools,
`pefile` against the shipped DLL, and a `-fsyntax-only` check of every file
written. The integrator owns the build.

**All counts in this document are as of 2026-09-08, round 5, census
843 called / 585 ported.** Read §1 before comparing them with any other
document's — the number is supposed to move, and §2 is what settles which of
those symbols are merely storage and which the DLL actually constructs.

**Delivered**

| File | What it is |
|---|---|
| `tools/rt_mint_gen.py` | Generator. **Runs `runtime_census.py` itself**, decodes the DLL, cross-checks the round-1 harvest, emits the tables. |
| `standalone/rt_data_mint_tables.inc` | Generated. 46 vtables (429 slots), 33 `BaseTypeInfo`, 28 storage symbols, and the load-time-construction verdict for each. |
| `tools/rt_data_door_audit.py` | The door gate (§3). Exits 2 while any minted DATA symbol is read through a raw door. |
| 18 source files | The door fix: 57 sites moved from `ResolveExportRaw` to `ResolveExport`, 3 marked `RT_RAW_OK`. |
| `standalone/rt_data_mint.{h,cpp}` | The mint: runtime fill, named traps, the type chain, the `?_loader` construction, and the shadow check. |
| `standalone/resolve{.h,_standalone.cpp}` | `RtPortLookupExport` / `RtPortLookupAddr` — residue-free registry queries (§6). |
| `standalone/main.cpp` | One line: a second shadow pass at exit. |

Registration is **off by default**, and so is construction. `BANISHED_RT_MINT=1`
makes the minted data live; `BANISHED_RT_MINT_CTOR=1` runs the load-time
constructions without registering; `BANISHED_RT_MINT_SHADOW=1` reports. With
none of them set nothing in the tree changes behaviour.

---

## 1. The denominator is a moving target, and it tracks porting progress

| Measured | Exported DATA | Why it differs |
|---|--:|---|
| Round 1 (harvest, `wave9_data_harvest.csv`) | 60 | 28 `??_7` + 18 `?_singleton@` + 14 `?_typeInfo@`, filtered by **name prefix** |
| This lane, first pass | 70 | same set re-derived by **PE section** — +10 `.data` statics with ordinary names |
| This lane, round 3 | 80 | +10 `??_7*Data@Video` vtables **that our tree did not reference until lane B2 ported the twelve companion classes** |
| This lane, round 4 | 81 | +`??_7Loader@Engine@@6B@`, which nothing in the process references except the constructor this lane just reproduced (§2) |
| This lane, round 5 | **107** | +26 the CENSUS CANNOT SEE — reached through a wrapper, a named constant, or a table walked in a loop (§3) |

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
| `?_loader@LoaderLoop@Engine@@1VLoader@2@A` | 56 | an `Engine::Loader` **object**, and the one storage symbol the DLL CONSTRUCTS at load — §2 |
| `?_rng@Particle@Graphics@@1VRandom@System@@A` | 2500 | a `System::Random` **object**, CONSTRUCTED at load — MT19937 seeded from `time()`; see §2 |

`_loader`'s size is corroborated twice: `static_assert(sizeof(Loader) == 0x38)`
in `runtime/types/engine/UpdateLoop.h`, and the DLL's export table puts the next
symbol exactly 56 bytes later.

## 2. What the DLL CONSTRUCTS at load — swept against its own `.CRT$XC` array

A file-based harvest cannot answer "is this really just storage". Every one of
these statics is BSS in the file whether or not a constructor runs over it at
load, so section flags and file bytes both say the same thing about a plain
`int` and about a live `Thread` with four kernel handles. **The DLL's own CRT
initialiser array is the ground truth**, and `tools/rt_mint_gen.py` now reads
it on every run (`find_crt_init_table` + `scan_ctor_refs`):

* The array is located structurally — a run of qwords in `.rdata` that all
  point into `.text`, bounded by a zero qword at each end, strictly increasing.
  Vtable regions also produce long runs of `.text` pointers, so "strictly
  increasing" plus the zero terminators is what separates them; the generator
  asserts the candidate is **unique** and refuses to guess. Result: **106
  initialisers**, at `0x1801477b0`–`0x18014a510`.
* Every initialiser is then scanned for a rip-relative reference to any symbol
  in the mint's set. Every hit was cross-checked against Ghidra's own xrefs;
  there were no false positives.

**Result, all 107 symbols swept — 37 are constructed at load:**

| Symbol(s) | Initialiser | Disposition |
|---|---|---|
| the 14 `?_typeInfo@` | one each | Already reproduced by `MintTypeInfos()`. Every one of the 14 is constructed — a **positive control**: the generator fails if any `BaseTypeInfo` turns out *not* to be, because that would mean the sweep is broken rather than that the type is special. |
| `?_firstTypeInfo@BaseTypeInfo@Object@@` | 104 of the 106 | The chain head, head-inserted by each `BaseTypeInfo` ctor. Reproduced by our own chain (§6). |
| `?_loader@LoaderLoop@Engine@@` | `0x180148fe0`, one | **Implemented** — `ConstructLoader()`. |
| `?_rng@Particle@Graphics@@` | `0x180149650`, one | **Implemented** — `ConstructParticleRng()`. Missed by the first sweep; see below. |
| everything else — all 18 singletons, all 46 vtables, the other 8 `.data` statics | none | Plain storage. Zeroed bytes are faithful. |

### `?_rng@Particle@Graphics@@` IS constructed — and this section said the opposite for a day

Round 3 listed `_rng` beside `_loader` as needing a constructor. Round 4 said
the sweep had disproved that and zeroed storage was faithful. **Round 4 was
wrong, and the mint's own shadow report is what caught it.**

The report prints, per storage symbol, whether each side has written anything.
It showed `?_rng@Particle@Graphics@@  DLL=WRITTEN` **at `phase=init`** — a
2500-byte BSS object with content before any game code could have touched it.
A boolean cannot say *what* was written, so the check now prints the offset of
the first non-zero byte, and a re-run answered it:

```
FIRST-NONZERO ?_rng...: DLL byte +0x4 = f0 8c 9f 6a 16 94 f6 f9   (phase=init, index still 0)
FIRST-NONZERO ?_rng...: DLL byte +0x0 = 81 01 00 00 29 d4 60 70   (phase=exit, index = 0x181 draws)
```

`+0x00` zero and `+0x04` onward populated is a **seeded MT19937 with a fresh
index** — exactly what a constructor leaves behind. Ghidra names the writer:
CRT initialiser **`0x180149650`**, decompiled:

```c
mt[0] = (uint32)_time64(NULL);
for (i = 1; i < 0x270; ++i)
    mt[i] = 0x6c078965 * (mt[i-1] ^ (mt[i-1] >> 30)) + i;   // MT19937 init_genrand
```

**Why the sweep missed it, and the fix.** The scan matched an initialiser's
rip-relative references against each symbol's **base address**. That
initialiser never mentions the base — it writes `mt[0]` at `+0x04` and walks
upward — so `_rng` looked untouched. A constructor referencing a *field* rather
than the base is ordinary, and `scan_ctor_refs` now matches anywhere inside
`[base, base+size)`. With that fix the generator **refused to emit** until the
constructor existed, which is the gate doing its job.

**The seed is time-derived, so the value cannot and need not match retail** —
two runs of the *shipped* DLL do not match each other. Answering the question
directly: there is no constant to reproduce, and a mismatch here is not a
defect. **Seeding at all is not optional**, though: `NextGroup` (`0x1800c7dd0`)
is a pure twist with no seeding path, so an all-zero state twists to all-zero
forever and every `Graphics::Particle` draw would silently return 0 — plausible
numbers, no crash, nothing a test would catch. Reproduced by
`ConstructParticleRng()`, transcribed from the initialiser above; `index` at
`+0x00` is deliberately left zero, as retail leaves it.

**What this cost and what it bought.** A day of a wrong statement in a doc and
a commit message — and the correction came from the weaker of the two
mechanisms. The `.CRT$XC` sweep is the precise instrument and it was wrong; the
liveness line is a one-bit observation and it was right. That is the argument
for keeping cheap redundant observations in a report even when a better
instrument exists, and it is why the shadow now prints the offset rather than
the bit.

### `?_loader@LoaderLoop@Engine@@` — implemented

`FUN_180148fe0`, transcribed and reproduced by `ConstructLoader()`:

```
System::Thread::Thread(&_loader, 0x40000);   // 256 KB stack — ported, ??0Thread@System@@QEAA@I@Z
_loader.vtable = ??_7Loader@Engine@@6B@;     // +0x00
_loader._loop  = nullptr;                    // +0x30  (static_assert sizeof(Thread)==0x30)
atexit(...);                                 // retail's teardown IS ??1Thread with the
                                             // Thread vtable restamped — our ported dtor
```

Two things fell out of implementing it:

1. **It added a 39th vtable to this lane.** `??_7Loader@Engine@@6B@` is in no
   census: the only thing in the entire process that references it is the
   initialiser that stamps it, so our tree never resolved it — until this
   construction made us its consumer. It is now minted via an explicit
   `EXTRA_VTABLES` entry with that reason written down. Lane C2's own instance
   of "porting grows the cutover surface", found inside a single function.
   Its 2 slots are `?Run@Loader@Engine@@UEAAXXZ` and (inherited, not
   overridden) `?OnCompleted@Thread@System@@UEAAXXZ`; **neither is ported**, so
   both are traps. That is still strictly better than the null vtable zeroed
   storage would have given: after cutover the loading thread says *which*
   method is missing instead of dispatching through address zero.
2. **`??_7Thread@System@@6B@`'s shape is now explained.** Thread's 2-slot
   vtable is `[_purecall, OnCompleted]` because `Run` is pure virtual in
   `Thread`; `Loader` overrides `Run` and inherits `OnCompleted`. Porting the
   single function `?OnCompleted@Thread@System@@UEAAXXZ` fills a slot in **two**
   vtables.

**Constructing is gated**, because it starts a real OS thread and three events:
`BANISHED_RT_MINT=1` (cutover) or `BANISHED_RT_MINT_CTOR=1` (build the objects
without registering the mint, so the shadow check can compare ours against the
DLL's live one while both exist).

### The durable part

The sweep is not a one-off answer written into a document. It runs inside the
generator, and a storage symbol that is constructed but has no entry in
`KNOWN_CTORS` is a **hard failure**: the generator refuses to emit rather than
produce a symbol that would be minted as zeroed bytes and silently differ from
the loaded DLL. If a later lane's port pulls in another constructed static —
and §1 says it will — the next regeneration says so by name.

## 3. The DOOR — a minted symbol read through `ResolveExportRaw` is still NULL

Handed over by lane V, which found three `Graphics` vtables stamped into slot 0
by a ported constructor and absent from the mint. Both halves of that are real,
and the second is the larger one.

**Minting a symbol does not make it reachable.** The mint registers with
`RegisterRuntimeExport`, and only `ResolveExport` consults that registry.
`ResolveExportRaw` is `GetProcAddress` against Runtime-x64.dll — by design, so
the differential oracle can always reach the shipped body — and at cutover it
returns NULL for a symbol the mint carries correctly just as much as for one it
has never heard of. A ported ctor doing
`self->vtable = ResolveExportRaw("??_7Foo@…")` therefore writes a **NULL vtable**
into every object of that class and the first virtual call is an EXEC-of-0.
Adding rows to the table fixes nothing on its own.

`tools/rt_data_door_audit.py` is the general form. It reads the minted set from
the generated table, finds names reached through a **wrapper** or a **named
constant** as well as directly, and fails if any minted symbol is read through a
raw door. First run: **60 sites**, not three.

**Fixed: 57 sites.** 48 by swapping the door at the site, plus three wrappers
whose one-line body carried the rest — `RtVtable` (`mesh.cpp`, 5 sites, the
helper that hid this from a `grep ResolveExportRaw("`), `RtDataExport`
(`saveload.cpp`, 5), and two `RtOriginal<void**>` reads of the `IO::Persistence`
singleton inside `Audio::Configuration` (the oracle helper used for a *data*
symbol — same family).

**`ResolveExport` is the correct door, and this is a no-op today.** While the
DLL is loaded and the mint unregistered, `ResolveExport` misses the registry and
falls through to the same `ResolveExportRawImpl` — the identical pointer. The
change only bites at cutover, which is when it must.

*Verified rather than inherited* (`saveload.cpp`'s comment asserts
GetProcAddress resolves data exports; the brief asked not to take that on
trust): the mint's own shadow check resolves all 107 symbols with
`GetProcAddress` on the module handle and the recorded run reports **0 absent**.
That is a measurement over the whole set, not a claim about one symbol.

**Three sites deliberately stay raw**, marked at the site with `RT_RAW_OK` and a
reason, and reported separately by the audit: `dialog.cpp`'s `VerifyDtorThunk`
(an oracle read of the DLL's own thunk bytes) and two `rt_orphan_probe.cpp`
probes. All three die with the oracle.

### The other half of the door: ADDRESS-keyed reads, and the unexported table head

Round 5 fixed the reads that ask by NAME. Thirty sites in this tree ask for
exported DATA by **address** instead — `ResolveAddr(Module::Runtime,
0x1801fe7b8)` and friends — and `RegisterRuntimeExport` does not cover those.
A mint registered under a name only would have been walked straight past by all
thirty, which is the round-5 bug one door over.

**The mint now registers BOTH doors** for every row: `RegisterRuntimeExport`
when the symbol has a name, and `RegisterRuntimeAddr(dllVa, storage)` always.
Coverage of those thirty sites, measured against the generated table:

| | count |
|---|--:|
| non-`.text` addresses our tree resolves by address | 30 |
| already a minted row's BASE address | 27 |
| an INTERIOR field of a minted row (`_loader+0x08` runFlags, `_loader+0x28` hFinished, read by `engine/funcs/engine_funcs.cpp`) | 2 |
| an unexported `.data` static with no minted row at all (`0x1801fc6d0`, `IO::Modification`'s loaded-DLL `HMODULE`, `FreeLibrary`'d by `game_main.cpp` at shutdown) | 1 |
| **uncovered after this change** | **0** |

An exact-match address registry does not cover an interior offset, so the two
`_loader` fields are registered explicitly as aliases; a new interior read would
need a row, which is why they are named in the code rather than derived.

#### "Unexported" is not "unmintable" — the `IO` BinaryEncoding vtable

`runtime/src/io/compressedstream.cpp` recorded `??0MemoryStream` and
`??0CompressedStream` as blocked because the concrete encoding vtable they
stamp — `0x1801554a8` — has *"no name for `ResolveExport` and no row the mint
can carry, because the mint is keyed by exported symbol."* **The second half was
false, and it was load-bearing on the cutover.** A vtable needs CONTENTS and an
ADDRESS, not a name, and `RegisterRuntimeAddr` / `ResolveAddr(Module::Runtime,
x)` is a door keyed on exactly that. It is now minted, with `symbol == nullptr`
and everything else identical.

Measured, not assumed:

* **5 slots**, and the count is proved by the STRONGEST of the two proofs the
  other 47 vtables use: the table ends *exactly* where
  `??_7BinaryFileStream@IO@@6B@` begins. So the live re-check
  (`7d25f9a`'s adjacency branch) re-checks the same half of the proof for this
  row as for every other adjacency-proved row.
* It is a table HEAD, not an interior: `0x180155488..0x1801554a0` are four
  `_purecall` slots of the abstract base, and `0x1801554a8` is where the
  concrete table starts — which the compressedstream.cpp comment had already
  established by address.
* **Five consumers in the DLL, not two.** `??0CompressedStream`,
  `??0MemoryStream`, `??0BinaryFileStream`, `UserInterface::SaveAllSettings`
  and `DialogInstance::OnOpened`. All five stamp it at `+0x00` of a
  freshly-allocated object which then goes to `stream+0x10` — the identical
  seven-instruction shape at every site.

**What minting it does and does NOT do, stated plainly.** All five slot targets
are unexported `.text` internals (`0x1800a81e0`, `0x1800a81d0`, `0x1800a81c0`,
`0x1800a8120`, `0x1800a8080`) and none is ported, so all five slots mint as
NAMED TRAPS. That converts the cutover failure from *stamping NULL and taking
an EXEC-of-0 on first dispatch* into *a trap that names the vtable and the
slot* — and, more usefully, it removes the stated reason `??0MemoryStream` and
`??0CompressedStream` could not be ported. **Minting is enabling, not
sufficient**: those two ctors are exported (`0x1800aa3d0`, `0x1800ae710`) and
remain ordinary port work. Whether the five bodies also need porting depends on
whether anything dispatches through the encoding on the save/load path; nothing
in our tree does today, and if something does, the trap says which slot.

Also worth recording: those two ctors are **absent from the census** even though
our ported `CompressedMemoryStream` ctor calls them, because it reaches them
through `RT_EXPORT_ONCE(...)` — a macro, and the census's `RE_EXPORT` needs the
literal directly inside `ResolveExport(`. That is the fourth distinct shape of
the census blind spot §3 opened with.

**One limit on verification.** The address-door registration only executes under
`BANISHED_RT_MINT=1`, which is cutover-only, so no shadow run exercises it. Its
evidence is the static coverage table above (30 of 30) plus the fact that it is
one map insert per row — not a measurement. Said explicitly so nobody reads the
next clean shadow run as having proved it.

### The bigger half: 26 symbols the census cannot see

Chasing the door turned up why those three vtables were missing from the mint at
all, and it is not that they were overlooked. `runtime_census.py`'s `RE_EXPORT`
requires the string literal to sit **directly** inside `ResolveExport(...)`. It
therefore cannot see:

| Shape | Example |
|---|---|
| a wrapper | `RtVtable("??_7MeshInstance@Graphics@@6B@")` |
| a named constant | `ResolveExportRaw(kFrameIndexData)` |
| a table walked in a loop | `ResolveExport(s.mangled)` over `kElementTypeSlots[]` |

All three are live here, and the third is the expensive one:
`standalone/reg_element_typeids.cpp` holds **21 UI `?_typeInfo@` symbols** whose
own header comment says *"a partial table costs one crash per missing entry"* —
and every one of them was outside the mint.

The generator now takes its symbol set from **the census UNION a source scan for
DATA-symbol literals**, with comments stripped (a mangled name in a comment is a
note, not a dependency — that alone removed a false positive from
`marketwork.cpp`). **+26 symbols: 81 → 107.**

| | before | after |
|---|--:|--:|
| exported DATA minted | 81 | **107** |
| `??_7` vtables | 39 | **46** (+3 Graphics, +3 IO, +1 UI) |
| `?_typeInfo@` | 14 | **33** |
| vtable slots | 315 | **429** |

Every one of the 7 new vtables carries the same slot-count proof as the other
39 — 3 by adjacency to an exported neighbour (so the live re-check the
integrator fixed in `7d25f9a` can re-check the same half of the proof), 4 by the
tail probe — and all 19 new `BaseTypeInfo` pass the CRT-constructed positive
control (§2).

## 4. `_purecall` — 113 slots across 27 of the 46 vtables

Lane C reported "30 duplicate slots across 10 vtables". That counts **fold-group
duplicates** (10 groups, 40 slots, minus one representative each). The number
that matters for minting is how many slots must receive the trap: every slot
pointing at `0x18007ec28`. On lane C's own 28-vtable set that is **46 across
16** — the six it missed (`Base@Object`, `Thread@System`, `Element@UI`,
`ParticleInstance@Graphics`, `DialogLoopT`, `DialogLoaderLoopT`) each have
exactly one such slot and so formed no fold group. Lane C's ranking *table* was
already right; only the prose under it undercounts.

With lane B2's ten `*Data` vtables the total is **113 slots across 27
vtables** — because in those ten, *every slot except slot 0 is `_purecall`*
(`ContextData` and `DeviceData` are 19 of 20 each). This is all in lane B's
favour: 113 slots are not functions anyone has to port.

## 5. Slot counts: verified two independent ways, zero suspects

A minted vtable that is one slot short does not fail to link — it dispatches
whatever follows in `.rdata`. So every count is re-established:

* **26 of 46 by adjacency** — the last slot ends exactly where the next
  exported symbol begins. Exact, no inference. (All ten new `*Data` vtables are
  in this group; they are packed contiguously.)
* **20 of 46 by tail probe** — the qword one past the last slot is not a valid
  address anywhere in the DLL image, so the decode stopped on its own and not
  at the bound.
* **0 suspect.** The generator refuses to emit a table whose count it cannot
  establish either way, and the shadow check re-runs the tail probe live.

## 6. How the mint works, and why the tables are filled at runtime

The obvious design — bake `&OurGeometryClean` into a generated table — is wrong
here: **not one of the 46 vtables could be completely filled when this was written** (§7), so a
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

## 7. Per-symbol status

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
destructor in **43 of the 46** vtables (`Thread@System` is `_purecall`;
`ImageBufferData`/`RenderStateData` are in the table above).

### Vtables — 46 minted

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
| `??_7Loader@Engine@@6B@` | 2 | 0 | 2 | 0 |
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
| **total (round 3 census)** | **315** | **32** | **173** | **110** |

The seven added in round 5 (§3), same columns, same caveat about the offline
`ported` figure:

| Vtable | slots | ported | trap | purecall |
|---|--:|--:|--:|--:|
| `??_7EdgeMeshInstance@Graphics@@6B@` | 9 | 4 | 5 | 0 |
| `??_7MeshInstance@Graphics@@6B@` | 9 | 2 | 7 | 0 |
| `??_7ElementDescription@UI@@6B@` | 9 | 1 | 5 | 3 |
| `??_7EdgeMesh@Graphics@@6B@` | 9 | 0 | 9 | 0 |
| `??_7BinaryReader@IO@@6B@` | 26 | 0 | 26 | 0 |
| `??_7BinaryWriter@IO@@6B@` | 26 | 0 | 26 | 0 |
| `??_7Visitor@IO@@6B@` | 26 | 0 | 26 | 0 |
| **added** | **114** | **7** | **104** | **3** |

The three 26-slot `IO` tables are the largest single block of unported slots
this lane has ever added, and they arrived from *type headers*
(`runtime/types/io/Serializer.h`'s `kBinaryReaderVtableName` and friends), not
from a port — another shape the census cannot see.

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

Zeroed, 16-byte-aligned storage. Contents never compared against the DLL (§9).
One of the 28 — `?_loader@LoaderLoop@Engine@@` — needs more than storage and
gets it (§2); the other 27 are plain BSS, proved so by the `.CRT$XC` sweep
rather than assumed from their section.

## 8. Deferred, blocked, and what is *not* covered

1. **CLOSED — the load-time construction question, both halves.** BOTH
   `?_loader` and `?_rng` are constructed at load, and both are now reproduced
   (`ConstructLoader`, `ConstructParticleRng`). The intermediate claim that
   `_rng` needed nothing was wrong and is corrected in §2, with the evidence
   that settled it. The `_rng` seed is `time()`-derived, so its VALUE need not
   match retail and a mismatch there is not a defect — but seeding at all is
   mandatory. What the
   `_loader` construction leaves open is *its vtable's two slots*:
   `?Run@Loader@Engine@@UEAAXXZ` and `?OnCompleted@Thread@System@@UEAAXXZ` are
   unported, so the loading thread would trap after cutover rather than run.
   Blocked on lane B; `OnCompleted` is worth two slots (§2).
2. **The twelve `??_G` deleting-destructor thunks** (§7) — blocked on lane B,
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
   vtables here (§1) — and, as `_loader` showed, so can this lane's own work.
   Re-run the generator before closing the wave; it re-runs the census for you
   and fails loudly on any new exported `.data` static whose size nobody has
   pinned, and on any newly-constructed static nothing reproduces (§2).

## 9. The shadow check — what it compares, and what it refuses to

`BANISHED_RT_MINT_SHADOW=1`. Runs twice: at `RtDataMintInit` (`phase=init`) and
at process exit from `standalone/main.cpp` (`phase=exit`), because which
singletons the process actually populates is itself ground truth that dies with
the DLL.

**Compared**

| | |
|---|---|
| every vtable's exported address | vs the recorded VA |
| all 429 slots, live | DLL's qword vs the recorded target |
| every filled slot | ours == the registry's port of *that same function*, and **≠ the DLL's address** |
| slot count, live | the qword past the last slot must not be a DLL pointer |
| `BaseTypeInfo` `name`/`parentName`/`fileExtension` | by `strcmp`, not by pointer |
| `BaseTypeInfo` `factoryFn`/`inPlaceFactoryFn` | DLL's vs the recorded VA |
| a CONSTRUCTED static's **non-null word shape** | ours vs the DLL's live object, qword by qword |

**Deliberately not compared**

* **The minted vtable's bytes against the DLL's.** A filled slot holds *our*
  function's address, a different number by construction. A byte comparison
  fails on exactly the slots that represent progress and passes on the ones
  that do not. The comparison that means something is the *mapping*.
* **Any storage symbol's VALUES.** All 28 are BSS the DLL only writes at
  runtime — comparing them is
  `reference_oracle_must_not_compare_unwritten_bytes.md` verbatim, returning
  100% green or 100% red while meaning neither. The report prints instead which
  of the 28 each side has actually written. For the one CONSTRUCTED static
  there *is* something to compare, and it is still not the values: our vtable
  pointer and our HANDLEs are different numbers from the DLL's by construction.
  What must agree is the **shape** — which qwords the constructor leaves
  non-null. That is weaker than a value comparison and says so, but it catches
  the failure that actually matters (a ctor that leaves the handles or the
  vtable null).
* **`BaseTypeInfo::next`.** The DLL's is built by its static-init order, ours by
  ours; they cannot agree and a mismatch would be noise.
* **`BaseTypeInfo::typeId` after `Factory::Startup`.** Startup overwrites the
  DLL's sentinel with the real CRC and never touches ours, so at `phase=exit` a
  difference is expected. Reported as `INFO`, never counted as a mismatch.

**Effectiveness control.** The summary separates *ground truth* comparisons
(recorded vs DLL — these prove the harvest, not the port) from *minted from OUR
ported code* (the only number that measures the port). If the second is zero it
says so in four lines of asterisks, because 429 green ground-truth rows with
none of our own code in the tables would otherwise read as a finished lane.

### Expected result, to be checked against the run

Predicted from the registry at census 843/585, so a difference is itself
informative — most likely another lane landing ports between now and the run,
which should move `minted from OUR` **up** and `traps` down by the same amount.

### RECORDED RESULT — 2026-09-08, exe stamp `2026-09-08 00:17:23`

`tools/rt_run.sh mintshadow --timeout 300 -- BANISHED_AUTOSTART=1
BANISHED_RT_MINT_SHADOW=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
BANISHED_PAUSEMENU_BUTTON=6` — **rc=0 after 30s, no crash log**, and BOTH phase
summaries printed (the shutdown crash lane M saw in the stress cell did not
appear on this route; a shutdown crash would have eaten the `phase=exit`
summary).

```
[RtMint] minted 46 vtables (429 slots: 78 ported, 238 unported-trap, 113 purecall-trap),
         33 BaseTypeInfo, 28 storage symbols (2992 bytes)
[RtMint] 14 of 46 vtables are COMPLETE; registration is off
[RtMintShadow] ground truth       : 429 slot comparisons vs the live DLL, 0 MISMATCH
[RtMintShadow] minted from OUR    : 78 slots, 0 MISMATCH
[RtMintShadow] minted as traps    : 351 (113 purecall by design, 238 not yet ported)
[RtMintShadow] complete vtables   : 14 of 46
[RtMintShadow] slot-count suspects: 0
[RtMintShadow] BaseTypeInfo       : 165 field comparisons, 0 MISMATCH,
                                    0 typeId INFO (init) / 33 (exit)
[RtMintShadow] storage            : 28 symbols, 3 written by the DLL (init) / 23 (exit)
[RtMintShadow] constructed statics: 0 of 2 built   [gated off; _CTOR=1 to build]
[RtMintShadow] symbols absent from the DLL: 0
[RtMintShadow] ==== clean: 0 problem(s) ====      at BOTH phase=init and phase=exit
```

Every predicted delta landed exactly:

| | predicted | measured |
|---|--:|--:|
| ground-truth slot comparisons | 315 -> 429 | **429**, 0 MISMATCH |
| `BaseTypeInfo` field comparisons | 70 -> 165 | **165**, 0 MISMATCH |
| vtables | 39 -> 46 | **46** |
| minted from OUR code | 71 + ~7 | **78** |
| suspects / absent / MISMATCH | 0 | **0** |
| complete vtables | at least 14 | **14 of 46** |
| `typeId INFO` | 0 at init, 33 at exit | **0 / 33** |

The `typeId` split is the `Factory::Startup` ordering predicted in this section
and now observed: at `RuntimePortInit` the DLL's own `BaseTypeInfo` objects
still carry the ctor sentinel, and by exit all 33 hold real CRCs while ours
still hold the sentinel — expected, reported as `INFO`, never counted.

**Caveat on the "minted from OUR" figure, now confirmed by measurement**: this
document's offline arithmetic said 39; the run says 78. The census does not
list a port registered by ADDRESS that no resolve site asks for — exactly the
twelve `??_G` thunks, which nothing reaches except the minted vtable itself.
`RtPortLookupAddr` finds them at runtime. Any figure computed here is a floor.

### And what the run FOUND

The run was not just a pass. Its storage-liveness line said
`?_rng@Particle@Graphics@@ DLL=WRITTEN` at `phase=init`, which contradicted
§2's claim that nothing constructs it — and §2 was wrong. See §2 for the
correction, the offset evidence that settled it, the initialiser
(`0x180149650`, `_time64`-seeded MT19937), and the sweep fix. **A verification
run that only confirms what you predicted has told you less than one that
does both.**

### RECORDED RESULT — the construction gate, same day, exe stamp `00:24:27`

`... BANISHED_RT_MINT_CTOR=1 BANISHED_RT_MINT_SHADOW=1 ...` — **rc=0, no crash**,
and with the constructions actually run:

```
[RtMint] constructed 3 of the 3 statics the DLL's CRT builds at load
[RtMintShadow] constructed statics: 3 of 3 built (shape compared on 3, 0 SHAPE MISMATCH, 0 SHAPE-INFO)   phase=init
[RtMintShadow] constructed statics: 3 of 3 built (shape compared on 3, 0 SHAPE MISMATCH, 1 SHAPE-INFO)   phase=exit
[RtMintShadow]   SHAPE-INFO ?_loader@LoaderLoop@Engine@@1VLoader@2@A +0x30: DLL set, minted null
[RtMintShadow]   FIRST-NONZERO ?_rng...: DLL byte +0x4 = 01 8e 9f 6a ...   (init, index 0)
storage ?_loader...  DLL=WRITTEN  minted=written
storage ?_rng...     DLL=WRITTEN  minted=written
```

All the mapping-gate numbers above held unchanged in this run
(429/0, 78/0, 165/0, 0 suspects, 0 absent, clean at both phases).

The single `SHAPE-INFO` is `Loader::_loop` at `+0x30`: retail's `LoaderLoop`
stores the loop it is loading there, and nothing hands OUR minted Loader a
loop, so it stays null. Informational by construction, not a mismatch — and
the reason the shape check has an INFO category at all.

### PREDICTED for the next run — address-keyed mint (round 6)

Recorded baseline to compare against: **437 slots / 47 vtables / 165
`BaseTypeInfo` fields / 0 MISMATCH / 0 suspects / 0 absent, clean at both
phases.** Measured on the generated table (same regex both sides, so the deltas
are exact even where an absolute count is ambiguous):

| | from | to | why |
|---|--:|--:|---|
| vtables | 47 | **48** | the unexported `0x1801554a8` |
| ground-truth slot comparisons | 437 | **442** | +5 slots |
| `minted from OUR code` | unchanged | **unchanged** | all 5 new slots are unported internals |
| traps | +5 | **+5** | the same 5 |
| `purecall` | unchanged | **unchanged** | none of the 5 is `_purecall` |
| `BaseTypeInfo` field comparisons | 165 | **165** | no new types |
| complete vtables | unchanged | **unchanged** | the new one has 5 traps |
| storage rows | 30 | **31** | `0x1801fc6d0` |
| MISMATCH / suspects / absent | 0 | **0** | must not move |
| NEW line | — | `carried by ADDRESS   : 1 row(s)` | the one row with no exported name |

A move anywhere else is unintended and should be treated as a finding, not as
noise.

**Any non-zero `MISMATCH`, `slot-count suspects` or `absent` is a real finding**
and a run showing one should be treated as failed.

## 10. How to run it

No new build target; the files are picked up by `standalone/*.cpp`.

```bash
BANISHED_RT_MINT_SHADOW=1 mingw32-make sa-run
grep -E "^\[RtMint" "C:/GOG Games/Banished/standalone_debug.log"
```

`BANISHED_RT_MINT_SHADOW=1` alone gives the mapping check. Add
`BANISHED_RT_MINT_CTOR=1` for the constructed-statics gate:

```bash
BANISHED_RT_MINT_CTOR=1 BANISHED_RT_MINT_SHADOW=1 mingw32-make sa-run
```

That builds `?_loader` for real — a suspended-then-resumed OS thread and three
events, exactly as the DLL's initialiser does — and compares its shape with the
DLL's live one. It does **not** register anything, so nothing else in the
process changes.

**Do not set `BANISHED_RT_MINT=1`** — that registers the minted data over the
DLL's and is for cutover only (§8.6).

Regenerating the tables (do this whenever another lane lands ports, and before
the wave closes) and re-checking the doors:

```bash
tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_mint_gen.py
tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_data_door_audit.py   # exits 2 on a raw-door read
```

Run them in that order: the audit reads the minted set out of the table the
generator writes, so a symbol added by a regeneration is only door-checked
afterwards.

It re-runs the census, re-decodes the DLL, cross-checks the round-1 harvest, and
exits non-zero writing nothing if any slot disagrees, any slot count cannot be
established, or any new exported `.data` static has no pinned size.
