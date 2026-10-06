# B5 gate table (one row per gated merge; `runs/<label>/summary.txt` is the record, untracked)

Invariants on every row unless noted: builds debug / release / perf / unit exit 0, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame clean, -O2 new game clean, typetable == baseline (`b983f144`), resave
5/5 == golden. `devlost` = `devlost.sh` with `DLMAX=5`: pass is INJECT=1, COMPLETE>=1, crash 0 (killed=YES expected, B5_RULES §6).

| # | label | commit | what | gate | devlost | unit | notes |
|---|---|---|---|---|---|---|---|
| 1 | W0 | 10c2ea362 | typed owners, class-scope operator new/delete on the twelve bases, dead ctor doors deleted, Dx11SeamInstall tolerates a flipped table | 0 problems | — | 914/0 | |
| 2 | PILOT | c00afc64f | Merge b5-PILOT: VideoProgramData + RenderBufferData real subclasses; W0b (Finish dispatches Shutdown/CreateDevice by name) rode along | 0 problems | INJECT=1 COMPLETE=5 crash 0 | 915/0 | castmetric dx11 240 -> 236 |
| 3 | BUF | 7a8dcd4fb | Merge b5-BUF: ConstantBufferData, IndexBufferData, VertexBufferData; Serialize slot typed `IO::Serializer*`; buffers.cpp 43 -> 3 | 0 problems | INJECT=1 COMPLETE=5 crash 0 | 916/0 | dx11 236 -> 194 |
| 4 | CTX | cbe5ca16e | Merge b5-CTX: ContextData; Field<> accessors -> Context / Device / Configuration members; Context.h void* forwarders gone; `Device::onVideoError / onVideoRestart` named; Finish prints the removal reason | 0 problems | INJECT=1 COMPLETE=5 crash 0 | 916/0 | first commit of the branch does not build alone (the merge does). `removed reason`: 0x0 on the injected frame, 0x887a0005 (DEVICE_REMOVED, no sub-cause) on the 5 real trips |
