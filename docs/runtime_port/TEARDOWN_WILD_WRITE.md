# The teardown wild write (open, 2026-09-13)

**One 4-byte wild write, not three bugs.** This is the defect that shows up as
`sa-stress-debug` crashing in teardown inside `rtalloc::Free`, and — on the same
evidence — very probably also as the "teardown hang after the pause-menu Quit"
and the two other intermittents seen during this hunt (a boot-time free-list
fault while loading `Game\Achievement\AchievementList.rsc`, and an XAudio2 fault
mid-sim). It is NOT a teardown-ordering bug, NOT a double free, and NOT a bad
shutdown order. It is a single stray 32-bit store that lands in the engine
allocator's own metadata; the crash happens later on whichever block the store
happened to sit next to.

This document records what is established, what is not, and the exact next
experiment. Nothing here is fixed yet.

## 1. What is established

### 1.1 The heap is clean when teardown starts and corrupt inside ONE markRemoval

`EntityList::Shutdown` (`code/Game/Game/gameupdateloop_teardown.cpp`) already had
a bracket instrument (`BANISHED_TEARDOWN_BRACKET`, Wave 9 lane N Finding 3). It was
unusable at step granularity because the boundary-tag walk called `IsBadReadPtr`
once per block (~2 s per walk on the 800-citizen town). That walk is now
region-cached with `VirtualQuery` (this commit, `standalone/redzone_alloc.cpp`),
which makes `BANISHED_TEARDOWN_BRACKET=1` — a walk after EVERY entity — affordable.

With that, two independent runs agree:

```
[TeardownBracket] ARMED every 1 step(s); heap at entry is clean
[TeardownBracket] *** HEAP WENT CORRUPT *** phase=mark step=13 ...
```

So: clean at `EntityList::Shutdown` entry, corrupt after the **13th** entity's
`MarkForRemoval`. Everything after that (the drain, the sweep, `RemoveEntity`,
`ComponentEntity::~ComponentEntity`, `MeshInstance::~MeshInstance` freeing
`_materials`) is a VICTIM, not the cause. The original crash report's stack
(`rtalloc::Free` <- `MemFree` <- `ArrayDtorImpl` <- `MeshInstance::~MeshInstance`
<- `ModelComponent::ScalarDtor` <- `InternalBasePtr::DeletePtr` <-
`EntityList::RemoveEntity` <- `EntityList::Shutdown`) is the sweep tripping over
the damage laid down ~hundreds of entities earlier.

### 1.2 The damage is always a 4-byte small-int store at a 4-mod-8 address

Every capture has the same shape. Three teardown captures:

| run | corrupt block | tag read | correct tag would be |
|---|---|---|---|
| 1 | `…4526c1a8` | `0x0000000200000006` | `0x……00000006` (24 bytes) |
| 2 | `…9145eb58` | `0x0000000200000010` | `0x……00000010` (64 bytes) |
| 3 | `…91b63d68` | `0x0000000200000010` | `0x……00000010` (64 bytes) |

The block **size** dword (bits 0..31) is intact in every case; only the dword at
`block+4` has changed, and it has changed to **2**. `kTagSizeMask` is bits 0..40,
so a `2` in the high dword means `size += 0x200000000`, i.e. `next = blk + 0x800000018`,
which is the out-of-range read that faults in `Free`'s forward-coalesce
(`runtime/src/system/allocator.cpp`, `uint64_t nv = Tag(next);`).

A fourth capture, at BOOT rather than teardown, is the same store hitting a free
node instead of a tag:

```
bucket 12: free node @0x00000220598d0140 OUT OF RANGE
  reconstructed in-range node @0x0000021f598d0140
```

i.e. the free-list next-delta came back exactly `+2^32` too large — bit 32 of a
41-bit signed delta set by a stray small int. Same signature, different victim.

Since allocator user pointers are always 8-aligned (`user = tag + 8`, tags are
8-aligned), a store at a 4-mod-8 address means the writer thinks it owns an object
whose field offset is `4 mod 8` (`+0x04`, `+0x0C`, `+0x14`, `+0x1C`, `+0x24`,
`+0x2C`, …) and writes a small integer (0, 1 or 2) into it. The block it lands in
is live and unrelated — so the writer is holding a **stale pointer**, or is writing
past a buffer it under-allocated.

### 1.3 The cascade that contains the write

The bracket's detection was cross-checked with the heap watchdog
(`BANISHED_REDZONE=watch`), which suspends and unwinds the main thread on
detection. Symbolized (addresses are `reimpl-x64.exe` offsets; base 0x140000000):

```
GameUpdateLoop::Shutdown                        gameupdateloop.cpp:2952
 EntityList_Shutdown_c3d30  (mark loop)         gameupdateloop_teardown.cpp:300
  ActHunt_MarkEntityRemoval_c2ea0  (0x1400c2ea0) action_base_hunt.cpp:176
   WorkComponent_OnRemoveVSlot                  workcomponent.cpp:367
    WorkComponent_OnRemove_CancelWork           workcomponent.cpp:220
     WorkComponent_RegisterWorkSlot (0x14012eb50) workcomponent.cpp:322
      ActCt_TaskCleanup_8e30     (0x1400f8e30)  action_base_complete.cpp:538
       MarketWork_Slot8_Impl                    marketwork.cpp:355
        WorkBase_TaskListCleanup_c5400 (0x1400c5400) work_base_reimpl.cpp:253
         DropOffRawMaterial_CheckEntity_Impl (0x1400c72f0) dropoffrawmaterial.cpp:376
          Pf_SpillAllStorage_7570  (0x1400c7570) action_pf_roots_drop.cpp:257
           Pf_SpawnRawMaterialOnCell_78c0 (0x1400c78c0) action_pf_roots_drop.cpp:226
            EntityList_RegisterEntity_Impl (0x1400c48a0) componententity.cpp:1063
             ComponentEntity_Dtor                componententity.cpp:266
```

In words: **during shutdown, marking a workplace for removal cancels its work,
which cancels a citizen's carried DropOffRawMaterial action, which spills the
carried inventory — and spilling SPAWNS new raw-material entities into an entity
list that is already CLOSED (`entityList+0xE0 == 1`), so registration deletes each
one on the spot.** This whole recursive spawn-and-immediately-destroy tree runs
inside one `MarkForRemoval`. It is retail-shaped (the closed-list delete branch is
retail's own), but it is by far the most complex thing that happens during
teardown and it is where the corruption appears.

### 1.4 The write happens BEFORE the first spawn

A second instrument (temporary, since removed) put a heap walk at the entry and
after each sub-call of every level above. In the one run that reproduced with it
armed, the first probe to see corruption was `7570:before-spawn` — at the top of
`Pf_SpillAllStorage_7570`'s FIRST loop iteration, before `Pf_SpawnRawMaterialOnCell_78c0`
had been called at all. So the writer is **upstream of the spawn**, somewhere in
`MarkForRemoval → WorkComponent::OnRemove → RegisterWorkSlot → 0x1400f8e30 →
MarketWork slot8 → TaskListCleanup → DropOff cancel`.

### 1.5 Candidates this narrows onto (not yet proven)

Both are 32-bit stores of a small int at a 4-mod-8 offset, both inside the interval:

* `workcomponent.cpp` `WorkComponent_RegisterWorkSlot`:
  `*(uint32_t*)(self + 0x2c) = *(uint32_t*)(valueIbp[0] + 0x24);`
  — `+0x2C` is 4 mod 8, the value is a worker/slot id (small), and this runs
  immediately before the `0x1400f8e30` call in the stack above. If `self` (a
  WorkComponent) has already been freed further up the mark cascade, this is
  exactly the observed store.
* Lane N Finding 3's own mechanism: `0x1400f8e30` step 5 destroys the work object
  **unconditionally**, even when step 4's bucket erase no-opped, leaving a bucket
  entry that points at freed memory; the key that entry is later re-keyed/erased
  through is `*(int*)(workObj + 0x24)` — also 4 mod 8, also a small int. Finding 3
  measured `entryNotFound=0` on a 1,200-frame gameplay run, but that probe has
  never been run over THIS route (teardown, 6,000 frames, market work).

### 1.6 What is ruled out

* Teardown ordering / freeing the allocator's pages before the last entity — the
  heap is provably clean at `EntityList::Shutdown` entry and the fault is a tag,
  not a missing range.
* A double free of `MeshInstance::_materials` (the original suspicion) — the
  materials array is simply the block that happened to sit after the smashed tag.
* An overflow out of the physically-preceding block — the "overflower" the redzone
  dump names is a 24-byte block whose 16-byte payload differs run to run
  (`[1,2,2,2]`, `[1,0,0,0]`, `[1,0,1,0]`) while the corrupt dword is always `2`,
  which is a stale pointer's field, not a contiguous overrun.
* A worker-thread race: the watchdog dumped every thread; only the main thread is
  in game code during teardown.

## 2. Reproduction

```
tools/game_dir_lock.sh acquire <tag> 600
BANISHED_TEARDOWN_BRACKET=1 mingw32-make sa-stress-debug      # or sa-stress for the perf exe
tools/game_dir_lock.sh release <tag>
```

Then read, in order: `standalone_crash.log`, then `[TeardownBracket]` lines in
`standalone_error.log` (the run is also archived under `perf/results/<stamp>_run/`).
`BANISHED_TEARDOWN_BRACKET=1` costs about 25 % of the run's wall time now that the
walk is region-cached; `=1000` is free.

**Rate (measured 2026-09-13, 800 citizens / 3 sets / speed 10):**

| build | frames | runs | reproduced |
|---|---|---|---|
| debug (-O0), no in-cascade probes | 6000 | 5 | 3 |
| debug (-O0), no in-cascade probes | 1500 | 5 | 0 |
| debug (-O0), in-cascade probes armed | 6000 | 8 | 1 |
| perf (-O2) | 6000 | 3 | 0 (inconclusive — run was cut short) |

Two readings matter. **6,000 frames is required** — at 1,500 frames the teardown
cascade looks identical in the log (same 25 `[Unassign]`, same 6 `[Residence] ERASE`)
but the corruption never appeared in 5 runs, so what matters is how much inventory
citizens are carrying at the moment of the quit, not the shape of teardown.
And **the in-cascade probes themselves suppress it** (1 in 8 with them, 3 in 5
without): a whole-heap walk between the sub-calls changes teardown timing enough to
hide it, exactly as the redzone shim was already recorded to do. Any further
instrument has to be cheaper than a heap walk per sub-call.

Two unrelated intermittents were also hit during this hunt and cost runs: an
XAudio2_7.dll fault at ~frame 4600 during `[MusicPlayer] PLAY`, and a boot-time
fault walking free-list bucket 12 while loading `AchievementList.rsc` (§1.2 — that
one is almost certainly the same wild write with a different victim).

## 3. The next experiment

Do NOT add more heap walks into the cascade; they hide the bug (§2). Instead:

1. **Narrow with a hash, not a walk.** Snapshot every block tag's high dword into
   a flat array once at `EntityList::Shutdown` entry, then after each sub-call
   compare a rolling 64-bit hash of that array. A hash over ~1 M dwords is a
   memory-bound pass of a few ms with no `VirtualQuery` and no allocator lock —
   an order of magnitude cheaper than the walk, and cheap enough not to move the
   race. Walk only once, when the hash first changes.
2. **Then arm a hardware watchpoint.** Once the sub-call is named, re-run with
   `standalone/hw_watch.cpp` armed on the victim address. The victim address is
   only known after the write, so this needs step 1 first — but corruption recurs
   across entities within the same run, so arming on the first victim after
   REPAIRING its tag should catch the second occurrence with a full stack.
3. **Cheap falsification of §1.5 first**, before either: make
   `WorkComponent_RegisterWorkSlot` log `self` and `*(uint32_t*)(self+0x2c)`'s
   address for every call during teardown, and check whether any of them equals a
   later-reported corrupt `block+4`. That is one run and no heap walking at all; if
   it hits, the bug is a WorkComponent used after free in the mark cascade and the
   fix is upstream of the store, not a guard on it.
4. If §1.5's second candidate is the one, the fix is in `0x1400f8e30` step 5 —
   which per Finding 3 is a place where **retail hard-faults and our port guards**
   (`if (node != nullptr)` around the bucket search, retail's `xor ecx,ecx / mov
   rdx,[rcx+8]`). That guard is currently MASKING a state retail treats as
   impossible; the fix is to stop reaching that state, not to keep the guard.

## 4. Instrument left in the tree by this work

`standalone/redzone_alloc.cpp` only: `ReadablePtr` and
`RedzoneAlloc_FindCorruptBlockNative` now advance their readable window with one
`VirtualQuery` per memory region instead of one `IsBadReadPtr` per block. This is
unconditional and costs nothing at runtime (both are only called from the redzone
watchdog and the teardown bracket, which are already gated), and it is what makes
`BANISHED_TEARDOWN_BRACKET=1` usable. No new flags; the temporary per-sub-call
probes were removed.
