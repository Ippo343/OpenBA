# Wave 4, lane D — IO::FileSystem / IO::Modification

Following [PLAN.md](PLAN.md) §4.2 and §6. Lane D owns the 7 `IO::FileSystem` /
`IO::Modification` symbols our tree calls (the brief's list), plus their
layouts.

Files owned and written by this lane:

| File | What |
|---|---|
| `runtime/types/io/FileSystem.h` | new — `FileSystem`, `PackfileIndexNode`, `PackageFileView` |
| `runtime/types/io/Modification.h` | new — `Modification`, `Mod` |
| `runtime/src/io/filesystem.cpp` | 5 portable ports + `RtIoFileSystemRegister()` |
| `runtime/src/io/filesystem_win.cpp` | new — the one Win32-dependent port (`GetSaveDirectory`) + `RtIoFileSystemWinRegister()` |
| `docs/runtime_port/WAVE4_LANE_D.md` | this file |

No edits outside that set. The "COMMENTS TO CORRECT" section at the end lists
what needs fixing at its source, for the orchestrator to apply.

---

## 1. Result

| | |
|---|---|
| Ported | **6 of 7** — `Exists`, `GetDataDirectory`, `GetSourceDirectory`, `GetSaveDirectory`, `GetPackfileId`, `Modification::Find` |
| Held back | `?_singleton@FileSystem@IO@@0PEAV12@EA` — **DATA symbol, cannot be registered**, confirmed (see §5) |
| Registration count | `RtIoFileSystemRegister()` returns **6** (5 in `filesystem.cpp` + 1 in `filesystem_win.cpp`) |
| Diff sites added | **6** — 4 full oracles, 2 value oracles (documented) |
| Measured in a run | **nothing yet** — the integration run is the orchestrator's; every claim below is static (decompile + disassembly + syntax-checked compile), not observed |
| Syntax check | `ccache g++ -std=c++17 -fsyntax-only -Wall -Wextra …` clean on both `.cpp` files (the exact command in the brief) |
| `tools/abi_retslot_audit.py` | **0 suspicious of 49** (unchanged from before this lane; see §2) |
| `tools/export_name_audit.py` | none of this lane's 7 names appear in its phantom-name list |

**Nothing in this lane has executed.** No build, no `sa-run` — those are the
orchestrator's per the brief. Read every claim below as "this is what the DLL
does and this is what the port does," never as "this was observed agreeing."

---

## 2. The 7 symbols — verification and hidden-return-slot audit

Export-name verification for the whole wave was done centrally by the
orchestrator and is authoritative: `docs/runtime_port/wave4_export_names.txt`
(`pefile` over the shipped `Runtime-x64.dll`, all 39 IO symbols the wave
calls, 0 MISSING). This lane's own independent pefile pass before writing any
code (below) agrees with it exactly, including that `_singleton`'s `0PEAV12@EA`
mangling — which an earlier draft of this section second-guessed — is the
correct, present export:

```
OK  0x1800ab310 ord=2321  ?Exists@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z
OK  0x180007bd0 ord=2554  ?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ
OK  0x1800acfd0 ord=2886  ?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ
OK  0x180007b90 ord=2922  ?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ
OK  0x1800ab050 ord=2829  ?GetPackfileId@FileSystem@IO@@QEBAIAEBVFilename@System@@@Z
OK  0x1800aea10 ord=2339  ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z
OK  0x1801fc6a8 ord=4811  ?_singleton@FileSystem@IO@@0PEAV12@EA
```

The two dependent helpers `Exists` tail-calls (not in this lane's 7-symbol
scope, called through by resolved export — see §3) were verified too:

```
OK  0x1800ab180 ord=2322  ?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z
OK  0x1800adc70 ord=3378  ?PlatformExists@FileSystem@IO@@AEBA_NAEBVFilename@System@@@Z
```

`Modification::_singleton`'s mangling — not ported (out of this lane's
7-symbol scope, and DATA regardless, §5) — is
`?_singleton@Modification@IO@@2PEAV12@EA` (digit `2`, protected static, not
`0`/private static like `FileSystem`'s); recorded so nobody chases a
different string later.

**No export folding this wave** (checked centrally, confirmed against this
lane specifically): none of the 7 addresses is exported under more than one
name, including `GetDataDirectory` (`0x180007bd0`) and `GetSourceDirectory`
(`0x180007b90`), which sit only `0x40` apart and share a tail-call target
(§3) but are separate bodies — each was decompiled and disassembled
individually rather than assumed identical from proximity.

### Hidden return slots — three, all already correct

`GetDataDirectory`, `GetSourceDirectory` and `GetSaveDirectory` all end
`?AVFilename@System@@XZ` — `Filename` returned **by value** — so each has an
RDX hidden-return-slot argument (`this` in RCX, retval in RDX). Checked both
existing typedef families:

* **`engine/funcs/engine_funcs.h`'s central typedefs** — `fn_FileSystem_GetDataDirectory_t`
  (line 1251) and `fn_FileSystem_GetSourceDirectory_t` (line 3601) are both
  already `void*(*)(void* self, void* retval)`. **Correct**, no wild write.
* **The local typedef in `code/Game/Game/w2_takescreenshot.cpp:97`**
  (`FS_GetSaveDirectory_t`) is `void*(*)(const void* self, void* retFilename)`.
  **Correct.**

`tools/abi_retslot_audit.py` reports **0 suspicious of 49** before and after
this lane's changes (the tool audits `engine/funcs/engine_funcs.{h,cpp}`
resolves, which is unaffected by files under `runtime/src/`) — consistent
with the by-hand check above, not a substitute for it.

**One live but pre-existing defect found in the audit, not caused by this
lane**: `engine/funcs/engine_funcs.h:1249`'s comment names the WRONG mangled
string for `GetDataDirectory` — see §7.

None of `Exists`, `GetPackfileId` or `Modification::Find` returns by value
(`_N` bool, `I` unsigned, `H` int), so none has a hidden slot.

---

## 3. The six ports

All six are read-only walks of the LIVE DLL structures at their real
addresses — nothing here keeps a replica. No detour is installed (as Waves 1–3):
registering redirects only our call sites, which is safe for a read-only walk.

### `IO::FileSystem::Exists` — retail `0x1800ab310`

```
if (existsInPackfileGate) if (ExistsInPackfile(path)) return true;
if (!platformExistsGate) return false;
return PlatformExists(path);
```

`ExistsInPackfile` (`0x1800ab180`) and `PlatformExists` (`0x1800adc70`) are
exported but **not in this lane's 7-symbol scope** — our tree never calls
either directly, only through `Exists`. They are called by resolved export
(`RtOriginal`), exactly as Wave 3's `Type::Serialize` dispatched into the
DLL's out-of-scope `ReaderWriter` vtable. That is a real transcription of
`Exists`'s own gate-reading logic, not a forwarder.

**Full oracle.** Read-only on both sides (`PlatformExists` stats a file — no
mutation).

### `IO::FileSystem::GetDataDirectory` / `GetSourceDirectory` — retail `0x180007bd0` / `0x180007b90`

Both are single tail-calls into the SAME address, **confirmed by
disassembly, not assumed from the decompiler's `Filename::Filename(...)`
label**:

```
180007bd0: LEA RDX,[RCX+0x8]; MOV RCX,RBX(=retval); CALL 0x1800067d0   ; GetDataDirectory
180007b90: MOV RDX,RCX(=this); MOV RCX,RBX(=retval); CALL 0x1800067d0 ; GetSourceDirectory
```

`0x1800067d0` is `??0Filename@System@@QEAA@AEBV01@@Z` — Wave 2 lane A's
already-ported `RtSystem_Filename_CtorCopy` (`runtime/src/system/filename.cpp:81`).
So `GetDataDirectory` is `Filename(this->dataDirectory)` (an AddRef of
`FileSystem+0x08`) and `GetSourceDirectory` is `Filename(*this)` — the class's
OWN first field reinterpreted as a `Filename`, not a nested lookup. That is
only possible because `FileSystem+0x00` really is a String/Filename-shaped
object; `FileSystem.h` models it as one.

**Full oracle**, `RT_DIFF_BYTES` on the resulting 8-byte `Filename`. The copy
ctor only AddRefs a shared buffer, so both results necessarily share the same
`data` pointer when correct — proving our AddRef landed on the DLL's own live
buffer, not a stale or wrong one. Each diff-mode call releases its own copy
afterward so diffing never leaks a reference.

### `IO::FileSystem::GetPackfileId` — retail `0x1800ab050`

The most involved of the six. Only proceeds when the query path is longer
than `dataDirectory` (retail substitutes a LENGTH check for an actual prefix
compare — it trusts the caller passes a full path under `dataDirectory`).
Builds the relative substring (`pathData + dataDirLen`, a suffix of the SAME
buffer, not yet a copy), normalises it with `Filename::AdjustDelimiters`
(Wave 2's port, reused), then walks the `PackfileIndexNode` tree at
`FileSystem+0x40` with a case-insensitive two-sided comparison (transcribed
exactly — see the code comment for the `condA`/`condB` derivation from the
raw decompile, which is not a standard three-way BST compare written the
obvious way). On a hit, returns `packfiles[node.packIndex]->packfileId`
(`PackageFile+0xA8`); on a miss, or if the length check fails, returns 0 —
retail's own "in no package" sentinel, not an added guard.

Reuses `RtSystem_String_Set` (Wave 2, already ported) for the substring copy
and duplicates a small `StriCmpAscii` helper rather than sharing
`filename.cpp`'s file-local one or calling the CRT's `_stricmp` — same
portability reasoning `filename.cpp` itself gives (PLAN §4.1: `runtime/src`
must not depend on a CRT whose locale behaviour differs across toolchains).

**Full oracle.** Read-only on both sides.

### `IO::Modification::Find` — retail `0x1800aea10`

Linear scan of the mod array (stride `0x80`, filename at `+0x58` — both
already documented by `code/Game/Dialog/mod_dialog_vtables.cpp`'s
`ModProbe_DumpModList`, this lane's header just adds the `static_assert`s),
comparing stored length first and only then `StriCmpAscii`. Returns the index
of the first match or `-1`.

**Full oracle.** Read-only on both sides.

#### Not-found behaviour and the mod-mismatch load stall

Asked to check specifically, given the user-reported stall when a save is
loaded with a mod that was ENABLED at save time but is now DISABLED/removed:

* **`Find` returns `-1`** when the queried `Filename` matches no mounted
  mod — the initialised `ours = -1` never changes if the loop runs to
  completion. Retail's own sentinel, transcribed exactly.
* **`Find` has ZERO internal callers inside `Runtime-x64.dll`.**
  `get_xrefs_to(0x1800aea10)` in the retail DLL returns only a `.data` table
  entry and the export-table entry-point reference — no `CALL` site anywhere
  in the DLL's own code. This means **the DLL's own save/load path does not
  call `IO::Modification::Find` at all**; whatever function actually compares
  a save's recorded mod list against the currently mounted mods (the
  mechanism lane H is chasing) is a *different* function. This lane's port —
  registration-only, no detour — could not be the cause of that stall even in
  principle, because nothing inside the DLL is redirected to it.
* **The only caller in OUR tree is `code/Game/Dialog/moddetailsdialog.cpp`**
  (two sites, lines 225 and 288), building the "conflicts with other
  installed mods" list for the mod-details dialog. Both pass the result
  **completely unguarded** into `fn_Modification_GetMod(modification, cIdx)`
  — i.e. `?GetModification@Modification@IO@@QEBAAEBUMod@12@H@Z` (retail
  `0x18000da80`), whose entire body is:
  ```
  Mod* GetModification(int idx) const { return mods + idx * 0x80; }
  ```
  No bounds check on either end, in retail. A `-1` result therefore produces
  a wild pointer `0x80` bytes *before* the mods array, which the caller then
  reads as a `Mod` (name pointer, enabled byte at `+0x50`, BST root at
  `+0x60`) — whatever heap bytes happen to precede the array, interpreted as
  those fields. That is retail's own faithful (if fragile) behaviour, not
  something this lane added or should guard.
* **A normal `sa-run` will not exercise the `-1` path here.** Both call sites
  look up mods from `m+0x70`, the CURRENT mod's own recorded conflict list —
  by construction, a mod's conflict metadata should already reference mods
  that exist in the mount table (they were discovered as actual conflicts at
  scan time). `-1` would only occur if a mod's conflict list names another
  mod that has since been uninstalled — an edge case, and one this lane's
  diff oracle will only ever see if the orchestrator specifically constructs
  that scenario (open the mod-details dialog for a mod whose declared
  conflict is no longer installed).

**Bottom line for lane H**: `Modification::Find` is very unlikely to be the
mechanism behind the save/mod-mismatch stall — it is not called from
anywhere in the DLL's own code, only from our mod-details dialog UI. The
stall almost certainly lives in a different function (a save-time mod-list
comparison inside `Save::SaveLoad` or `IO::Persistence`, neither owned by
this lane).

### `IO::FileSystem::GetSaveDirectory` — retail `0x1800acfd0` (`filesystem_win.cpp`)

The one Win32-dependent port. Retail loads `this` (RCX) but **never
dereferences it** — confirmed in the disassembly — so this function reads NO
`FileSystem` state at all; it queries `SHGetFolderPathW(NULL, CSIDL_PERSONAL,
NULL, 0, buf)`, converts to UTF-8 through `System::WideString`, formats
`"%s\\%s\\"` with `System::String::Format` (Wave 2, already ported, reused
directly), and AddRefs the formatted buffer directly into the return slot
(not a copy-construct — retail's own idiom, transcribed).

`System::WideString` and `System::Error::SetText` are **not ported** (out of
this lane's 7-symbol scope; `PLAN.md` §5 explicitly excluded `WideString` from
Wave 2 as "not called by our tree" — this is the first caller). Both are
called through resolved exports, same principle as `Exists` calling into
`ExistsInPackfile`/`PlatformExists`. The three inlined buffer releases in the
decompile are byte-identical to `System::String`'s own release idiom
(refcount at `data-4`, free at `data-8`), so they are reused via
`RtString_ReleaseInline` with a `reinterpret_cast`, rather than re-deriving a
WideString-specific release this lane does not own.

**Value oracle, not a byte oracle.** `GetSaveDirectory` allocates a FRESH
buffer every call (unlike the two AddRef-based getters above), so `ours` and
`theirs` never share a pointer — comparing raw struct bytes would report a
"divergence" on every single call even when both sides are correct. The site
compares by TEXT (`strcmp`), same shape as `Filename::GetPathFilename`'s
oracle in `runtime/src/system/filename.cpp`.

---

## 4. What each diff site can and cannot prove

| Site | Kind | Proves | Does not prove |
|---|---|---|---|
| `IO::FileSystem::Exists` | **full** `RT_DIFF_SCALAR` | the two-gate short-circuit logic, end to end | the internals of `ExistsInPackfile`/`PlatformExists` themselves (unported, called through) |
| `IO::FileSystem::GetDataDirectory` | **full** `RT_DIFF_BYTES` | the AddRef landed on the live `dataDirectory` buffer | — |
| `IO::FileSystem::GetSourceDirectory` | **full** `RT_DIFF_BYTES` | the AddRef landed on the live `sourceDirectory` (= `this`) buffer | — |
| `IO::FileSystem::GetPackfileId` | **full** `RT_DIFF_SCALAR` | the length-gate, substring, normalisation and tree walk all agree, end to end, including the CRC-free comparison path | — |
| `IO::Modification::Find` | **full** `RT_DIFF_SCALAR` | the linear scan and length-checked compare | — |
| `IO::FileSystem::GetSaveDirectory` | **value oracle** (text compare, not byte compare) | the formatted path text is identical | pointer identity (deliberately not compared — see §3) |

All six report a real `calls=` count once exercised — none is a
`Type::Serialize`-style "executed, not diffable" site (§4 of WAVE3_LANE_A.md).

---

## 5. `?_singleton@FileSystem@IO@@0PEAV12@EA` — DATA symbol, held back

Confirmed with the same `pefile` check used for every other symbol: it is
listed in the export table with **no associated code** (a data export,
`ord=4811`, address `0x1801fc6a8`). Wave 3's reasoning for
`Object::Factory::_singleton` (WAVE3_LANE_A.md §7/§10) holds unchanged here:
a data symbol is a variable's ADDRESS, not a callable — there is nothing for
`RegisterRuntimeExport`/`RegisterRuntimeAddr` to redirect. It is **not**
registered, and should not be.

This does not block anything the lane needs: every one of our ~20 existing
call sites that dereferences the singleton
(`g_pFileSystemSingleton`/`g_pFileSystemSingleton2` in `engine_funcs.cpp`,
`code/Game/Game/filesystem_setup.cpp`, `code/Game/Dialog/mod_dialog_vtables.cpp`)
keeps reading the DLL's live `FileSystem` object by address, which is correct
and required until `FileSystem`'s OWNER (construction/destruction/the
`SetupFileSystem`/`LoadPackages` mutators) is ported — none of which is in
this lane's scope. `IO::Modification::_singleton`
(`?_singleton@Modification@IO@@2PEAV12@EA`, `0x1801fc6b0`) is the same shape
and likewise out of scope.

---

## 6. What a normal `sa-run` actually reaches

Per the brief's warning 4 (read `calls=`, not just `divergences=`), here is
what each site's caller actually is, from a repo-wide grep — this goes
further than the brief's own file list, which named 5 callers; there are more:

| Function | Reached through | On the default autostart path? |
|---|---|---|
| `Modification::Find` | `fn_Modification_Find`, `code/Game/Dialog/moddetailsdialog.cpp:225,288` | **No** — only when the mod-details dialog is opened (`BANISHED_AUTOMENU_BUTTON=7` then a details click; see CLAUDE.md's Mods-menu example) |
| `FileSystem::GetDataDirectory` | `fn_FileSystem_GetDataDirectory`, `code/Game/Dialog/scenariodialog.cpp:501` and `code/Game/Tutorial/tutorial_next_scenario.cpp:292` | **No on the ordinary path** — `scenariodialog.cpp`'s call is in the "SAVED scenario" branch (loading a saved-state scenario), not the `fn_StartNewGame` branch autostart's New-Game click takes. Reachable via `BANISHED_AUTOTUTORIAL` or a saved-scenario click. |
| `FileSystem::GetSourceDirectory` | `fn_FileSystem_GetSourceDirectory`, `code/Game/Dialog/spritecaptureui.cpp:182` | **No** — sprite-capture UI is a debug-menu tool |
| `FileSystem::GetSaveDirectory` | local `ResolveExport` in `code/Game/Game/w2_takescreenshot.cpp:155` | **No** — only on a quicksave/quickload keypress (`GameUpdateLoop::TakeScreenshot`) |
| `FileSystem::Exists` | local `ResolveExport` in `code/Game/Game/w2_takescreenshot.cpp:156` (the screenshot-numbering loop) | Same as above — no |
| `FileSystem::GetPackfileId` | `code/Game/Dialog/mod_dialog_vtables.cpp`'s `ModProbe_TestPaths` (env-gated diagnostic, `BANISHED_MOD_PROBE`) | **No** on an ordinary run — only the diagnostic probe calls it directly today; every other consumer of package lookup goes through `Exists`'s internal `ExistsInPackfile`, which this lane does NOT port (§3) |

**Consequence for the integration run**: a bare `sa-run BANISHED_RT_DIFF=1`
autostart session should show `calls=0` on all six of this lane's sites
unless it also opens the mods dialog, presses quicksave/quickload, or drives
`BANISHED_AUTOTUTORIAL`/a saved-scenario load. That is not a sign the port is
inert — it is the honest shape of this call surface, and the orchestrator
should drive at least one of those paths (the Mods-menu harness in CLAUDE.md,
or a quicksave keypress) before concluding anything from `divergences=0`.

### The Wave 3 load-harness fix this wave was asked to check

PLAN.md §5's definition-of-done row said "**Wave 4 (IO) should fix
[the `BANISHED_AUTOLOAD` desync] first, since it lives entirely on that
path**." This lane's 7 symbols do not touch `Persistence`, `Serializer`,
`SaveLoad` or the `Stream` family — lanes A/B/C own those. **Not fixed by
lane D; still open**, and should not be read as addressed by this wave just
because it landed.

---

## 7. Comments to correct (outside this lane's files — not edited here)

Per PLAN §6 step 8 and the brief's instruction that files outside this lane's
ownership are for the orchestrator to edit, not this lane:

| File:line | Claim | Truth |
|---|---|---|
| `engine/funcs/engine_funcs.h:1249` | `// ?GetDataDirectory@FileSystem@IO@@QEBAAEBVString@System@@XZ` (a reference-returning `String` overload) | **Wrong mangled string.** The real (and only) export is `?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ` — returns a `Filename` **by value** (`?AV`, not `AEBV`), and `engine_funcs.cpp:2593` already resolves the CORRECT string; only the comment above the typedef is stale. The typedef signature itself (`void*(*)(void* self, void* retval)`) is accidentally still right either way, since both spellings need a hidden return slot — but the comment would mislead the next reader into believing a non-existent reference-returning overload exists. Replacement text for line 1249: `// ?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ` |

No other comment in this lane's dependency chain was found stale. The
`abstract`/file-extension misnomer from Wave 3 lane A and the PLAN.md §2.2/§2.3
corrections are unrelated to this lane's scope and are not re-litigated here.

---

## 8. Faithfulness decisions

* **No null guards were added.** `Exists`, `GetPackfileId` and `Find` all
  read fields unconditionally exactly as retail does; the one guard in
  `GetPackfileId` (the length comparison) is retail's own, not an addition.
* **`GetSaveDirectory` ignores its `this` parameter**, because retail's
  disassembly never dereferences RCX. Kept as a parameter anyway (byte-
  faithful signature), with the reason recorded in the source comment so
  nobody "cleans up" an apparently-unused parameter that is retail's own
  shape.
* **`StriCmpAscii` is duplicated, not shared**, between `filesystem.cpp` and
  `filename.cpp`'s file-local helper of the same shape. Both exist for the
  identical reason (`runtime/src` must not depend on the CRT's locale-
  sensitive `_stricmp` — PLAN §4.1's portability seam), and sharing it would
  require a header this lane does not own touching a file owned by a
  different lane's wave.
* **`GetPackfileId`'s two-sided `condA`/`condB` comparison is transcribed
  exactly**, not simplified to an obvious three-way compare, because the
  retail bytecode's redundant-looking double `stricmp` is exactly what it
  does; simplifying it risks silently changing behaviour on a tie.

---

## 9. What the integration run must look for

1. **`calls=` on each of the six sites** — expect **zero** on a bare
   autostart `sa-run` per §6; drive the Mods-menu harness or a quicksave
   keypress before reading anything into `divergences=0`.
2. **`GetPackfileId`/`Exists`/`Find` crashing at a fault address of `0x0` or
   small** would mean a `PackfileIndexNode`/`Mod` layout offset is wrong —
   symbolize with `nm` against `reimpl-x64.exe`.
3. **`GetSaveDirectory`'s value-oracle divergence**, if any, is the first
   place to suspect the `WideString`/`AsUTF8` release idiom reuse (§3) is
   wrong, since it is the one guess in this lane not backed by an existing
   Wave 2 port.
4. **Perf**: none of these six is hot (§6 — none is on the default per-frame
   path), so this lane should not move `sa-stress`'s number either way. If it
   does, that itself is a finding.

---

## 10. Findings that change other lanes' or other waves' assumptions

* **`FileSystem+0x00` is `sourceDirectory` and `+0x08` is `dataDirectory`**,
  both `System::String`-shaped (not `Filename`-shaped — retail never
  declares the field as one, only casts at the call site). This refines
  `code/Game/Dialog/mod_dialog_vtables.cpp`'s prior partial layout, which
  named `+0x08` but not `+0x00`.
* **`GetDataDirectory` and `GetSourceDirectory` both tail-call the exact same
  address, `0x1800067d0`** (Wave 2's `Filename` copy ctor) — confirmed by
  disassembly, which is a stronger claim than the decompiler's identical
  `Filename::Filename(...)` labels on their own would justify.
* **The `PackfileIndexNode` tree at `FileSystem+0x40` uses a non-obvious
  two-sided comparison**, not a standard three-way compare; any future lane
  reading or rebuilding that tree (e.g. `LoadPackages`, which this lane did
  NOT port) should start from the transcription in `filesystem.cpp`, not
  re-derive it from the pseudocode's surface reading.
* **`System::WideString` shares `System::String`'s exact allocation-header
  release idiom** (refcount at `data-4`, free at `data-8`), confirmed from
  `GetSaveDirectory`'s own decompile. Whichever lane eventually ports
  `WideString` should start from `runtime/types/system/String.h`'s existing
  documentation rather than re-deriving it.
