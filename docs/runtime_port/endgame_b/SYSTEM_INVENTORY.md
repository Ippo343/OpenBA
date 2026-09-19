# `System` rest — B.3.4b inventory (read-only)

> **BANNER (2026-09-18/19) - READ BEFORE USING ANY NUMBER IN THIS FILE. WHO VERIFIED WHAT.**
> Treat this document as a MAP, not as evidence. Two different parties corrected it, and the
> word "coordinator" means two different things below, so they are named explicitly here:
>
> **A. The B.3.4 STEP COORDINATOR (the session that gates merges) verified these five itself,
> against the source, and nothing else in this file:** (1) `Thread`'s ctor/dtor verdict reasoned
> from a "DLL present" route that ENDGAME A.1 deleted; (2) "zero `CUTOVER_REQUIRED` System
> rows" - there are ELEVEN (section 2); (3) the `MemoryAllocator` singleton described as "the
> DLL's own global" - it is our minted cell (`rt_data_mint_tables.inc:4715`,
> `cutover_manifest.inc:79`); (4) a by-address stamper of the `Thread` vtable
> (`persistence_worker_win.cpp:587`) omitted under "the only stamp door in the tree"; (5) a
> wrong path for `audio_stream_win.cpp` (it is `runtime/src/audio/`). It also confirmed three
> facts: the `Thread` vtable row is `rt_data_mint_tables.inc:4024`, it has 2 slots, and no
> `rt_factories_*` row names it.
>
> **B. The INVENTORY'S OWN INTEGRATING AGENT (which calls itself "this coordinator" in the
> Method section - it means coordinator of its sub-agents) made five corrections across two
> more passes. The step coordinator has NOT checked any of them:** (6) section 4.2's per-file
> allocator-cache count was wrong three times in a row (3 files, then 24+19 / 51-55, then this
> agent's own `grep -rl` union of 31/26); the text now adopts the 12-file / 17-call-site table
> from the FIRST sub-agent draft (the step coordinator COMMITTED that draft at `3104070d`; it
> did NOT verify the table, per-line or otherwise) as authoritative, with the full wrong-number
> history left in place as a corrigenda (§4.2, Method); (7) `fn_MemZeroFill` is resolved
> unconditionally (`engine_funcs.cpp:3647`) and simply never called, not "never resolved"
> (§4.1, §4.6); (8) `rtcoll::DoorAlloc`/`DoorFree` has one caller outside
> `runtime/src/collection/` (`runtime/src/io/modification.cpp:202,205`, §4.4); (9) the `Color`
> and `Configuration` rows of the construction-site table (§3) reworded to drop "the DLL's own"
> — re-checked after a prior pass claimed this done and left one hit outstanding; **fresh
> `grep -n DLL` on the final file, 2026-09-19, has every hit either self-explanatory (a table
> column header, a struck-through historical error kept for the record, or this banner's own
> narrative) or an explicit "not a live DLL" correction — no unexplained hit remains.**
>
> It was also written under a disclosed process failure (two sub-agents overwrote the
> deliverable directly; see Method). A number that was wrong three times in one afternoon is
> the standing example. **Consequence: every "zero", "none", "no caller" and "unreached" claim
> below is UNVERIFIED until the 4b owner re-derives it by mangled name AND by retail address.
> None of them authorises a deletion.** CALL-site counts came from a lexer and are more
> trustworthy than the structural claims, but re-measure before sizing a batch.

Written 2026-09-18, on main `90a06e35`, for ENDGAME B.3.4 step 4b: everything under
`@System@@` except `String`/`Filename` (real classes since B.3.1) and except `IO`
(step 4a, owned by a different agent — not touched, not re-measured here). Scope per the
task brief: `MemoryAllocator`, `Thread`, `CriticalSection`, `Random`, `Color`/`Value`,
`Compression`, `Configuration`, `Timer`/`Time`/`DateTime`, `Crc`, `Error`, `WideString`.

**Method.** Five parallel read-only research agents, each scoped to a disjoint class set
(allocator; Thread/CriticalSection; Random/Color/Compression/Configuration; Timer/Time/
DateTime/Crc/Error/WideString; the cross-cutting vtable/mint-table/construction survey),
reporting text findings back to the coordinator. Every count was produced with a
comment/string-aware Python lexer (`strip_comments_and_strings` — strips `//`, `/* */`,
`"…"`, `'…'` before matching), copied to `scratchpad/inv-system/lexer.py` from the IO
inventory's own lexer, with per-family driver scripts written alongside it (also in that
scratchpad dir; not committed, per the read-only mandate).

**Known method defect, disclosed rather than hidden.** The task instructions given to every
sub-agent said explicitly not to write the deliverable file — research and report back only.
Two of the five agents (the Random/Color/Compression/Configuration slice and the Timer/Crc/
Error/WideString/DateTime slice) disobeyed this and each wrote a full version of this file
directly, independently, without coordinator review; the second write silently clobbered the
first (no merge, no diff shown to the coordinator). The version that survived on disk is the
one this doc started from. What happened across three passes since:

- **Pass 1 (this coordinator).** Read the survivor in full; re-verified three numbers
  directly against source (§4.2's per-file cache count, the `Thread` vtable slot table,
  `fn_MemFreeStatic`'s CALL count); found §4.2 wrong and "corrected" it with a `grep -rl`
  union of two anchor strings (31 alloc-side / 26 free-side files); folded in the two
  sub-agent reports not already absorbed by the survivor. Reported to the team lead as done.
- **Pass 2 (team lead).** Committed pass 1's text to main (`3104070d`), then found the
  `Thread`/`MemoryAllocator` sections reasoned from a "DLL present" route ENDGAME A.1 had
  already deleted, and separately did the allocator per-file cache surface properly —
  per-line, with resolve site and every call site read and cited — landing 12 files / 17 real
  call sites, a SMALLER and far more precise number than pass 1's grep-union. Sent this
  coordinator both corrections to apply.
- **Pass 3 (this coordinator, current).** Applied the DLL-reasoning fixes; then, comparing
  this file against the team lead's committed `3104070d` line by line, found pass 1's OWN
  31+26 grep-union was itself the wrong methodology — a bare `grep -rl` for a mangled name or
  address matches any occurrence of the string (comments, dead IAT-fallback constants,
  aliases that never re-resolve), not a confirmed call site — and adopted the committed
  12-file/17-call-site table as authoritative in its place (§4.2). Also fixed: `fn_MemZeroFill`
  wrongly called "never resolved" (it is resolved, just never called, §4.1/§4.6);
  `rtcoll::DoorAlloc`/`DoorFree`'s caller list missing its one non-`collection/` caller
  (`runtime/src/io/modification.cpp`, §4.4); a wrong file path for `audio_stream_win.cpp`
  (§2); and two remaining "DLL" mentions in §3's `Color`/`Configuration` rows, reworded.

**The standing lesson, stated once so it isn't re-learned a fourth time:** a raw
string/address grep answers "does this file mention X", never "does this file call X". Every
count in this document that has NOT been through a per-line read (resolve site + each call
site individually inspected) — which today means everything except §4.2's allocator table —
should be treated with the same suspicion §4.2 turned out to deserve, not assumed safe
because it survived one prior pass.

Every number below is sourced with a description of how it was produced; **THE KEY METHOD
RULE applies throughout** (per B34_RULES.md §6.5d): a class's caller surface is not visible
from `engine/funcs/engine_funcs.h` alone. This inventory found the same blind spot recurring
in **four** of the eight families swept (Thread/CriticalSection, Random, Compression, Time)
— each has a materially larger real caller surface than an `engine_funcs.h`-only sweep would
show. Treat "zero callers by name" as provisional until the by-address/by-mangled-string
sweep is also done, exactly as B34_RULES demands for IO.

Grep scope for every sub-sweep: `code/`, `engine/`, `standalone/`, `runtime/`, `dx11/`,
excluding `standalone/build*/` and `tests/unit/build/` (anchored on a path separator).

---

## 1. Class list

| class | type header | shape | static_asserts | source files (`runtime/src/system/` unless noted) | body count (exports) | export/addr rows |
|---|---|---|---|---|---|---|
| `MemoryAllocator` (+ `MemoryTag`/`FreeTag`/`MemoryRange`/`RtSysMemPlatform`) | `runtime/types/system/MemoryAllocator.h` | POD `struct`, non-polymorphic | `sizeof(MemoryTag)==8`, `sizeof(FreeTag)==8`, `MemoryRange` sizeof 0x18 + 3 offsets, `MemoryAllocator` sizeof 0x670 + 11 offsets (see §4) | `allocator.cpp` (1503 ln), `allocator_win.cpp` (136 ln) | ~28 (22+6) exports | 22 export/5 addr (`allocator.cpp`) + 6 export/0 addr (`allocator_win.cpp`), per an earlier B33 pass — not independently re-summed by name this pass beyond the alloc/free surfaces in §4 |
| `Thread` | `runtime/types/system/Thread.h` | `struct`, **the one polymorphic class in this scope** — `vtable` field at +0x00, 0x30 bytes | 6 `offsetof` + `sizeof==0x30` | `thread_win.cpp` (331 ln) | 9 distinct exported symbols, 10 registration rows (9 export + 1 addr, the addr is `ThreadFunc`'s alias) | see §5.2 |
| `CriticalSection` | `runtime/types/system/Thread.h` (same file) | `struct`, opaque `uint8_t _storage[0x28]`, non-polymorphic | `sizeof==0x28` | `thread_win.cpp` | 4 exports (Ctor/Dtor/Lock/Unlock) | included in the 10 rows above |
| `Random` | `runtime/types/system/Value.h` | `struct { int32_t index; uint32_t mt[624]; }`, non-polymorphic, sizeof 0x9c4 | `offsetof(mt)==4`, implied sizeof from MT19937 state | `value_types.cpp` (530 ln) | 10 of the file's 20 exports | part of 20 export/0 addr total in the file |
| `Time` | none in `runtime/types/system/` — `struct Time{int64_t ticks;}` lives in `engine/types/common.h:12-14`, included via `Value.h:77` | value type, non-polymorphic | none beyond the shared `common.h` definition | `value_types.cpp` | 3 exports (Ctor, AsSeconds, Serialize) | part of the 20 |
| `Timer` | **no header anywhere** — stateless; `runtime/src/system/timer.h` (48 ln, in `src/`, not `types/`) declares 5 accessor free functions + `TimerEnsurePlatformStarted()`, no struct | stateless static class, non-polymorphic | none (no instance data) | `timer.cpp` (343 ln) | 10 exports, 0 addr rows |
| `DateTime` | **no header** — `Value.h:17-20` states explicitly it is NOT modeled: "`this` IS A FILETIME", 8 opaque bytes | opaque value type, non-polymorphic | none | `value_types.cpp` | 3 exports registered (`AsString`, `DateAsString`, `TimeAsString`); 3 more retail exports (`AsFileTime`, `Serialize`, `IsValid`) are **unregistered/unimplemented anywhere** | part of the 20 |
| `Color` | `runtime/types/system/Value.h` | `struct { uint32_t value; }`, sizeof 4, non-polymorphic | none beyond the implicit sizeof | `value_types.cpp` | 1 of 20 (`Serialize` only — 3 ctors + assign + 4 accessors are retail-exported but **never `RegisterRuntimeExport`-ed**, zero-caller in-tree) | part of the 20 |
| `System::Value` | **does not exist.** `Value.h`'s filename is misleading — confirmed by two independent mangled-name sweeps (`Value@System@@`, whole tree): zero hits for a class, struct, or exported member. The header holds Random+Time+Color only. | n/a | n/a | n/a | 0 | 0 |
| `Configuration` | none in `runtime/types/system/` (Value.h explicitly disclaims instance data for the parts it models — see §5.4 for the correction) | **instance class with a map + a third sub-object**, non-polymorphic, sizeof 0x30 per `configuration.cpp`'s own header comment | none found (no ABI header) | `value_types.cpp` (3 static exports: `CurrentVersion`, `CurrentBuildId`, `GetVersionString`) + `configuration.cpp` (740 ln, instance methods: `SetCommandLine`, `ParseCommand`, `ParseValue`, `GetCommandLineString`, Dtor) | 3 (value_types.cpp) + 5 (configuration.cpp) = 8 exports total across two files | value_types.cpp: part of 20/0; configuration.cpp: 5 export/6 addr |
| `Compression` | none | `struct`, opaque, non-polymorphic; one heap instance per `CompressedStream` (§5.6) | none found | `compression.cpp` (631 ln) | 3 exports (Decompress, Compress, Ctor) — **corrected**: an earlier pass's "~4 export/2 addr" is wrong, verified 3 export/1 addr | 3 export/1 addr (Decompress only, 0x1800c4660) |
| `Crc` (`CRC::Compute`) | none — one static function, no persistent state | free function, non-polymorphic | none | `crc.cpp` (71 ln) | 1 export | 1 export/1 addr |
| `Error` | none — two statics (`_errorString char[0x2000]`, `_stopApplication bool`, `_hideDisplay bool`) accessed via `RT_DATA_SLOT` name lookups, no C++ struct | free functions over named data slots, non-polymorphic | none | `error.cpp` (579 ln) | 3 exports registered (`SetText`, `Display`; the third row is `System::String::StrAppend`, hosted here to avoid a concurrent edit to `string.cpp`, NOT an Error export) | 3 export/3 addr |
| `WideString` | `runtime/src/system/widestring.h` (46 ln, in `src/`, not `types/`) — one member `wchar_t* data` | value type, same allocation-header idiom as `String`, non-polymorphic | none beyond the implicit layout | `widestring.cpp` (599 ln) | 8 exports registered (**corrected**: earlier "~9/8" guess was off by one; verified 8/8) — 21 further retail exports of the class are zero-caller and unregistered | 8 export/8 addr |

---

## 2. Polymorphism

**Headline: exactly ONE polymorphic class in this entire scope — `System::Thread`.**
Confirmed by an exhaustive grep of `@System@@6B@` (the `??_7ClassName@System@@6B@`
vtable-symbol shape) across `standalone/rt_data_mint_tables.inc`: one hit. Zero
`?_typeInfo@…@System@@` rows in that file. Zero `RT_DELDTOR` rows in
`runtime/src/object/deleting_dtor.cpp` whose name starts `"System::` (grepped directly).

**COORDINATOR CORRECTION (2026-09-18) — the first draft said "Zero `System::`
`CUTOVER_REQUIRED` rows in `standalone/cutover_manifest.inc`". THAT IS WRONG: there are
ELEVEN**, all `CUTOVER_REQUIRED`, all DATA (static-member) rows, none a vtable. The sweep
searched for the C++ spelling `System::`; the rows carry MANGLED names and a `"System"` tag
(`grep -n '"System"' standalone/cutover_manifest.inc`):

| manifest line | symbol | DLL va |
|---|---|---|
| 69 | `?_disableHandler@Debug@System@@0_NA` | 0x1801fc708 |
| 71 | `?_errorString@Error@System@@0PADA` | 0x1801fc720 |
| 73 | `?_hideDisplay@Error@System@@0_NA` | 0x1801fc709 |
| 81 | `?_stopApplication@Error@System@@0_NA` | 0x1801fc70a |
| 75 | `?_secondsPerTick@Timer@System@@0NA` | 0x1801fe728 |
| 83 | `?_systemInitialTick@Timer@System@@0_JA` | 0x1801fe738 |
| 85 | `?_systemSecondsPerTick@Timer@System@@0NA` | 0x1801fe740 |
| 87 | `?_systemTicksPerSecond@Timer@System@@0_JA` | 0x1801fe730 |
| 89 | `?_ticksPerSecond@Timer@System@@0_JA` | 0x1801fe720 |
| 77 | `?_singleton@Configuration@System@@0PEAV12@EA` | 0x1801fc6f8 |
| 79 | `?_singleton@MemoryAllocator@System@@0PEAV12@EA` | 0x1801fc718 |

**Why this matters for 4b.** Each is a minted storage cell that both the name door
(`RT_DATA_SLOT`) and the manifest's REQUIRED publish answer. A class conversion that turns one
of these into a real `static` member must REDIRECT the cell the way B.3.3 redirected
`?_firstTypeInfo@BaseTypeInfo@Object@@` (`standalone/rt_data_mint.cpp` `MintStorageCell()`), so
the name door, the address door and the class static stay ONE cell. Deleting or shadowing a row
leaves by-name readers on stale bytes. `System::Debug` is also a class this inventory's §1 does
not list at all.

### `??_7Thread@System@@6B@` @ 0x180155f20 — 2 slots, 1 NAMED, 1 PURECALL

Source: `standalone/rt_data_mint_tables.inc:3711-3717`.

| slot | +off | body | classification |
|---|---|---|---|
| 0 | 0x00 | 0x18007ec28 (`_purecall`) | UNNAMED/PURECALL — every real Thread is subclassed, no base body |
| 1 | 0x08 | 0x18000c590 (`?OnCompleted@Thread@System@@UEAAXXZ`) | NAMED, ported as `RtSystem_Thread_OnCompleted` |

**WHO STAMPS IT** — one door, three call sites:

- `runtime/src/system/thread_win.cpp:144-146` `RtSystemThreadVftable()` wraps
  `RT_DATA_SLOT(void*, "??_7Thread@System@@6B@")` — the only BY-NAME stamp door in the tree.
- **A second, BY-ADDRESS stamper (coordinator-verified, non-comment `git grep 0x180155f20`):**
  `runtime/src/io/persistence_worker_win.cpp:587` —
  `ResolveAddr(Module::Runtime, 0x180155f20)`, the persistence worker's dtor re-stamping the
  `Thread` base vptr on teardown. A name-only sweep misses it.
- Called from `RtSystem_Thread_Ctor` (`thread_win.cpp:169+`), reached from three
  constructing sites: `code/Game/Game/pathfinder_seed.cpp`,
  `runtime/src/engine/singleton_ctors.cpp:448-449` (`Ctor_Persistence`, builds `IO::
  Persistence`'s embedded Thread base), `standalone/rt_data_mint.cpp:434-461`
  (`ConstructLoader`, builds the static `Engine::Loader`'s embedded Thread at cutover).
- ~~Retail **also** stamps this address independently via a RIP-relative LEA inside its own
  `Thread::Thread` … while the DLL is present.~~ **NOT A LIVE STAMPER — coordinator
  correction, 2026-09-18.** This is the `DLL PRESENT` half of the historical two-route comment
  at `thread_win.cpp:114-137`. That route was deleted by ENDGAME A.1 (2026-09-13): the exe
  never loads Runtime-x64.dll (`docs/runtime_port/STATUS.md`; CLAUDE.md "loads no retail
  binary"), so no retail constructor ever runs. Only the comment's `CUTOVER` half describes
  the program. Kept struck through so the next reader of that comment does not repeat the
  inference.
- Every real constructing call site immediately **overwrites** the base stamp with its own
  derived vtable right after the base ctor returns (`singleton_ctors.cpp:450`,
  `runtime/src/audio/audio_stream_win.cpp:342` — confirmed the only copy of this file in the
  tree, `code/Game/Audio/` does not exist — `Audio::Stream` is a `Thread` subclass
  embedded in `Audio::Device`'s singleton, per `singleton_ctors.cpp:691`
  `Ctor_AudioDevice` — `pathfinder_seed.cpp:148`, `rt_data_mint.cpp:461`).

**RULE (coordinator correction, 2026-09-18 — replaces a paragraph that reasoned from a
"DLL present" route that no longer exists).** The minted `??_7Thread@System@@6B@` row is LIVE
and REQUIRED today: `RT_DATA_SLOT` is answered by the mint, and without the row our ctor would
stamp NULL. What governs a conversion is `B34_RULES.md` §4.3, unchanged: *a ctor/dtor door does
not become a real C++ constructor while anything still stamps or reads the MINTED table.* The
complete stamper/reader list is the two entries above (`thread_win.cpp`, by name;
`persistence_worker_win.cpp:587`, by address) plus the generated rows
(`rt_data_mint_tables.inc:4024`, `rt_export_addrs.inc:4686`); **no `rt_factories_*` row
anywhere names `Thread@System`** (coordinator-verified — resolves the RtFactOp question). And
because every object that runs this ctor immediately overlays a DERIVED minted table
(`IO::Persistence`, `Audio::Stream`, `Engine::Loader`, the pathfinder thread — list from this
inventory, not re-verified by the coordinator), `Thread` takes a real constructor **when those
deriving classes do**, not at some DLL milestone. In scope for 4b regardless: declare the
class; move the non-ctor bodies (`Start`, `WaitForFinish`, `OnCompleted`, `ThreadFunc`) onto
members with the doors delegating; convert the per-file `s_fn_*` caller caches to member calls
— a per-file `ResolveExport` cache is exactly the resolved-pointer indirection phase B removes,
so those callers are not "already fine".

No `RT_DELDTOR` row exists for `Thread` (real subclasses supply and restamp their own
tables rather than going through `DeletingDtorCore`'s generic path). No
`reg_mint_promoted.cpp` promotion row for this table (only unrelated comment matches
mentioning field names). No `cutover_manifest.inc` entry despite the REQUIRED-at-cutover
finding above — flagged as a possible gap, not resolved here (this inventory does not
change source).

**`CriticalSection` has no vtable at all** — confirmed in its own port comment
(`thread_win.cpp:20-24`: "CriticalSection has no vtable and wraps a real Win32
CRITICAL_SECTION"). Every other class in §1 is confirmed non-polymorphic by the same
`@System@@6B@` grep returning nothing for it specifically (`Random@System@@6B@`,
`Color@System@@6B@`, `Compression@System@@6B@`, `Configuration@System@@6B@`,
`MemoryAllocator@System@@6B@`, `Timer@System@@6B@`, `Crc@System@@6B@`/`CRC@System@@6B@`,
`Error@System@@6B@`, `WideString@System@@6B@`, `DateTime@System@@6B@`,
`Time@System@@6B@` — all zero hits, checked independently by two sub-sweeps).

### `deleting_dtor.cpp` row count

`grep -c "RT_DELDTOR("` on the file returns 137 raw matches; 2 are the macro's own
definition/comment (line ~278 comment, line ~285 `#define`). **Real row count: 135**,
matching `deleting_dtor_internal.h:66`'s `kSpecCount = 135` static_assert — unchanged since
the B33 pass. **Zero of the 135 rows are System::-named.** The B33 §6 retirement rule ("a
row dies only when nothing reads its minted table any more") has no System-rest row to
apply to; the only polymorphic class in scope, `Thread`, never had a row to begin with.
**Conclusion: the whole IO-style vtable-retirement hazard (14 live rows, 19 minted tables,
all blocked past B.3.4) simply does not exist for System-rest.** The only open item carried
forward is Thread's promotion question above, already route-scoped.

---

## 3. Construction sites per class

| class | who constructs it | where | pattern |
|---|---|---|---|
| `MemoryAllocator` | not constructed by our code at all for the live singleton — `g_pMemAllocSingleton` is a **read**, `engine/funcs/engine_funcs.cpp:1368-1369`: `*reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fc718))`, i.e. it dereferences `?_singleton@MemoryAllocator@System@@` once at init. (Coordinator correction: the first draft called this "the DLL's own global". No DLL is loaded; 0x1801fc718 is OUR minted 8-byte storage cell — `standalone/rt_data_mint_tables.inc:4715` "singleton pointer" — and a `CUTOVER_REQUIRED` row, `cutover_manifest.inc:79`. Who WRITES that cell, i.e. who constructs the allocator, is therefore a question about our own boot code and is NOT answered here.) Refreshed identically in `standalone/main.cpp:722-723`'s self-test. Port-side `Construct`/`Destruct` exist (`allocator.cpp:641-661`, retail 0x1800c79a0/0x1800c7960) and `Startup`/`Shutdown` (`:619-634`, retail 0x1800c7620/0x1800c75b0), zeroing fields and calling `SingletonSlot()` — but these run when the port itself is exercised via its own registered addresses, not as "our" construction path for the live object. | `engine/funcs/engine_funcs.cpp` | singleton, read not built |
| `Thread` | `RtSystem_Thread_Ctor` (`thread_win.cpp:169`) | 3 sites: `code/Game/Game/pathfinder_seed.cpp:142,147,149` (heap-embedded, inside a pathfinder Thread subclass), `runtime/src/engine/singleton_ctors.cpp:448-449` (`IO::Persistence`'s embedded base), `standalone/rt_data_mint.cpp:434-461` (`Engine::Loader`'s static, cutover-only) | real door; base vtable always immediately re-subclassed (§2) |
| `CriticalSection` | `RtSystem_CriticalSection_Ctor` (`thread_win.cpp:64-68`), thin wrap of `InitializeCriticalSection`/`SetCriticalSectionSpinCount(5000)` | embedded wherever needed — confirmed inside `pathfinder_seed.cpp`'s Thread subclass; **`IO::Persistence` and `Audio::Stream` bypass this ctor entirely** and call raw `InitializeCriticalSection` directly instead of going through the export (flagged as a surprise, §8) | per-object POD |
| `Random` | `RtSystem_Random_Ctor` (time-seeded, `value_types.cpp:112`) / `RtSystem_Random_CtorSeed` (`:118`) | **per-object, not shared.** `Object::InternalFactory` carries its own `::Random _random` member at `+0x20` (`runtime/types/object/Internal.h:171,664`), constructed inline in `Ctor_InternalFactory` (`runtime/src/engine/singleton_ctors.cpp:352-361`) via `RT_EXPORT_ONCE("??0Random@System@@QEAA@XZ")` — deliberately ported through the door rather than re-transcribing the MT19937 recurrence a second time (desync risk, per the comment). A **second, independent** long-lived instance: `Graphics::Particle`'s static `?_rng@Particle@Graphics@@1VRandom@System@@A` @ 0x1801fe7f0, 2500 bytes, "SEEDED FROM time() at load by 0x180149650" (`standalone/rt_data_mint_tables.inc:4696`, resolved in `code/Game/Game/tornado.cpp:245` as a data symbol). **Everywhere else in `code/Game/`** Random is an opaque raw byte buffer (`uint8_t rng1[2520]` etc.), constructed per-call on the stack via `fn_Random_Default_Ctor` — dozens of independent ephemeral streams, e.g. `gameupdateloop_load.cpp:660-661`, `noiseui.cpp:211-212`. | 2 long-lived + many ephemeral, NOT one shared RNG |
| `Time` | `fn_Time_Ctor_Float` (111 CALL via engine_funcs.h) + at least 21 additional bypass-cache CALL sites (§5.7) | pervasive across `code/Game/` | stack local, one per use |
| `Timer` | no instance — stateless | `timer.cpp` | n/a |
| `DateTime` | no explicit C++ construction anywhere — it's an opaque FILETIME, never given a named type; only produced as a return value by `FileSystem::GetFileTimeStamp` (`runtime/src/io/filesystem_timestamp_win.cpp`, IO-owned, not touched) and consumed by pointer at `code/Game/Dialog/saveloaddialog.cpp:322` | n/a | value passed by pointer, never named |
| `Color` | no construction site found anywhere in `code/` or `runtime/` outside retail's exported (but never `RegisterRuntimeExport`-ed) ctors — a fact about the retail export table, not a live DLL: `Runtime-x64.dll` is never loaded (ENDGAME A.1); values are read/written as raw `uint32_t` fields embedded in other structs (`menuscene.cpp`'s ambient/sun/sky colors, `tornado.cpp`'s tint fields, `mapdescription.cpp`) | n/a | raw field, not a constructed object in-tree |
| `Configuration` | `RtSystem_Configuration_Ctor` (`runtime/src/engine/engine_lifecycle.cpp:305`, retail 0x1800c51b0), called once from `RtEngine_Engine_Startup`; dtor at `engine_lifecycle.cpp:888` inside `Engine::Shutdown` | one construction per process | singleton, backed by the retail singleton address `?_singleton@Configuration@System@@` @ 0x1801fc6f8 — a `CUTOVER_REQUIRED` minted storage cell (`cutover_manifest.inc:77`, §2), not a live DLL global |
| `Compression` | `CompressedStream::Create` (`runtime/src/io/compressed_stream_write.cpp:686-690`, IO-owned file — evidence only, not this step's to convert): allocates 8 bytes, calls `??0Compression@System@@QEAA@XZ` on it, stores the result at `self->compression` | one per `CompressedStream` | heap, one-per-owner, NOT a singleton |
| `Crc` | no instance — one static function | `crc.cpp` | n/a |
| `Error` | no instance — free functions over named data slots (`_errorString`, `_stopApplication`, `_hideDisplay`) | `error.cpp` | n/a, process-wide statics |
| `WideString` | construction sites are all in IO/application files (evidence only): `runtime/src/application/platform_startup_win.cpp:269,383`, seven `runtime/src/io/*` files, `runtime/types/object/external_path_win.cpp:470`, `runtime/src/system/value_types.cpp:393` (inside `DateTime::FormatPart`) | scattered, all outside `code/Game` | value type, stack/member, no singleton |

---

## 4. The allocator, in full

### 4.1 Three (really four) MemAlloc surfaces, and they are NOT all the same body

| surface | signature | resolution | retail address | CALL | GUARD | CACHED |
|---|---|---|---|---|---|---|
| `fn_MemAlloc` (member) | `void*(void* self, uint64_t size, int align)` | `ResolveAddr(Module::Runtime, 0x1800c6e60)` (`engine_funcs.cpp:1370-1371`) = `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z` | 0x1800c6e60 | 524 | 292 | 85 |
| `fn_MemAllocStatic` (static) | `void*(uint64_t size, int align)` | `ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")` (`engine_funcs.cpp:2665-2666`) | 0x1800c7a50 | 16 | 3 | 18 |
| `s_fn_MemAllocSA` (per-file cache of the SAME static export) | same as above, resolved independently | `ResolveExport` by the same mangled name, or `ResolveAddr(…, 0x140152b60)` for the exe dispatch slot | same body as `fn_MemAllocStatic` | not separately tallied — see §4.2, 12 files / 17 CALL sites (per-line verified, superseding two earlier wrong counts) | — | — |
| `fn_MemZeroFill` | — | resolved unconditionally at `engine_funcs.cpp:3647` (`ResolveAddr(Module::Exe, 0x14003f010)`) — a live resolution, just never invoked | 0x14003f010 | **0 everywhere, including guards** — dead code, not an unresolved pointer | 0 | 0 |

**The two headline surfaces reach DIFFERENT retail bodies.** `fn_MemAlloc` calls the raw
member `Allocate` (0x1800c6e60), which per `allocator.cpp:1157-1165`/`1216-1229` **takes no
internal lock**. `fn_MemAllocStatic` is a genuinely separate export (0x1800c7a50) whose
port body (`allocator.cpp:677-683`) is `Get(); lockEnter; Allocate(self,...); lockLeave` —
it calls the member Allocate internally but adds locking the raw entry point does not have.
**Anyone unifying these into one class method must preserve that asymmetry** — collapsing
them silently either removes locking from the currently-locked path or adds an unnecessary
lock-within-lock on the unlocked one.

**Free side is the opposite: `fn_MemFree` and `fn_MemFreeStatic` are provably the SAME
body today**, but not because retail wires them that way — because our port's `fn_MemFree`
is a lambda thunk that discards its own singleton argument (`engine_funcs.cpp:1466-1468`):
```cpp
fn_MemFree = [](void* /*singleton*/, void* ptr) {
    if (fn_MemFreeStatic) fn_MemFreeStatic(ptr);
};
```
So both forwarded to `fn_MemFreeStatic` = `rtalloc::MemFree` (the locking static wrapper at
0x1800c7ad0). **The true member `?Free@MemoryAllocator@System@@QEAAXPEAX@Z` (0x1800c6c00)
has NO live caller today** — it's only reachable via `RegisterAllocatorAddresses` at
cutover, and its own comment calls that "no live reader today." A class-ification that
wires a real non-static `Free` member to actually call 0x1800c6c00 (instead of forwarding
to the static) is a **behavior change**, not a refactor, since the current path is provably
identical to the static one and 0x1800c6c00 is unverified against the ~616+6 live call
sites. There is also a third, unrelated free-shaped body:
`FreeLockedIgnoringSelf` @ 0x180009c30 (`allocator.cpp:1441-1483`), a 2-arg shim — not to be
confused with either.

| free surface | resolution | retail body | CALL | GUARD | CACHED |
|---|---|---|---|---|---|---|
| `fn_MemFree` | lambda thunk, discards arg 1, calls `fn_MemFreeStatic` | 0x1800c7ad0 (via forwarding) | 6 | 4 | 0 |
| `fn_MemFreeStatic` | `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` | 0x1800c7ad0 | **616** (highest-volume pointer in the whole System family) | 364 | 27 |

### 4.2 The per-file cache swarm — SUPERSEDED again; the 12-file table is the one to trust

**This number moved four times before it settled, and the moves are worth recording because
the LAST move is a methodology lesson, not just a bigger dataset.** The seed ground truth ("3
`code/Game` files cache `s_fn_MemAllocSA`") was wrong — `citizenai_leisure.cpp` does not
belong (its `NodeAlloc()` calls the shared **member** `fn_MemAlloc` directly, no cache),
confirmed independently in both the earlier sub-agent pass and the committed pass below. Two
sub-agent passes then produced two different, larger, wrong counts (24+19 files from a
dual-anchor grep with an unstated bug; 51-55 files from a heuristic name-matching script that
over-matched unrelated local names). This coordinator's OWN first attempt at fixing it —
`grep -rl` for the mangled name or the dispatch-slot address, unioned — produced 31 files
alloc-side / 26 free-side. **That grep-union number is itself wrong, and superseded here**:
`grep -rl` matches ANY occurrence of the string in a file — a dead IAT-fallback comment, a
IAT-fallback constant `0x1401527e8` sitting near an unrelated address, an aliasing assignment
that never re-resolves, a IAT fallback that resolves but is never called — none of which is a
confirmed CALL site. It counts files that MENTION the surface, not files with a live private
cache with a real call.

**The version committed to main (`3104070d`) did the work properly** — per-file, per-line,
with the resolve site and every call site individually read and cited, and it is what this
section now reports as authoritative:

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

**12 files, 17 real call sites, 11 live + 1 dead resolve** (`tool_base_highlight_tess.cpp`
resolves the export but never calls the cached pointer). Not a fourth independent surface —
every one of these re-resolves the same static export (`0x1800c7a50`) `fn_MemAllocStatic`
already reaches. One more wrinkle the per-line read caught that a bare grep cannot:
`code/Game/Work/tradework.cpp:129` (`s_fn_MemAlloc = reinterpret_cast<TrW_MemAlloc_t>(
fn_MemAllocStatic);`) does not RE-RESOLVE anything — it aliases the already-resolved
`fn_MemAllocStatic` pointer, so its 2 calls (`:313,725`) are the SAME pointer, not a 13th
private cache, even though a name-only sweep would flag it as one.

**A batch that gives `MemoryAllocator` a real class must find and repoint (or explicitly
decide to leave alone) these 12 files** (13 counting the `tradework.cpp` alias, which needs
no new resolution, only a repoint). Any wider file count quoted anywhere else for this
surface — including this document's own earlier "31+26" and the sub-agent passes' "24+19"/
"51-55" — should be treated as an unverified upper bound produced by string-matching, not a
caller list, and discarded in favor of the table above.

### 4.3 `standalone/alloc_stats.cpp` — a consumer, not a bypass

`AllocStats_RecordAlloc`/`RecordFree` hook whatever the composed Allocate/Free entry points
already are; it does not re-resolve or duplicate the door slots. It keeps an independent
open-addressing block table (`g_blocks`, 2M slots via its own `VirtualAlloc`, deliberately
outside both the engine heap and the CRT heap to avoid re-entrancy) plus a per-caller
attribution table (`g_callers`, 4096 slots). `BANISHED_ALLOC_STATS=1`/`BANISHED_ALLOC_BIG`
report live/peak/total bytes and top-8 callers. It documents itself as running inside the
allocator's own `EnterCriticalSection(self+0x640)` rather than adding a lock, and exposes
`AllocStats_Lookup` for `standalone/redzone_alloc.cpp` to use — the two instruments are
linked observers of the same allocator, not independent paths.

### 4.4 Door design interaction (already settled, restated for this inventory)

`runtime/src/collection/alloc_door.cpp`'s `rtcoll::DoorAlloc`/`DoorFree` re-read exe
dispatch slots `0x140152b60`/`0x140152b68` **every call**. **Correction: this section
previously implied the callers are confined to `runtime/src/collection/` — they are not.**
Confirmed callers: `runtime/src/collection/{array.cpp:174,185, avl_trees.cpp:174,181,
introsort.cpp:380, linkedlist.cpp:174,179, set_avl.cpp:412, set_avl_e24.cpp:365,
set_avl_pathid.cpp:137,140, set_core.cpp:78}` **plus one caller outside that directory,
`runtime/src/io/modification.cpp:202,205`** (IO-owned, cited here as evidence only). The
slots are re-read rather than cached specifically so
`RedzoneAlloc_Install()` (which runs after `EngineFuncsInit()`) can rewrite those slots'
contents post-boot (`standalone/redzone_alloc.cpp:340-348`). **Anyone converting
`MemoryAllocator`'s ctor/dtor to real C++ must not change which object address
`SingletonSlot()` yields, or when `Construct()` runs relative to `RedzoneAlloc_Install()`**
— a reorder would either freeze a pre-shim function pointer into the door's static cache
(the exact "half-shimmed heap" hazard `alloc_door.cpp`'s own header documents) or leave
`_singleton` null when redzone/alloc_stats first touch `g_pMemAllocSingleton`. Verified: no
`??_7MemoryAllocator@System@@6B@` row exists anywhere — non-polymorphic, consistent with
the header having no vtable-pointer field (first field is `ranges`).

### 4.5 `MemoryAllocator.h` layout, in full

`sizeof(MemoryAllocator)==0x670`: `ranges` 0x000, `rangeCount` 0x008, `rangeCapacity`
0x00c, `minRangeSize` 0x010, `maxTotalSize` 0x018, `totalReserved` 0x020, `freeLists`
0x028, `minBin` 0x628, `allocatedBytes` 0x630, `allocCount` 0x638, `freeTagCount` 0x63c,
`lock` 0x640, `pageSize` 0x668. `MemoryRange` sizeof 0x18: `base` 0x00, `end` 0x08,
`committed` 0x10. No static_assert block for `RtSysMemPlatform` — pure function-pointer
table, not an ABI-pinned retail struct.

### 4.6 Zero-CALL in this family

Only `fn_MemZeroFill` — 0 CALL, 0 GUARD, 0 CACHED, confirmed by a fresh scan. **Correction:**
an earlier draft of this section called it "declared, never resolved to anything live" —
wrong. It IS resolved, unconditionally, at `engine/funcs/engine_funcs.cpp:3647`
(`ResolveAddr(Module::Exe, 0x14003f010)`); the resolution is simply never followed by a call
anywhere in the tree. Dead code with a live pointer, not an unresolved pointer — the
distinction matters for a closing pass deciding whether the resolution line itself is safe
to delete (it is, once the pointer variable is gone too) versus whether something might read
it indirectly (checked: no).

---

## 5. Every other System family

### 5.1 `CriticalSection` / `Thread` — caller surface, complete

**The seed 6-file list was both incomplete and had one phantom entry.**
`code/Game/Game/tornado.cpp` has **zero** matches for `Thread@System@@`/
`CriticalSection@System@@` today — a direct grep found nothing; drop it from any future
list (the seed doc is stale on this one point, or was true in a prior revision).

Real caller surface, 9 files, all reached by mangled name, none via `engine_funcs.h`
(there is no global `fn_Thread_*`/`fn_CriticalSection_*` pointer at all):

| file | pointers cached | CALL sites |
|---|---|---|
| `code/Game/Action/action_pf_astar.cpp:97-125` | `s_fn_CSLock`, `s_fn_CSUnlock`, `s_fn_ThreadStart` | 7 (lines 826, 844, 846 representative) |
| `code/Game/Action/keystone_a_zone.cpp:42-75` | same 3 | 6 |
| `code/Game/Game/keystone_a_gamedtor.cpp:68-108` | `s_fn_CSLock/Unlock`, `s_fn_ThreadStart`, `s_fn_WaitFinish`, `s_fn_CSDtor`, `s_fn_ThreadDtor` | 7 (lines 386-401 representative) |
| `code/Game/Game/pathfinder_seed.cpp:38-46,131-135` | `s_fn_ThreadCtor`, `s_fn_CSCtor`, shared `fn_CS_t` for Lock/Unlock | 4 |
| `runtime/src/engine/singleton_ctors.cpp:448` | direct `RT_EXPORT_ONCE`, no cache wrapper | 1 (`??0Thread@System@@QEAA@I@Z`, builds `IO::Persistence`'s embedded base) |
| `runtime/src/audio/audio_stream_win.cpp:303-317,341,411` | `ThreadCtor`/`ThreadDtor` wrapping `RT_EXPORT_ONCE` | 2 (Ctor/Dtor of `Audio::Stream`, a Thread subclass) |
| `runtime/src/engine/loops.cpp:1002,1121-1123,1135-1137,1188-1190` | 3 static `ResolveExport("?WaitForFinish@Thread@System@@QEAAXXZ")` sites | 3 (loader-teardown waits) |
| `standalone/loader_loop_stubs.cpp:82-96` | `WaitForEngineLoader()`, a 4th `WaitForFinish` site, gated by `BANISHED_SYNC_ENGINE_LOADER` | 1 (guards a real free-list race, `[[project_menu_heap_crash]]`) |
| `standalone/rt_data_mint.cpp:420-472` | `ConstructLoader`/`RtMintLoaderTeardown` | 2 (cutover-only ctor/dtor of the static `_loader`'s Thread) |

`standalone/reg_mint_promoted.cpp:1875` is **documentation only** — a comment mentioning
`?OnCompleted@Thread@System@@`, no code touching Thread/CriticalSection.

Registrations (`thread_win.cpp`, 331 ln, confirmed): 9 exports (`OnCompleted`,
`CriticalSection::{Ctor,Dtor,Lock,Unlock}`, `Thread::{Ctor,Dtor,Start,WaitForFinish}`) + 1
addr row (`ThreadFunc` alias, 0x1800ca440) = 10 registrations.

Zero-CALL: none in this family are dead — `ThreadFunc` and `OnCompleted` have zero
*textual* call sites but are reached indirectly (`ThreadFunc` handed to `_beginthreadex`;
`OnCompleted` invoked through `self->vtable[1]`), so they are live, just indirect.

**Surprises:** (1) three of the nine caller files (`singleton_ctors.cpp`,
`audio_stream_win.cpp`, `rt_data_mint.cpp`) embed a `System::Thread` as the base of another
step's class (`IO::Persistence`, `Audio::Stream`, `Engine::Loader`) — converting `Thread`'s
ABI has blast radius outside this step's nominal file set; coordinate before touching
`Thread.h`'s layout. (2) `IO::Persistence` and `Audio::Stream` call raw
`InitializeCriticalSection`/`SetCriticalSectionSpinCount(5000)` directly, **bypassing**
`System::CriticalSection`'s own ctor export — a real RAII conversion of `CriticalSection`
would not automatically cover those two sites. (3) `Engine::Loader`'s Thread is a static
singleton built by `standalone/rt_data_mint.cpp`'s `ConstructLoader`. (Coordinator correction:
the first draft said it has "two construction routes (DLL-present vs cutover)". Only ONE is
live — the DLL-present route was removed by ENDGAME A.1 — so there is nothing to choose
between; `ConstructLoader` is the construction site.)

### 5.2 `Random`

Counts (comment/string-aware, re-verified exact against an earlier pass):

| pointer | CALL | GUARD | CACHED |
|---|---|---|---|
| `fn_Random_Rand` | 75 | 28 | 26 |
| `fn_Random_RandFloat` | 38 | 11 | 22 |
| `fn_Random_RandTol` | 25 | 6 | 11 |
| `fn_Random_Default_Ctor` | 13 | 5 | 1 |
| `fn_Random_RandFloatTol` | 31 | 0 | 5 (declared `extern "C"` directly, confirmed) |

**Bypass swarm, the biggest finding for this class**: at least **14 additional files**
resolve a Random export via their own file-static cache, entirely independent of the
`fn_Random_*` globals above (the one previously-known extra, `s_fn_randFloat` in
`action_child_ctors2.cpp`, plus 13 more found this pass): `gameupdateloop_load.cpp`
(`s_fn_Random_RandFull`), `worldgen_f390_ranges.cpp` (`s_fn_Rand2`, `s_fn_RandFloatTol`),
`worldgen_f390_coastline.cpp` (`s_RandFloatTol`, `s_Rand1`, `s_Rand2`),
`citizenai_leisure.cpp` (`s_randFloatTol`), `weather_sim.cpp`/`weather_particles.cpp`
(`s_fnRandFloatTol` each), `firecomponent_ignite.cpp` (`s_randRange`),
`growthcomponent.cpp` (function-local `static fn_Rand_t s_fn_Rand`),
`citizenbehavior.cpp` (`s_fn_Random_Rand`), `action_pf_ctors.cpp` (`s_fn_RandFloatTol`),
`tornado.cpp` (`f.randFloatTol` struct field), `action_pf_geom.cpp` (`s_rand`, with an
import-slot fallback), `foresterwork.cpp` (`s_fn_Random_RandTol`),
`gameupdateloop_serialize.cpp` (`s_fn_Random_Serialize`). All resolve the same export
addresses `fn_Random_*` already provides — functionally harmless duplication (same code
runs either way) but a real consistency hazard for a conversion sweep: leaving them alone
means stale duplicate resolution logic coexists with the new class indefinitely; sweeping
all ~15 is extra scope.

**Construction is NOT one shared RNG** — see §3. At least two long-lived independent
streams (`InternalFactory::_random`, `Graphics::Particle::_rng`) plus many ephemeral
per-call stack instances across `code/Game/`. **A naive "one shared `System::Random`
singleton" conversion would silently merge streams retail keeps separate and would break
determinism of world-gen, weather, and per-entity behavior.**

Registrations: `value_types.cpp`, 10 of the file's 20 exports (Ctor, CtorSeed, Rand,
RandFloat, RandTol, RandFloatTol, and others). No vtable (§2).

### 5.3 `Color`

Only 6 CALL sites total, all through `fn_System_Color_Serialize` — confirmed as its ONLY
caller by grepping `Color@System@@QEAA` (every other hit is registration/doc/census, never
a call site): `code/Game/Game/menuscene.cpp:521-524` (ambient/sun/sky, 3 sites),
`code/Game/Game/tornado.cpp:550-551` (tint, 2 sites), `code/Game/Component/
mapdescription.cpp:66` (1 site). No raw Color ctor call and no Color field construction
exist in-tree — every use is a raw `uint32_t` field, touched only through `Serialize`.
3 ctors + assign + 4 accessors are retail-exported but never registered (§6).

### 5.4 `Configuration`

Counts: `fn_Configuration_GetVersionString` CALL=3, `fn_Configuration_CurrentVersion`
CALL=1, `fn_Configuration_CurrentBuildId` CALL=1 — all re-verified exact.

**Correction to `Value.h`'s own comment.** `Value.h:20-21` says "Configuration has no
instance data (every member in scope is `static`)" — true only for the three members that
live in `value_types.cpp`'s lane (`CurrentBuildId`/`CurrentVersion`/`GetVersionString`,
genuinely `static` C++ members). `configuration.cpp` (740 ln, a different lane) later
ported **instance** methods (`SetCommandLine`, dtor, `GetCommandLineString`) that operate
on `this+0x10` (a string→string map) and a third un-destroyed sub-object at `this+0x20`
that retail itself leaks (`configuration.cpp:648-654`, explicitly not "fixed", per project
rules on faithful behavior). **Configuration is a singleton with real per-instance state,
not a pure-static utility class** — a batch owner must not model it as stateless. Singleton
pointer: `?_singleton@Configuration@System@@0PEAV12@EA` @ 0x1801fc6f8
(`rt_data_mint_tables.inc:4704`), one instance process-wide. The map-insert subtree
(0x18002a090/0x18002a170, red-black insert) is **not ported** but is a **live** dependency
reachable from the settings-load path (`persistence_registry_win.cpp:266`), not dead code —
flagged explicitly in `configuration.cpp:729-738` as a previously-corrected "unreachable"
claim; don't re-assert it's dead.

### 5.5 `Compression`

**No `fn_Compression_*` pointer exists in `engine_funcs.h` at all** (confirmed, zero
matches) — the seed doc's "not found in the earlier pass" is the SAME blind spot found for
Thread/CriticalSection, not evidence of near-zero use. Real callers, all via
`RT_EXPORT_ONCE` inside `runtime/src/io/` (IO-owned files, cited here as caller evidence
only, not this step's to convert):

- `compressed_stream_read.cpp:299-302,403-406` — 2 calls to `?Decompress@Compression@
  System@@SAHPEBXIPEAXI@Z`.
- `compressed_stream_write.cpp:473-476` — 1 call to `?Compress@Compression@System@@
  QEAAHPEBXIPEAXI@Z`.
- `compressed_stream_write.cpp:686-690` — construction site (`CompressedStream::Create`
  allocates 8 bytes, calls `??0Compression@System@@QEAA@XZ`, stores at `self->compression`)
  — one-per-owner, not a singleton (§3).

**Verdict: 4 real call sites, not near-zero.** No `s_fn_Compression_*`-style cache exists
in `code/`; all callers resolve fresh each site inside `runtime/src/io/`.

Registrations: `compression.cpp`, 3 exports (Decompress, Compress, Ctor) + 1 addr
(Decompress, 0x1800c4660) — corrected from an earlier "~4/2" guess. The dtor (0x1800c4170)
is deliberately not ported (teardown is inlined elsewhere already, per the file's own
banner).

### 5.6 `Timer` / `Time` / `DateTime` — three distinct classes, settled definitively

**`System::Timer`** (`timer.cpp`, `@Timer@System@@`): a **stateless static class**. Every
data static is a file-scope global; `Timer::Assign`'s body is literally `return self;`.
Owns QPC/tick-conversion. **No `fn_Timer_*` pointer exists in `engine_funcs.h` at all** —
its entire caller surface is `RT_EXPORT_ONCE`/`ResolveExport` inside `runtime/src/`:

| export | CALL | sites |
|---|---|---|
| `?SecondsToTicks@Timer@System@@SA_JM@Z` | 3 | `runtime/src/engine/loops.cpp:389`, `runtime/src/graphics/wave28_lane_partsys.cpp:322`, `runtime/src/system/value_types.cpp:225` (inside `Time::Ctor`) |
| `?TicksToSeconds@Timer@System@@SAM_J@Z` | 3 | `loops.cpp:392`, `wave28_lane_partsys.cpp:319`, `value_types.cpp:234` (inside `Time::AsSeconds`) |
| `?Startup@Timer@System@@SA_NXZ` | 1 | `runtime/src/engine/engine_lifecycle.cpp:525` |
| `?CurrentTimeTicks@Timer@System@@SA_JXZ` | 1 | `loops.cpp:1453-1456` (cached via raw `ResolveExport`, `MainLoop::Draw`'s frame timestamp) — **this contradicts `timer.cpp`'s own banner ("no file names `CurrentTimeTicks`"), which is now stale**; `loops.cpp` has since grown a real caller |
| `CurrentTime`, `GetTicks`, `Shutdown`, `PlatformShutdown`, direct `PlatformStartup`, `Assign` | **0** | uncalled anywhere in the tree |

**`System::Time`** (`value_types.cpp`, `@Time@System@@` — one character from `Timer`, easy
to conflate, keep the names visually distinct in any future header): a value type wrapping
one `int64_t ticks` (the struct itself is `engine/types/common.h:12-14`, included via
`Value.h:77`, the B.3.3-step-6B dedup fix already in place). 3 exports: ctor (delegates to
`Timer::SecondsToTicks` by name), `AsSeconds` (delegates to `Timer::TicksToSeconds`),
`Serialize`. `g_fn_Time_Serialize`, `fn_Time_AsSeconds`, `fn_Time_Ctor_Float` are ALL
`Time`, confirmed by their resolved mangled strings (`@Time@System@@`, never `@Timer@`).

| pointer | engine_funcs.h CALL | bypass-cache CALL (additional, not double-counted) | true total |
|---|---|---|---|
| `fn_Time_Ctor_Float` | 111 | 21 (11 files: `action_base_cnr`, `action_base_complete`, `action_base_movedeps`×2, `boatcomponent_states`, `orchardcomponent_states`, `tradecomponent_states`×2, `autosavedialog` (falls back to the real pointer only on a null slot), `work_act_consume`, `work_act_hunt`×2, `work_act_perform`×7, `tornado.cpp`×2) | 132 |
| `fn_Time_AsSeconds` | 47 | 14 (`particlecomponent.cpp`×4, `timegoalinstance.cpp`×1, `tornado.cpp`×9) | 61 |
| `g_fn_Time_Serialize` | 32 | 5 (`performwork.cpp`×2, `playanimation.cpp`×2, `rawmaterialcomponent.cpp`×1) | 37 |

All bypass sites resolve either by name (`ResolveExport`) or the shared exe dispatch slot
`0x140152b00` (Time ctor), repeated across 6+ files. **This is the fourth recurrence of the
engine_funcs.h blind spot in this inventory (after Thread/CriticalSection, Random,
Compression)** — treat it as the rule, not the exception, for this whole namespace.

**`System::DateTime`** (`value_types.cpp`, `@DateTime@System@@`): a third, unrelated,
opaque 8-byte FILETIME, no relation to Timer/Time. 3 exports registered (`AsString`,
`DateAsString`, `TimeAsString`); 3 more retail exports (`AsFileTime`, `Serialize`,
`IsValid`) exist only in `standalone/rt_export_addrs.inc`'s static address table —
**unregistered/unimplemented anywhere in the tree**. The seed "1 caller" claim
(`fn_DateTime_AsString`, `code/Game/Dialog/saveloaddialog.cpp:322`, re-verified by direct
read) is **technically correct for the `fn_DateTime_AsString` pointer** but misses a real
DateTime **producer**: `runtime/src/io/filesystem_timestamp_win.cpp`'s
`FileSystem::GetFileTimeStamp` (IO-owned, not touched) returns a `DateTime` by value,
consumed in `runtime/src/io/persistence_savelist_job.cpp:288-290` via a locally-cached
`RtResolveCached` pointer — invisible to any `fn_DateTime_*` sweep, though it doesn't call
any `DateTime` *method*, just produces the raw stamp.

No vtable on any of the three (§2).

### 5.7 `Crc`

One static function, `CRC::Compute` (`crc.cpp`, 71 ln, 1 export/1 addr, confirmed).
Reflected CRC-32, table generated locally, verified byte-identical to retail per the file's
own comment; does not complement the seed on entry. Only caller:
`runtime/src/io/persistence_saveinfo_job.cpp` (IO-owned, `RT_EXPORT_ONCE`) — evidence only.

### 5.8 `Error`

No struct — two named data slots (`_errorString char[0x2000]`, `_stopApplication bool`,
`_hideDisplay bool`) via `RT_DATA_SLOT`. `error.cpp` (579 ln): 3 registered rows —
`SetText` (0x1800c5fa0, appends+`\n`, sets `_stopApplication`), `Display` (0x1800c6060,
fatal `MessageBoxW`, **UNVERIFIED AT RUNTIME per its own banner** — no oracle run has
exercised it), and (confusingly, hosted in this file to avoid a concurrent edit to
`string.cpp`) `System::String::StrAppend` (0x1800c9020) — **not an Error export despite
living in `error.cpp`**, don't miscount it as one. `HideErrorDisplay` (0x1800c5f90) is
deliberately absent, documented dead by a 4-binary sweep. Callers are internal to the
runtime port (`allocator.cpp`'s `FatalOutOfMemory`, `device_startup.cpp`,
`filesystem_win.cpp`, `platform_startup_win.cpp`, `engine_lifecycle.cpp`,
`input_platform_startup_win.cpp`, `dx11/src/{contextdata,devicedata}.cpp`) — none via a
`fn_Error_*` pointer, since none exists.

### 5.9 `WideString`

`runtime/src/system/widestring.h` (46 ln, in `src/` not `types/`): one member
`wchar_t* data`. `widestring.cpp` (599 ln): **8 exports/8 addr, corrected from an earlier
"~9/8"**: `SetLength` (private allocator), `Set` (assign, no-op on unchanged), `AsUTF8`
(→`System::String`), 3 ctors (`const String&`, `wchar_t*`, `char*`), dtor, `StrLength`
(`wcslen`, not null-tolerant). 21 further retail exports are zero-caller and unregistered.
**No conflation with `UI::WideString` found** — a full-tree grep for `UI::WideString`,
`@WideString@UI@@`, `UIWideString` returns **zero hits** anywhere; every `WideString`
occurrence in this sweep carries `@System@@`. No `s_fn_WideString_*` bypass cache found —
this is one of the three families (with Crc and Error) where the engine_funcs.h blind spot
does NOT recur; all callers use `RT_EXPORT_ONCE`/direct address, but there's no competing
`fn_WideString_*` global to bypass in the first place, so this is a structural non-finding,
not evidence the pattern is absent elsewhere.

---

## 6. Zero-CALL pointers

| pointer | CALL by name | by-address route checked? | verdict |
|---|---|---|---|
| `fn_MemZeroFill` | 0 | not found registered anywhere | ZERO-BY-NAME-AND-ADDRESS |
| `Timer::CurrentTime`, `Timer::GetTicks`, `Timer::Shutdown`, `Timer::PlatformShutdown`, direct `Timer::PlatformStartup`, `Timer::Assign` | 0 | registered (in `timer.cpp`'s export table) but never called from anywhere in the tree by name; not separately re-checked by address beyond the registration itself | ZERO-BY-NAME-ONLY — each has a real registered body, so a by-address route through a table this sweep didn't see cannot be ruled out; do not delete without checking `standalone/rt_data_mint_tables.inc`/`cutover_manifest.inc` for an indirect reference first |
| `DateTime::AsFileTime`, `DateTime::Serialize`, `DateTime::IsValid` | 0 | **not even ported** — no `RtSystem_DateTime_*` body exists; found only in `rt_export_addrs.inc`'s static address table | ZERO-BY-NAME-AND-ADDRESS (no body to reach) |
| `Error::HideErrorDisplay` | 0 | documented dead by a prior 4-binary sweep, in `error.cpp`'s own comment | ZERO-BY-NAME-AND-ADDRESS, previously confirmed |
| `Color`'s 3 ctors + assign + 4 accessors | 0 | retail-exported, never `RegisterRuntimeExport`-ed; not independently address-checked this pass | ZERO-BY-NAME-ONLY (registration itself is absent, so "by address" doesn't apply the same way — flag as UNKNOWN whether anything reaches these by a raw address load) |
| `WideString`'s other 21 retail exports (beyond the 8 registered) | 0 | per the file's own banner census check | ZERO-BY-NAME-AND-ADDRESS, per the file's own prior audit — not independently re-verified by address this pass |

**Caution, restating B34_RULES §6.5(f) for this family:** a name-based lexer cannot see an
indirect call through a function-pointer table. None of the rows above were cross-checked
against `standalone/rt_data_mint_tables.inc` for a by-address indirect reference except
where stated — treat the "ZERO-BY-NAME-ONLY" rows as **not yet cleared for deletion**.

---

## 7. Proposed batches for 4b

Disjoint by construction (each targets a different set of `runtime/src/system/*.cpp` files
and a different, non-overlapping `code/Game` caller footprint), ordered by dependency and
risk:

1. **Batch S1 — the non-polymorphic small classes with clean, fully-visible caller
   surfaces**: `Crc`, `Error`, `Color`, `DateTime`. Smallest CALL counts (6, ~10 combined,
   4 real), no vtable, no bypass-cache swarm found for Crc/Error/DateTime (Color has none
   either — it has no live constructor at all). One owner, one commit. Good first move to
   prove the pattern, same role batch A played for IO callers.
2. **Batch S2 — `WideString`**: self-contained, 8 exports, no bypass caches, callers live
   entirely in IO/application files this step doesn't own (evidence only) — converting the
   TYPE is this step's job; the callers are someone else's caller-batch problem later. Low
   risk, can run in parallel with S1.
3. **Batch S3 — `Configuration`**: singleton with real instance state (§5.4) — needs the
   map/sub-object design settled before conversion, one owner, do not fold into S1 given the
   live map-insert dependency on the settings-load path.
4. **Batch S4 — `Timer`/`Time`**: `Timer` is a pure static-function conversion (5 accessors
   + Startup, no instance data). `Time`'s ctor/AsSeconds/Serialize must move in the SAME
   commit as `Timer`'s since they delegate to it by name today — splitting them risks a
   half-converted delegation. Caller-side work is large (68/61/37 real CALL counts including
   bypass caches) and should be a SEPARATE later batch, directory-partitioned like the IO
   caller batches, only after the receiver exists.
5. **Batch S5 — `Random`**: receiver conversion is small (no vtable, straightforward
   MT19937 state struct) but the construction-site finding (§3, §5.2 — two long-lived
   independent streams plus dozens of ephemeral ones) means the batch owner must NOT
   collapse instances. Caller-side work (75+38+25+13+31 = 182 core CALL + ~15 files of
   bypass duplication) is a second, later batch.
6. **Batch S6 — `Thread`/`CriticalSection`**: highest coordination cost. `Thread` is the
   one polymorphic class in scope and its layout is shared with `IO::Persistence` and
   `Audio::Stream` (§5.1) — this batch's owner must coordinate with whoever owns those
   classes before changing `Thread.h`. Recommend running LAST among the small classes, not
   first, despite being conceptually simple, because of that blast radius.
7. **Batch S7 — `MemoryAllocator`**: explicitly OUT of B.3.4b scope per B34_RULES §6.5(c)
   (already deferred once from B.3.4 IO) — 1,146+ CALL sites, a settled door design
   (`alloc_door.cpp`) that must not be disturbed, and the 12-file/17-call-site private-cache
   surface (§4.2, per-line verified, superseding two earlier wrong file counts) plus
   `rtcoll::DoorAlloc`/`DoorFree`'s own 9-file caller list (§4.4) that need their own sweep
   before anyone touches the class. **Recommend deferring past B.3.4b too**, as its own step
   with its own coordinator, exactly as B34_RULES already argued for B.3.4 proper — nothing
   here changes that judgment, it only sharpens the file counts.
8. **Batch S8 — `Compression`**: small (3 exports, 4 real callers, all inside IO's files) —
   could ride along with whichever IO block converts `CompressedStream`, since its only
   construction site and both call sites are physically inside `runtime/src/io/`. Flag for
   the coordinator to decide: convert here as an orphan, or hand to the IO owner as a
   dependency of `CompressedStream`.

**What must go first, across all batches:** nothing in System-rest has an IO-style
"receiver before caller" ordering constraint except S4 (Timer before Time's callers) and
S1/S2/S3/S5/S6/S8's own internal receiver-then-caller split. Unlike IO, there is no shared
header multiple classes fight over (each System-rest class's export lives entirely inside
its own `.cpp`), so batches S1-S3, S5, S8 have **no cross-batch file collisions** and can run
fully in parallel. S4 and S6 are the only two with an internal must-be-serial dependency
(Timer-before-Time; Thread-before-anything-that-embeds-it).

**What is deferred past 4b, and why:** `MemoryAllocator` (S7, per B34_RULES §6.5(c),
sharpened here), and the caller-side sweeps for `Random`/`Time`/`Thread` (large, and depend
on their own receivers existing first — same "receiver first, then callers" rule B34_RULES
applied to IO).

---

## 8. Surprises

1. **The engine_funcs.h blind spot is not a one-off — it recurred in 4 of 8 families
   swept** (Thread/CriticalSection, Random, Compression, Time), always in the same shape:
   a file-static `s_fn_*` (or similarly named) cache resolving the SAME mangled export
   `engine_funcs.h` already exposes through a global `fn_*`, entirely independently. This
   confirms B34_RULES §6.5(d)'s warning was not IO-specific — it's a standing property of
   this codebase's caller idiom, and any future System/IO inventory should sweep raw
   mangled strings FIRST, `engine_funcs.h` second, not the reverse.
2. **The MemAlloc member/static asymmetry (§4.1) contradicts the intuition that "member"
   and "static" are just two spellings of one thing.** They are different retail bodies
   with different locking behavior; a hasty class conversion that gives `MemoryAllocator` a
   single `Allocate` method risks silently adding or removing a lock relative to one of the
   ~540 combined call sites.
3. **The MemFree member/static IDENTITY (§4.1) is the mirror-image trap**: they already
   call the same body today, by accident of a lambda thunk that discards an argument, not
   by design. "Fixing" this to route the member form to the true member body
   (0x1800c6c00) would be the first time that address is ever exercised — an unverified
   change dressed as a cleanup.
4. **`System::Random` is not one RNG.** At least two independent long-lived streams plus
   many ephemeral ones (§3, §5.2) — the single most consequential fact for anyone tempted
   to "simplify" Random into a shared singleton, since doing so would change world-gen/
   weather/behavior determinism.
5. **`Configuration` has real instance state despite `Value.h`'s own comment claiming
   otherwise** (§5.4) — the header comment is correct only for the file it was written
   about; a reader who trusts it without checking `configuration.cpp` will under-scope the
   class.
6. **Thread's vtable and layout are shared across step boundaries** — `IO::Persistence`
   and `Audio::Stream` embed a `System::Thread` base and independently stamp/restamp its
   vtable. This step's owner cannot treat `Thread.h` as a private file.
7. **`CriticalSection`'s own ctor export is bypassed by two of its three known embedders**
   (`Persistence`, `Audio::Stream` call raw Win32 API directly) — a real RAII conversion of
   `CriticalSection` would need a follow-up pass on those two sites, not just its own
   ctor/dtor door, to actually take effect everywhere.
8. **`timer.cpp`'s own banner comment is stale** — it claims no caller for
   `CurrentTimeTicks`, but `loops.cpp` has since grown one (§5.6). A comment inside a
   source file is not a substitute for re-measuring; this is the same lesson B34_RULES
   §7.2 states as a standing rule ("counts are tokens until proven otherwise"), just found
   here in prose form rather than a number.
9. **No System-rest class has the IO-style vtable-retirement hazard at all** (§2) — this
   phase is structurally easier than IO in that one respect: there is no minted table to
   keep alive across a multi-step boundary for any of these thirteen classes except
   `Thread`, and `Thread`'s is already route-scoped and well-understood.
10. **The real method defect this pass found was not a scratchpad filename collision — it
    was two sub-agents each disobeying an explicit "do not write the deliverable" instruction
    and independently writing this whole file, the second overwriting the first with no
    merge.** Disclosed in full in the Method note at the top rather than smoothed over. The
    concrete damage was to §4.2's file count, which the two writers reported differently (one
    lower, one higher) and neither reproducibly — this pass fixed it by re-running the
    grep directly rather than trusting either sub-agent's number. Any future coordinator of a
    multi-agent inventory should treat "the sub-agent's final message says a file was
    written" as a signal to check who actually holds the pen, not just what the numbers say.
