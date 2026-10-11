# Next session plan (written 2026-10-10, end of B9 — exceptions and the B8 residue; ENDGAME B CLOSED)

**B9 is CLOSED, and with it ENDGAME B.** Decision 7 (exceptions) is DONE by measurement, not by conversion: the tree
has ZERO `setjmp` / `longjmp` / SEH sites (the ~25 files of 2026-09-13 were the oracle and the wave harnesses, deleted in
A.4 and the registry teardown), retail's game code has ZERO `catch` frames in all three binaries and ZERO throws in the
exe, and the Runtime DLL's one throw (`Debug::ForceDump`, a minidump trigger) and one `__try` (`EnumerateVideoModes`'
access-violation swallow) are diagnostics / a hardware fault, ruled in rows 1210 / 1211. `-fexceptions` is spelled out
in both flag sets; `b9_session/EH_INVENTORY.md` is the evidence and its §5 the rule for any future unwinding body;
`ENDGAME.md` B.1 item 7 carries the DONE marker. The B8 residue was then worked by four opus blocks (RT, RES, SURV, RT2;
`B9_RULES.md`, `b9_session/handins/`), four gated merges at 0 problems (`b9_session/GATE_TABLE.md`): 54 block commits
(19 `behaviour:`, 33 `B9:`), castmetric **3 -> 2** (the introsort escape gone: `Array16Assign` is
`Array<EmitterInstance>::operator=`, `ArrayAssignCore` header-inline, the empty-source free in `Scene::Update` GONE —
row 880 closed), LEFT_FREE §A 121 -> **102** names (49 -> 28 `stays free`, each re-read), 13 LEFT rows FIXED and one
NOT A DIFFERENCE, 18 new rows filed (1210-1211, 1220, 1230, 1240-1247, 1260-1263). Invariants unchanged (new game
5683 / 0, tutorial 5773 / 12, typetable `965b527a` as since B8's HDR gate, resave 5/5 == `994eb184…` on every merge,
unit 920 / 0, 0 new warnings); stepclose clean, sa-stress 139 fps, worldcmp identical to B8's close. Numbers:
`b9_session/CLOSE_NUMBERS.md`.

## 0. Scope, as the user set it

B's acceptance is met (registry gone, no name / address resolution, compiler vtables everywhere, resave gate passing,
castmetric 2 = the two ruled casts `tool_base_render.cpp:724` / `achievement_state.cpp:217`). **The user's decision at
the B9 close (2026-10-10): close B, no B10; the next phase is the restructure, then the portable platform layer**
(§2; `ENDGAME.md` §E). The fidelity backlog B leaves is a tracked list (§2.4), worked when a route needs it. Rules that
stay: a guard retail lacks is dropped, never added; a faithful crash is root-caused upstream; no new flags or
instruments. Device-lost stays DEFERRED (§3).

## 1. Where it stands

| | |
|---|---|
| main | the B9 close commit (after `cbb701d58`, Merge b9-RT2, the last gated merge) |
| metric | castmetric **2**: runtime 2 (`tool_base_render.cpp:724` / `achievement_state.cpp:217`, ruled); code 0 |
| declarations | `exe_bodies.h` / `*_funcs.h`: 121 -> **102** names (17 headers): 58 `free in retail`, 28 `stays free` (measured), 23 harness / port-only; `void*`-first in code/Game 39 -> **34** |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `965b527a` (= B8 baseline minus the 10 lazy by-name lines; the gate's accepted shape), resave 5/5 == golden `994eb184a8e88db9372b5b9cd13a9ed9`, unit **920 / 0**, 0 new warnings |
| acceptance | 1 line off (10 = 19, accepted); 01 = 2 of 2; 08 = 0 of 102; 09 = 0 of 34 |
| known_port_issues | rows 12, 880, 993, 1080, 1120-1123, 1126, 1128, 1163, 1164, 1200, 1201, 1208, 1209 FIXED; 1207 NOT A DIFFERENCE; 1210 RULED; 1211 NOT A DIFFERENCE; row 34 SUPERSEDED (settings file); new SUSPECTED / owed: 1220, 1230, 1240-1247, 1260-1263 |
| close checks | `stepclose.sh` clean (CLOSE_NUMBERS §6), `worldcmp.sh` identical to B8's CLOSE run (md5 97fcc629; vs `oldbase` the `[PfSeed]` wording only) |
| tree | code/Game 923 files / 211k lines, runtime 545 / 181k (35 `_win.cpp` seams), standalone 74 / 20k, dx11 17 / 7k, engine/types 15 / 0.6k, tests 85 / 33k; docs 520 md + 2.3 GB of run logs; tools 3.9 GB (venvs, the renderdoc build) |
| worktrees | `oldbase` (`b2a5434ca`); the four `b9-*` block worktrees removed at the close (branches kept) |

## 2. The next step: PHASE D — the restructure, then the portable platform layer (decided 2026-10-10)

### 2.1 Restructure first (tree still Windows-only; the gate unchanged)

**DONE 2026-10-10** (the same session that closed B9): see `docs/runtime_port/phase_d/GATE_TABLE.md` and the Phase D block in
`STATUS.md`. The table below is the plan as written; the shape landed as proposed, with `common.h` -> `src/engine/global_types.h`,
the four colliding private headers renamed `*_internal.h`, `dx11/src/README.md` -> `SOURCES.md`, and the build output in `build/`.
The §2.3 decisions were taken (1162 RULED keep 0.0f; 1202-1204 installed, the race root-caused as none). Next is §2.2.

Pure moves and renames, nothing semantic: the resave gate, the full route set and `castmetric.py` prove that nothing
changed while every path did. Proposed shape (confirm the names at the start):

| today | lines | becomes |
|---|---:|---|
| `src/game/` | 211k | `src/game/` (the exe's logic; `engine/types/`'s 15 headers dissolved into it — the only `engine/` left) |
| `runtime/` | 181k | `src/engine/` (the Runtime DLL's classes; `runtime/types` + `runtime/src` folded per namespace) |
| `src/engine/**/*_win.cpp` (35 files) + the harness's platform bits | | `src/platform/win/` (the seam retail already had: filesystem, registry persistence, audio stream / voice, input, window, lifecycle, package read) |
| `standalone/` (74 files, 20k) | | split: `src/app/` (the launcher: `main`, options, boot) vs `src/diag/` (crash / hang / fail-fast handlers, watchdog, hw_watch, redzone, the AUTOSTART / menu-driver / tool / disaster harnesses) |
| `src/render/dx11/` | 7k | `src/render/dx11/` |
| `tests/` | 33k | `tests/` (unchanged) |
| `docs/` (520 md, 2.3 GB) | | `docs/reference/` + `docs/decisions/` kept; the wave / step history (`WAVE*.md`, `b*_session/runs`, `cutover*`, `evidence`) moved to ONE `docs/history/` tree or out of git; CLAUDE.md's table re-pointed |
| `tools/` (3.9 GB) | | the `ghidra_mcp` / `renderdoc_mcp` venvs and the renderdoc build OUT of git (`.gitignore`); `tools/attic/` archived with the history |
| `Makefile` | | CMake (a decision to confirm: the natural cross-platform build; keep `mingw32-make sa-run` as a wrapper until the gate scripts are ported) |

Order: (1) the gitignore / venv / build-dir eviction and the docs history move (no code touched, one commit each);
(2) `engine/types` into `code/Game`; (3) the big directory moves with include paths rewritten by script, ONE commit per
top-level move, gate after each; (4) the standalone split (launcher vs diagnostics: `options.h` already names which flag
is which); (5) the build system. Acceptance: the gate at 0 problems after every move, `worldcmp.sh` identical, CLAUDE.md
/ the skills / `tools/*.sh` paths updated in the same commit as the move.

### 2.2 Then the portable layer, one seam at a time (decisions taken 2026-10-10)

**2026-10-11 (later): PHASE D.4 WAVE 4 CLOSED too -- the first Linux game build runs; see STATUS.md's wave-4 line for the numbers and the user decisions owed; the next session starts with those decisions (§2.3), then §4 "After that".** Earlier the same day: **2026-10-11: Phase D.4 waves 1, 2b and 3 CLOSED; WAVE 4 is the next session's first step** (`phase_d/D4_RULES.md` §2 "Wave 4": SDL audio over the vendored decoder with decode-at-load, delete the Win32 backends, the first Linux game build over dxvk-native in WSL2, the gap rows 1273 / 1275 / Tornado, the ABI close; then the end-of-D.4 items: the clean-rebuild stress re-measure the user deferred, the VEH exit-3 proof decision, the acceptance script). Main at the close = the commit after `99cf47f08`; the block worktrees were removed (branches `d4-*` kept); the B11 base for wave 4 is cut at the start from main's `build/perf/obj`. The plans are `phase_d/plans/*.md`, the rule sheet with the user's answers and the wave table is `phase_d/D4_RULES.md`, every gated merge is a row of `phase_d/GATE_TABLE.md` "Phase D.4", each block's hand-in is `phase_d/handins/<BLOCK>.md`. Read those three before anything below; the bullets below are the plan as written.

- **Settings: a local settings file, not the Windows registry.** `IO::Persistence`'s registry arm
  (`src/platform/win/persistence_registry_win.cpp`, ~20 `Reg*` sites) becomes a file-backed store (a key-value file or a
  small local db) under the save directory. **The launcher dialog (row 34) is NOT ported as a Win32 dialog proc — its
  two settings become entries in that file** (supersedes the 2026-10-09 "port the proc" decision). `SHGetFolderPathW`
  (`io/filesystem.cpp` / `filesystem_win.cpp`) becomes a per-platform save-directory query.
- **Input and audio: the library with the widest platform reach** (Windows, Linux, macOS incl. Apple Silicon, Android
  as future targets). SDL3 is the recommendation (window + events + raw input + audio streams in one dependency; SDL2
  where a target lacks SDL3). Replaces the XAudio2 stream / voice files (13 files, 106 XAudio2 spellings), the raw
  Win32 input (4 files), `CreateWindowExW` / `PeekMessageW` in the application seam.
- **Threads, events, timer:** `_beginthreadex` (11 sites) / `CreateEventW` (15) / `WaitForSingleObject` (22) /
  `QueryPerformanceCounter` (28) / `CoInitializeEx` (9) behind a small platform API or the standard library.
- **Renderer: keep `src/render/dx11/` as the D3D11 implementation and run it over DXVK (dxvk-native: D3D11 -> Vulkan) on
  non-Windows first;** a native Vulkan backend only if DXVK proves insufficient. The retail shaders ship as compiled
  DXBC, which DXVK consumes as-is (a native backend would need the modkit's HLSL sources cross-compiled). macOS runs
  Vulkan through MoltenVK under DXVK; Android is the open question for this route (record, do not decide now).
- **ABI audit before the first non-MSVC-ABI build:** `long` (419 lines: 4 bytes on Windows, 8 on Linux), `wchar_t`
  (53 lines: 2 vs 4 bytes — the wide filename path), 47 `__stdcall` / `__cdecl` / `__fastcall` keywords, 7 `ms_abi`,
  5 `__declspec`; the layout `static_assert`s catch what the audit misses. Decision 7's result helps: zero SEH, so the
  unwind story is already portable.

### 2.3 The user decisions still OWED (ask FIRST, before any of the above)

Row **1162** (retail copies uninitialised floats into the heightmap edge; the port keeps 0.0f — keep deterministic, or
faithful?); rows **1202 / 1203 / 1204** (the uninstalled slot bodies: Decal [4] keep until the material-load race is
fixed upstream, Map [6] install with starttown's 0x100000 pre-set removed, RawMaterial [9] install — the slot-8 shape;
the bodies are `DecalComponent_Serialize` / `MapComponent_Draw` / `RawMaterialComponent_Update`, LEFT_FREE §A). Not
asked in B9 (the user was absent at its start).

### 2.4 The fidelity backlog B leaves (tracked, not a step)

- **The PORT GAPs:** 152 `PORT GAP` lines over 17 distinct retail bodies / tables: `Entity::FindAudioComponent`
  0x140002280 (row 1080's `em->PlaySlot(1)` sits behind its null), `FUN_140022480`, `Weather::DrawUpdate` 0x1400938d0,
  the RegionSystem build 0x14009e8c0, `Entity_impl_cc90` 0x1400fcc90, `EntityUI::Startup` / `Update` 0x140101ee0 /
  0x1401026a0, the import slots `Volume::Box::Box` / `InternalBasePtr::DeletePtr` / `Math::Vector3::operator==`, the
  .rdata tables (slope cutoff 0x140172130, corner offset 0x140172274, the tile-shape LUT 0x1401c22d0, the decal
  mask-atlas LUT 0x1401c23d0), `Dialog::GetReference` 0x1800388d0, the CRT `exit(code)` 0x18007f8a8, the .rdata prefix
  0x1801557e8; the device-lost pair `GameUpdateLoop::OnVideoError / OnVideoRestart` (§3, DEFERRED). The gameplay-visible
  ones (audio emitters, weather, EntityUI, the LUTs) are worth wiring when a route reaches them (CLAUDE.md: wiring an
  arm is `behaviour:`, read the consumer first).
- **Row 1243 (a REAL missing call, RES):** `GameLoadDialogLoopT::Startup` (`Game/splash_loop_stubs.cpp`, retail
  0x140098fa0) lacks retail's `ApplyKeyMapToUI(&_ui)` and the three UI calls after it (0x140098fbe..).
- **Row 1260 (SURV):** `TownRegistry_RegisterResidence` IS `ResidenceComponent::EnterBuilt` 0x140129de0's body plus
  three port tests and a log: fold it into EnterBuilt, the tests ruled by B9 D1.
- **Row 1230 (RT2):** what stays of the element-loop template — MenuScene's two bare-`T*` `ExternalPtr` arrays, the
  bucket body's inner CALL where retail inlines (the `[[gnu::noinline]]` price), `EntityList`'s open-coded master array.
- **Row 1220 (RT):** `Audio::Scene::Update`'s two `if (table)` tests retail lacks (0x1800df730 / 0x1800dfc7a).
- **Rows 1240-1247 (RES) and 1261-1263 (SURV):** SUSPECTED port-only tests found while reading (PlaceRoadCell's work /
  singleton / scheduler tests, StampCellRefs' callers, the menu Update slot's shape, RestoreVideoSettings, SeedAgriPool /
  ResolveTemplate, PlaySlot's five tests, SerializeAll's manager / factory tests + CitizenBehavior::Serialize's phantom
  third parameter, a second RefreshMesh after RegisterEntity, the set serializers' citTid test, SetFlag's settings test).
  Each: read every retail caller, drop or name the route (B9 D1).
- **The 28 `stays free` survivors** (LEFT_FREE §A, each with a B9 measurement on its line): typed overloads over one
  template core (stay by design), `TradeMerchantOrders_Destroy`, `TradeResourceTree_NewNode`, the `WorkCellBST` pair, the
  raw-triple holders (`ActRm_*`, `Reimpl_Array_Resize_Stride08`, `PathPairs_Clear_25b30`), the user-decision bodies, the
  harness rows. None blocks anything; acceptance 08 stays at 0 against them.
- **Comment debt (comments only):** banners naming the deleted `WorkBase_TreeNodeFree_14c30 / _e320` and
  `ArrayAssignCore`'s old file (~8 files, RT2's hand-in).

## 3. Device-lost (DEFERRED by the user — read before touching it)

Unchanged from B5: `b5_session/DEVLOST_FINDING.md`, known_port_issues row 890. The leading hypothesis is the exe PORT
GAP pair `GameUpdateLoop::OnVideoError / OnVideoRestart` (0x140095970 / 0x140095930). Do it only when the user lifts the
deferral; `devlost.sh` keeps `DLMAX=5`. The renderer decision (DXVK) may change its shape: re-read the finding then.

## 4. After that

- The restructure (§2.1), the portable layer (§2.2), then the first non-Windows build. `docs/retail_bugs.md`'s list is
  revisited only if the user wants a "fixed" build flavour someday.
- Device-lost, when lifted (§3).

## 5. Working rules that held (do not relax) and what B9 added

- Agents never run the game; builds `-j2`, at most four building agents; the coordinator alone gates; the gate builds
  the MERGED tree before the merge commit exists; commit the gate table BEFORE `mergegate.sh`; a fresh worktree is
  reset to a NAMED commit; never `git add` the session directory wholesale; merge main before the final build.
- NEW (B9 C1): **the kit needs `RESOLVE_CACHE_GONE`** — `gate.sh` counts a missing `[RtResolve] 0 misses` line as a
  problem unless that marker file sits beside it; copy it with the scripts (RT's gate printed 3 phantom problems).
- NEW (B9 C2): **a block's `HANDIN.md` at the worktree root collides on merge** — two blocks adding it conflict, and a
  LATER merge re-resolves a renamed `handins/X.md` onto the other block's text (git's rename + modify). Blocks write
  `<kit>/handins/<block>.md` directly next time; the coordinator checks the hand-ins after every merge.
- NEW (B9 C3): **an attribute is a measured spelling, not a helper:** `[[gnu::always_inline]]` where retail inlines
  (B7's shape: `ArrayAssignCore`), `[[gnu::noinline]]` where retail CALLs an instantiation and GCC would inline it
  (`Serializer::Serialize<T>`), `[[gnu::noipa]]` where `noinline` alone yields an `.isra` clone that drops an argument
  (`WorkBase_TreeNodeFree`, the WorkBST templates). Each is quoted with the B11 that chose it; a `noinline` on a
  template is per TEMPLATE, so one instantiation may CALL where retail inlines (row 1230's bucket body).
- NEW (B9 C4): **the Bash tool's heredoc eats backslashes AGAIN** (a python heredoc died on a backslash-U escape twice,
  2026-10-10, even with the backslash doubled): any text with a backslash goes through the Write / Edit tool.
- NEW routes in the kit: `harness_route.sh utility` with `BANISHED_UTILITY_TEMPLATE` = `Template` + backslash +
  `UtilityProfessions.rsc` (the profession dialog; the singular `UtilityProfession.rsc` does not exist), `tradepanel`
  with `BANISHED_BUILD_DEMO_TRADINGPOST=1` (TradeUI LIVE), `orchardpanel` with `BANISHED_BUILD_DEMO_ORCHARD=1
  BANISHED_ORCHARD_DIAG=50 BANISHED_ORCHARDPANEL_SELECT=0` (the orchard typed, `PlantOrchard constructed=1`), a forced
  tornado (`BANISHED_FORCE_TORNADO=1 BANISHED_FORCE_TORNADO_FRAME=300 BANISHED_FORCE_TORNADO_PERIOD=400` in the disaster
  route's shape). Run them on any merge touching dialogs, trade, orchards or the disaster harness.
- For the restructure: a move commit is ONE top-level directory, include paths rewritten by script, the gate after it;
  the kit scripts (`gate.sh`, `mergegate.sh`, `stepclose.sh`, the routes) hard-code `build/debug/...` and
  `src/game/...` and must move with the tree in the same commit.
- Kill an orphaned exe BY PID (`taskkill //F //PID`), never `/IM`; the lock's `exe_pids=` line names it.
