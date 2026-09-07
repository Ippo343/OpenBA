# Wave 4 — IO

Landed 2026-09-06 on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2.

Wave 3 was the first wave whose classes own state the DLL also drives. Wave 4 is the first whose
functions **move a cursor through a file the game must later read back**, so a wrong port here does
not crash — it desyncs a save and surfaces later on an innocent-looking record. It is also the
first wave that had to fix its own test harness before it could measure anything: PLAN §5 assigned
it the `BANISHED_AUTOLOAD` stall, and every one of the wave's save/load ports lives on that path.

## Result

| | |
|---|---|
| Ported | **39 of the 41 IO functions our tree calls** — Serializer 22, CompressedMemoryStream 6, Persistence 5, FileSystem/Modification 6 |
| Census | `IO 36/41 (87.8%)` — and the census itself was undercounting before this wave, see finding 6 |
| Diff oracle | **83 sites, 21,672,859 calls, 0 IO divergences**; 3,515,498 of those calls are the IO ports |
| Residual | 44 divergences in 2 sites — the same Wave 1 sin/cos-fed 1-ULP sites, unchanged in kind |
| Held back | `ReadSettings` / `WriteSettings` (mutate the real HKCU registry, no safe oracle), `FileSystem::_singleton` (a DATA export), 6 Serializer forms with zero call sites |
| Perf gate | **BREACHED at +7.7%, and DEFERRED BY THE USER to after the final stage** — see below |
| Harness | `BANISHED_AUTOLOAD` **fixed** — `Save9` and `Auto` load to gameplay unattended for the first time |

Lanes: A Serializer, B CompressedMemoryStream, C Persistence, D FileSystem + Modification,
H the harness. Detail per lane in
[WAVE4_LANE_A.md](WAVE4_LANE_A.md) · [WAVE4_LANE_B.md](WAVE4_LANE_B.md) ·
[WAVE4_LANE_C.md](WAVE4_LANE_C.md) · [WAVE4_LANE_D.md](WAVE4_LANE_D.md) ·
[WAVE4_LANE_H.md](WAVE4_LANE_H.md).

## Environment variables added

| Variable | Effect |
|---|---|
| `BANISHED_RT_IO=0` | Leave the whole IO namespace on the DLL |
| `BANISHED_RT_SERIALIZER=0` / `_STREAM=0` / `_PERSISTENCE=0` / `_FILESYSTEM=0` | Per-lane kill switches |

---

## Performance — measured, breached, and deliberately not chased

`sim ms/iteration` came in at **+7.7%** against `perf/baselines/current.csv`, on a machine
verified quiet. The 5% gate is breached and the baseline was NOT touched.

Three earlier runs read +8.4%, +7.0% and +9.8%, and were discarded: another session was building
and running the game on the same box, so they measured contention. Worth recording that discarding
them did **not** rescue the result — the clean run landed in the same range, so contention was not
the explanation. Wave 3 closed at +3.3-4.4%, so Wave 4 appears to add roughly 3-4 points.

**The user deferred the gate at this boundary** — *"dont really care if its slower we can fix that
after the final stage"* — so the wave is not held open on it and PLAN §5 now records the gate as
moved to the cutover stage rather than failed-and-ignored.

**The `BANISHED_RT_PORT=0` A/B was not run.** It was the next step when the deferral arrived.
Anyone picking performance up should run it FIRST: three runs, free, and the only thing that
separates "the IO ports cost 3-4 points" from "drift". The plausible suspect is the `Serialize`
forwarder family, which is genuinely hot — `Serialize(uchar&)` alone took 764,734 calls in a
single load — and now goes through our seam. That is a hypothesis, not a finding: Wave 2 asserted
exactly this shape about `Random::Rand` and was wrong, and the real cause there was one
`GetProcAddress` in a hot body. Measure before believing it.

---

## The findings that change other waves

### 1. The `BANISHED_AUTOLOAD` stall was never a port, and never the save

PLAN §5 recorded it as "pre-existing and not save-specific" and left four Wave 3 functions landed
but unexercised. It is `UI::UserInterface::ShowDialog` (`0x18011fd10`) being a **nested modal
loop** — `do { … MainLoop::Frame(singleton, true) } while (!done)`. Every save on this machine
records the `"Debug Menu"` mod and no `.pkm` is installed, so `SaveLoad::CheckMod` correctly
returns 1 and retail correctly raises `modsMissing`. Nothing could answer it, because
`AutoStart::TickMenu` runs *inside* that nested loop with its phase already `Done`. The symptom —
"loops in `MenuDLT::Draw` forever with no world created" — was literally true and completely
misleading.

Two more defects sat behind it, both ours:

* **The long-standing "load-path wild write, +0x7FC in the high dword" is ONE LINE.**
  `saveloaddialog.cpp` called `fn_String_Dtor` on an `InternalBasePtr` holding a `MemAlloc` user
  pointer, so `*(int*)(data-4) -= 1` decremented the **allocator boundary tag's high dword**.
  `0x1FF << 32`, times 4, is exactly the documented 0x7FC; two observed corrupt tags were
  `0x2000-1` and `0x2200-1` — a minus-one, not a scribble. It reproduced on `Save6.sav` (2.8 MB),
  so the "large hand-built town" framing in that note was wrong, and its ".pkm made one load
  succeed, treat that as heap-layout luck" line was the real mechanism being reasoned away.
* **The Load path never called `GameContainer_EnsureLists`**, so a loaded game reached
  `GameUpdateLoop::Update` with `AchievementManager+0x18` null. Retail does it once at app init
  (`GameContainer_Init` `0x140099db0`); standalone replaced that with per-dialog calls in New Game,
  Scenario and Achievements and missed Load. A missing init, not a Persist defect — which is what
  the source comment had asserted, in capitals, for a day.

### 2. Three facts this project had RECORDED about `IO::Serializer` were wrong

All three were handed to lane A as "re-verify but do not re-derive". The lane re-derived them and
was right to; each was then checked a third time by the orchestrator against the shipped bytes at
`0x1800b9960`.

| Recorded | Actually |
|---|---|
| the ctor's `int version` is IGNORED | **It is not.** The body never *touches* R8D, which is exactly why it is still live as argument 3 at the `CALL`. `BinaryWriter`'s ctor stores it at `+0x28` and **writes it to the stream** — it is the save-format version stamped into the file. "Not stored by this function" had been read as "discarded". |
| the factory-slot ARITY is not pinned; Ghidra is demonstrably wrong there | Ghidra is still wrong; the **instruction bytes are not**. `MOV RDX,RBX` at `0x1800b997f` sets arg 2 = `this` *before* the direction branch, so it is common to both paths, and R8D carries arg 3 through untouched. Arity is **3**: `(encoding, serializer, version)`. Wave 3's `ResetPersistState` carries a stated non-byte-faithful deviation resting on the old uncertainty and **can now be revisited**. |
| `Serializer+0x08` is an `Encoding*` | It is an **`IO::ReaderWriter*`**. `IO::Encoding` is a separate class at `stream+0x10`: the policy object whose slots *create* ReaderWriters. `engine/types/io_serializer.h` conflated them under one name; corrected, field renamed `m_readerWriter`. |

**The general lesson, and the reason this table is kept rather than edited away:** "re-verify but
do not re-derive" is bad instruction, and the orchestrator is the one who gave it. A fact recorded
in a memory note is a claim, not evidence.

### 3. The answer to the profile-build-offset trap is a runtime self-check, not care

The standing warning is that `IO::Encoding`'s vtable offsets DIFFER between retail and the profile
DLL (bool `+0x78`/`+0x90`, uint `+0x98`/`+0xb0`, int `+0xa0`/`+0xb8`), and that a body transcribed
from a profile decompile writes the right values through the wrong slots — which does not crash,
it desyncs the stream, and the damage surfaces later.

Lane A did better than "be careful". Every `Serializer` forwarder in the DLL is three instructions
whose `JMP [RAX+disp]` **states its own vtable slot in its own bytes**. `RtIoSerializerRegister()`
decodes all 18 displacements out of the shipped DLL and compares them against the port's
compiled-in constants, and byte-compares the three non-tail-jump bodies. **Any mismatch and nothing
registers — the whole family stays on the DLL.** It runs on every run, not only under
`BANISHED_RT_DIFF`, because a wrong slot is silent. As a side effect it would also have caught
export folding, had any existed.

Generalise this: where a port's correctness rests on a constant read out of a binary, the
registration can usually re-read that constant from the binary and refuse.

### 4. An oracle that cries wolf costs more than no oracle

The integration run reported two divergences on ports all four lanes had called clean. Both were
defects in the **oracles**, not the ports — but proving that took real work, and two attempted
fixes were worse than the bug.

* **`IO::Serializer::Serializer`** — 2,053 divergences in 150,574 calls, all writing-side. The
  shadow expected `BinaryReader`/`BinaryWriter`; the observed vtable, resolved through the run's
  ASLR delta, is `0x1801558b0` = **`??_7Visitor@IO@@6B@`** — the resource-reference VISIT pass,
  i.e. WAVE3.md finding 5's `VisitorStream`, with `[PreloadRefs] preloaded 5687/5688 referenced
  externals` in the same log. `Encoding` is a policy object and the port stores whatever its
  factory slot returns, so the port was right and the oracle's enumeration was too narrow.
  **Lane A had written this exact caveat into the code before it happened.**
* **`IO::CompressedMemoryStream::CompressedMemoryStream`** — compared **uninitialised padding**.
  `theirs` is a memset-0 scratch; `self` is the caller's uninitialised stack
  (`gameupdateloop_serialize.cpp`'s `alignas(16) uint8_t cms[0x100]`), so every byte neither ctor
  writes differs. Two wrong fixes, both now documented at the code so they are not retried:
  **(a)** excluding gaps one at a time as the run reported them fixed `+0x0c` and the very next run
  failed identically at `+0x18`; **(b)** deriving the mask at runtime by also constructing into a
  `0xff`-filled buffer looked elegant and **crashed the game** (`ACCESS_VIOLATION` in
  `MemoryAllocator::Free`, `Runtime+0xC74D6` via `+0xC6CC9`) — the ctors and dtor act on fields
  they never initialise, so `0xff` bytes hand them garbage pointers to free.
  The landed fix compares only the bytes the ctors actually write, read off **both** constructors'
  shipped bytes in full (`??0CompressedStream` `0x1800aa3d0`, `??0MemoryStream` `0x1800ae710`):
  gaps `0x0c-0x0f`, `0x18-0x1b`, `0x24-0x27`, `0x74-0x77`, `0x89-0x8b`, `0x94-0x97`.

### 5. `std::printf` does not survive the perf build, and no lane could have caught it

`standalone/log_strip.h` does `#define printf(...) ((void)0)`, which cannot rewrite a *qualified*
`std::printf` — it is a hard compile error, in `standalone-perf` only. Lanes were syntax-check-only
against the debug configuration, so this was invisible to every one of them and surfaced only at
the perf gate. **Use bare `printf` in `runtime/src/`.**

Worse, the failure nearly went unnoticed: `make … | grep …` takes **grep's** exit status, so the
failed build still ran the following `cp` and deployed a **stale** exe. That is the recorded
"a FAILED build measures the OLD exe" trap, hit again, by the orchestrator, in the wave that
quotes it.

### 6. The census could not see 12 live ports, six of them in a LANDED wave

`tools/runtime_census.py` matches `RegisterRuntimeExport("<literal>"` and **cannot see through a
named constant**. Twelve ports registered through `kName*` constants were live in the process and
absent from every number the census printed — including six in Wave 3's `type_system.cpp`, whose
headline `Object 21/41 (51.2%)` was really **27/41 (65.9%)**.

This is the mirror image of the failure this project already knows about: there a port is counted
but never runs; here a port runs but is never counted. Both end in a census that does not describe
reality. The census now prints a loud warning naming every file and identifier that registers under
a non-literal name, verified by probe (revert one registration to a constant and it fires). Wave
3's six were also registered by address, which is the only reason the loss was 6 and not worse —
luck, not design.

### 7. A zombie `reimpl-x64.exe` is clearable from a shell after all

`taskkill /F /PID` and PowerShell `Stop-Process -Force` both refuse the post-crash zombie
("There is no running instance of the task") while `tasklist` still lists it holding the exe and
the logs. `wmic process where "ProcessId=<pid>" delete` clears it and releases the locks
immediately. The old note said only Task Manager works; it does not, and the difference matters for
unattended runs — this cost two silent no-op stress runs before it was noticed.

---

## What landed

| Piece | Files |
|---|---|
| Wave entry point | `runtime/src/io/rt_io_port.cpp` — one `RtIoPortInit()`, four per-lane `Register` functions, written BEFORE the lanes started so no two ever edited the same file |
| Serializer (lane A) | `runtime/types/io/Serializer.h`, `runtime/src/io/serializer.{cpp,h}` |
| CompressedMemoryStream (lane B) | `runtime/types/io/Stream.h`, `runtime/src/io/compressedstream.cpp` |
| Persistence (lane C) | `runtime/types/io/Persistence.h`, `runtime/src/io/persistence.cpp` |
| FileSystem + Modification (lane D) | `runtime/types/io/{FileSystem,Modification}.h`, `runtime/src/io/filesystem.cpp`, `runtime/src/io/filesystem_win.cpp` |
| Harness (lane H) | `code/Game/Dialog/saveloaddialog.cpp`, `code/Game/Dialog/mainmenudialog.cpp` |
| Export-name sweep | `docs/runtime_port/wave4_export_names.txt` — all 39 verified against the shipped DLL, 0 phantoms, plus the no-ICF-folding result |
| Census hardening | `tools/runtime_census.py` warns on non-literal registration names |

`stream.cpp` was renamed `compressedstream.cpp` mid-wave: the recovered tree has BOTH
`IO\Stream.cpp` (the `IO::Stream` base) and `IO\CompressedStream.cpp`, so the scaffolding's name
would have collided with the base class's file when that is ported.

## Comments corrected at their source (PLAN §6 step 8)

| Where | Was |
|---|---|
| `engine_funcs.h:1242` | `GetSavePath` documented as `QEAA?AVFilename` — wrong on three counts; the real export is `QEBA?AVString` and the name given does not exist in the DLL |
| `engine_funcs.h:1249` | `GetDataDirectory` documented as `QEBAAEBVString`, a reference-returning String overload that does not exist; the sole export returns `Filename` by value |
| `gameupdateloop_serialize.cpp:1105` | "this is a different stream class" — factually wrong; a `CompressedMemoryStream` *contains* a `MemoryStream` at `+0x68` |
| `gameupdateloop_serialize.cpp:1235` | vague `sizeof` comment, replaced with the measured `0x98` and how it was measured |
| `gameupdateloop_serialize.cpp:727` | "no `IO::Stream::Tell` export" — true but sent the reader away empty-handed; `GetCurrentPos` is vtable slot 5 on every concrete Stream |
| `gameupdateloop.cpp:1450` | the null achievement instance list called "a load-path defect" needing a Persist fix; it was a missing `GameContainer_EnsureLists` call |
| `io_serializer.h` | `Serializer+0x08` typed and named as an `Encoding*`; it is a `ReaderWriter*` |
| `PLAN.md` §4.2 Wave 4 row | promised `PackageFile` and `BinaryReader/Writer`, which our tree never calls and this wave does not touch |
| `PLAN.md` §5 Wave 3 row | carried the undercounted `Object 21/41` |
| `WAVE3.md` finding E | marked closed by lane H |

## Still open, deliberately

* **`ReadSettings` / `WriteSettings`** stay on the DLL. Both mutate the real `HKCU` registry and
  `WriteSettings` also deletes keys, so no safe oracle exists and a mistake corrupts the player's
  settings rather than crashing a debug exe.
* **An INTERMITTENT crash on the load path**, narrowed but not closed. Lane H reported it at
  `compressedstream.cpp:375` (the `Commit` call inside `Close`) and the orchestrator first
  mis-attributed it to an oracle experiment that did not exist in the build that crashed.
  Symbolising the two Runtime frames places it elsewhere, and more usefully:
  `Runtime+0x6A57` is inside `FUN_180006a10`, the locked `MemoryAllocator::Free` wrapper, and
  `Runtime+0xA95E0` is the instruction immediately after `CALL 0x180006a10` at `0x1800a95db` —
  which is inside `??1CompressedMemoryStream` and frees `[RDI+0x10]`, i.e. **the embedded
  `MemoryStream`'s `Stream::_encoding`**. So the fault is a free of an already-freed or corrupted
  `_encoding` in the `MemoryStream` sub-object, raised by the DLL's own destructor. The
  destructor is the messenger, not the writer.
  What has been RULED OUT statically: retail's `~CompressedMemoryStream` (`0x1800a9560`) inlines
  the same `Commit` guard the port uses, on the same offsets, so the port's dtor is faithful
  there; `Commit` (`0x1800a9ee0`) does end in `Release`, so a double `Commit` WOULD produce this
  shape, but retail is equally exposed to it and nothing yet shows our port calling `Close`
  twice. Not yet run, and decisive: the same load with `BANISHED_RT_STREAM=0`, which leaves
  `CompressedMemoryStream` on the DLL. If it still crashes the port is not the cause.

* **Quit-to-menu teardown after loading `Auto.sav`** faults in the allocator via
  `EntityList_Shutdown` → `WorkComponent_OnRemove` → `HuntWork` scalar dtor → `MemFree`.
  `Save9`'s quit is clean, so it is content-dependent. Same family as the known 800x10 stress
  teardown fault, but now reachable from a save load in ~3 minutes instead of a stress town, which
  makes it far cheaper to chase.
* **The `Save9` mod prompt is answered, not resolved.** The saves reference a `"Debug Menu"` mod
  that is not installed; the harness now answers the modal instead of hanging. Restoring the two
  `.pkm` to `WinData\` would make `CheckMod` return 0 — but that silently disables all achievement
  evaluation (`AreUserPackFilesEnabled()` is `count > 0`), so the two harnesses want opposite
  states of that directory and cannot both be satisfied by this save set.
