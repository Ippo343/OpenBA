# Next session plan (written 2026-10-02, B.4 FIRST PASS DONE — the second pass starts fresh)

B.4 (the cast metric) is NOT closed. Its first pass (W0, W1, W2) is finished and merged; the user
stopped the session there on purpose: **the second pass starts with a fresh inventory and a fresh set
of agents allocated by what blocks a cast, not by directory.** This file says where it stands and how
to start. The plan B.4 started from is this file at `b2a5434c`
(`git show b2a5434c:docs/runtime_port/endgame_b/NEXT_SESSION.md`): its §0 (owed from the teardown),
§2 (carry-overs) and §4 (after B.4) still hold and are not repeated.

## 0. Scope, as the user set it (2026-09-29, unchanged)

**The DANGEROUS casts go completely; only safe casts stay.** Dangerous = any spelling of a read by
offset or of a `void*` / integer receiver reinterpreted as a class (`reinterpret_cast`, C-style,
`static_cast` through `void*` / `uintptr_t`, offset helpers, `memcpy` field copies). Safe = the
`// CAST-OK: <category>` floor (`b4_session/FLOOR.md`), the three tier-I shapes, arithmetic and
same-hierarchy `static_cast`. The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine`.

**The differences from retail are a SEPARATE task for another session**: `docs/known_port_issues.md`
entries 13 and 14 (rows now run to 124; row numbers are GLOBAL across the tables of entry 14).
Blocks record what they find and do not fix it. A cast that can only be removed by changing
behaviour (the H5 raw-storage members) is LEFT and listed with the row that blocks it.

## 1. Where it stands

| | |
|---|---|
| main | see `git log -1`; last gated merge `6a3ccdca` (GATE_TABLE row 44) |
| metric | 46,226 to remove at block 0 -> **8,950** (81% removed); tests (own row) 813 -> 796 |
| gated merges | 44, every one 0 problems (`b4_session/GATE_TABLE.md`) |
| invariants, unchanged | new game `calls=5683 openFileFail=0`; tutorial `5773/12`; typetable 428 lines md5 `b983f144`; resave golden md5 `dd3893af…`; unit 918 / 0; stub set 0 |
| running | nothing; no merge in progress; every agent worktree removed |

Every first-pass block of `B4_FILEMAP.md` is merged: W0 · T · R-UI-a/b, R-IO, R-GFX, R-VIDEO, R-ENG,
R-CORE · COMP-a..f · DLG-a..e · WORK-a..c · GAME-a..c · ACT-a/b · PLAYER · TUT · STANDALONE · TESTS.
`DX11` (15 files, 240) was not started: it belongs to the dx11 seam step.

Documents: rule sheet `B4_RULES.md`; `b4_session/` — the briefs (`BLOCK_BRIEF`, `W1_BRIEF`), six
`INVENTORY_*` with `data/` and `scripts/` (cut at block 0: STALE, see §2.1), `castmetric.py`,
`acceptance.sh`, `FLOOR.md` (ceilings total 423; the tool does not enforce them: compare `--floor`
per category by hand), `B4_FILEMAP.md` + `data/filemap.tsv`, `GATE_TABLE.md`, the gate scripts.

## 2. The second pass — start here

### 2.1 Fresh inventory first (read-only, sonnet / haiku, before any block is cut)
The first pass left casts for ONE reason almost everywhere: the thing they reach is owned by
another file. So the second pass is cut by BLOCKER, not by module.
1. `castmetric.py … --by-file --tsv`: every file's count today.
2. Every `// LEFT:` comment in the tree (`Grep "// LEFT:"`), grouped by the owner / reason it names;
   every `// B4 FORWARD` forwarder with its outside caller files; every bridge marker left.
3. From those, a table: blocker (a header field, a signature, an H5 member, a forwarder) -> the
   casts and files it unblocks. That table, not the directory tree, is the new file map.
4. Re-read the hand-ins' "fields I need in headers I do not own" lists: they are consolidated in
   §2.3; check each against the tree (some were served by later blocks).

### 2.2 How to allocate
- One agent per BLOCKER FAMILY, owning the header that changes AND every file that reads it (the
  first pass could not do this: files were owned by module). Disjoint file sets are still the rule;
  where two families touch one file, they run in sequence, not in parallel.
- Shared, tree-wide changes run ALONE, first, on main, gated before anything else starts:
  the Serialize / Persist stream parameter, `Work.h` / `Action.h` slot parameters.
- Small agents: every first-pass agent that took more than ~40 files or ~1,000 casts ran out of
  context (PLAYER took three agents, GAME-c four). Aim at <= 300 casts or <= 15 files per agent.
- Limits unchanged: at most FOUR agents at `-j2`, the gate at `-j4`, one game-running command at a
  time. A gate + ui + tools + stress chain exceeds the 10-minute background limit when agents are
  building: run the gate, then the routes, as separate commands.

### 2.3 The cross-module asks, consolidated from every hand-in
**Tree-wide signatures (one owner each, alone):**
- The `void*` stream parameter of `Object::Base` / `External` `Serialize` and `Persist` (every
  class; TUT ~22 sites, ACT-b one per action class, WORK, GAME-c, R-CORE). Reserved for this pass.
- `Work.h` slot parameters, `GetTarget`'s return buffer and the Persist stream (`void*`);
  `Action.h` slot parameters (`GetTarget` / `Execute` / `ActionStack`).
- `FindOrLoad` / `FindNext` return `const External*`: one `const_cast` per resolver (about 15 sites).

**Headers with raw storage (H5: needs retail's ctor / dtor read; `behaviour:` commits):**
- `WorldGeometry::_arrays` -> named fields. Readers to move first: geometry_draw.cpp's destructor,
  action_pf_road.cpp, decalcomponent.cpp, boatcomponent_route.cpp, statvaluedispatch.cpp,
  roadtool_calcpath.cpp. Then PLAYER's `tool_base_highlight_tess.cpp` (95), `tool_base_render.cpp`
  (84), `AddDecal` as a member, GAME-b's nine accessor bodies.
- `MenuDialogLoopT`'s three arrays and `_audioScene`; `_dlcShip` (+0xBC8: a 0xF0 scene object built
  by 0x1400D3F10, class not settled, row 120).
- `CitizenComponent::name`, `meshInstance`, `field_0x200`; `CitizenBehavior` +0x100 / +0x120 /
  +0x160 (trees and raw storage).
- `PlayerController::camera`; `Map::pickingScene`; `HighlightComponent::edgeMeshInstance` /
  `transform`; `MapGenerator` `ExternalRef::strPtr` / `resolved`; TerrainGenerator's and Toolbar's
  String / Array members; the Tutorial / Goal String accessors and `~Array` calls.

**Typed fields other files want:**
- `UserInterface::_dialogs` element and `DialogInstance::_ui` (`void*`, R-UI): DLG-d, TUT, PLAYER,
  STANDALONE.
- `PlayerToolManager` +0x08 (two floats held as one `uint64_t`), its +0x60 set (root + count),
  `TrackedEntityNode::key` (holds a dialog description, row 72); `Tool::dialogPtr` is the manager
  (rename, row 73); `EditTool::entityRef` / `lastEntityRef` (`uint64_t`).
- `Map` +0xE8 / +0xF0 as a set header; `Map::regionPairCache` (`int64_t*`); `AvlNode28`'s padding
  dword holds the segment tree's max cell.
- `ModelComponent::_description`, +0x13c; `CreateFencedDescription::_refs`,
  `CreateBridgeDescription::_refs` (`void*[6]`); `MultiObjectComponent`'s array as three fields.
- `Video::Context` +0x20f0 / +0x288 (R-VIDEO); `Geometry::_transform`'s InterpolatedState view,
  Picking user data `void*`, the 0xb0 TriangleList, AnimationGroup tags, `MaterialConstant`'s String,
  VertexProgram / PixelProgram elements (R-GFX); `TypeNode` int16 balance + height, `Filename`
  fields held as String, `Persistence::_savePath`, `Exists(RtSystem::Filename)` (R-IO);
  `MainLoopCallback::_func` / `_context` (R-ENG); `MemoryAllocator::freeLists` (`uint64_t`).
- `GameUpdateLoop::m_tutorial` holds a `WorkScheduler`, `m_imageCapture` the `Minimap` (renames);
  EnvironmentDescription's month record names mislead (`skyColor` is the ambient colour …).

**C signatures that take integers / `void*` (type them, rewire, delete the forwarder):**
- WORK: `WorkGrid_FindCell` / `ValidateCell` / `FreeStorageCapacity`, `WorkCellBST_Remove_ca60`,
  `WorkBST_BSTRemove_a6e0`, the `int*` Map helpers (`a00f0` / `8830` / `6330`), BuildRoad's ctor
  argument, the 21 outside callers of `WorkComponent_CalcMaxWorkers` (its two extra guards: row 80).
- ACT: `Pf_VectorResize60_fa80`, `Pf_AvlRebalanceA_a8c0`, `Pf_FineCellDirty`, `Pf_EnqueueCmd2`,
  `ActRm_ArrayResize16_da70`; the pool element types of `action_pf_pools.cpp`.
- COMP / CIT: `CitizenAI_WorkScheduler_Insert`, `StateMachine_TickReimpl`,
  `CitizenEmploy_SeatWorkers`, `CitizenList_AddWorker_91130`, `CompProf_*`.
- GAME: `StartTown_SpawnColonistRetail` should return `ComponentEntity*` / `CitizenComponent**`
  (unlocks starttown's citizen side, 195 left); `Reimpl_MapGetTerrainPos`'s `void*` normal (about
  150 callers); `Pathfinder_Seed`, `Region_Seed`, `MapTileGrid_Build` local externs;
  `TownRegistry_RegisterWorkplace` (`int64_t`); `SaveLoadHarness_Tick`.
- Runtime: the `rtcoll` ArraySpec callbacks and BST / AVL helpers taking the container as
  `uintptr_t*`; the type-erased `ArrayHdr` core and `avl_trees.cpp`'s node constructors; the POD
  String bridges (`pod_bridge.h`); `RtMemAlloc` results (not recognised as tier I).

**Files the first pass did not reach (their agents ran out of context; no other blocker):**
- `standalone/tool_harness.cpp` (455): convert against the final tool headers; the gate's tool
  routes run it (`PlacementTool CREATE -> 1`, `CalcPath (commit) -> 1` must still log).
- `code/Game/Game/starttown.cpp` (195): `StartTown_SpawnStructures`' outer body 85,
  `SeedHouseInventory` ~28, `DL_CollectAnimals` 20, `AssignFoundingHousing` ~18,
  `SeedLaborerProfessions` 12 (see `StartTown_SpawnColonistRetail` above).
- standalone leftovers (about 80 in 14 files), `avl_trees.cpp`'s node constructors.
- Tree counts at the pause: 478 `B4 FORWARD` lines, 1,434 `// LEFT` lines.

**Tests (796 left; the block removed 17):** the rest is the tests' subject (vtable probes, address
identities, sentinels, raw layout probes: they STAY) or waits on product code: `io_vtable_abi_test`
(~96: the minted IO tables and `serializer_class.cpp`'s retail-slot `vtable_read`s, the `rt::io` /
`RtIo` stream PODs), `system_abi_test` (7: the `RtSystem` Thread / Random / Color / WideString / Time
PODs), layout checks against `RtIo::CompressedStream` / `TextFileStream`, `RtUI::DialogInstance`,
`RtObject::BaseTypeInfo`, `math_test` (`Math::*::Serialize` takes `void*`), and three
`ControllerLive::TableRegisterState` handler casts. They fall with the closing wave's POD mirrors.

**Unreachable in `standalone/` (listed, NOT deleted — the user decides):** `Options::pastureAutoType`
and `Options::buildSyncLabor` are set and read nowhere; `RtAllocSwapArmCutover`
(`alloc_cutover_arm.cpp`) has no caller, so the two `RtAllocInstrument_*` hooks it installs are
unreachable. The user kept every harness: convert, never retire, unless told otherwise.

### 2.4 Rules the second-pass briefs must carry (each cost a re-run on 2026-10-02)
- **B.4 changes how a field is REACHED, never what the code computes.** Agents slipped in: an array
  swapped for `Collection::Array` (changes the first allocation), a rewritten weld (new hash and
  container), a local shrunk from 0x100 to 0x10 bytes, dropped null tests, dropped failure returns.
  Each must be proven from retail's disassembly (address quoted) or reverted.
- **The B11 object comparison is mandatory and its diffs are READ**, at the perf flags as well as
  debug (at -O0 every typed access differs; -O2 shows the real changes). It caught, before merge: a
  removal marker that stopped zeroing a field (one 8-byte store split in two), a loop bound
  +0x22c vs retail's +0x228, six lookups that lost a null test. Per function: same ordered call list
  (covers random-call and creation order), same store widths and covered bytes.
- **A `bool` field normalises a byte** (H8): where retail copies a byte that may not be 0 / 1, the
  store is LEFT. **A merged store split into named stores must still write every byte** (H7).
- **A respelling is not a removal**: C-style -> `reinterpret_cast` / `static_cast`, a union, a
  `memcpy`, a typed-view helper that still contains the cast. A cast that stays keeps its ORIGINAL
  spelling plus `// LEFT: <owner/reason>`.
- **Context**: commit per file; tool output goes to scratch files, never into the context; stop at a
  building commit with the exact list of what remains. Takeover = a new agent `git reset --hard` to
  the old branch tip on a new branch, with the unverified items as its FIRST tasks.
- **Reserved `known_port_issues` row ranges per agent** (it removed most conflicts; the file still
  conflicts when two blocks append to the same table: keep both sides).
- **Warnings**: the gate compares warning TEXT per file path. A named type changes a standing
  warning's wording and a moved file changes its path: read `warn_new_*.txt`, update the baseline,
  keep it SORTED (`LC_ALL=C sort -u`). Log-only locals need `[[maybe_unused]]` (perf strips logs).

## 3. After the second pass
1. **Closing wave.** Rewire the callers of every `// B4 FORWARD` forwarder and delete the forwarders
   and their declarations (`acceptance.sh` lines 08 / 09); delete the offset helpers left in headers
   (`widget_common.h`, `coll_bridge.h`, `description_common.h`; `--floor` lists 19 accessor
   definitions) and the `Rt<Ns>::` POD mirrors.
2. **An independent read-only AUDIT** before the C-body headers are deleted.
3. **Close:** `acceptance.sh`, `stepclose.sh`, then on a QUIET machine (no agent building):
   - `sa-stress`: the figures recorded during W1 swing with load (mean 1.64 - 2.15 ms against a base
     of 1.92, p95 2.03 - 3.31 against 2.40, the same tree giving 1.80 and 2.15 fifteen minutes
     apart). Only a quiet run says whether B.4 moved it: the one quiet run, at `6a3ccdca` with no
     agent building, gave update 1.68 ms/frame and p95 2.15, both under the base.
   - `devlost.sh` (owed since GAME-b changed geometry draw; it grew the game to 6.6 GB once: run it
     alone).
   - a generated-world comparison for the worldgen / weather / starttown conversions: the gate
     proves the new game loads the same resources and a RESAVE is byte-identical, not that a freshly
     generated world equals the base's. Compare a new-game save (same seed) between the B.4 base
     `b2a5434c` and the tip, or use `tools/trace_oracle`.
   - the removal tool's CalcPath has NO route (the harness skips it): drive it once by hand.
   Then `CLOSE_NUMBERS.md`, the STATUS block, this file replaced by the plan for the dx11 seam.

## 4. What bit (bind the next session)
- **Parallel blocks that type shared fields collide at merge.** `mergegate.sh` builds the merged tree
  BEFORE the merge commit exists; fix small breaks inside the merge (two by-name fixes for TUT on
  2026-10-02), or send the block back to merge main on its own branch.
- **A block that needs a one-line typing in a RUNNING block's header asks the coordinator**, who
  makes it on main and tells both (PlayerToolManager +0x00, the MapGenerator pools).
- **Do not touch main's tree while a merge script runs**; prepare docs in the scratchpad and commit
  between merges.
- **Two merge commits on main do not build** (`f41a352a`, `18662af7`), and several block branches
  hold one such commit (`838dc71f` in PLAYER): `git bisect skip`.
- **A route nobody ran finds a crash nobody caused** (`known_port_issues` 13): compare against the
  base before blaming a merge.
- **A lockup corrupts files**: after one, `git status` everywhere, `git fsck`, delete object
  directories when a link fails with "file format not recognized".
