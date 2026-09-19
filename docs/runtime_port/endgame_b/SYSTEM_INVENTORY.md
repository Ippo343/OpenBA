# `System` rest inventory — ENDGAME B.3.4b prep (read-only)

Written 2026-09-18. Scope: everything under `runtime/src/system/` and `runtime/types/system/`
EXCEPT `String`/`Filename` (real classes since B.3.1) and EXCEPT `IO` (owned by the parallel
B.3.4a agent — not touched or re-measured here). Covers `MemoryAllocator`, `Thread`,
`CriticalSection`, `Random`, `Color`/`Value`, `Compression`, `Configuration`, `Timer`, `Crc`,
`Error`, `WideString`, `DateTime`, and `Time` (adjacent, uninventoried elsewhere, picked up here).

**Method.** Five sub-passes, each with its own comment/string-aware Python lexer script derived
from the existing `strip_comments_and_strings` in
`…/scratchpad/inv-io-callers/lexer.py` (blanks `//`, `/* */`, `"…"`, `'…'` before classifying),
copied into `…/scratchpad/inv-system/lexer.py` and specialised per family
(`alloc_classify.py`, `thread_classify.py`, `random_classify.py`, `timer_classify.py`; the
cross-cutting vtable/construction survey used Grep directly, no classifier needed). Every
occurrence is CALL (`name(`, a real call) / GUARD (`if(name)`/`name&&`/ternary, not followed by
`(`) / CACHED (assigned or passed as a bare value) / PRINTF-ARG (inside a
printf/fprintf/DLOG/TRACE/LOG argument list — the perf build strips these,
`reference_perf_build_strips_printf_arguments.md`) / DECL (typedef/extern/init lines in
`engine_funcs.h`/`.cpp`). CALL is the headline. Scanned `code/`, `engine/`, `standalone/`,
`runtime/`, `dx11/`, excluding `standalone/build*/` and `tests/unit/build/` (path-separator
anchored). **THE KEY METHOD RULE, applied throughout**: `engine_funcs.h` alone is not a complete
caller list for a `System` class — `Thread`/`CriticalSection`/`Compression`/`Crc` and
`Configuration`'s singleton slot are all invisible that way, reached instead through file-static
`s_fn_*`/`RT_EXPORT_ONCE` caches resolved directly by mangled name. Every class below was swept
both ways: `engine_funcs.h` AND a raw `@Class@System@@` mangled-string grep of the whole tree.

**Headline**: eleven classes/free-function families measured. `MemoryAllocator` alone is ~1,163
CALL sites (524+6+616+16+17-ish third-surface, see §4) — bigger than the entire IO caller
surface `IO_CALLER_INVENTORY.md` found. Exactly ONE `System` class is polymorphic: `Thread`
(§2). Its minted vtable is governed by the same retirement condition as IO's fifteen tables
(`B34_RULES.md` §4.2/§4.3): a ctor/dtor door may not become a real C++ constructor while anything
still stamps or reads the MINTED table. **Coordinator correction (2026-09-18):** the first draft
of this file said the conversion is "blocked until the DLL is gone entirely" because "retail
itself still stamps `0x180155f20` from inside the DLL". That was read off the `DLL PRESENT` half
of a two-route comment in `thread_win.cpp:114-137`; that route has not existed since ENDGAME A.1
(2026-09-13 — the exe never loads Runtime-x64.dll). The only live route is the comment's
`CUTOVER` half. The real blocker set is small and entirely inside `runtime/`: see §2.

---

## 1. Class list

| class | type header | POD/class | asserts | source files | body count | export rows | addr rows |
|---|---|---|---|---|---|---|---|
| `MemoryAllocator` | `runtime/types/system/MemoryAllocator.h` (355 lines) | `struct` — `MemoryTag`(8B), `FreeTag`(8B), `MemoryRange`(0x18), `MemoryAllocator`(0x670), `RtSysMemPlatform` (fn-ptr table, no asserts) | `sizeof`+12 `offsetof` on `MemoryAllocator`; `sizeof` on the other three | `allocator.cpp` (1503 ln), `allocator_win.cpp` (136 ln) | ~28 | 22 (`allocator.cpp`) + 6 (`allocator_win.cpp`) = 28 | 5 |
| `Thread` | `runtime/types/system/Thread.h` | `struct`, 0x30 bytes, `vtable`+0x00/`runFlags`+0x08/`hThread`+0x10/`hStart`+0x18/`hStop`+0x20/`hFinished`+0x28 | `sizeof`+5 `offsetof` | `thread_win.cpp` (331 ln) | 8 registered + 1 unregistered (`IsRunning`) | 10 (incl. by-name `ThreadFunc`) | 1 (`0x1800ca440`, `ThreadFunc` again) |
| `CriticalSection` | `runtime/types/system/Thread.h` (same file) | `struct`, 0x28 bytes, opaque `uint8_t[0x28]` (no field asserts beyond size — it wraps an opaque Win32 `CRITICAL_SECTION`) | `sizeof` only | `thread_win.cpp` (shared) | 4 | (counted in Thread's 10) | — |
| `Random` | `runtime/types/system/Value.h:48-51` | `struct`, 2500 (0x9c4) bytes: `index`+0x00, `mt[624]`+0x04 | `offsetof(mt)`+`sizeof` | `value_types.cpp` (531 ln, shared) | 10 | 10 | 0 |
| `Color` | `runtime/types/system/Value.h:90-92` | `struct`, 4 bytes: `value` (packed B,G,R,A LE) | `sizeof` | `value_types.cpp` (shared) | 1 (`Serialize`) | 1 | 0 |
| `Value` | — | **does not exist** as a separate type — Value.h is a holdover filename covering Random/Time/Color/DateTime/Configuration bodies; confirmed by full read | n/a | n/a | n/a | n/a | n/a |
| `Time` | `engine/types/common.h:12` (moved out of `Value.h` in B.3.3 step 6 to dodge a redefinition clash with `runtime/types/math/Math.h`) | `struct`, 8 bytes: `ticks` | `sizeof` | `value_types.cpp:206-246` (shared) | 3 (ctor, `AsSeconds`, `Serialize`) | 3 | 0 |
| `DateTime` | none — treats `this` as an opaque `FILETIME` | n/a | n/a | `value_types.cpp` (shared) | 3 | 3 | 0 |
| `Compression` | none | n/a | n/a | `compression.cpp` (632 ln) | 3 | 3 | 1 |
| `Configuration` | none — "no instance data, every member is `static`" (`Value.h` file comment) | n/a | n/a | `configuration.cpp` (741 ln) + ctor in `runtime/src/engine/engine_lifecycle.cpp` | 6 (5 in configuration.cpp + ctor elsewhere) + 1 addr-only (`ConfigMap_Find`) | 5 | 5 + 1 addr-only |
| `Timer` | none — pure static-function class over file-scope globals | n/a | n/a | `runtime/src/system/timer.cpp` (343 ln) + `timer.h` (48 ln, both in `runtime/src/system/`, NOT `runtime/types/system/`) | 10 | 10 | 0 |
| `Crc`/`CRC` | none | n/a | n/a | `crc.cpp` (71 ln) | 1 (`Compute(void*,int,uint)`) + 1 unregistered sibling overload (`Compute(char*,uint)`) | 1 | 1 |
| `Error` | none — fully static | n/a | n/a | `error.cpp` (579 ln) | 3 (`SetText`, `Display`, plus a homed `String::StrAppend`) | 3 | 3 |
| `WideString` | `runtime/src/system/widestring.h` (46 ln — a `src/`-local header, deliberately NOT under `runtime/types/system/` so it can't be typedef'd interchangeably with `System::String`) | `struct`/class, small POD wrapper | not fully re-derived this pass (out of the allocator/thread deep dives) | `widestring.cpp` (599 ln) | 8 | 8 | 8 |

`runtime/types/system/*.h` is exactly five files: `Thread.h`, `MemoryAllocator.h`, `Filename.h`,
`String.h`, `Value.h`. No `Timer.h`/`Crc.h`/`Error.h`/`WideString.h`/`Compression.h`/
`Configuration.h`/`DateTime.h` exists — those seven classes have zero externally-visible struct.

`runtime/src/system/*.cpp` — 15 files, matching `B33_RULES.md` §8's count: `allocator.cpp`,
`allocator_win.cpp`, `closure_batch2b.cpp`, `compression.cpp`, `configuration.cpp`, `crc.cpp`,
`error.cpp`, `filename.cpp`, `rt_system_port.cpp`, `setupdir_helpers.cpp`, `string.cpp`,
`thread_win.cpp`, `timer.cpp`, `value_types.cpp`, `widestring.cpp`.

---

## 2. Polymorphism

**Exactly one polymorphic `System` class in scope: `System::Thread`.** `CriticalSection` is
confirmed a plain `struct`, no vtable — `runtime/types/system/Thread.h:30` states it outright
("the class has no vtable, so there is no ABI concern beyond size and alignment"), and no
`??_7CriticalSection@System@@6B@` row exists anywhere. Every other class in §1 is either a POD
value type with no vtable (`MemoryAllocator`, `Random`, `Color`, `Time`) or a fully-static class
with no instance at all (`Compression`, `Configuration`, `Timer`, `Crc`, `Error`, `DateTime`,
`WideString`).

**`??_7Thread@System@@6B@` — the ONLY `??_7…@System@@6B@` row in `standalone/rt_data_mint_tables.inc`**
(verified by grepping the literal substring `@System@@6B@` in that file — one hit):

| dll addr | slots | NAMED | UNNAMED/PURECALL |
|---|---|---|---|
| `0x180155f20` | 2 | 1 — slot 1, `?OnCompleted@Thread@System@@UEAAXXZ` → `0x18000c590` | 1 — slot 0, `0x18007ec28`, `RT_MINT_PURECALL` |

(`rt_data_mint_tables.inc:3711-3717` for the `kVSlots140[]`/`kVTable140[]` bodies, `:4024` for
the table row.)

**No `RT_DELDTOR` row named `"System::…"` exists** in `runtime/src/object/deleting_dtor.cpp`
(grepped `System::` — the only 3 hits, `:1046,1104,1167`, are prose comments about
`System::String` refcounting inside unrelated Object/Graphics/UI dtor bodies, not table rows).
Consistent with Thread never being virtually-`delete`d through a slot-0 deleting dtor in this
tree — its ctor/dtor are registered as plain `??0`/`??1` complete-object functions
(`thread_win.cpp:312-318`), not a `??_G` deleting dtor.

**No `?_typeInfo@…@System@@` row exists anywhere under `standalone/`** (grepped `_typeInfo@`
whole-tree in `standalone/`, filtered for `System` — zero). Structurally expected: `System`
classes are not reflected `Object::External` subclasses, so this axis produces no rows for
them — not an oversight.

**`cutover_manifest.inc`'s ten `System`-tagged `CUTOVER_REQUIRED` rows are all SCALAR statics,
none a vtable**: `Debug::_disableHandler` (2 doors), `Error::_errorString` (2), `Error::_hideDisplay`
(3), `Timer::_secondsPerTick` (20), `Configuration::_singleton` (5),
`MemoryAllocator::_singleton` (4), `Error::_stopApplication` (2), `Timer::_systemInitialTick` (2),
`Timer::_systemSecondsPerTick` (2), `Timer::_systemTicksPerSecond` (2), `Timer::_ticksPerSecond`
(2) — `cutover_manifest.inc:69-89`. **No `??_7Thread@System@@6B@` row appears in
`cutover_manifest.inc` at all** — the Thread vtable's cutover status is governed by
`thread_win.cpp`'s own comment (below), not this manifest.

**`standalone/reg_mint_promoted.cpp` has zero promoted rows for any `System` class.** The
`@System@@` grep hits there (`:19,2322,2758,2870,2957,2982,2986`) are all `RT_UNPROMOTABLE`
documentation blocks explaining why various scalar statics (`MemoryAllocator::_singleton`,
`Debug::_disableHandler`, `Error::_stopApplication`, `Timer::_ticksPerSecond`) are NOT promoted —
not `Thread`/`CriticalSection` table entries.

**Who stamps `0x180155f20` — the load-bearing finding for the ctor/dtor-door question.** TWO
live stampers, both ours, both inside `runtime/` (re-measured by the coordinator, non-comment
`git grep` for `??_7Thread@System@@6B@`, `0x180155f20`, `RtSystemThreadVftable`):
1. Our port: `RtSystem_Thread_Ctor` (`:171`) and `RtSystem_Thread_Dtor` (`:194`) both call
   `RtSystemThreadVftable()` (`:144-146`), which reads `RT_DATA_SLOT(void*, "??_7Thread@System@@6B@")`
   through the mint/registry — never invents a table.
2. ~~Retail itself: the DLL's own `??0Thread@System@@` stamps the same address with an in-DLL
   RIP-relative LEA.~~ **Not a live stamper.** This is the `DLL PRESENT` route of
   `thread_win.cpp:114-137`'s comment; the DLL has not been loaded since ENDGAME A.1, so no
   retail constructor ever runs. Kept struck through so the next reader of that comment does
   not make the same inference.
3. By-address readers: `standalone/rt_export_addrs.inc:4686` (`{0x180155f20,
   "??_7Thread@System@@6B@"}`), `runtime/src/io/persistence_worker_win.cpp:585-589`
   (`RtIo_PersistenceWorker_Dtor` re-stamps the base vptr on teardown via
   `ResolveAddr(Module::Runtime, 0x180155f20)` — a live, load-bearing read, not dead code — the
   persistence worker is a `Thread` subclass).

**Verdict (coordinator correction, 2026-09-18).** The first draft concluded "the ctor/dtor door
must NOT convert while the DLL-present route is live … later than B.3.4b" and cited
`B34_RULES.md` §6.5(d) for it; §6.5(d) says nothing of the kind (it is about the caller surface
being invisible to `engine_funcs.h`), and the DLL-present route is dead. The rule that DOES
apply is `B34_RULES.md` §4.3: *a ctor/dtor door does not convert if the member form would change
which vtable a live object wears while anything still reads the minted table.* For `Thread` the
complete list of things that stamp or read the minted table is: `thread_win.cpp` (our ctor and
dtor) and `persistence_worker_win.cpp:585-589` (the subclass dtor's base re-stamp, by address) —
plus the generated rows in `rt_data_mint_tables.inc:4024` / `rt_export_addrs.inc:4686`. That
stamper list is VERIFIED (coordinator grep). What follows is taken from §3 below and was NOT
re-verified: §3 lists objects that run the `Thread` base ctor and then overlay a table of their
own (`Persistence`'s worker via `singleton_ctors.cpp`, `Audio::Stream`, the loader thread built
in `standalone/rt_data_mint.cpp`) and one plain embedded `Thread` (`pathfinder_seed.cpp`). If
that is right, those derived objects — each still wearing a minted table — are what decides the
timing: **`Thread` takes a real constructor when the classes that derive from it do, not "when
the DLL is gone".** The 4b owner confirms §3's list before scheduling anything. What 4b CAN do
regardless is what 4a does for streams: declare the class, move the non-ctor bodies (`Start`,
`WaitForFinish`, `OnCompleted`, `ThreadFunc`) onto members, keep the doors delegating.

**The 0x180155f20 / "ThreadBaseVtable()" reconciliation** (STATUS.md vs B33_RULES.md §5 appeared
to conflict — resolved): `0x180155f20` IS the real, live Thread base vtable, cited today by the
three readers above. `ThreadBaseVtable()` was a SEPARATE, unrelated helper function that once
lived at `runtime/src/audio/audio_stream_win.cpp:303`, had zero callers (only a comment
referenced it), and was deleted in ENDGAME B.3.3 step 7 (`B33_RULES.md:231,638`,
`STATUS.md:149-150` both confirm the deletion). Current `audio_stream_win.cpp:381-387` is left
as a comment explaining the helper is gone but the address itself "stays, reached by
persistence_worker_win.cpp and by the mint/export tables." Two facts sharing a subject
("the Thread base vtable"), never the same artifact. No discrepancy remains.

**RESOLVED by the coordinator (2026-09-18), was UNKNOWN**: no factory-replay row names the
Thread table. `git grep -n 'Thread@System'` over every `rt_factories_*.inc` / `rt_factories_*.cpp`
under `runtime/src/` and `standalone/` returns zero hits, so no `RtFactOp::Vptr` (or `Call`) row
stamps or constructs a `System::Thread`.

---

## 3. Construction sites per class

**`System::Thread`** — three named-door constructing call sites (`thread_win.cpp:124-125`'s own
enumeration, verified):
1. `code/Game/Game/pathfinder_seed.cpp:130-147` — resolves `??0Thread@System@@QEAA@I@Z` via
   `ResolveExport` into `s_fn_ThreadCtor` (`:131`), calls it on a heap-embedded `Thread`
   subobject inside a larger pathfinder object (`pfV`, 0x1f8 bytes, own vtable `0x14015ef18`
   restamped after `:147`) — `code/Game` layer, heap.
2. `runtime/src/engine/singleton_ctors.cpp:411-448` — Persistence's worker-thread ctor path,
   calls `RT_EXPORT_ONCE(ThreadCtorFn, "??0Thread@System@@QEAA@I@Z")` at `:448` on a
   Persistence-derived object whose own vtable (`0x180155860`) overrides Thread's afterward
   (`:434`) — `runtime` layer, heap (Persistence is heap-owned).
3. `standalone/rt_data_mint.cpp:415-448` (`ConstructLoader`, gated by `g_loaderStorage`) — looks
   up `??0Thread@System@@QEAA@I@Z` via `RtPortLookupExport` (`:436`), constructs the Engine
   `Loader` singleton (`_loader`, static/global storage per `:418`) — `standalone` layer,
   static/global singleton, mirroring retail's own `ConstructLoader`/`atexit` pattern.
4. `runtime/src/audio/audio_stream_win.cpp:341` (`RtAudio_Stream_Ctor`) constructs a Thread base
   subobject inside `Audio::Stream` via a local `ThreadCtor` wrapper around `RT_EXPORT_ONCE` —
   `runtime` layer, embedded base subobject. Not one of `thread_win.cpp`'s own three named sites
   but confirmed live by direct read; add it to the enumeration.
5. `code/Game/Game/keystone_a_gamedtor.cpp` documents (destroys, does not construct) a
   worker-queue `Thread` at retail `0x1400a6720` whose dtor tail-calls `??1Thread@System@@`
   (`:362-363`) — the actual construction site for THIS particular queue Thread was not located
   in this pass; **UNKNOWN**, flagged not guessed.

**`System::CriticalSection`** — no CriticalSection is ever heap-allocated standalone; every
`??0CriticalSection` call seen (`pathfinder_seed.cpp:149`) constructs it EMBEDDED at a fixed
offset inside a larger object (`self+0x30`), matching the "no vtable, no independent identity"
POD design. Registered via `thread_win.cpp:312`.

**`System::Random`** — every construction is via `fn_Random_Default_Ctor` placement-construction
into an EXISTING struct's memory — never a fresh standalone heap/stack `Random`. All thirteen
CALL sites are `dl/lp + 0x194` (or `+0x118`) offsets into retail dialog/menu-loop structs (e.g.
`code/Game/Dialog/noiseui.cpp:212`, `newgamedialog.cpp:871,1121`, `mainmenudialog.cpp:256`,
`saveloaddialog.cpp:808`, `code/Game/Game/gameupdateloop_load.cpp:664,667`,
`game_main.cpp:724`, `worldsetup_reload.cpp:122`), i.e. `Random` is a member subobject of
retail dialog/menu structs, plus one raw-buffer pattern at `engine_funcs.h:3418-3426`
(`uint8_t rng[2520]` then `fn_Random_Default_Ctor(rng)`).

**`System::Color`** — no construction call anywhere (4-byte POD, no ctor); referenced only as
named struct members (`ambientColor`/`lightColor`/`fogColor` at `Weather+0x60/64/68`,
`code/Game/Game/weather.cpp:175-181`).

**`System::Configuration`** — singleton constructed once, `runtime/src/engine/engine_lifecycle.cpp`
(`RtSystem_Configuration_Ctor`, retail `0x1800c51b0`), stored into the file-static singleton
slot (see §5's hidden-cache finding below); no `code/Game` construction site — reached there
only through the four static String-returning functions.

**`System::Compression`** — constructed only inside `IO::CompressedStream`
(`runtime/src/io/compressed_stream_write.cpp:689`,
`RT_EXPORT_ONCE(CompressionCtorFn, "??0Compression@System@@QEAA@XZ")(p)`) — `runtime`-internal
only, never in `code/Game` or `standalone`.

**`System::MemoryAllocator`** — one process-wide singleton (`_singleton`, `0x1801fc718`, IS the
`Engine` instance's address per `MemoryAllocator.h:26-29` — the allocator is embedded at offset
0 of `Engine::Engine`). No per-call construction; every caller reaches it via
`g_pMemAllocSingleton` (member form) or the static free-function forms (§4).

**`Timer`/`Crc`/`Error`/`DateTime`/`WideString`** — no instance state (Timer/Crc/Error/DateTime
are pure static-function classes over file-scope globals or opaque `this`); `WideString` is
constructed at each of its call sites as a stack local (`runtime/src/application/application_win.cpp:343,360,362`,
`platform_startup_win.cpp:269,383`, `io/filesystem_find_win.cpp:581`,
`io/filesystem_setupdirs_win.cpp:278,347`, `io/object/external_path_win.cpp:470`,
`system/value_types.cpp:393` inside `DateTime::FormatPart`) plus dx11's own POD locals
constructed through its by-address import-table ctor (`dx11/src/devicedata.cpp:1711,1755`).

---

## 4. The allocator, in full

**Method**: `alloc_classify.py` (comment/string-aware) scanning `code/`, `engine/`,
`standalone/` only (`runtime/` excluded — receiver side, not a caller).

### 4.1 The four MemAlloc/MemFree surfaces

| surface | signature | resolution | CALL | GUARD | CACHED |
|---|---|---|---|---|---|
| `fn_MemAlloc` (member) | `void*(void* self, uint64_t size, int align)` | `ResolveAddr(Module::Runtime, 0x1800c6e60)` (`engine_funcs.cpp:1370-1371`) — reaches `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z`, **unlocked** | **524** | 292 | 85 |
| `fn_MemFree` (member) | `void(void* self, void* p)` | thunks onto `fn_MemFreeStatic` today (`engine_funcs.cpp:1460-1468`'s two-arg form is now a lambda, not a live `0x1800c6c00` reader) | **6** | 4 | 0 |
| `fn_MemFreeStatic` (static) | `void(void* p)` | `ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z")` (`engine_funcs.cpp:1458-1459`) — reaches `0x1800c7ad0`, **locking** | **616** | 364 | 27 |
| `fn_MemAllocStatic` (static) | `void*(uint64_t size, int align)` | `ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")` — reaches `0x1800c7a50`, **locking** | **16** | 3 | 18 |

`fn_MemZeroFill` — **0 CALL anywhere**, but not unresolved: still resolved unconditionally at
`engine/funcs/engine_funcs.cpp:3647` (`ResolveAddr(Module::Exe, 0x14003f010)`). Dead on arrival.

Per-directory CALL split:

| directory | fn_MemAlloc | fn_MemFreeStatic |
|---|---|---|
| Component | 179 | 199 |
| Dialog | 126 | 106 |
| Game | 100 | 103 |
| Work | 25 | 90 |
| Action | 27 | 45 |
| Tutorial | 28 | 24 |
| Player | 17 | 21 |
| Goal | 8 | 9 |
| Entity | 5 | 8 |
| engine | 5 | 6 |
| standalone | 4 | 5 |

Representative sites:
```
code/Game/Action/action_base_cnr.cpp:547   void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
code/Game/Component/controller_live_base.cpp:92  fn_MemFree(g_pMemAllocSingleton, self);
code/Game/Action/action_base_hunt.cpp:149  fn_MemFreeStatic(oldBase);
code/Game/Component/tradecomponent_states.cpp:305  void* mem = fn_MemAllocStatic(0x58, 8);
```

### 4.2 The THIRD surface — 12 files with a private per-file cache of the static export

Bypassing `engine_funcs.h`'s `fn_MemAllocStatic` entirely, each independently resolves
`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` into a file-local static:

| file | var | resolve line | call sites | count |
|---|---|---|---|---|
| `code/Game/Action/action_pf_astar.cpp` | `s_fn_MemAllocSA` | :126 | :827 | 1 |
| `code/Game/Action/keystone_a_zone.cpp` | `s_fn_MemAllocSA` | :76 | :90 | 1 |
| `code/Game/Work/tradework_serialize.cpp` | `s_memAlloc` | :52,54-55 | :85,112 | 2 |
| `code/Game/Work/gatherrawmaterials.cpp` | `s_fn_MemAlloc` | :264-268 (IAT fallback 0x1401527e8) | :885 | 1 |
| `code/Game/Player/tool_base_highlight.cpp` | `s_MemAlloc` | :146 | :225,267 | 2 |
| `code/Game/Work/foresterwork.cpp` | `s_fn_MemAlloc` | :269-270 | :1167 | 1 |
| `code/Game/Action/action_pf_geom.cpp` | `s_memAlloc` | :52-54 | :555 | 1 |
| `code/Game/Work/pickup.cpp` | `s_x_memAlloc` | :370-372 | :413/415,553 | 2 |
| `code/Game/Player/tool_base_highlight_tess.cpp` | `s_memAlloc` (+wrapper `MemAlloc()`) | :69-71 | **none** | 0 (resolved but dead) |
| `code/Game/Component/keystone_a_restree.cpp` | `s_memAlloc` (+wrapper `MemAlloc()`) | :70,72-76 | :122,154 | 2 |
| `code/Game/Component/wellcomponent.cpp` | `s_fn_MemAllocStatic` | :239-240 | :313 | 1 |
| `code/Game/Player/tool_base_trade.cpp` | `s_memAlloc` | :54-56 | :353,383,404 | 3 |

**17 real call sites across 11 live files + 1 dead resolve.** Not an independent fourth
surface but a genuine third, separately-resolved route to the SAME retail static
(`0x1800c7a50`): `code/Game/Work/tradework.cpp:129` (`s_fn_MemAlloc =
reinterpret_cast<TrW_MemAlloc_t>(fn_MemAllocStatic);`) merely ALIASES the already-resolved
`fn_MemAllocStatic` pointer rather than re-resolving — its 2 calls (`:313,725`) are invisible to
a plain `fn_MemAllocStatic(` grep but are the same function pointer, not a separate surface.

**Correction to a prior inventory claim**: `IO_CALLER_INVENTORY.md:399` lists
`citizenai_leisure.cpp` under the `s_fn_MemAllocSA` pattern — checked directly
(`code/Game/Component/citizenai_leisure.cpp:107-109`), it actually calls the shared
`fn_MemAlloc`/`g_pMemAllocSingleton`, not a private cache. That doc entry is stale/incorrect.

### 4.3 `rtcoll::DoorAlloc`/`DoorFree` — a separate, fourth, containers-only surface

`runtime/src/collection/alloc_door.cpp:97-123` — reads exe dispatch slots `0x140152b60`/
`0x140152b68` PER CALL (never caches the contents, only the slot address — `:38-46` explains
why: `RedzoneAlloc_Install()` rewrites slot contents post-boot), falling back to
`ResolveAddr(Module::Runtime, 0x1800c7a50/0x1800c7ad0)`. Callers, confirmed containers-only:
`runtime/src/collection/array.cpp:174,185`, `linkedlist.cpp:174,179`, `set_core.cpp:78`,
`set_avl_pathid.cpp:137,140`, `avl_trees.cpp:174,181`, `introsort.cpp:380`,
`set_avl_e24.cpp:365`, `set_avl.cpp:412`, `runtime/src/io/modification.cpp:202,205`. No
`code/Game` file calls `DoorAlloc`/`DoorFree` directly.

### 4.4 Instrumentation

- **`standalone/alloc_stats.cpp`** (`BANISHED_ALLOC_STATS`/`BANISHED_ALLOC_BIG`) — pure
  accounting, hooks nothing itself; fed by the allocator's own locked entry points
  (`:1-5,28-30`). `g_liveBytes/g_totalAllocBytes/g_allocCount/g_freeCount` (`:34-37`) + a hashed
  `BlockRec` table (`:39-56`, 2M slots) + a per-caller `CallerRec` table (`:52-75`, 4096 slots).
  `StatsRecordAlloc`/`StatsRecordFree` (`:77-114`) run inside the allocator's own critical
  section, use `VirtualAlloc`-backed storage (never re-enters the engine allocator).
- **`standalone/redzone_alloc.cpp`** (`BANISHED_REDZONE`) — `RedzoneAlloc_Install()`
  (`:299-366`) swaps the global function pointers `fn_MemAlloc`/`fn_MemFreeStatic` (`:340-341`,
  saving originals at `:331-332`) AND rewrites the exe `.rdata` dispatch-slot CONTENTS at
  `0x140152b60`/`0x140152b68` in place (`:346-349`) so the twelve §4.2 per-file callers are
  ALSO shimmed — `:230-238`'s comment names `tool_base_highlight*.cpp`/`work_act_*.cpp`
  explicitly as the reason a half-shimmed heap produced a phantom "corrupt decal vertex pool."
  `ShimAlloc` (`:201-228`) tracks each block with a canary tail if `g_pad>0`.

### 4.5 Exe dispatch-slot registrations

`standalone/address_registry.cpp:1324-1327`:
```cpp
RegisterDispatchSlot(0x140152b60, (uintptr_t)ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
RegisterDispatchSlot(0x140152b68, (uintptr_t)ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
```
Confirmed exactly as `alloc_door.cpp`'s header comment claims. `fn_MemAlloc`/`fn_MemFreeStatic`
themselves are NOT separately dispatch-slot-registered — they resolve directly via
`ResolveAddr`/`ResolveExport` at `EngineFuncsInit()` time (`engine_funcs.cpp:1370-1371,1458-1459`),
a Runtime-side seam, not an exe-side dispatch-slot row.

### 4.6 How many distinct retail bodies, and do the forms share one

Four retail addresses in play:
- `0x1800c6e60` `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z` — member, **unlocked** raw
  core (`allocator.cpp:419`) — bound to `fn_MemAlloc`.
- `0x1800c6c00` `?Free@MemoryAllocator@System@@QEAAXPEAX@Z` — member, unlocked, registered
  "so the pair cannot drift apart" (`allocator.cpp:1283`) but **no live reader today**
  (`fn_MemFree` now thunks onto `fn_MemFreeStatic` instead).
- `0x1800c7a50` `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` — static, **locking**
  (`Get()`+lock+`Allocate`+unlock, `allocator.cpp:677-683`) — bound to `fn_MemAllocStatic`,
  reached by all twelve §4.2 files and `DoorAlloc`'s fallback.
- `0x1800c7ad0` `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` — static, locking — bound to
  `fn_MemFreeStatic` and `DoorFree`'s fallback.

**The member and static forms do NOT call the same top-level address.** They differ by exactly
one thing: the critical section. `fn_MemAlloc`'s unlocked `Allocate` core and the static
wrapper's locked path both terminate in the same underlying free-list logic
(`allocator.cpp:1344-1360`: "one arena, two entry bodies... the member pair gets the composed
locked entry points [via `g_pMemAllocSingleton`'s own internal locking, not the static
wrapper]... the static pair gets `rtalloc::MemAlloc/MemFree`, which take the critical section
themselves exactly as retail's statics do" — comment paraphrased, re-verify exact wording before
quoting in a commit). **UNKNOWN**, flagged: whether retail's own member-form call sites are
provably single-threaded (making the lock-skip safe) was not established by any in-tree comment.

### 4.7 Type header

`runtime/types/system/MemoryAllocator.h` (355 lines): `MemoryTag{uint64_t bits}` (8B, bit
layout documented inline `:47-64`); `FreeTag{uint64_t link}` (8B, `:65-70`);
`MemoryRange{base+0x00,end+0x08,committed+0x10}` (0x18); `MemoryAllocator` (0x670) — `ranges`
+0x000, `rangeCount`+0x008, `rangeCapacity`+0x00c, `minRangeSize`+0x010, `maxTotalSize`+0x018,
`totalReserved`+0x020, `freeLists[192]`+0x028, `minBin`+0x628, `allocatedBytes`+0x630,
`allocCount`+0x638, `freeTagCount`+0x63c, `lock[0x28]`+0x640, `pageSize`+0x668 — 12
`offsetof` asserts + `sizeof==0x670`. `RtSysMemPlatform` is a portability fn-ptr table
(reserve/commit/decommit/release/pageSize/lock*), not binary-pinned. `_singleton`
(`0x1801fc718`) IS the `Engine` instance's own address (allocator embedded at offset 0).

---

## 5. Every other System family

### 5.1 `System::Thread` / `System::CriticalSection`

Registered bodies (`thread_win.cpp:307-330`, `RtSystemThreadRegister()`): `OnCompleted` (no-op,
fills Thread vtable slot 1), `CriticalSection::{Ctor,Dtor,Lock,Unlock}`,
`Thread::{Ctor,Dtor,Start,WaitForFinish,ThreadFunc(by name+by addr)}`. **A ninth Thread export,
`?IsRunning@Thread@System@@QEBA_NXZ` (0x18000c5a0), is listed in `standalone/rt_export_addrs.inc:289`
but has NO `RegisterRuntimeExport` row and NO resolver call anywhere in the tree** — genuinely
unported and uncalled, not part of the "8 exports" framing used in file banners.

Caller surface — **eight files**, all resolving by mangled name into file-static caches (none
visible via `engine_funcs.h`), one more than the five previously named, plus a by-address ninth:

| file | ops | CALL sites |
|---|---|---|
| `code/Game/Action/action_pf_astar.cpp:123-126` | Lock,Unlock,Start | 7 (`:795,815,826,844,846,985,995`) |
| `code/Game/Action/keystone_a_zone.cpp:73-77` | Lock,Unlock,Start | 6 (`:89,107,109,168,171,173`) |
| `code/Game/Game/keystone_a_gamedtor.cpp:102-109` | Lock,Unlock,Start,WaitForFinish,CS-dtor,Thread-dtor | 7 (`:362,363,386,390,398,400,401`) — the ONLY `code/Game` `WaitForFinish` site |
| `code/Game/Game/pathfinder_seed.cpp:131-135` | Thread-ctor,CS-ctor,Lock,Unlock | 4 (`:73,87,147,149`) — the only `code/Game` construction site |
| `runtime/src/audio/audio_stream_win.cpp:300-317` | Thread-ctor,Thread-dtor (wrapped `ThreadCtor`/`ThreadDtor`) | 2 (`:341,411`) |
| `standalone/rt_data_mint.cpp:434-471` | Thread-ctor,Thread-dtor (`ConstructLoader`/`RtMintLoaderTeardown`) | 2 (`:460,425`) |
| `standalone/loader_loop_stubs.cpp:81-95` | WaitForFinish | 1 (`:93`, gated `Opt().syncEngineLoader`, default true) |
| `runtime/src/io/persistence_worker_win.cpp:585-588` | base-vtable re-stamp, BY ADDRESS not by name | 1 (`:587`) |

**Correction**: `code/Game/Game/tornado.cpp` was named in the original B34_RULES brief as a
Thread/CriticalSection caller — direct grep found ZERO matches for `Thread`/`CriticalSection` in
any form in that file. Drop it from the caller roster; the brief's claim is unsupported by the
current source.

`runtime/types/system/Thread.h`: `CriticalSection{uint8_t _storage[0x28]}` (sizeof 0x28, opaque,
no per-field assert); `Thread{vtable+0x00,runFlags+0x08(+pad),hThread+0x10,hStart+0x18,
hStop+0x20,hFinished+0x28}` (sizeof 0x30, 5 offset asserts + 1 size assert).

`thread_win.cpp` is a POD wrapper over Win32 primitives, not a hand-rolled reimplementation:
`CriticalSection` ops are direct `InitializeCriticalSection`/`SetCriticalSectionSpinCount(…,5000)`/
`EnterCriticalSection`/`LeaveCriticalSection`/`DeleteCriticalSection`; `Thread` builds via
`_beginthreadex(CREATE_SUSPENDED)` + 3 `CreateEventW` handles, and `RtSystem_Thread_ThreadFunc`
(0x1800ca440) faithfully reproduces `WaitForMultipleObjects({hStart,hStop})` → vtable slot 0 →
reload vtable → slot 1 (`OnCompleted`) → loop. All eight caller files' `ResolveExport` calls
resolve the SAME mangled strings this file registers, so they land on `RtSystem_*` bodies once
the port registry takes precedence (`reference_resolveaddr_consults_port_registry_first.md`).

Zero-CALL: `IsRunning` (genuinely zero-caller AND zero-port, unlike everything else in this
subsection). `OnCompleted` has zero direct C++ calls but is dispatched every worker cycle
through the vtable slot — not dead, just never called by name.

### 5.2 `System::Random` / `System::Color`

`value_types.cpp:494-530` registers 20 exports total across Random(10)/Time(3)/Color(1)/
DateTime(3)/Configuration(3). Only 5 `fn_Random_*` pointers exist in `engine_funcs.h`
(`Rand`,`RandFloat`,`RandFloatTol`(`:2137`, declared `extern "C"` directly, not the usual
typedef pair),`RandTol`,`Default_Ctor`); `RandBound`/`RandRange`/`Serialize` are registered but
have NO exe-side dispatch pointer at all — no caller needs them yet.

| pointer | CALL | GUARD | CACHED |
|---|---|---|---|
| `fn_Random_Rand` | **75** | 28 | 27 |
| `fn_Random_RandFloat` | **38** | 11 | 23 |
| `fn_Random_RandTol` | **25** | 6 | 12 |
| `fn_Random_Default_Ctor` | **13** | 5 | 2 |
| `fn_Random_RandFloatTol` | **31** | 0 | 5 |
| `fn_System_Color_Serialize` | **6** | 3 | 3 |

(Re-measured; matches a prior pass exactly.) Per-directory (Rand family): Component 40+25+8+22,
Game 23+9+12+9, Dialog 5+1+2, Work 4+2, Player 1, engine 1+1+2, standalone 1.
`fn_System_Color_Serialize` (`engine_funcs.h:5559-5560`): Game 5 (`weather.cpp` family),
Component 1 — confirmed it belongs in this bucket, not a separate "Serialize bodies" bucket a
prior pass filed it under. **No other `fn_Color_*` pointer exists.**

Zero-CALL: none among the nine measured pointers (`fn_Configuration_CurrentVersion`/
`CurrentBuildId` are the lowest, at 1 each — see §5.4).

### 5.3 `System::Compression`

**A second invisible-to-`engine_funcs.h` case, like Thread/CriticalSection.** No `fn_Compression_*`
pointer exists anywhere in `engine_funcs.h` (grepped — zero hits). `compression.cpp` (632
lines) registers 3 exports (`RtSystemCompressionRegister`, `:595`): `Decompress` (+1 addr row,
`0x1800c4660`), `Compress`, the default ctor. Dtor is deliberately left unregistered — teardown
is inlined into `CompressedStream::Release`, already ported (owned by the IO agent). Only
in-tree callers: `runtime/src/io/compressed_stream_read.cpp:300,404` and
`compressed_stream_write.cpp:474,689`, all via `RT_EXPORT_ONCE` mangled-name resolution local to
those two files — 4 real call sites total, all inside IO's own territory, none in `code/Game`.

### 5.4 `System::Configuration`

`configuration.cpp` (741 lines) registers 5 name+addr pairs
(`RtSystemConfigurationRegister`, `:691`): `SetCommandLine`, `ParseCommand`, `ParseValue`,
`GetCommandLineString`, dtor. Plus one addr-only row with no export name,
`RegisterRuntimeAddr(0x18002ace0, RtSystem_ConfigMap_Find)` (the map's BST find, unexported in
retail). Ctor registered separately in `runtime/src/engine/engine_lifecycle.cpp:995`, not in
`configuration.cpp`.

**A hidden singleton cache, independently duplicated in THREE translation units**, none visible
via `engine_funcs.h`: `?_singleton@Configuration@System@@0PEAV12@EA` (retail `0x1801fc6f8`),
wrapped by a `ConfigurationSingletonSlot()` helper around `RT_DATA_SLOT`, written/read
independently at `runtime/src/engine/engine_lifecycle.cpp:282-284`,
`runtime/src/io/filesystem_setupdirs_win.cpp:194,199`, `runtime/src/io/filesystem_startup.cpp:83,126`.

`engine_funcs.h` pointers: `fn_Configuration_GetVersionString`=**3** CALL (Dialog),
`fn_Configuration_CurrentVersion`=**1** (Dialog), `fn_Configuration_CurrentBuildId`=**1**
(Dialog) — all re-measured, matching the prior pass.

### 5.5 `System::Timer` / `System::DateTime` / `System::Time` / `System::Crc` / `System::Error` / `System::WideString`

**Timer** — `runtime/src/system/timer.cpp` (343 ln, confirmed in `runtime/src/system/`, not
`runtime/types/system/`) registers 10 exports, 0 addr rows (`:330-343`). No `fn_Timer_*` pointer
in `engine_funcs.h` at all — every caller reaches it via `RT_EXPORT_ONCE`/`ResolveExport` on the
literal mangled string:

| export | CALL | where |
|---|---|---|
| `SecondsToTicks` | 3 | `engine/loops.cpp:389`, `graphics/wave28_lane_partsys.cpp:322`, `system/value_types.cpp:225` |
| `TicksToSeconds` | 3 | `engine/loops.cpp:392`, `wave28_lane_partsys.cpp:319`, `value_types.cpp:234` |
| `Startup` | 1 | `engine/engine_lifecycle.cpp:525` |
| `CurrentTimeTicks` | 1 | `engine/loops.cpp:1453-1456`, file-static cache `nowTicks` — the hidden-cache pattern again |
| `Shutdown`,`PlatformStartup`,`PlatformShutdown`,`GetTicks`,`CurrentTime`,`Assign` | 0 each | zero named callers anywhere |

No instance state — five file-scope statics published via `PublishTimerStatics()`.

**DateTime** — lives inside `value_types.cpp` (no dedicated file). 3 exports: `AsString`
(forwards internally to the other two, `:293-294`), `DateAsString`, `TimeAsString` (share
`RtSystem_DateTime_FormatPart`, Win32 `FileTimeToLocalFileTime`/`GetDateFormatW`/
`GetTimeFormatW`). Only `fn_DateTime_AsString` is `engine_funcs.h`-visible:
**CALL=1**, `code/Game/Dialog/saveloaddialog.cpp:322`. `DateAsString`/`TimeAsString` have zero
external callers (only `AsString`'s internal forwarding).

**Time** — distinct from Timer. Type moved to `engine/types/common.h:12` in B.3.3 step 6 (was in
`Value.h`, collided with `runtime/types/math/Math.h`'s own `Time`). 3 exports in
`value_types.cpp:509-511` (ctor-from-float delegates to `Timer::SecondsToTicks`, `AsSeconds`
delegates to `Timer::TicksToSeconds`, `Serialize`). `engine_funcs.h` pointers, re-measured,
matching a prior pass exactly: `fn_Time_Ctor_Float`=**111** CALL / 82 GUARD / 5 CACHED,
`fn_Time_AsSeconds`=**47** / 17 / 18, `g_fn_Time_Serialize`=**32** / 17 / 3. Overwhelmingly
`code/Game/Component` (citizen AI/growth/disease/fire/herd/tornado state machines) and
`code/Game/Dialog`/`Game`; 1 site each in `standalone/menudialogloopt_stubs.cpp` and
`standalone/reg_code_lane4.cpp`; no dx11 usage.

**Crc** — `crc.cpp` (71 ln, no header). 1 export (`?Compute@CRC@System@@SAIPEBXHI@Z`, `:68`) + 1
addr row (`0x1800c5490`, `:69`). **Its only real caller in the whole tree** is a file-static
cache, `runtime/src/io/persistence_saveinfo_job.cpp:326-329` (`RtResolveCached`) — again
invisible to `engine_funcs.h`. **THREE independent CRC-32 table reimplementations that never
call this door**: `persistence_worker_win.cpp:174-213` (own `CrcTable()`/`Crc32Chunked()`),
`persistence_saveinfo_job.cpp:191-213` (a SECOND, separate `CrcTable()` in the same file used at
`:336`), `runtime/src/object/type_reflection.cpp:177-194` (`BuildCrcTable`, for type-id CRCs).
The DLL's exported `?_crcTable@CRC@System@@0PAIA` (0x1801f9080) is minted zeroed
(`rt_data_mint_tables.inc:4681`) and deliberately never read — our own `crc.cpp` builds its own
table. A second overload, `?Compute@CRC@System@@SAIPEBDI@Z` (0x1800c5430, `const char*` form),
exists only in `rt_export_addrs.inc:3311` — never registered, never called, unported sibling.

**Error** — `error.cpp` (579 ln, no header). 3 exports (`SetText`, a homed `String::StrAppend`,
`Display`) + 3 addr rows. Widest caller surface of any class measured this pass — **≈25 real
`SetText` call sites across SIX independent doors**, none reachable from `engine_funcs.h`:
`ErrorSetText` wrappers in `platform_startup_win.cpp:118-122` (2 calls), `input_platform_startup_win.cpp:113-117`
(7 calls), `video/device_startup.cpp:163-168` (4 calls); `allocator.cpp:142-153`'s
`FatalOutOfMemory` (1 call); `io/filesystem_win.cpp:108,134` (`RT_EXPORT_ONCE`, 1 call); and
dx11's own by-address import slot `g_im.ErrorSetText` (`dx11/src/contextdata.cpp:245,704`,
`dx11/src/devicedata.cpp` ×8 — 10 calls). `Display` has 2 calls (`dx11/contextdata.cpp:246,705`,
`allocator.cpp:145-148`). `HideErrorDisplay` is proven dead (no caller in any of 4 shipped
binaries per the file's own comment) and deliberately unregistered. `StrAppend` has zero
external callers (internal only).

**WideString** — `runtime/src/system/widestring.h` (46 ln, `src/`-local, deliberately NOT under
`runtime/types/system/` "so a caller cannot pass one where the other [`System::String`]
belongs"). **8 exports** (correction: a prior pass said 9), 8 addr rows: `SetLength`(private),
`Set`, `AsUTF8`, `CtorFromString`, `CtorFromWide`, `Dtor`, `StrLength`, `CtorFromChars`. Callers,
all via `RT_EXPORT_ONCE`/`ResolveExport` or dx11's by-address import table (again invisible to
`engine_funcs.h`): `Set`≥4 (`io/filesystem_createfile_win.cpp:355`,
`io/persistence_registry_win.cpp:299,927`, `value_types.cpp:394`), `AsUTF8`≥3+dx11×2,
`CtorFromString`=**7** (contradicts the file's own stale comment claiming only dx11+3 lane-FS
closures use it — `closure_batch2b_win.cpp:135`, `filesystem_createfile_win.cpp:143`,
`filesystem_openfile_win.cpp:337`, `filesystem_timestamp_win.cpp:112`,
`modification_setupfilesystem.cpp:351`, `persistence_registry_win.cpp:444,870,874`),
`CtorFromWide`/`Dtor`/`StrLength` = 0 by name but 2/2/1 by dx11 address (`g_im.WideString*`,
`dx11/src/devicedata.cpp:1711,1721,1753,1755,1769` — genuinely called, just invisible to a
name-only sweep). `CtorFromChars` and `SetLength` (by name-door) are the true zero-callers.
**`UI::WideString` is confirmed unrelated** — zero hits for `@UI@@`-mangled `WideString`
anywhere; every hit found is `@System@@`.

---

## 6. Zero-CALL pointers

| pointer/export | class | status | other references |
|---|---|---|---|
| `fn_MemZeroFill` | MemoryAllocator | ZERO-BY-NAME — still resolved unconditionally (`engine_funcs.cpp:3647`) | none, not even a guard |
| `?Free@MemoryAllocator@System@@QEAAPEAXQEBX@Z` (0x1800c6c00, member Free) | MemoryAllocator | ZERO-BY-NAME-ONLY — registered "so the pair cannot drift", no live C++ reader today; **not checked by address this pass** | UNKNOWN whether a by-address route exists — flag before deleting |
| `?IsRunning@Thread@System@@QEBA_NXZ` (0x18000c5a0) | Thread | ZERO-BY-NAME-AND-ADDRESS — no `RegisterRuntimeExport` row, no resolver call anywhere | listed only in `rt_export_addrs.inc:289` |
| `CtorFromChars` (WideString, `PEBD` ctor) | WideString | ZERO-BY-NAME — no `RT_EXPORT_ONCE`/`ResolveExport` call site found by name; dx11's import table does not carry this slot either | none found |
| `SetLength` (WideString, by name-door) | WideString | ZERO-BY-NAME-ONLY for external callers — reached only via internal C++ calls from `Set`/both ctors | live internal caller |
| `DateAsString` (DateTime) | DateTime | ZERO-BY-NAME — 0 external callers; reached only via `AsString`'s internal forwarding | live internal caller |
| `TimeAsString` (DateTime) | DateTime | same as above | live internal caller |
| `HideErrorDisplay` (Error) | Error | ZERO-BY-NAME-AND-ADDRESS, proven dead in all 4 shipped binaries, deliberately unregistered | none |
| `StrAppend` (homed on Error, actually `System::String`) | Error/String | ZERO-BY-NAME-ONLY for external callers — internal to `SetText`/`Display` | live internal caller |
| `?Compute@CRC@System@@SAIPEBDI@Z` (0x1800c5430, char* overload) | Crc | ZERO-BY-NAME-AND-ADDRESS — never registered, never called; only in `rt_export_addrs.inc:3311` | none |
| `Timer::Shutdown` | Timer | ZERO-BY-NAME — only called internally by `Startup`'s own body (a plain C++ call, not through a door) | live internal call |
| `Timer::PlatformStartup`/`PlatformShutdown` | Timer | same — internal-only | live internal call |
| `Timer::GetTicks` | Timer | ZERO-BY-NAME | none found |
| `Timer::CurrentTime` | Timer | ZERO-BY-NAME | none found |
| `Timer::Assign` (compiler-gen copy-assign) | Timer | ZERO-BY-NAME — no-op body anyway | none |

**None of these were checked exhaustively by retail ADDRESS for an indirect table route** (the
IO agent's §6 caveat applies here too: a name-based lexer cannot see an indirect call through a
function-pointer table) — every row above is marked ZERO-BY-NAME at minimum; only
`IsRunning`, the char*-`Compute` overload, and `HideErrorDisplay` were additionally confirmed
absent from `rt_export_addrs.inc`'s registration path (ZERO-BY-NAME-AND-ADDRESS). The rest are
ZERO-BY-NAME-ONLY and need an address cross-check before any deletion.

---

## 7. Proposed batches for 4b

Unlike IO, this surface splits cleanly by CLASS OWNERSHIP rather than by caller directory,
because most `System` classes have small, disjoint, class-owned source files
(`runtime/src/system/*.cpp`) and their callers, while numerous, are almost all mechanical
`fn_X(args)` → `X.Method(args)` rewrites with no serialize-header-ordering hazard like IO's.

- **Batch S1 — the allocator core, ONE owner, serial, NOT this step.** 1,163+ CALL sites (524
  member + 616 static-free + 16 static-alloc + 17 third-surface, excluding the separate
  container-only `DoorAlloc`/`DoorFree` path), bigger than IO's entire caller surface. Must
  reconcile with `alloc_door.cpp`'s design and the redzone shim BEFORE any caller batch runs —
  a caller conversion that moves a call off `fn_MemAllocStatic` without keeping the redzone shim
  reachable regresses `BANISHED_REDZONE` silently. Recommend its own step (B.3.5 or later),
  matching `B34_RULES.md` §6.5(c)'s existing deferral for IO's identical allocator pointers.
- **Batch S2 — `Thread`/`CriticalSection`, ONE owner.** Small caller count (≈27 real ops across
  8 files) but the ONLY polymorphic class in scope and the only one with a live cross-module
  vtable-identity hazard (§2). Must NOT give `Thread` a real C++ constructor in 4b — NOT because
  of the DLL (coordinator correction, §2: that route is dead) but because objects deriving from
  it still wear minted tables (`B34_RULES.md` §4.3). In scope for 4b: declare the class, move the
  non-ctor bodies onto members with the doors delegating, convert the ~27 caller ops from
  per-file `s_fn_*` caches to member calls (they are NOT "already fine": a per-file
  `ResolveExport` cache is exactly the resolved-pointer indirection B removes), and decide
  `IsRunning`'s fate (port it, or delete the dead export). No `RtFactOp` row exists (§2, resolved).
- **Batch S3 — Random/Color/Time/DateTime, disjoint from S1/S2, ~350 CALL sites**
  (`code/Game/Component` and `Game` dominate, per §5.2/§5.5). All mechanical rewrites once each
  class exists; `Random`'s "member subobject of a menu struct" construction pattern (§3) is the
  one judgement call — do not give it a standalone ctor when retail places it in-line.
- **Batch S4 — Configuration/Compression/Timer/Crc/Error/WideString, disjoint, ~60-90 CALL
  sites total but WIDE (6+ independent doors for Error alone, §5.5).** Recommend ONE owner
  because so many of these are single-digit-count classes whose caller work is mostly "confirm
  the door, don't touch the 25 Error call sites without also finding all six door wrappers."
  The three duplicate CRC-32 table implementations (§5.5) are a design question for this
  batch's owner, not a caller-conversion task — collapsing them is out of scope for a caller
  sweep and risks the same kind of behavioural drift `B34_RULES.md` §6.5(b) warned about for
  IO's aliased slots.
- **Deferred past 4b, and why**: the allocator (S1, above — interacts with the redzone door
  design exactly as `B34_RULES.md` §6.5(c) already deferred IO's identical pointers);
  `System::Thread`'s ctor/dtor conversion (blocked on DLL removal, §2); the three-way CRC table
  consolidation (a design decision, not a batch).

---

## 8. Surprises

1. **`System::Thread` is the ONLY polymorphic class in the entire `System`-rest scope.**
   Every other class — including `CriticalSection`, which some earlier framing implicitly
   grouped with Thread as "the threading classes" — is a plain POD or fully-static class. A
   batch owner assuming CriticalSection needs the same vtable care as Thread would be solving a
   problem that doesn't exist for it.
2. **Three separate System classes are entirely invisible to `engine_funcs.h`** (not just
   Thread/CriticalSection as B34_RULES.md already flagged): `Compression` (zero `fn_Compression_*`
   pointers anywhere), `Crc` (its one real caller is a file-static cache in
   `persistence_saveinfo_job.cpp`), and `Configuration`'s SINGLETON specifically (the singleton
   slot is cached independently in three files, even though Configuration's four static
   String-returning members ARE visible via `engine_funcs.h`). Any batch scoped by reading
   `engine_funcs.h` alone would size these three at zero and be wrong.
3. **`Error::SetText` has SIX independent call doors**, the widest fan-out of any pointer
   measured in this pass — three per-file `ErrorSetText` wrapper functions, one allocator-owned
   wrapper, one `RT_EXPORT_ONCE` direct call, and one dx11 by-address import-table slot. A batch
   that "converts `Error::SetText`" by finding only the `engine_funcs.h`-style callers (there are
   none) would report false completion.
4. **Three independent CRC-32 table implementations coexist** and none of the other two ever
   calls the one class actually named `System::CRC`. This is very likely intentional (each
   caller may need a different poly/seed), but it means "CRC's caller surface" as a single
   number is meaningless without saying which table.
5. **`fn_MemFree`'s registered retail body (`0x1800c6c00`) has no live C++ reader** — the
   `engine_funcs.h` `fn_MemFree` pointer itself has silently become a thunk onto
   `fn_MemFreeStatic` rather than the member `Free`. This is a pre-existing fact, not something
   this pass changed, but it means "the member form" is really two different retail bodies
   depending which of `fn_MemAlloc`/`fn_MemFree` you're looking at (Alloc reaches
   `0x1800c6e60`; Free reaches `0x1800c7ad0`, the STATIC free, not `0x1800c6c00`). A closing
   pass should decide whether `0x1800c6c00`'s registration should be deleted as zero-caller — it
   is a live registration for a dead route.
6. **A prior inventory (`IO_CALLER_INVENTORY.md:399`) misattributed `citizenai_leisure.cpp`** to
   the `s_fn_MemAllocSA` per-file-cache pattern; direct read shows it uses the ordinary shared
   `fn_MemAlloc`. Corrected here (§4.2); the file should not be counted as a "hidden allocator
   surface" site in any future batch sizing.
7. **`WideString::CtorFromString` has 7 live callers**, not the "dx11 + 3 lane-FS closures"
   its own file comment claims — the comment describes an earlier wave's state and is stale.
   Anyone reading that comment to scope a WideString batch would undercount by more than half.
8. **Four of `WideString`'s eight exports (`CtorFromWide`, `Dtor`, `StrLength`, and part of
   `AsUTF8`) are reached ONLY through dx11's own by-address import table** (`g_im.WideString*`),
   never by a mangled-name resolve in `code/`, `engine/`, or `standalone/`. A name-only sweep of
   these four would report them zero-caller, which is exactly the "most dangerous wrong answer"
   `B34_RULES.md` §6.5(d) warned about for Thread — the same hazard exists here, on a different
   axis (by-address dx11 import table rather than a `s_fn_*` cache).
9. **The B34_RULES.md brief's claim that `tornado.cpp` uses `CriticalSection`/`Thread` is
   unsupported.** A direct grep of that file for both mangled substrings returns zero hits. This
   inventory drops it from the caller roster; a batch owner should not budget time to touch it
   for this reason.
10. **`Timer.h` lives in `runtime/src/system/`, not `runtime/types/system/`** — the one class in
    this scope with a header that isn't where the project's usual convention (`.claude/rules/type-headers.md`)
    would suggest to look for it. `WideString`'s header is similarly src/-local, deliberately, so
    this may be an intentional pattern for classes with no ABI-pinned layout rather than an
    inconsistency — worth a one-line confirmation from whoever owns `.claude/rules/type-headers.md`
    before a batch owner "fixes" it by moving the file.
