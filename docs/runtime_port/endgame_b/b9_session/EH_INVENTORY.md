# B9 Block 0 — the exception / unwind inventory (ENDGAME B.1 decision 7), 2026-10-10

Decision 7 (DECIDED by the user 2026-10-09): where retail UNWINDS, the port unwinds with an ordinary C++
`throw` caught at retail's catch site; where retail returns a STATUS, a plain error return; `setjmp` /
`longjmp` / SEH go; `-fexceptions` on every object; the crash / hang handlers stay Windows-only
diagnostics. This file is the inventory the decision asked for: one row per site in the TREE that uses a
non-C++ unwind, and one row per site in RETAIL that unwinds or catches, with what each one is.

Method. The tree: `grep -rnE 'setjmp|longjmp|jmp_buf|__try|__except|RaiseException|AddVectoredExceptionHandler|
SetUnhandledExceptionFilter|RtlUnwind'` over `code runtime standalone dx11 engine tests`. Retail: a Ghidra
script over each binary's `.pdata` (every `RUNTIME_FUNCTION` whose `UNWIND_INFO` carries a handler): the
handler symbol classifies the frame -- `__C_specific_handler` = an SEH `__try` frame (its scope table read:
begin / end / filter / target), `__CxxFrameHandler3` = a C++ frame (its `FuncInfo` read: `nTryBlocks > 0`
means a `catch`, 0 means destructor cleanup only), `__GSHandlerCheck` = a stack-cookie frame; plus the
xrefs to `_CxxThrowException` (the throw sites). Scripts were inline (`_mcp_inline_EhScan*.java`); the
numbers below are what they printed.

## 1. The tree (main `22b1d9613`): ZERO non-C++ unwind sites

| mechanism | sites in code / runtime / dx11 / engine / tests | sites in standalone/ (the Windows harness) |
|---|---:|---|
| `setjmp` / `longjmp` / `jmp_buf` | 0 | 0 |
| `__try` / `__except` / `__finally` | 0 (MinGW x64 has none; 4 comments mention the absence) | 0 |
| `RaiseException` / `RtlUnwind` | 0 | 0 |
| `SetUnhandledExceptionFilter` / `AddVectoredExceptionHandler` | 0 | `main.cpp:1184-1185` (CrashHandler, FirstChanceFailFast: the crash log, the fail-fast log, the hardware watchpoint), `hw_watch.cpp` |
| `std::set_terminate` | 0 | `main.cpp` (TerminateHandler: the pure-virtual / uncaught-exception log) |
| `ExitProcess` / `TerminateProcess` | `runtime/src/system/error.cpp:177` (the DLL CRT's `exit(code)` at 0x18007f8a8, PORT GAP: `System::Error::Display`'s stop -- a status exit, not an unwind) | the crash / hang / redzone exits (codes 3, 4, 5, 0x122-0x124) |
| `throw` / `try` / `catch` | 0 (every grep hit is a comment or `noexcept` on the String / Filename move members) | 0 |

The ~25 files of 2026-09-13 were the DLL-present oracle and the wave harnesses (`RtSaveTrace::Compare`'s
prefix stop -- memory `reference_longjmp_unwind_runs_retail_cleanup` -- the shadow bisects, the probes):
every one was deleted with ENDGAME A.4 and the registry teardown. Nothing was converted in B9 because
nothing was left to convert.

`-fexceptions`: GCC's default for C++; made EXPLICIT in `SA_CXXFLAGS` and `UNIT_CXXFLAGS` (Makefile) so
the invariant is visible and a future `-fno-exceptions` is a diff, not a default. No TU is built with
`-fno-exceptions`; no `noexcept` sits on a function that can throw (the only `noexcept` members are the
String / Filename move ctor / move assignment, which copy one pointer). `extern "C"` bodies in the tree are
compiled as C++ with unwind tables (a throw through them unwinds; none throws).

## 2. Retail: what unwinds, what catches

| binary | `_CxxThrowException` call sites | C++ frames with a `catch` | C++ cleanup-only frames | SEH frames (`__C_specific_handler`) | of which GAME code |
|---|---:|---:|---:|---:|---|
| Application-x64.exe (0x140...) | 1: `FUN_1401328f8` (the CRT's own `__CxxUnhandledExceptionFilter` region) | **0** | 2321 | 33 (all `0x14003axxx` / `0x140130xxx-0x140141xxx`: `_onexit`, `doexit`, `_freefls`, `__unDName`, `__CxxCallCatchBlock`, ... the MSVC CRT) | **none** |
| Runtime-x64.dll (0x180...) | 1 GAME: `System::Debug::ForceDump` 0x1800c5f00 (see §3) + 1 CRT (`FUN_180128448`) | **0** | 2227 | 57: 56 CRT + **1 GAME: `Video::Device::EnumerateVideoModes` 0x1800f8610** (see §3) | 2 sites |
| VideoDX11-x64.dll (0x180..., replaced by `dx11/`) | 1 CRT (`FUN_18000d53c`) | **0** | 33 | 30 (all CRT) | none |
| the profile DLL (reference only) | 2 (`FUN_180096d00` / `FUN_180096cd0`, the profile build's assert path) | -- | -- | -- | profile-only |

So: **no function in retail's game code catches a C++ exception, and the exe never throws one.** The
2227 + 2321 "cleanup-only" C++ frames are MSVC's unwind tables for locals with destructors; they are
exercised only if something throws through them, and the one throw (§3.1) is a dump trigger that nothing
catches. Decision 7's rule "where retail unwinds, a `throw` caught at retail's catch site" has an EMPTY set
of sites in this game. Error paths in retail are STATUS returns everywhere (a failed open, a missing
resource, `System::Error::Display` + stop), and the port already returns the status.

## 3. The two game-level sites, read

### 3.1 `System::Debug::ForceDump` 0x1800c5f00 (Runtime) -- `throw 0`, caught by nobody

```
1800c5ed0  SUB RSP,0x28
1800c5ed4  MOV dword ptr [RSP+0x30],0      ; the thrown int, value 0
1800c5edc  LEA RDX,[0x1801af4e8]           ; ThrowInfo (catchable type: a 4-byte scalar, size 0 -> int)
1800c5ee3  LEA RCX,[RSP+0x30]
1800c5ee8  CALL _CxxThrowException          ; never returns
1800c5f00  JMP 1800c5ed0                    ; ?ForceDump@Debug@System@@SAXXZ
```

Its own frame is an SEH scope `[1800c5ed4-1800c5eee]` whose filter `FUN_180138540` calls `FUN_1800349a0`
(the minidump writer) and returns 1 (EXECUTE_HANDLER) -- i.e. the throw exists to make the exception
machinery run so that a dump is written. No `catch` in any binary; no caller of ForceDump in the exe's
code (its only xref is the export table). It is a DIAGNOSTIC trigger, not control flow. Port: declared in
`runtime/types/system/Debug.h` (0x1800c5f00) with the other Debug exports, deliberately undefined ("nothing
calls it -- a call is a link error"); the standalone's own crash log is the equivalent diagnostic. Decision
7 classifies it: crash diagnostics, Windows-only, stays as it is. Row 1211.

### 3.2 `Video::Device::EnumerateVideoModes` 0x1800f8610 (Runtime) -- an SEH swallow of a driver fault

Scope `[1800f8678-1800f8683]`, filter `FUN_180141476`:

```
*(param_2+0x30) = param_1;
code = *(int*)*param_1;                 // ExceptionRecord->ExceptionCode
*(param_2+0x20) = (code == 0xC0000005); // EXCEPTION_ACCESS_VIOLATION only
return *(param_2+0x20);
```

Retail wraps ONE instruction -- `MOV RDX,[RAX]`, the vptr load on the `DeviceData` a renderer DLL just
created -- in a `__try`, raises `Debug::_disableHandler` around it so its own VEH stays quiet, and on an
ACCESS VIOLATION lands at 0x1800f8683, releases that DeviceData and continues with the next slot. This is
a HARDWARE FAULT caught by SEH, not a C++ unwind: no destructor runs on that path (the landing pad reloads
`self` / the loop index / the DeviceData from the frame and falls into the normal `_disableHandler = 0`
path). C++ exceptions cannot express it on any compiler without SEH, and decision 7 says SEH goes. The port
(`runtime/src/video/enumerate_video_modes.cpp`, banner "THE `__except` THIS PORT DOES NOT REPRODUCE")
raises and lowers the flag faithfully and does NOT swallow the fault: on a video driver that faults during
mode enumeration retail skips that renderer and we die with a named crash. The only renderer slot the
standalone ever fills is its own in-tree `dx11/`, whose `EnumerateInterfaces` does not fault, so the
window is theoretical on this build. RULED under decision 7: an error-path difference, kept, documented
(row 1210); no VEH trick to re-create the swallow (DIRECTION.md: no new instruments; the banner's reason
stands).

## 4. What decision 7 therefore changes in the tree

1. `Makefile`: `-fexceptions` explicit in `SA_CXXFLAGS` and `UNIT_CXXFLAGS` (this commit).
2. `docs/known_port_issues.md`: rows 1210 (the EnumerateVideoModes swallow, RULED) and 1211 (ForceDump,
   diagnostics) -- the two game-level sites, each with its ruling.
3. `ENDGAME.md` B.1 item 7: marked DONE with this file as the evidence (at the B9 close).
4. Nothing else. No `throw` is introduced (there is no retail unwind to mirror), no `try` (there is no retail
   catch), no SEH is removed (there was none left). The standalone's crash / hang / fail-fast handlers
   (`standalone/main.cpp`, `boot_state_watchdog.cpp`, `hw_watch.cpp`, `redzone_alloc.cpp`) are the
   harness's Windows-only diagnostics and stay.

## 5. Rules that follow (for any future port of a body that unwinds)

- A retail body that calls `_CxxThrowException` is ported as `throw <the thrown type>`; its catch site is
  the function whose `FuncInfo.nTryBlocks > 0` whose try range covers the call chain -- read it with the
  EhScan script, not guessed. In THIS game that set is empty; a new one would be a finding.
- A retail `__try` whose filter tests an exception CODE (0xC0000005, 0xC000008E, ...) is a hardware-fault
  swallow: it cannot be ported portably; it is documented as an error-path difference (row 1210's shape).
- A retail `__try` / `__finally` (target 0 in the scope table) is RAII: the body's cleanup becomes a
  destructor on a local.
- Never `setjmp` / `longjmp`: a longjmp skips the destructors real classes now have
  (`reference_longjmp_unwind_runs_retail_cleanup`).
