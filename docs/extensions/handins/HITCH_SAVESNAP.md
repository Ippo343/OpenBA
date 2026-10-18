# HITCH_SAVESNAP hand-in (2026-10-18)

Branch `savesnap`, based on `hitch` `9ecc6e4fb`. The question (the user's): can the save stay
**retail-compatible** (byte-identical for the same world state, loadable by retail and by us, the
resave gate PASS) and still be taken as a **snapshot**, so the main thread does not stall for the
serialize? **Answer: yes.** The snapshot does not need a copy. The sim is frozen while a worker
serializes, and the main thread keeps drawing and moving the camera. Prototype `ext.background_save`
(class A, default off): **the save frame's main-thread share is 2.4-3.4 ms and the stress town has
no hitch row at the save**. Faithful saves had 76 ms over; `ext.async_save_compress` alone had 47 ms
over. Not merged.

| Commit | What |
|---|---|
| `cf6028eab` probe | the temporary measurement probe (section 1) |
| `c817e5b3d` | its revert (nothing of the probe stays in the tree) |
| `556969b56` refactor | byte-neutral: SerializeAll in two halves, EntityList's flush as a member, UpdateSave's tail as a member, the null engine seam |
| `a9617df4a` ext | `ext.background_save` |
| (this commit) docs | this hand-in, the `perf/README.md` line |

All numbers: stress town, speed 50, save at frame 6000 (`BANISHED_SAVE_TEST=6000`), perf exe,
machine load CLEAN on every leg quoted.

## 1. What the serialize reads, and whether it has side effects

Probe legs `20261018_131105_savesnap_probe`, `..._131941_savesnap_probe2` and `..._133541_savesnap_probe3`.

| quantity | value |
|---|---|
| raw image | 24.4 MB |
| objects persisted (InternalFactory objects) | 150,099 of 150,239 |
| of them ComponentEntity | 24,952 |
| field writes | 8.73 M (3.37 M of 1 byte, 1.19 M of 2, 4.00 M of 4, 0.17 M of 8) |
| Serializers constructed | 277,030 (each: a writer allocation, a 12-byte header, a back-patch) |
| seeks | 554,060 (two per Serializer) |
| process private bytes | 690-700 MB |

| section | bytes | walk ms |
|---|---|---|
| head (reference table to PlayerController) | 0.32 MB | 0.5 |
| entities | 14.3 MB | 42.0 |
| world geometry | 4.3 MB | 4.0 |
| citizen list | 0.1 MB | 0.3 |
| map | 3.9 MB | 12.4 |
| the rest (tutorial, tools, calendar, events, weather, minimap) | 1.2 MB | 1.5 |

The walk times above are the counted walk, which carries the object hook's overhead.

**What it reads beyond a component's own fields.**
- **Other objects, inline.** `InternalBasePtr::Serialize` writes `{present, factory index, generation,
  type}`. `InternalFactory::PersistObject` then writes the target's whole body at its first
  reference. So the save is a depth-first walk of the whole object graph from the roots, in
  first-reference order. Ids are factory indices, not pointer values.
- **The resource tree.** Every `ExternalPtr` asks `ExternalFactory::GetReferenceId`: about 187,000
  asks for about 925 distinct Externals, with the HITCH_SAVE memo taking all but the first.
- **Global state.** The InternalFactory's per-save persist-id table, the ExternalFactory's reference
  mode flag, and the UI's settings (`SaveAllSettings`).

**Side effects of the write path.**
- `EntityList::Serialize` opens with `PreUpdate`, `PostUpdateStaging` and the removal list. These
  delete the entities queued for removal. They mutate the sim, but they are a flush the next Update
  would do anyway.
- `CitizenBehavior::PostLoad` and `EntityList_PostLoad_c4730` do nothing on the save direction.
- `professionResources.SetSize(0x15)` is idempotent.
- **No side effect is visible in the output.** The flat image of a replayed walk equals the real save's
  decompressed image, and three replays are identical to each other. That holds at 24.4 MB, in all
  three probe legs. It confirms the recorded "write-path Serialize is replayable" on the dense town.

**The raw-walk cost** (probe2, best of three):

| walk into | ms | what it adds |
|---|---|---|
| a null stream (Write only advances the cursor) | 25.1-25.8 | the walk plus the per-field call chain |
| of which the per-field chain: 8.73 M `Serializer::Serialize(int&)` into the null stream | 11.2-13.2 (1.3-1.5 ns each) | |
| so the object walk alone | about 13-14 | Serializer construction, persist, virtual Persist, memo lookups |
| a flat pre-touched `IO::MemoryStream` | 48.0 | the CRT memcpy per field, about 2.2 ns |
| the real `CompressedMemoryStream`, compression excluded | about 39 | the window fast path from HITCH_SAVE |
| 8.73 M inline 4-byte appends, no call at all | 4.8 | |

## 2. The approaches

| approach | main-thread cost | memory | risk | class | verdict |
|---|---|---|---|---|---|
| (a) world clone / copy-on-write | not reachable | about 700 MB to copy | the graph is not relocatable | n/a | rejected |
| (b) flat intermediate on the main thread, encode on a worker | at least about 20 ms; about 30 ms keeping the call chain | +24 MB | replaying the window order | A, or faithful | 1.3-2x at best, still a hitch |
| (c) incremental, spread over frames | the slice per frame | none | the file mixes ticks | worse than C | impossible |
| (d) the sim pauses, the render keeps presenting | about 45-75 ms, unchanged | none | low | A | the render cannot present new frames |
| (d') the sim frozen, the serialize on a worker, the main thread draws | **2.4-3.4 ms measured** | the stream only | concurrency, mitigated | **A** | **prototyped** |
| (e) retail | its full stall | | | | hides nothing |

**(a) World clone.** Not possible in-process. Every object holds raw pointers: `InternalPtr` fields,
String data, Array buffers and tree nodes. A memcpy of the heap at another address would need every
embedded pointer fixed up, and only the type-aware serializer knows where they are. The only
same-address snapshot is OS copy-on-write:
- **Linux:** `fork()` would work. The child serializes and writes into a shared mapping. The
  allocator lock must be held across the fork.
- **Windows:** there is no supported fork. `RtlCloneUserProcess` is undocumented and the clone
  cannot use Win32. `PssCaptureSnapshot` VA clones cannot run code.
- **Verdict:** rejected for the shipped platform.

**(b) Tighter writer plus a worker encode.**
- **The floor.** A perfect sink still pays the object walk (13-14 ms) plus whatever call per field
  remains: 1.3-1.5 ns through the current chain, or 0.55 ns fully inlined.
- **Not 2-4x.** Against today's 39 ms the gain is about 1.3x keeping the 861 call sites' chain, and
  about 2x with every call site inlined.
- **The encode cannot be a plain pass over the final image.** The compressor's hash table carries
  over between windows. A few seek-backs reach windows already evicted (8 in HITCH_SAVE), and those
  windows were compressed before the back-patch. A worker encoder would have to replay the window
  sequence with the pre-patch bytes.
- **Verdict:** a possible faithful perf block later, about 39 to 20-30 ms. It does not remove the hitch.

**(c) Incremental serialize.** Measured per object, by exclusive-byte hash, against the save image:

| after | objects changed | object bytes changed | citizens' CitizenComponent | gone / born |
|---|---|---|---|---|
| 1 Update (50 sim iterations) | 5.0-5.1% | 9.4% | 79% | 80-128 / 0-32 |
| 2 Updates | 5.9% | 11.7% | | 155-231 / 13-29 |
| 10 Updates | 6.4-6.8% | 12.5-12.9% | 100% (CitizenAI 100%, Residence 100%) | 691-886 / 65-87 |

Most of the town is static: trees, MapComponent, NaturalResource and Spawn barely change. Which
objects change is not predictable, though. Objects are deleted between ticks: MoveTo, PlayAnimation
and PerformWork action objects and SendComponentMessage. A file spread over frames would hold
references written at tick t to objects freed at tick t+k, and cross-object invariants from
different ticks. Making that consistent needs a write barrier in every sim mutation. **Impossible,
not merely class C.**

**(d) Sim stalls, the render keeps presenting.** The render thread (`ext.render_pipe`) only emits
packets the main thread gathers. With the main thread inside the serialize it has nothing new to
present. Draw-only frames would not help by themselves either:
- `PlayerController::UpdateCamera` is idempotent and alpha is fixed, so every frame would be the
  same picture.
- A held picture is what a hitch looks like.

**The catch-up**, measured from the Update and Draw timeline after the save:
- **A stall over 1/15 s** (faithful, 75 ms): `MainLoop::Frame`'s clamp resets the accumulator to one
  step. The next frame runs one Update and the time is simply dropped.
- **A stall under it** (`async_save_compress`, 46 ms): the next frame runs **3 Updates, 23 ms**.

That second frame is a real secondary hitch. The hitch recorder does not flag it, because its
baseline allows k Updates.

**(d') What works.** The sim is frozen for the serialize; it is not merely stalled. The main thread
keeps running frames, so camera input and Draw continue, and the serialize moves to a worker. This
is sound when nothing the main thread does in those frames touches what the worker reads.
Measured:
- **Draw changes no serialized byte.** The image at the next Update's start equals the image at the
  save Update's end, by exclusive-byte hash, every object.
- **Draw makes no ExternalFactory call.** It made 0 in 3,000 Draw windows, which include the frame
  head and the message pump, against about 7,000 per Update. So the worker's reference mode and
  its reference-id lookups never meet a resource load on the main thread.
- **Camera input writes two things.** PlayerController (yaw, target, distance) is serialized in
  the head, which is done on the main thread before the worker starts. Three tool-manager floats
  (+0x3c zoom, +0x40 pitch target, +0x44 manual pitch) are serialized by the body, so the frozen
  frames write a shadow copy, which is copied back after the join.
- **The frozen Updates consume the fixed-step accumulator**, so no catch-up follows.

**(e) Retail.** Retail does the whole save inside one Update on the main thread: `UpdateSave` state 1,
`SaveRestore`, compression inline. `SetMinOperationTime(1.0 s)` only keeps the spinner on screen
*after* the work, in state 2. It hides nothing. On this path retail's own stall is the faithful
leg's 75 ms or more. Retail cannot be timed: there is no unattended retail route.

## 3. The prototype: `ext.background_save`

- **The engine seam** (`src/engine/save`): `Save::SaveLoad::_extSaveStart` / `_extSavePoll`, null by
  default. They are called in place of `SaveRestore(true, state)` in `UpdateSave` state 1. While
  the worker runs, the machine waits in progress `0x100`. The rest of state 1 (name, SaveBlocks,
  enqueue) is `UpdateSaveTail`, shared by both arms.
- **The main thread runs**:
  1. `CompressedMemoryStream::Create`;
  2. `async_save_compress`'s attach, when that row is on;
  3. `SerializeAllHead`, the reference table through `PlayerController::Serialize`;
  4. `EntityList::FlushForSerialize`. The flush deletes entities, so it must not run beside Draw.
     It writes no bytes, and `EntityList::Serialize`'s own call then finds the lists empty.
- **The worker runs** `GameUpdateLoop_SaveRestoreBody`: `SerializeAllBody`, the B / C manifest tail,
  `Close` and `Store`. These are the same statements, in the same order, as the save arm of
  `SaveRestore`.
- **While busy.** `GameUpdateLoop::Update` is `BackgroundSave::FrozenUpdate`:
  `PlayerToolManager::UpdateCamera` on the shadow, the step-6 camera transform, and
  `SaveLoad::Update`, whose poll joins the worker and runs the tail. Draw forces alpha to 1, as when
  paused. Hotkeys, tools, UI, audio scene and sim do not run. `~GameUpdateLoop` joins a running save.
- **The faithful path** pays one null test in `UpdateSave` and one `Busy()` test per Update and per
  Draw.

**Identity.**

| check | result |
|---|---|
| resave gate, faithful profile, `GATE_EXE=build/cmake-perf/OpenBA.exe` (after the refactor) | PASS, golden md5 `971aaec2cf95` (the main checkout's), compressed resave hash `4127020a2ae682eb` (= HITCH_SAVE's) |
| `BANISHED_STRESS_COMPPROF=4`, `STRESS_EXT=background_save`, save at frame 6000 (`..._135225_savesnap_bg_verify`) | `[BackgroundSave] verify IDENTICAL`: the background image equals a synchronous save of the same state, 24,379,027 bytes, over 9 frozen Updates |
| unit (`tools/build.sh unit perf`) | 1170 tests, 0 failures, 3 skipped (the kit cross-checks) |
| acceptance | 0 lines off target |

The verify mode serializes synchronously at the save point, which the replay result in section 1
makes exact. The background path then runs. After the join its decompressed image is compared byte
for byte, through `StressHarness_Verify("BackgroundSave")`. The compressed bytes come from the same
CompressedStream code: the synchronous compression on the worker, or the HITCH_SAVE async path,
which is proven identical.

**Before and after** (`perf/results/`, 7000 frames). In these legs the save's row is frame 6001.

| leg | main-thread save ms | frame 6001 period ms | excess ms | next frame | sim frozen ms (wall) |
|---|---|---|---|---|---|
| `20261018_135522_savesnap_t_faithful` | 75.2 (SaveRestore) | 84.7 | 76.1 | 1 Update (clamp reset) | n/a |
| `20261018_135743_savesnap_t_async` (`async_save_compress`) | 46.1 | 55.3 | 46.6 | 3 Updates, about 23 ms | n/a |
| `20261018_140003_savesnap_t_bg` (`background_save`) | **3.4** | no row | **0** | no row | 92 (worker 85, 6 frozen Updates) |
| `20261018_140223_savesnap_t_bgasync` (`background_save,async_save_compress`) | **2.4** | no row | **0** | no row | 60 (worker 47, 4 frozen Updates) |

Neither background leg has a hitch row anywhere from frame 5990 to 6080. The save completes at
frame 6055 / 6057 instead of 6061; the 1 s spinner minimum still holds. The bgasync leg's save is
kept as `perf/results/20261018_140223_savesnap_t_bgasync/Auto_background_save.sav`; that directory
is untracked.

**Recommended row:** `background_save` together with `async_save_compress`. That gives a 2.4 ms
main-thread share and a 60 ms frozen sim. The town holds still for that time at speed 50, while the
camera and frames keep moving.

## 4. Risks and what is not proven

- **Camera motion during a save is untested unattended.** The stress camera never moves, so the
  shadow path ran but with no input. A by-hand check: autosave every minute, pan and zoom through
  saves, then confirm the zoom and pitch afterwards and resave with verify on.
- **Draw's writes are proven for the frames observed, not in general.** The checks cover the
  afterDraws hash and the verify leg's 9 frozen Updates, with the camera still. A camera that
  moves to unseen terrain or LODs during a save is not covered. The verify mode catches a
  difference per save, so run it on a hand-played session.
- **Non-atomic String refcounts.** Retail's `INC [data-4]` is not atomic. The worker copies Strings
  only in the memo's first asks (resource paths, which Draw does not touch) and inside Serialize
  bodies. Draw reads no entity names. No race was observed. An audit of each String copy in the
  write path would close it.
- **Paths not exercised.**
  - A second save in the same process.
  - Toggling the row mid-save: the poll then clears the seam.
  - Window resize or device lost during the frozen frames: `OnVideoRestart` runs on the main
    thread beside the worker.
  - Quitting mid-save: the destructor joins.
  - The debug flavour: only the perf build was compiled.
- **What the player loses for 60-90 ms:** tool clicks, hotkeys and UI. The input is dropped, not
  queued. The town's animation holds still. The sim time is dropped, as retail drops a stall over
  1/15 s.
- **No row icon.** The Extensions sprite sheet is full; this is the same open UI call as for
  `async_save_compress`.

## 5. The proof plan for a merge

1. Resave gate, faithful, `GATE_EXE=build/cmake-perf/OpenBA.exe`: PASS (done; repeat on the merged tree).
2. `BANISHED_STRESS_COMPPROF=4 STRESS_EXT=background_save,async_save_compress BANISHED_SAVE_TEST=6000
   tools/stress_run.sh --speed 50 --frames 7000`: `[BackgroundSave] verify IDENTICAL`, no crash, no
   hang. The comparison runs inside one process, so the stress town's non-reproducibility between
   processes does not matter.
3. The by-hand camera session from section 4, with the verify level on.
4. The expected frame-6001 / 6002 row: none. The save's main-thread share is 2-4 ms, under the 7.14 ms bar.
5. Class A: the saved bytes are retail's, no serialized state is added, and the sim per tick is
   unchanged. The sim stands still for one save's worth of wall time.
