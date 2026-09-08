# Wave 9 lane V2 — driving `rawdoor_audit.py` to 0 UNGUARDED-CALL

2026-09-07, branch `wave/upgrade-path-complete`, worked on top of `f53f887`.

Companion to [WAVE9_LANE_V.md](WAVE9_LANE_V.md), and deliberately the *other*
half of the same question. Lane V triages the measured `[RtRawResidue]` rows —
"what did a run actually ASK the DLL for", which needs execution and therefore
misses everything the harness never reaches. This lane works the SOURCE
checker — "what WOULD ask, if reached", which needs no execution and therefore
misses nothing but proves nothing about reachability. Where the two disagree,
the source reading wins **for cutover purposes**: an unresolvable import is
fatal whether or not anything calls it.

## Result

| | before | after |
|---|---|---|
| `rawdoor_audit.py` UNGUARDED-CALL | **28** | **0** |
| raw-door sites scanned (the DENOMINATOR) | **555** | **519** |
| DEFERRED-CALL | 77 | 72 |
| `port_bypass_audit.py` ADDRESS door | 0 | 0 |
| `port_bypass_audit.py` NAME door | **3 BYPASS** | **0** |
| `rawdoor_audit.py --self-test` | green | green |
| `port_bypass_audit.py --self-test` | green | green |

Both audits exit 0.

### The denominator drop is fully accounted for, to the site

The orchestrator's brief quoted 27 UNGUARDED-CALL on a 550-site denominator
(lane H's hardened run). This lane measures **28 on 555** at `f53f887`. The
difference is the tree moving under both of us, not a disagreement about
method — the same lesson as the `Timer::SecondsToTicks` regression:
**re-run after every LANDING, not after every edit.**

555 − 519 = **36**, and this lane converted **exactly 36** raw-door call sites
from `RT_ORIGINAL_ONCE` / `RtOriginal<>` to `RT_EXPORT_ONCE`. A converted site
leaves the raw-door population by construction, so the denominator *must* fall
by exactly the conversion count. It does:

| file | conversions |
|---|---|
| `runtime/src/audio/audio.cpp` | 4 |
| `runtime/src/io/compressedstream.cpp` | 8 |
| `runtime/src/io/filesystem.cpp` | 2 |
| `runtime/src/io/filesystem_win.cpp` | 3 |
| `runtime/src/save/saveload.cpp` | 9 |
| `runtime/src/system/value_types.cpp` | 5 |
| `runtime/src/ui/widgets_layout.cpp` | 4 |
| `runtime/src/video/device.cpp` | 1 |
| **total** | **36** |

This check is not ceremony. Twice in this project a checker's own count fell
because a change *hid* a site rather than fixing it
(`reference_documenting_a_check_can_delete_it.md`, 166 → 162 reading as a clean
pass), and every one of the 36 conversions here also rewrote a comment block
next to the site. An unexplained drop of even one would mean a site went dark.

The DEFERRED-CALL drop of 5 is the same 36 seen from the other side: five of
the converted sites stored their pointer in a local (`value_types.cpp` ×2,
`compressedstream.cpp`'s `Create`/`Open`, `filesystem.cpp`'s `ours = …`), which
is the accessor shape the deferred pass tracks.

## The verdict table — all 28

Verdicts are ORACLE-ONLY / REAL DEPENDENCY / DATA READ / DEAD, per function,
with the evidence. **DATA READ: zero rows** — every one of the 28 was a call,
not an exported data symbol, so `standalone/rt_data_mint.*` has no bearing on
any of them. **DEAD: zero rows** — nothing here could be *proved* unreachable,
and this lane declines to assert it (see "could not resolve", item 1).

| # | site (pre-edit line) | symbol | verdict | evidence | action |
|---|---|---|---|---|---|
| 1 | `audio/audio.cpp:105` | `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` | REAL DEPENDENCY | file-local `MemAlloc()` helper; every caller is a live ported body allocating. Nothing diffs it. | → `RT_EXPORT_ONCE` |
| 2 | `audio/audio.cpp:108` | `?MemFree@…@SAXPEAX@Z` | REAL DEPENDENCY | as 1, the matching free. | → `RT_EXPORT_ONCE` |
| 3 | `io/compressedstream.cpp:189` | `??0CompressedStream@IO@@QEAA@XZ` | REAL DEPENDENCY | `RtIo_CompressedMemoryStream_Ctor` constructing its base sub-object. The oracle in the same function is separate and already guarded (scratch-buffer ctor at :235). | → `RT_EXPORT_ONCE` |
| 4 | `io/compressedstream.cpp:191` | `??0MemoryStream@IO@@QEAA@XZ` | REAL DEPENDENCY | same ctor, the `+0x68` member sub-object. | → `RT_EXPORT_ONCE` |
| 5 | `io/compressedstream.cpp:311` | `?Commit@CompressedStream@IO@@QEAAXXZ` | REAL DEPENDENCY | dtor's inlined `Close()`; flushes the pending windows. Not compared against anything. | → `RT_EXPORT_ONCE` |
| 6 | `io/compressedstream.cpp:314` | `??1MemoryStream@IO@@UEAA@XZ` | REAL DEPENDENCY | dtor tearing down the member sub-object. | → `RT_EXPORT_ONCE` |
| 7 | `io/compressedstream.cpp:315` | `??1CompressedStream@IO@@UEAA@XZ` | REAL DEPENDENCY | dtor tearing down the base sub-object. | → `RT_EXPORT_ONCE` |
| 8 | `io/compressedstream.cpp:413` | `?Commit@CompressedStream@IO@@QEAAXXZ` | REAL DEPENDENCY | `…_Close`'s commit; the save's compressed bytes come from it. | → `RT_EXPORT_ONCE` |
| 9 | `io/filesystem_win.cpp:123` | `?SetText@Error@System@@SAXPEBD_N@Z` | REAL DEPENDENCY | `ComputeSaveDirectory`'s failure arm reports the error retail reports. | → `RT_EXPORT_ONCE` |
| 10 | `io/filesystem_win.cpp:130` | `?Set@WideString@System@@QEAAXPEB_W@Z` | REAL DEPENDENCY | live body; wraps the `SHGetFolderPathW` result. | → `RT_EXPORT_ONCE` |
| 11 | `io/filesystem_win.cpp:133` | `?AsUTF8@WideString@System@@QEBA?AVString@2@XZ` | REAL DEPENDENCY | live body; produces the string that is formatted into the return value. | → `RT_EXPORT_ONCE` |
| 12 | `save/saveload.cpp:125` | `?MemAlloc@…` | REAL DEPENDENCY | file-local allocation helper, live path. | → `RT_EXPORT_ONCE` |
| 13 | `save/saveload.cpp:128` | `?MemFree@…` | REAL DEPENDENCY | as 12. | → `RT_EXPORT_ONCE` |
| 14–18 | `save/saveload.cpp:663,666,669,672,675` | `?UpdateList@` / `?UpdateLoad@` / `?UpdateLoadFromFile@` / `?UpdateSave@` / `?UpdateDelete@SaveLoad@Save@@QEAAXXZ` | REAL DEPENDENCY | this five-way switch **is** `SaveLoad::Update`'s work; the file's own header says in as many words that the shadow oracle does **not** check the dispatch. | → `RT_EXPORT_ONCE` ×5 |
| 19 | `save/saveload.cpp:808` | `?Open@MemoryStream@IO@@QEAA_NH@Z` | REAL DEPENDENCY | `SavedState::Retrieve`'s live allocation of the output stream; the oracle is the separate guarded scratch-object replay at :834. | → `RT_EXPORT_ONCE` |
| 20 | `system/value_types.cpp:312` | `?DateAsString@DateTime@System@@QEBA?AVString@2@XZ` | REAL DEPENDENCY | `DateTime::AsString` formats its two results; unported. | → `RT_EXPORT_ONCE` |
| 21 | `system/value_types.cpp:314` | `?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ` | REAL DEPENDENCY | as 20. | → `RT_EXPORT_ONCE` |
| 22 | `system/value_types.cpp:396` | `?Release@String@System@@AEAAXXZ` | REAL DEPENDENCY **and a live port BYPASS** | `String::Release` is REGISTERED at `system/string.cpp:1113` (lane B3). `port_bypass_audit` named this row. | → `RT_EXPORT_ONCE` |
| 23 | `ui/widgets_layout.cpp:84` | `?RemoveChildren@Element@UI@@QEAAXXZ` | REAL DEPENDENCY | `RtRemoveChildren` helper, called by live ported bodies. | → `RT_EXPORT_ONCE` |
| 24 | `ui/widgets_layout.cpp:94` | `?CacheText@TextElement@UI@@AEAAXXZ` | REAL DEPENDENCY | live helper; reformats the cached display string. | → `RT_EXPORT_ONCE` |
| 25 | `ui/widgets_layout.cpp:101` | `?UpdateContainers@TabElement@UI@@AEAAXXZ` | REAL DEPENDENCY | live helper; recomputes the visible page. | → `RT_EXPORT_ONCE` |
| 26 | `ui/widgets_layout.cpp:114` | `?MemFree@…` | REAL DEPENDENCY | live helper. | → `RT_EXPORT_ONCE` |
| 27 | `ui/widgets_layout.cpp:741` | `?RemoveGraphs@GraphElement@UI@@QEAAXXZ` | **ORACLE-ONLY** | see below — the one row that is not a conversion. | **wrapped in `if (rt_diff_on__)`; stays RAW** |
| 28 | `video/device.cpp:253` | `?GetDefaultResolution@Device@Video@@QEBAHAEBUAdapter@12@@Z` | REAL DEPENDENCY | `CallGetDefaultResolution` has **one** caller — `ValidateConfigurationBody`'s resolution fallback, which is the LIVE ported body, not the oracle. | → `RT_EXPORT_ONCE` |

### Row 27 is the interesting one, and it is a third instance of lane V's defect

`GraphElement_RemoveGraphs` **is** the registered port of
`?RemoveGraphs@GraphElement@UI@@QEAAXXZ` (see `widgets_layout.cpp`'s
registration block). Its `RT_ORIGINAL_ONCE` call to that same name sat *between*
`RT_DIFF_BEGIN` and `RT_DIFF_SCALAR` — outside any guard — so on a normal,
non-diffing run it called retail's whole `RemoveGraphs` on every invocation.
That is exactly the shape lane V found twice by hand and `rawdoor_audit.py` was
written to catch. This is the third.

It gets the guard, not the conversion, and the reason is worth recording
because it is the **counter-example to this lane's own rule**:

> An oracle for a port of X is the one case where the raw door is the only
> correct door.

Converting row 27 to `RT_EXPORT_ONCE` would resolve `?RemoveGraphs@…` through
the port registry, find **this function**, and recurse until the stack died. A
blanket conversion pass would have shipped that.

### Deliberately left RAW, and why

* **Row 27**, above — self-recursion. Guarded instead, so it is gated out at
  cutover with the rest of the diff machinery.
* **`Audio::Configuration::Save`'s readback** (`audio/audio.cpp`, the
  `RT_ORIGINAL_ONCE` beside the `getFn`). Not in the 28 and not touched. Its
  comment already records that **the write is the work and the readback is the
  ORACLE**, and its neighbouring `setFn`/`getFn` are already `RT_EXPORT_ONCE`.
  Converting the readback would make the audit greener and destroy the oracle.
  Re-read and left exactly as it is.
* **Every `RtOriginal<>` inside `if (rt_diff_on__)`** across all eight files —
  `filesystem_win.cpp:171`, `filesystem.cpp`'s `RT_DIFF_SCALAR` argument,
  `compressedstream.cpp`'s scratch ctor/dtor, `saveload.cpp`'s twelve guarded
  oracles, `value_types.cpp`'s `RtDiffRandomShadow`. These are ORACLE-ONLY and
  correct as they stand: they must reach the SHIPPED body, and `RtDiffEnabled()`
  gates them out at cutover.

## The checker hole this lane hit twice, then measured

`rawdoor_audit.py`'s resolve-and-call regex (`RESOLVE_CALL`) runs on ONE SOURCE
LINE at a time. When the argument list opens on the *following* line —

```cpp
const bool ok = RT_ORIGINAL_ONCE(
    CsCreateFn, "?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z")(
    &self->base, &self->memoryStream, 2);
```

— the site is a live unguarded call and the checker does not see it. This lane
found two of them by *reading files it was already editing*
(`compressedstream.cpp`'s `Create`/`Open`), which is the least reliable way to
find anything, so the population was measured instead rather than reasoned
about: a paren-matching re-scan over all five roots
(`docs/runtime_port/wave9_laneV2_wrapscan.py`), taking guard state from
`rawdoor_audit`'s **own** per-line map so a hit is unguarded by exactly the
audit's rule and not by a second opinion.

**Result: 364 immediate-call raw sites tree-wide, 83 with the argument list on a
later line, of which 4 were UNGUARDED and invisible to the audit.** All four are
live-path REAL DEPENDENCIES and are converted:

| site | symbol | why it was invisible |
|---|---|---|
| `audio/audio.cpp:451` | `?FindFree3DEffect@Scene@Audio@@…` | mangled name on its own line |
| `audio/audio.cpp:760` | `?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z` | args on the next line |
| `io/filesystem.cpp:139` | `?ExistsInPackfile@FileSystem@IO@@…` | args on the next line |
| `io/filesystem.cpp:145` | `?PlatformExists@FileSystem@IO@@…` | args on the next line |

Plus two more inside files this lane was already converting, hidden by the same
hole: `compressedstream.cpp`'s `Create`/`Open`, and `saveload.cpp:582`'s
`?GetDataDirectory@FileSystem@IO@@…` — the last of which sits two lines above
two sites lane R *did* convert, because the checker flagged those and not this
one. That is the hole doing real damage, not a hypothetical.

Re-run after the edits: **0 wrapped-unguarded remain** (79 wrapped sites, all
guarded). The 36-site conversion total above includes all six of these, which
is why 36 > 28.

**Follow-up for whoever owns `rawdoor_audit.py`** — NOT done here, this lane
owns call sites and not the checker: the fix is to paren-match forward from the
resolve over the joined text instead of matching `\)\s*\(` per line, exactly as
the deferred pass already crosses statements. Until then the audit's
UNGUARDED-CALL count is a floor, and today's 0 means "0 of the shape it can
see" plus this lane's separate re-scan.

## What was checked by hand, because a clean audit is a floor

`port_bypass_audit.py`'s known blind spot means a hand-grep of every mangled
name this lane touched is mandatory. Doing it turned up one disposition worth
writing down.

**The remaining raw `MemAlloc`/`MemFree` sites are NOT port bypasses, and the
reason is a DETOUR, not a flag.** Eight raw-door sites still name those two
symbols (`object/internal.cpp:110/116`, `system/string.cpp:69/75`,
`ui/sprite.cpp:143/482/483`, `video/buffers.cpp:82`). Both names ARE registered
(`system/allocator.cpp:1088/1090`), so the obvious reading is "eight more
bypasses the checker cannot see". It is wrong: that registration happens only
after `g_installDetours` has **patched the DLL's own `MemAlloc`/`MemFree`
prologues**, and the code refuses to register at all if the detours do not take.
So when the allocator is armed, a raw-door call lands on the DLL's entry point
and is detoured into our allocator anyway. There is no half-heap here, and the
eight are DEFERRED-CALL rows whose only remaining problem is cutover nullness —
a different gate. They are left alone on that evidence, rather than on silence
from the checker.

The three `String::Release` rows were the opposite case and were real: nothing
in `value_types.cpp` changed to create them; lane B3 landed the port elsewhere
and turned three correct raw calls into bypasses with that file untouched. That
is the regression the gate row predicts in as many words, observed.

## Could not resolve — named, not buried

1. **Reachability is not established for any row.** This lane's evidence is
   structural (which function encloses the call, what the enclosing body does),
   never "a run executed it". Four rows have no execution evidence here at all:
   `filesystem_win.cpp`'s error arm (needs `SHGetFolderPathW` to fail),
   `video/device.cpp`'s `GetDefaultResolution` fallback (needs a resolution the
   adapter does not offer), and `audio.cpp:760`'s `Device::Stop` loop. For
   **cutover** that does not matter — an unresolvable import is fatal
   regardless — but nobody should read this table as "all 28 were live". Lane
   V's residue rows are the complement and should be read alongside it.
2. **Nothing here was run.** No build, no deploy, no game (another lane owns
   `C:/GOG Games/Banished`). Verification is: both audits at 0 and exit 0, both
   self-tests green, the wrap re-scan at 0, and `g++ -fsyntax-only` clean on all
   eight files. **The 36 conversions have not been executed once.** Twelve of
   them (`saveload`'s dispatch, `compressedstream`'s ctor/dtor/Commit) sit on
   the save/load path, which is this project's most-cited silent-failure
   surface. Every one of the 36 resolves a name that is either UNPORTED (so
   `ResolveExport` returns the same DLL pointer `ResolveExportRaw` did) or
   detour-backed, so no behaviour should change — but "should" is doing work in
   that sentence, and an autoload / save9 cell would settle it.
3. **`rawdoor_audit.py` has not been fixed**, only measured — see the follow-up
   above. Its UNGUARDED-CALL number is a floor until it paren-matches.
4. **Rows 14–18 now depend on another lane's port arming.** The `SaveLoad`
   dispatch routes through the registry, so the day someone registers
   `?UpdateSave@SaveLoad@Save@@…` the save path silently starts using it. That
   is the correct behaviour at cutover and it is also a live wire:
   `saveload.cpp`'s header argued the opposite for eight waves, for reasons that
   were good at the time. Those reasons are **withdrawn in the file** rather
   than deleted, so the next reader sees the argument and its reversal together.
