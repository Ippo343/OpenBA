# SAVELOAD -- the save load's name lookups (block of LOADPLAN, 2026-10-14)

Target (LOADPLAN §3): `ser.entities` 785 -> <= 300 ms on the perf exe, Gate.sav, byte-identical.
Static gate only; the game was not run. Branch `worktree-agent-aa8d069c29eaf51b8`.

## 1. The path, read

Per resource reference inside `ser.entities` (reference mode ON, `gameupdateloop_serialize.cpp`
SerializeAll), `Object::SerializeExternal<T>` -> `ExternalPtrSerializeBody`
(`src/engine/object/external_ptr_serialize.cpp`, mode 2):

1. `SerializeUInt(refId)`; `GetPathForReferenceId(refId)` (`external.cpp`): an id-AVL descent, then
   two Filename copy ctors (each runs AdjustDelimiters TWICE), then `path = tmp` (one more).
2. `FindOrLoadAs` -> `RtObject_ExternalFactory_FindOrLoad` (`external_compiled.cpp`):
   `ExpandRelativePath` (`src/engine/io/external_path.cpp`): GetPathExtension / Identifer /
   Filename / Directory (a SubString + AdjustDelimiters each), the data directory copied twice
   (copy ctor = 2 AdjustDelimiters), the 6-term candidate (`...\<file>_<ident>.crs`), the gated
   package probe `ExistsInPackfile` (a Filename ctor + an AVL descent with `_stricmp`), the
   answer's 4-6 concatenations, the final ctor: **~25 String allocations, ~10 AdjustDelimiters**.
3. private `Find` (`external_find.cpp`): two copy ctors (4 AdjustDelimiters), GetPathIdentifer,
   the `_resources` AVL descent. Hit -> `Add` if `_loadEnabled`; miss -> `External::Load`.

`AdjustDelimiters` re-read `String::GetLength` (an out-of-line call) once per character in phase 1
and twice per character in phase 2: ~20 calls x ~50 characters x 3 = ~3,000 calls per reference.
That is the census's 37% AdjustDelimiters + 26% GetLength + 30% copy ctor. The allocations are
the 35% MemAlloc/MemFree (the allocator takes a critical section per call).

Before the entities, `[PreloadRefs]` (SerializeAll, reference mode OFF) calls FindOrLoad once per
reference-table row; those are the first requests for every name the entities then repeat.

## 2. As built (two `perf:` commits)

**15ecc302f -- AdjustDelimiters and GetLength, cheaper, same bodies.**
- `String::GetLength` (0x1800063c0) header-inline, `always_inline`, body unchanged
  (`src/engine/system/String.h`, removed from `string.cpp`).
- `Filename::AdjustDelimiters` (`src/engine/system/filename.cpp`): the length is read once per scan
  (nothing in a scan can change it: phase 1 writes characters in [0, len), never the header at
  data-8; phase 2's scan writes nothing); a phase-2 scan returns at once when its window [0, len-3]
  holds no `\..` triple (no splice is possible without one, whatever `lastDelim` holds, and
  retail's scan then returns). When a triple exists the transcribed scan runs unchanged. Nearly
  every call is on an already-normalised path: one tight pass for '/' and one for `\..`.

**8090fd9b3 -- a memo in front of `ExpandRelativePath`'s body.**
- `src/engine/object/expand_memo.{h,cpp}`: request characters + `appendIdentifier` -> the body's
  answer, under a context (`FileSystem::_singleton`, its `_dataDirectory` characters, the package
  index generation). Any change of context drops every entry. CRT heap (`std::unordered_map`),
  never destroyed (a loader thread at exit must not meet a dead map), bounded at 65,536 entries.
- `src/engine/io/packfile_generation.h`: `IO::BumpPackfileIndexGeneration()` after every change of
  `FileSystem::_packfileIndex`: `ReadPackfile`'s insert and its override arm
  (`filesystem_packages.cpp`), `ClosePackages` (same file), `~FileSystem`
  (`filesystem_createfile.cpp`). Those are the only writers of the tree (grep of `_packfileIndex`,
  `FilenameSetInsert`). One relaxed atomic; no retail state changes.
- `src/engine/io/external_path.cpp`: the transcribed body is now `ExpandRelativePathBody`,
  unchanged but for an out-flag set at `hitPackfile` / `hitPackfileRetry`. The exported
  `RtObject_ExternalFactory_ExpandRelativePath` takes `ResLoadGuard` first (so the memo is
  single-threaded), uses the memo only for a non-empty request **with the path stack empty**,
  stores only answers **a package probe gave**, and returns `Filename(cached chars)` on a hit.
- `FindOrLoad`, `Find`, `Add`, `External::Load` are untouched: the memo is below them, so the found
  External is the one the body's answer finds, and `External::Load` runs exactly as often.

### Why the memo cannot go stale (expand_memo.h has the full list)

The body's answer is a function of: the request characters, `appendIdentifier` (both the key); the
path-stack top's filename and directory (memo used only when the stack is EMPTY: a request made
while an .rsc is parsed always runs the body); the data directory characters and the filesystem
object (the context); and "does the candidate exist". Only package-probe answers are stored: if the
package directory holds the candidate, the outcome is "found" whatever the two gate bytes and the
disk say (gate set: the first probe finds it; gate clear: GetFileAttributesW finds it or the
ungated retry does), so a stored answer depends on the package directory alone, i.e. on the
generation. "Not found" and disk-probe answers are never stored and always run the body.

What a hit skips has no effect outside the body: its own allocations; the probes (reads, a stat);
the `g_pc` counters (no reader in the tree); AdjustDelimiters on fresh buffers or on the
already-normalised data directory (idempotent: proved in `filename_adjust_test.cpp`). The request's
own buffer is never written by the body (the only copy of it, in GetPathIdentifer, happens for a
request with no '/' or '\', where AdjustDelimiters writes nothing). The returned Filename is a
fresh unshared buffer with the body's characters, as the body's own answer is (its last
concatenation always allocates: the "." literal).

**The debug exe checks every hit:** under `_DEBUG` a hit also runs the body and compares; a
difference prints `[ExpandMemo] MISMATCH path=... memo=... body=...` on stderr and returns the
body's answer. Perf / release trust the memo.

## 3. Proof

- `tests/unit/filename_adjust_test.cpp`: the pre-change AdjustDelimiters (kept in the test only)
  vs the new one on every string of length 0..8 over `{ a / \ . : }` (488,281 strings) and on
  the game's resource names (`tests/unit/data/resource_names.inc`, 140 names from `src/`, each
  raw, under a data directory, with `../` segments spliced in, and as a `.crs` path): identical
  characters and length. Negative control: the old body with `lastDelim >= 0` fails at once
  (5 reported diffs, 1 failure). Idempotence of AdjustDelimiters on the same corpus; the inline
  GetLength on null / "" / a name.
- `tests/unit/expand_memo_test.cpp`: a hit only for the same characters and flag (prefix, suffix,
  case variant miss); generation, filesystem pointer and data-directory changes each drop
  everything (and an old generation does not bring an entry back); null vs "" directory; the bound.
- Static gate after merging main (`ecfef5a07`): see §6.

## 4. The remount on a save load (left as is)

`Save::SaveLoad::UpdateLoad` progress 1 (`src/engine/save/saveload_update.cpp:685`) disables every
mod and sets `mods->_dirty = 1` unconditionally, then re-enables and bubbles each mod the save
lists (`:745`, `:753`). The OuterLL `Loader::Load` (`src/engine/engine/loops.cpp:420`) then runs
`Modification::SetupFileSystem` (`src/engine/io/modification_setupfilesystem.cpp:318`, returns
only when `_dirty == 0`), which: snapshots every loaded External's compiled path -> package id;
`ClosePackages` (closes every package handle, deletes every `PackageFile`, frees the index);
restamps `Mod::order` on every mod and rebuilds `_userPackFilenames` from the enabled ones;
`LoadPackages` (re-reads every package directory); re-checks every External and, where its
package id changed, `Destruct` + in-place re-construct + `External::Load` (or `ForceUnload` when
it vanished); `_dirty = 0`. `_loadEnabled` is held true across it.

Not skipped. Even when the enabled list and order are unchanged, the remount is not observably
a no-op: the `PackageFile` objects, their file handles and every index node are new (different
heap addresses and handle values afterwards), `Mod::order` is rewritten, and UpdateLoad's bubble
can reorder the mod array without changing the enabled set; proving "identical after" would
mean comparing the whole package state, for 28 ms (`fs.remount`). Recorded for the user / a later
block; the memo above handles it correctly (ClosePackages and every insert bump the generation,
so the memo is emptied by every remount).

## 5. The SaveLoad minimum operation time (1,465 ms, block SPLASH; NOT changed)

- **Set:** `SaveLoadDialog::DoLoad`, `src/game/Dialog/saveloaddialog.cpp:859-860`:
  `RtSystem_Time_Ctor(&minTime, 1.5f); saveLoad->SetMinOperationTime(minTime);` (retail
  0x1400b61e0; `DoSave` has the same 1.5 s at `:815`; `OnOpened` 0.75 s for the list at `:430`).
  `SetMinOperationTime` stores the ticks in `_minOperationTime` (`src/engine/save/saveload.cpp:598`).
- **Started:** `saveLoad->SetOperation(2, selectedIdx)` at `saveloaddialog.cpp:1049` (the
  `load.click` stamp) zeroes `_elapsed` (`saveload.cpp:587`).
- **Clock:** `Save::SaveLoad::Update(delta)` adds the frame delta, `_elapsed += delta._ticks`
  (`saveload.cpp:796`), before its worker-idle test, every menu frame
  (`MenuDialogLoopT::Update`, `src/game/Game/menudialogloopt_stubs.cpp:353`; in game
  `GameUpdateLoop`, `src/game/Game/gameupdateloop.cpp:1168`). The delta is the MainLoop's
  fixed-step time, so it is logical seconds (~wall while frames are short).
- **Gate:** `UpdateLoad` progress 2, `src/engine/save/saveload_update.cpp:815`:
  `if (_elapsed <= _minOperationTime) return;` -- the load proceeds (operation reset, then the
  dialog's `loadFlag` path builds the loop chain) only once 1.5 s have passed since the click.
  The save read itself finishes in ~14 ms, so `save.read.io-done` -> `save.read.done` is the
  remainder of the 1.5 s (1,465 ms in the census).

## 6. Gate (static, worktree)

After merging main `ecfef5a07`: `build debug`, `build perf`, `unit debug` at `--jobs 2`: exit 0;
**0 new warnings** vs the main checkout's `warn_baseline_{debug,perf}.txt` (debug 39 = 39, perf
107 = 107, clean builds, worktree prefix mapped); unit **1184 / 0**; `castmetric.py src` **2
before the merge, 4 after it** -- the two extra casts are main's, not this block's
(`src/diag/load_time.cpp:65` static_void and `src/game/Game/gameupdateloop_load.cpp:1227`
static_cross, both from main's loadkit / `[WorldHash]` commit; `--detail` lists no file of this
block); `abi_audit.py` OK; `acceptance.sh` 0 off target. No type header changed layout (FileSystem /
ExternalFactory untouched; the counter is a free inline variable).

## 7. Expected saving and what the coordinator must check

Expected, perf exe, Gate.sav: AdjustDelimiters + GetLength become one or two short passes per
call (most of the 37% + 26%, overlapping); on a memo hit ExpandRelativePath is a hash lookup and
one allocation instead of ~25 (`[PreloadRefs]` makes the first request for every referenced name,
so nearly every entity reference hits). **`ser.entities` ~785 -> ~250-350 ms**; `[PreloadRefs]`
(inside `ser.core`) gets the AdjustDelimiters share too. Every other FindOrLoad route (boot, new
game: ~22k calls per run) gets the AdjustDelimiters / GetLength share and the memo for repeated
requests made outside an .rsc parse.

The coordinator checks:
1. Resave gate 5/5 == golden `971aaec2cf95` (23,453,453 bytes); `[ExtLoad] calls=5694 / 0`,
   tutorial 5773 / 12, `[RefTable]` counts and typetable `965b527a` unchanged.
2. **`grep -c "\[ExpandMemo\] MISMATCH" standalone_error.log` = 0** on the debug gate runs (new
   game, tutorial, resave): the debug exe compared every memo hit with the body.
3. World identity kit (`loadkit/hash.sh`) unchanged -- only path strings are touched, but cheap.
4. Census `[LoadTime]` on the perf exe: `ser.entities` and `ser.core` (the preload) for Gate.sav;
   `loader.resources` on both routes for the AdjustDelimiters share.
5. One mods-mounted save load if available (the remount empties the memo; the bubble path in
   UpdateLoad is unverified end to end, LOADRESEARCH / saveload_update.cpp banner).

Open: the remount skip (§4) and the floor (§5) are not this block's.
