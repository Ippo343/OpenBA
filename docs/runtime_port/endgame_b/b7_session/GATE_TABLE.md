# B7 gate table (one row per gated merge; `runs/<label>/summary.txt` is the record, untracked)

Invariants on every row unless noted: builds debug / release / perf / unit exit 0, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame clean, -O2 new game clean, typetable == baseline (`b983f144`), resave
5/5 == golden (`3efc80a84907fbb8e25d64b4f2c99cff`, unchanged unless the row says RE-CUT).

| # | label | commit | what | gate | unit | castmetric | notes |
|---|---|---|---|---|---|---|---|
| 1 | CTOR | 3e5238a9e | Merge b7-CTOR: System::Random / Engine / Renderer / Scene / Frustum real ctors (doors deleted with every caller), the Vector4 pixel rows (Context memset), the 0x700 Engine memset and the 0xC0 Frustum zeroing dropped; new row 1020; B7_RULES C1-C2 | 0 problems | 920/0 | 26 -> 18 | tools: placement / road / removal exit 0 crash 0; devlost INJECT=1 COMPLETE=5 crash 0 killed=YES (the DLMAX cap); routes 31 / 21 / 9 / 24 s |
