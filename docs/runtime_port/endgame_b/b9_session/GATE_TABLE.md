# B9 gate table (coordinator; one row per gated merge, written BEFORE `mergegate.sh` runs)

Kit commit: `cf62fb35a` (Block 0: the exception inventory, `-fexceptions`, rows 1210 / 1211, the rule sheet).
Baseline: `runs/baseline` (B8's close run), warning baselines `runs/warn_baseline_{debug,perf}.txt`, B11 base
`runs/b11base/{perf,debug}` = main `22b1d9613`'s objects (identical to the kit commit's: the flag spelling
recompiles nothing).

| row | branch | merge commit | gate problems | new game | tutorial | typetable | resave | unit | warnings | castmetric | routes | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RT | b9-RT | `df2c61a96` (tip `e3f506288`) | 0 (the gate printed 3: the `[RtResolve] 0 misses` check, dead since the resolve cache went -- `RESOLVE_CACHE_GONE` marker not copied into the kit; copied after, no re-run needed) | 5683 / 0 | 5773 / 12 | `965b527a` = baseline minus 10 lazy by-name lookup lines (unchanged since B8's HDR gate; the gate's accepted shape) | 5/5 PASS == golden | 920 / 0 | 0 / 0 | **2** | tools: run by hand after the gate (`runs/RT_tools`) | the introsort escape's free NOT gone (ArrayAssignCore out of line; row 880 re-tagged); the three serializers are one `IO::Serializer::Serialize<T>(int, T*, const char*)` template, not Array's (owed); row 1220 filed |
| RES | b9-RES | | | | | | | | | | ui, tools, panelclose, pick, fire | |
| SURV | b9-SURV | | | | | | | | | | tools, fire (tornado) | |
