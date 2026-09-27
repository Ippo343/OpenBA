| merge | unit | result |
|---|---:|---|
| baseline (1d7866d4) | 516 | 0 problems |
| pilot b38-rules (addb4dfd) | 523 | 0 problems, resave 3/3 |
| RB part 1 UI bases (3f090079) | 533 | 0 problems, resave 3/3 |
| CIT (e61cae1a) | 537 | 9 problems: newgame EXEC past 16-slot table (StateMachine lazy linker); fixed on main | 
| CIT + linker fix | 537 | 0 problems, resave 5/5 |
| ACT (45b8cc2e) | 557 | 0 problems, resave 5/5 |
| TOOL (bb0ee047) | 567 | 0 problems, resave 5/5 |
| RB (65eafdc0) | 594 | 0 problems, resave 5/5 |
| PNL (03e6799f) | 634 | 0 problems, resave 5/5 |
| K1 (fc92ee69) | 645 | 0 problems, resave 5/5 |
| WRK (3eb4c02b) | 670 | 0 problems, resave 5/5 |
| MI-A (95128b08) | 676 | 0 problems, resave 5/5; tutorial TownCensus entA=1 |
| DLG (877f68c3) | 691 | 1 problem = typetable 312->311 registered / 417->416 chain: AutoSaveDialog duplicate removed (D4, intended; baseline re-cut); resave 5/5; Mods route exit 0 |
| VIS (e3f09481) | 699 | 0 problems, resave 5/5 |
| MI-B (73142f29) | 709 | 0 problems, resave 5/5 |
| LOOP (7bdc92fd) | 722 | 18 problems: every route exit 3 at ~7s, EXEC 0x3 in Graphics::Scene::Clean geo->Clean() slot 8 from MenuDialogLoopT_TickParticles; merge UNDONE (main reset to 73142f29), sent back to LOOP |
| GOAL (9fb05e19) | 723 | 0 problems, resave 5/5 |
| LOOP (a9937675, fix 723b0232) | 736 | 1 problem = resave DIFF: 128 leaked menu UI objects now freed (retail); golden re-cut 758597e2 (757e48a1 -> dd3893af); resave 5/5 on new golden; new-game calls 5686->5683 openFileFail 3->0, tutorial 5776/15 -> 5773/12 |
| CB3 (69c0bf3c) | 736 | 0 problems, resave 5/5 |
| CB1b (98edc88f) | 736 | 0 problems, resave 5/5 |
| CB4 (f3bec4e4) | 736 | 1 problem = typetable minus 15 ResolveTypeIdByNameLazy log lines (lookups now X::TypeId(); counts/chain/ids unchanged) -> gate taught; resave 5/5 |
| CB4 tool routes | - | placement/road/removal harness exit 0, crash 0 |
| CB6 (f7f7181f) | 736 | 0 problems, resave 5/5, Mods route exit 0 |
| CB5 (bda29cbd) | 736 | 0 problems, resave 5/5 |
| CB1a (8b282a76) | 736 | 0 problems, resave 5/5 |
| CB2 (abae0ac2) | 736 | 0 problems, resave 5/5, Mods route exit 0 |
