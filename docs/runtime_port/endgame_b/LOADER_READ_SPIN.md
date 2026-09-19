# The loader read race — NULL resources, the `CompressedStream::Read` spin, and the front-end crashes

Investigation, 2026-09-19. Branch `b34-loader-investigation`. **Diagnosis only — nothing on
this path was changed. Every code edit made for this investigation is a temporary instrument
or a throwaway experiment in the investigation worktree, committed separately as
`TEMP INSTRUMENTS — never merge` and then reverted; `git diff --stat main` for this branch
shows only this file.**

---

## 0. One-paragraph answer

All of Banished's data lives in four package files (`WinData/data0.pkg`, `data1.pkg`,
`patch0.pkg`, `patch1.pkg`). Every resource byte is therefore read through
`IO::FileSystem::ReadFile`'s **packed** arm, which is a `SetFilePointer` followed by a
`::ReadFile` on the package's **one** Win32 handle — i.e. on **one kernel file pointer**,
with no lock. Our standalone runs **two** resource-loading threads concurrently (our
`AsyncLoader` worker and the Runtime `Engine::Loader` thread; the `[GUL::Load] redundant
dispatch` guard only stops the second *world build*, not the second thread's loads). When
the two interleave inside that seek/read pair, one thread reads bytes from the other's
offset. The torn bytes land wherever the reader happened to be, which is why the casualty
varies: a torn block directory fails `CompressedStream::Open`'s `total == len` check
(`openCompFail` → the resource loads as NULL); a torn data block makes the compiled-resource
magic wrong (`badMagic`); a torn `String` length inside a nested `Serialize` asks
`CompressedStream::Read` for bytes past the end of the block, where `chunk = w->end - pos`
is zero and the `while (n > 0)` loop can never advance — the spin. Measured: **11 of 24**
control boots defective, **0 of 12** with the seek+read made atomic (a positioned read, no
lock, same syscall count, both loader threads still demonstrably running).

---

## 1. Evidence table

Route for every boot: `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`,
debug exe, game dir held under `tools/game_dir_lock.sh acquire loader-spin`.
Columns: `oF/oC/bM/rN` = `openFileFail / openCompFail / badMagic / retNull` from the
`[ExtLoad]` line. `ovlXL` / `ovlPk` = concurrent-entry events measured by the instruments
(§4). Raw logs under
`scratchpad/loader-spin/runs/{inst,inst2,ctrl,ctrl2,pread}/run*/`.

### 1a. CONTROL — unmodified code, 24 boots (md5 `ec7de6bd`, `9785c446`)

| batch/run | exit | topics | diseases | oF | oC | bM | rN | calls | verdict |
|---|---|---|---|---|---|---|---|---|---|
| ctrl/1  | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5686 | clean |
| ctrl/2  | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5743 | clean |
| ctrl/3  | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5686 | clean |
| ctrl/4  | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5686 | clean |
| ctrl/5  | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5686 | clean |
| ctrl/6  | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5746 | clean |
| **ctrl/7**  | 0 | **-1** | 12 | 3 | **1** | 0 | 4 | 5687 | **NULL HelpTopics** |
| ctrl/8  | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5686 | clean |
| **ctrl/9**  | 0 | 66 | 12 | 3 | **8** | **3** | 14 | 5847 | **8 NULL resources, 3 bad magic** |
| **ctrl/10** | **4** | — | — | — | — | — | — | — | **HANG — loader spinning in `CompressedStream::Read`** |
| **ctrl/11** | **3** | — | — | — | — | — | — | — | **CRASH — NULL deref in `RtUiDialogInstance_Ctor`** |
| **ctrl/12** | 0 | 66 | 12 | 3 | **1** | **1** | 5 | 5730 | **1 NULL resource, 1 bad magic** |
| ctrl2/1 | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5686 | clean |
| ctrl2/2 | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5740 | clean |
| **ctrl2/3** | 0 | **-1** | 12 | 3 | **1** | **1** | 5 | 5687 | **NULL HelpTopics** |
| ctrl2/4 | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5739 | clean |
| **ctrl2/5** | **3** | — | — | — | — | — | — | — | **CRASH — `NewGameDialog_Ctor_Standalone`** |
| **ctrl2/6** | **3** | — | — | — | — | — | — | — | **CRASH — `NewGameDialog_StartGame`** |
| **ctrl2/7** | **3** | — | — | — | — | — | — | — | **CRASH — `PopulateGeneratorCombo`** |
| **ctrl2/8** | **3** | — | — | — | — | — | — | — | **CRASH — `NewGameDialog_Ctor_Standalone`** |
| **ctrl2/9** | 0 | 66 | 12 | 3 | **10** | **3** | 16 | 5878 | **10 NULL resources, 3 bad magic** |
| ctrl2/10–12 | 0 | 66 | 12 | 3 | 0 | 0 | 3 | 5686 | clean ×3 |

**Control failure rate: 11 / 24 (46%)** — consistent with the coordinator's cross-build
tally of ~40%.

### 1b. INSTRUMENTED — the detectors of §4 compiled in, 12 boots (md5 `4e57399f`)

| run | exit | topics | oC | rN | calls | ovlXL | ovlPk | packed reads | tidA / tidB |
|---|---|---|---|---|---|---|---|---|---|
| inst/1 | 0 | 66 | 0 | 3 | 5686 | 0 | 0 | 20892 | 33084 / 28088 |
| inst/2 | 0 | 66 | 0 | 3 | 5692 | **6** | **1** | 20908 | 32944 / 30768 |
| inst/3 | 0 | 66 | 0 | 3 | 5693 | **7** | **1** | 20911 | 29308 / 18072 |
| inst/4 | 0 | 66 | 0 | 3 | 5692 | **6** | **1** | 20910 | 33244 / 38412 |
| inst2/1 | 0 | 66 | 0 | 3 | 5693 | **7** | 0 | 20911 | 36180 / 28644 |
| inst2/2–5, 7, 8 | 0 | 66 | 0 | 3 | 5686 | 0 | 0 | ~20891 | two tids |
| inst2/6 | 0 | 66 | 0 | 3 | 5693 | **7** | **1** | 20913 | 42880 / 28860 |

**Instrumented failure rate: 0 / 12.** The defect vanishes under instrumentation — which is
itself evidence for a race, stated honestly here rather than presented as a fix.

### 1c. EXPERIMENT — packed seek+read replaced by one positioned read, 12 boots (md5 `7b292547`)

| run | exit | topics | diseases | oC | bM | rN | calls |
|---|---|---|---|---|---|---|---|
| pread/1–12 | 0 ×12 | 66 ×12 | 12 ×12 | 0 ×12 | 0 ×12 | 3 ×12 | 5686, 5693, 5728, 5740, 5741 ×2, 5742, 5743 ×3, 5686 ×2 |

**Experiment failure rate: 0 / 12**, and in **7 of 12** boots `calls > 5686`, i.e. the
second loader thread was still doing its own resource loads. The experiment removed the
shared-cursor window, **not** the concurrency. Fisher exact, 11/24 vs 0/12: **p ≈ 0.0007**.

### 1d. What `calls` variation means (a finding in its own right)

`[ExtLoad] calls` is bimodal: **5686** when only one thread loads, **5687–5878** when the
second one does too. In the instrumented batch this correlates exactly: every boot with
`calls > 5686` also had `ovlExtLoad = 6–7`, and every `calls == 5686` boot had
`ovlExtLoad = 0`. So the run-to-run `calls` variation the coordinator observed is simply
*whether the second loader thread got any resource loads in*, and it is visible without any
instrument at all.

### 1e. `openFileFail = 3` is benign and is NOT the defect

The instrument named all three, in every boot including the clean ones:

```
[LS] openFileFail tid=28088 path=C:\GOG Games\Banished\WinData\Game\LoaderResources_resource.crs   (×3)
[LS] FindOrLoad->NULL tid=28088 found=0000000000000000 path=Game\LoaderResources.rsc              (×3)
```

`Game/LoaderResources.rsc` is genuinely absent from the packages. It is a constant-3
`openFileFail` and a constant-3 `retNull` floor in every boot. **`retNull = 3` is the clean
baseline**; `retNull > 3` is the defect.

---

## 2. The shared state and the lock inventory

### 2.1 Every lock that exists on this path

There is **none**. A full grep of `runtime/src/io/`, `runtime/src/object/`,
`runtime/src/engine/`, `runtime/src/system/`, `standalone/rt_data_mint.cpp` and
`code/Game/Game/gameupdateloop_load.cpp` for `CriticalSection`, `EnterCriticalSection`,
`Lock(`/`Unlock(`, `std::mutex`, `SRWLOCK`, `Interlocked`, `std::atomic` returns:

| what | where | guards |
|---|---|---|
| `System::CriticalSection` ctor/dtor/Lock/Unlock — the primitive itself | `runtime/src/system/thread_win.cpp:57-82` (registered `:312-315`) | **nothing in `runtime/src/io/` or `runtime/src/object/` calls it.** Zero callers on the load path. |
| the allocator's heap lock (`_singleton+0x640`) | described in comments at `runtime/src/io/compressed_stream_write.cpp:209-210`, `compressedstream.cpp:473-474`, `file_stream.cpp:279-280`, and ~12 more | the heap only. Not resource state. |
| `Persistence` worker events | `runtime/src/io/persistence_worker_win.cpp:309-310, 592-597` | the save/load worker thread's lifetime. Not the package read path. |
| `std::atomic<void*> s_loadingLoop` | `code/Game/Game/gameupdateloop_load.cpp:915` | **the world BUILD only** — see §2.3. |
| `std::atomic<void*> s_worldBuiltLoop` | `code/Game/Game/gameupdateloop_load.cpp:889` | the loading-screen completion flag. |

### 2.2 The shared state two concurrent loads touch

| # | state | where | shared how |
|---|---|---|---|
| **S1** | **the package's one Win32 `HANDLE` and therefore its one kernel file pointer** | `runtime/src/io/filesystem_openfile_win.cpp:390` copies the `PackageFile`'s embedded `FileInfo` from `pkg+0xC0` (`RtIo_PackageFile_GetFileInfo`, `:297`); `:398-400` `SetFilePointer(pack.winHandle, abs, FILE_BEGIN)`; `:403` recurses into the loose arm at `:377` `::ReadFile(fi->winHandle, …)`. | **The copy at `:390` copies the handle VALUE. The file position lives in the kernel FILE_OBJECT behind that handle, not in the copy.** Four packages, ~20,900 packed reads per boot, two threads, no lock. **This is the defect.** |
| **S2** | the `PackageFile`'s own `FileInfo.cursor` at `+0xC0` | `runtime/src/io/package_file_read_win.cpp:514, 536, 540, 543-546, 551, 554` | mutated by `ReadPackfile`. Startup-only today, so not implicated in the boots measured, but the same hazard shape. |
| **S3** | the two refcounted `Filename` buffers inside that `FileInfo` | `filesystem_openfile_win.cpp:390` → `RtIo_FileInfo_CopyCtor` (`:216-236`) does two `Filename` copy ctors; `:407-408` two destructors | the refcount is a plain `++src[-4]` / `--` (`runtime/src/system/string.h:34`) — **non-atomic**. Two AddRefs + two Releases on the *package's* shared name strings on **every packed read**, from two threads. A lost update frees a live buffer. Not proven to have fired, but it is a second, independent hazard on exactly the same line. |
| **S4** | `ExternalFactory::_resources` / `_references` AVL trees | `runtime/src/object/external_set.cpp`, inserted by `external_add.cpp`, descended by `external_find.cpp:158` | unlocked, mutated by both threads. This is the already-documented `ExternalFactory::Add` AVL-rotate race (B33_RULES §0). **Separate defect — see §5.** |
| **S5** | `FileSystem::packfileIndexRoot` BST and `FileSystem::packfiles[]` | `filesystem_openfile_win.cpp:460-476` | read-only after startup; no writer during the boots measured. Not implicated. |
| **S6** | **`ExternalFactory::_pathStack`** — the relative-path stack on the **singleton** | written by `?PushPath@ExternalFactory@Object@@` / `?PopPath@` (`runtime/src/object/external_path_stack.cpp`, retail `0x1800bd0e0` / `0x1800bd040`), read by `ExpandRelativePath` (`runtime/src/object/external_path_win.cpp`) which `FindOrLoad` calls at `external_compiled.cpp:~230` before `Find` | **A single stack on a single shared object, pushed and popped by NESTED loads — from both threads.** Two interleaved nests scramble it, and the casualty is a *wrongly expanded path*, not wrong bytes. Not implicated by any boot measured here, but it is the one candidate that could produce a NULL resource with **every `[ExtLoad]` counter at the floor** (§5a). **Unproven — flagged, not claimed.** |

Explicitly **not** shared, checked and cleared: the `CompressedStream` object (stack-local
in `External::Load`, `runtime/src/object/external_load.cpp:287`), its window/header/block
arrays (per-object), `DecompressFromSource`'s scratch (stack, `compressed_stream_read.cpp:282`),
and `Compression::Decompress` (static, local cursor). There is no file-scope cache, buffer,
tree root or "current" pointer anywhere in `compressed_stream_*.cpp`, `file_stream*.cpp`,
`filesystem*.cpp`, `package_file*.cpp` or `external*.cpp` — every one of their statics is a
diagnostic counter.

### 2.3 What `[GUL::Load] redundant dispatch` actually rejects — and what still gets through

`code/Game/Game/gameupdateloop_load.cpp:894-912` records the fact in full: the loop's
`Load` (+0x30) is dispatched **twice, concurrently, on the same `GameUpdateLoop`, by two
distinct tids** — (1) our standalone `AsyncLoader` worker, (2) the Runtime
`Engine::Loader` thread spawned inside `fn_DialogLoaderLoopT_Startup`. The guard at `:915`
makes the second dispatch return immediately, so the **world build** runs once.

It does **not** stop the second thread from loading resources. That thread still runs
`UpdateLoop::LoadResources`, still constructs its dialogs, and — measured here — still
enters `Object::External::Load` **6–7 times while the other thread is inside it**
(`ovlExtLoad` in §1b). So `redundant dispatch` appearing in every boot, clean ones included,
is exactly right and is not a discriminator: the guard is doing its job on the build while
the *loading* stays concurrent.

---

## 3. The mechanism, link by link

| # | link | status | what proves it |
|---|---|---|---|
| L1 | Two threads run resource loads concurrently during boot. | **PROVEN** | `gameupdateloop_load.cpp:894-912` (two tids, confirmed 2026-06-17); instrument: `tidA` and `tidB` both non-zero doing packed reads in **all 12** instrumented boots; `ovlExtLoad = 6–7` in 5 of 12. |
| L2 | All resource bytes come through the packed arm of `FileSystem::ReadFile`. | **PROVEN** | `C:/GOG Games/Banished/WinData/` contains only `data0.pkg`, `data1.pkg`, `patch0.pkg`, `patch1.pkg` — no loose data tree. Instrument: **20,890–20,913 packed reads per boot**, and the only three loose opens are the absent `LoaderResources_resource.crs` (§1e). |
| L3 | That arm is a seek and a read on ONE shared kernel file pointer, unguarded. | **PROVEN** | `filesystem_openfile_win.cpp:390` (copy of `pkg+0xC0`), `:398-400` (`SetFilePointer` on `pack.winHandle`), `:403`→`:377` (`::ReadFile` on the same handle). Lock inventory §2.1: zero locks. |
| L4 | The two threads do interleave inside that pair. | **PROVEN (directly observed)** | Instrument: `[LS] OVERLAP packedRead pack=… owner=tidA me=tidB`, fired in 3 of 12 instrumented boots (`ovlPk = 1`). Rare per boot — which matches a ~46% boot-level failure rate, not a per-read one. |
| L5 | An interleave makes one thread read bytes from the other's offset. | **LIKELY** (mechanically forced by L3+L4; not separately logged, because logging it perturbs it out of existence) | The kernel file pointer is per-handle. If B's `SetFilePointer` lands between A's `SetFilePointer` and A's `::ReadFile`, A's read is served from B's offset. No other unlocked state on the byte path can produce wrong *bytes* (§2.2 S1 is the only one). |
| L6a | Torn bytes in the block directory → `CompressedStream::Open` fails `total == len` → `openCompFail` → `External::Load` returns `existing` (null) → the resource is NULL. | **PROVEN** | `compressed_stream_open.cpp:166` is the `total == len` check; `external_load.cpp:299-302` is the `openCompFailed` arm and `:442-450` is why it returns `existing`. Control boots: `oC ≥ 1` in 5 of 24, and every `topics = -1` boot had `oC ≥ 1`. |
| L6b | Torn bytes in a data block → the compiled-resource magic is wrong → `badMagic`. | **PROVEN** | `[ExtLoad] badMagic` = 1, 3, 1, 3 in four control boots and **0 in every clean boot**. The magic `0x5412a91f` is the first dword of the *decompressed* stream, read after `Open` already validated the directory — so those bytes came from the wrong file offset. A deterministic content problem would be constant across boots; this is not. |
| L6c | Torn bytes mid-`Serialize` → a `String` length reads garbage → the read runs past the block end → `chunk = w->end - pos == 0` → `while (n > 0)` cannot advance → **infinite spin**. | **PROVEN (site), LIKELY (trigger)** | `compressed_stream_read.cpp:429-442`: `chunk = w->end - pos` when `pos + n > w->end`, then `position += chunk; n -= chunk` — `chunk == 0` is a guaranteed non-terminating loop. Symbolized on **this** build, control run `ctrl/10` (exit 4): loader tid 23644 = `RtIo_CompressedStream_Read` (`compressed_stream_read.cpp:436`) ← `StreamMove` (`serialize_family.cpp:252`) ← `RtIo_BinaryReader_SerUShort` (`:330`) ← `SerializeArray<uint16_t>` (`:267`) ← `System::String::Serialize` (`string.cpp:1013`) ← `Object::ExternalPtrSerializeBody` (`external_ptr_serialize.cpp:404`) ← `RtObject_External_Load` (`external_load.cpp:428`), frames repeating; MAIN thread = `WaitForEngineLoader` (`standalone/loader_loop_stubs.cpp:94`) → `RtSystem_Thread_WaitForFinish` (`thread_win.cpp:212`). **The spin is a `String`'s character array being read past the end of its block.** |
| L6d | A NULL resource reaching an unguarded dialog ctor → exit 3. | **PROVEN** | 5 control crashes, all `READ of 0`/small offsets: `RtUiDialogInstance_Ctor` (`runtime/src/ui/dialog_create.cpp:684`) from `CreateBuildIdChildDialog` / `NewGameDialog_Ctor_Standalone`; `RtUi_NumberEditElement_GetValue` (`widgets_input.cpp:788`) from `NewGameDialog_StartGame`; `RtUi_ComboElement_AddItem` (`:734`) from `PopulateGeneratorCombo`. |
| L7 | Removing the shared-cursor window removes the defect while leaving the concurrency. | **PROVEN** | §1c: 0/12 with a positioned read, 7 of those 12 with both loaders active; 11/24 without. p ≈ 0.0007. |

**The spin is not itself a bug.** `CompressedStream::Read`'s loop is retail's, byte for byte,
and retail spins on the same input. It spins because it was handed a desynced stream, and
retail is never handed one because retail has one loader. *Do not put a `chunk <= 0` guard
in it* — that would skip the behaviour rather than restore it, and it would hide the next
occurrence of the real defect.

---

## 4. The instruments (temporary, reverted; commit `TEMP INSTRUMENTS — never merge`)

Count-limited `fprintf(stderr,…)` + `fflush`; no control flow changed; no `getenv`, no new
`BANISHED_*` flag, no new option; nothing with side effects inside a printf argument.

1. **Packed-read concurrent-entry detector** — `filesystem_openfile_win.cpp`, an atomic depth
   counter + owner tid wrapped around the packed arm (the `SetFilePointer` + recursive
   `::ReadFile` pair), printing `[LS] OVERLAP packedRead pack=N depth=D owner=tidX me=tidY`
   on a second thread's entry, first 50; plus an `atexit` tally
   `[LS] packedReads=… overlaps=… tidA=… tidB=…`.
2. **`External::Load` concurrent-entry detector** — same shape around the whole load body
   (recursion on one thread does not count), `[LS] OVERLAP ExternalLoad … path=…`.
3. **Every NULL-producing arm named** — `external_load.cpp` prints path + tid on
   `openFileFail`, `openCompFail` (with the `Open` validation numbers: which check failed,
   `blockCount`, `headerSize`, computed `total`, source `len`, the `FileInfo.handle`/`size`),
   `badMagic` (with the magic read), `notExternal` (with the type id), `badVersion` (got vs
   want). `external_compiled.cpp` prints every `FindOrLoad → NULL` with `found` and the path.
   `CompressedStream::Open` records its failing check thread-locally
   (`compressed_stream_open.cpp`).
4. **Spin detector** — `compressed_stream_read.cpp` prints once (cap 8) when
   `chunk <= 0` inside the `Read` loop, with stream identity, `position`, `activeWindow`,
   window `start`/`end`, `headerSize`, `windowSize`, `direction`, `source`, tid. **The loop is
   not broken and no value is changed.**
5. **`LoadHelpTopics_Step4` call-site tid** — `[LS] HelpTopics FindOrLoad on tid=…`.

These added several atomics and a branch to a path taken ~20,900 times per boot. That is
enough perturbation to close the window: 0/12. Reported as evidence, not as a result.

---

## 5. One defect or several?

| symptom | verdict |
|---|---|
| NULL resource (`topics=-1`, `openCompFail`, `retNull > 3`) | **Same defect.** |
| `badMagic` | **Same defect.** Direct evidence of bytes from the wrong offset. |
| Front-end / dialog crashes (exit 3) | **Same defect**, downstream: a NULL resource hits an unguarded deref. |
| Teardown / loader hang (exit 4) in `CompressedStream::Read` | **Same defect**, downstream: a desynced stream. Both the coordinator's QUITTING-phase hang and this investigation's RUNNING-phase hang (`ctrl/10`) are the *same spin site* under a nested `External::Load`; they differ only in what the main thread was doing when the loader stopped making progress. |
| `openFileFail = 3` | **Not the defect.** `Game/LoaderResources.rsc` is genuinely absent; constant in every boot (§1e). |
| `[GUL::Load] redundant dispatch` | **Not a discriminator.** It appears in every boot; the guard is working (§2.3). |
| `ExternalFactory::Add` AVL-rotate crash (~1 in 12, resave gate) | **A SECOND defect, still open.** Same root *cause* (two loader threads), different shared state (S4, the unlocked AVL tree). The positioned-read experiment does nothing for it, and it was not reproduced in the 48 boots run here. |
| Resave-gate ±1 resident-resource row flake (`count` 5688/5689) | **Probably the second defect (S4)**, possibly also this one — a resource that loaded NULL is a resource that never gets a row. Not separable from the evidence here; left UNKNOWN. |

So: **one defect with four casualties, plus one more open race on the same cause.** The
single root cause both share is that our standalone runs two resource loaders where retail
runs one.

### 5a. Two corroborating observations from the coordinator, and the one they leave open

**(i) The failing resource is not a bad file, and it loads successfully later in the same
process.** On the resave-gate route (`main` df735053, run 3 of 3) a boot printed
`topics=-1` with `openCompFail=1 retNull=4`, **and that same process's resave was
byte-identical to the golden**, whose reference table contains the row
`Game\Help\HelpTopics.rsc:resource`. So the resource was resident by save time.

This is predicted by the code and needs no further run: on a miss, `FindOrLoad`
(`external_compiled.cpp`) only `Add`s when `Load` returned non-null. **A failed load leaves
no cache entry**, so the next request for that path misses again and re-drives `Load` — which
succeeds, because nothing is wrong with the file. That is also why the affected boots carry
inflated `calls`: `ctrl2/9` ran **5878** loads against a clean baseline of 5686, i.e. ~190
re-attempts. It rules out "a `Serialize` body deterministically desyncs on this input"
(the 2026-09-12 cause) and confirms a transient, exactly as L5/L6a state. The coordinator's
question (a) — *which* thread eventually loads it — is therefore answered structurally
(whichever thread asks next) and does not need a measurement that instrumentation would
suppress anyway.

**(ii) `openCompFail` moved by exactly 1 on that boot**, so the failing attempt **did**
reach `External::Load`. That is the L6a path.

**What stays open:** the other agent's boot with `topics=-1` and **`openCompFail = 0`**, with
every other `[ExtLoad]` counter at the floor. That cannot be L6a, and it cannot be
`badMagic`/`notExternal`/`badVersion` (those counters would have moved), and it cannot be
`openFileFail` (that would read 4, not 3). Two candidates remain, and this investigation
separates neither:

* **S6, the shared `ExternalFactory::_pathStack`** — a scrambled expansion produces a
  *different* path, whose own load may legitimately succeed and return an object that is not
  an `ExternalList`, or whose `Find` legitimately misses. This is the only shared state
  identified that can produce a NULL resource with no `[ExtLoad]` counter moving.
* a `Find` hit on a half-built entry (S4, the AVL race), where the tree walk descends a node
  mid-rotation.

Both sit on the same root cause (two loaders) and neither is addressed by the fix of §6.
Instrumenting either means printing inside `ExpandRelativePath`/`PushPath`/`PopPath` or
inside the AVL walk — both hot, both on the path this investigation already showed is
perturbation-sensitive. **Recommended: fix the root cause (§6 primary) and re-measure; if a
NULL with all counters at the floor survives a single-loader build, it is a third,
genuinely independent defect.**

---

## 6. The recommended fix, and where it belongs

**The root cause is not a missing lock. It is a second loader.** Retail's `Engine::Loader`
owns resource loading; our standalone added an `AsyncLoader` worker alongside it
(`standalone/menudialogloopt_stubs.cpp` `Stub_Startup` step 3,
`standalone/loader_loop_stubs.cpp`). Retail's `FileSystem::ReadFile` gets away with a bare
seek+read on a shared handle because nothing else is reading. Adding a lock to
`FileSystem::ReadFile` would make the symptom go away while leaving our tree a
non-transcription of retail's at that address, and it would not touch the AVL race.

**Primary (faithful) fix — restore a single loader.** Move the single-owner decision from
the dispatch *target* (`GameUpdateLoop_Load`, `gameupdateloop_load.cpp:915`) to the point
where the second thread is allowed to begin loading at all, so that only one thread ever
executes `FindOrLoad`/`External::Load` during boot. The guard comment at `:905-912` already
records why the obvious version (nulling `m_nextLoop` across `DialogLoaderLoopT::Startup`)
fails; the workable shape is to stop **our** `AsyncLoader` worker from doing the load when
the Runtime `Engine::Loader` thread exists, rather than to let both run and reject the
loser's build. Belongs in `standalone/`, not in `runtime/`: it is a standalone-only
divergence and that is where it should be corrected and commented.

**Fallback, if a single loader is not reachable:** make the packed read positional — one
`::ReadFile` with an `OVERLAPPED` offset in place of `SetFilePointer` + `::ReadFile`, in
`filesystem_openfile_win.cpp`'s packed arm. Identical bytes, identical syscall count, no
lock, no blocking, no added atomics; the only thing removed is the interleave window. This
is the experiment of §1c (0/12). It **must** carry a comment saying it is a deliberate
divergence from retail's two-call shape, justified by our standalone genuinely having two
loaders where retail has one — exactly the exception CLAUDE.md allows. It does **not**
address S3 (the non-atomic `Filename` refcount on every packed read) or S4 (the AVL race).

**Do not** add a `CriticalSection` to `FileSystem::ReadFile`, a retry around
`CompressedStream::Open`, a bound or a `chunk <= 0` break in `CompressedStream::Read`, or a
null check at `LoadHelpTopics_Step4`. Each hides one casualty of a defect that has four.

---

## 7. What the B.3.4 STREAMS conversion must not disturb

The conversion of `runtime/src/io/` to real classes is spelling-only on every line below.
Concretely, it must not:

1. **Change the syscall shape of `FileSystem::ReadFile`'s packed arm.** Not the order, not
   the count, not the recursion into itself (`:403`). If the fallback of §6 is ever adopted,
   it is a *separate, deliberate, commented* change — not something a class conversion does
   on the way past.
2. **Cache the package `FileInfo`.** `:390` copies it out of `pkg+0xC0` on every read, with
   two `Filename` AddRefs. Caching it would change the refcount traffic and would hide S3.
3. **Make the `Filename` refcount atomic**, or non-atomic-but-different. It is
   `++src[-4]` (`runtime/src/system/string.h:34`) and it is retail's. Changing it changes
   timing on a path this investigation is about.
4. **Touch `CompressedStream::Read`'s loop** (`compressed_stream_read.cpp:429-442`). No
   bound, no `chunk <= 0` early-out, no signed/unsigned "cleanup" of `w->end - pos`.
5. **Touch `CompressedStream::Open`'s two deliberate signedness quirks** (`:140`, `:147`) or
   the `total == len` check (`:166`). That check is our only detector of a torn directory.
6. **Touch `SetWindow`'s three documented quirks** (§3.1–3.3 of `compressed_stream_read.cpp`)
   or `GetAvailableWindow`'s LRU tie-breaks. They decide which window is evicted and
   therefore what a desynced stream does next.
7. **Introduce any file-scope buffer, cache or "current stream" pointer.** Today there is
   none (§2.2); `DecompressFromSource`'s `scratch[0x8000]` is on the stack (`:282`) and must
   stay there. A static scratch would turn a rare interleave into a constant one.
8. **Give `CompressedStream` instances anything shared** — window arrays, header arrays,
   block arrays are per-object and must remain so.
9. **Add or remove a lock anywhere** on `IO::FileSystem`, `IO::PackageFile`, `IO::FileStream`,
   `IO::CompressedStream` or `Object::ExternalFactory`, per B33_RULES §0. If a commit makes
   this defect stop reproducing, that is a red flag to report, not a win — re-run the §1a
   control (24 boots) before believing it.

---

## 8. Ghidra questions for the coordinator to schedule

*(No Ghidra MCP tool was used by this investigation — one instance is shared and
`switch_program` is global state.)*

1. **`?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z` @ `0x1800ace00`** — does it
   call `?Lock@CriticalSection@System@@` / `EnterCriticalSection`, or read a
   `CriticalSection` field off `FileSystem::_singleton`, anywhere between the
   `SetFilePointer` at `0x1800ace61` and the recursive call at `0x1800ace7e`?
   *This is the decisive question.* If **no**, retail's correctness rests on a single
   loader and the fix belongs in `standalone/`, not in a lock here.
2. **`?OpenFile@FileSystem@IO@@` @ `0x1800ac880`** — same question, around the
   `packfileIndexRoot` BST walk at `0x1800ac938`–`0x1800ac98c`.
3. **Does `IO::FileSystem` contain a `System::CriticalSection` member at all?** Dump its
   ctor and look for `InitializeCriticalSection` / `SetCriticalSectionSpinCount` on any
   offset of the singleton. A CS field that our `FileSystem` type header does not model
   would change the whole answer.
4. **`?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z` @
   `0x1800bf320`** and **`?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z` @
   `0x1800be570`** — does either take a lock, and does `Object::ExternalFactory` have a
   `CriticalSection` member? (This decides the S4 / AVL-race investigation, not this one.)
5. **How many threads does retail's `Engine::Loader` create, and does the exe ever dispatch
   `UpdateLoop::Load` / call `FindOrLoad` from the main thread while that loader thread is
   alive?** Specifically: in the exe, does `fn_DialogLoaderLoopT_Startup` dispatch
   `m_nextLoop->Load` from anywhere besides `Loader::Run`? *This is the question that
   confirms our `AsyncLoader` worker is a standalone-only addition and therefore that the
   fix is ours to make.*
6. **`?GetFileInfo@PackageFile@IO@@QEBA?AUFileInfo@FileSystem@2@XZ` @ `0x180025400`** —
   confirm retail really copies the whole `FileInfo` (two `Filename` AddRefs) on every
   packed read, i.e. that the refcount traffic of S3 is retail's and not an artefact of our
   port.
7. **`?PushPath@ExternalFactory@Object@@` @ `0x1800bd0e0` / `?PopPath@` @ `0x1800bd040` /
   `?ExpandRelativePath@ExternalFactory@Object@@`** — does any of them take a lock, and is
   `_pathStack` per-object or is there a per-thread stack retail keeps that our
   `ExternalFactory` layout does not model? (S6, §5a.)
8. **`??0PackageFile@IO@@`** — does a `PackageFile` hold exactly one `FileInfo` / one
   `HANDLE`, or is there a second handle (e.g. per-thread) anywhere in its 0xC0+ layout that
   our `PackageFileView` does not model?
