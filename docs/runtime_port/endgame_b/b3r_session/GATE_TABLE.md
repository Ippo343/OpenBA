# Gate table — the registry teardown (B3R)

One row per gated merge, by the coordinator, with `b3r_session/gate.sh` (full build, three routes, the
-O2 new-game route, resave xN). Invariants (B3R_RULES R10): new game `calls=5683 openFileFail=0`,
tutorial `5773/12`, typetable 428 lines md5 `b983f144`, resave == golden, `[RtUnresolved]` 0, no NEW
address in a route's `NOT IMPLEMENTED` set. Load Game's `calls` floats (4758-4787).

Baseline, `65b800d4`, from a `make clean` rebuild: 0 problems; warning baseline 111 debug / 262 perf
(1,094 compiled files); unit 919 / 0; `[RtPort] runtime registry: 1347 exports, 1817 addrs`;
`[AddressRegistry] 759`; `[RtMint] registered 2 by name, 2 by address`; stub set new game 10
(`0x140002280 0x140022480 0x140089470 0x1400938d0 0x1400fcc90 0x140101ee0 0x1401026a0 0x140152a70
0x1401c22d0 0x1401c23d0`), tutorial 8 (`0x140002280 0x140022480 0x1400938d0 0x14009e8c0 0x1400fcc90
0x140101ee0 0x1401026a0 0x140152a70`), Load Game 1 (`0x140002280`).

| # | merge | main after | problems | unit | resave | registry lines | stub set | notes |
|---|---|---|---:|---|---|---|---|---|
| 1 | `b3r-owed` (the seven items owed from the cutover) | `6dbfe3cd` | 0 | 919 / 0 | 5/5 | unchanged | unchanged | UI routes: Mods, Quit, Options, Achievements exit 0; GoalDialog opens. One `behaviour:` commit (`DialogInstance::Update` passes `_ui` to `Contains`) |
| 2 | `b3r-W0-P0b` (Component declarations) | `c741cb2c` | 0 | 919 / 0 | 5/5 | unchanged | unchanged | declarations only |
| 3 | `b3r-W0-P0a` (Action / Work declarations) | `46b30133` | 0 | 919 / 0 | 5/5 | unchanged | unchanged | declarations only |
| 4 | `b3r-W0-SM` (mirror deleted, 7 rows registered explicitly; `MemoryAllocator::_singleton` and `Debug::_disableHandler` class statics; mint and generator deleted) | `43a3d739` | 0 | **918** / 0 (re-cut: the mint-shape test went with the mint) | 5/5 | **re-cut:** `[RtPort] runtime registry: 1338 exports, 1537 addrs` (was 1347 / 1817: the mirrored keys with no reader left); `[RtMint]` lines gone; `[AddressRegistry] 759` | unchanged | the numbers are the ones the block computed before the run; `mirror_proof.py` PROOF HOLDS; UI routes (Mods, Quit, Options, Achievements) exit 0, GoalDialog opens; tool routes placement / road / removal exit 0; `mintgen` left the gate with the generator |
| 5 | `b3r-W0-P0c` (Game / Player / Dialog / Tutorial declarations, the `engine_funcs.h` move, 280 Runtime declarations, `exe_constants.h`, `IsDerivedFromNullSafe`) | `be7dce94` | 0 | 918 / 0 | 5/5 | unchanged (1338 / 1537, 759) | unchanged | declarations only |
