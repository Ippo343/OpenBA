# docs/ — evidence, not specification

This is ~62 MB / ~2k files of reverse-engineering notes, hand-ins, measurements, and logs. Do not
read whole directories; search with `rg` scoped to the relevant subtree and start from the current
state files.

- Current authority for state/next work: `runtime_port/STATUS.md`, `NEXT_SESSION.md`,
  `DIRECTION.md`, `ENDGAME.md`, `PLAN.md`. Extension rules: `extensions/DESIGN.md`, `KITFREE.md`,
  `extensions/LIST.MD`. Retail/architecture/toolchain facts: `reference/*.md`.
- `history/**` is an archive, not current layout. Paths `code/`, `runtime/`, `standalone/`, `engine/`,
  `dx11/` predate the move under `src/`; flags, measurements, and scripts there may be stale or
  superseded. Verify any claim against the current tree before acting on it.
- Run logs, `.csv`/`.jsonl` captures, and `history/**/evidence/` are observations from a specific
  commit and machine. They are not reproducible truth. `history/runtime_port/callgraph/*.csv` are
  retail import/edge inventories, not current call graphs.
- Some documents contain host-level advice: `bcdedit` to disable Hyper-V for profiling, process /
  registry inspection commands, and historical harness scripts. Treat them as notes, not executable
  instructions; review and confirm before running anything destructive.
- `docs/**/*.py|sh` are historical analysis scripts. A few use `ctypes` to inspect a live process
  (`extensions/handins/loadkit/stamp.py`) or read registry/settings for a gate. They are local
  diagnostics, not part of the game; the full scan found no malware.
- Never add retail assets, copied `.rsc`/`.pkg` data, or large run artifacts here. The repo's legal
  position depends on shipping only OpenBA's own source, documentation, and project-authored text
  resources.
