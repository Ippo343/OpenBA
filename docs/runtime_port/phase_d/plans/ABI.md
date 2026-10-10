# Phase D.4 plan: the ABI audit (decision 5)

Planning agent ABI, 2026-10-10, main at `4e36b7000`. Read-only survey of `src/` and `tests/`; counts are
`grep` over `*.cpp` / `*.h`. Sources: NEXT_SESSION.md §2.2, ENDGAME.md §E item 5, `.claude/rules/type-headers.md`,
memories `reference_gcc_msabi_retslot_before_this`, `reference_hidden_retslot_wild_write`,
`reference_virtual_dtor_cutover_rules`.

## 0. The fact the whole plan rests on

The tree is NOT built with MSVC. It is built with MinGW-w64 GCC (`x86_64-w64-mingw32`), which already uses the
**Itanium C++ ABI** for everything C++-specific: class layout (tail-padding reuse, empty bases), vtables (two
destructor entries D1/D0, the reason retail slot N is compiler slot N+1), RTTI, pointer-to-member (16 bytes,
`{ptr, adj}`), name mangling. The 1,180 `sizeof` and 5,763 `offsetof` asserts already prove that THIS Itanium
layout reproduces retail's MSVC offsets. Linux x86-64 GCC uses the same Itanium rules. What actually changes
from MinGW to Linux GCC is short:

| differs MinGW -> Linux x86-64 | MinGW (today) | Linux GCC | tree exposure |
|---|---|---|---|
| `long` / `unsigned long` | 4 bytes | 8 bytes | 0 members in pinned layouts (§A.1) |
| `wchar_t` | 2 bytes, UTF-16 | 4 bytes, UTF-32 | 1 pinned member, a pointer (`WideString::_data`) |
| `int64_t` / `size_t` spelling | `long long` / `unsigned long long` | `long` / `unsigned long` | overload + `printf` format only, never width |
| calling convention | ms_abi | SysV | 0 `ms_abi` attributes; only foreign-binary calls care |
| bit-field layout | `-mms-bitfields` default | GCC layout | 0 bit-fields in the tree |
| `va_list` | `char*` | `__va_list_tag[1]` | 13 lines, all standard `va_start` / `vsnprintf` use |
| `long double` | 16 (x87) | 16 (x87) | 0 uses |
| C runtime | msvcrt/ucrt (`_stricmp`, wide CRT) | glibc | 13 `_stricmp` calls, 4 wide-CRT code calls |

So the "MSVC vs Itanium" layout hazards were paid for when the tree moved onto MinGW. A Linux build re-meets
only the rows above, and the static_asserts fail the build on every row that touches a pinned layout.

## A. Inventory per hit class

Class (a) = in a binary-compatible layout or serialized format, MUST keep the Windows size. (b) = local,
counter, loop or diagnostic variable: harmless, may be retyped. (c) = API-boundary type, moves with its seam.

### A.1 Width-varying types

| pattern | files / lines | (a) | (b) | (c) and owning seam | proposed rewrite |
|---|---|---|---|---|---|
| `long`, all spellings | 197 / 695 | | | | 504 lines are `long long` (8 bytes on both: not a width hazard) |
| bare `long` (not `long long`) | 57 / 191 | **0** | ~75 code lines: diag counters, harness sums, `static long s_frame`, slot numbers in `slot_check.h` / `VSlot.h` | ~20: HRESULT held as `long` (app/main.cpp 2 = RENDER; `audio_platform_startup_win` 4 = SDL; `engine_lifecycle` CoInit 3 = THREADS; dx11 `(unsigned long)hr` casts 4 = RENDER); `Debug.h:50-51` SEH filter + `MsgBox` (crash handler; MsgBox = SDL) | (b): `long` -> `int32_t`, Windows-identical. (c): `HRESULT` / `int32_t` at the seam. ~96 hits are comments (the English word "long") |
| `unsigned long` (not `long long`) | 0 in layouts | 0 | printf casts of `DWORD` in diag | `NvOptimusEnablement` (app) | `uint32_t` |
| `wchar_t` | 16 / 66 | **1**: `WideString`'s code unit (`WideString.h:60` asserts `sizeof(wchar_t) == 2`; never serialized, §A.3) | 2 scratch buffers (`value_types.cpp:425`, app class-name buffers) | ~45: registry + filesystem (`platform/win/persistence_registry_win`, `filesystem_*_win`, 7 files) = SETTINGS; `modification_setupfilesystem.cpp:285` `GetFileAttributesW` = SETTINGS; window class / title (app, diag) = SDL; `ResolveGfxImport` = RENDER | `WideString` over `char16_t` (2 bytes everywhere, retail block layout kept); Win32 W-API calls `reinterpret_cast<const wchar_t*>` at the seam with `// CAST-OK: win32` (the `error.cpp:534` pattern); a POSIX seam converts UTF-16 to UTF-8 |
| `L"..."` / `L'x'` | 5 / 25 | 0 | 0 | app / diag / platform Win32 strings | `u"..."` where it feeds `WideString`; else stays in Windows-only code |
| `size_t` | 192 / 1811 | **0** pinned members (the 5 hits are locals) | all | | none: 8 bytes on both |
| `long double`, `__int64` | 0 / 3 comments | | | | |
| Win32 typedefs in code (`DWORD` 92, `HANDLE` 40, `MAX_PATH` 31, `HWND` 26, `LONG` 21, `LARGE_INTEGER` 19, `HRESULT` 15, `BOOL` 11 lines) | | **0** in engine/game type headers: they already hold `void*` for handles and opaque `uint8_t[0x28]` for `CRITICAL_SECTION` (audio `Stream`+0x50, audio `Device`+0xa50, `Video::Device`+0x60, `MemoryAllocator`+0x640; RULE B5-6) | `LARGE_INTEGER` QPC locals in game (`gameupdateloop`, `entitylist`, `splash_loop_stubs`, `gameupdateloop_serialize`) | the rest: platform/win, render/dx11, app, diag | THREADS replaces the QPC locals with the platform timer; nothing in a layout to retype |

### A.2 Calling conventions, attributes, linkage, CRT

| pattern | files / lines | classification | owning seam | proposed rewrite |
|---|---|---|---|---|
| `ms_abi` | 11 / 13 | **all comments** (the "retslot before `this`" rule) | | no code. The `T* M(T* ret, ...)` signatures are plain C++ now (no retail caller remains): portable as-is. Reword the comments from "GCC's ms_abi" to "retail's order" when a file is next touched |
| `__stdcall` / `__cdecl` / `WINAPI` / `CALLBACK` | 39 / 80 | ~43 code lines (c); the rest are comments (all 12 `src/game/Work` hits) | Win32 callbacks + `WinMain` (app, diag, `platform_startup_win.cpp:201` WindowProc) = SDL; `Thread::ThreadFunc` (`Thread.h:247`, `thread_win.cpp:264`, `tests/unit/system_abi_stubs.cpp:82`) = THREADS; `devicedata.cpp:205-206` loader typedefs = RENDER; `renderdoc_integration.cpp` 9 `__cdecl` = diag | delete: `__stdcall` is ignored on x64 Windows and an undefined macro on Linux. RenderDoc: its own `RENDERDOC_CC`, empty on Linux |
| hand-declared `__declspec(dllimport) __stdcall` Win32 prototypes OUTSIDE `src/platform/win` | 5 files / 15 | (c), misplaced | `mainloop_frame.cpp:268-272` (PeekMessageW, TranslateMessage, DispatchMessageW, PostQuitMessage) = SDL; QPC in `mainloop_frame.cpp:251`, `mainloop_run.cpp:163`, `timer.cpp:99-100`, `SwitchToThread`, `GetTickCount64` (`gameupdateloop.cpp:411`) = THREADS; `error.cpp:153-156,530` (LoadLibraryW, GetProcAddress, ExitProcess, MessageBoxW) = SDL | each seam moves them behind its platform API; nothing for ABI to retype |
| `__declspec(dllexport)` | 1 / 2 (`app/main.cpp:53-54`, the Optimus / PowerXpress exports) | (c) | app (SDL) | `#ifdef _WIN32`: a Windows driver hint |
| `__declspec(align)`, `__declspec(thread)` | 0 | | | |
| `#pragma pack` | 2 / 4 | (a) both: `BmpFileHeader` (`save_image_to_file.cpp:131`, file format, `uint16_t`/`int32_t`, asserted 14) and `controller_state_table.h:82` (pack 1, every member naturally aligned, plain function pointers) | | none: GCC treats the pragma identically on both targets |
| `__asm__("_ZTV...")` vtable labels | `exe_type.h:131` (6 uses) + 24 test lines | portable on Linux (no user-label prefix); wrong on macOS (`_` prefix) | | a macro pasting `__USER_LABEL_PREFIX__` (C.2) |
| `va_list` | 5 / 13 | (b): `vsnprintf` forwarding only | | none |
| `wmain`, `_s` CRT, `_aligned_malloc`, `__builtin_ms_va_list` | 0 | | | |
| `_stricmp` | 7 files / 13 callers | (b) | | the tree's own `rtcrt::StrICmp` (`src/engine/crt/msvc_stricmp.cpp:170`, retail's body) through a header; glibc has no `_stricmp` |
| wide CRT (`wcslen`, `wcscmp`) | 3 / 4 code | (c) | app/diag Win32 class names = SDL | stays in Windows-only code |
| `<windows.h>` family outside `src/platform/win` | 64 files: engine 9, game 25 (incl. `game_main.h`), app 7 (incl. `boot_window.h`), diag 22, tests 1 | seam code, plus ~14 game files with no Win32 identifier in them: `action_base_complete`, `action_pf_astar`, `keystone_a_zone`, `citizen_updatehealth`, `mainmenudialog`, `citizenbehavior`, `componententity`, `forester_force_harness`, `keystone_a_gamedtor`, `pathfinder_seed`, `buildinggoal`, `keystone_a_removalcalcpath`, `placementtool`, `work_base_reimpl` | audio, input = SDL; QPC = THREADS; filesystem = SETTINGS; d3d = RENDER | delete the dead ones (C.4); seams take the rest |
| `__rdtsc`, `_mm_*` | 3 files (diag stress profiler, `crt/msvc_libm.cpp`) | x86-only: fine on Linux x86-64 | | ARM note only (§D) |
| member-function pointers | 14 files / 47 (`is_same` checks in tests) + the decode in `VSlot.h` / `slot_check.h` | Itanium x86-64 `{ptr = 1 + offset, adj}`: same on Linux x86-64 | | none on x86-64; differs on AArch64 (§D) |

### A.3 Serialized formats (the resave golden `994eb184...`)

- The scalar family (`src/engine/io/Serializer.h:523-533`, `:851-870`) is spelled in fixed-width types only:
  `int64_t`, `uint64_t`, `int`, `unsigned int`, `short`, `unsigned short`, `float`, `bool`, `[un]signed char`,
  and `SerializeBytes(int n, void*)`. There is no `long`, `wchar_t` or `size_t` overload, so a caller passing one
  fails to bind on Windows today: no such field can be in a save.
- On Linux `int64_t` becomes `long`. A caller passing a `long long&` would stop binding: a compile error, not a
  silent change. `System::Time` (8-byte tick, `value_types.cpp`) goes through `int64_t&`: same bytes.
- Raw blobs through `SerializeBytes`: heightmap floats and `TerrainVertex[]` (`geometry_serialize.cpp:201-221`),
  UI pixels (`saveload_meta.cpp:375`), `NameDatabase` 4-grams (`uint32_t`), `SoundBuffer::_format` (an opaque
  `uint8_t[0x28]` WAVEFORMATEX image: the SDL audio seam must parse it as little-endian bytes, never cast it to
  a header struct), dx11 buffer bytes. None holds a `long`, `wchar_t` or pointer-sized field.
- `WideString` is never serialized; the text codec writes UTF-16 through `uint16_t` (`text_codec.cpp`).
- Every target in scope is little-endian: no byte-order work.

**Verdict:** the save format has no width-varying field. The resave golden needs no change to survive a Linux
build. What remains is the Linux build's own correctness, checked by running the resave gate there once the
seams allow a run.

### A.4 The safety net

| directory | `static_assert(sizeof` | `static_assert(offsetof` | `static_assert(alignof` |
|---|---|---|---|
| src/engine | 485 | 2484 | 162 |
| src/game | 575 | 3157 | 316 |
| src/render | 36 | 83 | 0 |
| src/platform | 3 | 1 | 0 |
| src/app | 1 | 3 | 0 |
| src/diag | 0 | 0 | 0 |
| tests/unit | 81 | 35 | 0 |

572 source files carry at least one. A width change in a pinned class moves a size or an offset, so it fails the
compile. The audit script (C.1) covers what the asserts cannot see: a NEW member typed `long` in a class whose
size happens to absorb it.

### A.5 Unit tests on Linux

`tests/unit` (920 tests) is close to host-portable. The blockers:

1. `src/platform/win/allocator_win.cpp` is linked (`VirtualAlloc` / `VirtualFree`, `CRITICAL_SECTION`). It needs
   a POSIX twin (`mmap` + a mutex placed in the opaque 0x28 bytes); `MemoryAllocator.h:455` already anticipates
   one. Owner: THREADS.
2. `b39_io_classes_test.cpp:32` includes `<windows.h>` only for `INVALID_HANDLE_VALUE`: replace with a
   `kInvalidHandle` constant beside `FileInfo::winHandle` in `FileSystem.h`.
3. `system_abi_stubs.cpp:82` `__stdcall` (THREADS) and `system_abi_test.cpp:418` `sizeof(wchar_t) == 2`
   (becomes `char16_t` in C.5).
4. `%lld` with `int64_t` warns under `-Wformat` (172 `%ll` format sites tree-wide): warnings, not errors.

Nothing else under `tests/unit` touches Win32 (the other "Win32" grep hits there are comments). All 51 slot-helper
users decode the Itanium x86-64 PMF.

## B. MSVC-vs-Itanium layout hazards

| hazard | MSVC | Itanium (MinGW today, and Linux) | does the tree hit it? | mitigation |
|---|---|---|---|---|
| tail-padding reuse | never reuses a base's tail padding | reuses it when the base is non-POD | met and solved on MinGW already: 26 comments document it (`Light.h` explicit `_padD4` / `_pad124`, `Mesh.h:516`, `Material.h:502`, `GraphicsMesh.h:574`: "exactly sizeof(base), no tail-padding reuse"); the asserts pass | nothing new for Linux (same rule). Only a future MSVC / clang-cl build re-meets it; C.1 `--layout` finds the candidates |
| empty-base placement | limited EBO with multiple bases | full EBO | solved on MinGW, as above | same |
| bit-field packing across types | MS rules | GCC rules unless `-mms-bitfields` (MinGW default ON, Linux default OFF) | **0 bit-fields** in src + tests | ban them in pinned layouts (C.1); pass `-mms-bitfields` on Linux anyway: an x86 option on every x86 target, free |
| `#pragma pack` interplay | | same semantics | 2 sites, no bit-fields | none |
| vtable destructor slots | 1 scalar-deleting dtor | 2 (D1 complete, D0 deleting) | yes, by design since the cutover: retail slot N = compiler slot N+1 | none for Linux (same scheme). It is fixed by the C++ ABI: `-mabi=ms` and `-fms-extensions` do not change it. A clang-cl / MSVC build would have one slot, and `slot_check.h` / `VSlot.h` would need a shift of 0 |
| `long double` | 8 | 16 | 0 uses | none |
| member-function-pointer size | 8 to 24, by inheritance model | always 16 | `VSlot.h:25` asserts two words and decodes them | none on x86-64 |
| enum underlying type | always `int` | `int` when the values fit | enums in layouts are covered by the offsets | none |

**What the switches do.** `-mabi=ms` on Linux GCC switches every function to the Microsoft calling convention.
It changes no struct layout, no `long` / `wchar_t` width and no vtable. `__attribute__((ms_struct))` or
`-mms-bitfields` changes bit-field and packing layout only. Nothing switches the dtor pair; it belongs to the
C++ ABI. The tree needs none of these except the free `-mms-bitfields` guard.

**Recommendation: keep the retail (Windows) layout bit-for-bit everywhere, on every platform.** The opposite
choice (retail layout only where a serialized format depends on it, native layout elsewhere) buys nothing: the
survey found no pinned member whose type changes size, so keeping every static_assert true on Linux costs zero
code. It keeps one set of asserts instead of `#ifdef`'d pairs, keeps the 2 ruled casts and the raw-offset
diagnostics valid, and keeps the B11 object comparison meaningful across platforms. The rule for new code: a
pinned layout holds only fixed-width integers, pointers, `float` / `double`, `bool`, enums and other pinned
classes. Never `long`, `wchar_t`, `size_t`, a bit-field, or a native lock / handle type (use opaque bytes or
`void*`). The one cost lands on THREADS: a native mutex must fit the opaque 0x28 bytes (glibc's
`pthread_mutex_t` is 40 = 0x28 and fits; macOS's is 64 and does not, §D).

## C. Steps for the implementing agent

One commit per step, gated on Windows. Because `long` and `int32_t` are both 4 bytes on Win64, and `wchar_t` and
`char16_t` both 2, **every retype must be B11-IDENTICAL on Windows: that is the gate.** Use
`docs/runtime_port/phase_d/b11pair.py` with a rename map, since a signature carrying `long` or `wchar_t` changes
its mangling (`l` -> `i`, `w` -> `Ds`). Per commit: `mingw32-make standalone-debug`, unit 920 / 0, 0 new warnings,
B11 IDENTICAL on every touched object. The coordinator runs `sa-run` and resave 5/5 at the merge. Models: sonnet
for C.2 to C.6 (mechanical), opus for C.1 and C.7.

1. **`tools/abi_audit.py` (opus).** Read-only scanner; no tree change. (i) For every class with a
   `static_assert(sizeof|offsetof)`, list member declarations typed `long`, `unsigned long`, `wchar_t`, `size_t`,
   `ptrdiff_t`, `long double`, a Win32 typedef, or a bit-field; fail on any. Baseline today: 1
   (`WideString::_data`, until C.5). (ii) Count the §A patterns outside `src/platform/win` and `src/render/dx11`
   against a checked-in `tools/abi_audit.baseline`; fail on growth. (iii) `--layout`: compile one TU per pinned
   header with `-fdump-lang-class` and list every derived class whose base has `size` greater than `base size`
   (dsize): the tail-padding candidates. Today that reproduces the documented ones; a clang-cl build would need it.
2. **Portability header** `src/engine/abi.h`, included by `global_types.h`: central asserts for the data model
   the layouts assume (`sizeof(void*) == 8`, `int` 4, `long long` 8, `float` 4, `double` 8,
   `alignof(int64_t) == 8`, `CHAR_BIT == 8`, little-endian, `char` signed), and `RT_ASM_LABEL(sym)` pasting
   `__USER_LABEL_PREFIX__`. Route `exe_type.h:131` and the 24 test labels through it. B11 identical.
3. **Bare `long` -> `int32_t`, class (b) only:** src/engine + src/game + tests (~45 code lines), then src/app +
   src/diag (~30). Change the matching format strings in the same commit (`%ld` -> `%d`), or the 0-new-warnings
   invariant breaks. Leave class (c) (HRESULT, SEH filter, `DWORD` casts) to the seams.
4. **Dead `<windows.h>` includes:** delete the ~14 candidates in src/game. A file that then fails to compile was
   not dead: restore it and hand it to its seam. B11 must be identical; a DIFF means a `min` / `max` macro was in
   use: read it.
5. **`WideString` over `char16_t`,** after SETTINGS has deleted the registry arm (fewer platform sites remain):
   `WideString.h`, `widestring.cpp`, `value_types.cpp:425`, `updateloop_loadresources.cpp:136`,
   `modification_setupfilesystem.cpp:284`, the asserts at `WideString.h:60` and `system_abi_test.cpp:418`. Each
   remaining Win32 W-API call in `src/platform/win` gets one `reinterpret_cast<const wchar_t*>` with
   `// CAST-OK: win32`; castmetric unchanged (ruled lines).
6. **`_stricmp` -> `rtcrt::StrICmp`** for the 13 callers, declared in a header beside `rt_libm.h`. It is neutral
   only where the host CRT and retail agree in the "C" locale (documented at `msvc_stricmp.cpp:24`). Title it
   `behaviour:`; B11 will show the call target DIFF, read and explained.
7. **The Linux dry run (opus).** `-fsyntax-only` first, then a link and run of the unit suite. Toolchain: WSL2
   Ubuntu 24.04 with `g++-13` (or clang-18) over the tree at `/mnt/c/...`; a CMake flavour `linux-check` with
   `-std=gnu++17 -fexceptions -fno-lifetime-dse -fno-strict-aliasing -mms-bitfields`. Scope: `src/engine`,
   `src/game`, `tests/unit`. Excluded: `src/platform/win`, `src/render/dx11`, `src/diag`, `src/app`. One
   `src/platform/stub/` TU of link stubs covers seam functions with no POSIX body yet. Output: the list of
   remaining blockers (expected: the seam files above, `%lld` warnings, `long long` vs `int64_t` overload misses),
   each filed against its seam. Pass criterion: unit 920 / 0 under Linux GCC, which proves every layout assert
   holds there. A game run and the resave gate on Linux wait for SDL / THREADS / RENDER.
8. **Close:** in `.claude/rules/type-headers.md`, replace "MinGW GCC x64 uses the same Microsoft x64 ABI as MSVC"
   with the Itanium-layout fact and the pinned-member rule from §B; add `tools/abi_audit.py` to the gate kit;
   record the counts in STATUS.md.

Eight commits. Steps 2, 3, 4 and 6 can run beside the seams. Step 5 waits for SETTINGS. Step 7 waits for THREADS'
POSIX allocator twin (§A.5 item 1) or stubs it.

## D. Risks and user questions

Questions that change the work:

1. **Is WSL2 available, and may an agent install a Linux toolchain in it?** MSYS2 ships no Linux-targeting
   compiler or sysroot, so step 7 has no other cheap route. Without WSL, step 7 becomes "the first Linux CI run".
2. **Does `src/diag` go to Linux?** It is 22 Win32-heavy files (watchdog, redzone allocator, sampler, crash and
   hang handlers, window drivers). Recommendation: Windows-only for the first Linux build, ported on demand.
   Porting it would be a seam of its own.
3. **`_stricmp` (step 6):** the ported retail body everywhere (faithful; recommended), or the host CRT on Windows
   and `strcasecmp` on POSIX?

Risks recorded, not decided (the ARM targets of ENDGAME §E: Apple Silicon, Android):

- **AArch64 PMF encoding differs** (the virtual flag lives in `adj`, not `ptr`): `VSlot.h` and `slot_check.h`
  (51 users) would decode wrong. Branch on `__aarch64__` when an ARM build starts.
- **`char` is unsigned on AArch64 Linux / Android:** build with `-fsigned-char`; the C.2 assert catches it.
- **macOS `pthread_mutex_t` is 64 bytes** and does not fit the opaque 0x28 lock buffers: THREADS needs a smaller
  lock there (`os_unfair_lock`, or a pointer to a heap mutex).
- **`msvc_libm.cpp` uses SSE intrinsics:** the ported retail math is x86-only; ARM needs a scalar path, and
  cross-ISA float determinism is a fidelity question for that day.
- **dxvk-native and calling conventions:** the dx11 code must compile against dxvk-native's Windows headers on
  Linux, which define `WINAPI` / `STDMETHODCALLTYPE` for SysV. That is RENDER's; its loader typedefs
  (`devicedata.cpp:205-206`) are the only calling-convention keywords that stay meaningful.
