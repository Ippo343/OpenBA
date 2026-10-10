# Hand-in: block ABI (Phase D.4 wave 1; plans/ABI.md steps C.1-C.4)

Branch `d4-abi`, base `f0c5032d0`, main merged at `fcb4a996f` (docs only). Static gate only: no game
run. Every commit: `-j2` debug / perf / unit exit 0, unit 920 / 0, 0 new warnings against main's
`runs/warn_baseline_{debug,perf}.txt`, castmetric (`castmetric.py src`) 2. Python: the system
`python` 3.12; the `tools/ghidra_mcp/.venv` interpreter does not exist in the worktree.

## Commits and B11 verdicts (perf objects against `runs/b11base_d4/perf/obj`)

| commit | step | B11 |
|---|---|---|
| `832f8b1cd` build: tools/abi_audit.py + baseline | C.1 | no source touched |
| `54320f55a` refactor: src/engine/abi.h, RT_ASM_LABEL | C.2 | every object (global_types.h reaches nearly all): 931 IDENTICAL (16,934 functions); 3 objects hold no function on either side |
| `ece02bd3f` refactor: long -> int32_t, engine + game + tests | C.3 part 1 | IDENTICAL 487, RENAMED-IDENTICAL 7, SAME 9, RENAMED-SAME 2, DIFF 0 |
| `06e3a9a5d` refactor: long -> int32_t, app + diag | C.3 part 2 | IDENTICAL 157, RENAMED-IDENTICAL 1, SAME 9, DIFF 0 |
| `3e53b937c` refactor: dead `<windows.h>` in src/game | C.4 | IDENTICAL 243, SAME 4 (the C.3 ones), DIFF 0 |

**The SAME verdicts (C8 asks for IDENTICAL; read, not papered over).** Every SAME and RENAMED-SAME function
(citizen_updatehealth 4, keystone_a_map 1, spawncomponent 3, autosavedialog 3, perf_experimental 8,
scene_watch 1) is in a TU whose `%ld` format strings became `%d`. The strings shrink, so the `.rdata`
items placed after them (float constants, other literals) move, and the `%rip` displacements that reach
them change. With the displacements, the jump and call targets and the objdump comment addresses
normalised, all 20 functions are instruction-for-instruction identical to the base (0 diff lines). The
normaliser is `fnnorm.sh` in the block's scratchpad. The RENAMED-IDENTICAL rows are each TU's
`_GLOBAL__sub_I_*` static-init function, which b11pair pairs by position.

## Deviations from the plan

- C.1 (iii) `--layout` runs from a worktree with no build: `g++ -fsyntax-only -fdump-lang-class` on one TU
  per pinning header, the Makefile's include path, about 50 s at `-j 2`. The result is 0 tail-padding
  candidates used as a base, and 8 padded classes that nothing derives from (listed by the script).
  4 headers do not compile standalone: Game/PlayerToolManager.h, Game/WorkScheduler.h, Work/CellBst.h
  and Work/TradeWork.h. They lack includes of their own; that is not ABI's to fix.
- C.1 (i): a class name is resolved in the asserting file first, then in headers only. Without that
  rule, two file-local `Entry` structs were false hits. The baseline is 1 row, `WideString::_data`.
- C.2: the plan counted 24 test labels; the tree has 22.
- C.3: three `(long)` casts in diag are left. They are at boot_state_watchdog.cpp:342 and :379 and
  redzone_alloc.cpp:1163. Each prints a Win32 `LONG` fed by `Interlocked*`, the same shape as the
  DWORD casts the coordinator assigned to the seams. `orchard_diag.cpp:90` and the `gameupdateloop.cpp`
  `atol` hold a CRT `long` result in an `int32_t`; that is identical on Win64. The value was already
  narrowed to `int` (orchard), or it is a frame number (screenshot).
- C.4: 12 of the 14 includes were dead. Two were not and are restored (below).

## Class (c) lines left, by seam

| line | what | seam |
|---|---|---|
| engine/engine/engine_lifecycle.cpp:222 | CoInit returns HRESULT as `long` | THREADS |
| engine/system/Debug.h:50-51 | SEH filter, `MsgBox` | crash handler / SDL |
| game/Game/gameupdateloop_load.cpp:731 | `GetCurrentThreadId` as `unsigned long` | THREADS |
| app/main.cpp:53-54 | `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance` exports | app (SDL) |
| app/main.cpp:882,885 | HRESULT as `long` | RENDER |
| diag/boot_state_watchdog.cpp:161,211,243 | DWORD casts for printf | THREADS / diag split (C10) |
| diag/boot_state_watchdog.cpp:342,379; diag/redzone_alloc.cpp:1163 | Win32 `LONG` cast for printf | THREADS / diag split |
| game/Dialog/mainmenudialog.cpp:709 | `PostQuitMessage(0)`, the Quit fallback (keeps `<windows.h>`) | SDL (W3) |
| game/Entity/componententity.cpp:474-477 | `VirtualQuery` readable-page probe in a serializer desync diagnostic (keeps `<windows.h>`) | THREADS (memory query) or diag |

## Open items for the coordinator

- `tools/abi_audit.py` is not in the gate kit yet; that is step C.8. Run it as `python tools/abi_audit.py`.
  It exits 1 on a NEW pinned member or a pattern count above the baseline. The baseline was lowered
  in each commit: long app 7 -> 4, diag 26 -> 6, engine 6 -> 3, game 29 -> 1, tests 12 -> 0;
  asm_label tests 22 -> 0; win_include game 24 -> 12.
- The baseline counts lines per top-level directory. A SETTINGS / THREADS / SDL merge that moves Win32
  code out of `src/engine` or `src/game` lowers rows: run `--write-baseline` after such a merge. A merge
  that RAISES a row fails the check. Each rise is either a Win32 line outside `src/platform/win` (a
  finding) or a new legitimate row to justify in that commit.
- Merge order (D4_RULES §2): ABI merges after SETTINGS-1 and THREADS-1. My retypes touch
  `gameupdateloop.cpp` (SimProfReport, the screenshot frame) and `citizen_updatehealth.cpp`. THREADS
  touches the game QPC probes in `gameupdateloop.cpp`. Expect line-level conflicts there only; keep both.

## Shared files touched

`src/engine/global_types.h` (one include line), `src/game/Object/exe_type.h` (an include and the
DEFINE_EXE_VTABLE label), `src/engine/object/VSlot.h` (return type), `tests/unit/slot_check.h`.
The new files are `src/engine/abi.h`, `tools/abi_audit.py` and `tools/abi_audit.baseline`.
