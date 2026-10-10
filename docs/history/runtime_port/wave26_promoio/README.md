# WAVE 26 lane PROMOIO — run 1 evidence

`run1_evidence.txt` is the committed extract from that run's
`standalone_debug.log`; the full log and `standalone_error.log` were written to
this directory but are excluded by `.gitignore:37` (`*.log`) and are NOT in the
repository. The extract carries the lines the lane report quotes, with their
original line numbers, so a reader can locate them if the full log is recovered.

## Provenance of run 1

```
branch      wave26/promoio  (off wave26/base 7de42dd1)
exe md5     a4f5607664f4c61d3f2a3bba16af472b
            — built here AND, checked immediately before launch, deployed at
              C:/GOG Games/Banished/reimpl-x64.exe with the SAME md5.
              This is the mandatory per-run check from the Wave 26 kickoff's
              standing warning 1; the game-dir lock was held as `lane-PROMOIO`
              for the whole deploy-and-run, acquired blocking (never piped,
              never backgrounded) and released afterwards.
env         BANISHED_W17_ATTEND=1 BANISHED_AUTOSTART=1
            BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
            BANISHED_RT_MINT_SHADOW=1
result      exit 0 · standalone_crash.log 0 bytes
            · full autostart -> in-game -> pause menu -> quit
mods        0 .pkm mounted (the lock reports this; it changes the INPUT of a run
            and is not covered by the lock)
```

## What the extract contains

* `[RtMint] promoted 44 …` and the vtable promotion guard line — the promotion
  count and the guard's own effectiveness counter.
* every `[RtMintShadow]` per-row readiness line, including the `[PROMOTED]`
  marker and the named TRAP slots of the rows that are still BLOCKED.
* the whole `[W17Attend]` door-attendance table, with its GREEN MainLoop control.

## What it does NOT establish

`post` counts DOOR OPENS, not dispatches, and `post==N` is not "ran N times" —
nearly every consumer caches in a function-local static. A READY line says the
table is safe to stamp, not that anything dispatches it. Neither is a
verification that a slot BODY agrees with retail's; that is a separate tier
(`runtime/src/io/file_stream_slots_probe.cpp` and friends) and was not run.

Full analysis: `../WAVE26_LANE_PROMOIO.md`.
