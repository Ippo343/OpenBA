# Phase D gate table (coordinator; one row per gated merge / move, written BEFORE the gate runs)

Kit: copied from `b9_session` at the Phase D start (`gate.sh`, `mergegate.sh`, `stepclose.sh`, `worldcmp.sh`, the
route scripts, `keepboth.py`, `reftable_diff.py`, `RESOLVE_CACHE_GONE`); `runs/baseline` = B8's close run (as B9),
warning baselines `runs/warn_baseline_{debug,perf}.txt` (B9's). `runs/` is gitignored. B11 base for the blocks:
`b9_session/runs/b11base` (main at the B9 close; Phase D's first commits are docs / Makefile-rule only).

Invariants (B9 close): new game 5683 / 0, tutorial 5773 / 12, typetable `965b527a`, resave 5/5 == golden
`994eb184a8e88db9372b5b9cd13a9ed9`, unit 920 / 0, 0 new warnings, castmetric 2.

| row | branch / step | merge commit | gate problems | new game | tutorial | typetable | resave | unit | warnings | castmetric | routes | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
