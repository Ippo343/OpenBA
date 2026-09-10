# Wave 19 phase 0c — lane TRAPS evidence

Report: [`../WAVE19_PHASE0C_TRAPS.md`](../WAVE19_PHASE0C_TRAPS.md).
All three runs on `ebbec6e`, debug standalone, DLL present, under
`tools/game_dir_lock.sh run lane-TRAPS 1800 -- …`.

| file | what it is |
|---|---|
| `run_shadow.sh` | the runner: `assert-held`, deploy, launch with env, `boot_state.sh --wait 300`, wait for self-quit, snapshot logs. Takes an output dir then `KEY=VAL` env pairs. |
| `run_batch.sh` | runs run2 and run3 back to back under ONE lock |
| `run1_full_debug.log` | run1 (default flags) — the complete `standalone_debug.log`, the primary measurement |
| `readiness_exit_run1.txt` | run1's exit-phase `[RtMintShadow]` report on its own |
| `run2_attend/extract_instruments.txt` | run2 (`BANISHED_W17_ATTEND=1`) — every `[W17Attend]`, `[RtMintShadow]` and `[Encoding] ATTENDANCE` line |
| `run3_ctrl/extract_instruments.txt` | run3 (`BANISHED_RT_BINWRITER=0`) — the positive control, same lines |
| `run*/exit.tbl` | the 57 readiness rows, one per line, for diffing |

**All three runs exited 0 with a 0-byte `standalone_crash.log`.** The empty
crash logs were deleted rather than committed; `run1_full_debug.log` ends with
`[Standalone] === GameMain returned 0 ===` and the clean exit-phase report.

Reproduce: `mingw32-make standalone-debug`, then

```
tools/game_dir_lock.sh run lane-TRAPS 1800 -- docs/runtime_port/wave19_phase0c/run_shadow.sh <outdir> BANISHED_RT_MINT_SHADOW=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
```
