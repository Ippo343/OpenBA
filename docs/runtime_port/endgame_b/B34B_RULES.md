# ENDGAME B.3.4b — the `System` rest: rule-sheet addendum

`B34_RULES.md` is the rule sheet and is **NOT restated here**; sections are cited. This file
adds only what is specific to 4b, and everything in it was **re-derived from source, by mangled
name and by retail address, in this worktree**, because `SYSTEM_INVENTORY.md`'s own banner says
its structural claims are unverified. Where this sheet agrees with the inventory it says so;
where it corrects it, the correction is marked **CORRECTION**.

Read before converting anything: `CLAUDE.md`; `B34_RULES.md` §0, §4.2, §4.3, §6.0, §6.5(c)/(d),
§7.1, §7.2; `B34_BRIEFS.md` §A (all of it) and §F; this file.

---

## 0. The two facts that make 4b a different shape from 4a

**(a) There is no vtable-retirement hazard in `System`, at all.** Verified this pass, not
taken from the inventory:

- `grep -n 'RT_DELDTOR(' runtime/src/object/deleting_dtor.cpp | grep System` returns **two
  lines and both are false positives** — `"Graphics::ParticleSystemInstance"` and
  `"Graphics::ParticleSystem"` match on the substring `System` inside `ParticleSystem`.
  **Zero `System::`-named `RT_DELDTOR` rows.** The count stays 135 (`kSpecCount`).
- `standalone/rt_data_mint_tables.inc` carries exactly **one** `@System@@6B@` vtable row:
  `{ "??_7Thread@System@@6B@", 0x180155f20ull, 2, kVSlots140, kVTable140, kVTraps140,
  "tail-not-a-pointer" }` at line 4024, whose two slots (line 3711-3717) are `_purecall`
  and `RT_MINT_NAMED "?OnCompleted@Thread@System@@UEAAXXZ"` @ 0x18000c590.
- Zero `?_typeInfo@…@System@@` rows anywhere.

So B34_RULES §4.2's "no row may retire" applies to 4b **vacuously except for Thread**, and
Thread's row is live and required (§2 below).

**(b) The POD and the class collide by NAME, which 4a never had to handle.** In IO the PODs
were `RtIo::` / `rt::io::` / `RtIO::`, siblings of `IO`. In System that is true of `Random`,
`Time`, `Color` (they are at **global scope** in `runtime/types/system/Value.h`, not in
`namespace System` — verified) but **NOT of `Thread`/`CriticalSection`**:
`runtime/types/system/Thread.h:17-86` declares `struct System::Thread` and
`struct System::CriticalSection` — the POD already **occupies the class's own name**.

> **RULE B4b-1 (the name-collision rule).** When a POD already occupies the retail name, the
> POD is renamed into `namespace RtSystem` in the same commit that declares the class, and the
> class takes the retail name. The POD does not get deleted and does not get a destructor
> (B34_RULES §1.5 rule 3). Every POD user is swept and re-spelled in that same commit, by
> **include graph**, not by grep: the compiler finds them all because the old name no longer
> resolves to a POD. §7.2's GREP HAZARD is at its worst here — after the commit, a bare
> `Thread` inside any `namespace System {` block binds the CLASS.

`RtSystem` is the established POD namespace for this directory already
(`runtime/types/system/String.h` → `RtSystem::String`, `Filename.h` → `RtSystem::Filename`),
so this introduces no fourth spelling.

---

## 1. The verified class list

Columns: **poly** = has a retail vtable; **mint/deldtor** = rows in
`standalone/rt_data_mint_tables.inc` / `runtime/src/object/deleting_dtor.cpp`; **hdr** = the
type header, if any. "exports" = rows actually `RegisterRuntimeExport`-ed in our tree, counted
by reading the registration function, not by grep.

| class | poly | mint row | deldtor row | hdr today | bodies live in | exports |
|---|---|---|---|---|---|---|
| `System::Thread` | **YES**, `??_7Thread@System@@6B@` @ 0x180155f20, **2 slots** | `.inc:4024` | **none** | `types/system/Thread.h` (POD, in `namespace System`) | `src/system/thread_win.cpp` | 6 |
| `System::CriticalSection` | no | — | — | same file (POD, in `namespace System`) | `src/system/thread_win.cpp` | 4 |
| `System::Random` | no | — | — | `types/system/Value.h`, **global scope** | `src/system/value_types.cpp` | 10 |
| `System::Time` | no | — | — | `engine/types/common.h:12`, global `struct Time` | `src/system/value_types.cpp` | 3 |
| `System::Color` | no | — | — | `types/system/Value.h`, **global scope** | `src/system/value_types.cpp` | 1 |
| `System::DateTime` | no | — | — | **none** (opaque FILETIME) | `src/system/value_types.cpp` | 3 |
| `System::Timer` | no | — | — | **none** (`src/system/timer.h` is free functions) | `src/system/timer.cpp` | 10 |
| `System::Configuration` | no | — | — | **none** | `src/system/value_types.cpp` (3 statics) + `src/system/configuration.cpp` (5 instance) | 8 |
| `System::Compression` | no | — | — | **none** | `src/system/compression.cpp` | 3 |
| `System::CRC` | no | — | — | **none** | `src/system/crc.cpp` | 1 |
| `System::Error` | no | — | — | **none** | `src/system/error.cpp` | 2 (+1 that is a `String` export, §5) |
| `System::WideString` | no | — | — | `src/system/widestring.h` (in `src/`, not `types/`) | `src/system/widestring.cpp` | 8 |
| `System::MemoryAllocator` | no | — | — | `types/system/MemoryAllocator.h` | `src/system/allocator{,_win}.cpp` | ~28 |

**Two classes the inventory does not list at all, both found this pass:**

- **`System::Debug`** — `?_disableHandler@Debug@System@@0_NA` @ 0x1801fc708 is a
  `CUTOVER_REQUIRED` row (manifest line 69). Non-comment tree readers: **two**, both in
  `runtime/src/video/enumerate_video_modes.cpp`, plus a doc mention in
  `standalone/reg_mint_promoted.cpp`. **No body is ported and no member is registered.**
  Verdict: nothing to convert; the storage row stays (§4).
- **`System::StackCriticalSection`** — a real retail RAII class,
  `??0StackCriticalSection@System@@QEAA@AEAVCriticalSection@1@@Z` @ 0x180034fa0 and
  `??1…@QEAA@XZ` @ 0x180034f90. Unported, unregistered, zero tree readers.
  **It is `CriticalSection`'s scoped-lock helper and it is the retail-sanctioned RAII shape**,
  which matters to §2: retail did NOT give `CriticalSection` RAII, it gave it a separate
  guard class. Recorded so nobody "modernises" `CriticalSection` into a `std::lock_guard`
  shape and calls it faithful. Not scheduled: an uncalled port is an unverified port.

### 1.1 Exported members, by mangled name and retail address

Addresses are from `standalone/rt_export_addrs.inc` (generated from the retail DLL), cited by
line. `R` = registered by name in our tree; `A` = also `RegisterRuntimeAddr`-ed.

**`System::Thread`** — `thread_win.cpp:307-330`
| mangled | addr | body | R/A |
|---|---|---|---|
| `??0Thread@System@@QEAA@I@Z` | 0x1800ca4e0 | `RtSystem_Thread_Ctor` | R |
| `??1Thread@System@@QEAA@XZ` | 0x1800ca590 | `RtSystem_Thread_Dtor` | R |
| `?Start@Thread@System@@QEAAXXZ` | 0x1800ca610 | `RtSystem_Thread_Start` | R |
| `?WaitForFinish@Thread@System@@QEAAXXZ` | 0x1800ca5f0 | `RtSystem_Thread_WaitForFinish` | R |
| `?OnCompleted@Thread@System@@UEAAXXZ` | 0x18000c590 | `RtSystem_Thread_OnCompleted` — **slot 1** | R |
| `?ThreadFunc@Thread@System@@CAIPEAX@Z` | 0x1800ca440 | `RtSystem_Thread_ThreadFunc` | R + **A** |
| `?IsRunning@Thread@System@@QEBA_NXZ` | 0x18000c5a0 | **UNPORTED**, zero tree readers | — |

Slot 0 is `_purecall` (0x18007ec28) in the minted row. Its **name is `Run`**, established
positively rather than guessed: the one derived class whose table we hold,
`Engine::Loader`, exports its override as **`?Run@Loader@Engine@@UEAAXXZ` @ 0x1800dbfc0**
(`rt_export_addrs.inc:3695`), and `runtime/types/engine/UpdateLoop.h:40` records that table as
`[slot 0 Run 0x1800dbfc0, slot 1 0x18000c590]`. So the declaration is
`virtual void Run() = 0;` at slot 0 and `virtual void OnCompleted();` at slot 1, **no virtual
destructor** (ENDGAME decision 2).

**`System::CriticalSection`** — `thread_win.cpp:312-315`
`??0CriticalSection@System@@QEAA@XZ` 0x18000c5c0 · `??1…@QEAA@XZ` 0x18000c5b0 ·
`?Lock@…@QEAAXXZ` 0x18000c700 · `?Unlock@…@QEAAXXZ` 0x18000c6f0. All R, none A.

**`System::Random`** — `value_types.cpp:506-516`, 10 rows, none A:
`??0Random@System@@QEAA@XZ` 0x1800c7f20 · `??0…@QEAA@I@Z` 0x1800c7ed0 ·
`?NextGroup@…@AEAAXXZ` 0x1800c7dd0 · `?Rand@…@QEAAIXZ` 0x1800c7f90 ·
`?Rand@…@QEAAII@Z` 0x1800c8040 · `?Rand@…@QEAAHHH@Z` 0x1800c7d70 ·
`?RandFloat@…@QEAAMXZ` 0x1800c7d40 · `?RandFloatTol@…@QEAAMXZ` 0x1800c8000 ·
`?RandTol@…@QEAAHH@Z` 0x1800c7da0 · `?Serialize@…@QEAAXAEAVSerializer@IO@@@Z` 0x1800c7cf0.
`?Initialize@Random@System@@AEAAXI@Z` 0x1800c7e90 is retail's private seeding member; our tree
has **no body and no registration** for it under that name (the two ctors inline the
`init_genrand` recurrence). Do not "restore" it as a member without a port.

**`System::Time`** — `value_types.cpp:518-520`: `??0Time@System@@QEAA@M@Z` 0x1800078c0 ·
`?AsSeconds@…@QEBAMXZ` 0x180007900 · `?Serialize@…@QEAAXAEAVSerializer@IO@@@Z` 0x1800ca740.
Retail also exports `??0Time@System@@QEAA@XZ` 0x1800078b0, `??0…@QEAA@_J@Z` 0x1800078e0,
`?AsTicks@…@QEBA_JXZ` 0x180007920 and `?SerializeText@…` 0x1800ca640 — **unported**.

**`System::Color`** — `value_types.cpp:522`: **one** row,
`?Serialize@Color@System@@QEAAXAEAVSerializer@IO@@@Z` 0x1800c4150. Retail exports 3 ctors
(0x18001fdd0/0x18001fde0/0x18001fdf0), `?AsUInt@` 0x18001fe20, four byte accessors
(0x18001fe30/40/50/60), four Normalized* (0x18001fe70/90/b0/d0) and `?SerializeText@`
0x1800c4060 — **none registered, none ported**.

**`System::DateTime`** — `value_types.cpp:524-533`: `?AsString@` 0x1800c54d0 ·
`?DateAsString@` 0x1800c56a0 · `?TimeAsString@` 0x1800c55b0. **CORRECTION to the inventory's
§6:** it lists `AsFileTime`/`Serialize`/`IsValid` as the only unported trio; there are **six**
unported retail exports — add `??0DateTime@System@@QEAA@XZ` 0x1800c57f0,
`??0…@QEAA@U_FILETIME@@@Z` 0x1800c5800 and `??1…@QEAA@XZ` 0x1800c5860 — plus
`?AsFileTime@` 0x180026dc0, `?Serialize@` 0x1800c57a0, `?IsValid@` 0x1800c5810.

**`System::Timer`** — `timer.cpp:332-341`, 10 rows, none A:
`??4Timer@System@@QEAAAEAV01@AEBV01@@Z` 0x180007890 (assign; body is `return self;`) ·
`?Startup@` 0x1800ca7c0 · `?Shutdown@` 0x1800ca7b0 ·
`?PlatformStartup@` 0x1800ca860 · `?PlatformShutdown@` 0x1800ca8e0 · `?GetTicks@` 0x1800ca840 ·
`?CurrentTime@` 0x1800ca760 · `?CurrentTimeTicks@` 0x1800ca8f0 · `?SecondsToTicks@` 0x1800ca800 ·
`?TicksToSeconds@` 0x1800ca820. Five statics live in the eleven cutover rows (§4).

**`System::CRC`** — `crc.cpp:68-69`: `?Compute@CRC@System@@SAIPEBXHI@Z` 0x1800c5490, **R + A**.
Retail also exports `?Compute@CRC@System@@SAIPEBDI@Z` 0x1800c5430 (the `const char*`
overload) — **unported**; the class static `?_crcTable@CRC@System@@0PAIA` @ 0x1801f9080 exists
in the export table but is **NOT** a cutover row (our port generates the table locally).

**`System::Error`** — `error.cpp:547-569`: `?SetText@` 0x1800c5fa0 (R+A) · `?Display@`
0x1800c6060 (R+A). `?HideErrorDisplay@` 0x1800c5f90 is documented-dead. The third row in that
function is `?StrAppend@String@System@@SAPEADPEADHPEBD@Z` 0x1800c9020, which is **a
`System::String` member hosted in this file** — the inventory says so and it is correct; do not
count it as an Error export and **do not move it**: `System::String` is a real class since
B.3.1 and that registration is its door, sitting here only to avoid a concurrent edit.

**`System::WideString`** — `widestring.cpp:558-591`: 8 R + 8 A.
`?SetLength@…@AEAAXH@Z` 0x1800ca9b0 · `?Set@…@QEAAXPEB_W@Z` 0x1800caf40 ·
`?AsUTF8@…@QEBA?AVString@2@XZ` 0x1800cacd0 · `??0…@QEAA@AEBVString@1@@Z` 0x1800cab50 ·
`??0…@QEAA@PEB_W@Z` 0x18000a6e0 · `??0…@QEAA@PEBD@Z` 0x1800cb110 · `??1…@QEAA@XZ` 0x18000a730 ·
`?StrLength@…@SAHPEB_W@Z` 0x1800cab20.

**`System::Compression`** — `compression.cpp:598-628`: `?Decompress@…@SAHPEBXIPEAXI@Z`
0x1800c4660 (R+A) · `?Compress@…@QEAAHPEBXIPEAXI@Z` 0x1800c4230 (R) ·
`??0Compression@System@@QEAA@XZ` 0x1800c41a0 (R). `??1Compression@System@@QEAA@XZ` 0x1800c4170
is deliberately unported. Three private statics `?First@`/`?Next@`/`?Index@`
(0x180023e20/e30/e40) are unported.

**`System::Configuration`** — split across two files, which is the trap:
`value_types.cpp:534-536` registers the three **statics** (`?CurrentBuildId@` 0x1800c4c60,
`?CurrentVersion@` 0x1800c4f40, `?GetVersionString@` 0x1800c4f90);
`configuration.cpp:694-722` registers the five **instance** members, all R+A
(`?SetCommandLine@` 0x1800c47a0, `?ParseCommand@` 0x1800c4a00, `?ParseValue@` 0x1800c4930,
`?GetCommandLineString@` 0x1800c5300, `??1Configuration@System@@QEAA@XZ` 0x1800c5170) plus a
by-address row `RegisterRuntimeAddr(0x18002ace0, RtSystem_ConfigMap_Find)` that is the map
helper, **not** a Configuration member. **The constructor 0x1800c51b0 is called through
`RT_EXPORT_ONCE` from `runtime/src/engine/engine_lifecycle.cpp:305` but is registered
NOWHERE** — re-verified; a conversion must not invent one. 22 further retail exports
(`?Startup@` 0x1800c4c50, `?Shutdown@` 0x1800c4c40, `?IsCommandSet@` 0x1800c5410,
`?AddCommandLine@` 0x1800c5200, the Language/Platform/Build accessors, the three
ResourceParameter members, `?Get@Configuration@System@@SAPEAV12@XZ` 0x18000da70 …) are
unported.

### 1.2 "Is it a class?" — the verdict per class, and the test that decides it

B.3.4a's `IO::TextCodec` is the precedent for the other answer: four receiverless unexported
free functions, **nothing landed for it, and that is a valid recorded outcome**. So each 4b
class had to be tested rather than assumed.

> **THE TEST (coordinator, 2026-09-19).** Read the MANGLED names. MSVC emits
> `?Name@Class@Namespace@@SA…` (public static), `…@CA…` (private static) or `…@0…A` (private
> static data) **only for a member of a class of that name**. A free function cannot carry a
> class-scoped mangling. `IO::TextCodec`'s functions had no class-scoped exported names at
> all — that is what made it the other shape.

| class | verdict | the evidence | landed |
|---|---|---|---|
| `System::Thread` | **class, polymorphic** | `??_7Thread@System@@6B@`, `UEAA` on `OnCompleted` | SYSTEM (2) |
| `System::CriticalSection` | **class, no vtable** | `QEAA` on all four | SYSTEM (2) |
| `System::Random` | **class with instance state** | `QEAA` on nine, `AEAA` on `NextGroup` | SYSTEM (3) |
| `System::Time` | **class with instance state** | `QEAA` ctor, `QEBA` `AsSeconds` | SYSTEM (3) |
| `System::Color` | **class with instance state** | `QEAA`/`QEBA` on all twelve | SYSTEM (3) |
| `System::DateTime` | **class with instance state** | `QEBA` on all three + `AsFileTime` | SYSTEM (3) |
| `System::Timer` | **class, ALL-STATIC** | 6 × `SA`, 3 × `CA`, 5 × `0…A`, 1 × `QEAA` (`operator=`) | SYSTEM (4) |
| `System::CRC` | **class, ALL-STATIC** | 2 × `SA` (`Compute`), 1 × `0PAIA` (`_crcTable`) | SYSTEM (4) |
| `System::Error` | **class, ALL-STATIC** | 3 × `SA`, 3 × `0…A` | SYSTEM (4) |
| `System::Configuration` | **class with instance state** | 5 × `QEAA` over `this+0x10` (a map) and `this+0x20` | SYSTEM (4c) |
| `System::Compression` | **class with instance state** | `??0` + `QEAA Compress`; an 8-byte heap instance | SYSTEM (4b) |
| `System::WideString` | **class with instance state** | one `wchar_t* data`; 6 × `QEAA`, 1 × `AEAA`, 1 × `SA` | SYSTEM (4a) |
| `System::MemoryAllocator` | **class with instance state** | 0x670 bytes | deferred, §6 |
| `System::Debug` | class, but **nothing ported** | one `0_NA` static, no function anywhere | nothing to do |
| `System::StackCriticalSection` | class, but **nothing ported** | `??0`/`??1` only, zero readers | nothing to do |

**None of the thirteen came out `TextCodec`-shaped.** The three all-static ones came closest
and are still classes — `Timer`, `CRC` and `Error` each declare `class X { public: static … };`
with the bodies moved and the doors delegating, which is the established shape for that case.
Their *data* statics are a separate question and are governed by rule B4b-4 (§4.1), not by
this table.

### 1.3 SYSTEM (4a) — `System::WideString`: what converted, what did not, and why

**Verdict: a class with instance state**, by the §1.2 test: seven `this`-carrying members
(6 × `QEAA` + 1 × `AEAA` on the private `SetLength`) plus one `SA` static, `?StrLength@`. A
free function cannot carry a class-scoped mangling and a `SA` row cannot exist without the
class, so retail named it and hung `StrLength` off it.

| converted | not converted, and why |
|---|---|
| the three constructors (0x1800cab50 / 0x18000a6e0 / 0x1800cb110), the destructor (0x18000a730), `Set`, `AsUTF8`, `SetLength`, `StrLength` — **all eight bodies moved onto members** | the 21 other exports of the class: no caller anywhere in `code/`, `engine/`, `runtime/`, `standalone/` or `dx11/`, and an uncalled port is an unverified port |

**It is the first 4b class to take REAL CONSTRUCTORS AND A REAL DESTRUCTOR, and that is the
one place it departs from `Random`/`Time`/`Color`/`DateTime`.** Those four deferred their
constructors for a stated reason each (§1.1 and `Value.h`'s banner): a by-value holder that
would be seeded twice, 132 raw-buffer call sites still to convert, or — for `Color` and
`DateTime` — no ported body to move at all. **None of those reasons exists here.** All four of
WideString's lifecycle exports have bodies in this tree today and all four are registered, and
B34_RULES §4.3's test passes vacuously because the class has no vtable. So the doors
placement-new the class into the caller's raw bytes and call `~WideString()` explicitly,
which is exactly `System::String`'s B.3.1 step-1 shape one type over.

**RULE B4b-1 DOES NOT FIRE.** The POD is `RtSystem::WideString` and has been since Wave 9: it
never occupied the class's name, so nothing is renamed and no by-value holder changes
spelling. The four files that hold one by value and drive it through the doors —
`runtime/src/application/platform_startup_win.cpp`, `application_win.cpp`,
`runtime/src/engine/updateloop_loadresources.cpp` and `runtime/src/system/value_types.cpp` —
are untouched by this commit.

> **The asymmetry this class introduces, and it is asserted in both directions.** The POD stays
> **trivially destructible** (those four holders release by hand through the `??1` door; a
> second, implicit release at scope exit is the `RtSystem::String` double-free) while the CLASS
> is deliberately **NOT** trivially destructible (it owns retail's release idiom). Two types
> over the same eight bytes where exactly one destroys. `WideString.h` asserts both, and
> `tests/unit/system_abi_test.cpp` PART G restates both at runtime, so a "tidy" in either
> direction fails rather than boots.

The layout moved from `runtime/src/system/widestring.h` to
**`runtime/types/system/WideString.h`**, which is where a type header belongs and where the
unit test can reach it; `widestring.h` keeps the POD spelling and the eight door declarations
for its four includers, so no include changes.

### 1.4 SYSTEM (4b) — `System::Compression`: what converted, what did not, and why

**Verdict: a class with instance state.** `??0Compression@System@@QEAA@XZ` (0x1800c41a0) and
`?Compress@…@QEAAHPEBXIPEAXI@Z` (0x1800c4230) carry `this`; `?Decompress@…@SAHPEBXIPEAXI@Z`
(0x1800c4660) is an `SA` public static hung off the same class. Three class-scoped manglings,
so not the `TextCodec` shape.

| converted | not converted, and why |
|---|---|
| the constructor, `Compress` and `Decompress` — **all three ported bodies moved onto members** | `??1Compression@System@@` 0x1800c4170: **unported on purpose.** The live teardown is INLINED into `?Release@CompressedStream@IO@@` (`compressed_stream_release.cpp:104-113`, retail 0x1800aa1f3..0x1800aa26f), which frees the 0x40000 table and then the 8-byte object through `MemFree`. A real destructor would be a second opinion about a teardown that already happens, running on top of it. The class stays **trivially destructible**, asserted. |
| | `?First@` / `?Next@` / `?Index@` (0x180023e20/e30/e40), retail's three private statics: unported, uncalled, undeclared. |

**THE 8 BYTES ARE PINNED BY A CALL SITE, NOT BY A POD.** There was no POD — the type was
`void*` at every door and `*static_cast<uint32_t**>(self)` inside the body — so `Compression.h`
has a `sizeof`/`offsetof` pair and no cross-assert. `sizeof == 8` is a contract with two
allocations that are **outside this step's file set and cannot be told the class grew**:
`runtime/src/io/compressed_stream_write.cpp:629-633` and
`runtime/src/save/saveload_meta.cpp:395-398` both do `MemAlloc(8, 4)` and hand the raw bytes
to the `??0` door BY NAME. The door placement-news into them and returns `self`, which is
retail's `MOV RAX,RCX` and is what both sites store. Neither file changes.

**Two free functions were DELETED and are declared here rather than absorbed into a rename:**
`DecompressBody` and `CompressBody`, the wrappers that existed "so the oracle can run the
identical code onto scratch". The oracles died at cutover; the two bodies are the members now.
Token tally: `DecompressBody` 2→0, `CompressBody` 2→0, `self` 7→5 (exactly `CompressBody`'s
signature and its one dereference, which became `this` / `_table`), and **every other token in
the file is unmoved**, branch counters included.

### 1.5 SYSTEM (4c) — `System::Configuration`, and a CORRECTION to §1.1

**Verdict: a class with instance state**, 0x30 bytes. Nine class-scoped manglings across the
ported surface: three `QEAA`/`QEBA` instance members, two `CA` private statics, three `SA`
public statics and a `QEAA` constructor.

> **CORRECTION to §1.1.** That section says the constructor `??0Configuration@System@@QEAA@XZ`
> (0x1800c51b0) "is called through `RT_EXPORT_ONCE` from
> `runtime/src/engine/engine_lifecycle.cpp:305` but is registered **NOWHERE**". **That is
> wrong.** It is registered twice, by NAME and by ADDRESS, at
> `runtime/src/engine/engine_lifecycle.cpp:995-996`, and its body is at `:322` in the same
> file. (`standalone/reg_mint_promoted.cpp:3026`'s `RT_UNPROMOTABLE` note for
> `?_singleton@Configuration@System@@` still says "the writer `??0Configuration@System@@` is
> unported", which is stale for the same reason and is recorded here rather than edited —
> `standalone/` is another owner's file set.)

| converted | not converted, and why |
|---|---|
| `SetCommandLine`, `GetCommandLineString`, `~Configuration`, `ParseCommand`, `ParseValue`, `GetVersionString`, `CurrentVersion` — **seven bodies moved onto members**, split across `configuration.cpp` and `value_types.cpp` exactly as their registrations are | **the constructor.** Ported and registered, but in `runtime/src/engine/engine_lifecycle.cpp`, reading a file-local `ConfigurationSingletonSlot()`. Splitting a declaration from its body across two owners is how a merge loses one half — §3's own sequencing argument. The class declares no constructor; that door still writes all six slots by hand. |
| | **`?CurrentBuildId@` 0x1800c4c60.** The one ported member left as a free door, and it is an ABI question, not a gap: retail calls `String::Set` on the hidden return slot **without zeroing it first**, where every other String-returning body in this class placement-constructs it. A by-value member plus a placement-newing door would zero the slot instead — either the faithful reading (MSVC inlined `Set`, proved `data` null, folded the store away) or a behaviour change on a slot the caller did not zero. `value_types.cpp` already decided "left exactly as retail spells it"; 4b does not overturn a prior decision on inference. **OWED** once someone reads 0x1800c4c60's prologue. |
| | **`?_singleton@Configuration@System@@0PEAV12@EA` 0x1801fc6f8** — rule B4b-4, debt tabled in §4.1. Stays an `RT_DATA_SLOT` read at `engine_lifecycle.cpp:283`. |
| | the 22 unported exports (`?Startup@`, `?Shutdown@`, `?IsCommandSet@`, `?AddCommandLine@`, `?Get@` 0x18000da70, the Language/Platform/Build accessors, the three ResourceParameter members): no body, no registration, an uncalled port is an unverified port. |

**Not a member, and it stays that way: the map find at 0x18002ace0.** It takes the MAP
(`configuration + 0x10`), not `this`, and §1.1 already records its by-address row as the map
helper rather than a Configuration export. It stays `RtSystem_ConfigMap_Find`.

**THE §7.2 GREP HAZARD BITES HARDEST IN THIS FILE, AND IN A NEW WAY.**
`configuration.cpp` carries `using RtSystem::String;` at global scope, so a bare `String` there
means the **POD**. Inside a `System::Configuration` member body, unqualified lookup reaches
`namespace System` BEFORE the global scope, so the same bare `String` binds **the class** —
silently, because the two are the same eight bytes. Every member body spells
`RtSystem::String` in full where it means the POD, and the using-declaration now carries a
comment saying why. *This is the first place in `System` where the hazard is a
using-declaration rather than a namespace block, and it will recur in any converted file that
has one.*

**A TRAIT TRAP, RECORDED SO IT IS NOT REDISCOVERED.**
`std::is_trivially_default_constructible` **cannot** be used to pin "this class declares no
constructor" once the class has a real destructor: GCC (and libstdc++'s `__is_trivially_*`)
fold the destructor's triviality into `is_trivially_constructible`, so the trait reads FALSE
for a reason unrelated to the constructor. The header asserts
`is_default_constructible` plus `!is_trivially_destructible` and states the strong claim in a
comment. (`System::Compression` is unaffected: it has a constructor and no destructor.)

---

## 2. `System::Thread` — every stamper and reader, re-derived

**Non-comment readers of the vtable 0x180155f20, whole tree, build dirs excluded — TWO:**

1. `runtime/src/system/thread_win.cpp:144-146` — `RtSystemThreadVftable()`, wrapping
   `RT_DATA_SLOT(void*, "??_7Thread@System@@6B@")`. The **by-name** door. Read by
   `RtSystem_Thread_Ctor` (:171) and `RtSystem_Thread_Dtor` (:194).
2. `runtime/src/io/persistence_worker_win.cpp:585-589` — `ResolveAddr(Module::Runtime,
   0x180155f20)` into a function-local `static`, then `p->workerVtable = kBaseThreadVtable;`
   on the worker teardown. The **by-address** stamp. Confirms the inventory's correction (4).

Every other hit of that constant is a comment (`audio_stream_win.cpp` ×5,
`thread_win.cpp` ×3, `Persistence.h:303`, `Thread.h:49`).

**Constructing sites (who runs `??0Thread@System@@QEAA@I@Z`), re-derived — four:**

| site | what it builds | derived table stamped right after |
|---|---|---|
| `code/Game/Game/pathfinder_seed.cpp:142-148` | the pathfinder's Thread subclass, 0x1f8 bytes | exe **0x14015ef18**, by address (`ResolveAddr(Module::Exe, …)`, also read at `keystone_a_gamedtor.cpp:338`) |
| `runtime/src/engine/singleton_ctors.cpp:448-450` (`Ctor_Persistence`, retail 0x1800b8020) | the `System::Thread` **embedded at `IO::Persistence`+0x10** | **0x180155860**, by address, written over `this+0x10` |
| `standalone/rt_data_mint.cpp:434-461` (`ConstructLoader`) | `Engine::Loader`'s static `_loader` | `??_7Loader@Engine@@6B@`, by NAME out of the mint table |
| `runtime/src/audio/audio_stream_win.cpp:303-341` | `Audio::Stream`, a Thread subclass | its own, after the base stamp |

> **RULE B4b-2.** None of those four stamps, and neither of the two vtable readers, changes in
> 4b. `System::Thread` gets **no real constructor and no real destructor**: B34_RULES §4.3's
> test — "would the member form change which vtable a live object wears?" — answers YES for
> every one of the four, and two of them (`singleton_ctors.cpp`, `rt_data_mint.cpp`) are
> outside this step's file set entirely. The ctor/dtor doors stay free functions.

> **RULE B4b-3 — `System::Thread` stays ABSTRACT, and the by-value holder takes the POD.**
> `runtime/types/engine/UpdateLoop.h:137` holds `System::Thread _thread;` **by value** inside
> the POD `RtEngine::Loader`. B34_BRIEFS §A.3/§A.4 would make a by-value-held class concrete —
> but retail's slot 0 here is `_purecall`, there is no registered body to delegate to, and
> inventing one is forbidden. The resolution is the other way round: `RtEngine::Loader` is a
> POD mirror of a retail class, so **it holds the POD** — the member is re-spelled
> `RtSystem::Thread _thread;` and `System::Thread` keeps `virtual void Run() = 0;`. Nothing
> constructs a `System::Thread` as a C++ object (B34_BRIEFS §A.4: abstract costs nothing), and
> `sizeof` still works on an abstract class so every layout pin holds.
>
> **THE READING OF §A.3/§A.4 THIS SETTLES (coordinator ruling, 2026-09-19).**
> §A.3's "a class held BY VALUE as a member needs a public non-virtual destructor" and §A.4's
> "concrete where something holds the class by value" are about a **REAL CLASS holding a REAL
> CLASS** — `IO::CompressedMemoryStream` holds an `IO::MemoryStream`, and both are converted, so
> the enclosing class's implicit members must be able to reach the enclosed one's. **A POD
> MIRROR HOLDING THE POD IS NOT THAT CASE AND DOES NOT TRIGGER THE RULE.** When the holder is
> itself an unconverted POD, the correct move is the POD-to-POD spelling, and the class stays
> whatever retail's table says it is. Stated once here so the next agent who meets a by-value
> holder checks *what the holder is* before reaching for `= default` + `friend`.
>
> **The pins this rule owes, and they are in the header:** `sizeof` and every `offsetof`
> asserted on the class, the same set asserted on the POD, and a **cross-assert of each against
> the other**, so the two spellings can never drift. The class's +0x00 is the implicit vptr
> where the POD's is the explicit `void* vtable` field — that is the one member that does not
> cross-assert and the header says so at the assert.
>
> **Slot 1 is named from evidence, not guessed:** `rt_data_mint_tables.inc:3714` carries it as
> `RT_MINT_NAMED "?OnCompleted@Thread@System@@UEAAXXZ"` @ 0x18000c590, so the declaration is
> `virtual void OnCompleted();`. Only slot 0 needed the `Run` derivation (§1.1).

**The POD's other users, by include graph (this is the sweep RULE B4b-1 demands):**
`runtime/src/system/thread_win.cpp` (converted in the same commit) ·
`runtime/types/engine/UpdateLoop.h:137` (the by-value member) ·
`standalone/rt_data_mint.cpp:149` (`static_assert(sizeof(System::Thread) == 0x30, …)` — **this
line keeps compiling and keeps being true against the CLASS**, so `standalone/` needs no edit
while it is under `b34-closing`) · `runtime/src/engine/loops.cpp:124,724` (a forward
declaration `void RtSystem_Thread_Start(System::Thread* self);` and a `reinterpret_cast` to
it — **also unchanged**, because the door's parameter becomes the class). `audio_stream_win.cpp`
and `engine_funcs.h` mention the header in comments only.

**`System::CriticalSection`.** Non-polymorphic, 0x28 opaque bytes, four exports. Its ctor/dtor
doors ARE in this step's file set and no vtable is swapped, so B34_RULES §4.3 would permit a
real ctor/dtor — and **they still do not convert in 4b**, for the reason B34_RULES §1.5 rule 3
gives: two of its three known embedders (`IO::Persistence`, `Audio::Stream`) call raw
`InitializeCriticalSection`/`SetCriticalSectionSpinCount(5000)` and never reach the door at
all, so a real ctor would leave those two objects constructed a second, different way.
`Lock`/`Unlock` become members; `Ctor`/`Dtor` stay doors.

---

## 3. The `IO::Serializer` int64 overload — DECIDED FROM RETAIL, and it is a YES

`runtime/src/system/value_types.cpp:251-257` (`RtSystem_Time_Serialize`, retail 0x1800ca740)
hand-inlines the ReaderWriter **+0x90** slot — `rw_vtbl::kScalarInt64` — because
`runtime/types/io/Serializer.h:788-791` says retail's int64/uint64 scalar forms are "ABSENT ON
PURPOSE: no call site in this tree reaches them, and an uncalled port is an unverified port."

**The retail evidence, read this pass out of `standalone/rt_export_addrs.inc`:**

```
1204:    { 0x180035000ull, "?Serialize@Serializer@IO@@QEAAXAEA_JPEBD@Z" },   <- int64&, +0x90
2602:    { 0x18007c3d0ull, "?Serialize@Serializer@IO@@QEAAXAEA_KPEBD@Z" },   <- uint64&, +0x88
2606:    { 0x18007c410ull, "?Serialize@Serializer@IO@@QEAAXHPEA_JPEBD@Z" },  <- int64[], +0x38
2607:    { 0x18007c420ull, "?Serialize@Serializer@IO@@QEAAXHPEA_KPEBD@Z" },  <- uint64[], +0x30
2603:    { 0x18007c3e0ull, "?Serialize@Serializer@IO@@QEAAXAEADPEBD@Z" },    <- char&,   +0x70
```

So the overload **exists in retail at 0x180035000** and `Serializer.h`'s note is accurate about
retail while its premise ("no call site in this tree reaches them") is now false:
`Time::Serialize` is that call site, and `g_fn_Time_Serialize` has 32 CALL sites through
`engine_funcs.h` plus 5 bypass-cache sites.

> **DECISION.** Add `void Serialize(int64_t& v, const char* name);  // 0x180035000  +0x90` to
> `IO::Serializer` in the established shape — member body `RwSlot(rw, rw_vtbl::kScalarInt64)`,
> door `?Serialize@Serializer@IO@@QEAAXAEA_JPEBD@Z` registered by name **and** by address
> 0x180035000 exactly as the other 17 are — and convert `RtSystem_Time_Serialize` onto it.
> Add **only** the int64 scalar: the uint64 and the two 64-bit array forms still have no call
> site, and `char&` still has none.
>
> **SEQUENCING.** The member body lives in `runtime/src/io/serializer.cpp`, which
> `b34-closing` owns today. `runtime/types/io/Serializer.h` is not in that agent's declared
> set, but splitting a declaration from its body across two owners is how a merge loses one
> half. **This is deferred to a single commit after `b34-closing` lands**, and the Time site
> stays byte-identical until then — which is what batch H already decided and recorded in the
> body's own comment.

---

## 4. The eleven `System` `CUTOVER_REQUIRED` rows — they are REDIRECTED, never dropped

Re-read from `standalone/cutover_manifest.inc` this pass (`grep -n '"System"'`); the
inventory's list and line numbers are **confirmed exactly**:

| line | symbol | DLL va | owner class |
|---|---|---|---|
| 69 | `?_disableHandler@Debug@System@@0_NA` | 0x1801fc708 | `System::Debug` (unported, §1) |
| 71 | `?_errorString@Error@System@@0PADA` | 0x1801fc720 | `System::Error` |
| 73 | `?_hideDisplay@Error@System@@0_NA` | 0x1801fc709 | `System::Error` |
| 81 | `?_stopApplication@Error@System@@0_NA` | 0x1801fc70a | `System::Error` |
| 75 | `?_secondsPerTick@Timer@System@@0NA` | 0x1801fe728 | `System::Timer` |
| 83 | `?_systemInitialTick@Timer@System@@0_JA` | 0x1801fe738 | `System::Timer` |
| 85 | `?_systemSecondsPerTick@Timer@System@@0NA` | 0x1801fe740 | `System::Timer` |
| 87 | `?_systemTicksPerSecond@Timer@System@@0_JA` | 0x1801fe730 | `System::Timer` |
| 89 | `?_ticksPerSecond@Timer@System@@0_JA` | 0x1801fe720 | `System::Timer` |
| 77 | `?_singleton@Configuration@System@@0PEAV12@EA` | 0x1801fc6f8 | `System::Configuration` |
| 79 | `?_singleton@MemoryAllocator@System@@0PEAV12@EA` | 0x1801fc718 | `System::MemoryAllocator` |

Each is a minted 8-byte (or 0x2000-byte, for `_errorString`) storage cell in
`standalone/rt_data_mint_tables.inc`, answered through `RT_DATA_SLOT` by name and through
`ResolveAddr` by address.

> **RULE B4b-4.** A `static` data member that a class conversion takes over is **REDIRECTED**
> at `standalone/rt_data_mint.cpp`'s `MintStorageCell()` (lines 634-638), exactly as B.3.3
> redirected `?_firstTypeInfo@BaseTypeInfo@Object@@` — one `strcmp` returning
> `(void*)&System::X::_y` instead of `s.storage`, so the name door, the address door and the
> C++ static stay **ONE cell**. Deleting the manifest row, deleting the mint row, or declaring
> the static without the redirect all leave by-name readers on stale bytes and the cutover gate
> failing. Five live by-name readers exist today and must keep working:
> `timer.cpp:164-172` (all five Timer statics, through `RT_DATA_SLOT`) and
> `error.cpp:168-177` (all three Error statics).
>
> **COORDINATOR RULING, 2026-09-19: THE REDIRECT IS NOT 4b WORK.** `Timer`, `Error` and
> `Configuration` convert their **FUNCTIONS** in 4b and leave their **STATICS** as
> `RT_DATA_SLOT` reads, each carrying a one-line comment naming this rule. That is not a
> compromise; it is the same "faithful partial beats a complete-looking deletion" call
> B34_RULES §3.1 made for `IO::Serializer`'s slot calls. The redirect is **OWED**, per row, at
> the class-static conversion — the END of B.3, with the rest of the by-name readers — and the
> debt is recorded here, per row, so nothing is dropped and nobody half-does it.

### 4.1 The redirect debt, per row

Each row below is **OWED a `MintStorageCell()` redirect at the class-static conversion (end of
B.3)**, not in 4b. "Live by-name readers" is what must keep working the instant the static
becomes a C++ member; every one was read this pass.

| row | becomes | live by-name readers today | redirect owed |
|---|---|---|---|
| `?_ticksPerSecond@Timer@System@@0_JA` 0x1801fe720 | `static int64_t System::Timer::_ticksPerSecond` | `timer.cpp:164` | YES |
| `?_secondsPerTick@Timer@System@@0NA` 0x1801fe728 | `static double System::Timer::_secondsPerTick` | `timer.cpp:166` | YES |
| `?_systemTicksPerSecond@Timer@System@@0_JA` 0x1801fe730 | `static int64_t …::_systemTicksPerSecond` | `timer.cpp:168` | YES |
| `?_systemInitialTick@Timer@System@@0_JA` 0x1801fe738 | `static int64_t …::_systemInitialTick` | `timer.cpp:170` | YES |
| `?_systemSecondsPerTick@Timer@System@@0NA` 0x1801fe740 | `static double …::_systemSecondsPerTick` | `timer.cpp:172` | YES |
| `?_errorString@Error@System@@0PADA` 0x1801fc720 | `static char System::Error::_errorString[0x2000]` | `error.cpp:168` | YES |
| `?_stopApplication@Error@System@@0_NA` 0x1801fc70a | `static bool …::_stopApplication` | `error.cpp:171` | YES |
| `?_hideDisplay@Error@System@@0_NA` 0x1801fc709 | `static bool …::_hideDisplay` | `error.cpp:177` | YES |
| `?_singleton@Configuration@System@@0PEAV12@EA` 0x1801fc6f8 | `static Configuration* …::_singleton` | `engine_lifecycle.cpp:283` (the `RT_DATA_SLOT` accessor) | YES |
| `?_singleton@MemoryAllocator@System@@0PEAV12@EA` 0x1801fc718 | `static MemoryAllocator* …::_singleton` | `allocator.cpp:664-668` `SingletonSlot()` | YES — **and not before §6's defer is lifted** |
| `?_disableHandler@Debug@System@@0_NA` 0x1801fc708 | — | none (`System::Debug` is unported, §1) | **NO** — nothing to convert it to; the row simply stays |

Note the `_errorString` row is 0x2000 bytes, not 8: a redirect must return the array's address,
not a pointer to a pointer. `timer.cpp:124-142`'s own banner already explains why all five
Timer statics must be published the same way at the same moment — splitting them is what that
comment exists to prevent, so the five redirect **together or not at all**.

---

## 5. Caller surfaces, per class — the five shapes, and which classes have which

B34_RULES §6.5(d) is the standing rule: **sweep `ResolveExport("…@System@@…")` directly, never
`engine_funcs.h`**. The inventory found the blind spot in 4 of 8 families; this pass found it in
**8 of 13**. The shapes, named so a batch prompt can carry them (the last two are from
`git log -1 --format=%B 920e78a7`):

1. **global `fn_*` / `g_fn_*` door** in `engine/funcs/engine_funcs.h`;
2. **file-static cache** — `static fn_X_t s_fn_X = ResolveExport("…")`, entirely independent of (1);
3. **helper-resolved slot** — `RT_EXPORT_ONCE(Fn, "…")(args)` inline at the call;
4. **mangled-name resolve into a local** — `ResolveExport` assigned to a block-scope `static`;
5. **two-statement `uintptr_t` indirection** — `uintptr_t a = ResolveAddr(Module::…, 0x…);`
   then a cast-and-call, which no name-based lexer sees at all.

Measured file counts (non-comment mangled-name hits, build dirs excluded, docs excluded). These
are **file counts, not CALL counts**; per B34_RULES §7.2 every number is a token until a
per-line read confirms it, and a batch must re-measure before sizing.

| class | `fn_*` doors in `engine_funcs.h` | other-shape files | blind spot? |
|---|---|---|---|
| `Thread` | **none** | 15 files (`thread_win`, `audio_stream_win`, `keystone_a_gamedtor`, `rt_data_mint`, `loops`, `singleton_ctors`, `Persistence.h`, `pathfinder_seed`, `action_pf_astar`, `reg_mint_promoted`, `loader_loop_stubs`, `Thread.h`, `persistence_worker_win`, `vtbatch7b_stream_run_win`, `keystone_a_zone`) | **YES, total** |
| `CriticalSection` | **none** | 6 files | **YES, total** |
| `Random` | 5 (`fn_Random_Rand`, `_RandFloat`, `_RandFloatTol`, `_RandTol`, `_Default_Ctor`) | 38 files | **YES** |
| `Time` | 3 (`fn_Time_Ctor_Float`, `fn_Time_AsSeconds`, `g_fn_Time_Serialize`) | **68 files** | **YES, badly** |
| `Color` | 1 (`fn_System_Color_Serialize`) | 5 files | no |
| `DateTime` | 1 (`fn_DateTime_AsString`) | 6 files | minor |
| `Timer` | **none** | 17+ files, all `RT_EXPORT_ONCE`/`ResolveExport` inside `runtime/src/` | **YES, total** |
| `CRC` | **none** | 2 files (`crc.cpp`, `persistence_saveinfo_job.cpp`) | n/a |
| `Error` | **none** | 11 files incl. `dx11/src/{contextdata,devicedata}.cpp` | **YES, total** |
| `WideString` | **none** | 12 files, 9 of them `runtime/src/io/` | **YES, total** |
| `Compression` | **none** | 4 files, all `runtime/src/io/` + `save/saveload_meta.cpp` | **YES, total** |
| `Configuration` | 3 (`_CurrentVersion`, `_CurrentBuildId`, `_GetVersionString`) | 13 files | **YES** |
| `MemoryAllocator` | 4 (`fn_MemAlloc`, `fn_MemAllocStatic`, `fn_MemFree`, `fn_MemFreeStatic`, + dead `fn_MemZeroFill`) | 12 private-cache files + `rtcoll::DoorAlloc`'s 9 | **YES** (§6) |

**CORRECTION to `SYSTEM_INVENTORY.md` §5.6 and §5.5.** The inventory scopes `Timer`'s caller
surface to five exports across `runtime/src/engine|graphics|system` and `Compression`'s to four
call sites. The mangled-name sweep returns **17+ files for `Timer@System@@`** (adding
`runtime/src/ui/` ×6, `runtime/src/input/`, `runtime/src/audio/`, `standalone/reg_mint_promoted.cpp`
×5) and shows `Compression@System@@` reaching `runtime/src/save/saveload_meta.cpp` as well. And
`Time@System@@` touches **68 files** against the inventory's implied ~20. Neither number
authorises anything — they are token counts — but both say the inventory under-scopes, never
over-scopes, which is the dangerous direction.

**Receiver first, callers after.** Every class in 4b converts its receiver in its own commit;
caller batches are a separate, later, directory-partitioned pass, exactly as B34_RULES §3.2
ordered IO. The one class where the receiver conversion alone rewrites callers is
`Thread`/`CriticalSection`, because five `code/Game` files (`action_pf_astar`,
`keystone_a_zone`, `keystone_a_gamedtor`, `pathfinder_seed`, and `tornado.cpp` — which the
inventory correctly deletes from the list, verified: **zero** `Thread@System@@` /
`CriticalSection@System@@` hits in `tornado.cpp` today) reach it only through shape (2). Those
files are `code/Game/**` and are **`b34-closing`'s until it lands**.

---

## 6. `System::MemoryAllocator` — assessment, and the recommendation is DEFER

### 6.1 The three caller surfaces, re-derived

Confirmed against `engine/funcs/engine_funcs.cpp` and `runtime/src/system/allocator.cpp`:

| surface | resolution | retail body | locking |
|---|---|---|---|
| `fn_MemAlloc` (member) | `ResolveAddr(Module::Runtime, 0x1800c6e60)`, `engine_funcs.cpp:1370` | `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z` | **none** |
| `fn_MemAllocStatic` | `ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")` | 0x1800c7a50 = `rtalloc::MemAlloc` | `Get(); lockEnter; Allocate; lockLeave` |
| `s_fn_MemAllocSA`-family | 12 files re-resolve the SAME static export, or the exe dispatch slot 0x140152b60 | same body | same |

plus the free side: `fn_MemFreeStatic` (616 CALL) → 0x1800c7ad0 `rtalloc::MemFree`, and
`fn_MemFree` — a lambda thunk at `engine_funcs.cpp:1466-1468` that **discards its singleton
argument** and forwards to `fn_MemFreeStatic`, so the true member `?Free@…@QEAAXPEAX@Z`
(0x1800c6c00) has **no live caller**. ~1,146 CALL sites total.

**The asymmetry is the finding and it must survive any class-ification**: `Allocate` is
lock-free, `MemAlloc` is locked, and they are different retail bodies. One `Allocate` method is
a silent lock change on one side or the other.

Additionally, `rtcoll::DoorAlloc`/`DoorFree` (`runtime/src/collection/alloc_door.cpp`) **re-read
exe dispatch slots 0x140152b60/0x140152b68 on every call** so that
`RedzoneAlloc_Install()` can rewrite them after `EngineFuncsInit()`. A real ctor that moves
when `Construct()` runs relative to that install either freezes a pre-shim pointer or leaves
`_singleton` null when redzone/alloc_stats first read it.

### 6.2 The singleton cell

`?_singleton@MemoryAllocator@System@@0PEAV12@EA` @ 0x1801fc718 is **our minted cell** —
`rt_data_mint_tables.inc:4715` (`&kStorageBytes[14016]`, 8 bytes, "singleton pointer") and
`cutover_manifest.inc:79` `CUTOVER_REQUIRED`. It is written by `rtalloc::Construct`
(`allocator.cpp:652`) and **nulled by `rtalloc::Destruct`** (`allocator.cpp:658-661`), both through
`SingletonSlot()` → `RT_DATA_SLOT`. `engine_funcs.cpp:1369` and `standalone/main.cpp:720`
dereference it once each at init.

### 6.3 OPEN DEFECT — the allocator `LockEnter` crash, and a mechanism from the code

> **UPDATE 2026-09-19 (defect pass, `a9873f47`; account in `../STATUS.md`): W2′ below is NOT
> live** — every door into the free list takes the allocator lock, as retail's inlined
> Enter/Allocate/Leave; the "unlocked Allocate/Free" premise came from a stale comment in
> `standalone/loader_loop_stubs.cpp`, since rewritten. W2 fits the stack; retail's only loader
> join ahead of `~Engine` is `LoaderLoop::Shutdown` (0x1800dc280), which our stubs now perform.
> Still not reproduced — a recurrence is a finding. The text below is kept as written.
>
> **STATUS (as written): OPEN. Not fixed, not reproduced on demand, not to be null-checked.**
> Recorded here verbatim at the coordinator's request (2026-09-19) with its file:line anchors,
> because it is the one piece of 4b evidence that outlives 4b. Owner: the loader defect
> (B34_BRIEFS §D / `[[project_menu_heap_crash]]`), **not** this step.
> The diagnostic that would close it — printing `_singleton`'s value in the crash dump — is a
> `standalone/` change and is scheduled by the coordinator after `b34-closing` lands.

Reported: `RtObject_External_Load -> GraphicsMesh_Serialize -> String::Serialize -> SetLength
-> rtalloc::MemAlloc -> LockEnter on a NULL lock`, 2 in 7 resave runs under heavy machine load,
never since.

**The unguarded deref is real and it is faithful.** `allocator.cpp:677-683`:

```cpp
void* MemAlloc(uint64_t size, int align) {
    MemoryAllocator* self = Get();          // may be nullptr
    g_plat.lockEnter(self->lock);           // <-- no test
    ...
}
```

`lock` is `uint8_t lock[0x28]` at **+0x640** (`MemoryAllocator.h:240`), so with `self ==
nullptr` this is `EnterCriticalSection((LPCRITICAL_SECTION)0x640)` — a **near-null** fault,
which is exactly what a crash log summarises as "LockEnter on a NULL lock". Retail's body has
the same unguarded deref; per CLAUDE.md this is load-bearing and **must not be guarded** — the
question is only who handed us the null.

`Get()` returns null in exactly two windows, and both are real:

- **W1 — before `Construct`.** `SingletonSlot()` is `RT_DATA_SLOT`, which returns null until
  `RtDataMintInit()` has built the cell. Poor fit for this stack: `External::Load` runs far
  past boot.
- **W2 — after `Destruct`.** `RtEngine_Engine_Dtor` (`engine_lifecycle.cpp:445`) calls
  `??1MemoryAllocator@System@@QEAA@XZ` → `Dtor_MemoryAllocator` (`singleton_ctors.cpp:243`) →
  `rtalloc::Destruct`, which **nulls the cell and then `lockDelete`s the critical section**.
  From that instruction on, any thread still allocating gets `self == nullptr`. **Good fit**:
  the reported stack is the loader path, and `standalone/loader_loop_stubs.cpp:60-96` documents
  in its own comment that `Loader::Run -> LoadResources -> recursive External::Load` runs on a
  separate thread and that the main thread is NOT parked unless `SyncEngineLoader()` is on.
- **W2' — the same race, one indirection out.** That same comment records that
  `MemoryAllocator::Allocate` and the internal `Free` mutate the boundary-tag free list
  **without a shared lock** (only `MemFree` wraps `Free` with the CS) — `[[project_menu_heap_crash]]`.
  A corrupted free list returns a bogus block, and a write through it is a wild write; the
  minted cell at `&kStorageBytes[14016]` is ordinary `.bss` and is as reachable as anything
  else. That would zero `_singleton` **without** `Destruct` ever running, and the next
  `MemAlloc` on any thread faults in `LockEnter` with `self == nullptr`.

**Verdict.** The crash is allocator-**lifetime**-shaped in the W2 sense and allocator-**race**-
shaped in the W2' sense, and the two are not distinguishable from the reported stack alone; W1
is effectively excluded by the call site. Both W2 and W2' are consequences of the loader thread
allocating concurrently with a main thread that may tear down or corrupt the arena, which is
the defect `B34_BRIEFS` §D and `[[project_menu_heap_crash]]` already name. "Never reproduced
since" is consistent with `d704c44a` (single loader: the standalone `AsyncLoader` removed,
`Engine::Loader` the only dispatcher) having narrowed the interleaving, **not** with the defect
being fixed. **Recommend it be tracked against the loader defect, not against 4b**, and that
the next occurrence be captured with `_singleton`'s value printed in the crash dump — that one
byte separates W2 (null, written by `Destruct`) from W2' (null or garbage, written by nobody).
It should **not** be "fixed" with a `if (!self) return nullptr;`.

### 6.4 Recommendation

**DEFER `System::MemoryAllocator` past 4b**, as its own step with its own coordinator. The
evidence, in order of weight:

1. **The open crash above is unexplained in the sense that matters** — we have two candidate
   windows and no measurement separating them. Converting the class means touching `Construct`,
   `Destruct` and `SingletonSlot()`, i.e. the exact code both windows run through. A conversion
   that made the crash stop would be indistinguishable from a conversion that made it rarer,
   and a conversion that made it worse would surface as a resave DIFF with no attribution.
2. **The member/static locking asymmetry (§6.1) is a behaviour question, not a refactor** — and
   so is wiring a real non-static `Free` to 0x1800c6c00, an address with no live caller today.
3. **~1,146 CALL sites across three surfaces plus `alloc_door.cpp`'s slot-rereading design**,
   which B.3.2 settled deliberately and which a ctor-ordering change breaks silently.
4. It has **zero dependency** on anything else in 4b, so deferring costs nothing and buys clean
   attribution — the same argument B34_RULES §6.0 used to split 4a from 4b.

This matches B34_RULES §6.5(c) and `SYSTEM_INVENTORY.md` §7 batch S7; nothing found this pass
argues the other way. **The decision is the coordinator's.**

---

## 7. Order of work for 4b, and what each commit may touch

Receiver-side only until `b34-closing` reports the 4a closing pass merged. Until then
`code/Game/**`, `engine/**`, `standalone/**` and `runtime/src/io/**` are **off limits**.

| # | commit | files |
|---|---|---|
| 1 | this sheet | `docs/runtime_port/endgame_b/B34B_RULES.md` |
| 2 | `System::CriticalSection` + `System::Thread` | `runtime/types/system/Thread.h`, `runtime/src/system/thread_win.cpp`, `runtime/types/engine/UpdateLoop.h`, `runtime/src/engine/loops.cpp` (one word, §7.1), `tests/unit/system_abi_test.cpp` + `system_abi_stubs.{cpp,h}` |
| 3 | `System::Random`, `Color`, `Time`, `DateTime` | `runtime/types/system/Value.h`, `runtime/src/system/value_types.cpp`, `tests/unit/system_abi_test.cpp` (PART E) |

### 7.0 `System::Time` — resolved, and it cost nothing outside `runtime/`

The coordinator's condition at commit 2 was: declare the class in
`runtime/types/system/` with a cross-assert against the shared POD **and zero edits outside
`runtime/`**, or leave `Time` for after the closing pass. **The condition is met**, so `Time`
landed with commit 3. The facts that decide it:

- **Who owns the POD.** `struct Time { int64_t ticks; }` is `engine/types/common.h:12-15`,
  with its own `sizeof == 8` assert. `runtime/types/math/Math.h` includes that header, so it
  reaches most of the tree. B.3.3 step 6B deliberately de-duplicated it there (it used to be
  declared in `Value.h` as well, and any TU including both failed with `redefinition of
  'struct Time'`). **It is not moved and not touched.**
- **The layout.** One `int64_t` at +0x00, pinned by `??0Time@System@@QEAA@M@Z` (0x1800078c0),
  which writes one qword. `System::Time` is a NEW name in a NEW namespace over the same eight
  bytes, cross-asserted `sizeof` and `offsetof` against `::Time`.
- **Which spelling the callers use.** All three doors are `void*` in `engine_funcs.h`:
  `fn_Time_Ctor_Float` (:1827), `fn_Time_AsSeconds` (:1083), `g_fn_Time_Serialize` (:5475). A
  whole-tree sweep for the C++ spelling `System::Time` returns **comments only** under
  `code/Game`. So no caller has a type dependency on either spelling.

Net: the class is declared in `Value.h`, the three bodies moved onto members in
`value_types.cpp`, and **no file outside `runtime/` changed**. The 132 `fn_Time_Ctor_Float`
caller sites and the int64 `Serialize` overload are still gated behind the 4a closing pass.
| 4 | `Timer` / `CRC` / `Error` — the ALL-STATIC trio, verdicts in §1.2 | `runtime/types/system/{Timer,Crc,Error}.h` (new), `runtime/src/system/{timer,crc,error}.cpp`, `tests/unit/{system_abi_test.cpp,sources.mk}` |
| 5 | `Configuration` / `Compression` / `WideString` — the three with instance state, verdicts in §1.2 | `runtime/src/system/{configuration,compression,widestring}.{cpp,h}`, `runtime/types/system/` as needed |
| — | `System::MemoryAllocator` | **not converted**; §6 is the deliverable |
| — | `IO::Serializer::Serialize(int64_t&)` + the `Time::Serialize` site | **after `b34-closing`**; §3 |

### 7.1 The forward-declaration tag, found at commit 2

`runtime/src/engine/loops.cpp:123` forward-declares the door's parameter as
`namespace System { struct Thread; }` and never includes `Thread.h`. Once `System::Thread` is a
**class**, that is a struct/class tag mismatch — which **GCC accepts silently**, which is
exactly why it is corrected rather than left working. The fix is one word and it is part of
commit 2's file set; the door's signature and the cast at `:724` are unchanged, because the
class keeps the POD's layout.

> **RULE B4b-5.** A class conversion sweeps forward declarations as well as definitions. A
> `struct X;` that now names a `class X` is not a build error on this toolchain and will not
> announce itself — grep `namespace <NS> {` for a bare tag of every converted name before
> committing, and do not rely on the compiler for this one.

### 7.2 Per-commit self-check

`B34_BRIEFS.md` §F minus the game-dir lock and the resave gate: the
coordinator gates each reported hash on the merged tree. What the agent runs itself:
`git merge main` · `standalone-debug` · `standalone-perf` (the real NDEBUG build; `standalone`
shares an obj dir with `standalone-debug`) · `unit` · zero new warnings in changed files ·
`python tools/dispatch_slot_map.py --check` at 0 · the cutover manifest gate · `rt_mint_gen
--check` UP TO DATE (and `git checkout --` `standalone/rt_export_addrs.inc` afterwards, §A.6) ·
`tools/runtime_census.py` numbers quoted in the commit message.
