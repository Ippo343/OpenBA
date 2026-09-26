# B.3 step 8 — caller surface of the converted exe classes (`code/Game`), BY BINDING

Read-only measurement, 2026-09-26, main @ 9fb05e19 (after the GOAL merge; block 0K). No source edited, no build,
no run. Scripts in `C:/Users/USPC/AppData/Local/Temp/claude/b38/0K/` (not in the repo). Units follow
`b37_session/CALLER_SURFACE.md`: sites by binding, comments stripped before counting, string contents blanked
except where a string IS the binding (§6). Build dirs (`standalone/build*/`, `tests/unit/build/`) excluded.

**The converted set** = every class with a real definition `class X [final] : public P` under `code/Game/**.h`
AND a row in `B38_RULES.md` §1.3 outside block LOOP: **298 reflected classes** (310 minus LOOP's 12 world
externals — `ClimateGenerator`, `Disease`, `MapGenerator`, `MenuScene`, `MusicList`, `NameDatabase`,
`Profession`, `ProfessionList`, `TerrainGenerator`, `Toolbar`, `Tornado`, `Weather`) **+ 3 helper classes**
(`ControllerComponent::Controller`'s `RootController`/`TableController`, `StatusIconGeometry`). The five loop
tables (`GameUpdateLoop`, `MenuDialogLoopT`, splash, the two `LoaderLoop`s) are NOT converted (block LOOP is
still on its branch) and are out of every count below. Of the 123 LOOP-owned files (B38_FILEMAP §1 LOOP),
the ones that hold sites are marked `L` in the appendices and put in the HELD batch.

## 0. Method

- `classes.py` — the class set: `step8_appendix_raw.txt` SECTION R (310 `REG` rows: name, ti, create, cip,
  ctor, table) and SECTION T (361 tables, each slot's target, OWN/TINY/IMPORT) joined to the tree's class
  definitions. `vt.py`/`conv.json` add the §1.3 member tables (controller members, `0x14015f070` interface,
  StatusIcon geometry `0x14015f0c0`).
- **Address → owner.** Table addresses from §1.3; function addresses = every OWN/TINY slot of a converted
  class's table (a slot body shared by N tables is listed under each, the base owns it), + `create`/`cip`/
  `ctor` from SECTION R (the shared abstract create `0x140003730` excluded).
- `vt.py`, `fn.py` — every `0x1[4]xxxxxxx` hex literal in `code/ engine/ standalone/ runtime/ dx11/ tests/`
  whose value is one of those addresses, classified by the **innermost enclosing call** (`RegisterExeAddr`,
  `ResolveAddr`, file-local `R<>` …) or, when it initialises a named constant, **traced** (`trace.py`,
  `vt2.py`, `fn2.py`): every `NS::ident` use tree-wide, each use classified the same way. That is how the 17
  `RegisterExeAddr(XAddr::Vtable, …)` rows and the two `MessageDialogAddr::Vtable` readers were found.
- `efn.py` — `engine_funcs.cpp` initialisers `X = …ResolveAddr(Module::Exe, 0x…)` whose address is a converted
  member or table; each global's code uses tree-wide (the `extern`, the definition and the initialiser's LHS
  excluded; a use on a `printf`/TRACE line counted apart — the perf build strips it).
- `raw.py`, `rawfam.py` — hand-inlined dispatch (§2). Tokens: `(*(T***)p)[N]`, `(*reinterpret_cast<T***>(p))[N]`,
  `*(T*)(*(uintptr_t*)p + off)` and its `reinterpret_cast` spelling (receiver must be a bare expression: `(p + k)`
  inside the inner cast is a FIELD read, not a vptr), and local vtable variables (`vt = *(void***)p;` … `vt[N]` /
  `vt + off`, variable names `vt*`, `*vtbl`, `*vtable`, `vptr` …) — 609 sites in `code/`, `engine/`,
  `standalone/` (`runtime/` has none onto exe classes). Receivers classified by rule (§2), `OTHER` read by hand.
- `members.py`, `callers.py`, `entry.py` — (2)/(6): every function DEFINITION in `code/ standalone/ engine/`
  (7,912; brace-matched, `extern "C"` included); the 2,573 `X::M(…){…}` member definitions of converted classes;
  a member whose body is ONE call to a free function passing `this` is a **delegate** (the free function holds
  the body; `new (this) X`, `s_fn_*`/`fn_*` pointers and `CloseWithResult` excluded); a free function whose
  body is one call into a converted member is a **door**; a free `X_*` function whose suffix names one of X's
  declared virtuals (same name, `SlotN` = the header's `// [N]`, or `Serialize/Shutdown`→`Persist`,
  `Draw/OnAdd`→`OnAdded`, `OnRemoved`→`OnRemove`, `Dtor`→`DeletingDtor`) is an **entry point**. CALLs of those
  names from OUTSIDE the class's own files (files defining an `X::` member, or named `<x>*.cpp/.h`) are (2);
  multiply-defined and underscore-free names are dropped (`Slot7` exists in 40 namespaces).
- `tid.py` — (5): every string literal equal to a converted class name, classified by the enclosing call; and
  every `0x1401c….` ti-cell literal of a converted class.
- `agg.py`, `agg2.py`, `mkmd.py` — the tables below and the appendices.

---------------------------------------------------------------------------------------------------
## 1. Totals, per spelling and per directory

| spelling | sites | what converts |
|---|---:|---|
| (1) raw vtable dispatch, **proved** (SELF/DECL 52 + TYPE-ARRAY 220 + 1 by hand) | **273** | `p->M(...)` on the proved static type; slot 2 `Type()` scans → `reinterpret_cast<Object::Base*>(p)->Type()` (or the typed find helper) |
| (1) raw vtable dispatch, **candidates** (receiver named like a converted class; prove per site) | 174 | same, once proved; else leave and list |
| (1) excluded (Runtime/Graphics/UI receivers, field-read idiom, LOOP externals) | 162 | nothing |
| (2) CALLs of an old entry point from outside the class's files | **27** (3 in LOOP files) | the member (qualified where it is the base body called from a derived slot) |
| (3) `ResolveAddr(Module::Exe, 0x…)` / `R<>(0x…)` onto a converted member, outside engine_funcs.cpp | **57** | the member (49 of them are `Component::Persist` = `0x1401189b0`) |
| (3) `engine_funcs` door CALLs onto a converted member | 5 (`fn_ComponentBase_Serialize`) | `Component::Persist` |
| (3r) `RegisterExeAddr` rows at a converted member's address | 85 — **77 reader-less** | delete with the last reader |
| (4) `RegisterExeAddr` rows at a converted class's retail vtable | 37 (+2 hand tables) — **22 reader-less** | D5: delete when no reader remains |
| (4) by-address readers of a converted vtable (stamps / identity) | 29 live + 8 dead (`engine_funcs` globals) | `X::Vtable()` or the real ctor (B8-5) |
| (5) by-name type-id lookups of a converted concrete class | **543** (157 classes) + 17 on abstract bases (stay) | `X::TypeId()` |
| (5x) ti-cell reads `0x1401c….` of converted classes (`g_TypeId_*`, `ResolveAddr`) | 502 (+96 `RegisterExeAddr` g_tid rows) | END of B.3 per D4/D10; listed for completeness |
| (6) members that DELEGATE to a free body | **401** members / 125 classes / 400 bodies / ~15,100 lines | fold the body into the member |

`runtime/`, `dx11/`: zero sites in every spelling (the Runtime does not reach exe classes). `tests/`: 1 by-name
lookup (`object_typeinfo_test.cpp`, a name test — stays).

### 1.1 Per directory

| directory | (1) proved | (1) cand. | (2) | (3) | (3r) rows / dead | (4) readers / rows | (5) | (5x) | (6) delegating |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| code/Game/Component | 81 | 44 | 3 | 53 + 5 ef | 2 / 1 | 11 / 1 | 166 | 168 | 182 |
| code/Game/Dialog | 53 | 17 | 7 | 0 | 0 | 2 / 8 | 64 | 9 | 114 |
| code/Game/Tutorial | 2 | 10 | 0 | 0 | 0 | 0 / 0 | 1 | 2 | 0 |
| code/Game/Goal | 0 | 0 | 0 | 0 | 0 | 0 / 1 | 0 | 13 | 0 |
| code/Game/Work | 47 | 18 | 0 | 2 | 0 | 15 / 16 | 27 | 100 | 0 |
| code/Game/Action | 7 | 2 | 0 | 0 | 0 | 0 / 7 | 19 | 55 | 67 |
| code/Game/Player | 20 | 5 | 3 | 1 | 0 | 0 / 0 | 41 | 25 | 38 |
| code/Game/Entity (non-LOOP) | 11 | 4 | 0 | 0 | 0 | 0 / 0 | 0 | 0 | 0 |
| code/Game/Object | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| engine | 7 | 0 | 0 | 0 (+5 dead globals) | 0 | 1 (g_FollowDialog_Vtable, used in Dialog) / 0 | 1 | 75 | 0 |
| standalone | 38 | 61 | 11 | 1 | 83 / 76 | 0 / 4 | 156 | 41 | 0 |
| **LOOP-held** (code/Game/Game 91 files + entitylist.cpp) | 7 | 13 | 3 | 0 | 0 | 0 | 67 | 14 | 0 |
| tests | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |

(3) "ef" = `engine_funcs` door CALLs. (6) is counted where the MEMBER is defined (that is where the fold-in
happens): the delegating classes are CIT/VIS/MI-A/MI-B components (182), PNL panels (113, through the
`<x>ui_sa::W_SlotN` wrappers) + DLG `ToolbarDialog` (1), ACT actions (67), TOOL tools (38); K1, WRK, GOAL,
DLG (but one) moved their bodies — **0 delegates**.

### 1.2 THE BATCHES (disjoint directories)

| batch | directories | (1) proved + cand. | (2) | (3) | (3r) dead rows | (4) readers / rows | (5) | (6) fold-in: members / lines |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| **CB1** | code/Game/Component | 81 + 44 | 3 | 58 | 1 | 11 / 1 | 166 | 182 / ~7,550 |
| **CB2** | code/Game/Dialog, code/Game/Tutorial, code/Game/Goal | 55 + 27 | 7 | 0 | 0 | 2 (+ citizenui:662) / 9 | 65 | 114 / ~1,140 |
| **CB3** | code/Game/Work, code/Game/Action | 54 + 20 | 0 | 2 | 0 | 15 / 23 | 46 | 67 / ~2,820 |
| **CB4** | code/Game/Player, code/Game/Entity (non-LOOP), code/Game/Object, engine, tests | 38 + 9 | 3 | 1 | 0 | 1 / 0 | 43 | 38 / ~3,430 |
| **CB5** | standalone | 38 + 61 | 11 | 1 | 76 | 0 / 4 | 156 | 0 |
| HELD | the LOOP file set (`code/Game/Game`, `entitylist.cpp`) | 7 + 13 | 3 | 0 | 0 | 0 | 67 | 0 |

- CB1 is the heaviest by far; if it needs splitting, split by the B38_FILEMAP block file sets inside
  `code/Game/Component` (CIT / VIS / MI-A+MI-B / K1 descriptions), not by spelling — the (3) sites and the
  delegate bodies of one class live in the same files.
- (6) fold-in lines by block: CIT 3,188 · TOOL 3,434 · ACT 2,823 · VIS 1,962 · MI-B 1,616 · PNL 1,122 ·
  MI-A 790 · helpers 164 · pilot 22 · DLG 17. TOOL's 3,434 is `code/Game/Player` (CB4).
- HELD waits for `b38-LOOP` to merge (its files are being rewritten there); then it joins CB4 or runs alone.
- The engine_funcs dead globals and the reader-less rows (§8) are the coordinator's closing items, not a
  batch's: deleting a row is safe only after the batch that converts its LAST reader has merged.

---------------------------------------------------------------------------------------------------
## 2. (1) Raw vtable-slot dispatch — 609 sites, by family (Appendix A)

| family | sites | proof | conversion |
|---|---:|---|---|
| SELF | 35 | the receiver is `this`/`self` inside a converted class's own member or its `X_*` body — the class is known | `this->M(...)`, UNQUALIFIED: retail dispatches these virtually (e.g. 20 Work leaves' `Execute` call `vtbl[10]` = `Work::GetTarget(void* retbuf)`); the B.3.7 "qualify inside slot bodies" rule is about the hand-built loop tables, not these |
| DECL | 17 | the receiver is declared `X*` with X converted | `self->M(...)` |
| TYPE-ARRAY | 220 | slot 2 on an element read from a component array (entity `+0x30/+0x38`) or a description's component-description array (`+0x28/+0x30`) — every one is the retail "Type()==tid then IsDerivedFrom" scan; the element is an exe `Component`/`Description` (Object::Base) | `reinterpret_cast<Object::Base*>(p)->Type()`; better, one typed find helper per array kind (the scans are ~60 hand copies of the same loop) |
| OTHER, proved by hand | 1 | `componententity.cpp:58` `TemplateComponentType`: slot 8 of a ComponentDescription template = `Description` slot 8, the RETSLOT (`Object::Type* M(Object::Type* ret) const`, B8-11) | call it exactly in the retslot shape |
| TYPE-OTHER | 48 | slot 2 on a receiver that is not an array element (params, `smc + 0x48` nodes, `SafeGetTypeId` helpers) | prove, then as TYPE-ARRAY |
| TOOL | 17 | named `tool`: `tool_activation_standalone.cpp` reads `*(void**)toolSlot` (the player's active tool), harness `tool_harness.cpp` | prove the slot → `Tool*` |
| CTRL | 12 | `node`/`chainHead`/`smPtr` from `StateMachineComponent +0x48` chains and `sm` lists — either the D6 controller member (`ControllerComponent::Controller`, converted) or a state-machine node (`statemachine.cpp`, LOOP) | prove per site; do not guess |
| NAMED | 97 | receiver named `comp`/`c`/`ctrl`/`wpUi`/`img`/`obj`… | prove per site |
| RUNTIME | 62 | Graphics (`geo`, `mesh`, `swapChain`), UI (`elem`, `button`, `dlg` base slots), IO (`enc`, `hdr`) receivers | not step 8 (a `dlg` slot 8 is `UI::DialogInstance::OnElementEvent`, a B.3.6 base virtual) |
| NOT-VPTR | 80 | `*(T*)(*(uintptr_t*)p + off)` where `p` is an IBP / singleton / world pointer: the inner read yields an OBJECT pointer, the outer a field — a double field read, not a slot | nothing |
| OTHER, excluded by hand | 20 | `particlecomponent.cpp` `ge->geo` (Graphics geometry, 8); `buildcomponent_state8.cpp:176` (draw children, Graphics); `optionsdialog.cpp:948-949` (the Game world, LOOP); `tornado.cpp:614/623` (LOOP); `zonecomponent.cpp:111`, `goals_serialize.cpp:683`, `pickup_gate.cpp:139`, `plantarea.cpp:771`, `plantorchard.cpp:624` (field reads through an IBP/result); `internal_factory_audit.cpp:74` (a byte scan); `tool_base_render.cpp:253` (a heightfield index) | nothing |

**Proved total 273, candidates 174.** A.3's producer column is the evidence; the batch re-reads each
candidate. Two traps seen while reading:
- `code/Game/Work/foresterwork.cpp:416` `FindSubComponentInList` calls slot 2 with **no argument**
  (`typedef uint32_t (*GetTypeId_t)(); getTypeId()`) — it works only because `DECLARE_EXE_TYPE`'s `Type()`
  never reads `this`. Converting to `p->Type()` fixes it; do not copy the typedef.
- SELF sites name slots by the tree's old names: `cleararea.cpp:266` calls `vtable[6]` through a 2-argument
  typedef called "Cleanup", but slot 6 is `Work::OnComplete(void*, void*, int)` (3 args) and `Cleanup` is slot
  8. THE DOOR DECIDES: map by slot index against `Work.h`, check arity, read the disassembly if they disagree.

---------------------------------------------------------------------------------------------------
## 3. (2) Old entry points called from outside the class — 27 (Appendix B)

Small, because the blocks kept the callers inside their own files. By target: `SelectResourceUI_Slot4/8/9` +
`CropUI_Slot7` ×7 (`orchardui.cpp`, `pastureui.cpp`, `cropui_promote.cpp` — a DERIVED panel calling the base
body: becomes the QUALIFIED `SelectResourceUI::OnCreated()` etc.); `Component_SerializeReimpl` ×3 (the
CharacterComponent / CitizenAIComponent / CitizenComponent serializers: `Component::Persist(stream)`
qualified); `DragTool_OnKey` ×2 + `DragTool_CancelDrag_Impl` ×1 (`fencedtool.cpp`, `roadtool.cpp`:
`DragTool::OnKey`/`OnCancel` qualified); `EditTool_CloseInfoDialog` ×3 (standalone harnesses);
`CitizenComponent_OnAdd` ×1 + `PickingComponent_RegisterMesh` ×1 (`starttown.cpp`, LOOP) ;
`ComponentEntity_RefreshEntityFlags` ×1 (`entitylist.cpp`, LOOP); `OrchardComponent_CanCommand/Command` ×2,
`HerdComponent_OnAdd/OnRemove`, `PopulationComponent_Startup/OnAdd/OnRemove` (standalone `reg_herd_live.cpp`
thunks — those thunks feed reader-less rows, §8; they die with the rows), `ToolbarDescription_Dtor` ×1.

Not in the 27: 424 CALLs of delegate bodies from INSIDE their own files (other free functions in the same
`.cpp` calling `X_Slot7(self)` instead of `self->OnUpdateLayout()`) — they convert as part of the (6) fold-in
of that class, not as a caller batch.

---------------------------------------------------------------------------------------------------
## 4. (3) Exe-address doors onto converted members (Appendix C)

- **`Component::Persist` (`0x1401189b0`)**: 48 `ResolveAddr(Module::Exe, 0x1401189b0)` + 1 file-local
  `R<>(0x1401189b0)` (`diseasecomponent.cpp:172`), one per component leaf's Persist body (the base Persist
  call retail makes directly), + `fn_ComponentBase_Serialize` 5 CALLs (`buildcomponent.cpp:91`,
  `firecomponent.cpp:130`, `modelcomponent.cpp:225`, `statusiconcomponent.cpp:149`,
  `tradecomponent_serialize.cpp:351`). Each becomes `Component::Persist(stream)` QUALIFIED inside the leaf's
  Persist (the pilot's shape). The row (`reg_component_economy.cpp:190` → `Component_SerializeReimpl`) and
  the door die after the last one.
- **Own-address reads** (a class resolving ITS OWN retail slot body back through the registry): `buildcomponent.cpp:71`
  (slot 6), `decalcomponent.cpp:385` (slot 6), `particlecomponent.cpp:806` (slot 8), `statusiconcomponent.cpp:80`
  (slot 8), `removaltool.cpp:497` (slot 13), `pasturework.cpp:246` (slot 4) — become direct member calls; the six
  rows that serve them (C.3, live = 1) then go.
- **Unregistered own-address read**: `pickup.cpp:87` resolves `0x1400f2f00` (Pickup slot 4) into
  `s_fn_Slot4_Original`; there is NO row for it, so it is always 0 and its fallback is dead code.
- `standalone/pasture_harness.cpp:40` resolves `0x140127720` (PastureComponent slot 15) — a harness; becomes
  `reinterpret_cast<PastureComponent*>(p)->OnMessage(msg)`.
- `engine_funcs` globals onto converted members with **zero code uses**: `fn_ConsumeProduceUI_Slot4`,
  `fn_DragTool_OnKey`, `fn_DragTool_Shutdown`, `fn_Work_SerializeBase` (dead, §8).
- Dispatch slots: none (every `RegisterDispatchSlot` row targets a Runtime import; exe classes are not
  imports). `RtAddr<>`/Runtime hex: none onto exe classes.

---------------------------------------------------------------------------------------------------
## 5. (4) Retail vtable addresses of converted classes (Appendix D)

37 `RegisterExeAddr(kVtable, X::Vtable())` rows (20 by hex, 17 through `XAddr::Vtable`) + 2 rows that register
a HAND table at a converted class's address (`reg_w2_lane1.cpp:82` `g_PurecallVtable` at `0x14015f070`, the
controller interface; `reg_w2_lane2.cpp:92` `g_TutorialInstance_Vtable_SA` at `0x140160bf8`, behind
`Opt().liveTutorialInstVt`, default off).

**Live readers (29 sites, 15 tables)** — all stamps of a fresh object's vptr by retail address:
`Action` ×7 (`citizenai_actionctors.cpp`, `citizenai_handlers.cpp`, `work_act_{clear,hunt,path,perform}.cpp`,
`work_base_actionctor.cpp`), `PerformWork` ×4, `PlayAnimation` ×3, `ShowTool` ×2, `CreateNaturalResource`,
`PickupRawMaterial`, `RemoveNaturalResource`, `Work` ×2 (`clearareacomponent_states.cpp:342`,
`work_base_reimpl.cpp:160`), `ClearArea`, `ClearStorage`, `FireWork`, `LocationWork`, `Pickup` (one each, in
`code/Game/Component`), `MessageDialog` ×2 (`saveloaddialog.cpp:922/1103` through `MessageDialogAddr::Vtable`),
`StatGoal` (the `standalone/main.cpp:952` factory self-test compares against it), `FollowDialog`
(`citizenui.cpp:662` through `g_FollowDialog_Vtable` — **no row: reads 0**, falls back to
`FollowDialog_GetStandaloneVtable()`). B8-5: a stamp becomes the real ctor (`new (p) X(...)`) where the reader
builds the object, `X::Vtable()` only where it truly re-stamps.

**Reader-less rows (22 — delete, §8):** AchievementDialog, GoalDialog, MainMenuDialog, ModDetailsDialog (each
read only by a dead `engine_funcs` global), ModDialog, ConsumeProduce, ControlUI, ControlUIConfig, FishingWork,
ForesterWork, HarvestArea, HerbalistWork, HuntWork, MarketWork, PlantArea, TendArea, TradeWork,
ResourceLimitUIConfig, UpgradeUIConfig, WellDescription (the pilot's exemplar), + the two hand-table rows.
KEEP: Action, CreateNaturalResource, PerformWork, PickupRawMaterial, PlayAnimation, RemoveNaturalResource,
ShowTool, ClearArea, ClearStorage, FireWork, LocationWork, Pickup, Work, MessageDialog, StatGoal — until
CB1/CB3 convert their readers.

Named address constants: 237 `XAddr::Vtable`/`ControllerVtable`/`SecondaryVtable`/`BaseVtable` constants hold a
converted table's address; **219 have no use** (201 in the `engine/types` forwarder headers, 18 in
`code/Game`). They go with the forwarders at the close. Likewise **332** `XAddr::<Member>` constants hold a
converted member's address with no use at all (documentation).

---------------------------------------------------------------------------------------------------
## 6. (5) By-name type-id lookups (Appendix E)

543 lookups of 157 converted concrete classes: `Object::GetTypeIdByName("X")` 323,
`Object::ResolveTypeIdByNameLazy("X")` 91, and 19 file-local wrappers (`TypeId`, `Tid`, `MW_TypeId`,
`ResolveTid`, `BC_ResolveTid`, `BC8_TypeId`, `PG_Tid`, `TypeIdCached`, `FloorTypeIdByName`, `PopulationTypeId`,
`QuarryTypeId`, `BCS1_TypeId`, `ResolveExeTypeId`, `Pickup_Tid`, …) 129. Each becomes `X::TypeId()` (a plain
read — also removes the lazy static / first-use cache, B6-5). 17 lookups name an ABSTRACT base (`Component`,
`Description`, `Tool`, `DragTool`, `Work`, `Action`, `TutorialGoal`, `TutorialSetup`, …) — no `TypeId()` there
(D4): they stay. NOT lookups (left): name tables (`brace` 129: harness tables, `townhallui.cpp:2165-2274`,
`debugui.cpp:1636-1642` pair the name with a `g_TypeId_*` cell), `ExternalPtrSerializeGeneric(…, "X")` ×7 (a
serialization name), `strcmp` ×7, `TypeNameCRC` ×5, `REGISTER_ABSTRACT_FACTORY` ×4, `DEFINE_EXE_TYPE` 207,
the abstract bases' own `TypeName()` literals.

(5x) the 502 ti-cell reads (`g_TypeId_*` initialisers and `ResolveAddr(Module::Exe, 0x1401c….)`) are the same
answer by address; D4/D10 put them at the END of B.3 (with the 96 `g_tid` `RegisterExeAddr` rows). A batch
converting a file may take them with it only where the cell is file-local.

---------------------------------------------------------------------------------------------------
## 7. (6) Leftover free bodies the members delegate to (Appendix F)

401 members in 125 classes are a single call `Free(this, …)` into the free function that still holds the body;
400 distinct bodies, ~15,100 lines. Per block: ACT 67 members (18 classes, 2,823 lines — every action's
`Serialize/AbstractImpl/Execute/Enter/Exit/CheckEntity/GetTarget` `_Impl`), CIT 62 (3,188), PNL 113 (1,122,
through `<x>ui_sa::W_SlotN` → `X_SlotN`, two hops: the line count is the first hop), VIS 50 (1,962), MI-B 43
(1,616), TOOL 38 (3,434), MI-A 24 (790), helper classes 2, pilot 1, DLG 1. K1 (82 classes), WRK (23), GOAL
(27) and DLG (18 of 19) have NONE. 65 of the delegate bodies are also address-taken by a `RegisterExeAddr`
row — the reader-less ones (§8) can go first, which frees the body to move.

---------------------------------------------------------------------------------------------------
## 8. Dead code for the coordinator — verified on the tree (58 items + 77 rows + 551 constants)

| # | item | count | where |
|---|---|---:|---|
| G.1 | empty `Register*Vtable()` stubs (body empty after comments) + their ONE call each in `address_registry.cpp:1709-1758` + their declarations at `address_registry.cpp:54-122` | 13 | 11 in `code/Game/Action` (AnimateNaturalResource, CompleteRoad, CompleteTask, ConsumeRawMaterial, CreateRawMaterial, DropOffRawMaterial, HuntAnimal, MoveTo, PickupStoredRawMaterial, RemoveAnimal, SendComponentMessage), `animaudioevent.cpp:92`, `captureboxdescription.cpp:115` |
| G.2 | `struct X;` forward declarations of converted classes | 27 | 20 standalone, 3 Component, 2 `engine_funcs.h` (732 `SaveLoadDialog`, 1672 `TownHallUI`), 1 Dialog, 1 Game (LOOP) — list in Appendix G.2; check each is at namespace scope before changing |
| G.3 | dead `engine_funcs` globals (zero code uses; `extern` + definition + initialiser each) | 12 | vtable: `g_AchievementDialog_Vtable`, `g_GoalDialog_Vtable`, `g_ModDetailsDialog_Vtable`, `g_ModUpdateDialog_Vtable`, `g_PauseDialog_Vtable`, `g_ReloadDialog_Vtable`, `g_ToolbarDialog_Vtable`, `g_TutorialInstanceVtable` (printf only, `engine_funcs.cpp:2473`); member: `fn_ConsumeProduceUI_Slot4`, `fn_DragTool_OnKey`, `fn_DragTool_Shutdown`, `fn_Work_SerializeBase`. NOT dead: `g_FollowDialog_Vtable` (citizenui.cpp:662), `fn_ComponentBase_Serialize` (5 CALLs) |
| G.4 | the `UI_DIALOG_STUB` macro and its helpers `StubNop`, `kStubVtableSlots`, `StubVtableInit` (`ui_dialog_stub_factories.cpp:67-133`) — **zero expansions** left | 1 block | the anonymous namespace + the `#define` |
| G.5 | reader-less `RegisterExeAddr` rows at converted members' addresses (no `ResolveAddr` of the address anywhere, or only a dead G.3 global) | 77 | 76 in 28 `standalone/reg_*.cpp` files (`reg_component_economy.cpp` 15, `reg_animal_serialize.cpp` 6, …), 1 `particlecomponent.cpp:1048`; Appendix G.5 / C.3. The `reg_herd_live.cpp` thunks and the `extern "C"` declarations in those files die with them |
| G.6 | reader-less vtable rows (§5) | 22 | incl. the two hand tables `g_PurecallVtable` (`reg_w2_lane1.cpp:67-83`) and `g_TutorialInstance_Vtable_SA` (`reg_w2_lane2.cpp`, gated by `Opt().liveTutorialInstVt` — retiring that flag is a coordinator decision, DIRECTION §2) |
| G.7 | unused address constants of converted classes (`XAddr::Vtable…` 219, `XAddr::<member>` 332) | 551 | 201 + most of the 332 are in the `engine/types` forwarder headers, which the close deletes anyway |

Unverified by construction (not in the list): the `Register*Vtable` functions that register only a CTOR door
(`RegisterNewGameDialogVtable`, `RegisterOptionsDialogVtable`, `RegisterOptionInputBindDialogVtable`,
`RegisterBuildRoadVtable`, `RegisterPastureWorkVtable`) are misnamed but alive — their ctor rows have readers
outside this measurement's scope.

---------------------------------------------------------------------------------------------------
## 9. Defects found on the way (NOT fixed — reported)

1. `foresterwork.cpp:416` calls `Type()` without `this` (§2).
2. `cleararea.cpp:266` names slot 6 "Cleanup" with a 2-argument typedef; retail slot 6 is `OnComplete` (3
   arguments) and slot 8 is `Cleanup` (§2) — read the disassembly before converting.
3. `pickup.cpp:87` resolves an unregistered own slot (`0x1400f2f00`): the pointer is always null (§4).
4. `citizenui.cpp:662`: `g_FollowDialog_Vtable` reads an unregistered converted table (D9 #15 still open for
   FollowDialog).
5. `standalone/reg_w2_lane2.cpp` would, with its flag on, register a 4-slot hand table at the address of
   `TutorialInstance`, a real class since GOAL — nothing reads it today, so it is inert, but it contradicts D5.

## 10. Reproduce

`cd C:/Users/USPC/AppData/Local/Temp/claude/b38/0K && python classes.py && python vt.py && python vt2.py &&
python fn.py && python fn2.py && python efn.py && python regfn.py && python raw.py && python rawfam.py &&
python members.py && python entry.py && python callers.py && python fold.py && python tid.py && python vtsum.py &&
python agg.py && python agg2.py && python mkmd.py` (regfn2/tid2/emptyreg/fwd/loopfiles are small inline steps
recorded in the session; `appendix.md` is the generated half of this file).

---------------------------------------------------------------------------------------------------
## Appendix A — (1) raw vtable-slot dispatch sites, by family

`L` = the file is in the LOOP block's file set (held until `b38-LOOP` merges). Slot = the retail slot index (offset/8 for the `+0xNN` spelling). Producer = the last assignment of the receiver in the enclosing function.


### A.1 SELF — receiver is `this`/`self` inside a converted class's own member or `X_*` body (proved) — 35

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Action/moveto.cpp:495 | MoveTo_Execute_Impl | `s` | 4 | (uint8_t*)self |
| code/Game/Component/attendancecomponent_states.cpp:244 | Attendance_State3Enter | `selfP` | 7 |  |
| code/Game/Component/modelcomponent.cpp:267 | ModelComponent_Shutdown | `s` | 6 | (uintptr_t)self |
| code/Game/Component/populationcomponent.cpp:438 | PopulationComponent_Startup | `self` | 11 | reinterpret_cast<uint8_t*>(selfC) |
| code/Game/Component/statusiconcomponent.cpp:327 | StatusIconComponent_OnUpdate2 | `s` | 8 | (uintptr_t)self |
| code/Game/Player/tool_base_render.cpp:730 | ReportBadSelectionRef | `self` | 2 |  |
| code/Game/Player/tool_base_render.cpp:731 | ReportBadSelectionRef | `self` | 2 |  |
| code/Game/Work/buildstructure.cpp:518 | BuildStructure::Execute | `this` | 10 |  |
| code/Game/Work/cleararea.cpp:266 | ClearArea::OnCancel | `this` | 6 |  |
| code/Game/Work/cleararea.cpp:743 | ClearArea::Execute | `this` | 10 |  |
| code/Game/Work/cleararea.cpp:848 | ClearArea::Execute | `this` | 10 |  |
| code/Game/Work/clearstorage.cpp:312 | ClearStorage::Execute | `this` | 10 |  |
| code/Game/Work/consumeproduce.cpp:1107 | ConsumeProduce::OnCancel | `this` | 6 |  |
| code/Game/Work/firework.cpp:318 | FireWork::Execute | `this` | 10 |  |
| code/Game/Work/fishingwork.cpp:474 | FishingWork::Execute | `this` | 10 |  |
| code/Game/Work/fishingwork.cpp:502 | FishingWork::Execute | `this` | 10 |  |
| code/Game/Work/fishingwork.cpp:606 | FishingWork::Execute | `this` | 2 |  |
| code/Game/Work/fishingwork.cpp:963 | FishingWork::OnCancel | `this` | 6 |  |
| code/Game/Work/foresterwork.cpp:918 | ForesterWork::Execute | `this` | 10 |  |
| code/Game/Work/gatherrawmaterials.cpp:660 | GatherRawMaterials::Execute | `this` | 10 |  |
| code/Game/Work/gatherrawmaterials.cpp:911 | GatherRawMaterials::Validate | `this` | 8 |  |
| code/Game/Work/harvestarea.cpp:513 | HarvestArea::Execute | `this` | 10 |  |
| code/Game/Work/harvestorchard.cpp:491 | HarvestOrchard::Execute | `this` | 10 |  |
| code/Game/Work/herbalistwork.cpp:657 | HerbalistWork::Execute | `this` | 10 |  |
| code/Game/Work/huntwork.cpp:505 | HuntWork::Execute | `this` | 10 |  |
| code/Game/Work/locationwork.cpp:325 | LocationWork::Execute | `this` | 10 |  |
| code/Game/Work/marketwork.cpp:1390 | MarketWork::Execute | `this` | 10 |  |
| code/Game/Work/marketwork.cpp:1745 | MarketWork::Execute | `this` | 10 |  |
| code/Game/Work/pasturework.cpp:524 | PastureWork::Execute | `this` | 10 |  |
| code/Game/Work/pasturework.cpp:668 | PastureWork::Execute | `this` | 10 |  |
| code/Game/Work/pickup.cpp:676 | Pickup_Execute_Port | `S` | 10 | reinterpret_cast<uintptr_t>(self) |
| code/Game/Work/pickup.cpp:750 | Pickup::Slot5 | `this` | 4 |  |
| code/Game/Work/plantarea.cpp:416 | PlantArea::Execute | `this` | 10 |  |
| code/Game/Work/plantorchard.cpp:370 | PlantOrchard::Execute | `this` | 10 |  |
| code/Game/Work/tendarea.cpp:380 | TendArea::Execute | `this` | 10 |  |

### A.2 DECL — receiver declared as a converted class pointer (proved) — 17

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Component/diseasecomponent.cpp:926 | CDTrace | `self` | 6 |  |
| code/Game/Component/storagedisplaycomponent.cpp:485 | StorageDisplayComponent_OnUpdate2 | `self` | 6 |  |
| code/Game/Dialog/cropui.cpp:70 | CropUI_IterateResources | `self` | 96 |  |
| code/Game/Dialog/cropui.cpp:77 | CropUI_IterateResources | `self` | 104 |  |
| code/Game/Dialog/pastureui.cpp:329 | PastureUI_Slot9 | `self` | 11 |  |
| code/Game/Dialog/professionui.cpp:535 | ProfessionUI_Slot5 | `self` | 8 |  |
| code/Game/Dialog/professionui.cpp:835 | ProfessionUI_HandleSpinner | `self` | 8 |  |
| code/Game/Dialog/storageui.cpp:173 | StorageUI_Serialize | `self` | 7 |  |
| code/Game/Player/dragplacementtool.cpp:539 | DragPlacementTool_CalcPath | `self` | 14 |  |
| code/Game/Player/dragtool.cpp:284 | DragTool_OnKey | `self` | 14 |  |
| code/Game/Player/dragtool.cpp:355 | DragTool_OnKey | `self` | 13 |  |
| code/Game/Player/dragtool.cpp:394 | DragTool_OnKey | `self` | 13 |  |
| code/Game/Player/fencedtool_calcpath.cpp:337 | EdgeRun_db8c0 | `self` | 14 |  |
| code/Game/Work/buildroad.cpp:284 | FinalizeAction | `self` | 10 |  |
| code/Game/Work/consumeproduce.cpp:381 | FinalizeAction | `self` | 10 |  |
| code/Game/Work/foresterwork.cpp:440 | AssignWorkToEntity | `self` | 10 |  |
| code/Game/Work/herbalistwork.cpp:339 | AssignWorkToEntity | `self` | 10 |  |

### A.3 TYPE-ARRAY — slot 2 `Type()` on an element of a component / description array (proved Object::Base-derived exe object; the scan is Type()==tid then IsDerivedFrom) — 220

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Action/action_pf_road.cpp:672 | Pf_EntityResolvePos_2e20 | `c` | 2 | *reinterpret_cast<void**>(arr + (int64_t)k * 8) |
| code/Game/Action/createrawmaterial.cpp:180 | FindRawMaterialDesc | `comp` | 2 | *(void**)(compArray + (int64_t)i * 8) |
| code/Game/Action/createrawmaterial.cpp:207 | FindComponentOnEntity | `comp` | 2 | *(void**)compIBP |
| code/Game/Action/createrawmaterial.cpp:446 | CreateRawMaterial_AbstractImpl_Impl | `secComp` | 2 | *(void**)(*(uintptr_t*)(secDesc + 0x28) + (int64_t)i * 8) |
| code/Game/Action/pickupstoredrawmaterial.cpp:134 | FindComponentByTypeId | `comp` | 2 | *compIBP |
| code/Game/Action/sendcomponentmessage.cpp:195 | SendComponentMessage_AbstractImpl_Impl | `comp` | 2 | *(uintptr_t*)compIBP |
| code/Game/Component/ambientenvironmentcomponent.cpp:201 | FindSiblingEmitter | `comp` | 2 | *ibp |
| code/Game/Component/attendancecomponent.cpp:109 | AttendanceComponent_ReportMissingWorkplace | `c` | 2 | comps[i] |
| code/Game/Component/attendancecomponent.cpp:262 | AttendanceComponent_GetJobHostComponent | `comp` | 2 | (void*)*slot |
| code/Game/Component/boatcomponent_route.cpp:186 | ResolveOnce | `obj` | 2 | *reinterpret_cast<uintptr_t*>(slot) |
| code/Game/Component/boatcomponent_route.cpp:251 | ResolveOnce | `obj` | 2 | *reinterpret_cast<uintptr_t*>(slot) |
| code/Game/Component/buildcomponent.cpp:332 | BC_FindComponentByType | `comp` | 2 | (void*)*slot |
| code/Game/Component/buildcomponent.cpp:510 | BuildComponent_FindStorage26a30 | `comp` | 2 | (void*)*slot |
| code/Game/Component/buildcomponent_state1.cpp:84 | BCS1_FindRawMatDesc | `comp` | 2 | *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8) |
| code/Game/Component/buildcomponent_state1.cpp:128 | BCS1_IsModelComplete | `comp` | 2 | *slot |
| code/Game/Component/buildcomponent_state8.cpp:600 | BuildComponent_State8Finalize | `c` | 2 | *reinterpret_cast<void**>(carr + (int64_t)i * 8) |
| code/Game/Component/cemeterycomponent.cpp:400 | CemeteryComponent_InitFuncs | `c` | 2 | *slot |
| code/Game/Component/charactercomponent.cpp:152 | CharComp_FindSiblingByName | `comp` | 2 | *ibp |
| code/Game/Component/citizen_employ_cascade.cpp:98 | FindResidenceComp | `comp` | 2 | *reinterpret_cast<void**>(elem) |
| code/Game/Component/citizen_lifeevent_handler.cpp:114 | CompTypeId | `comp` | 2 |  |
| code/Game/Component/citizen_reassign_workers.cpp:130 | FindComponentDerived | `comp` | 2 | *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8) |
| code/Game/Component/citizen_reassign_workers.cpp:164 | FindComponentDerived | `param_3` | 2 |  |
| code/Game/Component/citizen_updatehealth.cpp:677 | PlagueBootReport | `slot` | 2 | (void*)(found.data + (uintptr_t)i * 8) |
| code/Game/Component/citizenai_findhelpers.cpp:140 | CompTypeId | `comp` | 2 |  |
| code/Game/Component/citizenai_findtarget.cpp:103 | CompTypeId | `comp` | 2 |  |
| code/Game/Component/citizenai_leisure.cpp:162 | VGetTypeId | `obj` | 2 | = nullptr) return 0xffffffff |
| code/Game/Component/citizenaicomponent.cpp:713 | CitizenAI_FindSiblingCitizen | `comp` | 2 | *ibp |
| code/Game/Component/citizencomponent.cpp:1249 | CitizenComponent_ShouldSwitchWorkplace | `comp` | 2 | *compSlot |
| code/Game/Component/citizencomponent.cpp:2026 | StandaloneFindStorageComp | `comp` | 2 | *reinterpret_cast<void**>(arr + (uintptr_t)i * 8) |
| code/Game/Component/citizencomponent.cpp:2994 | CitizenComp_FindSiblingByName | `comp` | 2 | *ibp |
| code/Game/Component/clearareacomponent.cpp:237 | ClearAreaComponent::Persist | `c` | 2 | comps[i] |
| code/Game/Component/clearareacomponent.cpp:238 | ClearAreaComponent::Persist | `c` | 2 | comps[i] |
| code/Game/Component/clearareacomponent_states.cpp:164 | EntityHasNatResComp | `comp` | 2 | *slot |
| code/Game/Component/clearareacomponent_states.cpp:165 | EntityHasNatResComp | `comp` | 2 | *slot |
| code/Game/Component/clearareacomponent_states.cpp:296 | ClearArea_ScanKindFlags | `sub` | 2 | *reinterpret_cast<void**>(subArr + static_cast<uintptr_t>(j) * 8) |
| code/Game/Component/clearareacomponent_states.cpp:297 | ClearArea_ScanKindFlags | `sub` | 2 | *reinterpret_cast<void**>(subArr + static_cast<uintptr_t>(j) * 8) |
| code/Game/Component/clearareacomponent_states.cpp:313 | ClearArea_ScanKindFlags | `sub` | 2 | *reinterpret_cast<void**>(subArr + static_cast<uintptr_t>(j) * 8) |
| code/Game/Component/clearareacomponent_states.cpp:313 | ClearArea_ScanKindFlags | `rm` | 2 | *reinterpret_cast<void**>(rArr + static_cast<uintptr_t>(k) * 8) |
| code/Game/Component/clearareacomponent_states.cpp:314 | ClearArea_ScanKindFlags | `sub` | 2 | *reinterpret_cast<void**>(subArr + static_cast<uintptr_t>(j) * 8) |
| code/Game/Component/clearareacomponent_states.cpp:314 | ClearArea_ScanKindFlags | `rm` | 2 | *reinterpret_cast<void**>(rArr + static_cast<uintptr_t>(k) * 8) |
| code/Game/Component/consumeproduce_recipe_dump.cpp:82 | DescribeMaterial | `c` | 2 | *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(j) * 8) |
| code/Game/Component/consumeproducecomponent.cpp:180 | ConsumeProduceComponent_Serialize | `comp` | 2 | compArray[i] |
| code/Game/Component/cropfieldcomponent.cpp:511 | CropFieldComponent_InitFuncs | `node` | 2 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/cropfieldcomponent.cpp:514 | CropFieldComponent_InitFuncs | `node` | 2 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/decalcomponent.cpp:232 | FindModelComponentOnOwner | `comp` | 2 | *slot |
| code/Game/Component/decalcomponent.cpp:515 | DecalComponent::OnEvent | `comp` | 2 | *slot |
| code/Game/Component/diseasecomponent.cpp:366 | CDTrace | `comp` | 2 | *slot |
| code/Game/Component/firecomponent_ignite.cpp:571 | FireDesc | `*slot` | 2 |  |
| code/Game/Component/floorcomponent.cpp:266 | FloorComponent_FindResData | `comp` | 2 | *slot |
| code/Game/Component/floorcomponent.cpp:348 | FloorComponent_SubF390 | `c` | 2 | *reinterpret_cast<void**>(arr + (uintptr_t)k * 8) |
| code/Game/Component/happinesscomponent.cpp:273 | FishingComponent_NotifyTile | `comp` | 2 | *(void**)(arr + (int64_t)i * 8) |
| code/Game/Component/happinesscomponent.cpp:396 | HappinessComponent_InitFuncs | `c` | 2 | *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(j) * 8) |
| code/Game/Component/immigrationcomponent_states.cpp:180 | FindComponentByTid | `c` | 2 | *slot |
| code/Game/Component/interactcomponent.cpp:176 | InteractComponent_FindWorkplaceComponent | `comp` | 2 | (void*)*slot |
| code/Game/Component/interactcomponent.cpp:221 | InteractComponent_Draw | `comp` | 2 | (void*)*slot |
| code/Game/Component/multiobjectcomponent.cpp:252 | MultiObject_FindComponent | `c` | 2 | *reinterpret_cast<void**>(comps + static_cast<uintptr_t>(i) * 8) |
| code/Game/Component/naturalresourcecomponent.cpp:152 | NaturalResourceComponent_FindComponentByType | `comp` | 2 | *slot |
| code/Game/Component/nomadcomponent_states.cpp:98 | FindCharacter | `c` | 2 | *slot |
| code/Game/Component/orchardcomponent.cpp:753 | TraceCanCommand | `node` | 2 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/orchardcomponent.cpp:755 | TraceCanCommand | `node` | 2 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/pasturecomponent_states.cpp:211 | FindCompOnEntity | `comp` | 2 | *slot |
| code/Game/Component/pasturecomponent_states.cpp:227 | FindDescOnTemplate | `d` | 2 | *reinterpret_cast<void**>(arr + (uintptr_t)i * 8) |
| code/Game/Component/pickingcomponent.cpp:128 | PC_FindModelComponent | `comp` | 2 | *slot |
| code/Game/Component/pickingcomponent.cpp:227 | PickingComponent_RegisterMesh | `c` | 2 | *slot |
| code/Game/Component/pickingcomponent.cpp:270 | PickingComponent_RegisterMeshForEntity | `comp` | 2 | *slot |
| code/Game/Component/residence_heattick.cpp:153 | EnsureFns | `comp` | 2 | *reinterpret_cast<void**>(comps + static_cast<uintptr_t>(j) * 8) |
| code/Game/Component/resourceproduce_creatework.cpp:354 | WrapIBP | `c` | 2 | arr[i] |
| code/Game/Component/resourceproducecomponent_states.cpp:308 | ResourceProduceComponent::Command | `node` | 2 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/resourceproducecomponent_states.cpp:310 | ResourceProduceComponent::Command | `node` | 2 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/spawncomponent.cpp:160 | SpawnComponent_Update | `comp` | 2 | *(void**)(*(uintptr_t*)(templateData + 0x28) + (uintptr_t)j * 8) |
| code/Game/Component/spawncomponent.cpp:268 | SpawnComponent_InitialSpawn | `c` | 2 | *(void**)(compArr + (int64_t)j * 8) |
| code/Game/Component/spawncomponent.cpp:512 | SpawnComponent_DoSpawn | `comp` | 2 | *(void**)(arr + (int64_t)i * 8) |
| code/Game/Component/statemachinecomponent.cpp:572 | StateMachineComponent_LinkControllers_Standalone | `comp` | 2 | *ibp |
| code/Game/Component/statusiconcomponent.cpp:581 | StatusIcon_FindCompByName | `comp` | 2 | *ibp |
| code/Game/Component/storagecomponent.cpp:305 | FindRawMatDescription | `comp` | 2 | *(void**)(compArray + (uintptr_t)i * 8) |
| code/Game/Component/storagedisplaycomponent.cpp:417 | StorageDisplayComponent_Update | `comp` | 2 | *(void**)(compArray + (uintptr_t)c * 8) |
| code/Game/Component/tornadocomponent.cpp:660 | Tornado_StrikeEntityCounted | `slot` | 2 | reinterpret_cast<void*>(found.data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Component/tradecomponent_dock.cpp:817 | IntArrayPush | `node` | 2 | machine ? *reinterpret_cast<uintptr_t*>(machine) : 0 |
| code/Game/Component/wellcomponent.cpp:350 | WellComp_RegisterEntity_91cb0 | `comp` | 2 | *reinterpret_cast<void**>(slot) |
| code/Game/Component/wellcomponent.cpp:351 | WellComp_RegisterEntity_91cb0 | `comp` | 2 | *reinterpret_cast<void**>(slot) |
| code/Game/Component/workplacecomponent.cpp:124 | WPC_FindJobHostComponent | `comp` | 2 | (void*)*slot |
| code/Game/Dialog/buildrequirementui.cpp:75 | FindCompInDesc | `comp` | 2 | *(void**)(array + (uintptr_t)i * 8) |
| code/Game/Dialog/buildrequirementui.cpp:77 | FindCompInDesc | `comp` | 2 | *(void**)(array + (uintptr_t)i * 8) |
| code/Game/Dialog/buildui.cpp:238 | BuildUI_Slot4 | `comp` | 2 | *(void**)(*(uintptr_t*)(resEntry + 0x28) + c * 8) |
| code/Game/Dialog/cemeteryui.cpp:73 | CemeteryGetBurialCount_Impl | `comp` | 2 | (void*)compIPtr |
| code/Game/Dialog/consumeproduceui.cpp:142 | ConsumeProduceUI_Slot4 | `subEntry` | 2 | *(void**)(subArray + (uintptr_t)j * 8) |
| code/Game/Dialog/cropui.cpp:106 | CropUI_IterateResources | `subComp` | 16 | *(uintptr_t*)(subArray + (uintptr_t)j * 8) |
| code/Game/Dialog/debugui.cpp:396 | Game_DestroyAllRawMaterials_Impl | `comp` | 2 | compArray[i] |
| code/Game/Dialog/debugui.cpp:488 | Game_SpawnCitizens_Impl | `comp` | 2 | *pComp |
| code/Game/Dialog/debugui.cpp:604 | Game_SpawnCitizens_Impl | `comp` | 2 | *pComp |
| code/Game/Dialog/destroyui.cpp:134 | DestroyComp_CallBuildStateAction | `smPtr` | 2 | *(uintptr_t*)smPtrAddr |
| code/Game/Dialog/destroyui.cpp:152 | DestroyComponent_Reclaim_Impl | `smPtr` | 2 | *(uintptr_t*)smPtrAddr |
| code/Game/Dialog/destroyui.cpp:173 | DestroyComponent_Rebuild_Impl | `smPtr` | 2 | *(uintptr_t*)smPtrAddr |
| code/Game/Dialog/destroyui.cpp:189 | DestroyComponent_Destroy_Impl | `smPtr` | 2 | *(uintptr_t*)smPtrAddr |
| code/Game/Dialog/edittool_select.cpp:109 | EntityFindUIComponent | `comp` | 2 | *reinterpret_cast<void**>(slot) |
| code/Game/Dialog/edittool_select.cpp:202 | EntityFindUIComponent | `item` | 2 | *reinterpret_cast<void**>( *reinterpret_cast<uintptr_t |
| code/Game/Dialog/entityui.cpp:79 | GetDescPage_Impl | `comp` | 2 | compArray ? compArray[i] : nullptr |
| code/Game/Dialog/pastureui.cpp:69 | PastureCalcMaxCount_Impl | `comp` | 2 | (void*)compIPtr |
| code/Game/Dialog/productionui.cpp:303 | UpdateProductionList | `subEntry` | 2 | *(void**)(subArray + (uintptr_t)j * 8) |
| code/Game/Dialog/professionui.cpp:673 | ProfessionUI_Slot8 | `comp` | 2 | *(uintptr_t*)compIBP |
| code/Game/Dialog/professionui.cpp:704 | ProfessionUI_Slot8 | `comp` | 2 | *(uintptr_t*)compIBP |
| code/Game/Dialog/radiusui.cpp:68 | RadiusUI_Slot5 | `comp` | 2 | (void*)*(uint64_t*)(arrayBase + (uint64_t)i * 8) |
| code/Game/Dialog/selectresourceui.cpp:118 | SelectResourceUI_IterateResources | `subComp` | 16 | *(uintptr_t*)(subArray + (uintptr_t)j * 8) |
| code/Game/Dialog/statusbarui.cpp:238 | StatusBarUI_Startup | `comp` | 2 | compArray[j] |
| code/Game/Dialog/statusbarui.cpp:534 | StatusBarUI_StatusIconHandler_Impl | `comp` | 2 | compArray[i] |
| code/Game/Dialog/statusiconui.cpp:266 | StatusIconUI_FindStatusIconComp | `comp` | 2 | *ibp |
| code/Game/Dialog/storageui.cpp:235 | StorageUI_Startup | `comp` | 2 | *(void**)(*(uintptr_t*)(resEntry + 0x28) + c * 8) |
| code/Game/Dialog/toolbardialog.cpp:200 | ToolbarDialog_UpdateButtons_Impl | `subItem` | 2 | *(void**)(*(uintptr_t*)(subData + 0x28) + (int64_t)j * 8) |
| code/Game/Dialog/toolbardialog.cpp:390 | Toolbar_ShowGroup_Impl | `subItem` | 2 | *(void**)(*(uintptr_t*)(groupData + 0x28) + (uintptr_t)i * 8) |
| code/Game/Dialog/toolbardialog.cpp:614 | Toolbar_Utility_Impl | `subItem` | 2 | *(void**)(*(uintptr_t*)(param + 0x28) + (int64_t)i * 8) |
| code/Game/Dialog/toolbardialog.cpp:800 | HarnessDumpElemTree | `c` | 2 | ctrlArr[i] |
| code/Game/Dialog/toolbardialog.cpp:1294 | DynArray4_Push | `subItem` | 2 | *(void**)(*(uintptr_t*)(entityDesc + 0x28) + (int64_t)j * 8) |
| code/Game/Dialog/toolui.cpp:271 | ToolUI_Slot4 | `entry` | 2 | *(void**)(descArray + (uintptr_t)i * 8) |
| code/Game/Dialog/toolui.cpp:335 | ToolUI_Slot4 | `entry` | 2 | *(void**)(descArray + (uintptr_t)i * 8) |
| code/Game/Dialog/townhall_helpers.cpp:409 | FindWorkPlaceComp | `comp` | 2 | *(void**)slot |
| code/Game/Dialog/townhallui.cpp:699 | TownHallUI_InitAgriculture | `subComp` | 2 | *(uintptr_t*)(subArray + (uintptr_t)j * 8) |
| code/Game/Dialog/townhallui.cpp:1263 | TownHall_FindComponentByTypeId | `comp` | 2 | compArray[i] |
| code/Game/Dialog/townhallui.cpp:1301 | TownHall_FindComponentByTypeId_CE | `comp` | 2 | *(void**)ibpSlot |
| code/Game/Dialog/tradeui.cpp:205 | FindDescComponent | `comp` | 2 | arr[i] |
| code/Game/Dialog/tradeui.cpp:341 | TradeUI_RefreshSellTable | `comp` | 2 | descArray[d] |
| code/Game/Dialog/tradeui.cpp:353 | TradeUI_RefreshSellTable | `comp` | 2 | descArray[d] |
| code/Game/Dialog/tradeui.cpp:393 | TradeUI_RefreshSellTable | `comp` | 2 | compArr[i] |
| code/Game/Dialog/tradeui.cpp:1062 | TradeUI_Serialize | `comp` | 2 | compArray[c] |
| code/Game/Dialog/tradeui.cpp:1332 | FindResourceValue | `comp` | 2 | compArray[k] |
| code/Game/Dialog/tradeui.cpp:2400 | FindDescSubObject | `comp` | 2 | compArray[i] |
| code/Game/Dialog/tradeui_buyentries.cpp:125 | FindComp | `comp` | 2 | arr[i] |
| code/Game/Dialog/tradeui_price.cpp:76 | PriceFindComponentByTypeId | `comp` | 2 | arr[i] |
| code/Game/Dialog/upgradeui.cpp:88 | UpgradeUI_Serialize | `comp` | 2 | *(void**)(compArray + (uintptr_t)i * 8) |
| code/Game/Entity/citizenbehavior.cpp:916 | CitizenBehavior_CountResources | `comp` | 2 | compArray[i] |
| code/Game/Entity/citizenbehavior.cpp:1048 | CitizenBehavior_CountResources | `comp` | 2 | compArray[i] |
| code/Game/Entity/citizenbehavior.cpp:1048 | CitizenBehavior_CountResources | `subComp` | 2 | *reinterpret_cast<void**>( *reinterpret_cast<i |
| code/Game/Entity/citizenbehavior.cpp:1136 | CitizenBehavior_CountResources | `comp` | 2 | compArray[i] |
| code/Game/Entity/citizenbehavior.cpp:1136 | CitizenBehavior_CountResources | `subComp` | 2 | *reinterpret_cast<void**>( *reinterpret_cast<i |
| code/Game/Entity/citizenbehavior.cpp:1705 | CitizenBehavior_FindIdle | `comp` | 2 | (uintptr_t)compIBP |
| code/Game/Entity/citizenbehavior.cpp:1938 | CitizenBehavior_FindStudent | `comp` | 2 | (uintptr_t)compIBP |
| code/Game/Entity/citizenbehavior.cpp:2021 | CitizenBehavior_FindStudent | `comp` | 2 | (uintptr_t)compIBP |
| code/Game/Entity/componententity.cpp:487 | ComponentEntity::Persist | `c` | 2 | *(void**)ibpAddr |
| code/Game/Entity/componententity.cpp:617 | ComponentEntity::Persist | `existComp` | 2 | *(uintptr_t*)existIBPAddr |
| L code/Game/Game/citizenlist_serialize.cpp:830 | DWORD_1401c79e0_TypeId | `cand` | 2 | *slotPtr |
| L code/Game/Game/founding_spawn.cpp:142 | FoundingSpawn_FindCitizenComp | `comp` | 2 | *slot |
| L code/Game/Game/founding_spawn.cpp:173 | FoundingSpawn_FindCitizenComp | `desc` | 2 | *reinterpret_cast<void**>(arr + (uintptr_t)i * 8) |
| L code/Game/Game/starttown.cpp:757 | FindComponentOnEntity | `comp` | 2 | *slot |
| L code/Game/Game/starttown.cpp:776 | FindDescriptionOnTemplate | `desc` | 2 | *(void**)(arr + (uintptr_t)i * 8) |
| L code/Game/Game/starttown.cpp:838 | StartTown_RunOnCreatePass | `comp` | 2 | *slot |
| L code/Game/Game/town_workplace_registry.cpp:72 | FindComponentByTypeId | `comp` | 2 | = nullptr) { ++i |
| code/Game/Player/bridgetool.cpp:73 | BridgeTool_Startup | `comp` | 2 | components[i] |
| code/Game/Player/bridgetool_calcpath.cpp:120 | FindComponentOn | `comp` | 2 | *reinterpret_cast<void**>(slot) |
| code/Game/Player/dragplacementtool.cpp:63 | FindComponentByTypeId | `comp` | 2 | components[i] |
| code/Game/Player/edittool.cpp:257 | TryProjectEntityModelPos | `comp` | 2 | *(void**)slot |
| code/Game/Player/fencedtool.cpp:784 | FencedFindComponent | `comp` | 2 | components[i] |
| code/Game/Player/placementtool.cpp:941 | PlacementTool_Startup_Impl | `comp` | 2 | components[i] |
| code/Game/Player/placementtool.cpp:1152 | PlacementTool::CreateInPlaceObject | `c` | 2 | comps[i] |
| code/Game/Player/removaltool.cpp:71 | RemovalTool_Startup | `comp` | 2 | components[i] |
| code/Game/Player/roadtool.cpp:163 | RoadTool_Startup | `comp` | 2 | components[i] |
| code/Game/Player/roadtool_calcpath.cpp:107 | FindComponentByTypeId | `comp` | 2 | comps[i] |
| code/Game/Player/tool_base_render.cpp:185 | Sub7080 | `comp` | 2 | *reinterpret_cast<void**>(slot) |
| code/Game/Player/tool_base_render.cpp:218 | FindModelComp | `comp` | 2 | *reinterpret_cast<void**>(slot) |
| code/Game/Player/tool_base_tile.cpp:215 | ToolTile_FindModelComponentScoped | `c` | 2 | *reinterpret_cast<void**>(arr + static_cast<long long>(k) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:761 | BstFind | `obj` | 2 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorialimagetoolbaricon.cpp:219 | TutorialImageToolbarIcon::CreateElement | `comp` | 2 | *(void**)(arrayData + i * 8) |
| code/Game/Work/clearstorage.cpp:159 | ClearStorage::Execute | `comp` | 2 | *(void**)(compArray + (uintptr_t)j * 8) |
| code/Game/Work/clearstorage.cpp:182 | ClearStorage::Execute | `comp` | 2 | *(void**)compIBP |
| code/Game/Work/fishingwork.cpp:197 | FindComponentOnEntity | `comp` | 2 | *ibpSlot |
| code/Game/Work/fishingwork.cpp:222 | FindComponentOnSubObject | `comp` | 2 | *(uintptr_t*)(arrayPtr + (uintptr_t)i * 8) |
| code/Game/Work/fishingwork.cpp:606 | FishingWork::Execute | `comp` | 2 | *ibpSlot |
| code/Game/Work/foresterwork.cpp:393 | FindComponentInList | `obj` | 2 | *ibpPtr |
| code/Game/Work/foresterwork.cpp:416 | FindSubComponentInList | `obj` | 2 | *(uintptr_t*)(listBase + (uintptr_t)i * 8) |
| code/Game/Work/gatherrawmaterials.cpp:312 | FindRawMaterialInList | `obj` | 2 | *(uintptr_t*)(arr + (uintptr_t)i * 8) |
| code/Game/Work/herbalistwork.cpp:293 | FindComponentInList | `obj` | 2 | *ibpPtr |
| code/Game/Work/herbalistwork.cpp:317 | FindSubComponentInList | `obj` | 2 | *(uintptr_t*)(listBase + (uintptr_t)i * 8) |
| code/Game/Work/marketwork.cpp:700 | MW_FindComponentByTypeId | `comp` | 2 | *slot |
| code/Game/Work/marketwork.cpp:825 | MW_FindChildByTypeId | `child` | 2 | *reinterpret_cast<void**>( *reinterpret_cast<uintptr_t*>(o |
| code/Game/Work/pickup_gate.cpp:89 | PG_FindCompByTid | `comp` | 2 | *slot |
| code/Game/Work/pickup_gate.cpp:107 | PG_FindDescByTid | `d` | 2 | *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8) |
| code/Game/Work/work_act_hunt.cpp:252 | WorkAct_HuntWorkCtor_bbbc0 | `slot` | 2 | reinterpret_cast<void*>(arrBase + static_cast<intptr_t>(idx) * 8) |
| engine/funcs/engine_funcs.cpp:770 | Entity_FindComponentByExeTypeId | `comp` | 2 | compArray[i] |
| engine/funcs/engine_funcs.h:602 | fn_StorageComponent_QueryCount | `comp` | 2 | *(void**)(compArray + (uintptr_t)j * 8) |
| engine/funcs/engine_funcs.h:681 | FindComponentByTypeId | `compPtr` | 2 | *(void**)(arr + (uintptr_t)i * 8) |
| engine/funcs/engine_funcs.h:2810 | Storage_CanAddItem | `comp` | 2 | *(void**)(compArray + (uintptr_t)i * 8) |
| engine/funcs/engine_funcs.h:3058 | Game_TriggerDisasterByType_Impl | `comp` | 2 | *(void**)(compArray + (uintptr_t)i * 8) |
| engine/funcs/engine_funcs.h:3073 | Game_TriggerDisasterByType_Impl | `comp` | 2 | *(void**)(compArray + (uintptr_t)i * 8) |
| engine/funcs/engine_funcs.h:3144 | Game_SpawnResources_Impl | `comp` | 2 | *(void**)(*(int64_t*)(entity + 0x28) + compOffset * 8) |
| standalone/cropfield_force_start.cpp:71 | FindComp | `c` | 2 | *cs |
| standalone/cropfield_force_start.cpp:72 | FindComp | `c` | 2 | *cs |
| standalone/disaster_harness.cpp:261 | ComponentByName | `comp` | 2 | *slot |
| standalone/finish_builds_harness.cpp:67 | FindComp | `c` | 2 | *cs |
| standalone/food_panel_harness.cpp:132 | FindCompOnEntity | `c` | 2 | *cs |
| standalone/food_panel_harness.cpp:133 | FindCompOnEntity | `c` | 2 | *cs |
| standalone/food_panel_harness.cpp:275 | ProbeClass | `c` | 2 | ctrlArr[i] |
| standalone/food_panel_harness.cpp:276 | ProbeClass | `c` | 2 | ctrlArr[i] |
| standalone/food_panel_harness.cpp:386 | ProbeClass | `c` | 2 | ctrlArr[i] |
| standalone/food_panel_harness.cpp:449 | ProbeClass | `c` | 2 | ctrlArr[i] |
| standalone/food_panel_harness.cpp:491 | ProbeClass | `c` | 2 | ctrlArr[i] |
| standalone/food_placement_harness.cpp:162 | FindComp | `c` | 2 | *cs |
| standalone/food_placement_harness.cpp:163 | FindComp | `c` | 2 | *cs |
| standalone/food_production_diag.cpp:68 | FindComp | `c` | 2 | *cs |
| standalone/food_production_diag.cpp:69 | FindComp | `c` | 2 | *cs |
| standalone/food_production_diag.cpp:97 | DumpStorage | `comp` | 2 | *cs |
| standalone/food_production_diag.cpp:98 | DumpStorage | `comp` | 2 | *cs |
| standalone/orchard_diag.cpp:144 | DumpTownStorage | `comp` | 2 | *cs |
| standalone/orchard_diag.cpp:145 | DumpTownStorage | `comp` | 2 | *cs |
| standalone/orchard_force_start.cpp:105 | TryAssignRealTreeType | `c` | 2 | ctrlArr[i] |
| standalone/panel_hold_harness.cpp:112 | PanelHoldClick | `c` | 2 | *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8) |
| standalone/panel_hold_harness.cpp:159 | FindComp | `c` | 2 | *slot |
| standalone/panel_staffing_driver.cpp:134 | FindComp | `c` | 2 | *cs |
| standalone/panel_staffing_driver.cpp:135 | FindComp | `c` | 2 | *cs |
| standalone/panel_staffing_driver.cpp:164 | SeatViaPanel | `c` | 2 | ctrlArr[i] |
| standalone/tool_harness.cpp:454 | FireRoadTool | `comp` | 2 | *slot |
| standalone/tool_harness.cpp:997 | Comp0TypeId | `comp` | 2 | arr[0] |
| standalone/tool_harness.cpp:998 | Comp0TypeId | `comp` | 2 | arr[0] |
| standalone/tool_harness.cpp:1261 | DlgSettings_InstallBuffer | `comp` | 2 | *reinterpret_cast<void**>( *reinterpret_cast<uintptr_t |
| standalone/tool_harness.cpp:1282 | DlgSettings_InstallBuffer | `comp` | 2 | *reinterpret_cast<void**>( *reinterpret_cast<uintptr_t |
| standalone/tool_harness.cpp:1435 | FireUIComponentCensus | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:1695 | FirePasturePanel | `c` | 2 | ctrlArr[i] |
| standalone/tool_harness.cpp:1823 | FireOrchardPanel | `c` | 2 | ctrlArr[i] |
| standalone/tool_harness.cpp:1935 | FireTradePanel | `c` | 2 | ctrlArr[i] |
| standalone/tool_harness.cpp:2587 | BridgeDumpEntity | `comp` | 2 | *slot |
| standalone/tool_harness.cpp:2672 | FireBridgeTool | `comp` | 2 | *slot |
| standalone/trade_buy_harness.cpp:91 | FindCompOnEntity | `c` | 2 | *cs |
| standalone/trade_buy_harness.cpp:163 | FindCompOnEntity | `c` | 2 | ctrlArr[i] |

### A.4 TYPE-OTHER — slot 2 `Type()`, producer not an array (prove per site) — 48

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Component/boatcomponent_route.cpp:413 | TileInMap | `owner` | 2 | *ownerSlot |
| code/Game/Component/boatcomponent_states.cpp:275 | PostEventToDestination | `node` | 2 | *reinterpret_cast<void**>(smc + 0x48) |
| code/Game/Component/boatcomponent_states.cpp:278 | PostEventToDestination | `node` | 2 | *reinterpret_cast<void**>(smc + 0x48) |
| code/Game/Component/citizen_updatehealth.cpp:422 | PlagueBootReport | `comp` | 2 | *(uintptr_t*)compSlot |
| code/Game/Component/consumeproduce_recipe_dump.cpp:58 | IsRawMat | `c` | 2 |  |
| code/Game/Component/immigrationcomponent_states.cpp:200 | PostEventToEntity | `node` | 2 | *reinterpret_cast<void**>(smc + 0x48) |
| code/Game/Component/nomadcomponent_states.cpp:185 | Idle | `node` | 2 | *reinterpret_cast<void**>(smc + 0x48) |
| code/Game/Dialog/immigrationui.cpp:215 | ImmigrationUI_Slot9 | `smObj` | 2 | *(void**)(bsPtr + 0x48) |
| code/Game/Dialog/tradeui.cpp:2011 | TradeUI_OnDismissButton | `obj` | 2 | *(void**)(buildState + 0x48) |
| L code/Game/Game/force_work.cpp:89 | SafeGetTypeId | `obj` | 2 |  |
| L code/Game/Game/force_work.cpp:90 | SafeGetTypeId | `obj` | 2 |  |
| L code/Game/Game/town_storage_seed.cpp:125 | GetTypeIdOf | `obj` | 2 |  |
| code/Game/Tutorial/tutorialhighlightdialog.cpp:301 | TutHLDiag | `obj` | 2 |  |
| code/Game/Work/work_act_clear.cpp:227 | WorkAct_HarvestAreaCtor_c1530 | `param_4` | 2 |  |
| code/Game/Work/work_act_clear.cpp:256 | WorkAct_HarvestAreaCtor_c1530 | `param_1` | 2 |  |
| code/Game/Work/work_act_clear.cpp:298 | WorkAct_HarvestAreaCtor_c1530 | `param_4` | 2 |  |
| code/Game/Work/work_act_clear.cpp:384 | WorkAct_HarvestAreaCtor_c1530 | `param_1` | 2 |  |
| code/Game/Work/work_act_consume.cpp:275 | WorkAct_ConsumeProduceCtor_bef70 | `param_1` | 2 |  |
| code/Game/Work/work_act_consume.cpp:303 | WorkAct_ConsumeProduceCtor_bef70 | `param_1` | 2 |  |
| code/Game/Work/work_act_consume.cpp:446 | WorkAct_ConsumeProduceCtor_bef70 | `param_1` | 2 |  |
| code/Game/Work/work_act_path.cpp:225 | WAP2_MakeTime | `entity` | 2 |  |
| code/Game/Work/work_act_path.cpp:246 | WAP2_MakeTime | `entity` | 2 |  |
| code/Game/Work/work_act_path.cpp:366 | WAP2_MakeTime | `entity` | 2 |  |
| code/Game/Work/work_act_path.cpp:476 | WAP2_MakeTime | `entity` | 2 |  |
| standalone/cropfield_force_start.cpp:87 | PostNodeEvent | `node` | 2 | *reinterpret_cast<void**>(sm) |
| standalone/cropfield_force_start.cpp:89 | PostNodeEvent | `node` | 2 | *reinterpret_cast<void**>(sm) |
| standalone/entity_comp_validator.cpp:89 | TypeNameOf | `obj` | 2 |  |
| standalone/entity_comp_validator.cpp:90 | TypeNameOf | `obj` | 2 |  |
| standalone/food_panel_harness.cpp:358 | ProbeClass | `wpDesc` | 2 | *reinterpret_cast<void**>(w + 0x18) |
| standalone/food_workers_harness.cpp:140 | Fire | `comp` | 2 | *cs |
| standalone/food_workers_harness.cpp:141 | Fire | `comp` | 2 | *cs |
| standalone/market_overstock_harness.cpp:98 | GetTypeIdOf | `obj` | 2 |  |
| standalone/orchard_cut_harness.cpp:84 | Init | `cc` | 2 | *cs |
| standalone/orchard_diag.cpp:211 | DumpOrchards | `comp` | 2 | *cs |
| standalone/orchard_diag.cpp:259 | DumpOrchards | `comp` | 2 | reinterpret_cast<uintptr_t>(orchComp) |
| standalone/orchard_diag.cpp:259 | DumpOrchards | `wc` | 2 | *cs |
| standalone/orchard_force_start.cpp:177 | FireForceStart | `comp` | 2 | *cs |
| standalone/orchard_growth_harness.cpp:110 | Enabled | `comp` | 2 | *cs |
| standalone/orchard_workers_harness.cpp:134 | Fire | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:644 | FirePastureTool | `comp` | 2 | *hs |
| standalone/tool_harness.cpp:1531 | FireDlgSettingsRoute | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:1588 | FireDlgSettingsRoute | `comp` | 2 | *reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8) |
| standalone/tool_harness.cpp:1652 | FirePasturePanel | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:1695 | FirePasturePanel | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:1789 | FireOrchardPanel | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:1823 | FireOrchardPanel | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:1902 | FireTradePanel | `comp` | 2 | *cs |
| standalone/tool_harness.cpp:1935 | FireTradePanel | `comp` | 2 | *cs |

### A.5 TOOL — receiver named as the player tool (prove per site: the producer is the active-tool slot) — 17

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Dialog/toolbardialog.cpp:887 | HarnessDumpElemTree | `toolCtrl` | 9 | *(void**)(gs + 0x28) |
| L code/Game/Game/gameupdateloop.cpp:1882 | GameUpdateLoop_Draw | `tool` | 8 | *(void**)(entityUI + 0x28) |
| code/Game/Player/tool_activation_standalone.cpp:373 | Tree_Insert | `tool` | 6 | *(void**)toolSlot |
| code/Game/Player/tool_activation_standalone.cpp:397 | Tree_Insert | `t2` | 9 | *(void**)toolSlot |
| code/Game/Player/tool_activation_standalone.cpp:412 | Tree_Insert | `tool` | 7 | *(void**)toolSlot |
| code/Game/Player/tool_activation_standalone.cpp:421 | Tree_Insert | `tool` | 12 | *(void**)toolSlot |
| code/Game/Player/tool_activation_standalone.cpp:437 | Tree_Insert | `tool` | 8 | *(void**)toolSlot |
| standalone/tool_harness.cpp:165 | FireRemovalTool | `tool` | kSlot_Startup | RemovalTool_Factory_SA() |
| standalone/tool_harness.cpp:174 | FireRemovalTool | `tool` | kSlot_CalcCost | RemovalTool_Factory_SA() |
| standalone/tool_harness.cpp:190 | FireRemovalTool | `tool` | kSlot_CalcPath | RemovalTool_Factory_SA() |
| standalone/tool_harness.cpp:512 | FireRoadTool | `tool` | 14 | *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28) |
| standalone/tool_harness.cpp:525 | FireRoadTool | `tool` | 13 | *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28) |
| standalone/tool_harness.cpp:703 | FirePastureTool | `tool` | 14 | *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28) |
| standalone/tool_harness.cpp:709 | FirePastureTool | `tool` | 13 | *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28) |
| standalone/tool_harness.cpp:743 | FirePreviewPlacement | `tool` | 6 | entityUI ? *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + |
| standalone/tool_harness.cpp:2779 | FireBridgeTool | `tool` | kSlot_CalcCost | *reinterpret_cast<void**>(static_cast<uint8_t*>(entityUI) + 0x28) |
| standalone/tool_harness.cpp:2818 | BridgeCommit | `tool` | kSlot_CalcPath |  |

### A.6 CTRL — state-machine / controller-member node (prove per site: D6 member vs StateMachineComponent chain) — 12

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Component/firecomponent_smstate.cpp:110 | ForceSMState | `n` | 1 | *node |
| code/Game/Component/orchardcomponent_states.cpp:200 | OrchardRequestState | `chainHead` | 1 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/orchardcomponent_states.cpp:202 | OrchardRequestState | `chainHead` | 1 | *reinterpret_cast<void**>(sm) |
| code/Game/Component/resourceproducecomponent_states.cpp:317 | ResourceProduceComponent::Command | `smcIbp` | 8 | 0 |
| code/Game/Component/resourceproducecomponent_states.cpp:320 | ResourceProduceComponent::Command | `smcIbp` | 8 | 0 |
| code/Game/Component/statemachinecomponent.cpp:144 | StateMachineComponent_Serialize | `ibpValue` | 16 | nullptr |
| code/Game/Component/statemachinecomponent.cpp:154 | StateMachineComponent_Serialize | `ibpValue` | 17 | nullptr |
| code/Game/Component/statemachinecomponent.cpp:252 | StateMachineComponent_OnUpdate2 | `ibpValue` | 17 | nullptr |
| code/Game/Component/statemachinecomponent.cpp:624 | StateMachineComponent_LinkControllers_Standalone | `node` | 0 | cs ? cs(comp) : nullptr |
| code/Game/Work/work_act_clear.cpp:202 | WAC_AppendInline | `node` | 56 | reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(plVar15))[*rein |
| code/Game/Work/work_act_consume.cpp:223 | WCP_AppendInline | `node` | 7 | *reinterpret_cast<uintptr_t*>(base + static_cast<uintptr_t>(count) * 8 |
| code/Game/Work/work_leaf_hunt.cpp:332 | WorkLeaf_ConsumeProduce_b310 | `sm` | 1 | *reinterpret_cast<void**>(ent + 0x48) |

### A.7 NAMED — receiver named like a converted class (prove per site) — 97

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Action/sendcomponentmessage.cpp:243 | SendComponentMessage_AbstractImpl_Impl | `comp` | 15 | *(uintptr_t*)compIBP |
| code/Game/Action/sendcomponentmessage.cpp:243 | SendComponentMessage_AbstractImpl_Impl | `compObj` | 15 | foundComp |
| code/Game/Component/buildcomponent_sub8630.cpp:99 | BC8630_BuildDescTypeId | `storage + 0x28` | s * 2 | 0 |
| code/Game/Component/buildcomponent_sub8630.cpp:100 | BC8630_BuildDescTypeId | `storage + 0x28` | s * 2 + 1 | 0 |
| code/Game/Component/citizenai_housing_idle.cpp:509 | Helper_sub_7b60 | `payload` | 72 | *reinterpret_cast<int64_t*>(cur) |
| code/Game/Component/citizenai_housing_idle.cpp:514 | Helper_sub_7b60 | `payload` | 32 | *reinterpret_cast<int64_t*>(cur) |
| code/Game/Component/citizenai_work_scheduler.cpp:412 | WS_cb50 | `wobj` | 10 | *reinterpret_cast<uint8_t**>(pWorkIBP) |
| code/Game/Component/citizenai_work_scheduler.cpp:430 | WS_cb50 | `wobj2` | 10 | *reinterpret_cast<void**>(pWorkIBP) |
| code/Game/Component/cropfieldcomponent.cpp:419 | CropFieldComponent_InitFuncs | `growth` | 9 | 0 |
| code/Game/Component/cropfieldcomponent.cpp:420 | CropFieldComponent_InitFuncs | `growth` | 9 | 0 |
| code/Game/Component/destroycomponent_state6.cpp:217 | FindOn | `act` | idx |  |
| code/Game/Component/destroycomponent_state6.cpp:414 | MeshFromModelDesc | `mc` | 7 | reinterpret_cast<uintptr_t>(modelComp) |
| code/Game/Component/destroycomponent_state6.cpp:423 | MeshFromModelDesc | `mc` | 6 | reinterpret_cast<uintptr_t>(modelComp) |
| code/Game/Component/destroycomponent_state6.cpp:468 | MeshFromModelDesc | `mc` | 7 | reinterpret_cast<uintptr_t>(modelComp) |
| code/Game/Component/destroycomponent_state6.cpp:468 | MeshFromModelDesc | `dc` | 7 | reinterpret_cast<uintptr_t>(decalIbp) |
| code/Game/Component/destroycomponent_state6.cpp:469 | MeshFromModelDesc | `mc` | 6 | reinterpret_cast<uintptr_t>(modelComp) |
| code/Game/Component/destroycomponent_state6.cpp:469 | MeshFromModelDesc | `dc` | 6 | reinterpret_cast<uintptr_t>(decalIbp) |
| code/Game/Component/multiobjectcomponent.cpp:220 | MultiObjectComponent_InitFuncs | `comp` | 12 | *reinterpret_cast<void**>(comps + static_cast<uintptr_t>(k) * 8) |
| code/Game/Component/particlecomponent.cpp:643 | PC_GeoArrayResize | `res` | 8 | *(void**)(emArr + (size_t)i * 0x28) |
| code/Game/Component/particlecomponent.cpp:645 | PC_GeoArrayResize | `res` | 8 | *(void**)(emArr + (size_t)i * 0x28) |
| code/Game/Component/pasturecomponent_spawn.cpp:343 | PastureCap | `pasture` | 14 | reinterpret_cast<void*>(pastureIBP) |
| code/Game/Component/pasturecomponent_states.cpp:819 | PastureCtrl_PopulateTable | `pasture` | 13 |  |
| code/Game/Component/pasturecomponent_states.cpp:820 | PastureCtrl_PopulateTable | `pasture` | 13 |  |
| code/Game/Component/quarrycomponent_deform.cpp:168 | QuarryDecalSuspend | `decal` | 7 | QuarryFindComp(s_fn_FindComp_Decal, entity) |
| code/Game/Component/quarrycomponent_deform.cpp:177 | QuarryDecalResume | `decal` | 6 |  |
| code/Game/Component/resourceproduce_creatework.cpp:124 | WrapIBP | `w + 0x58` | i | reinterpret_cast<uintptr_t>(mem) |
| code/Game/Component/resourceproduce_creatework.cpp:321 | WrapIBP | `w + 0x68` | i | reinterpret_cast<uintptr_t>(mem) |
| code/Game/Component/statemachinecomponent.cpp:585 | StateMachineComponent_LinkControllers_Standalone | `comp` | 17 | *ibp |
| code/Game/Component/statemachinecomponent.cpp:598 | StateMachineComponent_LinkControllers_Standalone | `comp` | 16 | *ibp |
| code/Game/Component/statusiconcomponent.cpp:782 | StatusIcon_Toggle | `c` | 8 | (uintptr_t)selfC |
| code/Game/Dialog/controlui.cpp:127 | ControlUI_Slot4 | `subCompPtr` | 13 | (void*)*(uint64_t*)(s + 0x38) |
| code/Game/Dialog/controlui.cpp:165 | ControlUI_Slot7 | `subCompPtr` | 13 | (void*)*(uint64_t*)(s + 0x38) |
| code/Game/Dialog/controlui.cpp:186 | ControlUI_Slot7 | `subCompPtr` | 13 | (void*)*(uint64_t*)(s + 0x38) |
| code/Game/Dialog/controlui.cpp:219 | ControlUI_Slot9 | `subCompPtr` | 14 | (void*)*(uint64_t*)(s + 0x38) |
| code/Game/Dialog/cropui_promote.cpp:103 | CropUI_Slot11 | `comp` | 14 | FindComp(self) |
| code/Game/Dialog/destroyui.cpp:157 | DestroyComponent_Reclaim_Impl | `destroyComp` | 4 |  |
| code/Game/Dialog/mod_dialog_vtables.cpp:163 | ModDlg_LoadDialogResource | `obj` | 0 | *reinterpret_cast<void**>(e + off) |
| code/Game/Dialog/mod_dialog_vtables.cpp:164 | ModDlg_LoadDialogResource | `obj` | 0 | *reinterpret_cast<void**>(e + off) |
| code/Game/Dialog/toolbardialog.cpp:358 | Toolbar_ShowGroup_Impl | `controller` | 5 | *(void**)(pc + 0x28) |
| code/Game/Dialog/toolbardialog.cpp:486 | Toolbar_ShowGroup_Impl | `ctrl` | 11 | *(void**)(pc + 0x28) |
| code/Game/Dialog/toolbardialog.cpp:492 | Toolbar_ShowGroup_Impl | `ctrl` | 11 | *(void**)(pc + 0x28) |
| code/Game/Dialog/toolbardialog.cpp:522 | Toolbar_ShowGroup_Impl | `ctrl` | 4 | *(void**)(pc + 0x28) |
| code/Game/Dialog/toolbardialog.cpp:831 | HarnessDumpElemTree | `c` | slotIndex | ctrlArr[i] |
| code/Game/Dialog/toolbardialog.cpp:831 | HarnessDumpElemTree | `ctrl` | slotIndex | reinterpret_cast<void*>(ctrlBuf[0]) |
| code/Game/Entity/componententity.cpp:308 | ComponentEntity::ShutdownComponents | `compPtr` | 5 | *(uintptr_t*)ibpAddr |
| code/Game/Entity/componententity.cpp:338 | ComponentEntity::StartupComponents | `compPtr` | 4 | *(uintptr_t*)ibpAddr |
| code/Game/Entity/componententity.cpp:669 | ComponentEntity::Persist | `compPtr` | 4 | (void*)newIBP |
| code/Game/Entity/componententity.cpp:760 | ComponentEntity::OnAddedPass | `compPtr` | 6 | *(uintptr_t*)ibpAddr |
| L code/Game/Entity/entitylist.cpp:987 | AuditDyingEntity | `c` | 7 | *reinterpret_cast<int64_t*>(arr + (int64_t)i * 8) |
| L code/Game/Entity/entitylist.cpp:988 | AuditDyingEntity | `c` | 7 | *reinterpret_cast<int64_t*>(arr + (int64_t)i * 8) |
| L code/Game/Game/game_leaf_helpers.cpp:181 | Game_ImageCaptureDtor | `obj` | 0 | *reinterpret_cast<void**>(b + off) |
| L code/Game/Game/game_leaf_helpers.cpp:182 | Game_ImageCaptureDtor | `obj` | 0 | *reinterpret_cast<void**>(b + off) |
| L code/Game/Game/gameupdateloop_serialize.cpp:648 | InitCamView | `comp` | 11 | (uintptr_t)*compSlot |
| L code/Game/Game/starttown.cpp:878 | StartTown_RunOnCreatePass | `comp` | 7 | *slot |
| L code/Game/Game/tornado.cpp:598 | Tornado_CreateInstance | `p` | k | fn_MemAlloc(g_pMemAllocSingleton, 0xe0, 8) |
| L code/Game/Game/tornado.cpp:639 | InstallTornadoVtable | `p` | k |  |
| L code/Game/Game/tornado.cpp:640 | InstallTornadoVtable | `p` | k |  |
| code/Game/Tutorial/tutorial_wave.cpp:525 | BstFind | `obj` | 8 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:593 | BstFind | `obj` | 9 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:593 | BstFind | `img` | 9 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:595 | BstFind | `obj` | 9 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:595 | BstFind | `img` | 9 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:701 | BstFind | `obj` | 9 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:714 | BstFind | `obj` | 8 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:719 | BstFind | `obj` | 8 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Tutorial/tutorial_wave.cpp:768 | BstFind | `obj` | 10 | *reinterpret_cast<void**>(data + static_cast<uintptr_t>(i) * 8) |
| code/Game/Work/gatherrawmaterials_ctor.cpp:116 | GRMC_BuildDescTypeId | `storage + 0x28` | s * 2 | 0 |
| code/Game/Work/gatherrawmaterials_ctor.cpp:117 | GRMC_BuildDescTypeId | `storage + 0x28` | s * 2 + 1 | 0 |
| code/Game/Work/work_act_clear.cpp:232 | WorkAct_HarvestAreaCtor_c1530 | `param_4` | 3 |  |
| code/Game/Work/work_act_consume.cpp:356 | WorkAct_ConsumeProduceCtor_bef70 | `param_3` | 5 |  |
| standalone/dlgctrl_route.cpp:263 | Visit | `obj` | j | factory() |
| standalone/dlgctrl_route.cpp:263 | Visit | `obj` | i | factory() |
| standalone/dlgctrl_route.cpp:264 | Visit | `obj` | i | factory() |
| standalone/dlgctrl_route.cpp:282 | Visit | `obj` | kSpecs[s | factory() |
| standalone/dlgctrl_route.cpp:288 | Visit | `obj` | kSpecs[s | factory() |
| standalone/food_panel_harness.cpp:171 | ProbeClass | `comp` | 13 | c |
| standalone/food_panel_harness.cpp:172 | ProbeClass | `comp` | 14 | c |
| standalone/food_panel_harness.cpp:395 | ProbeClass | `wpUi` | 9 | c |
| standalone/food_panel_harness.cpp:460 | ProbeClass | `radius` | 6 | c |
| standalone/food_panel_harness.cpp:517 | ProbeClass | `wpUi` | 9 | c |
| standalone/food_panel_harness.cpp:517 | ProbeClass | `toolUi` | 9 | c |
| standalone/food_placement_harness.cpp:244 | ProbeClass | `comp` | i | c |
| standalone/orchard_force_start.cpp:132 | TryAssignRealTreeType | `c` | 9 | ctrlArr[i] |
| standalone/orchard_force_start.cpp:132 | TryAssignRealTreeType | `oui` | 9 | c |
| standalone/panel_hold_harness.cpp:136 | PanelHoldClick | `ctrl` | 9 | reinterpret_cast<void*>(ctrlIBP) |
| standalone/panel_staffing_driver.cpp:168 | SeatViaPanel | `wpUi` | 9 | c |
| standalone/scene_watch.cpp:454 | IsMeshInstance | `obj` | 3 |  |
| standalone/tool_harness.cpp:1711 | FirePasturePanel | `comp` | 9 | *cs |
| standalone/tool_harness.cpp:1711 | FirePasturePanel | `c` | 9 | ctrlArr[i] |
| standalone/tool_harness.cpp:1711 | FirePasturePanel | `pui` | 9 | c |
| standalone/tool_harness.cpp:1856 | FireOrchardPanel | `comp` | 9 | *cs |
| standalone/tool_harness.cpp:1856 | FireOrchardPanel | `c` | 9 | ctrlArr[i] |
| standalone/tool_harness.cpp:1856 | FireOrchardPanel | `oui` | 9 | c |
| standalone/tool_harness.cpp:2393 | FireUpgradeClick | `ctrl` | 4 | dialog ? GetControllerByName(dialog, " ") : nullptr |
| standalone/tool_harness.cpp:2394 | FireUpgradeClick | `ctrl` | 4 | dialog ? GetControllerByName(dialog, " ") : nullptr |
| standalone/tool_harness.cpp:2405 | FireUpgradeClick | `ctrl` | 9 | dialog ? GetControllerByName(dialog, " ") : nullptr |
| standalone/tool_harness.cpp:2409 | FireUpgradeClick | `ctrl` | 9 | dialog ? GetControllerByName(dialog, " ") : nullptr |

### A.8 OTHER — read by hand; verdict in the row — 21

| site | fn | receiver | slot | producer |
|---|---|---|---:|---|
| code/Game/Component/buildcomponent_state8.cpp:176 | BC8_ReaddDrawChildren | `h` | 12 | *reinterpret_cast<void**>(arr + (int64_t)k * 8) |
| code/Game/Component/particlecomponent.cpp:492 | PC_GeoArrayResize | `ge->geo` | 9 | &geoArr[index] |
| code/Game/Component/particlecomponent.cpp:494 | PC_GeoArrayResize | `ge->geo` | 9 | &geoArr[index] |
| code/Game/Component/particlecomponent.cpp:503 | PC_GeoArrayResize | `ge->geo` | 1 | &geoArr[index] |
| code/Game/Component/particlecomponent.cpp:508 | PC_GeoArrayResize | `ge->geo` | 1 | &geoArr[index] |
| code/Game/Component/particlecomponent.cpp:570 | PC_GeoArrayResize | `ge->geo` | 1 | &geoArr[index] |
| code/Game/Component/particlecomponent.cpp:572 | PC_GeoArrayResize | `ge->geo` | 1 | &geoArr[index] |
| code/Game/Component/particlecomponent.cpp:752 | PC_GeoArrayResize | `ge->geo` | 1 | &geoArr[i] |
| code/Game/Component/particlecomponent.cpp:754 | PC_GeoArrayResize | `ge->geo` | 1 | &geoArr[i] |
| code/Game/Component/zonecomponent.cpp:111 | ZoneComponent_ZoneMapAdd | `entityPtr` | 5 | entityPtrV |
| code/Game/Dialog/optionsdialog.cpp:948 | OptionsDialog_ApplyGameSettings | `gameWorld` | 3 | g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr |
| code/Game/Dialog/optionsdialog.cpp:949 | OptionsDialog_ApplyGameSettings | `gameWorld` | 3 | g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr |
| code/Game/Entity/componententity.cpp:58 | TemplateComponentType | `compTemplate` | 8 |  |
| L code/Game/Game/goals_serialize.cpp:683 | LayoutB_Find | `ibpField` | 4 | node + 0x08 |
| L code/Game/Game/tornado.cpp:614 | Tornado_Recreate | `self` | 0 |  |
| L code/Game/Game/tornado.cpp:623 | Tornado_Destruct | `self` | 0 |  |
| code/Game/Object/internal_factory_audit.cpp:74 | IsRegistered | `fb` | idx | reinterpret_cast<uint8_t*>(f) |
| code/Game/Player/tool_base_render.cpp:253 | SampleTile | `hf + 0xd8` | (int64_t)dim * cb + ca |  |
| code/Game/Work/pickup_gate.cpp:139 | PickupGate_AcceptMask_12d560 | `ret` | 0x3c | s_pg_FindStorage ? s_pg_FindStorage(*reinterpr |
| code/Game/Work/plantarea.cpp:771 | PlantArea::GetTarget | `result` | 9 | s_fn_FindSubEntity(entity, subIBP) |
| code/Game/Work/plantorchard.cpp:624 | PlantOrchard::GetTarget | `result` | 9 | s_fn_FindSubEntity(entity, subIBP) |

### A.9 Excluded — 142 (RUNTIME: Runtime/Graphics/UI/IO receivers by name or declared type; NOT-VPTR: the `*(T*)(*(uintptr_t*)p + off)` idiom on an IBP / singleton / world pointer, i.e. a double FIELD read)

| file | family | sites |
|---|---|---:|
| code/Game/Component/ambient_externalptr_serialize.cpp | RUNTIME | 1 |
| code/Game/Component/buildcomponent_state8.cpp | RUNTIME | 2 |
| code/Game/Component/growthcomponent.cpp | NOT-VPTR | 1 |
| code/Game/Component/herdcomponent.cpp | RUNTIME | 2 |
| code/Game/Component/orchardcomponent_states.cpp | RUNTIME | 2 |
| code/Game/Component/workplacecomponent.cpp | RUNTIME | 1 |
| code/Game/Dialog/eventnotifydialog.cpp | NOT-VPTR | 1 |
| code/Game/Dialog/moddialog.cpp | RUNTIME | 1 |
| code/Game/Dialog/residenceui.cpp | RUNTIME | 2 |
| code/Game/Dialog/saveloaddialog.cpp | RUNTIME | 4 |
| code/Game/Dialog/spritecaptureui.cpp | RUNTIME | 1 |
| code/Game/Dialog/toolbardialog.cpp | RUNTIME | 9 |
| code/Game/Dialog/townhallui.cpp | NOT-VPTR | 17 |
| code/Game/Dialog/townhallui.cpp | RUNTIME | 2 |
| code/Game/Dialog/tradeui.cpp | RUNTIME | 2 |
| code/Game/Dialog/workplaceui.cpp | RUNTIME | 4 |
| code/Game/Game/citizenlist_serialize.cpp | RUNTIME | 2 |
| code/Game/Game/dialogloaderloop.cpp | RUNTIME | 1 |
| code/Game/Game/game_leaf_helpers.cpp | RUNTIME | 2 |
| code/Game/Game/gameupdateloop.cpp | RUNTIME | 1 |
| code/Game/Game/menuscene.cpp | RUNTIME | 1 |
| code/Game/Game/weather_sim.cpp | RUNTIME | 4 |
| code/Game/Goal/statvaluedispatch.cpp | NOT-VPTR | 1 |
| code/Game/Player/edittool.cpp | RUNTIME | 1 |
| code/Game/Player/fencedtool.cpp | NOT-VPTR | 2 |
| code/Game/Player/fencedtool.cpp | RUNTIME | 1 |
| code/Game/Player/placementtool.cpp | RUNTIME | 1 |
| code/Game/Player/tool_base_createbar.cpp | NOT-VPTR | 1 |
| code/Game/Player/tool_base_render.cpp | RUNTIME | 5 |
| code/Game/Tutorial/tutorial_next_scenario.cpp | NOT-VPTR | 2 |
| code/Game/Work/buildroad.cpp | NOT-VPTR | 1 |
| code/Game/Work/firework.cpp | NOT-VPTR | 1 |
| code/Game/Work/foresterwork.cpp | NOT-VPTR | 3 |
| code/Game/Work/gatherrawmaterials.cpp | NOT-VPTR | 1 |
| code/Game/Work/harvestarea.cpp | NOT-VPTR | 1 |
| code/Game/Work/harvestorchard.cpp | NOT-VPTR | 1 |
| code/Game/Work/herbalistwork.cpp | NOT-VPTR | 2 |
| code/Game/Work/huntwork.cpp | NOT-VPTR | 1 |
| code/Game/Work/pasturework.cpp | NOT-VPTR | 1 |
| code/Game/Work/pickup.cpp | NOT-VPTR | 7 |
| code/Game/Work/work_act_consume.cpp | NOT-VPTR | 1 |
| code/Game/Work/work_act_consume.cpp | RUNTIME | 1 |
| code/Game/Work/work_act_hunt.cpp | NOT-VPTR | 4 |
| code/Game/Work/work_act_perform.cpp | NOT-VPTR | 7 |
| code/Game/Work/work_act_rawmat.cpp | NOT-VPTR | 9 |
| code/Game/Work/work_base_actionctor.cpp | NOT-VPTR | 12 |
| code/Game/Work/work_base_walkpos.cpp | NOT-VPTR | 2 |
| code/Game/Work/work_leaf_gather.cpp | NOT-VPTR | 1 |
| standalone/autostart.cpp | RUNTIME | 3 |
| standalone/main.cpp | RUNTIME | 3 |
| standalone/pausemenu_harness.cpp | RUNTIME | 3 |

## Appendix B — (2) CALLs of old entry points from outside the class's own files

| site | callee | kind | becomes |
|---|---|---|---|
| code/Game/Component/charactercomponent_serialize.cpp:109 | `Component_SerializeReimpl` | door | `Component::Persist` |
| code/Game/Component/citizenaicomponent_serialize.cpp:67 | `Component_SerializeReimpl` | door | `Component::Persist` |
| code/Game/Component/citizencomponent_serialize.cpp:167 | `Component_SerializeReimpl` | door | `Component::Persist` |
| code/Game/Dialog/cropui_promote.cpp:188 | `CropUI_Slot7` | delegate-body | `SelectResourceUI::OnUpdateLayout` |
| code/Game/Dialog/cropui_promote.cpp:200 | `SelectResourceUI_Slot8` | delegate-body | `SelectResourceUI::OnUpdate` |
| code/Game/Dialog/orchardui.cpp:83 | `SelectResourceUI_Slot4` | delegate-body | `SelectResourceUI::OnCreated` |
| code/Game/Dialog/orchardui.cpp:302 | `SelectResourceUI_Slot8` | delegate-body | `SelectResourceUI::OnUpdate` |
| code/Game/Dialog/orchardui.cpp:309 | `SelectResourceUI_Slot9` | delegate-body | `SelectResourceUI::OnElementEvent` |
| code/Game/Dialog/pastureui.cpp:144 | `SelectResourceUI_Slot4` | delegate-body | `SelectResourceUI::OnCreated` |
| code/Game/Dialog/pastureui.cpp:424 | `SelectResourceUI_Slot8` | delegate-body | `SelectResourceUI::OnUpdate` |
| L code/Game/Entity/entitylist.cpp:1316 | `ComponentEntity_RefreshEntityFlags` | door | `ComponentEntity::RefreshEntityFlags` |
| L code/Game/Game/starttown.cpp:276 | `PickingComponent_RegisterMesh` | delegate-body | `PickingComponent::OnAdded` |
| L code/Game/Game/starttown.cpp:3677 | `CitizenComponent_OnAdd` | delegate-body | `CitizenComponent::OnAdded` |
| code/Game/Player/fencedtool.cpp:117 | `DragTool_OnKey` | delegate-body | `DragTool::OnKey` |
| code/Game/Player/fencedtool.cpp:186 | `DragTool_CancelDrag_Impl` | delegate-body | `DragTool::OnCancel` |
| code/Game/Player/roadtool.cpp:197 | `DragTool_OnKey` | delegate-body | `DragTool::OnKey` |
| standalone/food_panel_harness.cpp:540 | `EditTool_CloseInfoDialog` | delegate-body | `EditTool::Shutdown` |
| standalone/orchard_cut_harness.cpp:91 | `OrchardComponent_CanCommand` | delegate-body | `OrchardComponent::CommandMask` |
| standalone/orchard_cut_harness.cpp:95 | `OrchardComponent_Command` | delegate-body | `OrchardComponent::Command` |
| standalone/orchard_force_start.cpp:250 | `EditTool_CloseInfoDialog` | delegate-body | `EditTool::Shutdown` |
| standalone/panel_staffing_driver.cpp:197 | `EditTool_CloseInfoDialog` | delegate-body | `EditTool::Shutdown` |
| standalone/reg_code_lane6.cpp:58 | `ToolbarDescription_Dtor` | door | `ToolbarDescription::DeletingDtor` |
| standalone/reg_herd_live.cpp:59 | `HerdComponent_OnAdd` | delegate-body | `HerdComponent::OnAdded` |
| standalone/reg_herd_live.cpp:60 | `HerdComponent_OnRemove` | X_entry | `HerdComponent::OnRemove` |
| standalone/reg_herd_live.cpp:62 | `PopulationComponent_Startup` | delegate-body | `PopulationComponent::Startup` |
| standalone/reg_herd_live.cpp:64 | `PopulationComponent_OnAdd` | delegate-body | `PopulationComponent::OnAdded` |
| standalone/reg_herd_live.cpp:65 | `PopulationComponent_OnRemove` | delegate-body | `PopulationComponent::OnRemove` |

## Appendix C — (3) exe-address doors onto converted members

### C.1 `ResolveAddr(Module::Exe, …)` / file-local `R<>(…)` outside engine_funcs.cpp

| site | address | member (retail slot) |
|---|---|---|
| code/Game/Component/absorbresourcecomponent.cpp:67 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/ambientemittercomponent.cpp:283 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/ambientenvironmentcomponent_serialize.cpp:48 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/animalwave_serialize.cpp:69 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/attendancecomponent.cpp:155 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/autopickupcomponent.cpp:117 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/boatcomponent.cpp:210 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/buildcomponent.cpp:71 | 0x140108d40 | BuildComponent.slot6 |
| code/Game/Component/cemeterycomponent.cpp:307 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/clearareacomponent.cpp:61 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/clearresourcecomponent.cpp:60 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/cropfieldcomponent.cpp:53 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/decalcomponent.cpp:374 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/decalcomponent.cpp:385 | 0x14011b560 | DecalComponent.slot6 |
| code/Game/Component/destroycomponent.cpp:49 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/diseasecomponent.cpp:172 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/floorcomponent.cpp:86 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/fruitcomponent_serialize.cpp:49 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/growthcomponent_serialize.cpp:66 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/happinesscomponent.cpp:84 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/healercomponent.cpp:50 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/highlightcomponent.cpp:43 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/immigrationcomponent.cpp:67 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/interactcomponent.cpp:60 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/mapcomponent.cpp:64 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/marketlocationcomponent.cpp:42 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/minecomponent.cpp:52 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/multiobjectcomponent.cpp:163 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/naturalresourcecomponent.cpp:179 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/nomadcomponent.cpp:500 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/orchardcomponent.cpp:279 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/particlecomponent.cpp:792 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/particlecomponent.cpp:806 | 0x140052b30 | ParticleComponent.slot8 |
| code/Game/Component/pasturecomponent.cpp:73 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/pickingcomponent.cpp:284 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/quarrycomponent.cpp:45 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/rawmaterialcomponent.cpp:93 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/residencecomponent.cpp:64 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/spawncomponent.cpp:301 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/statusiconcomponent.cpp:80 | 0x14012bbd0 | StatusIconComponent.slot8 |
| code/Game/Component/storagecomponent.cpp:61 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/storagedisplaycomponent.cpp:118 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/storagelocationcomponent.cpp:43 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/terraformcomponent.cpp:192 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/tombstonecomponent.cpp:198 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/tornadocomponent.cpp:211 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/trackercomponent.cpp:112 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/uicomponent_serialize.cpp:53 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/wellcomponent.cpp:215 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/wildanimalcomponent.cpp:67 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/workcomponent.cpp:48 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/workplacecomponent.cpp:69 | 0x1401189b0 | Component.slot3 |
| code/Game/Component/zonecomponent.cpp:293 | 0x1401189b0 | Component.slot3 |
| code/Game/Player/removaltool.cpp:497 | 0x1401035e0 | RemovalTool.slot13 |
| code/Game/Work/pasturework.cpp:246 | 0x1400f2370 | PastureWork.slot4 |
| code/Game/Work/pickup.cpp:87 | 0x1400f2f00 | Pickup.slot4 |
| standalone/pasture_harness.cpp:40 | 0x140127720 | PastureComponent.slot15 |

### C.2 `engine_funcs` globals bound to a converted class's member or table (by initialiser)

| global | initialiser | target | code uses (CALL / ref / printf-only) |
|---|---|---|---|
| `fn_ComponentBase_Serialize` | engine_funcs.cpp:2260 `0x1401189b0` | Component.slot3 (fn) | 5 / 2 / 0 |
| `fn_ConsumeProduceUI_Slot4` | engine_funcs.cpp:1305 `0x14005f3d0` | ConsumeProduceUI.slot4 (fn) | 0 / 0 / 0 |
| `fn_DragTool_OnKey` | engine_funcs.cpp:1077 `0x1400fc6b0` | BridgeTool.slot6, DragPlacementTool.slot6, DragTool.slot6 (fn) | 0 / 0 / 0 |
| `fn_DragTool_Shutdown` | engine_funcs.cpp:1079 `0x1400fc640` | BridgeTool.slot9, DragPlacementTool.slot9, DragTool.slot9 (fn) | 0 / 0 / 0 |
| `fn_Work_SerializeBase` | engine_funcs.cpp:2375 `0x1400f7400` | Work.slot3 (fn) | 0 / 0 / 0 |
| `g_AchievementDialog_Vtable` | engine_funcs.cpp:1562 `0x14015b020` | AchievementDialog (table) | 0 / 0 / 0 |
| `g_FollowDialog_Vtable` | engine_funcs.cpp:1493 `0x14015be00` | FollowDialog (table) | 0 / 1 / 0 |
| `g_GoalDialog_Vtable` | engine_funcs.cpp:1564 `0x14015beb0` | GoalDialog (table) | 0 / 0 / 0 |
| `g_ModDetailsDialog_Vtable` | engine_funcs.cpp:1710 `0x14015c670` | ModDetailsDialog (table) | 0 / 0 / 0 |
| `g_ModUpdateDialog_Vtable` | engine_funcs.cpp:1249 `0x14015c6c0` | ModUploadDialog (table) | 0 / 0 / 0 |
| `g_PauseDialog_Vtable` | engine_funcs.cpp:1330 `0x14015f550` | MainMenuDialog (table) | 0 / 0 / 0 |
| `g_ReloadDialog_Vtable` | engine_funcs.cpp:1742 `0x14015c480` | MessageDialog (table) | 0 / 0 / 0 |
| `g_ToolbarDialog_Vtable` | engine_funcs.cpp:1329 `0x14015fa00` | ToolbarDialog (table) | 0 / 0 / 0 |
| `g_TutorialInstanceVtable` | engine_funcs.cpp:2463 `0x140160bf8` | TutorialInstance (table) | 0 / 0 / 1 |

### C.3 `RegisterExeAddr` rows at a converted member's address (the seam doors) and their live readers

| row | address | member | registered function | live readers |
|---|---|---|---|---:|
| code/Game/Component/particlecomponent.cpp:1047 | 0x140052b30 | ParticleComponent.slot8 | `ParticleComponent_UpdateTick` | 1 |
| code/Game/Component/particlecomponent.cpp:1048 | 0x140053090 | ParticleComponent.slot12 | `ParticleComponent_Event` | 0 |
| standalone/reg_ambientenv_ui_serialize.cpp:21 | 0x1400437e0 | AmbientEnvironmentComponent.slot3 | `AmbientEnvironmentComponent_Serialize` | 0 |
| standalone/reg_ambientenv_ui_serialize.cpp:22 | 0x14012dd00 | UIComponent.slot3 | `UIComponent_Serialize` | 0 |
| standalone/reg_animal_serialize.cpp:55 | 0x140121f90 | HerdComponent.slot3 | `HerdComponent_Serialize` | 0 |
| standalone/reg_animal_serialize.cpp:56 | 0x140106e40 | AnimalComponent.slot3 | `AnimalComponent_Serialize` | 0 |
| standalone/reg_animal_serialize.cpp:57 | 0x140123e10 | LivestockComponent.slot3 | `LivestockComponent_Serialize` | 0 |
| standalone/reg_animal_serialize.cpp:58 | 0x1400544a0 | PopulationComponent.slot3 | `PopulationComponent_Serialize_Real` | 0 |
| standalone/reg_animal_serialize.cpp:59 | 0x140127f40 | PastureComponent.slot3 | `PastureComponent_Shutdown` | 0 |
| standalone/reg_animal_serialize.cpp:60 | 0x14012e300 | WildAnimalComponent.slot3 | `WildAnimalComponent_Serialize` | 0 |
| standalone/reg_animal_update.cpp:56 | 0x140106420 | AnimalComponent.slot9 | `AnimalComponent_Update` | 0 |
| standalone/reg_animal_update.cpp:57 | 0x140106d50 | AnimalComponent.slot4 | `AnimalComponent_Startup` | 0 |
| standalone/reg_animal_update.cpp:58 | 0x140106c20 | AnimalComponent.slot6 | `AnimalComponent_OnAdd` | 0 |
| standalone/reg_cemetery_healer_serialize.cpp:29 | 0x1400475c0 | CemeteryComponent.slot3 | `CemeteryComponent_Shutdown` | 0 |
| standalone/reg_cemetery_healer_serialize.cpp:30 | 0x14004aeb0 | HealerComponent.slot3 | `HealerComponent_Shutdown` | 0 |
| standalone/reg_citizen_employ_cascade.cpp:25 | 0x140114000 | CitizenComponent.slot6 | `CitizenComponent_OnAdd` | 0 |
| standalone/reg_citizen_needsupdate.cpp:35 | 0x140112260 | CitizenComponent.slot15 | `CitizenComponent_NeedsUpdate` | 0 |
| standalone/reg_citizen_onremove.cpp:46 | 0x140113ca0 | CitizenComponent.slot7 | `CitizenComponent_OnRemove` | 0 |
| standalone/reg_citizen_onremove.cpp:47 | 0x140108cb0 | BuildComponent.slot7 | `BuildComponent_OnCreate` | 0 |
| standalone/reg_citizenai_executor.cpp:34 | 0x1401094c0 | CharacterComponent.slot8 | `CharacterComponent_MoveTick_1094c0` | 0 |
| standalone/reg_code_lane2.cpp:70 | 0x14005f3d0 | ConsumeProduceUI.slot4 | `ConsumeProduceUI_Slot4` | 0 |
| standalone/reg_code_lane6.cpp:69 | 0x1400fc6b0 | BridgeTool.slot6, DragPlacementTool.slot6 | `DragTool_OnKey` | 0 |
| standalone/reg_code_lane6.cpp:86 | 0x14011b560 | DecalComponent.slot6 | `DecalComponent_Draw` | 1 |
| standalone/reg_code_lane6.cpp:96 | 0x14012bbd0 | StatusIconComponent.slot8 | `(next line)` | 1 |
| standalone/reg_component_cropdisease.cpp:78 | 0x140048b90 | DiseaseComponent.slot9 | `DiseaseComponent_Update` | 0 |
| standalone/reg_component_cropdisease.cpp:79 | 0x140048c50 | DiseaseComponent.slot4 | `DiseaseComponent_Startup` | 0 |
| standalone/reg_component_cropdisease.cpp:80 | 0x140048c00 | DiseaseComponent.slot6 | `DiseaseComponent_OnAdded` | 0 |
| standalone/reg_component_cropdisease.cpp:81 | 0x140048bb0 | DiseaseComponent.slot7 | `DiseaseComponent_OnRemoved` | 0 |
| standalone/reg_component_cropdisease.cpp:82 | 0x140048ca0 | DiseaseComponent.slot3 | `DiseaseComponent_Serialize` | 0 |
| standalone/reg_component_economy.cpp:97 | 0x14012b4a0 | ResourceProduceComponent.slot3 | `ResourceProduceComponent_BaseShutdown` | 0 |
| standalone/reg_component_economy.cpp:109 | 0x140108d40 | BuildComponent.slot6 | `BuildComponent_OnAddedThunk` | 1 |
| standalone/reg_component_economy.cpp:128 | 0x140043f10 | AttendanceComponent.slot13 | `AttendanceComponent_CommandMask` | 0 |
| standalone/reg_component_economy.cpp:129 | 0x140043e80 | AttendanceComponent.slot14 | `AttendanceComponent_Command` | 0 |
| standalone/reg_component_economy.cpp:134 | 0x140044410 | AttendanceComponent.slot3 | `AttendanceComponent_SerializeSlot` | 0 |
| standalone/reg_component_economy.cpp:135 | 0x1400443d0 | AttendanceComponent.slot7 | `AttendanceComponent_Unregister` | 0 |
| standalone/reg_component_economy.cpp:144 | 0x14011f920 | ForesterComponent.slot13 | `ForesterComponent_CommandMask` | 0 |
| standalone/reg_component_economy.cpp:145 | 0x14011f890 | ForesterComponent.slot14 | `ForesterComponent_Command` | 0 |
| standalone/reg_component_economy.cpp:149 | 0x14011f780 | ForesterComponent.slot18 | `ForesterComponent_CreateWork` | 0 |
| standalone/reg_component_economy.cpp:155 | 0x14004ea40 | MarketComponent.slot18 | `MarketComponent_CreateWork` | 0 |
| standalone/reg_component_economy.cpp:163 | 0x140118d00 | ConsumeProduceComponent.slot18 | `ConsumeProduceComponent_CreateWork` | 0 |
| standalone/reg_component_economy.cpp:168 | 0x140123110 | InteractComponent.slot6 | `InteractComponent_Draw` | 0 |
| standalone/reg_component_economy.cpp:175 | 0x14005c1d0 | TradeComponent.slot6 | `TradeComponent_Draw` | 0 |
| standalone/reg_component_economy.cpp:176 | 0x14005c190 | TradeComponent.slot7 | `TradeComponent_OnCreate` | 0 |
| standalone/reg_component_economy.cpp:177 | 0x14005c140 | TradeComponent.slot13 | `TradeComponent_Slot13` | 0 |
| standalone/reg_component_economy.cpp:178 | 0x14005c000 | TradeComponent.slot14 | `TradeComponent_Slot14` | 0 |
| standalone/reg_component_economy.cpp:190 | 0x1401189b0 | Component.slot3 | `Component_SerializeReimpl` | 50 |
| standalone/reg_component_nature.cpp:115 | 0x1401301f0 | ZoneComponent.slot6 | `ZoneComponent_Draw` | 0 |
| standalone/reg_component_nature.cpp:116 | 0x140130080 | ZoneComponent.slot7 | `ZoneComponent_OnCreate` | 0 |
| standalone/reg_fire.cpp:69 | 0x140049090 | FireComponent.slot15 | `FireComponent_WaterApplied_090` | 0 |
| standalone/reg_fruit_happiness_serialize.cpp:34 | 0x14004a070 | FruitComponent.slot3 | `FruitComponent_Serialize` | 0 |
| standalone/reg_fruit_happiness_serialize.cpp:35 | 0x14004aa80 | HappinessComponent.slot3 | `HappinessComponent_Shutdown` | 0 |
| standalone/reg_growth_spawn_serialize.cpp:36 | 0x1401207c0 | GrowthComponent.slot3 | `GrowthComponent_Serialize2` | 0 |
| standalone/reg_growth_spawn_serialize.cpp:37 | 0x140055ee0 | SpawnComponent.slot3 | `SpawnComponent_Serialize` | 0 |
| standalone/reg_herd_live.cpp:80 | 0x140121f40 | HerdComponent.slot6 | `Herd_OnAdd_Thunk` | 0 |
| standalone/reg_herd_live.cpp:81 | 0x140121e20 | HerdComponent.slot7 | `Herd_OnRemove_Thunk` | 0 |
| standalone/reg_herd_live.cpp:84 | 0x140054220 | PopulationComponent.slot4 | `Pop_Startup_Thunk` | 0 |
| standalone/reg_herd_live.cpp:90 | 0x1400541c0 | PopulationComponent.slot6 | `Pop_OnAdd_Thunk` | 0 |
| standalone/reg_herd_live.cpp:91 | 0x140054180 | PopulationComponent.slot7 | `Pop_OnRemove_Thunk` | 0 |
| standalone/reg_keystone_a_removal.cpp:41 | 0x1401035e0 | RemovalTool.slot13 | `RemovalCalc_Slot13` | 1 |
| standalone/reg_pasture_live.cpp:76 | 0x1401277e0 | PastureComponent.slot7 | `Pasture_ReleaseAllAnimals` | 0 |
| standalone/reg_pasture_live.cpp:79 | 0x140127720 | PastureComponent.slot15 | `Pasture_MsgReceive` | 1 |
| standalone/reg_pasture_live.cpp:82 | 0x140127dd0 | PastureComponent.slot13 | `Pasture_CanCommand` | 0 |
| standalone/reg_pasture_live.cpp:83 | 0x140127a60 | PastureComponent.slot14 | `Pasture_Command` | 0 |
| standalone/reg_pasture_live.cpp:92 | 0x140123c70 | LivestockComponent.slot6 | `LivestockComponent_OnAdd` | 0 |
| standalone/reg_pasture_live.cpp:93 | 0x140123c00 | LivestockComponent.slot7 | `LivestockComponent_OnRemove` | 0 |
| standalone/reg_player_bridge.cpp:27 | 0x1400f9cd0 | BridgeTool.slot13 | `BridgeTool_CalcPath_Impl` | 0 |
| standalone/reg_player_bridge.cpp:31 | 0x1400fadc0 | BridgeTool.slot8 | `BridgeTool_Draw_Impl` | 0 |
| standalone/reg_player_drag.cpp:54 | 0x1400fc640 | BridgeTool.slot9, DragPlacementTool.slot9 | `DragTool_CancelDrag_Impl` | 0 |
| standalone/reg_player_edit.cpp:57 | 0x1400dae00 | FencedTool.slot13 | `FencedTool_CalcPath_Port` | 0 |
| standalone/reg_quarry_live.cpp:50 | 0x1400550f0 | QuarryComponent.slot12 | `QuarryComponent_Slot12` | 0 |
| standalone/reg_quarry_live.cpp:51 | 0x140054b80 | QuarryComponent.slot15 | `QuarryComponent_Slot15` | 0 |
| standalone/reg_terraform_live.cpp:50 | 0x140057500 | TerraformComponent.slot3 | `(next line)` | 0 |
| standalone/reg_terraform_live.cpp:52 | 0x140057350 | TerraformComponent.slot7 | `(next line)` | 0 |
| standalone/reg_terraform_live.cpp:57 | 0x140057310 | TerraformComponent.slot12 | `TerraformComponent_Slot12` | 0 |
| standalone/reg_terraform_live.cpp:65 | 0x140125490 | MultiObjectComponent.slot12 | `MultiObjectComponent_Slot12` | 0 |
| standalone/reg_tornado.cpp:43 | 0x140058b80 | TornadoComponent.slot3 | `TornadoComponent_Shutdown` | 0 |
| standalone/reg_tornado.cpp:44 | 0x1400585a0 | TornadoComponent.slot4 | `TornadoComponent_Serialize` | 0 |
| standalone/reg_tornado.cpp:45 | 0x140057cd0 | TornadoComponent.slot8 | `TornadoComponent_OnRemove` | 0 |
| standalone/reg_town_services.cpp:90 | 0x140047200 | CemeteryComponent.slot6 | `CemeteryComponent_BuildPlotGrid` | 0 |
| standalone/reg_town_services.cpp:91 | 0x1400471c0 | CemeteryComponent.slot7 | `CemeteryComponent_OnRemove` | 0 |
| standalone/reg_town_services.cpp:96 | 0x14004ae70 | HealerComponent.slot7 | `HealerComponent_OnRemove` | 0 |
| standalone/reg_trade_serialize.cpp:22 | 0x14005bd50 | TradeComponent.slot3 | `TradeComponent_Serialize` | 0 |
| standalone/reg_work_base.cpp:49 | 0x1400f7400 | Work.slot3 | `WorkBase_Serialize_f7400` | 0 |
| standalone/reg_work_tail_serialize.cpp:55 | 0x1400f2370 | PastureWork.slot4 | `PastureWork_Slot4_Impl` | 1 |
| standalone/reg_work_tail_trade.cpp:72 | 0x1400f5d40 | TradeWork.slot4 | `TradeWork_Slot4_Impl` | 0 |

## Appendix D — (4) retail vtable addresses of converted classes: rows and readers

| class | retail table | `RegisterExeAddr` row(s) | by-address readers (live) | verdict |
|---|---|---|---|---|
| AchievementDialog | 0x14015b020 | code/Game/Dialog/menu_dialog_standalone_vtables.cpp:67 | engine/funcs/engine_funcs.cpp:1563 engine_funcs g_AchievementDialog_V (DEAD) | **reader-less: delete row** |
| Action | 0x14015faa0 | code/Game/Action/action.cpp:53 | code/Game/Component/citizenai_actionctors.cpp:145 ResolveAddr; code/Game/Component/citizenai_handlers.cpp:80 ResolveAddr; code/Game/Work/work_act_clear.cpp:139 ResolveAddr; code/Game/Work/work_act_hunt.cpp:126 ResolveAddr; code/Game/Work/work_act_path.cpp:154 ResolveAddr; code/Game/Work/work_act_perform.cpp:162 ResolveAddr; code/Game/Work/work_base_actionctor.cpp:161 ResolveAddr | KEEP until the readers convert |
| ClearArea | 0x140161110 | code/Game/Work/cleararea.cpp:1207 | code/Game/Component/clearareacomponent_states.cpp:82 ResolveAddr | KEEP until the readers convert |
| ClearStorage | 0x1401611a0 | code/Game/Work/clearstorage.cpp:672 | code/Game/Component/destroycomponent_states.cpp:157 ResolveAddr | KEEP until the readers convert |
| ConsumeProduce | 0x140161220 | code/Game/Work/consumeproduce.cpp:1295 | — | **reader-less: delete row** |
| ControlUI | 0x14015b380 | code/Game/Dialog/controlui.cpp:363 | — | **reader-less: delete row** |
| ControlUIConfig | 0x14015b3e0 | code/Game/Dialog/controluiconfig.cpp:113 | — | **reader-less: delete row** |
| ControllerComponent::Controller | 0x14015f070 | standalone/reg_w2_lane1.cpp:82 | standalone/reg_w2_lane1.cpp:60 other: "0x14015f070 dispatched)\n" (DEAD) | **reader-less: delete row** |
| CreateNaturalResource | 0x14015fca0 | code/Game/Action/createnaturalresource.cpp:389 | code/Game/Work/work_act_hunt.cpp:127 ResolveAddr | KEEP until the readers convert |
| FireWork | 0x1401612a0 | code/Game/Work/firework.cpp:698 | code/Game/Component/firecomponent_ignite.cpp:296 ResolveAddr | KEEP until the readers convert |
| FishingWork | 0x140161320 | code/Game/Work/fishingwork.cpp:1080 | — | **reader-less: delete row** |
| FollowDialog | 0x14015be00 | — (no row) | engine/funcs/engine_funcs.cpp:1494 engine_funcs g_FollowDialog_Vtable | reader reads 0 (D9 #15) |
| ForesterWork | 0x1401613a0 | code/Game/Work/foresterwork.cpp:1246 | — | **reader-less: delete row** |
| GoalDialog | 0x14015beb0 | code/Game/Dialog/menu_dialog_standalone_vtables.cpp:69 | engine/funcs/engine_funcs.cpp:1565 engine_funcs g_GoalDialog_Vtable (DEAD) | **reader-less: delete row** |
| HarvestArea | 0x1401614a0 | code/Game/Work/harvestarea.cpp:974 | — | **reader-less: delete row** |
| HerbalistWork | 0x1401615a0 | code/Game/Work/herbalistwork.cpp:987 | — | **reader-less: delete row** |
| HuntWork | 0x140161620 | code/Game/Work/huntwork.cpp:824 | — | **reader-less: delete row** |
| LocationWork | 0x1401616a0 | code/Game/Work/locationwork.cpp:518 | code/Game/Component/attendancecomponent_states.cpp:91 ResolveAddr | KEEP until the readers convert |
| MainMenuDialog | 0x14015f550 | code/Game/Dialog/mainmenudialog.cpp:1037 | engine/funcs/engine_funcs.cpp:1330 engine_funcs g_PauseDialog_Vtable (DEAD) | **reader-less: delete row** |
| MarketWork | 0x140161710 | code/Game/Work/marketwork.cpp:1879 | — | **reader-less: delete row** |
| MessageDialog | 0x14015c480 | code/Game/Dialog/messagedialog.cpp:130 | engine/funcs/engine_funcs.cpp:1742 engine_funcs g_ReloadDialog_Vtable (DEAD); code/Game/Dialog/saveloaddialog.cpp:922 via MessageDialogAddr::Vtable; code/Game/Dialog/saveloaddialog.cpp:1103 via MessageDialogAddr::Vtable | KEEP until the readers convert |
| ModDetailsDialog | 0x14015c670 | standalone/reg_mods.cpp:77 | engine/funcs/engine_funcs.cpp:1710 engine_funcs g_ModDetailsDialog_Vt (DEAD) | **reader-less: delete row** |
| ModDialog | 0x14015c870 | standalone/reg_mods.cpp:76 | — | **reader-less: delete row** |
| ModUploadDialog | 0x14015c6c0 | — (no row) | engine/funcs/engine_funcs.cpp:1249 engine_funcs g_ModUpdateDialog_Vta (DEAD) | dead reader only |
| PerformWork | 0x14015fea0 | code/Game/Action/performwork.cpp:459 | code/Game/Component/citizenai_actionctors.cpp:146 ResolveAddr; code/Game/Work/work_act_path.cpp:155 ResolveAddr; code/Game/Work/work_act_perform.cpp:163 ResolveAddr; code/Game/Work/work_base_actionctor.cpp:162 ResolveAddr | KEEP until the readers convert |
| Pickup | 0x140161810 | code/Game/Work/pickup.cpp:932 | code/Game/Component/rawmaterialcomponent.cpp:128 ResolveAddr | KEEP until the readers convert |
| PickupRawMaterial | 0x14015ff10 | code/Game/Action/pickuprawmaterial.cpp:282 | code/Game/Work/work_base_actionctor.cpp:164 ResolveAddr | KEEP until the readers convert |
| PlantArea | 0x140161890 | code/Game/Work/plantarea.cpp:923 | — | **reader-less: delete row** |
| PlayAnimation | 0x140158950 | code/Game/Action/playanimation.cpp:265 | code/Game/Component/citizenai_actionctors.cpp:147 ResolveAddr; code/Game/Component/citizenai_handlers.cpp:81 ResolveAddr; code/Game/Work/work_base_actionctor.cpp:163 ResolveAddr | KEEP until the readers convert |
| RemoveNaturalResource | 0x140160050 | code/Game/Action/removenaturalresource.cpp:343 | code/Game/Work/work_act_clear.cpp:140 ResolveAddr | KEEP until the readers convert |
| ResourceLimitUIConfig | 0x14015d250 | code/Game/Dialog/resourcelimituiconfig.cpp:112 | — | **reader-less: delete row** |
| ShowTool | 0x1401589b0 | code/Game/Action/showtool.cpp:438 | code/Game/Work/work_act_clear.cpp:141 ResolveAddr; code/Game/Work/work_act_hunt.cpp:128 ResolveAddr | KEEP until the readers convert |
| StatGoal | 0x1401604c0 | code/Game/Goal/statgoal.cpp:274 | standalone/main.cpp:952 other: RunFactorySelfTest("StatGoa (DEAD) | KEEP (standalone/main.cpp:952 RunFactorySelfTest resolves it at :778) |
| TendArea | 0x140161990 | code/Game/Work/tendarea.cpp:685 | — | **reader-less: delete row** |
| ToolbarDialog | 0x14015fa00 | — (no row) | engine/funcs/engine_funcs.cpp:1329 engine_funcs g_ToolbarDialog_Vtabl (DEAD) | dead reader only |
| TradeWork | 0x140161a10 | code/Game/Work/tradework.cpp:925 | — | **reader-less: delete row** |
| TutorialInstance | 0x140160bf8 | standalone/reg_w2_lane2.cpp:92 | engine/funcs/engine_funcs.cpp:2464 engine_funcs g_TutorialInstanceVta (DEAD); standalone/reg_w2_lane2.cpp:94 other: std::fprintf(stderr, "[w2_l (DEAD) | **reader-less: delete row** |
| UpgradeUIConfig | 0x14015df60 | code/Game/Dialog/upgradeuiconfig.cpp:100 | — | **reader-less: delete row** |
| WellDescription | 0x14015af20 | code/Game/Component/welldescription.cpp:82 | — | **reader-less: delete row** |
| Work | 0x140161a80 | code/Game/Work/work_base_standalone.cpp:63 | code/Game/Component/clearareacomponent_states.cpp:342 ResolveAddr; code/Game/Work/work_base_reimpl.cpp:160 ResolveAddr | KEEP until the readers convert |

## Appendix E — (5) by-name type-id lookups of converted (concrete) classes, per file

| file | lookups | classes |
|---|---:|---|
| code/Game/Action/action_base_move.cpp | 1 | CharacterComponent |
| code/Game/Action/action_base_movedeps.cpp | 1 | ModelDescription |
| code/Game/Action/action_base_rawmat.cpp | 1 | RawMaterialComponent |
| code/Game/Action/action_pf_ctors.cpp | 2 | CharacterDescription, CitizenComponent |
| code/Game/Action/action_pf_road.cpp | 1 | ModelComponent |
| code/Game/Action/action_pf_roots_route.cpp | 5 | MarketLocationComponent, RawMaterialDescription, StorageComponent, StorageLocationComponent, StorageLocationDescription |
| code/Game/Action/completetask.cpp | 2 | CitizenComponent, ComponentEntity |
| code/Game/Action/createnaturalresource.cpp | 1 | GrowthComponent |
| code/Game/Action/performwork.cpp | 1 | CitizenComponent |
| code/Game/Action/sendcomponentmessage.cpp | 1 | OrchardComponent |
| code/Game/Action/showtool.cpp | 3 | CitizenComponent, ModelDescription, RawMaterialDescription |
| code/Game/Component/absorbresourcedescription.cpp | 1 | AbsorbResourceComponent |
| code/Game/Component/ambientemittercomponent.cpp | 1 | AmbientEmitterDescription |
| code/Game/Component/ambientemitterdescription.cpp | 1 | AmbientEmitterComponent |
| code/Game/Component/ambientenvironmentcomponent.cpp | 3 | AmbientEmitterComponent, AmbientEmitterDescription, AmbientEnvironmentDescription |
| code/Game/Component/ambientenvironmentdescription.cpp | 1 | AmbientEnvironmentComponent |
| code/Game/Component/animaldescription.cpp | 1 | AnimalComponent |
| code/Game/Component/attendancedescription.cpp | 1 | AttendanceComponent |
| code/Game/Component/autopickupdescription.cpp | 1 | AutoPickupComponent |
| code/Game/Component/boatdescription.cpp | 1 | BoatComponent |
| code/Game/Component/buildcomponent.cpp | 4 | ModelComponent, ModelDescription, MultiObjectComponent, PickingComponent |
| code/Game/Component/buildcomponent_state1.cpp | 3 | BuildDescription, ModelComponent, RawMaterialDescription |
| code/Game/Component/buildcomponent_state8.cpp | 4 | BuildDescription, MapComponent, ModelDescription, StorageDescription |
| code/Game/Component/buildcomponent_sub8630.cpp | 1 | BuildDescription |
| code/Game/Component/builddescription.cpp | 1 | BuildComponent |
| code/Game/Component/cemeterydescription.cpp | 1 | CemeteryComponent |
| code/Game/Component/characterdescription.cpp | 1 | CharacterComponent |
| code/Game/Component/citizen_employ_cascade.cpp | 1 | ResidenceComponent |
| code/Game/Component/citizen_updatehealth.cpp | 2 | CitizenComponent, WorkPlaceComponent |
| code/Game/Component/citizenai_executor.cpp | 1 | PickingDescription |
| code/Game/Component/citizenai_findhelpers.cpp | 7 | InteractComponent, RawMaterialDescription, ResidenceComponent, ResidenceDescription, StorageComponent, StorageLocationComponent, StorageLocationDescription |
| code/Game/Component/citizenai_findtarget.cpp | 6 | MarketLocationComponent, RadiusDescription, RawMaterialDescription, StorageComponent, StorageLocationComponent, StorageLocationDescription |
| code/Game/Component/citizenai_gather.cpp | 2 | CitizenDescription, RawMaterialDescription |
| code/Game/Component/citizenai_growup.cpp | 4 | CitizenDescription, RawMaterialDescription, StorageComponent, WorkPlaceComponent |
| code/Game/Component/citizenai_handlers2.cpp | 2 | CitizenDescription, InteractDescription |
| code/Game/Component/citizenai_handlers3.cpp | 2 | CitizenDescription, InteractDescription |
| code/Game/Component/citizenai_leisure.cpp | 5 | CitizenDescription, HappinessComponent, HappinessDescription, InteractComponent, MapComponent |
| code/Game/Component/citizenaicomponent.cpp | 4 | RawMaterialDescription, ResidenceComponent, StorageComponent |
| code/Game/Component/citizenaidescription.cpp | 1 | CitizenAIComponent |
| code/Game/Component/citizencomponent.cpp | 1 | StorageComponent |
| code/Game/Component/citizendescription.cpp | 1 | CitizenComponent |
| code/Game/Component/clearareacomponent.cpp | 1 | ClearAreaComponent |
| code/Game/Component/clearareadescription.cpp | 1 | ClearAreaComponent |
| code/Game/Component/clearresourcedescription.cpp | 1 | ClearResourceComponent |
| code/Game/Component/consumeproducedescription.cpp | 1 | ConsumeProduceComponent |
| code/Game/Component/cropfielddescription.cpp | 1 | CropFieldComponent |
| code/Game/Component/decalcomponent.cpp | 2 | DecalDescription, ModelComponent |
| code/Game/Component/decaldescription.cpp | 1 | DecalComponent |
| code/Game/Component/destroycomponent_state6.cpp | 3 | DecalComponent, DestroyDescription, ModelDescription |
| code/Game/Component/destroycomponent_states.cpp | 1 | RawMaterialDescription |
| code/Game/Component/destroydescription.cpp | 1 | DestroyComponent |
| code/Game/Component/diseasecomponent.cpp | 1 | DiseaseComponent |
| code/Game/Component/diseasedescription.cpp | 1 | DiseaseComponent |
| code/Game/Component/entity_finders_disaster.cpp | 3 | AmbientEmitterComponent, CitizenComponent, FireComponent |
| code/Game/Component/firecomponent_ignite.cpp | 3 | AmbientEmitterDescription, FireComponent, FireDescription |
| code/Game/Component/firedescription.cpp | 1 | FireComponent |
| code/Game/Component/fishingdescription.cpp | 1 | FishingComponent |
| code/Game/Component/floorcomponent.cpp | 4 | FloorDescription, MapComponent, ModelComponent |
| code/Game/Component/floordescription.cpp | 1 | FloorComponent |
| code/Game/Component/foresterdescription.cpp | 1 | ForesterComponent |
| code/Game/Component/fruitdescription.cpp | 1 | FruitComponent |
| code/Game/Component/growthcomponent.cpp | 1 | ModelComponent |
| code/Game/Component/growthdescription.cpp | 1 | GrowthComponent |
| code/Game/Component/happinessdescription.cpp | 1 | HappinessComponent |
| code/Game/Component/healerdescription.cpp | 1 | HealerComponent |
| code/Game/Component/herbalistdescription.cpp | 1 | HerbalistComponent |
| code/Game/Component/herddescription.cpp | 1 | HerdComponent |
| code/Game/Component/highlightdescription.cpp | 1 | HighlightComponent |
| code/Game/Component/huntdescription.cpp | 1 | HuntComponent |
| code/Game/Component/immigrationcomponent_states.cpp | 6 | CitizenDescription, ImmigrationDescription, InteractDescription, NomadComponent, NomadDescription, ResidenceComponent |
| code/Game/Component/immigrationdescription.cpp | 1 | ImmigrationComponent |
| code/Game/Component/interactdescription.cpp | 1 | InteractComponent |
| code/Game/Component/livestockdescription.cpp | 1 | LivestockComponent |
| code/Game/Component/mapdescription.cpp | 1 | MapComponent |
| code/Game/Component/marketdescription.cpp | 1 | MarketComponent |
| code/Game/Component/marketlocationdescription.cpp | 1 | MarketLocationComponent |
| code/Game/Component/minedescription.cpp | 1 | MineComponent |
| code/Game/Component/modeldescription.cpp | 1 | ModelComponent |
| code/Game/Component/multiobjectdescription.cpp | 1 | MultiObjectComponent |
| code/Game/Component/naturalresourcedescription.cpp | 1 | NaturalResourceComponent |
| code/Game/Component/nomadcomponent.cpp | 1 | EntityUI |
| code/Game/Component/nomadcomponent_states.cpp | 1 | CharacterComponent |
| code/Game/Component/nomaddescription.cpp | 1 | NomadComponent |
| code/Game/Component/orchardcomponent_states.cpp | 2 | ClearAreaDescription, OrchardDescription |
| code/Game/Component/orcharddescription.cpp | 1 | OrchardComponent |
| code/Game/Component/particlecomponent.cpp | 1 | ParticleDescription |
| code/Game/Component/particledescription.cpp | 1 | ParticleComponent |
| code/Game/Component/pasturedescription.cpp | 1 | PastureComponent |
| code/Game/Component/pickingcomponent.cpp | 3 | MapComponent, ModelComponent, PickingComponent |
| code/Game/Component/pickingdescription.cpp | 1 | PickingComponent |
| code/Game/Component/populationcomponent.cpp | 3 | HerdComponent, HerdDescription, PopulationDescription |
| code/Game/Component/populationdescription.cpp | 1 | PopulationComponent |
| code/Game/Component/quarrycomponent_deform.cpp | 3 | ConsumeProduceDescription, QuarryDescription |
| code/Game/Component/quarrydescription.cpp | 1 | QuarryComponent |
| code/Game/Component/rawmaterialdescription.cpp | 1 | RawMaterialComponent |
| code/Game/Component/residencedescription.cpp | 1 | ResidenceComponent |
| code/Game/Component/spawndescription.cpp | 1 | SpawnComponent |
| code/Game/Component/statemachinecomponent.cpp | 1 | DestroyComponent |
| code/Game/Component/statemachinedescription.cpp | 1 | StateMachineComponent |
| code/Game/Component/statusicondescription.cpp | 1 | StatusIconComponent |
| code/Game/Component/storagecomponent.cpp | 2 | RawMaterialDescription, StorageDescription |
| code/Game/Component/storagedescription.cpp | 1 | StorageComponent |
| code/Game/Component/storagedisplaycomponent.cpp | 2 | ModelDescription, RawMaterialDescription |
| code/Game/Component/storagedisplaydescription.cpp | 1 | StorageDisplayComponent |
| code/Game/Component/storagelocationdescription.cpp | 1 | StorageLocationComponent |
| code/Game/Component/terraformdescription.cpp | 1 | TerraformComponent |
| code/Game/Component/tombstonedescription.cpp | 1 | TombstoneComponent |
| code/Game/Component/tornadocomponent.cpp | 2 | ParticleDescription, TornadoDescription |
| code/Game/Component/tornadodescription.cpp | 1 | TornadoComponent |
| code/Game/Component/trackerdescription.cpp | 1 | TrackerComponent |
| code/Game/Component/tradedescription.cpp | 1 | TradeComponent |
| code/Game/Component/uidescription.cpp | 1 | UIComponent |
| code/Game/Component/wellcomponent.cpp | 1 | MapComponent |
| code/Game/Component/wildanimaldescription.cpp | 1 | WildAnimalComponent |
| code/Game/Component/workdescription.cpp | 1 | WorkComponent |
| code/Game/Component/workplacedescription.cpp | 1 | WorkPlaceComponent |
| code/Game/Component/zonedescription.cpp | 1 | ZoneComponent |
| code/Game/Dialog/citizenui.cpp | 5 | CitizenAIComponent, CitizenComponent, CitizenDescription, ComponentEntity, EditTool |
| code/Game/Dialog/debugui.cpp | 1 | CitizenComponent |
| code/Game/Dialog/destroyui.cpp | 2 | ComponentEntity, DestroyDescription |
| code/Game/Dialog/dialog_exe_helpers.cpp | 1 | BuildComponent |
| code/Game/Dialog/edittool_select.cpp | 4 | EntityUI, ToolUI, UIComponent, UIDescription |
| code/Game/Dialog/entityui.cpp | 6 | ComponentEntity, MapComponent, ModelComponent, MultiObjectComponent, ToolUI, UIDescription |
| code/Game/Dialog/followdialog.cpp | 1 | ModelComponent |
| code/Game/Dialog/residenceui.cpp | 2 | ComponentEntity, ResidenceComponent |
| code/Game/Dialog/selectresourceui.cpp | 2 | ComponentEntity, SelectDescription |
| code/Game/Dialog/statusbarui.cpp | 2 | CitizenDescription, StatusIconComponent |
| code/Game/Dialog/statusiconui.cpp | 4 | ComponentEntity, EntityUI, StatusIconComponent, StatusIconUIConfig |
| code/Game/Dialog/storagecapacityui.cpp | 1 | ComponentEntity |
| code/Game/Dialog/storageui.cpp | 2 | ComponentEntity, RawMaterialDescription |
| code/Game/Dialog/toolbar_utility_helpers.cpp | 1 | ToolUI |
| code/Game/Dialog/toolbar_vtable_standalone.cpp | 1 | ToolbarDescription |
| code/Game/Dialog/toolbardialog.cpp | 1 | UIDescription |
| code/Game/Dialog/toolui.cpp | 1 | EntityUI |
| code/Game/Dialog/ui_dialog_stub_factories.cpp | 23 | AttendanceComponent, AttendanceDescription, BuildComponent, BuildDescription, CemeteryComponent, ClearAreaComponent, ComponentDescription, ComponentEntity, ConsumeProduceDescription, ImmigrationCompon |
| code/Game/Dialog/upgradeui.cpp | 2 | ComponentEntity, UpgradeUIConfig |
| code/Game/Dialog/workplaceui.cpp | 2 | ComponentEntity, WorkPlaceComponent |
| L code/Game/Entity/entitylist.cpp | 3 | EditTool, EntityUI, ModelComponent |
| L code/Game/Game/construction_force_harness.cpp | 1 | BuildComponent |
| L code/Game/Game/force_work.cpp | 1 | MapComponent |
| L code/Game/Game/forester_force_harness.cpp | 2 | ForesterComponent, ForesterDescription |
| L code/Game/Game/founding_spawn.cpp | 2 | CitizenComponent, CitizenDescription |
| L code/Game/Game/gameupdateloop_serialize.cpp | 2 | ComponentEntity |
| L code/Game/Game/serialize_externals.cpp | 1 | EnvironmentDescription |
| L code/Game/Game/starttown.cpp | 48 | AmbientEnvironmentComponent, BuildComponent, CitizenComponent, CreatePlacedDescription, FishingDescription, GrowthComponent, HerbalistDescription, InteractDescription, MapComponent, ModelComponent, Mo |
| L code/Game/Game/town_storage_seed.cpp | 4 | MapComponent, RawMaterialDescription, StorageComponent, StorageDescription |
| L code/Game/Game/town_workplace_registry.cpp | 2 | WorkComponent, WorkPlaceComponent |
| L code/Game/Game/unreg_lane5_musicplayer.cpp | 1 | CitizenDescription |
| code/Game/Player/bridgetool_calcpath.cpp | 4 | MapComponent, ModelComponent, MultiObjectComponent, PickingComponent |
| code/Game/Player/edittool.cpp | 3 | EntityUI, ModelComponent, ToolUI |
| code/Game/Player/pathtool_setselected.cpp | 1 | HighlightComponent |
| code/Game/Player/placementtool.cpp | 5 | CreatePlacedDescription, MapDescription, ModelDescription, RadiusDescription, RotationSetting |
| code/Game/Player/roadtool.cpp | 2 | CreateRoadDescription, DecalDescription |
| code/Game/Player/roadtool_calcpath.cpp | 2 | DecalDescription, MapDescription |
| code/Game/Player/tool_activation_standalone.cpp | 16 | BridgeTool, CreateBridgeDescription, CreateDragDescription, CreateFencedDescription, CreatePathDescription, CreatePlacedDescription, CreateRemovalDescription, CreateRoadDescription, DragPlacementTool, |
| code/Game/Player/tool_base_createbar.cpp | 2 | BuildRequirementUI, ToolbarDescription |
| code/Game/Player/tool_base_drag.cpp | 1 | ToolbarDescription |
| code/Game/Player/tool_base_render.cpp | 4 | HighlightComponent, HighlightDescription, ModelComponent |
| code/Game/Player/tool_base_tile.cpp | 1 | ModelComponent |
| code/Game/Tutorial/tutorial_base.cpp | 1 | WorkDescription |
| code/Game/Work/build_actions.cpp | 2 | BuildComponent, DestroyComponent |
| code/Game/Work/gatherrawmaterials.cpp | 3 | BuildDescription, MapDescription, RawMaterialDescription |
| code/Game/Work/gatherrawmaterials_ctor.cpp | 1 | BuildDescription |
| code/Game/Work/marketwork.cpp | 9 | ConsumeProduceComponent, InteractComponent, InteractDescription, MapComponent, MarketComponent, MarketDescription, RawMaterialDescription, StorageComponent, StorageLocationComponent |
| code/Game/Work/pickup.cpp | 2 | Pickup, RawMaterialDescription |
| code/Game/Work/pickup_gate.cpp | 4 | RawMaterialDescription, StorageComponent, StorageLocationComponent, StorageLocationDescription |
| code/Game/Work/work_base_reimpl.cpp | 6 | CitizenComponent, CompleteTask, InteractComponent, StorageComponent, TradeComponent, WorkComponent |
| engine/funcs/engine_funcs.h | 1 | EntityUI |
| standalone/cropfield_force_start.cpp | 3 | CropFieldComponent, NaturalResourceDescription, StateMachineComponent |
| standalone/finish_builds_harness.cpp | 2 | BuildComponent, StateMachineComponent |
| standalone/food_panel_harness.cpp | 9 | AttendanceComponent, RadiusUI, ToolUI, WorkComponent, WorkPlaceComponent, WorkPlaceDescription, WorkPlaceUI |
| standalone/food_placement_harness.cpp | 4 | MapComponent, StateMachineComponent, WorkPlaceComponent |
| standalone/food_production_diag.cpp | 13 | CropFieldComponent, FishingComponent, HerbalistComponent, HerbalistDescription, HuntComponent, MapComponent, OrchardComponent, PastureComponent, RadiusDescription, StateMachineComponent, StorageCompon |
| standalone/market_overstock_harness.cpp | 4 | MarketComponent, MarketDescription, RawMaterialDescription, StorageComponent |
| standalone/orchard_cut_harness.cpp | 1 | OrchardComponent |
| standalone/orchard_diag.cpp | 4 | OrchardComponent, StateMachineComponent, StorageComponent, WorkPlaceComponent |
| standalone/orchard_force_start.cpp | 2 | OrchardComponent, OrchardUI |
| standalone/orchard_growth_harness.cpp | 1 | OrchardComponent |
| standalone/orchard_workers_harness.cpp | 1 | OrchardComponent |
| standalone/panel_staffing_driver.cpp | 4 | StatusIconComponent, WorkComponent, WorkPlaceComponent, WorkPlaceUI |
| standalone/reg_action_child_ctors2.cpp | 4 | ComponentEntity, ConsumeProduceComponent, ConsumeProduceDescription, RawMaterialDescription |
| standalone/reg_animal_serialize.cpp | 1 | PopulationComponent |
| standalone/reg_animal_update.cpp | 2 | CharacterComponent, CharacterDescription |
| standalone/reg_autosave.cpp | 2 | AutoSaveDialog, Scenario |
| standalone/reg_citizen_lifeevent_handler.cpp | 2 | CemeteryDescription, TombstoneComponent |
| standalone/reg_citizenai_typeids.cpp | 3 | CitizenAIComponent, CitizenComponent, HappinessComponent |
| standalone/reg_cleararea_typeids.cpp | 2 | ClearAreaComponent, ClearAreaDescription |
| standalone/reg_component_animal.cpp | 6 | AnimalComponent, HerdComponent, HerdDescription, LivestockComponent, LivestockDescription, WildAnimalComponent |
| standalone/reg_component_cropdisease.cpp | 2 | DiseaseComponent, DiseaseDescription |
| standalone/reg_component_fruit.cpp | 2 | FruitComponent, FruitDescription |
| standalone/reg_component_nature.cpp | 1 | NaturalResourceComponent |
| standalone/reg_component_spawn.cpp | 2 | GrowthDescription, SpawnDescription |
| standalone/reg_component_tombstone.cpp | 1 | TombstoneDescription |
| standalone/reg_controlui.cpp | 1 | ControlUIConfig |
| standalone/reg_economy_typeids.cpp | 29 | AttendanceComponent, AttendanceDescription, BuildRoad, CitizenDescription, CreateBridgeDescription, CreateDragDescription, CreateRemovalDescription, DestroyComponent, DestroyDescription, ForesterCompo |
| standalone/reg_growth_typeid.cpp | 1 | GrowthComponent |
| standalone/reg_market_typeid.cpp | 3 | MarketComponent, MarketDescription, StorageLocationComponent |
| standalone/reg_orchard_typeid.cpp | 2 | OrchardComponent, OrchardDescription |
| standalone/reg_pipeline_typeids.cpp | 8 | CreateFencedDescription, CreatePlacedDescription, DecalComponent, MapDescription, ModelComponent, ModelDescription, RotationSetting, SelectDescription |
| standalone/reg_tornado.cpp | 1 | ParticleComponent |
| standalone/reg_town_services.cpp | 2 | CemeteryComponent, HappinessDescription |
| standalone/reg_tracker_typeid.cpp | 1 | TrackerComponent |
| standalone/reg_tradeui_typeids.cpp | 5 | BoatComponent, ComponentDescription, TradeComponent, TradeDescription, ValueDescription |
| standalone/reg_w2_lane2.cpp | 1 | TutorialInstance |
| standalone/reg_w2_lane3.cpp | 1 | ParticleDescription |
| standalone/reg_work_tail_fire.cpp | 1 | InteractComponent |
| standalone/reg_work_tail_serialize.cpp | 3 | AnimalDescription, PastureComponent, PastureDescription |
| standalone/reg_work_tail_trade.cpp | 1 | CitizenComponent |
| standalone/stress_harness.cpp | 2 | CitizenAIComponent, CitizenComponent |
| standalone/tool_harness.cpp | 13 | MapComponent, OrchardComponent, OrchardUI, PastureComponent, PastureUI, PickingComponent, TradeComponent, TradeUI, UIComponent |
| standalone/trade_buy_harness.cpp | 3 | StorageComponent, TradeComponent, TradeUI |
| tests/unit/object_typeinfo_test.cpp | 1 | CitizenComponent |

Abstract bases (no `TypeId()`, D4 keeps the by-name answer): 17 lookups — code/Game/Action/action.cpp:23 Action, code/Game/Component/component.cpp:42 Component, code/Game/Component/component.cpp:113 ControllerComponent, code/Game/Component/description.cpp:22 Description, code/Game/Component/resourceproducecomponent_states.cpp:393 ResourceProduceComponent, code/Game/Dialog/selectresourceui.cpp:308 SelectResourceUI, code/Game/Game/goals_serialize.cpp:115 Work, code/Game/Player/dragtool.cpp:43 DragTool, code/Game/Player/rotationsetting.cpp:30 ToolSetting, code/Game/Player/tool.cpp:19 Tool, code/Game/Player/tool_activation_standalone.cpp:194 Tool, code/Game/Player/tool_activation_standalone.cpp:195 ToolSetting, code/Game/Tutorial/tutorialgoal.cpp:29 TutorialGoal, code/Game/Tutorial/tutorialsetup.cpp:29 TutorialSetup, code/Game/Work/autopickupwork.cpp:35 AutoPickupWork, code/Game/Work/work_base_standalone.cpp:49 Work, standalone/reg_animal_serialize.cpp:50 Component.


## Appendix F — (6) fold-in sizing per class (members whose body is one call to a free function holding the body)

| block | class | member defs | delegating | delegate bodies | body lines |
|---|---|---:|---:|---:|---:|
| ACT | AnimateNaturalResource | 10 | 4 | 4 | 63 |
| ACT | CompleteRoad | 7 | 2 | 2 | 38 |
| ACT | CompleteTask | 9 | 4 | 4 | 113 |
| ACT | ConsumeRawMaterial | 11 | 5 | 5 | 269 |
| ACT | CreateNaturalResource | 8 | 2 | 2 | 170 |
| ACT | CreateRawMaterial | 8 | 2 | 2 | 272 |
| ACT | DropOffRawMaterial | 9 | 4 | 4 | 183 |
| ACT | HuntAnimal | 10 | 5 | 5 | 249 |
| ACT | MoveTo | 12 | 7 | 7 | 439 |
| ACT | PerformWork | 11 | 6 | 6 | 146 |
| ACT | PickupRawMaterial | 7 | 3 | 3 | 107 |
| ACT | PickupStoredRawMaterial | 10 | 4 | 4 | 234 |
| ACT | PlayAnimation | 9 | 4 | 4 | 55 |
| ACT | RemoveAnimal | 9 | 4 | 4 | 99 |
| ACT | RemoveNaturalResource | 9 | 4 | 4 | 135 |
| ACT | SendComponentMessage | 8 | 3 | 3 | 179 |
| ACT | ShowTool | 10 | 4 | 4 | 72 |
| CIT | AmbientEmitterComponent | 7 | 2 | 2 | 80 |
| CIT | AmbientEnvironmentComponent | 6 | 2 | 2 | 137 |
| CIT | AnimalComponent | 9 | 4 | 4 | 403 |
| CIT | CharacterComponent | 10 | 5 | 5 | 189 |
| CIT | CitizenAIComponent | 9 | 3 | 3 | 156 |
| CIT | CitizenComponent | 11 | 7 | 7 | 428 |
| CIT | DiseaseComponent | 9 | 5 | 5 | 70 |
| CIT | FireComponent | 7 | 3 | 3 | 93 |
| CIT | FruitComponent | 7 | 3 | 3 | 133 |
| CIT | GrowthComponent | 11 | 4 | 4 | 360 |
| CIT | HerdComponent | 8 | 3 | 3 | 171 |
| CIT | LivestockComponent | 8 | 4 | 4 | 106 |
| CIT | NaturalResourceComponent | 6 | 1 | 1 | 22 |
| CIT | PopulationComponent | 10 | 5 | 5 | 231 |
| CIT | SpawnComponent | 7 | 3 | 3 | 176 |
| CIT | TombstoneComponent | 7 | 3 | 3 | 85 |
| CIT | TornadoComponent | 7 | 3 | 3 | 285 |
| CIT | WildAnimalComponent | 6 | 2 | 2 | 63 |
| DLG | ToolbarDialog | 10 | 1 | 1 | 17 |
| MI-A | AbsorbResourceComponent | 5 | 1 | 1 | 20 |
| MI-A | AttendanceComponent | 10 | 5 | 5 | 114 |
| MI-A | BoatComponent | 6 | 1 | 1 | 89 |
| MI-A | BuildComponent | 9 | 3 | 3 | 129 |
| MI-A | CemeteryComponent | 8 | 2 | 2 | 95 |
| MI-A | ClearAreaComponent | 5 | 1 | 1 | 16 |
| MI-A | ClearResourceComponent | 5 | 1 | 1 | 19 |
| MI-A | DestroyComponent | 7 | 2 | 2 | 46 |
| MI-A | HappinessComponent | 7 | 2 | 2 | 56 |
| MI-A | HealerComponent | 6 | 2 | 2 | 32 |
| MI-A | ImmigrationComponent | 6 | 1 | 1 | 67 |
| MI-A | NomadComponent | 6 | 1 | 1 | 44 |
| MI-A | ResidenceComponent | 8 | 2 | 2 | 63 |
| MI-B | ConsumeProduceComponent | 8 | 3 | 3 | 174 |
| MI-B | CropFieldComponent | 10 | 4 | 4 | 192 |
| MI-B | FishingComponent | 9 | 5 | 5 | 114 |
| MI-B | ForesterComponent | 8 | 2 | 2 | 41 |
| MI-B | HerbalistComponent | 6 | 2 | 2 | 23 |
| MI-B | HuntComponent | 7 | 2 | 2 | 60 |
| MI-B | MarketComponent | 6 | 2 | 2 | 23 |
| MI-B | MarketLocationComponent | 6 | 2 | 2 | 33 |
| MI-B | MultiObjectComponent | 7 | 2 | 2 | 95 |
| MI-B | OrchardComponent | 10 | 4 | 4 | 238 |
| MI-B | PastureComponent | 13 | 7 | 7 | 277 |
| MI-B | StorageLocationComponent | 5 | 1 | 1 | 15 |
| MI-B | TradeComponent | 10 | 5 | 5 | 201 |
| MI-B | WorkPlaceComponent | 7 | 2 | 2 | 130 |
| PILOT | WellComponent | 6 | 1 | 1 | 22 |
| PNL | BuildUI | 6 | 3 | 3 | 21 |
| PNL | CitizenUI | 8 | 5 | 5 | 30 |
| PNL | ControlUI | 6 | 3 | 3 | 48 |
| PNL | CropUI | 9 | 6 | 6 | 93 |
| PNL | DebugUI | 7 | 4 | 4 | 33 |
| PNL | DestroyUI | 7 | 4 | 4 | 28 |
| PNL | EntityUI | 8 | 5 | 5 | 35 |
| PNL | EventLogUI | 6 | 3 | 3 | 11 |
| PNL | HelpUI | 6 | 3 | 3 | 21 |
| PNL | ImmigrationUI | 6 | 3 | 3 | 21 |
| PNL | MapUI | 5 | 2 | 2 | 17 |
| PNL | OrchardUI | 10 | 7 | 7 | 88 |
| PNL | PageToggleUI | 6 | 3 | 3 | 21 |
| PNL | PastureUI | 10 | 7 | 7 | 96 |
| PNL | ProductionUI | 5 | 2 | 2 | 16 |
| PNL | ProfessionUI | 6 | 3 | 3 | 24 |
| PNL | RadiusUI | 6 | 2 | 2 | 11 |
| PNL | ResidenceUI | 8 | 4 | 4 | 23 |
| PNL | ResourceLimitUI | 6 | 3 | 3 | 21 |
| PNL | SelectResourceUI | 7 | 4 | 4 | 78 |
| PNL | StatusBarUI | 9 | 5 | 4 | 40 |
| PNL | StatusIconUI | 6 | 3 | 3 | 13 |
| PNL | StorageCapacityUI | 6 | 3 | 3 | 20 |
| PNL | StorageUI | 7 | 3 | 3 | 21 |
| PNL | TitleDescToolTipUI | 4 | 1 | 1 | 47 |
| PNL | ToolUI | 6 | 3 | 3 | 22 |
| PNL | TownHallUI | 9 | 5 | 5 | 39 |
| PNL | TradeUI | 10 | 7 | 7 | 48 |
| PNL | UpgradeUI | 6 | 3 | 3 | 19 |
| PNL | WorkPlaceUI | 7 | 4 | 4 | 117 |
| TOOL | BridgeTool | 7 | 3 | 3 | 247 |
| TOOL | DragPlacementTool | 7 | 4 | 4 | 398 |
| TOOL | DragTool | 5 | 2 | 2 | 231 |
| TOOL | EditTool | 7 | 4 | 4 | 502 |
| TOOL | FencedTool | 12 | 7 | 7 | 591 |
| TOOL | PathTool | 8 | 4 | 4 | 155 |
| TOOL | PlacementTool | 9 | 5 | 5 | 644 |
| TOOL | RemovalTool | 7 | 4 | 4 | 307 |
| TOOL | RoadTool | 8 | 5 | 5 | 359 |
| VIS | AutoPickupComponent | 8 | 1 | 1 | 37 |
| VIS | DecalComponent | 11 | 4 | 4 | 151 |
| VIS | FloorComponent | 9 | 3 | 3 | 75 |
| VIS | FrustumOutlineElement | 7 | 3 | 3 | 78 |
| VIS | HighlightComponent | 8 | 1 | 1 | 28 |
| VIS | InteractComponent | 6 | 2 | 2 | 122 |
| VIS | MapComponent | 8 | 1 | 1 | 24 |
| VIS | MineComponent | 7 | 1 | 1 | 13 |
| VIS | ModelComponent | 10 | 5 | 5 | 227 |
| VIS | PickingComponent | 9 | 2 | 2 | 136 |
| VIS | QuarryComponent | 9 | 2 | 2 | 142 |
| VIS | RawMaterialComponent | 8 | 3 | 3 | 126 |
| VIS | StateMachineComponent | 10 | 5 | 5 | 79 |
| VIS | StatusIconComponent | 9 | 3 | 3 | 159 |
| VIS | StorageComponent | 7 | 2 | 2 | 139 |
| VIS | StorageDisplayComponent | 10 | 4 | 4 | 160 |
| VIS | TerraformComponent | 8 | 1 | 1 | 8 |
| VIS | TrackerComponent | 5 | 1 | 1 | 60 |
| VIS | UIComponent | 6 | 1 | 1 | 18 |
| VIS | WorkComponent | 9 | 2 | 2 | 55 |
| VIS | ZoneComponent | 7 | 3 | 3 | 125 |
| helper | StatusIconGeometry | 5 | 2 | 2 | 164 |

## Appendix G — dead code, verified on the tree

### G.1 Empty `Register*Vtable()` stubs (body empty after comments) and their only calls

| stub | call |
|---|---|
| code/Game/Action/animatenaturalresource.cpp:226 `RegisterAnimateNaturalResourceVtable` | standalone/address_registry.cpp:1744 |
| code/Game/Action/completeroad.cpp:154 `RegisterCompleteRoadVtable` | standalone/address_registry.cpp:1745 |
| code/Game/Action/completetask.cpp:293 `RegisterCompleteTaskVtable` | standalone/address_registry.cpp:1746 |
| code/Game/Action/consumerawmaterial.cpp:387 `RegisterConsumeRawMaterialVtable` | standalone/address_registry.cpp:1747 |
| code/Game/Action/createrawmaterial.cpp:585 `RegisterCreateRawMaterialVtable` | standalone/address_registry.cpp:1749 |
| code/Game/Action/dropoffrawmaterial.cpp:392 `RegisterDropOffRawMaterialVtable` | standalone/address_registry.cpp:1750 |
| code/Game/Action/huntanimal.cpp:533 `RegisterHuntAnimalVtable` | standalone/address_registry.cpp:1751 |
| code/Game/Action/moveto.cpp:814 `RegisterMoveToVtable` | standalone/address_registry.cpp:1752 |
| code/Game/Action/pickupstoredrawmaterial.cpp:475 `RegisterPickupStoredRawMaterialVtable` | standalone/address_registry.cpp:1755 |
| code/Game/Action/removeanimal.cpp:308 `RegisterRemoveAnimalVtable` | standalone/address_registry.cpp:1756 |
| code/Game/Action/sendcomponentmessage.cpp:314 `RegisterSendComponentMessageVtable` | standalone/address_registry.cpp:1758 |
| code/Game/Component/animaudioevent.cpp:92 `RegisterAnimAudioEventVtable` | standalone/address_registry.cpp:1717 |
| code/Game/Component/captureboxdescription.cpp:115 `RegisterCaptureBoxDescriptionVtable` | standalone/address_registry.cpp:1709 |

### G.2 `struct X;` forward declarations of converted classes (should be `class X;` or go)

| site | class |
|---|---|
| code/Game/Component/citizen_employ_cascade.cpp:38 | CitizenComponent |
| code/Game/Component/citizencomponent.cpp:113 | StatusIconComponent |
| code/Game/Component/clearareacomponent_states.cpp:51 | ClearAreaComponent |
| code/Game/Dialog/townhall_helpers.cpp:38 | CitizenComponent |
| code/Game/Game/starttown.cpp:327 | CitizenComponent |
| engine/funcs/engine_funcs.h:732 | SaveLoadDialog |
| engine/funcs/engine_funcs.h:1672 | TownHallUI |
| standalone/address_registry.cpp:240 | ComponentEntity |
| standalone/orchard_force_start.cpp:39 | EditTool |
| standalone/reg_animal_serialize.cpp:27 | PastureComponent |
| standalone/reg_animal_serialize.cpp:28 | WildAnimalComponent |
| standalone/reg_citizen_onremove.cpp:41 | BuildComponent |
| standalone/reg_code_lane2.cpp:26 | ConsumeProduceUI |
| standalone/reg_component_cropdisease.cpp:38 | DiseaseComponent |
| standalone/reg_component_economy.cpp:58 | InteractComponent |
| standalone/reg_component_economy.cpp:62 | TradeComponent |
| standalone/reg_component_gap_A.cpp:36 | CitizenComponent |
| standalone/reg_component_nature.cpp:29 | ZoneComponent |
| standalone/reg_fruit_happiness_serialize.cpp:30 | HappinessComponent |
| standalone/reg_mods.cpp:68 | MainMenuDialog |
| standalone/reg_pasture_live.cpp:36 | PastureComponent |
| standalone/reg_pasture_live.cpp:37 | LivestockComponent |
| standalone/reg_town_services.cpp:32 | CemeteryComponent |
| standalone/reg_town_services.cpp:39 | HealerComponent |
| standalone/reg_tradeui_checkout.cpp:26 | TradeUI |
| standalone/reg_work_tail_trade.cpp:30 | TradeWork |
| standalone/trade_buy_harness.cpp:41 | TradeUI |

### G.5 Reader-less `RegisterExeAddr` rows at converted members' addresses, per file

| file | rows |
|---|---:|
| code/Game/Component/particlecomponent.cpp | 1 |
| standalone/reg_ambientenv_ui_serialize.cpp | 2 |
| standalone/reg_animal_serialize.cpp | 6 |
| standalone/reg_animal_update.cpp | 3 |
| standalone/reg_cemetery_healer_serialize.cpp | 2 |
| standalone/reg_citizen_employ_cascade.cpp | 1 |
| standalone/reg_citizen_needsupdate.cpp | 1 |
| standalone/reg_citizen_onremove.cpp | 2 |
| standalone/reg_citizenai_executor.cpp | 1 |
| standalone/reg_code_lane2.cpp | 1 |
| standalone/reg_code_lane6.cpp | 1 |
| standalone/reg_component_cropdisease.cpp | 5 |
| standalone/reg_component_economy.cpp | 15 |
| standalone/reg_component_nature.cpp | 2 |
| standalone/reg_fire.cpp | 1 |
| standalone/reg_fruit_happiness_serialize.cpp | 2 |
| standalone/reg_growth_spawn_serialize.cpp | 2 |
| standalone/reg_herd_live.cpp | 5 |
| standalone/reg_pasture_live.cpp | 5 |
| standalone/reg_player_bridge.cpp | 2 |
| standalone/reg_player_drag.cpp | 1 |
| standalone/reg_player_edit.cpp | 1 |
| standalone/reg_quarry_live.cpp | 2 |
| standalone/reg_terraform_live.cpp | 4 |
| standalone/reg_tornado.cpp | 3 |
| standalone/reg_town_services.cpp | 3 |
| standalone/reg_trade_serialize.cpp | 1 |
| standalone/reg_work_base.cpp | 1 |
| standalone/reg_work_tail_trade.cpp | 1 |
