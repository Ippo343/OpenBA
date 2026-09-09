# WAVE 12 — the Runtime `Application` namespace to its ceiling

Opened 2026-09-09 from `wave10/base` tip `58f9634`. Wave 11 (object creation path)
stays parked; this wave is a small, fully-enumerable namespace and is being closed first.

## The whole namespace, enumerated (not estimated)

`Runtime-x64.dll` export table, filtered on `Application` (`pefile`, run today):
**26 functions + 1 data export**. `tools/runtime_census.py` reads `total 26` for this
namespace, and the arithmetic below closes exactly against it — so unlike most waves,
the denominator here is a MEASUREMENT, not a survey.

### Already ported (10) — DO NOT re-port; grep before you write
| export | addr | where |
|---|---|---|
| `??0Application@0@QEAA@PEAX@Z` (ctor) | 0x18009f590 | `runtime/src/engine/singleton_ctors.cpp` |
| `?IsPaused@Application@1@QEAA_NXZ` | 0x18009f4e0 | `runtime/src/application/application.cpp` |
| `?Quit@Application@1@QEAAXXZ` | 0x18009f4d0 | `runtime/src/application/application.cpp` |
| `?PlatformUpdate@Application@1@AEAA_NXZ` | 0x18009fdf0 | `application_win.cpp` |
| `?SleepThread@Application@1@QEAAXH@Z` | 0x18009f850 | `application_win.cpp` |
| `?YieldToThreads@Application@1@QEAAXXZ` | 0x18009f920 | `application_win.cpp` |
| `?RegisterMessageCallback@…` | 0x18009fa60 | `message_callbacks.cpp` |
| `?UnregisterMessageCallback@Application@1@QEAAXI@Z` | 0x18009fa20 | `message_callbacks.cpp` |
| `?PlatformStartup@Application@1@AEAA_NXZ` | 0x18009fc10 | `platform_startup_win.cpp` |
| `?WindowProc@Application@1@CA_JPEAUHWND__@@I_K_J@Z` | 0x18009f930 | `platform_startup_win.cpp` |

### Remaining (16) — this wave's entire scope
| # | export | addr | lane |
|---|---|---|---|
| 1 | `?Get@Application@1@SAPEAV11@XZ` | 0x18000a2f0 | A |
| 2 | `?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ` | 0x18009fb90 | A |
| 3 | `?GetWindow@Application@1@QEBAPEAUHWND__@@XZ` | 0x18009fba0 | A |
| 4 | `?IsCursorEnabled@Application@1@QEAA_NXZ` | 0x18000a0c0 | A |
| 5 | `?PlatformIsPaused@Application@1@AEAA_NXZ` | 0x18009fe80 | A |
| 6 | `?PlatformDestruct@Application@1@AEAAXXZ` | 0x18009fc00 | A |
| 7 | `?Update@Application@1@QEAA_NXZ` | 0x18009f4f0 | A (thunk → 0x18009fdf0) |
| 8 | `?Shutdown@Application@1@QEAAXXZ` | 0x18009f500 | A (thunk → 0x18009fd70) |
| 9 | `?Startup@Application@1@QEAA_NXZ` | 0x18009f510 | A (thunk → 0x18009fc10) |
| 10 | `??1Application@0@QEAA@XZ` (dtor) | 0x18009f520 | B |
| 11 | `?PlatformShutdown@Application@1@AEAAXXZ` | 0x18009fd70 | B |
| 12 | `?PlatformConstruct@Application@1@AEAAXPEAX@Z` | 0x18009fbe0 | B |
| 13 | `??0Application@0@QEAA@AEBV00@@Z` (copy ctor) | 0x18000a0d0 | B |
| 14 | `??4Application@0@QEAAAEAV00@AEBV00@@Z` (operator=) | 0x18000a160 | B |
| 15 | `?EnableCursor@Application@1@QEAAX_N@Z` | 0x18009f800 | C |
| 16 | `?LaunchProcess@Application@1@QEAA_NPEBD_N@Z` | 0x18009f5e0 | C |

10 + 16 = 26. That closes against the census `total`.

### The data export — the likely ceiling
`?_singleton@Application@1@0PEAV11@EA` @ `0x1801fc678` is DATA, not a function. It is
already wired (exe dispatch slot `0x140152368` → `ResolveAddr(Module::Runtime,0x1801fc678)`).
The name→function seam cannot redirect it before cutover. If it is the only row left in
the census `--unported` list at the end, it is the CEILING, and the wave says so with
evidence — it does not get counted as a port.

## Ground truth already harvested for you (read these, don't re-derive)
- `<SCRATCH>/app_disasm_main.txt` — objdump -d -M intel of 0x18009f4c0..0x1800a0000
- `<SCRATCH>/app_disasm_inline.txt` — objdump -d -M intel of 0x18000a0c0..0x18000a320

`objdump -d -M intel --start-address=0xA --stop-address=0xB "C:/GOG Games/Banished/Runtime-x64.dll"`
works directly and gives clean, correct disassembly. **Prefer it to Ghidra's decompiled C**
(memory: Ghidra's C drops dereferences and prints MOVSXD as a zero extension — 3 defects
in Wave 6 came from trusting it).

## Rules for this wave (each one cost an earlier wave real time)
1. **Your worktree is yours.** Work ONLY in the path named in your prompt, on the branch
   named in your prompt. Never `git checkout` another branch, never edit the main checkout.
   Report BOTH the worktree path and the branch name in your final report.
2. **Ghidra is a SHARED single instance.** It is already switched to `Runtime-x64.dll`.
   **Do not call `switch_program`** — another lane is using it. If you need the profile
   build, ask the integrator instead. Prefer objdump.
3. **Grep for the port before writing it.** Three times in one earlier wave a lane ported
   what the tree already had; a duplicate registration is last-binding-wins and silently
   makes one body dead code. Grep the mangled name AND the address across
   `code/ engine/ runtime/ standalone/ dx11/`.
4. **`AEAA` means PRIVATE, not unexported.** This DLL exports its private members. Never
   justify an address door with the word "unexported" — check the export table.
5. **Oracle discipline.** `RT_DIFF_*` against the real DLL is the oracle. Read the
   **`calls=`** column, not just `divergences=` — a port with zero callers reports clean.
   `calls=0` with `worked>0` means the body ran and always took the branch the oracle does
   not cover. If you cannot get a caller, say so explicitly and propose a synthesised
   oracle (a body with no gameplay route can still have one — that has found a REAL defect).
   `RT_DIFF_SHADOW` sites are SELF-CHECKS, not oracles: they never consult the DLL.
6. **Do not "fix" a faithful crash with a null check.** Find the upstream null.
7. **Never use a bash heredoc** for file content or commit messages — it silently rewrites
   backslashes and runs backticks. Use the Write tool + `git commit -F <file>`.
8. **`tools/game_dir_lock.sh`** — if you need to deploy/run the game, acquire the lock and
   **never pipe the acquire** (a pipeline returns the LAST command's status, so a TIMEOUT
   reads as a GRANT). Do not background a blocking acquire.
9. **Report honestly.** If a function is ported but unverifiable, say PORTED-NOT-VERIFIED
   and name the missing route. Do not report a lane finished and then keep editing.

## Per-function recipe
`docs/runtime_port/PLAN.md` §6. Short form: types with `static_assert` on every offset in
`runtime/types/application/Application.h` → byte-faithful body in
`runtime/src/application/<file>.cpp` (`_win` suffix = the Win32 platform seam; keep
`<windows.h>` out of every other file here) → `RegisterRuntimeExport` + `RegisterRuntimeAddr`
in `rt_application_port.cpp` → build → run with `BANISHED_RT_DIFF=1` → record.

## Definition of done for the wave
`tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py` reads
`Application  26  13  13  …  100.0%` on the called side, and a separate stated count of
26/26 functions registered — or a named, evidenced ceiling for each row that is not.

## Integrator verification of the denominator (2026-09-09, before dispatch)

Two INDEPENDENT counts agree, which is why this wave states 26 as a measurement:

* `pefile` over retail `Runtime-x64.dll`: 29 exports whose name contains `Application`.
  Removing `?GetApplication@Engine@1@` (Engine ns), `?_stopApplication@Error@System@@`
  (System ns) and the DATA export `?_singleton@Application@1@` leaves **26 functions**.
* `docs/runtime_port/survey_ghidra_census.md` line 58, a live Ghidra count:
  `| Application | 1 class | 26 | 26 | 2,450 bytes |`.

So for THIS namespace `exported == total`: there is no unexported-internal residue to
discover later. That is unusual and it is what makes a 100% claim checkable here.

### Three unexported neighbours that are NOT in scope
Ghidra finds function starts at `0x18009f420`, `0x18009fbb0` and `0x18009fe90` inside/beside
the contiguous Application code block (`0x18009f4c0`-`0x18009fe90`). None is exported and none
is counted in the 26. Recorded here so a later reader does not mistake them for missed rows:
* `0x18009fbb0` - 6 instructions; conditionally publishes RCX into the global `0x1801fc680`
  (the slot immediately ABOVE `_singleton` at `0x1801fc678`) and returns 1.
* `0x18009fe90` - a large body on a DIFFERENT object shape (`[rcx]` is a byte flag, `[rcx+8]`
  a handle closed via `0x18014b0a0`); not an Application member.
* `0x18009f420` - starts before the block; its epilogue is what precedes `Quit`.

## The `_singleton` ceiling is REAL, and the recorded reason for it was wrong

`runtime/src/application/rt_application_port.cpp` justifies leaving
`?_singleton@Application@1@0PEAV11@EA` (`0x1801fc678`) unported with: *"a DATA export,
not a function; the seam maps names to functions, so it cannot be redirected before
cutover."* **That reason is stale.** Wave 10 built exactly the machinery it says does not
exist: `standalone/rt_data_mint.cpp` + `reg_mint_promoted.cpp` mint storage for DATA
exports and register them by name, and 17 such rows are promoted and default-on today.
So "we cannot" is false — the honest question is "should we", and the answer is no.

Ghidra `get_xrefs_to(0x1801fc678)`, run 2026-09-09:
**26 in-DLL RIP-relative references**, 2 WRITE and 24 READ, spread across at least four
namespaces — `Application::Application` (ctor, WRITE `0x18009f5b8`), `Startup`
(WRITE `0x1800dbbe2`), `WindowProc`, `Get`, `EnableCursor` (`0x18012258d`),
`Input::PlatformStartup` / `PlatformShutdown` / `PlatformUpdate` /
`PlatformGetKeyboardState` / `PlatformGetMouseState`, and two `Update` bodies.

**We intercept only at DOORS, and a RIP-relative MOV is not a door.** Minting our own
storage for this slot would therefore SPLIT THE BRAIN: our readers would see our copy
while those 26 DLL sites kept using theirs, and the ctor would publish into one of the
two. That is strictly worse than the status quo.

The status quo is correct and already verified in shape: lane A`s `Get` resolves the
EXPORTED NAME through `RT_DATA_SLOT`, i.e. it reads the very slot retail writes, and it
carries a real oracle (`RT_DIFF_SCALAR` against the shipped `?Get@`) rather than a
self-check. Nothing is split today.

**Conclusion:** `_singleton` is the Application namespace ceiling on the census`
called side, giving a ceiling of **12/13 = 92.3%** there, while the FUNCTION port can
still reach 26/26. It is a ceiling for an evidenced structural reason, not for lack of
work — and the reason on record until today was the wrong one.
