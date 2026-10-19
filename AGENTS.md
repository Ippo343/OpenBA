# AGENTS.md — OpenBA

OpenBA is a GPLv3, source-only reimplementation of Banished's 1.0.7 engine. It contains no retail
code, binaries, or game assets; it runs beside a user-supplied Banished install (`BANISHED_DIR`,
default `C:/GOG Games/Banished`). This is Ippo343's fork; upstream reference is `fran-cap/OpenBA`.
Read `CLAUDE.md` first — its hard rules and per-function porting workflow still govern changes.
This file records scan results and setup facts that are expensive to rediscover.

## Security baseline (full-tree scan, 2026-10-19)

- Static scan of HEAD plus all git history, and a Windows Defender custom scan: no malware, miner,
  backdoor, persistence, credential access, or exfiltration. No PE/ELF/ZIP payloads in the tree or
  history, no hidden refs/unreachable objects, no disguised binaries.
- The game has no network stack: no Winsock/WinHTTP/WinINet/curl/OpenSSL, sockets, telemetry, or
  auto-update. Network-capable code is dev-only: `tools/fetch_dxvk_native.sh` (pinned GitHub release
  plus SHA-256) and `.mcp.json` (`npx vscode-as-mcp-server`, unpinned; the local Ghidra/RenderDoc
  bridge scripts are absent or gitignored in a fresh clone).
- No stagers or obfuscation: no base64/PowerShell-encoded blobs, remote `eval`/`exec`, hardcoded
  external IPs, RTL/zero-width filenames, or image-appended payloads. The few `\xNN` literals are
  character/cursor tables, not encoded code.
- Deliberate, benign, AV-looking code — do not remove without reading it: `src/diag/` VEH crash
  handler, hardware watchpoints/debug registers, thread snapshots, sampling profiler, redzone
  allocator, and mouse/keyboard injection harnesses; `Application::LaunchProcess`
  (`ShellExecuteW`/`CreateProcessW`, currently no callers); `GetAsyncKeyState` input polling with no
  logging or transmission.
- Registry use is read-only: legacy settings import and font-path lookup. Persistent state lives in
  `Documents/Banished` and the game directory. No Run keys, services, scheduled tasks, or startup
  writes.
- Tooling side effects are real: `tools/stress_run.sh` can `taskkill //F //IM OpenBA.exe` (all game
  processes); `tools/build.sh deploy/run` overwrite `$BANISHED_DIR/OpenBA.exe`; `tools/ach_reset.sh`
  edits `Documents/Banished/settings.cfg`. `docs/` contains profiling notes that recommend `bcdedit`
  to disable Hyper-V — documentation, not code; do not execute blindly.
- Keep new code free of network calls, telemetry, persistence, and committed binaries unless the user
  explicitly changes the threat model. No prebuilt exe exists here; if a release zip is used, scan
  that binary separately.

## Build/run facts that bite

- `tools/build.sh` is the only entry point; there is no Makefile. Flavours are `debug` (default),
  `og`, `perf`, `release`; `stress` defaults to `perf`. `run` passes `/profile:faithful` (all
  extensions off); `run-menu` does not.
- `deploy` overwrites `$BANISHED_DIR/OpenBA.exe` and deletes `standalone_debug.log`. Logs and boot
  state are written in the game directory, not the repo. Poll with `tools/boot_state.sh`; do not
  grep-until-timeout.
- Git submodules `third_party/SDL` and `third_party/freetype` are empty in a fresh clone and must be
  initialised before configuring. They are pinned to genuine upstream releases (SDL 3.4.18,
  FreeType 2.13.3). Linux additionally needs `tools/fetch_dxvk_native.sh`.
- All `BANISHED_*` flags are inventoried in `src/app/options.h`, read once by `OptionsInit()` and
  accessed through `Opt()`. Never call `getenv` elsewhere; adding a flag is discouraged.
- `tests/unit/sources.cmake` is hand-maintained. A body used by a test must be listed there or the
  failure is at link time, not compile time.
- Perf/release builds force-include `src/app/log_strip.h`; a qualified `std::printf` breaks them
  (`tools/check_log_strip.py` is the guard). All TUs also get `src/app/one_write_stdio.h`, which
  macro-wraps `fprintf`/`vfprintf`/`printf` on MinGW.
- Classes in `src/engine` are ABI-pinned: GCC/Itanium layout must reproduce retail MSVC offsets
  (`static_assert` on every field; `src/engine/abi.h`). Do not substitute `long`, `size_t`, native
  handles, or bit-fields; `tools/abi_audit.py` enforces it.
- The port is behaviour-faithful: do not add guards for retail's unguarded dereferences without
  identifying the upstream null (CLAUDE.md hard rule). There is no address registry: ported bodies
  are normal C++ calls, and `// PORT GAP` marks a missing body.

## Where to look

- Current state and next work: `docs/runtime_port/STATUS.md`, `NEXT_SESSION.md`, `DIRECTION.md`,
  `ENDGAME.md`.
- Extension model and inventory: `docs/extensions/DESIGN.md`, `LIST.MD`, `KITFREE.md`;
  `src/ext/registry.*` is the extension registry, not the Windows registry.
- Architecture and retail facts: `docs/reference/engine_architecture.md`, `binaries.md`,
  `environment.md`. `decomp/` holds inventories, not decompiled binaries.
- Historical paths (`code/`, `runtime/`, `standalone/`, `engine/`, `dx11/`) under `docs/history/` are
  pre-restructure and do not exist now; treat history as evidence, not current layout.
