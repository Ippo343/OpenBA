# Wave 9 — Lane D: the unnamed DLL internals, triaged

Produced 2026-09-07 on `wave/upgrade-path-complete`. Inventory and triage only — nothing
under `code/`, `engine/`, `runtime/`, `standalone/`, `dx11/` was edited, nothing was built,
the game was not run.

Per-address data: **[wave9_internals.csv](wave9_internals.csv)** (66 rows).

## The finding, first

**The 66 addresses are 14 distinct jobs, and 34 of them — 52% — are one container
template.**

`Collection::Array<T>` is a header-only template with no exports in *either* build (checked:
zero `Array@Collection` and zero `Set@Collection` matches in the profile DLL's 5,182-name
export directory). MSVC emitted one copy of each member per instantiation and the linker did
not fold them. So the `FUN_` pool's biggest contribution to the cutover surface is not 34
mysteries — it is **three functions written 34 times**.

The proof is mechanical, not a judgement call. Hashing the *mnemonic sequence* of each
function body (Ghidra script, whole-body instruction iteration) gives:

| mnemonic hash | ins | addresses sharing it | what it is |
|---|---|---|---|
| `8c81ee925f` | 29 | **11** | `Array<T>::~Array`, trivially-destructible `T` |
| `12d4c57876` | 32 | **6** | the same, with an element-destroy prologue |
| `fdb8d3531b` | 73 | 2 | `Array<T>::SetSize`, stride 8 / stride 4 |
| `058664e0af` | 74 | 2 | `SetSize`, stride 8 |
| `560b750a6d` | 77 | 2 | `SetSize`, stride 8 |
| `f32250c89e` | 79 | 2 | `SetSize`, stride 16 |

The eleven `8c81ee925f` bodies have **identical mnemonics and identical immediates**
(`[48,32,72,80,1600,64,8,72,80,48]`); only the RIP-relative displacements differ, which is
why their raw byte hashes all differ and a byte-level dedup would have missed this entirely.

Read out of the disassembly at `0x18001c750` (29 instructions), the whole family is:

```
Array<T>::~Array(this):
    p = this->_data;                                    // [RCX]
    if (p) {
        cs = *?_singleton@MemoryAllocator@System@@ + 0x640;
        EnterCriticalSection(cs);
        MemoryAllocator::Free(*_singleton, p);          // 0x1800c6c00, EXPORTED
        LeaveCriticalSection(cs);
    }
    this->_data = 0;                                    // +0x00
    *(int64*)(this + 8) = 0;                            // +0x08 _size, +0x0c _capacity
```

and `SetSize` (`0x18007a0d0`, 77 instructions) is one algorithm with three parameters —
element stride, alignment, and how an element is assigned:

```
Array<T>::SetSize(this, int newSize, const T* fill):
    old = _data; oldSize = (int64)(int32)_size;                              // MOVSXD
    if (newSize == _size) return;
    _size = newSize;
    if (newSize > _capacity) {
        _capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) _capacity = newSize * 3; // growth
        _data = MemAlloc((int64)(int32)_capacity * stride, align);           // MOVSXD
    }
    start = (old == _data) ? oldSize : 0;
    for (i = start; i < newSize; ++i) _data[i] = (i < oldSize ? old[i] : *fill);
    if (old != _data && old) { Enter; Free(_singleton, old); Leave; }
```

Two `MOVSXD`s in that body are exactly the case
`reference_ghidra_c_output_vs_disassembly.md` warns about — the decompiled C prints them as
zero extensions. Every body quoted here was read from the disassembly, not the C.

**The whole Array cluster's only external dependency is the exported allocator pair**
`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (`0x1800c7a50`) and
`?Free@MemoryAllocator@System@@QEAAXPEAX@Z` (`0x1800c6c00`), plus the allocator singleton's
layout (`0x1801fc718` = `?_singleton@MemoryAllocator@System@@0PEAV12@EA`, its
`CRITICAL_SECTION` at `+0x640`) and KERNEL32's `Enter`/`LeaveCriticalSection`. Nothing else.

## Corrections to WAVE9.md

1. **The count is 66, not 57.** The lane table and the §1 kind table both say 57;
   `wave9_unported_20260907.txt` says `internal 66` and lists 66 rows. The 309-entry gameplay
   residue's "~57 unnamed internals" is a *different* population — 44 of these 66 appear in
   it — so the two numbers were never measuring the same set and should not share a figure.

2. **Three of the 66 are not code.** `0x1801fc6d0`, `0x1801fe7c0` and `0x1801fe7e0` are in
   `.data`. Two of them are **interior fields of an exported object**:
   `?_loader@LoaderLoop@Engine@@1VLoader@2@A` sits at `0x1801fe7b8`, so `0x1801fe7c0` is
   `_loader+0x08` (the thread `runFlags`) and `0x1801fe7e0` is `_loader+0x28` (`hFinished`) —
   exactly what `engine/funcs/engine_funcs.h:377` already documents. These belong to **lane C
   (mint the data)**, not lane D. `0x1801fc6d0` is a genuinely unexported 8-byte slot wedged
   between `?_singleton@ExternalFactory@Object@@` (`0x1801fc6c8`) and
   `?_singleton@InternalFactory@Object@@` (`0x1801fc6d8`) — the Modification-DLL `HMODULE`
   that `code/Game/Game/game_main.cpp:365` calls `FreeLibrary` on. Mint 8 bytes.

3. **Four of the 66 are dead code behind a fallback, and all four addresses are wrong.** Each
   is the `else` arm of a `ResolveExport(name); if (!x) ResolveAddr(Module::Runtime, addr)`
   pair. `ResolveExport` consults the port registry first
   (`standalone/resolve_standalone.cpp:494`), and in each case the name is either a live
   registered port or a real DLL export — so the fallback never runs. That is fortunate,
   because none of the four addresses is a function entry point:

   | site | fallback address | what is actually there | the export's real address |
   |---|---|---|---|
   | `code/Game/Player/tool_base_toolui.cpp:96` | `0x180000000 + kRVA_Dtor` = `0x1801b800e` | **`.rdata`**, no data defined | `??1DialogInstance@UI@@UEAA@XZ` = `0x180114d40` |
   | `engine/funcs/engine_funcs.cpp:3144` | `0x18006a3e0` | mid-body of `FUN_18006a390` | `?GetPath@External@Object@@QEBA?AVFilename@System@@XZ` = `0x180006250` |
   | `code/Game/Player/fencedtool.cpp:373` | `0x18008f530` | mid-body of `FUN_18008f38c` | `?Abs@Scalar@Math@@QEBA?AV12@XZ` = `0x1800646b0` |
   | `code/Game/Player/fencedtool.cpp:378` | `0x18008f880` | mid-body of `FUN_18008f38c` | `??0Vector3@Math@@QEAA@MMM@Z` = `0x1800152d0` |

   Addresses verified against the shipped export directory with `pefile`, and as
   entry-point-or-not with `FunctionManager.getFunctionAt` in Ghidra. All four are absent
   from **both** residue runs, corroborating the static reading. The fix is four deleted
   branches, not four ports. Note the neighbouring `0x1800c6610` fallback for
   `?GetPathIdentifer@Filename@System@@` **is** correct — so this is a scattering of bad
   addresses, not a systematic offset, and the other fallbacks in the tree are worth the same
   check.

4. **`0x18007fbfc` is the DLL's own statically-linked MSVC `_beginthreadex`**, not engine
   code. `runtime/src/system/thread_win.cpp:129` reaches it through `ResolveRuntimeRaw`,
   which **neither consults the port registry nor records residue**
   (`standalone/resolve_standalone.cpp:486`). Its absence from both residue lists is a tool
   blind spot, not evidence of unreachability — every `System::Thread` construction goes
   through it. At cutover it is satisfied by calling MinGW/UCRT's own `_beginthreadex`; the
   CRT-ownership comment already in that file is a reason to check the thread exit path, not
   a reason to port 62 instructions of Microsoft CRT.

5. **`0x18001f470` is the only one of the 66 with a `profile_to_retail.csv` entry, and it
   maps to `FUN_1800517c0` — also unnamed.** PLAN §4.3 risk 4 stands, unchallenged and
   un-re-run. The profile build cannot name any of these, because `Collection` is header-only
   there too.

## How far the surface actually goes

Transitive call-graph closure from the 58 code roots, depth ≤ 3, skipping thunks and
externals: **176 functions** (58 roots, 41 at depth 1, 31 at depth 2, 25 at depth 3, 21
deeper). That number is the argument *against* "port everything the 66 call" and *for*
cutting at the exported boundary. The cut is clean for the container clusters — J1/J2/J3/J4's
entire depth-1 set is `MemAlloc`, `Free`, `MemFree` and `Allocate`, all exported — and it is
not clean at all for J9, whose depth-3 frontier contains `ExpandRelativePath` (834
instructions), `FindOrLoad`, `BinaryFileStream`, `CompressedStream` and `VisitorStream`.

This is the mechanism WAVE9.md's "porting a function can GROW the cutover surface" section
describes, measured for a whole lane rather than for one function: **porting J2 creates 6 new
lane-D rows and J8 creates 6 more**, because their element-destroy and cell helpers are
themselves unexported.

## Ranked work list

Ranked by *addresses retired per body written*, which is the only ratio that matters here.

| # | Job | Addrs | Disposition | Bodies | Depends on | Size |
|---|---|---|---|---|---|---|
| 1 | **J1** `Array<T>::~Array`, trivial element | **11** | SHARED | 1 | exported `Free`; allocator `_singleton` + cs `+0x640` | XS |
| 2 | **J3** `Array<T>::SetSize(n, fill)` | **16** | SHARED | 1 template, 6 spellings | exported `MemAlloc` / `Free` | S |
| 3 | **J2** `~Array` + element-dtor prologue | 6 | SHARED | J1 + one call | **+6 new rows**: `180011010`, `18001ce90`, `180034440`, `1800419b0`, `18005c550`, `180074100` (43–66 ins each) | S |
| 4 | **J5** `Collection::Set` / `Map` AVL | 5 | SHARED | 5 | `1800236a0`, `180023570`, `18002a170`, `18002a310` | M |
| 5 | **J4** `Array<T>::operator=` / deep copy | 2 | SHARED | 1 | J3 | S |
| 6 | **J14** `.data` globals | 3 | **lane C, not D** | 0 | two are `_loader+0x08` / `+0x28` on an exported object | XS |
| 7 | **J13** dead + broken fallbacks | 4 | **DELETE** | 0 | none | XS |
| 8 | **J12** `_beginthreadex` | 1 | **CRT swap** | 0 | none | XS |
| 9 | **J11** `Volume::Box` transform `0x18001f470` | 1 | PORT | 1 | `AsTransform3`; 151 ins, no allocation, pure math | S |
| 10 | **J2b** `~Array` via `eh_vector_destructor_iterator` | 1 | SHARED | J1 + CRT iterator | `18007ec98` | S |
| 11 | **J6** allocator free-under-lock `0x180009c30` | 1 | **lane E** | 1 | allocator layout; 24 ins | XS |
| 12 | **J7** `Graphics::Scene` quadtree grid | 5 | PORT | 5 (365 ins) | J3, `18005fb00`, `18005f4b0` | M |
| 13 | **J8** `Picking::Scene` cell grid | 5 | PORT | 5 (346 ins) | **+6 new rows** | M |
| 14 | **J10** Audio `EffectSlot` array + bulk serialize | 2 | **gated on lane A** | 2 | the Wave 8 Audio holdbacks; `180052640` makes an indirect call | M |
| 15 | **J9** `IO::Mod` value semantics | 3 | **HARD — own lane, or stay on the seam** | 3 (304 ins) | the resource-load stack, 834-ins `ExpandRelativePath` included | L |

**Rows 1–5 retire 40 of the 66 (61%) for five jobs.** Rows 6–8 retire 8 more for zero
porting. That leaves **18 addresses that are genuine individual porting work**, in four
clusters with named owners.

## Oracles

Lane D is unusually well placed here, because the shared jobs have a property no wave since 6
has had: **one body serves 11 or 16 call sites, so a single `RT_DIFF` registration is
exercised by all of them.** `calls=` cannot come back zero the way
`reference_uncalled_port_is_not_verified.md` describes unless the port is genuinely dead.

* **J1 / J2 / J2b** — replay is unsafe on a destructor (ours then theirs double-frees). Use
  the **ours-writes / theirs-reads** shape that closed `KeyMap::RegisterInput`: run ours, then
  assert `_data == 0` and `*(int64*)(this+8) == 0`, and let `BANISHED_REDZONE=watch` catch a
  wrong pointer handed to `Free`.
* **J3 / J4** — full replay works and is cheap: `SetSize` is a pure function of
  (`_data`, `_size`, `_capacity`, `n`, `*fill`). Snapshot the 16-byte header and the buffer,
  call ours, snapshot, restore, call theirs, compare. Heed
  `reference_oracle_must_not_compare_unwritten_bytes.md`: compare `n * stride`, **not**
  `_capacity * stride` — the tail past `_size` is never written by either side, and comparing
  it would fail 100% of calls and read like a catastrophic defect.
* **J5** — no replay (a second insert of the same key returns early, and both sides mutate one
  tree). Reuse the oracle already built for `0x180023060` / `0x180023170`: our writer, the
  DLL's readers.
* **J11** — pure, no allocation, no globals except two `.rdata` float constants. Ordinary
  `RT_DIFF` replay. The cheapest fully-verified port in the lane.
* **J7 / J8** — these mutate one shared grid, so they are WAVE9.md rows 27-28
  (`FindOrLoad` / `Clean`) in miniature: running ours and theirs mutates the one grid twice.
  They need a bespoke record-and-replay and should be scheduled *after* J1–J5, not before.

## What lane D asks of lane 0

Four rows are claimed unreachable on static grounds (J13) and one is claimed **reachable
despite being invisible to the residue tool** (J12, via `ResolveRuntimeRaw`). Lane 0's matrix
should confirm the first four stay absent and must **not** read J12's absence as a clearance.
Twenty-two of the 66 are absent from the gameplay residue run; that is a starting point for
"probably init-only", not a proof, for the reason WAVE9.md already states — residue answers
what the process *asks the DLL for*, not what it calls.

---

# Addendum: the resolve-fallback audit (correction 3, generalised)

Asked for after the main report, because correction 3 found four bad fallback addresses and
established there is no formula to correct them with. Full table:
**[wave9_resolve_fallbacks.csv](wave9_resolve_fallbacks.csv)** (166 rows).

## Result

**166 guarded fallback sites across the tree. 161 correct, 5 defective.** One of the five is
materially worse than the four already reported, and it is worse in the direction that does
not crash.

| # | Site | Export the primary asks for | Fallback address | What is actually at the fallback address | Verdict |
|---|---|---|---|---|---|
| 1 | `engine/funcs/engine_funcs.cpp:2855` | `?FindCheckBoxes@RadioElement@UI@@QEAAXXZ` → `0x180047c00`, **1 param** | `0x1800d5750` | **A real exported entry point — the WRONG OVERLOAD.** `?FindCheckBoxes@RadioElement@UI@@AEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z`, private, **2 params** | **WRONG-ADDRESS, executes** |
| 2 | `engine/funcs/engine_funcs.cpp:3144` | `?GetPath@External@Object@@…` → `0x180006250` | `0x18006a3e0` | mid-body of `FUN_18006a390`, `+0x50` | WRONG-ADDRESS |
| 3 | `code/Game/Player/fencedtool.cpp:373` | `?Abs@Scalar@Math@@QEBA?AV12@XZ` → `0x1800646b0` | `0x18008f530` | mid-body of `FUN_18008f38c`, `+0x1a4` | WRONG-ADDRESS |
| 4 | `code/Game/Player/fencedtool.cpp:378` | `??0Vector3@Math@@QEAA@MMM@Z` → `0x1800152d0` | `0x18008f880` | mid-body of `FUN_18008f38c`, `+0x4f4` | WRONG-ADDRESS |
| 5 | `code/Game/Player/tool_base_toolui.cpp:96` | `??1DialogInstance@UI@@UEAA@XZ` → `0x180114d40` | `0x180000000 + kRVA_Dtor` = `0x1801b800e` | **`.rdata`**, no function, no data | UNRESOLVED-NAME |

Everything else — 161 sites — names an export whose address in the shipped DLL is exactly the
fallback address. Where a fallback exists in this tree, it is usually right.

## Site 1 is the one to read carefully

The other four fault on entry: a jump into `.rdata` or into the middle of a function body dies
loudly with a `standalone_crash.log` naming the address. Site 1 does not.

`0x1800d5750` is a legitimate function entry point that Ghidra resolves with **`params=2`**,
against `params=1` for the export the code asked for. The typedef at
`engine/funcs/engine_funcs.h:1725` is `void (*)(void* self)`, and the single caller
(`code/Game/Dialog/toolbardialog.cpp:1517`) passes one argument. So if this fallback ever
fired, the private overload would run with `RDX` holding **whatever the caller left there**,
interpreted as an `InternalPtr<Element>`, and perform radio-group tree surgery with it.

That is `reference_serialize_lazy_resolve_and_wrong_slot.md` exactly — "a slot read under the
WRONG name can hold a VALID pointer" — and it is why "is the fallback address an entry point?"
is not a sufficient check on its own. **The check has to be "is it the entry point of the
function this name denotes", and for an overload set that means comparing the full mangled
name, not the class and method.** Sites 2-4 would have passed a naive plausibility check too
(all three are in `.text`).

## Is any of the five live today?

No. All five primaries resolve non-null, verified two ways:

* `?Abs@Scalar@Math@@QEBA?AV12@XZ` and `??0Vector3@Math@@QEAA@MMM@Z` are **registered ports**
  (`runtime/src/math/rt_math_port.cpp:32,51`, Wave 1), and `ResolveExport` consults the port
  registry before the DLL (`standalone/resolve_standalone.cpp:494`).
* `?GetPath@External@Object@@…`, `?FindCheckBoxes@RadioElement@UI@@QEAAXXZ` and
  `??1DialogInstance@UI@@UEAA@XZ` are present in the shipped export directory (checked with
  `pefile` against `C:\GOG Games\Banished\Runtime-x64.dll`).
* None of the five fallback addresses appears in either residue run
  (`wave9_residue_autostart.txt`, `wave9_residue_gameplay.txt`).

**But "dead" here is a property of the export table, not of our code**, and that is the whole
point of auditing them now. A phantom name — one character wrong in a mangling, the failure
`reference_resolveexport_phantom_names.md` documents six times over and Wave 8 had to fix
again — makes the primary return null silently, and at that instant the fallback fires. Four
of these five then jump into `.rdata` or mid-function; the fifth quietly calls the wrong
overload. Someone already wrote the thought down at `engine/funcs/engine_funcs.cpp:4060`:

> *"...but a fallback that is only ever taken when something is already wrong is exactly where
> a wrong address hides."*

That comment sits above a fallback that is **correct**. The five below it were never checked.

## Triage — not fixed, per instruction

These are three different decisions, and only the first is obvious:

| Site | Recommendation | Why it is not just "delete it" |
|---|---|---|
| 3, 4 (`fencedtool.cpp`) | **DELETE the fallback** | Both names are registered ports. The fallback cannot fire without the Wave 1 registration itself being gone, at which point a null is a better failure than a jump into `FUN_18008f38c+0x1a4`. |
| 5 (`tool_base_toolui.cpp`) | **DELETE the fallback**, or correct `kRVA_Dtor` to `0x114d40` | Note the guard here is on `fn_DialogInstance_Dtor`, a *different* pointer — so this is an `else` arm, not a `!x` fallback, and whoever edits it must check that `fn_DialogInstance_Dtor` is genuinely always assigned before the vtable is installed. It is used the same way at 30+ other dialog sites, so this is a shared assumption, not a local one. |
| 2 (`engine_funcs.cpp:3144`) | **CORRECT to `0x180006250`** | `?GetPath@External@Object@@…` is DLL-only today (no port registers it), so this fallback is one phantom-name away from live, and `GetPath` is 15 instructions — correcting it costs nothing. |
| 1 (`engine_funcs.cpp:2855`) | **CORRECT to `0x180047c00`, and say so in a comment** | Deleting it is defensible too, but this is the one where a future reader most needs to find the note: `RadioElement::FindCheckBoxes` is an overload set, and the private 2-param sibling sits 0x8db50 away. Whichever way it goes, the record of *why* has to survive. |

## What should stop this recurring

The audit is thirty lines of Python and needs no Ghidra: for each guarded
`ResolveAddr(Module::Runtime, addr)` fallback, pull the mangled name assigned to the same
target, look the name up in the shipped export directory with `pefile`, and compare addresses.
It runs against the DLL on disk, so it works today and stops working the day the DLL is
removed — which argues for landing it as a `tools/` checker beside
`tools/export_name_audit.py` and `tools/dispatch_slot_map.py --check` **before** lane G, not
after. Both of those tools exist because the same class of defect was found by hand once and
then again.

Two caveats for whoever lands it, both hit during this sweep:

* **Pair the name by assignment target, not by proximity.** A window-based match produced 170
  candidates with names attached to the wrong addresses; requiring that the export name and
  the fallback address be assigned to the *same variable* produced 166 clean pairs.
* **An unconditional `ResolveAddr` is not a fallback.** `runtime/src/ui/element.cpp:243` was
  flagged by a widened context window and is not a defect at all — it is a deliberate
  by-address call to a header-only `Collection::Array` instantiation, with eighteen lines of
  comment saying so. A checker that cannot tell those apart will cry wolf on the entire J1/J3
  cluster from the main report.
