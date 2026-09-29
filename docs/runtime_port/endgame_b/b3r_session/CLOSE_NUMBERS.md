# The registry teardown: close numbers

Base `65b800d4` (the cutover's close) → the W2 tree `104f0986`. Code lines (`codegrep.py`) unless
marked raw. The W1 column is the audit's part F (`W1_AUDIT.md` §6, tree `fef215fe`).

| measure | base | after W1 | close |
|---|---:|---:|---:|
| `RegisterRuntimeExport(` | 1,258 | 139 | 0 |
| `RegisterRuntimeAddr(` (space-tolerant) | 1,453 | 109 | 0 |
| `RegisterExeAddr(` | 696 | 674 | 0 |
| dispatch slot cells | 51 | 51 | 0 |
| `ResolveAddr(Module::Exe` | 2,185 | 197 | 0 |
| `ResolveAddr(Module::Runtime` | 275 | 3 | 0 |
| `ResolveExport(` | 205 | 79 | 0 |
| `RT_EXPORT_ONCE` | 242 | 1 | 0 |
| `RtAddr<` | 430 | 0 | 0 |
| `X_ResolveFuncs` / `_InitFuncs` lines | 354 | 2 | 0 |
| `engine_funcs` externs | 227 | 227 | 0 (files deleted) |
| `engine_funcs` use lines outside `engine/funcs` | 4,279 | 1,055 | 0 |
| includers of `engine_funcs.h` rewritten | | | 514 |
| includers of `resolve.h` / `rt_port.h` rewritten | | | 233 / 272 |
| `standalone/reg_*.cpp` files | 127 | 128 | 0 |
| `standalone/address_registry.cpp` lines | 1,243 | 1,243 | 0 (deleted) |
| `standalone/rt_export_addrs.inc` rows | 4,926 | 0 | 0 (deleted) |
| minted storage rows | 2 | 0 | 0 (the mint is deleted) |
| `reinterpret_cast` (all directories) | 29,064 | 27,433 | 26,488 |
| … code / runtime / standalone / engine / dx11 / tests | 20,887 / 5,028 / 2,190 / 259 / 168 / 532 | 19,600 / 4,766 / 2,170 / 238 / 134 / 525 | 19,542 / 4,772 / 1,509 / 5 / 134 / 526 |
| `// B.3.x bridge` markers (raw) | 6,698 | 6,863 | 6,859 |
| `PORT GAP` markers (raw) | 149 | 191 | 189 |
| `STANDALONE DEVIATION` markers (raw) | 49 | 56 | 56 |
| unit tests | 919 | 918 | 918, 0 failures |
| `NOT IMPLEMENTED` addresses: new game / tutorial / Load Game | 10 / 8 / 1 | 1 / 1 / 1 | 0 / 0 / 0 |

Boot lines: `[RtPort] runtime registry: 1347 exports, 1817 addrs` → gone; `[AddressRegistry] 759` →
gone; `[RtMint] registered 2/2` → gone; `[RtResolve] 0 misses` → gone (nothing resolves).

Tree: 324 commits, 65 merges; 1,267 files outside `docs/`, +25,701 / −85,391 lines.

Gated merges: 29 rows in `GATE_TABLE.md` (+21b). Problems found by the gate: RT-UI's 24 orphaned
delegations (warnings), RT-Core's unused counter. Found by the coordinator reading the log: the missing
`[ExtLoad]` line (the gate now fails on it). Found by the audit: the KeyMap free (E1). Coordinator
errors: two overlapping runs (row 16), a kit copy without its marker file (row 29).

`behaviour:` commits (each with its retail evidence in the message; none produced a diff on a route or a
resave): the allocator instrument seat and ALLOC_STATS seeing frees; `String::ToInteger` / `memmove`
(RT-UI); `ConsumeRawMaterial` 5 arguments; the market tree cleanup 2 arguments; the zone refresh and the
region path length (G-Work, G-Game, then the phantom parameters removed by Leftovers); the road A*
0x14009ce00 returns `int`; the dialog `Contains` argument, `AnimationEvent::Construct`,
`DeviceData::Slot4` (owed from the cutover); the KeyMap deleting destructor frees again; fire and
disease read the settings block's auto-slow byte; the allocator snapshot read live.
