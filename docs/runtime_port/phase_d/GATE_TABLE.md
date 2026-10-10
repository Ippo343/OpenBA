# Phase D gate table (coordinator; one row per gated merge / move, written BEFORE the gate runs)

Kit: copied from `b9_session` at the Phase D start (`gate.sh`, `mergegate.sh`, `stepclose.sh`, `worldcmp.sh`, the
route scripts, `keepboth.py`, `reftable_diff.py`, `RESOLVE_CACHE_GONE`); `runs/baseline` = B8's close run (as B9),
warning baselines `runs/warn_baseline_{debug,perf}.txt` (B9's). `runs/` is gitignored. B11 base for the blocks:
`b9_session/runs/b11base` (main at the B9 close; Phase D's first commits are docs / Makefile-rule only).

Invariants (B9 close): new game 5683 / 0, tutorial 5773 / 12, typetable `965b527a`, resave 5/5 == golden
`994eb184a8e88db9372b5b9cd13a9ed9`, unit 920 / 0, 0 new warnings, castmetric 2.

| row | branch / step | merge commit | gate problems | new game | tutorial | typetable | resave | unit | warnings | castmetric | routes | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SLOTS | d0-slots (tip `9f1f1038f`) | `570b1ee31` | 0 | 5683 / 0 | 5773 / 12 | `965b527a` | 5/5 PASS (23452937 bytes) | 920 / 0 | 0 / 0 | 2 | tools (placement / road / removal as B9); orchard / stress / disaster run after the ENGTYPES merge (`runs/POSTBLOCKS_*`) | rows 1202 / 1203 / 1204 FIXED, 1270 (no decal race: six port-only early returns in OnAdded) FIXED, 1271 / 1272 LEFT; starttown's 0x100000 pre-sets and MarkBuildingCells gone; slot 12 now paints building minimap dots |
| ENGTYPES | d0-engtypes (tip `fb5faef68`) | (pending) | | | | | | | | | gate routes | engine/types dissolved: 8 forwarders deleted, common.h -> runtime/types/global_types.h (runtime names; accepted deviation), ui/Dialogs.h / Panels.h -> code/Game/Dialog/, 4 loop aliases + their 4 unit CHECKs gone (count 920 unchanged); B11 vs a rebuilt base at 2c1840ad4: 16944 functions IDENTICAL, 934 / 935 objects byte-identical stripped |
