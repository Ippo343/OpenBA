# SETTINGS-2 hand-in (Phase D.4 wave 2, branch `d4-settings2`, base `c44b484e3`)

Plan: `plans/SETTINGS.md` steps E.3 to E.6, under the user's answers Q1 (one-shot launcher
entries honoured every boot), Q2 (import the registry once, read-only) and Q11 (the names).
Static gate only: the game, `sa-run` and `ach_reset.sh` against the real file were never run.
The registry and the real `settings.cfg` were never touched. `git merge main` (`d41216e27`)
was done before the final build; its one conflict is described under "Shared files".

## Commits

| commit | title | gate |
|---|---|---|
| `b118ab295` | behaviour: IO::Persistence settings in `<SaveDirectory>settings.cfg`, not the registry | debug 0 / perf 0 / unit 941 / 0 (+8 new); 0 new warnings; castmetric 2 |
| `04c96527a` | behaviour: harness guard and ach_reset.sh on settings.cfg | same; `bash -n` and a scratch-file test of the sed rule |
| `89ff62f70` | behaviour: the launcher's two check boxes are settings.cfg entries | same |
| `474aea373` | refactor: delete IO::Registry; settings docs | same; B11 below; acceptance.sh 0 off target |
| `d41216e27` | merge main (THREADS-1, ABI) | debug 0 / perf 0 / unit 953 / 0; 0 new warnings; castmetric 2; acceptance 0 off target; `abi_audit.py` OK |

**B11 for E.6** (perf objects of `89ff62f70` against `474aea373`, both built in this worktree;
every other object is byte-identical): IDENTICAL 71, SAME 2. The two SAME functions are
`GameSettings::Load` / `Save`, whose only change is the moved log string in `.rdata`.
E.3 to E.5 are `behaviour:` commits; each message quotes the retail call sites.

## What each commit does

- **E.3.** `src/engine/io/persistence_settings.cpp` holds ReadSettings (0x1800b84b0) and
  WriteSettings (0x1800b88e0). ReadSettings clears the map, then loads the file or the
  one-time read-only import, and inserts with retail's insert plus re-seat. WriteSettings
  walks the map in retail's order and MERGES it over the file. Retail's delete pass deleted
  nothing, so disk-only keys survive. The file layer is `src/engine/io/settings_store.{h,cpp}`
  (`DefaultPath`, `Load`, `Store`), which uses only the standard library and the platform
  seam. One `std::mutex` serialises writes. `persistence_registry_win.cpp` is deleted.
- **E.4.** In `src/app/main.cpp`, `DumpVideoSettings` and `HarnessGuardUnattendedBoot` work
  over the store. They make the same two repairs with the same log lines. When the file is
  absent, the guard imports, then writes the whole set plus the repair.
  `tools/ach_reset.sh` deletes the `^Ach[0-9]+=` lines. It refuses when the file is absent,
  because it may not touch the registry.
- **E.5.** PlatformConfigure reads `LauncherDisableMods` / `LauncherResetSettings` before
  the half split and sets both to false (one-shot). The WriteSettings each half makes
  persists the clear. Half B ORs the entries with `-nomods` / `-reset` where retail ORs its
  two bytes. Half A honours the entries too, which retail does not (Q1). `-nomods` / `-reset`
  stay Half-B-only, as in retail. `RtEnginePlatformShowLauncherDialog` is deleted, and
  known_port_issues row 34 is FIXED (status cell only).
- **E.6.** `Registry.h` (with `kRegistry*`) is deleted. Stale `IO::Registry` mentions and the
  comments that described this exe as using the registry are updated. The
  `engine_architecture.md` section is renamed "Launcher dialog and settings".

## Deviations from the plan

1. **Allocator-free platform functions (E.3).** `HarnessGuardUnattendedBoot` runs before
   `RuntimeInit`. `System::MemoryAllocator::_singleton` is null until `Engine::Startup`, so
   any `System::String` / `WideString` allocated there would dereference null.
   SETTINGS-1's file functions converted paths through `WideString`. They now use
   `MultiByteToWideChar(CP_UTF8)`, and the import uses a file-local UTF-16 to UTF-8
   converter with `AsUTF8`'s rule (a valid pair is 4 bytes, a lone surrogate is dropped).
   `Platform::DocumentsDir(std::string&)` was added beside the `System::String` overload,
   which still serves `GetSaveDirectory` unchanged.
2. **The path is built once, std-only.** It is `DocumentsDir + "\Banished\settings.cfg"`,
   which equals `GetSaveDirectory() + "settings.cfg"`. AdjustDelimiters is a no-op on an
   `SHGetFolderPathW` path, and the UTF-8 rule is AsUTF8's. The engine and the guard
   therefore name one file.
3. **The unit test drives `IO::SettingsStore`, not the Persistence members.** A Persistence
   embeds a `System::Thread` worker, and the map insert is `rtavl::StringMapInsert`
   (`avl_trees.cpp`). Neither links in the unit harness. The 8 tests in
   `tests/unit/persistence_settings_test.cpp` run over a temp directory through the real
   Win32 file bodies, with an injected legacy reader. They cover file read, read-only
   import, empty store, first write creating the folder, merge, round trip, UTF-8 path and
   the default path's tail. The unit build now links `settings_win.cpp` and `widestring.cpp`.
4. **New log lines.** Each ReadSettings prints `[Settings] ReadSettings: N entries from
   file|registry import (read-only)|none (defaults) (<path>)` to stderr, which lands in
   `standalone_error.log`. A failed write prints `[Settings] WriteSettings: FAILED ...`.
   DumpVideoSettings prints a `[VideoReg AT-BOOT] source: ...` line, and its REG type
   column is gone. The PlatformConfigure summary counts `half-a / half-b / launcher-nomods /
   launcher-reset`.
5. **Every `settings.cfg` carries `LauncherDisableMods=false` and
   `LauncherResetSettings=false`** after the first boot, because the clear is a `Set(false)`.
   That is what makes the entries discoverable.
6. **E.6 scope.** `options.cpp` has no registry wording at this base. STATUS.md's only
   mention is the dated B9-close entry, so it was left alone. Comments that state retail
   facts (keymap_load, modification_refresh, device.cpp, devicedata.cpp) stay.

## The byte mapping (E.5, as instructed)

The ported code's two comments agree: the banner says 0x1801fc698 is "reset" and 0x1801fc699
is "nomods". The call-site comment pairs 0x1801fc699 with 0x1800a17ff, the mods test.
`docs/reference/engine_architecture.md`'s launcher section recorded no byte mapping, so there
was nothing to disagree with. This block had no Ghidra and did not re-read 0x1800a17ff /
0x1800a18bf. The code keeps that order with a comment naming the doubt. The effect order
(mods, then reset) is retail's either way.

## The coordinator's verification checklist

Run under the game-dir lock. `$SP` is a scratch directory and `$CFG` is
`<Documents>/Banished/settings.cfg`. Find Documents with:
`powershell -NoProfile -Command "[Environment]::GetFolderPath('MyDocuments')"`.

1. **Before the first run**, snapshot the registry as UTF-8 JSON (REG_SZ values only).
   Check that `$CFG` is absent. If it exists, move it aside so the import path runs.
   From bash, in `$SP` (single quotes, so bash leaves the `$` names to PowerShell):
   ```
   powershell -NoProfile -Command '$i=Get-Item "HKCU:\Software\Shining Rock Software LLC\Banished"; $o=[ordered]@{}; foreach($n in $i.GetValueNames()){ if($i.GetValueKind($n) -eq "String"){ $o[$n]=$i.GetValue($n) } }; $o | ConvertTo-Json | Set-Content -Encoding UTF8 reg_before.json'
   ```
2. **`mingw32-make sa-run`, first run (import).**
   - `standalone_error.log` holds `[Settings] ReadSettings: N entries from registry import
     (read-only)`, where N is the REG_SZ count.
   - `standalone_debug.log` holds `[VideoReg AT-BOOT] source: registry import ...`.
   - `$CFG` now exists.
3. **The exact compare** (scratch script, not committed). Every REG_SZ name and value must
   appear in `$CFG` unchanged, apart from keys the run itself wrote. The likely run-written
   keys are `VideoSettings`, `VideoWindow*`, `Game*`, `ModIndex_*` / `ModEnabled_*` (same
   values expected) and the two new `Launcher*` keys.
   ```python
   import json, sys
   reg = json.load(open(sys.argv[1], encoding='utf-8-sig'))
   esc = {'\\':'\\','n':'\n','r':'\r','t':'\t','s':' ','=':'=','#':'#'}
   def unesc(s):
       out, i = [], 0
       while i < len(s):
           if s[i] == '\\' and i + 1 < len(s) and s[i+1] in esc: out.append(esc[s[i+1]]); i += 2
           else: out.append(s[i]); i += 1
       return ''.join(out)
   cfg = {}
   for line in open(sys.argv[2], encoding='utf-8').read().split('\n'):
       if not line.strip() or line.lstrip().startswith('#'): continue
       i = 0
       while i < len(line) and not (line[i] == '=' and (i == 0 or line[i-1] != '\\')): i += 1
       cfg[unesc(line[:i].strip())] = unesc(line[i+1:].strip())
   for k, v in reg.items():
       if k not in cfg: print('MISSING', k)
       elif cfg[k] != v: print('CHANGED', k, repr(v), '->', repr(cfg[k]))
   print('extra in file:', sorted(set(cfg) - set(reg)))
   ```
   Run it as `python -I cmp.py $SP/reg_before.json "$CFG"`. Expect no MISSING line, and
   CHANGED only for keys the run wrote. The splitter treats `\=` as escaped, which is
   enough for retail keys.
4. **The registry is never written.** Snapshot it again as `reg_after.json` and diff it
   against `reg_before.json`. They must be identical.
5. **`sa-run` a second time (file path).** `standalone_error.log` holds
   `[Settings] ReadSettings: N entries from file`.
6. **`tools/resave_gate/resave_gate.sh Gate`.** This is the content-drift check, because
   `ModEnabled_*` decides whether `Gate.sav` loads. Then run
   `docs/runtime_port/phase_d/mods_route.sh` (`SaveList` / `RefreshList`) and
   `mingw32-make sa-run-menu`.
7. **E.4 guard.** Kill a run mid-boot so `VideoSettings=true` stays in `$CFG`. The next
   `sa-run` must print `[Standalone] HarnessGuard: VideoSettings true -> false`, and `$CFG`
   must then read `VideoSettings=false`.
8. **E.5, on a BACKUP of `$CFG`** (`cp "$CFG" $SP/settings.cfg.bak`).
   - Set `LauncherDisableMods=true`, then run `sa-run-menu`. Afterwards every `ModEnabled_*`
     reads false, `LauncherDisableMods` reads false, and the summary has
     `launcher-nomods=1`. Restore the backup.
   - Set `LauncherResetSettings=true`, then run `sa-run-menu`. Afterwards Video / Audio /
     Game keys hold their defaults, every `Ach<N>` is still present (the merge), and
     `LauncherResetSettings` reads false. Restore the backup.
   - Restore the mods directory state as the memory `reference_mods_off_breaks_save_loading`
     requires.
9. **Optional.** `tools/ach_reset.sh` on a backup of `$CFG`. It prints the count and leaves
   no `Ach` line.

## Open items

- **The `abi_audit` baseline was lowered** in the final commit with `--write-baseline`,
  because main.cpp's registry code is gone. No row went up. The rows that changed:

  | row | before | after |
  |---|---|---|
  | `wchar_t` src/app | 10 | 2 |
  | `wide_literal` src/app | 14 | 1 |
  | `wide_crt` src/app | 4 | 1 |
  | `win32_typedef` src/app | 83 | 73 |
- **D4_RULES C3 is still owed.** `ReadWholeFile` / `WriteFileAtomic` should be re-based on
  THREADS' file primitives when THREADS-2 lands them. They must stay allocator-free, because
  the harness guard calls them before the engine allocator exists.
- **Stale objects.** `build/{debug,perf}/obj/src__platform__win__persistence_registry_win.o`
  can stay in an existing build directory. They are not linked, because the link list comes
  from the sources.
- **Memory.** `feedback_banished_registry_strings.md` should be marked historical, since the
  store is a file now. Memory lives outside the worktree.
- **Retail and this exe no longer share settings** (plan §F). An A/B against retail needs
  the mod list synced by hand, or `settings.cfg` deleted to re-import.
- **The byte-mapping doubt** above is unverified in the disassembly.

## Shared files touched

- **`src/app/main.cpp`** is shared with SDL-A's B0.2. Changes: two includes (`io/settings_file.h`,
  `io/settings_store.h`) and `<cstdlib>`; `DumpVideoRegistry` became `DumpVideoSettings`
  with the `FindSetting` / `SettingsSourceName` helpers above it; `HarnessGuardUnattendedBoot`'s
  body and banner; the boot-order comment block 1b; the StallDebug mark text; and the
  BootState comment.
- **`src/engine/system/Thread.h`**: comment only. The merge conflict with THREADS-1's
  `Platform::LockStorage` was resolved by keeping main's code and this block's comment.
- **`tests/unit/sources.mk`** got one block appended, and `sources.cmake` was regenerated
  after the merge.
- **`src/platform/common/platform_settings.h`, `src/platform/win/settings_win.cpp`**: these
  are SETTINGS-1's files. THREADS-2's C3 re-base touches them.
- **Comment-only edits**: `src/engine/io/Persistence.h`, `FileSystem.h`, `PackageFile.h`,
  `src/engine/engine/engine_lifecycle.cpp`, `src/engine/achievement/achievement_state.cpp`,
  `src/diag/autostart.cpp`, `src/game/Game/game_main.cpp`, `inputbindings.cpp`.
- **`game_settings_persist.cpp`**: one log string changed.
- **`src/engine/engine/engine_platform.cpp`, `src/platform/win/engine_platform_win.cpp`.**
- **Docs**: `docs/known_port_issues.md` (row 34's status cell only),
  `docs/reference/engine_architecture.md`, `tools/ach_reset.sh`.
- **New files**: `src/engine/io/persistence_settings.cpp`, `settings_store.{h,cpp}`,
  `tests/unit/persistence_settings_test.cpp`.
- **Deleted**: `src/platform/win/persistence_registry_win.cpp`, `src/engine/io/Registry.h`.
