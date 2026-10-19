# tools/ — dev harness orientation

Everything here is development tooling, not shipped with OpenBA. Several scripts mutate the game
install, user settings, or running processes. Read a script before running it; none are part of the
game's trust boundary.

- `build.sh` is the single build entry (no Makefile): `build|unit|deploy|run|run-menu|stress|clean`.
  `run` deploys into `$BANISHED_DIR` and launches with `BANISHED_AUTOSTART=1 /profile:faithful`;
  `run-menu` omits the faithful profile. `deploy` overwrites `OpenBA.exe` and deletes
  `standalone_debug.log`.
- `game_dir_lock.sh` serialises use of one shared Banished install across agents/lanes. Do not bypass
  it when launching or deploying; concurrent runs invalidate perf measurements.
- `boot_state.sh` is the status probe. Exit codes: `0` RUNNING, `1` NOT_RUNNING, `2` BLOCKED_DIALOG,
  `3` BOOTING, `4` STALE, `5` HUNG. Logs/state live in the game directory, not the repo.
- `stress_run.sh` is the perf harness. On timeout it runs `taskkill //F //IM OpenBA.exe`, which kills
  **every** OpenBA process on the machine, not just its child.
- `release_package.sh` builds `build/release/` with the exe and user-supplied FrancophilSans TTFs; it
  refuses `.crs`/`.pkg`/`.pkm` so no game content can ride along.
- `ach_reset.sh` deletes `Ach<N>=` lines from `Documents/Banished/settings.cfg`; it does not touch the
  registry (retail's registry is imported read-only once).
- `fetch_dxvk_native.sh` is the only network fetch: pinned GitHub release plus SHA-256, extracted to
  gitignored `third_party/dxvk-native/`. Linux builds need it for the dxvk headers.
- `capture_window.ps1` screenshots the foreground game window; `machine_load.ps1` samples CPU and
  per-process usage. Both are read-only OS queries used by the perf/visual harnesses.
- Build-time helpers are local only: `ext_rsc_embed.py` generates the embedded `.rsc` header,
  `check_log_strip.py` is the release-build guard, and `abi_audit.py` checks pinned layouts.

## Analysis / Ghidra

- `ghidra_mcp/regen_hash_index.py` talks only to `127.0.0.1:8089` (`GHIDRA_SERVER_URL`) and writes
  gitignored hash indexes. `requirements.txt` is standard PyPI.
- `.mcp.json` starts local MCP servers. `bridge_mcp_ghidra.py` and `tools/renderdoc_mcp/` are
  gitignored/absent in a fresh clone; `npx vscode-as-mcp-server` is unpinned, so enabling MCP
  auto-start is a supply-chain exposure, not a repo feature.
- Full scan baseline (2026-10-19): no malware, miner, backdoor, persistence, credential access, or
  exfiltration; no committed binaries. Keep new tooling local-first and network-free unless the user
  explicitly approves otherwise.
