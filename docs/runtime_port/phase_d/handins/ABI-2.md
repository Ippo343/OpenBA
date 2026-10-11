# Hand-in: block ABI-2 (Phase D.4 wave 2b; plans/ABI.md steps C.6, C.5)

Branch `d4-abi2`, base `5f9dcb6d2`. C.6 was done first because `platform_file.h` was not on main yet;
main was merged at `eb4107806` (THREADS-2, RENDER-3, SDL-B) in `3dd59ef39`, then C.5 went in with the
`PathChar` flip. Static gate only: no game run. Every commit: `-j2` debug / perf / unit exit 0,
castmetric (`castmetric.py src`) 2, `python tools/abi_audit.py` OK.

## Commits and B11 verdicts (perf objects)

| commit | step | unit | B11 |
|---|---|---|---|
| `19d2f77e0` behaviour: `_stricmp` -> `rtcrt::StrICmp` | C.6 | 953 / 0 | vs `runs/b11base_d4b`, 944 objects: DIFF 7, IDENTICAL 16,638, RENAMED-IDENTICAL 396 (static-init). The 7 DIFFs are exactly the 7 caller TUs, one function each (below) |
| `3dd59ef39` merge main `eb4107806` | | | conflicts: `tools/abi_audit.baseline` (main's side, then `--write-baseline` drops only the 4 stricmp rows), `src/app/main.cpp` (keep both includes) |
| `5a8b55347` refactor: WideString over `char16_t`; `PathChar` flips | C.5 | 971 / 0 | vs the merge commit built without C.5 (isolates C.5 from the three merges), 971 objects: IDENTICAL 16,899, RENAMED-IDENTICAL 415 (399 static-init + 16 renames), SAME 1, DIFF 3, ONLY-BASE 1 / ONLY-TIP 5, all in `settings_win.o` (the finding below) |

**C.6 DIFFs, read.** The base calls the host `_stricmp` through the import slot `__imp__stricmp` (an
indirect `call *`), so a rename map cannot pair it; the instruction text of each function was diffed:

| object | function | what differs |
|---|---|---|
| `app/frame_limiter.o` | `FrameLimiter::Tick` | hoisted `mov __imp__stricmp,%rbx` gone, 2 calls retargeted, 1 alignment nop |
| `app/main.o` | `DumpVideoState` | 1 call retargeted |
| `diag/saveload_harness.o` | `SaveLoadHarness_Tick` | hoisted load gone, 2 calls retargeted |
| `engine/save/saveload.o` | `Save::SaveLoad::CheckMod` | 1 call retargeted; callee-saved pairs swapped (r12 <-> r14, r13 <-> r15), 201 / 201 instructions |
| `engine/save/saveload_update.o` | `Save::SaveLoad::UpdateLoad` | 1 call retargeted, 1 nop |
| `game/Dialog/newgamedialog.o` | `NewGameDialog::StartGame` | 2 hoisted loads gone, 6 calls retargeted, nops moved |
| `game/Game/gameupdateloop_serialize.o` | `GameUpdateLoop_SaveRoundtripCheck` | 1 call retargeted |

**C.5 rename map** (mangling `w` -> `Ds`, bare names unchanged, each RENAMED-IDENTICAL):
`System::WideString::{WideString (C1), Set, StrLength}` (widestring.o); `RtEnginePlatformWidePathExists`
(engine_platform_win.o); `Platform::{FileOpenRead, FileCreateTemp, FileDelete, FileMove, DirCreate,
FileOpenReadShared, FileCreateWrite, FileReplace, PathAttributes, ExecutablePath, FindFirst,
FormatLocalDateTime}` (file_win.o).

## Finding: `settings_win.o` is a DIFF under C.5 (D4_RULES C8)

SETTINGS / THREADS-2 hold a path as `std::basic_string<Platform::PathChar>` (`PathString`). After the
flip it is `std::basic_string<char16_t>`. libstdc++'s `char_traits<wchar_t>` calls `wcslen` / `wmemchr`
/ `wmemcpy` out of line; `char_traits<char16_t>` uses generic inline loops, so inlining changes:
`Utf8ToPath` 88 -> 156 lines, `WriteFileAtomic` 254 -> 300, `ReadWholeFile` only the `_M_dispose`
instantiation renamed (121 / 121), `ReadLegacySettings` SAME. Same UTF-16 units, same standard-library
operations: no observable change expected. It is new D.4 code, not a retail body. It is left as the
planned flip's cost; the coordinator's settings checklist (`mods_route.sh`, `sa-run-menu`: settings.cfg
written and read back) is the run that covers it.

## Deviations from the plan

- C.6: 14 call sites in 7 files, not 13. The declaration moved to a new `src/engine/rt_crt.h` "beside
  rt_libm.h"; `rt_libm.h` includes it, so its 7 existing includers are untouched, and
  `msvc_stricmp.cpp` includes it so the definition is checked. `tests/unit/set_test.cpp`'s
  `AsciiStrICmp` is a test oracle, not a `_stricmp` call, and stays. The stale `saveload.cpp` banner
  ("ours calls the CRT we are linked against") is corrected.
- C.5: `value_types.cpp:425` and `modification_setupfilesystem.cpp:284` needed nothing after the merge:
  THREADS-2 already routes them through `Platform::FormatLocalDateTime` (a `PathChar[]` buffer) and
  `Platform::PathAttributes`. `file_win.cpp` already casts once in its `Wide()` helper;
  `window_win.cpp` / `window_hook_win.cpp` never meet a WideString. The casts added are 7:
  `application_win.cpp` 3 (CreateProcessW, ShellExecuteW x2), `engine_platform_win.cpp` 1,
  `settings_win.cpp` 1 (DocumentsDir), `devicedata.cpp` 2 (Description; DeviceName hoisted into one
  pointer so StrLength and the tail ctor share 1 cast).
- C.5: no `L"..."` literal feeds a WideString (the 9 in the tree go to Win32 or `wcscmp`), so none
  became `u"..."`.
- C.5's B11 base is the merge commit rebuilt without C.5, not `runs/b11base_d4b`: against the latter
  every object the three merges touched differs for reasons that are not C.5's.

## Audit and gate numbers

| check | before | after |
|---|---|---|
| `abi_audit` pinned rows | 1 (`WideString::_data`) | 0 |
| `stricmp` app / diag / engine / game | 2 / 2 / 2 / 7 | 0 (C.6) |
| `wchar_t` src/engine / src/platform / tests | 17 / 1 / 1 | 0 / 0 / 0 (C.5) |
| castmetric total | 2 | 2 |
| cast_ok | 662 | 669 (the 7 marked casts) |
| stray markers | 5 | 5 |

Warnings: 0 new under C.6. Under C.5 one line reads as new in debug and perf: the baseline's
`widestring.cpp: SameWide(const wchar_t*, const wchar_t*) defined but not used` now spells its
signature `char16_t`. Same warning, renamed: the warn_baseline lines want the new spelling.

## Open items for the coordinator

- Update `runs/warn_baseline_{debug,perf}.txt`'s `SameWide` line to the `char16_t` spelling.
- `settings_win.o` DIFF (above): covered by the settings checklist at the merge.
- C.6 is `behaviour:`: the two retail callers (`CheckMod`, `UpdateLoad`) now call retail's own body;
  the other twelve compare env / option strings and the video adapter name. A null argument now
  returns 0x7FFFFFFF with one `[RtCrt]` line instead of the host CRT's invalid-parameter path.

## Shared files

- `src/platform/common/platform_file.h` (THREADS-2's): the `PathChar` line and its banner only.
- `src/platform/win/settings_win.cpp` (SETTINGS-2, then THREADS-2's C3 re-base): one cast in
  `DocumentsDir`; the allocator-free `MultiByteToWideChar` conversion is untouched.
- `src/render/dx11/devicedata.cpp` (RENDER-3's): the two DXGI name reads.
- `src/app/main.cpp` (SDL-B's): one include beside SDL's.
- `tools/abi_audit.baseline`: rows lowered only.
