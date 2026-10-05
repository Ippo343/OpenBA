# B5 gate table (one row per gated merge; `runs/<label>/summary.txt` is the record, untracked)

Invariants on every row unless noted: builds debug / release / perf / unit exit 0, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame clean, -O2 new game clean, typetable == baseline (`b983f144`), resave
5/5 == golden. `devlost` = `devlost.sh` with `DLMAX=5`: pass is INJECT=1, COMPLETE>=1, crash 0 (killed=YES expected, B5_RULES §6).

| # | label | commit | what | gate | devlost | unit | notes |
|---|---|---|---|---|---|---|---|
| 1 | W0 | 10c2ea362 | typed owners, class-scope operator new/delete on the twelve bases, dead ctor doors deleted, Dx11SeamInstall tolerates a flipped table | 0 problems | — | 914/0 | |
| 2 | PILOT | c00afc64f | Merge b5-PILOT: VideoProgramData + RenderBufferData real subclasses; W0b (Finish dispatches Shutdown/CreateDevice by name) rode along | 0 problems | INJECT=1 COMPLETE=5 crash 0 | 915/0 | castmetric dx11 240 -> 236 |
