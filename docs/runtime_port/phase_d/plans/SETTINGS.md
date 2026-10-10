# Phase D.4 seam plan: SETTINGS (registry -> settings file, launcher -> file entries, save-dir query)

Decision: ENDGAME.md §E item 2, NEXT_SESSION.md §2.2. Surveyed at `4e36b7000` (read-only). Owner of the
implementation: one opus agent; the coordinator runs every gate.

## A. Inventory

**The store.** `IO::Persistence` (`src/engine/io/Persistence.h:464`) holds ONE flat in-memory map,
`_settingsMap` (`RtIo::SettingsSet`, AVL on `strcmp`, `Persistence.h:323`). Every key is
`Format("%s%s", section, key)` (`persistence.cpp:56`): no separator, so `"Video"+"Width"` = `VideoWidth`.
Values are UTF-8 strings: ints `"%i"` (`Set(int)` 0x1800b80c0), bools `"true"`/`"false"` (`Set(bool)`
0x1800b8380); `Get(int)` 0x1800b81e0 is `strtol`, `Get(bool)` 0x1800b8290 is `strcmp(v,"true")==0`; a miss
leaves the out-param untouched. Callers only ever see the map; two members touch the OS:

| Member | Retail | File:line | OS calls | Called from |
|---|---|---|---|---|
| `ReadSettings` | 0x1800b84b0 | `src/platform/win/persistence_registry_win.cpp:342` | `RegEnumValueW`, `RegQueryValueExW` (type ignored, value buf 0x400 bytes, name buf 0x400 wchar) | `Persistence::Startup` `persistence_startup.cpp:119` (boot, BEFORE `GetSaveDirectory`); OptionsDialog cancel / restore `optionsdialog.cpp:1081`, `:1099` |
| `WriteSettings` | 0x1800b88e0 | same file `:689` | phase 1 `RegSetValueExW` REG_SZ per map entry; phase 2 `RegDeleteKeyW` on stale VALUE names = **never deletes anything** (banner `:416`) | `Engine::PlatformConfigure` `engine_platform.cpp:300`, `:317`; `Engine::Shutdown` `engine_lifecycle.cpp:787` (if `_noConfigure==0`); game shutdown `game_main.cpp:292`; OptionsDialog apply/OK `optionsdialog.cpp:1056`; `SetAchievementMet` `achievement_state.cpp:190` |
| `IO::Registry` ctor/dtor | 0x1800b9500 | same file `:267`, `:316`; class `src/engine/io/Registry.h` | `RegOpenKeyExW` / `RegCreateKeyExW` (CREATES the key) `HKCU\Software\Shining Rock Software LLC\Banished`, `KEY_ALL_ACCESS` | only the two members above |
| `ResetConfigurations` (`-reset`) | 0x1800a1939 | `engine_platform.cpp:176` | none: `_settingsMap.Clear()`, then the configs are rebuilt with defaults | `PlatformConfigure` |

**The keys** (all REG_SZ in retail; memory `feedback_banished_registry_strings.md`):

| Keys | Type | Default (ctor) | Reader | Writer |
|---|---|---|---|---|
| `VideoSettings` (the launcher arming bit, field `settingsValid`) | bool | true | `Video::Configuration::Startup` 0x1800f6680 `device.cpp:487/492` | `Video::Configuration::Save` 0x1800f6d10 `device.cpp:426` |
| `Video` + `Interface Adapter Width Height WindowWidth WindowHeight WindowX WindowY RefreshN RefreshD Multisample TextureFilter ShadowScalar ShaderDetail ReflectionDetail` | int | 1 0 -1 -1 -1 -1 -1 -1 60 1 1 0 1 2 1 (`Device.h:729`) | same | same |
| `VideoFullscreen`, `VideoVSync` | bool | true, true | same | same |
| `Audio` + `UIVolume MusicVolume AmbientVolume EffectVolume` | int | 100 50 100 100 | `Audio::Configuration::Startup` 0x1800ddee0 `audio.cpp:588` | `::Save` 0x1800dde10 `audio.cpp:560` |
| `Input` + `MouseClipping Legacy MouseDisplay` | bool | ctor | `Input::Configuration::Load` 0x1800e0c80 `input_config_startup.cpp:151` | `::Save` 0x1800e0c00 `input.cpp:431` |
| `Input<description>`, `Input<description>Alt` (key binds) | int | binding table | `KeyMap::Load` 0x1800a7190 walks the map `keymap_load.cpp:170` | `KeyMap::Save` 0x1800a7520 `keymap_save.cpp:210` |
| `ModEnabled_<name>`, `ModIndex_<name>` | bool, int | absent | `Modification::RefreshList` 0x1800af5f0 `modification_refresh.cpp:745/759` (`Find`) | `Modification::SaveList` 0x1800b05f0 `modification.cpp:696` |
| `Ach<N>` | bool | absent | `AchievementManager::ReadAchievements` 0x18009eb90 `achievement_state.cpp:171` | `SetAchievementMet` `achievement_state.cpp:189` (+ immediate `WriteSettings`) |
| `Game` + `UIScale AutosaveInterval StatusIconOpacity StatusIconScale LastOption LastTerrain LastMapSize LastClimate LastStart LastDisaster` (int), `StatusIconEnable EdgeScroll AutoPause ShowCelcius ShowWeather AutoSlow` (bool) | | GameSettings ctor | `GameSettings_Load` 0x140098b60 `game_settings_persist.cpp:90` | `GameSettings_Save` 0x140098970 `:116` |

**Outside the engine** (not retail, must move too): `src/app/main.cpp:109` `DumpVideoRegistry` (logs 9 Video
keys) and `:170` `HarnessGuardUnattendedBoot` (unattended run: `VideoSettings` true -> false; every run:
`VideoWindowWidth/Height` below 64 -> -1), both before `Engine::Startup`. `tools/ach_reset.sh` deletes
`Ach<N>` with `reg delete`. No other tool, skill or gate script touches the registry.

**Launcher.** `Engine::PlatformConfigure` 0x1800a1670 (`engine_platform.cpp:265`; Win32 half
`src/platform/win/engine_platform_win.cpp`): if `VideoSettings` is false and neither `-launcher` nor shift is
held, set it true, `Save`, `WriteSettings`, proceed (Half A). Otherwise (Half B) `ValidateConfiguration`, then
`DialogBoxParamW(template 0x66)` -- refused today, returns 1 = Play -- then `Save`, `WriteSettings`. The two
check-box bytes 0x1801fc699 / 0x1801fc698 are ORed with `-nomods` (`DisableAllModifications`: every mod
`enabled=0`, `SaveList`) and `-reset` (`ResetConfigurations`) at `engine_platform.cpp:320-327`. Today both
bytes are 0. The byte-to-option mapping comes from our comment's order; the implementer confirms it at
0x1800a17ff / 0x1800a18bf before step 5. The dialog's video page edits the `Video*` keys, which the file
already carries.

**Save directory.** `FileSystem::GetSaveDirectory` 0x1800acfd0 (`src/platform/win/filesystem_win.cpp:120`,
that file's only body): `SHGetFolderPathW(CSIDL_PERSONAL)` into `wchar_t[264]`, `WideString` -> `AsUTF8`,
`Format("%s\\%s\\", docs, "Banished")`, `Filename` (AdjustDelimiters); on failure `Error::SetText(..., true)`
and a null Filename. `this` is unused. Callers: `persistence_startup.cpp:129` (-> `_savePath`),
`persistence_savelist_job.cpp:175`, `w2_takescreenshot.cpp:142`. `-lshell32` exists for it (`Makefile:66`).

**Options overlap.** None. `src/app/options.h` holds no setting value. `unattendedBoot` (`options.cpp:247`)
only decides whether the harness guard clears `VideoSettings`. `BANISHED_MENU_SCENE` (`options.h:67`) is an
Options flag, not a setting, and is untouched.

## B. Design

- **Format: flat `key=value` UTF-8 text, one entry per line.** The map is flat and its keys are already
  concatenated (`VideoWidth`, `ModEnabled_Foo`), so INI sections would be cosmetic and could not be split back
  without guessing (`ModEnabled_` has no section). JSON needs a parser or a dependency for a one-level map.
  Write: `key=value\n`, LF, no BOM, sorted by `strcmp` (the map's in-order walk, deterministic). Read: skip a
  UTF-8 BOM, accept CRLF, skip blank lines and lines starting with `#`, split on the first UNESCAPED `=`, trim
  spaces and tabs around key and value, last duplicate wins. Escapes in key and value: `\\`, `\n`, `\r`, `\t`,
  `\s` (a significant leading or trailing space), and `\=` (keys only). Mod names come from `.pkm` file names
  and may hold spaces, `=` or non-ASCII, so the escapes are needed. No length cap.
- **Location:** `<SaveDirectory>settings.cfg`. Windows: `<Documents>\Banished\settings.cfg`, next to `Save\`.
  The save dir is unchanged, so saves stay shared with a retail install. POSIX: see C.
- **Timing: exactly retail's.** `ReadSettings` stays the one reader (boot, options-dialog cancel) and
  `WriteSettings` the one writer at its six call sites. No new read or write points, no caching across calls.
- **ReadSettings:** `Clear()` the map (retail's first call), read the file, insert every entry in file order with
  the same insert-then-reseat as today. File missing: import (below). Path unavailable: an empty map, which is
  retail's "key could not be opened" outcome (`_key == 0`).
- **WriteSettings: a merge, not a rewrite.** Read the file again, overlay every map entry (map wins), write the
  union atomically (`settings.cfg.tmp`, then replace), creating the parent directory if absent. The merge IS
  retail's observable behaviour: phase 2's `RegDeleteKeyW` deletes nothing, so a value absent from the map
  survives on disk. It matters after `-reset` (map cleared): retail keeps `Ach<N>`, `ModEnabled_*` and key binds
  in the registry, and a plain rewrite would erase them. Path unavailable: a no-op (retail: `_key == 0`).
- **Migration: import once, read-only, Windows only.** When `settings.cfg` is absent, `ReadSettings` reads
  `HKCU\Software\Shining Rock Software LLC\Banished` (open with `KEY_READ`, never create, REG_SZ values only,
  others skipped with one stderr line) into the map. Nothing is written then; the first `WriteSettings` (every
  boot reaches one in `PlatformConfigure`) materialises the file. The registry is never written again. Deleting
  the file re-imports. Recommended over no migration because the gate machine's state lives in the registry:
  `ModEnabled_*` decides whether `Gate.sav` loads (memory `reference_mods_off_breaks_save_loading.md`).
- **The two launcher entries:** `LauncherDisableMods` and `LauncherResetSettings` (bool, absent = false). The
  names are ours, prefixed so they cannot collide with a retail key. `PlatformConfigure` reads both with
  `Persistence::Get(bool)` before the Half A / Half B split and sets both back to false with `Set(bool)`, so the
  `WriteSettings` each half already makes persists the clear. It ORs them with `-nomods` / `-reset` where retail
  ORs its two bytes. They are **one-shot**, because a persistent `LauncherResetSettings=true` would wipe the
  settings on every boot. They are honoured on **every** boot, because the file is now the launcher. Half B shows
  nothing and proceeds as Play, as it does today; `RtEnginePlatformShowLauncherDialog` is deleted.
- **File missing and no registry:** an empty map, so every config keeps its constructor default (retail's first
  run). `VideoSettings` defaults true so Half B runs (no dialog), and the first `WriteSettings` creates the file.

## C. Platform API

One header, `src/platform/settings_platform.h` (the coordinator may rename it to match the other seam plans'
header convention), namespace `Platform`:

```cpp
bool DocumentsDir(System::String& out);                          // UTF-8, no trailing separator; false on failure
bool ReadWholeFile(const char* pathUtf8, System::String& out);   // false if absent
bool WriteFileAtomic(const char* pathUtf8, const char* data, size_t n);  // tmp + replace, mkdir parent
// Legacy import; false when there is no legacy store (POSIX always). `emit` gets UTF-8 key / value.
bool ReadLegacySettings(void (*emit)(void* ctx, const char* key, const char* value), void* ctx);
```

- **Platform-neutral, new:** `src/engine/io/settings_file.{h,cpp}`, the codec: `Parse(text)` to an ordered list
  of string pairs, `Serialize(list)`, `Merge(disk, map)`. No engine types, so it is unit-testable.
  `src/engine/io/persistence_settings.cpp` holds `ReadSettings` / `WriteSettings` over the codec and the API.
- **Moves out of `src/platform/win/`:** `GetSaveDirectory`'s body goes to `src/engine/io/filesystem.cpp` and calls
  `Platform::DocumentsDir`. The `Format("%s\\%s\\", docs, "Banished")`, the `Filename` and the `Error::SetText`
  arm stay retail's. `filesystem_win.cpp` and `persistence_registry_win.cpp` are deleted.
- **New Win32 file:** `src/platform/win/settings_platform_win.cpp`: `SHGetFolderPathW` (moved), `CreateFileW` /
  `ReadFile` / `WriteFile` / `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)` / `CreateDirectoryW` over a UTF-8 to
  wide conversion, and the read-only registry import. `wchar_t` stays inside this file.
- **Unchanged:** `engine_platform_win.cpp` keeps `RtEnginePlatformShiftHeld` (the input seam owns it later) and
  `RtEnginePlatformWidePathExists`. It loses only `RtEnginePlatformShowLauncherDialog`.
- **Deleted once unused:** `IO::Registry` (`src/engine/io/Registry.h`) and its `kRegistry*` constants.
- **A `src/platform/posix/` twin needs:** `DocumentsDir` as the per-user data dir without `getenv` (the rule is
  absolute): `getpwuid(getuid())->pw_dir + "/.local/share"` now, or `SDL_GetPrefPath` once SDL3 lands (macOS
  `~/Library/Application Support`, Android internal storage); `open` / `write` / `fsync` / `rename` and `mkdir`;
  `ReadLegacySettings` returning false. The backslash `Filename` shape `GetSaveDirectory` returns belongs to the
  FILESYSTEM seam. Makefile and CMake glob only `src/platform/win/*.cpp` today (`Makefile:32`,
  `CMakeLists.txt:69`); the posix glob arrives with the first posix build, not here.

## D. Fidelity

**Preserved exactly:** every key string (the concatenation and the case); the value spellings (`%i`,
`true`/`false`) and the `strtol` / `strcmp` readers, untouched in `persistence.cpp`; `ReadSettings` clearing the
map first (options cancel depends on it); the read and write call sites and their order in `PlatformConfigure` and
`Engine::Shutdown`; the never-delete outcome of `WriteSettings` (the merge); the empty-map outcome when the store
cannot be opened; `GetSaveDirectory`'s string and error text; the arming-bit logic and the harness guard's two
repairs; the `-nomods` / `-reset` effects. The resave gate's pins (`BANISHED_MENU_SCENE=0`, `BANISHED_AUTOLOAD`,
`Gate.sav`) are Options flags and a save file, not settings. Its determinism needs the settings CONTENT
unchanged, which the import gives and step 3 verifies.

**Replaced by decision (ENDGAME §E item 2):** the HKCU store and `IO::Registry`; the launcher dialog
(`DialogBoxParamW`, template 0x66, proc 0x1800a1ae0), replaced by the two file entries (known_port_issues row 34).

**Declared deviations, none reachable from retail-written data:** keys compare case-sensitively (registry value
names do not; no retail key pair differs only by case); no 511-character value cap; non-REG_SZ registry values
are skipped on import rather than read as raw bytes; the write is atomic where the registry wrote value by value.

## E. Steps (one commit each; the coordinator gates each with `mingw32-make sa-run`)

1. **`refactor: settings file codec`.** Add `src/engine/io/settings_file.{h,cpp}` and
   `tests/unit/settings_file_test.cpp` (pattern: `tests/unit/filename_test.cpp`, `UNIT_TEST` / `CHECK_STREQ`).
   Add the `.cpp` to `tests/unit/sources.mk` and regenerate `sources.cmake` with
   `tools/restructure/sources_mk_to_cmake.py`. Tests: round-trip of `=`, `\`, LF, CR, tab, leading and trailing
   spaces and UTF-8 in keys and values; BOM, CRLF, comments, blank lines, spaces around `=`; duplicate key, last
   wins; empty value; no final newline; `Merge` keeps disk-only keys and lets the map win; sorted output. No
   caller yet. Gate: `mingw32-make unit`, then `sa-run` (behaviour unchanged).
2. **`refactor: platform settings API; GetSaveDirectory over Platform::DocumentsDir`.** Add the header and
   `src/platform/win/settings_platform_win.cpp` (all four functions). Move `GetSaveDirectory`'s body into
   `src/engine/io/filesystem.cpp` and delete `filesystem_win.cpp`. Gate: `sa-run`, then
   `tools/resave_gate/resave_gate.sh Gate` (the save list and `Gate.sav` are found through this path).
3. **`behaviour: IO::Persistence settings in <SaveDirectory>settings.cfg, not the registry`.** Add
   `src/engine/io/persistence_settings.cpp` (Read: clear, file or import, insert. Write: read, merge,
   `WriteFileAtomic`). Delete `persistence_registry_win.cpp` and update the member map at `Persistence.h:452`.
   Verification by the coordinator: before the first run, save `reg query "HKCU\Software\Shining Rock Software
   LLC\Banished"` to the scratchpad. After `sa-run`, compare every REG_SZ name and value with `settings.cfg`
   using a scratch script that is not committed. They must match exactly, apart from keys the run itself wrote.
   Then run `sa-run` a second time (file path, no import), the resave gate, and
   `docs/runtime_port/phase_d/mods_route.sh` (it exercises `SaveList` / `RefreshList`, the `ModEnabled_*`
   round-trip). Until step 4 lands, the harness guard still edits the registry. That is safe because Half B
   never blocks.
4. **`behaviour: harness guard and ach_reset.sh on settings.cfg`.** In `src/app/main.cpp`, `DumpVideoRegistry`
   (renamed `DumpVideoSettings`) and `HarnessGuardUnattendedBoot` load, edit and write the file through the codec
   and the platform API, with the same two repairs and the same log lines. If the file is absent, the guard
   imports first, then writes. `tools/ach_reset.sh` deletes `^Ach[0-9]+=` lines from the file. Gate: `sa-run`
   (unattended: the guard's `VideoSettings true -> false` line appears after a killed run), then `sa-run-menu`.
5. **`behaviour: the launcher's two check boxes are settings.cfg entries`.** In `engine_platform.cpp`, read, clear
   and OR `LauncherDisableMods` / `LauncherResetSettings` as in B. Delete `RtEnginePlatformShowLauncherDialog`.
   Mark known_port_issues row 34 FIXED. Verify on a BACKUP of `settings.cfg`, because both entries change the
   user's real settings. Set `LauncherDisableMods=true`, run `sa-run-menu`, and check that every `ModEnabled_*`
   and the entry itself read false afterwards. Restore the backup. Repeat for `LauncherResetSettings`: Video,
   Audio and Game keys return to defaults and `Ach<N>` stays present (the merge). Restore the backup again.
6. **`refactor: delete IO::Registry; settings docs`.** Delete `Registry.h` and its stale mentions. Update the
   "Launcher dialog" section of `docs/reference/engine_architecture.md`, the registry wording in comments
   (`game_main.cpp`, `inputbindings.cpp:137`, `game_settings_persist.cpp:102`, `options.cpp:247`) and STATUS.md.
   Mark memory `feedback_banished_registry_strings.md` historical. Gate: `sa-run`, `unit`, and the no-doors
   `acceptance.sh`.

## F. Risks and questions for the user

- **Top risk: settings content drift breaks the gate silently.** A lost or altered `ModEnabled_*` / `ModIndex_*`
  makes `Gate.sav` fail to load or resave differently. Step 3's exact registry-to-file compare runs before
  anything else uses the new store.
- **Retail and our exe stop sharing settings.** After the import, a change made in retail Banished (registry) no
  longer reaches our exe, and the reverse. Any A/B against the retail exe must sync mod lists by hand or delete
  `settings.cfg` to re-import.
- **Concurrent `WriteSettings`:** the retail map has no lock, so concurrent writers are already a retail race. The
  tmp-file replace adds one failure mode (two writers on the same tmp name). The store serialises its own writes
  with one mutex.
- **Q1 (changes step 5):** should the launcher entries be one-shot and honoured on every boot (recommended),
  persistent, or honoured only when retail would have shown the dialog (arming bit set, `-launcher`, or shift)?
- **Q2 (changes step 3):** import the registry once when the file is absent (recommended), or start from defaults
  with no import? Without the import, the gate machine loses its mod list.
- **Q3 (names only):** are `settings.cfg` next to `Save\` in `<Documents>\Banished\` and the entry names
  `LauncherDisableMods` / `LauncherResetSettings` acceptable?
