# Banished x64 — Progressive Reimplementation

Keep this file under ~120 lines: rules and pointers only. No status, counts, dates or
history here — those go in `docs/runtime_port/STATUS.md`. Procedures go in
`.claude/skills/`, area-specific rules in `.claude/rules/`, facts in `docs/reference/`.

## Goal
Reverse engineer Banished (Shining Rock Software) and replace it with clean, readable C++
that mirrors the game's logic and compiles to a replacement exe — like OpenRCT2, re3 and
devilution. `reimpl-x64.exe` is fully standalone: it loads no retail binary.
Runtime-x64.dll is ported (`runtime/`), VideoDX11-x64.dll is replaced (`dx11/`), game
logic lives in `code/Game/`. There is no address registry: a call is a C++ call, and a
function with no body yet is a `// PORT GAP 0x<addr> <name>` comment at its call site.

## Current work — read these, don't restate them here
- **Where things stand / what is next:** `docs/runtime_port/STATUS.md`
- **Phase plan** (flag cleanup, real C++ classes): `docs/runtime_port/ENDGAME.md`,
  rules under `docs/runtime_port/endgame_b/`
- **How work is chosen and measured:** `docs/runtime_port/DIRECTION.md`;
  facts and the per-function recipe: `docs/runtime_port/PLAN.md` §2, §3, §6
- No waves, lanes, new instruments or new `BANISHED_RT_*` flags (DIRECTION.md §2).

## Build, run, gates
Use `mingw32-make` (`/c/msys64/ucrt64/bin/mingw32-make.exe`).
```bash
mingw32-make standalone-debug  # debug exe -> standalone/build/reimpl-x64.exe
mingw32-make standalone        # release exe
mingw32-make sa-deploy         # debug build + copy to C:/GOG Games/Banished
mingw32-make sa-run            # THE GATE: deploy + launch, auto-starts a new game
mingw32-make sa-run-menu       # same, stops at the main menu
mingw32-make unit              # unit tests
mingw32-make clean
tools/resave_gate/resave_gate.sh Gate   # serializer regression check (README alongside)
```
`sa-run` is the default way to verify any gameplay / rendering / world-build change.

## Reimplementation workflow (per function)
1. **Decompile** in Ghidra: rename, annotate, document the struct layout (`ghidra` skill)
2. **Type header**: binary-compatible class, `static_assert` on every offset
   (`runtime/types/<ns>/` for engine classes, the module's header in `code/Game/`)
3. **Write clean C++** in `code/Game/<Module>/` — the tree mirrors the original source layout;
   declare it in the class's header (or the module's `exe_bodies.h`, with its retail address)
4. **Call it by name** from its callers: include the declaring header, never a local `extern`.
   Replace the caller's `// PORT GAP` arm in a commit titled `behaviour:`
5. **Test**: `mingw32-make sa-run`, then check boot state and logs (below)

## Hard rules
- **Don't "fix" a faithful crash with a null check.** When a byte-faithful function
  crashes on an unguarded deref, retail is not wrong — something upstream handed us a
  null. Find that. Retail's unguarded derefs are load-bearing; a guard doesn't restore
  the behaviour, it skips it. Guard only where our standalone genuinely differs from
  retail (e.g. an entity legitimately lacking a component because a panel template is
  shared), and say so in the comment.
- **Never `getenv`.** Flags are read once at boot into `Options`; read `Opt().field`.
  Every `BANISHED_*` flag and its reader is listed in `standalone/options.h`. There are
  no live-feature env gates.
- **Poll a run with `tools/boot_state.sh --wait 300`, never a grep-until-timeout loop.**
  A quiet log does not mean it is loading: a modal dialog blocks boot while the process
  looks alive.
- **`[AutoStart]` and env-gated diagnostics go to `standalone_error.log`**, not
  `standalone_debug.log`. A grep of the wrong log returns 0 on a healthy run.
- **When the exe dies, read `standalone_crash.log` first** (exit 3). Exit 4 +
  `standalone_hang.log` means it froze.
- **A pixel diff between two runs measures nothing** (frames are not reproducible, even
  with seed + camera pin). Compare with `tools/trace_oracle/`.
- **No doors.** Nothing registers or resolves a body by retail name or address
  (`docs/runtime_port/endgame_b/b3r_session/acceptance.sh` must stay at target). Wiring a
  body into a `PORT GAP` arm changes behaviour: read the consumer first, title it `behaviour:`.
- **Never run `find /`** or any unscoped filesystem search; it has contaminated perf A/Bs.
  Use Grep/Glob scoped to the worktree. Memory lives at
  `~/.claude/projects/C--code-GIT-untitled-rev-eng-project/memory/` (index `MEMORY.md`;
  history in `archive/`, open only when a task needs it) and `~/.claude/memory/`.
- MSYS2 packages are stale: don't `pacman -S`.

## Subagents
Use a cheaper model where the task allows: `haiku` for file/log searches, greps and
lookups; `sonnet` for mechanical edits following an established pattern and for Ghidra
lookups; inherit the main model only for porting, debugging and design. Subagents may not
see this file's context — put the relevant hard rules in the prompt.

## Logs (all in `C:/GOG Games/Banished/`)
- `standalone_debug.log` — stdout (TRACE/printf, unbuffered)
- `standalone_error.log` — stderr (survives abnormal exit; `[AutoStart]`, `[BootState]`)
- `standalone_crash.log` — VEH crash dump · `standalone_hang.log` — watchdog thread stacks
- `standalone_boot_state.txt` — one-line machine-readable boot state

## Where everything else lives
| Need | Read |
|---|---|
| Crash / hang / stuck boot, exit codes, symbolizing | skill `triage-run` |
| Clicking menus unattended, autostart internals, modal answers | skill `menu-driver` |
| Perf runs, stress test, profilers | skill `stress-test`, `perf/README.md` |
| Framing a screenshot, rendering verification | skill `camera-pin`, `tools/trace_oracle/README.md` |
| Ghidra setup and naming conventions | skill `ghidra`, `HOW_TO_START_GHIDRA.md` |
| Type header ABI rules | `.claude/rules/type-headers.md` (auto-loads on `*/types/`) |
| Retail binary facts, import table, modding toolkit | `docs/reference/binaries.md` |
| Main loop, launcher dialog, threading, address resolution | `docs/reference/engine_architecture.md` |
| Toolchain versions, VS Code / MCP debugging | `docs/reference/environment.md` |
| Per-binary function inventories | `decomp/README.md` |
| Known retail bugs | `docs/retail_bugs.md` |
| Known / suspected bugs in OUR port, not yet fixed | `docs/known_port_issues.md` |
