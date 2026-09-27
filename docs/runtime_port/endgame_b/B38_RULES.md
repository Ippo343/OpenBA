# B.3 step 8 — `code/Game`: the exe-phase classes become real C++ classes: the rule sheet

Written 2026-09-26 by the step-8 question-pass owner (branch `b38-rules`, from main `bc57c0a5`). This is the sheet
the conversion blocks are handed. It answers `NEXT_SESSION.md` §2b from the RETAIL `Application-x64.exe` (11473
functions, image base 0x140000000), from the block-0 inventories and from THE PILOT (§7), which converted the exe
bases and one leaf of each shape on this branch and passed the static gate and a runtime check. It supersedes
`b38_session/INVENTORY_A.md`, `INVENTORY_B.md` and `SEAM_AND_FILEMAP.md` wherever they disagree — those are the
maps, this is the ruling. §6 lists what is still NOT VERIFIED; nothing outside §6 was taken on trust.

Standing rules from `B34B_RULES.md` §2d, `B35_RULES.md` §8, `B36_RULES.md` §8/§10/§14 and `B37_RULES.md` §8, §10 and
§14 (all of them — B7-5 argument passing, B7-8 deep copies, B37 §14.6 hex sweeps by demangled owner, §14.7 warning
A/B in a base worktree) apply unchanged and are not repeated except where this batch instantiates them.
`ENDGAME.md` §B.1/§B.2 apply unchanged EXCEPT decision 2's multiple-inheritance paragraph (§0.1) and, for the exe
classes, the "no real ctor on a polymorphic class" rule of steps 6/7 (D1).

## 0. How this sheet was measured

| fact | instrument |
|---|---|
| every exe table, slot count, each slot's target, the reflected name + parent literal, `sizeof`, member vptrs | block 0C's `b38_session/ghidra/Step8Appendix.java` (EXE mode), output `B38_SLOTS.md` + `b38_session/step8_appendix_raw.txt`. Copy from there. |
| tree-side tables, factories, dtors, 48-slot padding, controller members | block 0A (`INVENTORY_A.md`: components, descriptions, actions, works, entity) and 0B (`INVENTORY_B.md`: dialogs, panels, loops, goals, tools, tutorial, Game externals) |
| the RegisterExeAddr seam, by-address vtable readers, the B6-5 census, the loader-thread reach, the file rule | block 0D (`SEAM_AND_FILEMAP.md`) |
| this pass's own retail reads (Ghidra, `program=Application-x64.exe` per call, bytes decoded by hand) | `WellComponent` create/ctor/dtor/slots 0, 3, 7 and its 9 member slots (0x14000fc80-0x14000fe9f, 0x14005cf60-0x14005d23f, 0x140010090-0x14001026f, table 0x14015ae40); `WellDescription` create/createInPlace/slots 0, 5-9; the shared slot-7 helper 0x140001640; `Component` ctor 0x1401188b0, Persist 0x1401189b0, slot 0 0x140031cc0; `ControllerComponent` slot 0 0x140032800 and Persist 0x1401195a0; Goal/Scenario leaf rows of `B38_SLOTS.md` §6 re-read for slot 10 / slot 8 |
| the class list of §1 | `scripts/classlist.py` (in the session scratchpad): B38_SLOTS §0's 310 reflected rows joined to 0D's block lists, parent-literal fallback for 11 rows, base owners for 10, the pilot's 5 |
| tree registrations vs retail literals | `scripts/parentcmp.py`: every `REGISTER_TYPE` / `_FULL` / `UI_DIALOG_STUB` / `REGISTER_ABSTRACT_FACTORY` / `DEFINE_EXE_TYPE` under `code/` against §1 — **310 = 310 names, 0 missing, 0 extra, 1 parent disagreement** (`OptionsDialog`, §3 D4), **1 duplicate** (`AutoSaveDialog`) |
| the file map | `scripts/filemap.py` = 0D's `blocks_lib.py` path rule + the §5 overrides → `B38_FILEMAP.md`, 731 `code/Game` files, 0 unassigned |
| the pilot | built and run on this branch (§7): static gate + new-game/pause-quit + a demo-well run + the resave gate |

### 0.1 CORRECTION to `ENDGAME.md` B.1 decision 2 (retail-proved by 0A and 0C independently, re-read here)

Decision 2's paragraph "Multiple inheritance (the 56 `ControllerComponent` types): … `class BuildComponent : public
Component, public ControllerComponent` puts the second vptr at +0x28 … the compiler emits the this-adjusting thunks
retail hand-wrote" is **wrong for this exe**:
1. There is **no multiple inheritance and no RTTI for game classes** in `Application-x64.exe` (`B38_SLOTS.md` head,
   facts 1-2): no `SUB RCX,imm; JMP` thunk in any table; the only `.?AV` descriptors are the CRT's.
2. The "second vptr" is an **embedded MEMBER object** with its own vtable: stamped AFTER the leaf's primary vptr
   (member-construction order), at a PER-CLASS offset (+0x28 for 22 classes, +0x30 Happiness, +0x38 Attendance,
   +0x48 Pasture, +0x58 Nomad, +0x78 Boat, +0x88 Trade — a second base would sit at one offset), and
   `ControllerComponent` slot 17 (pure) is overridden in every leaf by `LEA RAX,[RCX+off]; RET`.
3. The family is **29 classes, not 56**: 23 with parent literal `ControllerComponent` + the 6
   `ResourceProduceComponent` subclasses (28 concrete × 2 tables = 56 is 0A's hypothesis for the old count, not verified). `StateMachineComponent`
   (parent `Component`) embeds a member of the same interface; `StatusIconComponent` embeds an exe
   `Graphics::Geometry` subclass (0x14015f0c0) at +0x28. 24 distinct member tables.
The retail shape, and D6's: `class X : public ControllerComponent { …; XController _controller; }` with the member
deriving a 9-slot all-pure interface. `ENDGAME.md` carries a one-line pointer to this correction.

---

## 1. THE CLASS LIST, SETTLED

**310 reflected exe classes, 51 unreflected tables (361).** Every reflected name in retail is registered in the tree
under the SAME name (310/310), so saves (which hash the NAME) are unaffected by construction. Per block (the pilot's
five are done):

| block | reflected classes | | block | reflected classes |
|---|---:|---|---|---:|
| PILOT (done) | 5 (`Component`, `ControllerComponent`, `Description`, `WellComponent`, `WellDescription`) | | GOAL | 27 |
| K1 | 82 | | LOOP | 12 (+ 5 unreflected loop tables, §1.2) |
| ACT | 18 (17 + `Action`) | | CIT | 19 |
| WRK | 23 (21 + `Work`, `AutoPickupWork`) | | VIS | 23 |
| TOOL | 12 (9 + `Tool`, `DragTool`, `ToolSetting`) | | MI-A | 13 |
| DLG | 19 | | MI-B | 15 (14 + `ResourceProduceComponent`) |
| PNL | 42 (incl. `SelectResourceUI`) | | **total** | **310** |

### 1.1 Shapes (counts from B38_SLOTS §3a's parent census)

- `Object::Internal`-rooted: `Component` 16 slots → 39 plain leaves (16) + `ControllerComponent` 18 → 23 leaves (18) +
  `ResourceProduceComponent` 22 → 6 leaves (22); `Action` 10 → 17; `Work` 13 → 16 + `AutoPickupWork` 13 → 6; `Tool` 13
  → 4 + `DragTool` 15 → 5; `ToolSetting` 4 → `RotationSetting`; `ComponentEntity` (4); `TutorialInstance`;
  `Achievement::GoalInstance` → 2.
- `Object::External`-rooted: `Description` 9 → 78 leaves (10); 19 direct (`ComponentDescription`,
  `EnvironmentDescription`, the 12 world externals, `Tutorial`, `TutorialStep`, …); `TutorialGoal` 9 → 4, `TutorialImage`
  10 → 3, `TutorialSetup` 11 → 4; `Achievement::Goal` → 5 (11); `::Achievement::Achievement` → `Scenario` (9).
- UI: `UI::DialogInstance` → 18 + `OptionsDialog` (`"public UI::DialogInstance"`) + `MessageDialog` 14 →
  `OptionInputBindDialog`; `UI::DialogController` → 35 + `DebugUI` (`"DialogController"`) + `SelectResourceUI` 14 → 3;
  `UI::DialogControllerConfig` → 4; `Element` → 2; `ElementDescription` → 2.
- Graphics: `Graphics::AnimationEvent` → `AnimAudioEvent`; `Graphics::Particle` → `Tornado`.
- 13 abstract (create = 0x140003730): `Component`, `ControllerComponent`, `ResourceProduceComponent`, `Description`,
  `Action`, `Work`, `AutoPickupWork`, `Tool`, `DragTool`, `ToolSetting`, `SelectResourceUI`, `TutorialGoal`,
  `TutorialSetup`.

### 1.2 The 51 unreflected tables (B38_SLOTS §4) — who owns each

| tables | owner | disposition |
|---|---|---|
| 24 controller-member tables + the interface 0x14015f070 | the leaf's block (MI-A/MI-B; VIS for `StateMachineComponent` 0x1401644d0) | a member class per D6; the interface is `ControllerComponent::Controller` (pilot) |
| 5 loop subclasses: `GameUpdateLoop` 0x14015e2f0 (12), `MenuDialogLoopT` 0x14015ed00 (13), splash 0x140158800 (13), LoaderLoop 0x140158870 / 0x1401588d0 (12) | LOOP | real classes on the real Engine loop bases (D8); the `RtEngine::` PODs and the five `standalone/*_stubs.cpp` tables retire with them |
| 11 exe-emitted tables of RUNTIME classes whose ctor the exe inlined (`AchievementInstance` 0x14015e350, `Graphics::{EdgeMesh, FrameCopyImageBuffer, MaterialInstance, MeshInstance, ParticleSystemInstance, SunLight}`, `UI::{DialogInstance 0x14015f5a0, SpriteCapture, SpriteSheet}`, `Video::ImageBuffer`) | the block whose file builds the object (LOOP: SkyMesh/Sun/AchievementInstance; DLG: ModDialog ImageBuffer, thumb mirrors; TOOL: ToolUI child DialogInstance) | NOT exe classes: each becomes `new (p) NS::X(...)` of the real Runtime class; the copy/mirror tables die (the five slot-copy sites, D5) |
| 5 unreflected exe subclasses of Runtime classes: `System::Thread` 0x14015ef18 (the pathfinder, `MemAlloc(0x1f8)`), `Graphics::Geometry` 0x14015f0c0 (StatusIcon member), 0x14015f110 (`MemAlloc(0x230)`), 0x140160240 (`MemAlloc(0xf0)`), `Graphics::ParticleInstance` 0x1401601e0 | LOOP (pathfinder, 0x14015f110, 0x140160240 — geometry draw / weather), VIS (StatusIcon geometry), LOOP (Tornado instance) | real C++ subclasses, no reflection |
| 5 CRT entries | — | not ours |

### 1.3 The table (every reflected class)

`parent literal EXACTLY` is what retail's `BaseTypeInfo` ctor receives (copy it; quirks in D4). `slots` = retail's
count; `sizeof` = the create function's `MemAlloc`; `member vptrs` = tables stamped at this+N≠0 (D6). The block
column is §5's. Generated by `scripts/classlist.py`; re-run it rather than editing rows by hand.

| class (reflected literal) | parent literal EXACTLY | retail table | slots | sizeof | member vptrs (this+N) | block |
|---|---|---|---:|---|---|---|
| AbsorbResourceComponent | `"ControllerComponent"` | 0x140162000 | 18 | 0x48 | +0x28=0x14015f070 +0x28=0x140161fb0 | MI-A |
| AbsorbResourceDescription | `"Description"` | 0x140162090 | 10 | 0x20 |  | K1 |
| AchievementDialog | `"UI::DialogInstance"` | 0x14015b020 | 10 | 0xd0 |  | DLG |
| Action | `"Object::Internal"` | 0x14015faa0 | 10 | abstract |  | ACT |
| AmbientEmitterComponent | `"Component"` | 0x140158a60 | 16 | 0x48 |  | CIT |
| AmbientEmitterDescription | `"Description"` | 0x140158ae0 | 10 | 0x38 |  | K1 |
| AmbientEnvironmentComponent | `"Component"` | 0x140158b70 | 16 | 0x48 |  | CIT |
| AmbientEnvironmentDescription | `"Description"` | 0x140158bf0 | 10 | 0x38 |  | K1 |
| AnimalComponent | `"Component"` | 0x140162110 | 16 | 0x58 |  | CIT |
| AnimalDescription | `"Description"` | 0x140162190 | 10 | 0x58 |  | K1 |
| AnimateNaturalResource | `"Action"` | 0x14015fb10 | 10 | 0x30 |  | ACT |
| AnimAudioEvent | `"Graphics::AnimationEvent"` | 0x14015e0c0 | 10 | 0x30 |  | VIS |
| AttendanceComponent | `"ControllerComponent"` | 0x140158ce0 | 18 | 0x58 | +0x38=0x140158c90 +0x38=0x14015f070 | MI-A |
| AttendanceDescription | `"Description"` | 0x140158d70 | 10 | 0x28 |  | K1 |
| AttendanceUI | `"UI::DialogController"` | 0x14015b070 | 11 | 0x38 |  | PNL |
| AutoPickupComponent | `"Component"` | 0x140158df0 | 16 | 0x40 |  | VIS |
| AutoPickupDescription | `"Description"` | 0x140158e70 | 10 | 0x28 |  | K1 |
| AutoPickupWork | `"Work"` | 0x140160fa0 | 13 | abstract |  | WRK |
| AutoSaveDialog | `"UI::DialogInstance"` | 0x14015b120 | 10 | 0xd0 |  | DLG |
| BoatComponent | `"ControllerComponent"` | 0x140158f40 | 18 | 0x98 | +0x78=0x140158ef0 +0x78=0x14015f070 | MI-A |
| BoatDescription | `"Description"` | 0x140158fd0 | 10 | 0x20 |  | K1 |
| BridgeTool | `"DragTool"` | 0x140161b00 | 15 | 0x40 |  | TOOL |
| BuildComponent | `"ControllerComponent"` | 0x140162260 | 18 | 0x78 | +0x28=0x14015f070 +0x28=0x140162210 | MI-A |
| BuildDescription | `"Description"` | 0x1401622f0 | 10 | 0x38 |  | K1 |
| BuildingGoal | `"Achievement::Goal"` | 0x140160300 | 11 | 0x48 |  | GOAL |
| BuildRequirementUI | `"UI::DialogController"` | 0x14015b1d0 | 11 | 0x60 |  | PNL |
| BuildRoad | `"Work"` | 0x140161020 | 13 | 0x70 |  | WRK |
| BuildStructure | `"Work"` | 0x1401610a0 | 13 | 0x70 |  | WRK |
| BuildUI | `"UI::DialogController"` | 0x14015f210 | 11 | 0x50 |  | PNL |
| CaptureBoxDescription | `"ElementDescription"` | 0x14015d4a0 | 10 | 0x78 |  | K1 |
| CaptureBoxElement | `"Element"` | 0x14015d440 | 12 | 0xe0 |  | VIS |
| CemeteryComponent | `"ControllerComponent"` | 0x1401590a0 | 18 | 0x58 | +0x28=0x140159050 +0x28=0x14015f070 | MI-A |
| CemeteryDescription | `"Description"` | 0x140159130 | 10 | 0x28 |  | K1 |
| CemeteryUI | `"UI::DialogController"` | 0x14015b240 | 11 | 0x38 |  | PNL |
| CharacterComponent | `"Component"` | 0x140162370 | 16 | 0x80 |  | CIT |
| CharacterDescription | `"Description"` | 0x1401623f0 | 10 | 0x48 |  | K1 |
| CitizenAIComponent | `"Component"` | 0x140162470 | 16 | 0xd0 |  | CIT |
| CitizenAIDescription | `"Description"` | 0x1401624f0 | 10 | 0x20 |  | K1 |
| CitizenComponent | `"Component"` | 0x140162580 | 16 | 0x290 |  | CIT |
| CitizenDescription | `"Description"` | 0x140162600 | 10 | 0x148 |  | K1 |
| CitizenUI | `"UI::DialogController"` | 0x14015ba00 | 11 | 0xe8 |  | PNL |
| ClearArea | `"Work"` | 0x140161110 | 13 | 0x78 |  | WRK |
| ClearAreaComponent | `"ControllerComponent"` | 0x1401626d0 | 18 | 0x50 | +0x28=0x14015f070 +0x28=0x140162680 | MI-A |
| ClearAreaDescription | `"Description"` | 0x140162760 | 10 | 0x38 |  | K1 |
| ClearResourceComponent | `"ControllerComponent"` | 0x140162840 | 18 | 0x48 | +0x28=0x14015f070 +0x28=0x1401627f0 | MI-A |
| ClearResourceDescription | `"Description"` | 0x1401628d0 | 10 | 0x20 |  | K1 |
| ClearStorage | `"Work"` | 0x1401611a0 | 13 | 0x68 |  | WRK |
| ClimateGenerator | `"Object::External"` | 0x14015e140 | 9 | 0x110 |  | LOOP |
| CompleteRoad | `"Action"` | 0x14015fb70 | 10 | 0x30 |  | ACT |
| CompleteTask | `"Action"` | 0x14015fbd0 | 10 | 0x38 |  | ACT |
| Component | `"Object::Internal"` | 0x140162920 | 16 | abstract |  | PILOT |
| ComponentDescription | `"Object::External"` | 0x140162a00 | 9 | 0x38 |  | K1 |
| ComponentEntity | `"Object::Internal"` | 0x14015fa78 | 4 | 0x48 |  | K1 |
| ConsumeProduce | `"Work"` | 0x140161220 | 13 | 0x78 |  | WRK |
| ConsumeProduceComponent | `"ResourceProduceComponent"` | 0x140162a80 | 22 | 0x70 |  | MI-B |
| ConsumeProduceDescription | `"Description"` | 0x140162b30 | 10 | 0x58 |  | K1 |
| ConsumeProduceUI | `"UI::DialogController"` | 0x14015b2e0 | 11 | 0x40 |  | PNL |
| ConsumeRawMaterial | `"Action"` | 0x14015fc20 | 10 | 0x40 |  | ACT |
| ControllerComponent | `"Component"` | 0x140162b80 | 18 | abstract |  | PILOT |
| ControlUI | `"UI::DialogController"` | 0x14015b380 | 11 | 0x40 |  | PNL |
| ControlUIConfig | `"UI::DialogControllerConfig"` | 0x14015b3e0 | 9 | 0x28 |  | PNL |
| CreateBridgeDescription | `"Description"` | 0x140162c30 | 10 | 0x60 |  | K1 |
| CreateDragDescription | `"Description"` | 0x140162ca0 | 10 | 0x40 |  | K1 |
| CreateFencedDescription | `"Description"` | 0x1401591a0 | 10 | 0x78 |  | K1 |
| CreateNaturalResource | `"Action"` | 0x14015fca0 | 10 | 0x38 |  | ACT |
| CreatePathDescription | `"Description"` | 0x140159210 | 10 | 0x20 |  | K1 |
| CreatePlacedDescription | `"Description"` | 0x140162cf0 | 10 | 0x58 |  | K1 |
| CreateRawMaterial | `"Action"` | 0x14015fd10 | 10 | 0x58 |  | ACT |
| CreateRemovalDescription | `"Description"` | 0x140162d80 | 10 | 0x48 |  | K1 |
| CreateRoadDescription | `"Description"` | 0x140162df0 | 10 | 0x30 |  | K1 |
| CropFieldComponent | `"ControllerComponent"` | 0x140162ec0 | 18 | 0x68 | +0x28=0x14015f070 +0x28=0x140162e70 | MI-B |
| CropFieldDescription | `"Description"` | 0x140162f50 | 10 | 0x28 |  | K1 |
| CropUI | `"SelectResourceUI"` | 0x14015f270 | 14 | 0x60 |  | PNL |
| DebugUI | `"DialogController"` | 0x14015b870 | 11 | 0x130 |  | PNL |
| DecalComponent | `"Component"` | 0x140162fd0 | 16 | 0x38 |  | VIS |
| DecalDescription | `"Description"` | 0x140163050 | 10 | 0x50 |  | K1 |
| Description | `"Object::External"` | 0x1401629a0 | 9 | abstract |  | PILOT |
| DestroyComponent | `"ControllerComponent"` | 0x140163120 | 18 | 0x68 | +0x28=0x14015f070 +0x28=0x1401630d0 | MI-A |
| DestroyDescription | `"Description"` | 0x1401631b0 | 10 | 0x38 |  | K1 |
| DestroyUI | `"UI::DialogController"` | 0x14015bb00 | 11 | 0x78 |  | PNL |
| Disease | `"Object::External"` | 0x14015e190 | 9 | 0x48 |  | LOOP |
| DiseaseComponent | `"Component"` | 0x140159290 | 16 | 0x58 |  | CIT |
| DiseaseDescription | `"Description"` | 0x140159310 | 10 | 0x48 |  | K1 |
| DragPlacementTool | `"DragTool"` | 0x140161b90 | 15 | 0x48 |  | TOOL |
| DragTool | `"Tool"` | 0x140161c10 | 15 | abstract |  | TOOL |
| DropOffRawMaterial | `"Action"` | 0x14015fd80 | 10 | 0x40 |  | ACT |
| EditTool | `"Tool"` | 0x140161ca0 | 14 | 0x38 |  | TOOL |
| EntityUI | `"UI::DialogController"` | 0x14015bb90 | 11 | 0x48 |  | PNL |
| EnvironmentDescription | `"Object::External"` | 0x14015e1f0 | 9 | 0x1b8 |  | K1 |
| EventLogUI | `"UI::DialogController"` | 0x14015bc40 | 11 | 0x80 |  | PNL |
| EventNotifyDialog | `"UI::DialogInstance"` | 0x14015bca0 | 10 | 0xf0 |  | DLG |
| EventPopupDialog | `"UI::DialogInstance"` | 0x14015bd80 | 10 | 0xc0 |  | DLG |
| FencedTool | `"DragTool"` | 0x1401605d0 | 15 | 0x70 |  | TOOL |
| FinishTutorialGoal | `"Achievement::Goal"` | 0x140160420 | 11 | 0x38 |  | GOAL |
| FinishTutorialInstance | `"Achievement::GoalInstance"` | 0x140160478 | 7 | 0x28 |  | GOAL |
| FireComponent | `"Component"` | 0x140159380 | 16 | 0x48 |  | CIT |
| FireDescription | `"Description"` | 0x140159400 | 10 | 0x50 |  | K1 |
| FireWork | `"Work"` | 0x1401612a0 | 13 | 0x60 |  | WRK |
| FishingComponent | `"ResourceProduceComponent"` | 0x140163230 | 22 | 0x58 |  | MI-B |
| FishingDescription | `"Description"` | 0x1401632e0 | 10 | 0x30 |  | K1 |
| FishingWork | `"Work"` | 0x140161320 | 13 | 0x70 |  | WRK |
| FloorComponent | `"Component"` | 0x140163360 | 16 | 0x30 |  | VIS |
| FloorDescription | `"Description"` | 0x1401633e0 | 10 | 0x30 |  | K1 |
| FollowDialog | `"UI::DialogInstance"` | 0x14015be00 | 10 | 0xd8 |  | DLG |
| ForesterComponent | `"ResourceProduceComponent"` | 0x140163460 | 22 | 0x58 |  | MI-B |
| ForesterDescription | `"Description"` | 0x140163510 | 10 | 0x30 |  | K1 |
| ForesterWork | `"AutoPickupWork"` | 0x1401613a0 | 13 | 0x60 |  | WRK |
| FruitComponent | `"Component"` | 0x140159450 | 16 | 0x38 |  | CIT |
| FruitDescription | `"Description"` | 0x1401594d0 | 10 | 0x38 |  | K1 |
| FrustumOutlineDescription | `"ElementDescription"` | 0x14015c300 | 10 | 0x80 |  | K1 |
| FrustumOutlineElement | `"Element"` | 0x14015c2a0 | 12 | 0xd8 |  | VIS |
| GatherRawMaterials | `"Work"` | 0x140161420 | 13 | 0x78 |  | WRK |
| GoalDialog | `"UI::DialogInstance"` | 0x14015beb0 | 10 | 0xc8 |  | DLG |
| GrowthComponent | `"Component"` | 0x140163590 | 16 | 0x50 |  | CIT |
| GrowthDescription | `"Description"` | 0x140163610 | 10 | 0x68 |  | K1 |
| HappinessComponent | `"ControllerComponent"` | 0x1401595d0 | 18 | 0x50 | +0x30=0x140159580 +0x30=0x14015f070 | MI-A |
| HappinessDescription | `"Description"` | 0x140159660 | 10 | 0x30 |  | K1 |
| HarvestArea | `"AutoPickupWork"` | 0x1401614a0 | 13 | 0x68 |  | WRK |
| HarvestOrchard | `"AutoPickupWork"` | 0x140161520 | 13 | 0x68 |  | WRK |
| HealerComponent | `"ControllerComponent"` | 0x140159730 | 18 | 0x48 | +0x28=0x1401596e0 +0x28=0x14015f070 | MI-A |
| HealerDescription | `"Description"` | 0x1401597c0 | 10 | 0x20 |  | K1 |
| HelpUI | `"UI::DialogController"` | 0x14015bf30 | 11 | 0x78 |  | PNL |
| HerbalistComponent | `"ResourceProduceComponent"` | 0x140159860 | 22 | 0x50 |  | MI-B |
| HerbalistDescription | `"Description"` | 0x140159910 | 10 | 0x38 |  | K1 |
| HerbalistWork | `"AutoPickupWork"` | 0x1401615a0 | 13 | 0x60 |  | WRK |
| HerdComponent | `"Component"` | 0x140163680 | 16 | 0x60 |  | CIT |
| HerdDescription | `"Description"` | 0x140163700 | 10 | 0x40 |  | K1 |
| HighlightComponent | `"Component"` | 0x140159990 | 16 | 0x128 |  | VIS |
| HighlightDescription | `"Description"` | 0x140159a10 | 10 | 0x40 |  | K1 |
| HotKeyToolTipUI | `"UI::DialogController"` | 0x14015bfc0 | 11 | 0x30 |  | PNL |
| HuntAnimal | `"Action"` | 0x14015fde0 | 10 | 0x40 |  | ACT |
| HuntComponent | `"ResourceProduceComponent"` | 0x140163770 | 22 | 0x58 |  | MI-B |
| HuntDescription | `"Description"` | 0x140163820 | 10 | 0x28 |  | K1 |
| HuntWork | `"AutoPickupWork"` | 0x140161620 | 13 | 0x58 |  | WRK |
| ImmigrationComponent | `"ControllerComponent"` | 0x140159b30 | 18 | 0x68 | +0x28=0x140159ae0 +0x28=0x14015f070 | MI-A |
| ImmigrationDescription | `"Description"` | 0x140159bc0 | 10 | 0x68 |  | K1 |
| ImmigrationUI | `"UI::DialogController"` | 0x14015c080 | 11 | 0x50 |  | PNL |
| InteractComponent | `"Component"` | 0x1401638b0 | 16 | 0x38 |  | VIS |
| InteractDescription | `"Description"` | 0x140163930 | 10 | 0x58 |  | K1 |
| LivestockComponent | `"Component"` | 0x1401639b0 | 16 | 0x38 |  | CIT |
| LivestockDescription | `"Description"` | 0x140163a30 | 10 | 0x30 |  | K1 |
| LoaderDialog | `"UI::DialogInstance"` | 0x14015f300 | 10 | 0xc8 |  | DLG |
| LocationWork | `"Work"` | 0x1401616a0 | 13 | 0x50 |  | WRK |
| MainMenuDialog | `"UI::DialogInstance"` | 0x14015f550 | 10 | 0xc8 |  | DLG |
| MapComponent | `"Component"` | 0x140163aa0 | 16 | 0x38 |  | VIS |
| MapDescription | `"Description"` | 0x140163b20 | 10 | 0x38 |  | K1 |
| MapGenerator | `"Object::External"` | 0x14015ed80 | 9 | 0x178 |  | LOOP |
| MapParamsDialog | `"UI::DialogInstance"` | 0x14015c180 | 10 | 0xf8 |  | DLG |
| MapUI | `"UI::DialogController"` | 0x14015c350 | 11 | 0x50 |  | PNL |
| MarketComponent | `"ResourceProduceComponent"` | 0x140159c40 | 22 | 0x50 |  | MI-B |
| MarketDescription | `"Description"` | 0x140159cf0 | 10 | 0x38 |  | K1 |
| MarketLocationComponent | `"ControllerComponent"` | 0x140159dd0 | 18 | 0x48 | +0x28=0x140159d80 +0x28=0x14015f070 | MI-B |
| MarketLocationDescription | `"Description"` | 0x140159e60 | 10 | 0x20 |  | K1 |
| MarketWork | `"Work"` | 0x140161710 | 13 | 0x58 |  | WRK |
| MenuScene | `"Object::External"` | 0x14015ede0 | 9 | 0x208 |  | LOOP |
| MessageDialog | `"UI::DialogInstance"` | 0x14015c480 | 14 | 0xc0 |  | DLG |
| MineComponent | `"Component"` | 0x140159ed0 | 16 | 0x28 |  | VIS |
| MineDescription | `"Description"` | 0x140159f50 | 10 | 0x20 |  | K1 |
| ModBrowseUI | `"UI::DialogController"` | 0x14015c500 | 11 | 0x30 |  | PNL |
| ModDetailsDialog | `"UI::DialogInstance"` | 0x14015c670 | 10 | 0xc8 |  | DLG |
| ModDialog | `"UI::DialogInstance"` | 0x14015c870 | 10 | 0xd8 |  | DLG |
| ModelComponent | `"Component"` | 0x140163ba0 | 16 | 0x170 |  | VIS |
| ModelDescription | `"Description"` | 0x140163c20 | 10 | 0x40 |  | K1 |
| ModUploadDialog | `"UI::DialogInstance"` | 0x14015c6c0 | 10 | 0xb8 |  | DLG |
| MoveTo | `"Action"` | 0x14015fe40 | 10 | 0x480 |  | ACT |
| MultiObjectComponent | `"ControllerComponent"` | 0x140163cf0 | 18 | 0x60 | +0x28=0x14015f070 +0x28=0x140163ca0 | MI-B |
| MultiObjectDescription | `"Description"` | 0x140163d80 | 10 | 0x20 |  | K1 |
| MusicList | `"Object::External"` | 0x14015ee40 | 9 | 0x30 |  | LOOP |
| NameDatabase | `"Object::External"` | 0x14015eea0 | 9 | 0x60 |  | LOOP |
| NaturalResourceComponent | `"Component"` | 0x140163e10 | 16 | 0x30 |  | CIT |
| NaturalResourceDescription | `"Description"` | 0x140163e90 | 10 | 0x88 |  | K1 |
| NewGameDialog | `"UI::DialogInstance"` | 0x14015c920 | 10 | 0xae8 |  | DLG |
| NoiseUI | `"UI::DialogController"` | 0x14015ca80 | 11 | 0x70 |  | PNL |
| NomadComponent | `"ControllerComponent"` | 0x14015a020 | 18 | 0x78 | +0x58=0x140159fd0 +0x58=0x14015f070 | MI-A |
| NomadDescription | `"Description"` | 0x14015a0b0 | 10 | 0x38 |  | K1 |
| OptionInputBindDialog | `"MessageDialog"` | 0x14015ce40 | 14 | 0xc8 |  | DLG |
| OptionsDialog | `"public UI::DialogInstance"` | 0x14015ceb0 | 10 | 0x1c0 |  | DLG |
| OrchardComponent | `"ControllerComponent"` | 0x14015a180 | 18 | 0x68 | +0x28=0x14015a130 +0x28=0x14015f070 | MI-B |
| OrchardDescription | `"Description"` | 0x14015a210 | 10 | 0x30 |  | K1 |
| OrchardUI | `"SelectResourceUI"` | 0x14015cf30 | 14 | 0x48 |  | PNL |
| PageToggleUI | `"UI::DialogController"` | 0x14015cfb0 | 11 | 0x48 |  | PNL |
| ParticleComponent | `"Component"` | 0x14015a290 | 16 | 0x48 |  | CIT |
| ParticleDescription | `"Description"` | 0x14015a310 | 10 | 0x38 |  | K1 |
| PastureComponent | `"ControllerComponent"` | 0x140163f60 | 18 | 0x68 | +0x48=0x14015f070 +0x48=0x140163f10 | MI-B |
| PastureDescription | `"Description"` | 0x140163ff0 | 10 | 0x28 |  | K1 |
| PastureUI | `"SelectResourceUI"` | 0x14015f610 | 14 | 0x40 |  | PNL |
| PastureWork | `"AutoPickupWork"` | 0x1401617a0 | 13 | 0x58 |  | WRK |
| PathTool | `"Tool"` | 0x140160680 | 14 | 0x60 |  | TOOL |
| PerformWork | `"Action"` | 0x14015fea0 | 10 | 0x48 |  | ACT |
| PickingComponent | `"Component"` | 0x140164040 | 16 | 0x38 |  | VIS |
| PickingDescription | `"Description"` | 0x1401640c0 | 10 | 0x40 |  | K1 |
| Pickup | `"Work"` | 0x140161810 | 13 | 0x60 |  | WRK |
| PickupRawMaterial | `"Action"` | 0x14015ff10 | 10 | 0x30 |  | ACT |
| PickupStoredRawMaterial | `"Action"` | 0x14015ff80 | 10 | 0x48 |  | ACT |
| PlacementTool | `"Tool"` | 0x140161d20 | 13 | 0x70 |  | TOOL |
| PlantArea | `"Work"` | 0x140161890 | 13 | 0x70 |  | WRK |
| PlantOrchard | `"Work"` | 0x140161910 | 13 | 0x70 |  | WRK |
| PlayAnimation | `"Action"` | 0x140158950 | 10 | 0x40 |  | ACT |
| PopulationComponent | `"Component"` | 0x14015a360 | 16 | 0x70 |  | CIT |
| PopulationDescription | `"Description"` | 0x14015a3e0 | 10 | 0x58 |  | K1 |
| PopulationGoal | `"Achievement::Goal"` | 0x140160370 | 11 | 0x38 |  | GOAL |
| ProductionUI | `"UI::DialogController"` | 0x14015d070 | 11 | 0x50 |  | PNL |
| Profession | `"Object::External"` | 0x14015efa0 | 9 | 0x60 |  | LOOP |
| ProfessionList | `"Object::External"` | 0x14015ef50 | 9 | 0x50 |  | LOOP |
| ProfessionUI | `"UI::DialogController"` | 0x14015d0f0 | 11 | 0x78 |  | PNL |
| QuarryComponent | `"Component"` | 0x14015a490 | 16 | 0x28 |  | VIS |
| QuarryDescription | `"Description"` | 0x14015a510 | 10 | 0x30 |  | K1 |
| RadiusDescription | `"Description"` | 0x14015a580 | 10 | 0x30 |  | K1 |
| RadiusUI | `"UI::DialogController"` | 0x14015d160 | 11 | 0x38 |  | PNL |
| RawMaterialComponent | `"Component"` | 0x140164170 | 16 | 0x40 |  | VIS |
| RawMaterialDescription | `"Description"` | 0x1401641f0 | 10 | 0x60 |  | K1 |
| RemovalTool | `"DragTool"` | 0x140161de0 | 15 | 0x40 |  | TOOL |
| RemoveAnimal | `"Action"` | 0x14015ffe0 | 10 | 0x28 |  | ACT |
| RemoveNaturalResource | `"Action"` | 0x140160050 | 10 | 0x38 |  | ACT |
| ResidenceComponent | `"ControllerComponent"` | 0x1401642c0 | 18 | 0x60 | +0x28=0x14015f070 +0x28=0x140164270 | MI-A |
| ResidenceDescription | `"Description"` | 0x140164350 | 10 | 0x30 |  | K1 |
| ResidenceUI | `"UI::DialogController"` | 0x14015f6d0 | 11 | 0x40 |  | PNL |
| ResourceLimitUI | `"UI::DialogController"` | 0x14015d1f0 | 11 | 0x40 |  | PNL |
| ResourceLimitUIConfig | `"UI::DialogControllerConfig"` | 0x14015d250 | 9 | 0x28 |  | PNL |
| ResourceProduceComponent | `"ControllerComponent"` | 0x1401643f0 | 22 | abstract | +0x28=0x14015f070 +0x28=0x1401643a0 | MI-B |
| RoadTool | `"DragTool"` | 0x140161e70 | 15 | 0x48 |  | TOOL |
| RotationSetting | `"ToolSetting"` | 0x140160710 | 4 | 0x18 |  | TOOL |
| SaveLoadDialog | `"UI::DialogInstance"` | 0x14015f900 | 10 | 0x160 |  | DLG |
| Scenario | `"::Achievement::Achievement"` | 0x14015f020 | 9 | 0x90 |  | GOAL |
| ScenarioDialog | `"UI::DialogInstance"` | 0x14015d2d0 | 10 | 0xe8 |  | DLG |
| SelectDescription | `"Description"` | 0x14015a5f0 | 10 | 0x48 |  | K1 |
| SelectResourceUI | `"UI::DialogController"` | 0x14015d330 | 14 | abstract |  | PNL |
| SendComponentMessage | `"Action"` | 0x1401600c0 | 10 | 0x40 |  | ACT |
| ShowTool | `"Action"` | 0x1401589b0 | 10 | 0x30 |  | ACT |
| SpawnComponent | `"Component"` | 0x14015a670 | 16 | 0x38 |  | CIT |
| SpawnDescription | `"Description"` | 0x14015a6f0 | 10 | 0x30 |  | K1 |
| SpriteCaptureUI | `"UI::DialogController"` | 0x14015d4f0 | 11 | 0x70 |  | PNL |
| StateMachineComponent | `"Component"` | 0x140164520 | 16 | 0x58 | +0x28=0x14015f070 +0x28=0x1401644d0 | VIS |
| StateMachineDescription | `"Description"` | 0x1401645a0 | 10 | 0x20 |  | K1 |
| StatGoal | `"Achievement::Goal"` | 0x1401604c0 | 11 | 0x80 |  | GOAL |
| StatusBarUI | `"UI::DialogController"` | 0x14015d810 | 11 | 0x1d0 |  | PNL |
| StatusIconComponent | `"Component"` | 0x140164620 | 16 | 0x108 | +0x28=0x14015f0c0 | VIS |
| StatusIconDescription | `"Description"` | 0x1401646a0 | 10 | 0x38 |  | K1 |
| StatusIconUI | `"UI::DialogController"` | 0x14015d8a0 | 11 | 0x40 |  | PNL |
| StatusIconUIConfig | `"UI::DialogControllerConfig"` | 0x14015d900 | 9 | 0x28 |  | PNL |
| StorageCapacityUI | `"UI::DialogController"` | 0x14015d950 | 11 | 0x38 |  | PNL |
| StorageComponent | `"Component"` | 0x140164720 | 16 | 0x40 |  | VIS |
| StorageDescription | `"Description"` | 0x1401647a0 | 10 | 0x40 |  | K1 |
| StorageDisplayComponent | `"Component"` | 0x14015a780 | 16 | 0x38 |  | VIS |
| StorageDisplayDescription | `"Description"` | 0x14015a800 | 10 | 0x20 |  | K1 |
| StorageLocationComponent | `"ControllerComponent"` | 0x140164880 | 18 | 0x48 | +0x28=0x14015f070 +0x28=0x140164830 | MI-B |
| StorageLocationDescription | `"Description"` | 0x140164910 | 10 | 0x28 |  | K1 |
| StorageUI | `"UI::DialogController"` | 0x14015f970 | 11 | 0x40 |  | PNL |
| TendArea | `"Work"` | 0x140161990 | 13 | 0x60 |  | WRK |
| TerraformComponent | `"Component"` | 0x14015a880 | 16 | 0x48 |  | VIS |
| TerraformDescription | `"Description"` | 0x14015a900 | 10 | 0x38 |  | K1 |
| TerrainGenerator | `"Object::External"` | 0x14015f170 | 9 | 0x90 |  | LOOP |
| TimeGoal | `"Achievement::Goal"` | 0x140160540 | 11 | 0x38 |  | GOAL |
| TimeGoalInstance | `"Achievement::GoalInstance"` | 0x140160598 | 7 | 0x28 |  | GOAL |
| TitleDescToolTipUI | `"UI::DialogController"` | 0x14015d9f0 | 11 | 0x30 |  | PNL |
| TombstoneComponent | `"Component"` | 0x14015a980 | 16 | 0x30 |  | CIT |
| TombstoneDescription | `"Description"` | 0x14015aa00 | 10 | 0x28 |  | K1 |
| Tool | `"Object::Internal"` | 0x140161f10 | 13 | abstract |  | TOOL |
| Toolbar | `"Object::External"` | 0x140160120 | 9 | 0x68 |  | LOOP |
| ToolbarDescription | `"Description"` | 0x140164980 | 10 | 0x68 |  | K1 |
| ToolbarDialog | `"UI::DialogInstance"` | 0x14015fa00 | 10 | 0x108 |  | DLG |
| ToolSetting | `"Object::Internal"` | 0x140161ee8 | 4 | abstract |  | TOOL |
| ToolUI | `"UI::DialogController"` | 0x14015da70 | 11 | 0x58 |  | PNL |
| Tornado | `"Graphics::Particle"` | 0x140160190 | 10 | 0x90 |  | LOOP |
| TornadoComponent | `"Component"` | 0x14015aa80 | 16 | 0x68 |  | CIT |
| TornadoDescription | `"Description"` | 0x14015ab00 | 10 | 0x38 |  | K1 |
| TownHallUI | `"UI::DialogController"` | 0x14015dce0 | 11 | 0x1e8 |  | PNL |
| TrackerComponent | `"Component"` | 0x14015ab80 | 16 | 0x50 |  | VIS |
| TrackerDescription | `"Description"` | 0x14015ac00 | 10 | 0x20 |  | K1 |
| TradeComponent | `"ControllerComponent"` | 0x14015acd0 | 18 | 0xa8 | +0x88=0x14015ac80 +0x88=0x14015f070 | MI-B |
| TradeDescription | `"Description"` | 0x14015ad60 | 10 | 0x70 |  | K1 |
| TradeUI | `"UI::DialogController"` | 0x14015de80 | 11 | 0x148 |  | PNL |
| TradeWork | `"Work"` | 0x140161a10 | 13 | 0x58 |  | WRK |
| Tutorial | `"Object::External"` | 0x140160740 | 9 | 0x40 |  | GOAL |
| TutorialGoal | `"Object::External"` | 0x1401607a0 | 9 | abstract |  | GOAL |
| TutorialGoalButton | `"TutorialGoal"` | 0x140160800 | 10 | 0x30 |  | GOAL |
| TutorialGoalCount | `"TutorialGoal"` | 0x140160950 | 10 | 0x50 |  | GOAL |
| TutorialGoalDialog | `"TutorialGoal"` | 0x140160870 | 10 | 0x40 |  | GOAL |
| TutorialGoalGameSpeed | `"TutorialGoal"` | 0x1401608e0 | 10 | 0x28 |  | GOAL |
| TutorialHighlightDialog | `"UI::DialogInstance"` | 0x1401609e0 | 10 | 0xf0 |  | GOAL |
| TutorialImage | `"Object::External"` | 0x140160a40 | 10 | 0x30 |  | GOAL |
| TutorialImageHotKey | `"TutorialImage"` | 0x140160ab0 | 10 | 0x48 |  | GOAL |
| TutorialImageInputKey | `"TutorialImage"` | 0x140160b20 | 10 | 0x38 |  | GOAL |
| TutorialImageToolbarIcon | `"TutorialImage"` | 0x140160b90 | 10 | 0x48 |  | GOAL |
| TutorialInstance | `"Object::Internal"` | 0x140160bf8 | 4 | 0x38 |  | GOAL |
| TutorialSetup | `"Object::External"` | 0x140160c30 | 11 | abstract |  | GOAL |
| TutorialSetupCamera | `"TutorialSetup"` | 0x140160ca0 | 12 | 0x30 |  | GOAL |
| TutorialSetupDialog | `"TutorialSetup"` | 0x140160d20 | 12 | 0x78 |  | GOAL |
| TutorialSetupGameSpeed | `"TutorialSetup"` | 0x140160da0 | 12 | 0x28 |  | GOAL |
| TutorialSetupRestrictPlayer | `"TutorialSetup"` | 0x140160e20 | 12 | 0x68 |  | GOAL |
| TutorialStep | `"Object::External"` | 0x140160e90 | 9 | 0x70 |  | GOAL |
| TutorialUI | `"UI::DialogController"` | 0x140160f30 | 11 | 0x60 |  | GOAL |
| UIComponent | `"Component"` | 0x1401649f0 | 16 | 0x38 |  | VIS |
| UIDescription | `"Description"` | 0x140164a70 | 10 | 0x48 |  | K1 |
| UpgradeUI | `"UI::DialogController"` | 0x14015df00 | 11 | 0x38 |  | PNL |
| UpgradeUIConfig | `"UI::DialogControllerConfig"` | 0x14015df60 | 9 | 0x28 |  | PNL |
| ValueDescription | `"Description"` | 0x14015add0 | 10 | 0x50 |  | K1 |
| Weather | `"Object::External"` | 0x140160290 | 9 | 0x80 |  | LOOP |
| WellComponent | `"ControllerComponent"` | 0x14015ae90 | 18 | 0x48 | +0x28=0x14015ae40 +0x28=0x14015f070 | PILOT |
| WellDescription | `"Description"` | 0x14015af20 | 10 | 0x20 |  | PILOT |
| WildAnimalComponent | `"Component"` | 0x140164af0 | 16 | 0x30 |  | CIT |
| WildAnimalDescription | `"Description"` | 0x140164b70 | 10 | 0x20 |  | K1 |
| Work | `"Object::Internal"` | 0x140161a80 | 13 | abstract |  | WRK |
| WorkComponent | `"Component"` | 0x140164be0 | 16 | 0x38 |  | VIS |
| WorkDescription | `"Description"` | 0x140164c60 | 10 | 0x38 |  | K1 |
| WorkPlaceComponent | `"ControllerComponent"` | 0x140164d30 | 18 | 0x68 | +0x28=0x14015f070 +0x28=0x140164ce0 | MI-B |
| WorkPlaceDescription | `"Description"` | 0x140164dc0 | 10 | 0x30 |  | K1 |
| WorkPlaceUI | `"UI::DialogController"` | 0x14015e030 | 11 | 0x68 |  | PNL |
| ZoneComponent | `"Component"` | 0x140164e30 | 16 | 0x28 |  | VIS |
| ZoneDescription | `"Description"` | 0x140164eb0 | 10 | 0x20 |  | K1 |


---

## 2. The hierarchy and the base slot lists

### 2.1 The exe bases (retail `B38_SLOTS.md` §1; names of slots ≥4 are the tree's, the exe has no symbols)

```
Object::Internal (Runtime, real, public ctor) ─┬─ Component 0x140162920 (16, NO pure slot, 0x28)            [PILOT]
                                                │    [0] dtor 0x140031cc0 (restamp + ~Internal: EMPTY body)
                                                │    [1] TypeName [2] Type [3] Persist 0x1401189b0
                                                │    [4] Startup [5] Slot5 [6] OnAdded [7] OnRemove [8] Tick(time) [9] Update(time)
                                                │    [10] PostUpdate [11] Slot11 [12] OnEvent(int) [13] CommandMask(int) → 0 [14] Command(int)
                                                │    [15] OnMessage(int, void*, uint32, uint32)          4-12, 14, 15 = `RET`; 13 = `XOR EAX,EAX`
                                                │  ├─ 39 plain leaves (16 slots)                             CIT 19, VIS 20
                                                │  ├─ StateMachineComponent (16) + member 0x1401644d0 at +0x28   VIS
                                                │  ├─ StatusIconComponent (16) + exe Graphics::Geometry 0x14015f0c0 at +0x28  VIS
                                                │  └─ ControllerComponent 0x140162b80 (18, 0x28, adds NO bytes)                [PILOT]
                                                │       [0] 0x140032800 (EMPTY body) [3] Persist 0x1401195a0 = Component::Persist + 1 empty Serializer
                                                │       [16] GetDefaultState PURE (leaves: return 0/1/2/4)  [17] GetController PURE (leaves: LEA RAX,[RCX+off])
                                                │     ├─ 22 leaves (18 slots) + member at +off           MI-A 13 (+Well done), MI-B 8
                                                │     └─ ResourceProduceComponent 0x1401643f0 (22) + member 0x1401643a0 at +0x28   MI-B
                                                │          [0] own dtor 0x140037d40 [3] 0x14012b4a0 [13] 0x14012b520 [14] 0x14012b5a0
                                                │          [16] return 2 [17] this+0x28 [18] CreateWork PURE [19-21] RET 0x140006b10
                                                │          └─ 6 leaves (22)                               MI-B
                                                ├─ Action 0x14015faa0 (10): [0] 0x140021c90 [3] 0x1400c4b40 [4] PURE [5] Execute→0 [6] Enter [7] Exit [8] CheckEntity [9] GetTarget→false   ACT
                                                │     └─ 17 leaves (10)
                                                ├─ Work 0x140161a80 (13): [0] 0x14002ca60 [3] Persist 0x1400f7400 [4] PURE [5]→false [6-11] PURE [12] RET   WRK
                                                │     ├─ 16 leaves (13)      └─ AutoPickupWork 0x140160fa0 (13): [0],[3] 0x1400e3f80,[8] 0x1400e3f30 own → 6 leaves
                                                ├─ Tool 0x140161f10 (13): [0] 0x14002e9b0 [3] Internal::Persist [4-9] PURE [10]→false [11] 0x140004950 [12]→1   TOOL
                                                │     ├─ EditTool, PathTool (14: slot 13 is THEIR OWN new virtual), PlacementTool, RemovalTool
                                                │     └─ DragTool 0x140161c10 (15): [6] 0x1400fc6b0 OnKey [9] 0x1400fc640 CancelDrag [12]→5 [4,5,7,8,13,14] PURE → 5 leaves
                                                ├─ ToolSetting 0x140161ee8 (4) → RotationSetting                                      TOOL
                                                ├─ ComponentEntity 0x14015fa78 (4: [0] 0x140021a00 [3] Serialize 0x1400c3140)          K1
                                                ├─ TutorialInstance, Achievement::GoalInstance → 2 (7)                                 GOAL
                                                └─ UI::DialogController (Runtime) → 35 + DebugUI + SelectResourceUI 0x14015d330 (14: 11-13 PURE) → 3   PNL
Object::External (Runtime, real, public ctor) ─┬─ Description 0x1401629a0 (9, adds NO bytes)                          [PILOT]
                                                │    [0] 0x140031c70 (EMPTY body) [3] GetFileVersion PURE [4] External::Compile
                                                │    [5] Serialize PURE [6] Reconstruct PURE [7] Destruct PURE
                                                │    [8] GetComponentType PURE — RETSLOT `Object::Type* (Object::Type* ret) const`
                                                │  └─ 78 leaves (10: slot 9 = the leaf's own Construct())      K1 77 (+Well done)
                                                ├─ ComponentDescription 0x140162a00 (9), EnvironmentDescription                         K1
                                                ├─ 12 world externals (LOOP), Tutorial, TutorialStep, TutorialGoal 0x1401607a0 (9: 6-8 PURE) → 4,
                                                │  TutorialImage 0x140160a40 (10) → 3, TutorialSetup 0x140160c30 (11: 6-10 PURE) → 4         GOAL
                                                ├─ Achievement::Goal (Runtime) → BuildingGoal, PopulationGoal, StatGoal, TimeGoal, FinishTutorialGoal (11) GOAL
                                                └─ Achievement::Achievement (Runtime) → Scenario (9)                                    GOAL
UI::DialogInstance (Runtime) → 18 + OptionsDialog + MessageDialog 0x14015c480 (14, 0xc0) → OptionInputBindDialog   DLG
                             → TutorialGoalDialog, TutorialHighlightDialog, TutorialSetupDialog                     GOAL
UI::DialogControllerConfig (Runtime) → 4        PNL      UI::Element / ElementDescription → 2 + 2        VIS
Graphics::AnimationEvent → AnimAudioEvent       VIS      Graphics::Particle / ParticleInstance → Tornado (+instance)   LOOP
Engine::UpdateLoop → GameUpdateLoop (12); Engine::DialogLoopT → MenuDialogLoopT (13);
Engine::DialogLoaderLoopT → splash (13); Engine::LoaderLoop → 2 (12)        LOOP (the four loops are their OWN root, B7-1)
```

### 2.2 The External-leaf triad (read this pass; closes B37_RULES §13 item 6)

Every exe `Object::External` leaf ends its table with the same three bodies, whatever its base:
- **Reconstruct** (External slot 6): `CALL [vptr+0](this, 0)` (destroy in place), then the class's ctor on `this`
  again, returns `this` — e.g. `WellDescription` 0x14000fd90, `BuildingGoal` "Draw" 0x140025e30 (Ghidra's names in
  this position are wrong).
- **Destruct** (External slot 7): the ONE shared body 0x140001640 `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` =
  `DeletingDtor(0)`, in 119 tables.
- **Construct** (the LAST slot, introduced by the leaf): placement-new of the class on `this`; MSVC folded it with
  the class's createInPlace (identical bytes), so B38_SLOTS labels it `*_Cleanup` / `*_CreateInPlace`. Description
  leaves: slot 9 (`WellDescription` 0x14000fd40). **Goal leaves: slot 10** — the "exe-introduced slot 10" B37 left
  open is `Construct` (`BuildingGoal` 0x140025de0 = its createInPlace; `TimeGoal` 0x140026750). `Scenario`: slot 8
  (`Achievement::Achievement`'s `Construct`, B7-1.6).
The C++ spelling: `Destruct() { DeletingDtor(0); }`, `Reconstruct() { this->~X(); return new (this) X(); }`,
`virtual External* Construct() { return new (this) X(); }` (introduced per leaf; `ExternalList` did the same at 8).

### 2.3 The controller member (D6) — the family table

`B38_SLOTS.md` §2 is the per-class table (offset, member table, slot-16 constant); copy it. The interface
(0x14015f070, 9 pure, NO dtor slot) is `ControllerComponent::Controller`:

| slot | name (tree's) | signature | retail body shape (Well 0x14015ae40) |
|---|---|---|---|
| 0 | FindState | `int (int stateId)` | linear search of the class's state table (stride 0xf8), -1 on miss |
| 1 | FindTransition | `int (int stateIndex, int* desiredStateId)` | search the state's transitions; writes -1 to `*desired` on a miss (tree does not — §6) |
| 2 | PostEvent | `char (void* sm, int stateIndex, int eventId)` | events at +0xb0, count +0xf0; `MOV RCX,[RCX+0x18]; JMP handler` |
| 3 | Enter | `char (void* sm, int stateIndex)` | handler column +0x08 |
| 4 | Update | `char (void* sm, int stateIndex)` | column +0x20 |
| 5 | OnTransition | `void (void* sm, int stateIndex, int transitionIndex)` | transition handler |
| 6 | Tick | `char (void* sm, void* time, int stateIndex)` | column +0x10 |
| 7 | TickAlt | `char (void* sm, void* time, int stateIndex)` | column +0x18 |
| 8 | HasStates | `char ()` | `XOR EAX,EAX; CMP [count],EAX; SETNZ AL` (tree: `Stub` → 0, §3 D9) |

Member layout 0x20: +0x00 vptr, +0x08/+0x10 zeroed, +0x18 the owning component. Every slot body tail-calls with
`MOV RCX,[RCX+0x18]`. The tree's `ControllerLive::Dispatcher<Tag>` statics are the behaviour; the pilot's
`ControllerLive::TableController<Tag>` (controller_state_table.h) is the member class whose overrides forward to
them. `ResourceProduceComponent`'s own member (0x1401643a0) serves all six RP leaves (one `RPCtrlTag` table in the
tree — the SAME sharing retail has).

### 2.4 Sizes

`B38_SLOTS.md` §0/§3 carry every reflected `sizeof`; every one the pilot touched agrees with the tree
(`Component` 0x28, `WellComponent` 0x48, `WellDescription` 0x20). Blocks assert every class's size against
§1.3 and every member offset against §2 of B38_SLOTS; a disagreement is a finding, not an edit of the number.

---

## 3. DECISIONS

### D1 — Construction: exe classes get REAL constructors

**Ruling.** An exe class with a compiler vtable needs a real constructor (only a C++ constructor stamps a compiler
vtable). Every concrete exe class gets a public ctor; every exe abstract base a protected one. The body is the
retail ctor MINUS its vptr stores (the compiler emits them, in retail's order: base → … → leaf → members). The
ctor of an `Object::Internal`/`Object::External` subclass runs the base's REAL ctor in its initialiser list, which
is retail's `CALL ??0Internal@Object@@` / `??0External@Object@@`.

**Evidence (pilot).** `Object::Internal()` (internal_class.cpp:369) and `Object::External()` (external_class.cpp:120)
already are real public ctors with retail's bodies (AddObject; path/refs zeroed); `WellComponent()` =
`Component()` → `Internal()` → Base/Internal/Component/ControllerComponent/WellComponent vptr stores → member
`TableController(this)` (member vptr + owner) → `_flags |= 2`, exactly retail 0x14005d060's order. The run placed a
demo well and its state-2 enter (`[Well] water source REGISTERED`) ran through the member's compiler vtable.

**Runtime bases that today have `X() = default` + a ctor DOOR** (`UI::DialogInstance`, `DialogController`,
`DialogControllerConfig`, `Element`, `ElementDescription`, the four Engine loops, `Achievement::{Achievement,
AchievementInstance, Goal, GoalInstance}`, `Graphics::{AnimationEvent, Particle, ParticleInstance, Geometry}`):
the door stamps the MINTED base table for unconverted subclasses, so a derived C++ ctor must not call it (it would
stamp after the compiler's base stamp and, worse, a door reached after the leaf's stamp would overwrite the leaf's
vptr). **Shape (RULE B8-1):**
```cpp
class DialogInstance : public Object::Internal {
protected:
    DialogInstance() { InitFields(); }          // the compiler stamps the vptrs
    ~DialogInstance() { ReleaseFields(); }      // the door's teardown body (D2)
private:
    void InitFields();                          // = the door's body MINUS its vptr stores
    void ReleaseFields();
    friend Object::Base* RtUiDialogInstance_Ctor(void*);   // the door
};
// the door, for unconverted subclasses:  self->vptr = minted; ((DialogInstance*)self)->InitFields();
```
One body, two entry points; the door keeps its registration (exe import slots and unconverted by-address
callers). Done ONCE per base by block RB (§5) before any block derives from that base; RB's unit test constructs a
test subclass on each and pins the stamp order (the `exe_class_abi_test.cpp` PART C/D model).

**Factories.** `CreateNewObject()` = `Object::detail::InternalMemAlloc(sizeof(X), 8)` + `new (p) X()` (retail
create = `MemAlloc(size, 8)` + ctor); `CreateInPlaceObject(p)` = `new (p) X()`. **Retail's createInPlace for every
`Object::Internal`-rooted exe class is the IMPORTED `Internal::CreateInPlaceObject` (`XOR EAX,EAX; RET`)** — retail
cannot build a component/action/work in place. The tree's in-place entries do build one; step 8 KEEPS the tree's
(not a behaviour change of this step; §6 records it). External-rooted leaves have a real createInPlace in retail.
**Copy constructors** (the actions' and works' clone paths, `ActChild_*CopyCtor`, `DialogInstance(const&)` …) become
real `X(const X&)` with the retail body; never assert `is_trivially_copyable` (B7-8). **Hand-rolled init helpers**
(`ComponentFactory::InitInertTyped`/`MakeInertTyped`, `Action_InitInPlace(mem, size, vt)`, `ControllerLive`
factories, `UI_DIALOG_STUB`) die with their last user; a by-address reader still calling one with a converted
class's `Vtable()` must build the same bytes the ctor builds (RULE B8-5).

### D2 — Destruction: `DeletingDtor(unsigned)` per class, chaining; no `virtual ~T()`

**Ruling.** Every exe class with a retail slot-0 body overrides `Object::Base* DeletingDtor(unsigned flags)`:
`this->~X(); if (flags & 1) Object::detail::InternalMemFree(this); return this;` (External-rooted may use the same
free; it is the same `MemoryAllocator` static). `~X()` is protected and NON-virtual; its body is the retail dtor's
non-vptr work; the compiler chains the members' and bases' dtors in retail's order. The flags&2 array arm is not
written (nothing in the exe `new[]`s these). `Object::Destroy(p)` replaces `CallSlot0(p, 1)`.

**Retail chains, always.** Every exe slot 0 read ends in the base dtor (`~Internal`/`~External`/the Runtime base's).
**Where the retail base dtor is EMPTY the chain is behaviour-neutral** — proved this pass: `Component` 0x140031cc0,
`ControllerComponent` 0x140032800, `Description` 0x140031c70 and the controller member (restamp only). So the 66
components and 6 descriptions whose tree slot 0 skipped the chain change behaviour only through their OWN dtor
body. **Where the tree skipped a NON-EMPTY dtor** it is a behaviour change, each in its own titled commit, gated
with resave ×5 (RULE B8-3): the 38 `DialogController` subclasses (`~DialogController` never runs today — PNL), the
MenuDLT / splash / two LoaderLoop slot 0s (LOOP), `TutorialHighlightDialog`, `TutorialGoalDialog`,
`TutorialSetupDialog` (GOAL; guarded `~Internal` instead of `~External`), `GeomDraw_Dtor` (EMPTY in the tree,
retail 0x14001ecc0 walks the SoA arrays — LOOP), every component/description/action/work whose own retail dtor frees
members the tree leaks (the leaf's block lists them), the 12 `UI_DIALOG_STUB` tables (deleted, D4).

### D3 — Inert / stub / null / past-end slots

A compiler vtable has exactly the declared virtuals, so:
1. **Slot retail fills with the BASE's body** (`= parent` in B38_SLOTS): NOT declared in the leaf — it inherits the
   base's C++ body, which is retail's base body. Where that base body is trivial (`RET`, `return 0/1/5`) the tree's
   inert stub was equivalent and nothing changes. Where it is a real Runtime body (an `IMPORT` row:
   `External::Compile`, `DialogInstance::OnUpdateLayout/OnUpdate/Persist/OnSerializeSettings/OnClosed/OnElementEvent`,
   `DialogController::OnOpened/OnClosed/OnSerializeSettings`, `Internal::Persist`) inheriting turns ON behaviour the
   tree's nop dropped: `TutorialHighlightDialog` 3/5/8/9, the tutorial dialogs' slot 4, and the External tables whose
   slots 4/7 are `nullptr` today (15, INVENTORY_B §1.3) — each family is ONE titled behaviour-change commit.
2. **Own slot retail implements, tree stubs** (PORT GAP): declare the override, keep the TREE's body, mark it
   `// PORT GAP (retail 0x…) — B38 D9`; porting the retail body is a separate titled commit (pilot:
   `WellDescription::Reconstruct/Destruct`).
3. **Pure slots**: `= 0` on the base; `_purecall` is never reached; a leaf that retail leaves pure is abstract.
4. **Null slots** (Goal-family slot 1, `FinishTutorialGoal` 1/4/7, `Scenario` 1/4/7, the External families' 1/4/7):
   there are no nulls in a compiler table — slot 1 is `DECLARE_EXE_TYPE`'s `TypeName`, 4 the inherited `Compile`,
   7 `Destruct` (§2.2). A caller that relied on a null is a bug to report.
5. **Past retail's end — deleted, not kept:** the 48-slot component padding; the 11th Description slot; the
   hand-built Dispatcher's slots 9-15 (pilot); Scenario 9-12; `TimeGoal`/`FinishTutorialGoal` slot 11 (it is the
   adjacent GoalInstance table); MainMenuDialog/MapParamsDialog 10-19; the four world externals' 9-10;
   `FrustumOutlineElement`'s 24-entry copy; and **Attendance/CropField/Orchard/Pasture slots 18-21, which hold the
   DESCRIPTION's slots** (`ControllerLive_ScalarDeletingDtor` stamps 0x1401629a0). The owning block greps its
   files for a dispatch past retail's count (`vt[18..]`, `+0x90..`) BEFORE deleting; one found is a bug report.
6. **Shorter than retail: `EditTool`, `PathTool` have 14** (slot 13 = their own new virtual, `EditTool` 0x14002daf0,
   `PathTool` 0x140026b60); the tree's 13-slot tables read the next global on a slot-13 dispatch. TOOL declares it.
7. Slots 1/2 answer the class's own name/id (retail's) where the tree answered null/0 (49 components, 82
   descriptions): part of the conversion, not separately titled; a slot-1 CALLER found in a block is reported.

### D4 — Reflection: `DECLARE_EXE_TYPE` / `DEFINE_EXE_TYPE` over the FactoryRegistration list

**Ruling (pilot, `code/Game/Object/exe_type.h`).** Step 8 does NOT move the exe classes to the Runtime's
`DECLARE_TYPE`/`DEFINE_TYPE` (a CRT-init `BaseTypeInfo` static): that would change WHEN and in WHAT ORDER each record
enters `BaseTypeInfo::_firstTypeInfo`, which the pointer-free type table observes. Instead the class OWNS its
`Object::FactoryRegistration` (`DEFINE_EXE_TYPE(Class, "Parent")` placed where the file's `REGISTER_TYPE` line was:
same literal, same list, same TU position, so the registration order is unchanged). `DECLARE_EXE_TYPE` gives slot 1
`TypeName`, slot 2 `Type`, static `TypeId()` (a plain read of the record — replaces the lazy per-class
`static uint32_t cached; if (!cached) cached = GetTypeIdByName("X")`), `CreateNewObject`, `CreateInPlaceObject`,
`Vtable()`. The END of B.3 may fold the list into `DEFINE_TYPE` in one pass once the type table is proved
order-independent (§6).
- **Name literal** = `#Class`, bare, exactly §1.3's (310/310 already agree). **Parent literal** = §1.3's EXACTLY. The
  one tree disagreement: `OptionsDialog` registers `"UI::DialogInstance"`, retail `"public UI::DialogInstance"` —
  retail's `StripNamespace` reduces both to `DialogInstance`; DLG adopts retail's spelling and checks the type table
  gate (§6). `DebugUI` `"DialogController"` and `Scenario` `"::Achievement::Achievement"` already match.
- **Abstract bases** (the 13) keep `REGISTER_ABSTRACT_FACTORY` in `factory_registrations_bulk.cpp` this step (moving
  a row moves its record); their `Type()`/`TypeName()` answer through `GetTypeIdByName("X")` / the literal (pilot:
  `Component`, `ControllerComponent`, `Description`) — only a half-built object dispatches there.
- **`AutoSaveDialog`'s duplicate registration**: the `UI_DIALOG_STUB(AutoSaveDialog, …)` row
  (ui_dialog_stub_factories.cpp:1672) is deleted by DLG (the real class keeps `autosavedialog.cpp:284`'s). Which of
  the two the factory keeps today depends on static-init order: gate the deletion (type table + Load Game route).
- **The 8 dormant `UI_DIALOG_STUB` DialogInstance twins** (Achievement, Follow, Goal, MapParams, ModUpload,
  Scenario, SaveLoad, Toolbar) and the 3 DialogController-only ones (ModBrowseUI, NoiseUI, SpriteCaptureUI) are
  the ONLY registration of their names: the reflected factory builds a 48-nop object while the game's hand path
  builds the real one. Converting the class replaces the stub row with `DEFINE_EXE_TYPE` on the real class — the
  factory then builds the REAL object: a behaviour change per class (titled; the reflected-create path is the
  save/load one, so resave ×5).
- **The 1,301 lazy caches (0D §2.2):** a block converts the 370 per-class `GetTypeId` caches of ITS classes to
  `X::TypeId()` and every other lazy static/once-flag IN THE BODIES IT MOVES into a class (B6-5: namespace-scope
  resolve at registration, or a class static). Caches in caller files it does not convert, the 88 `g_tid_*` data
  cells and `engine_funcs.cpp`'s 54 type-id reads are OUT of step 8 (END of B.3).

### D5 — The RegisterExeAddr seam during the migration

**Ruling.** A converted class whose retail vtable address is still READ BACK by another file keeps
`RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(X::Vtable()))` — the exe address now serves the COMPILER's
vtable, so every unconverted by-address reader (552 sites, 183 tables, 0D §1.3) stamps a table whose slots are the
class's. `X::Vtable()` is `DEFINE_EXE_VTABLE(X, "_ZTV<len>X")`: the vtable group's address point read through the
linker (a misspelt symbol is an undefined reference, never a wrong pointer); unit test PART C proves
`Vtable() == *(void**)&obj` on this toolchain. A table with NO by-address reader loses its registration with the
table (the pilot kept `WellDescription`'s reader-less row once, as the exemplar; the closing pass deletes it).
**Because the seam serves the compiler table, the cross-block order constraints of 0D §4.1 disappear**: a block
never waits for another block's readers; the LAST converter of a table's readers deletes the registration.
- **Slot-body doors reached by address** (0D's (b) rows, 14): stay registered as one-line doors delegating to the
  member with a QUALIFIED call — `Component_SerializeReimpl` (52 callers; pilot: `Component::Persist` delegates to it
  until K1 folds it), `WorkBase_Serialize_f7400` (22), the two `DragTool` rows, the rest per 0D §1.2.
  0D's (a) rows (75: reached only through a table) die with the table.
- **The slot-copy sites — SIX files, seven tables** (0D found five, 0A a sixth): `tool_base_toolui.cpp:142-146`
  (TOOL), `frustumoutlineelement.cpp:229-237` (VIS), `thumb_vtables.cpp:69-77` (DLG), `dialog_controller_vtable.cpp`
  `Apply()` (PNL), `tornado.cpp:595-654` (LOOP), `statusicon_geometry.cpp:308` (VIS, copies `??_7Geometry@Graphics`).
  Each goes when its class becomes a real subclass of the real Runtime base; each is on the END-of-B.3 critical
  path (D10).
- **Identity compares by vtable** (4, all by name: `tornado.cpp:636`, `standalone/main.cpp:781`,
  `pausemenu_harness.cpp:227,240`): converted to `Type()`/`IsDerivedFrom`, or to `X::Vtable()` where the harness
  truly asks "is this the class".

### D6 — The controller member

**Ruling (pilot).** `ControllerComponent` is a single-inheritance class that adds no bytes; each leaf embeds its
controller BY VALUE at retail's offset, AFTER the leaf's own leading fields when retail's offset says so
(Attendance +0x38, Happiness +0x30, Pasture +0x48, Nomad +0x58, Boat +0x78, Trade +0x88):
```cpp
class WellComponent final : public ControllerComponent {
    DECLARE_EXE_TYPE(WellComponent)
public:
    int         GetDefaultState() const override { return 2; }          // [16] per-class constant
    Controller* GetController() override { return &_controller; }       // [17] LEA RAX,[RCX+0x28]
    ControllerLive::TableController<WellCtrlTag> _controller;           // +0x28, asserted
};
static_assert(offsetof(WellComponent, _controller) == 0x28, "…");
```
The member class derives `ControllerComponent::Controller` (9 pure slots, no dtor slot). The table-driven
`TableController<Tag>` serves every controller whose member table is the generic dispatcher over a per-class state
table (all 24 in retail follow that shape; each retail table hard-codes its own state-table base, the tree's
`Dispatcher<Tag>::table` is that base). The state table is populated at STATIC-INIT time as retail's
`0x14014xxxx` initialisers do (pilot: `s_wellStateTableBuilt`), not per create from two threads. Unit tests: size,
offset, back-pointer at +off+0x18, slot order via PMFs, raw `[vptr+0x18](node, sm, idx)` dispatch reaching the
handler with RCX = owner (PART D). AbsorbResource, MarketLocation, MultiObject have NO member in the tree today
(slots 16/17 inert): their block adds it (retail has one, §2 of B38_SLOTS) — a behaviour change, titled.
`StateMachineComponent` (VIS) embeds a controller-interface member too (0x1401644d0); `StatusIconComponent` (VIS)
embeds an exe `Graphics::Geometry` subclass (0x14015f0c0) — a real `Graphics::Geometry` subclass member.

### D7 — Threading

Components, actions, works, descriptions and the world externals are constructed on BOTH the loader thread (world
build, save load: `ComponentEntity_CtorFromDescription`, `EntityList_Serialize`, `ExternalList::Find`) and the main
thread (play). B6-5 applies to every member a block converts: no function-local lazy static, no first-use fill,
in a member or in anything it calls in the block's files — resolve at namespace scope at registration time, or make
it a class static initialised before the loader starts. The per-create vtable fills (`FillTypedVtable`,
`*_FinishLiveVtable`, `EnsureVtable`, the three lazy slot-copy sites) disappear WITH the compiler vtable — that is
most of the B6-5 surface. Per-class state tables are built at static init (D6). No lock is added anywhere
(B7-7.3). The two atomics (`gameupdateloop_load.cpp:895`, `:922`) stay.

### D8 — Loops, `RtEngine::` PODs, Goal family, Scenario

- **Loops (LOOP):** `GameUpdateLoop : Engine::UpdateLoop` (12), `MenuDialogLoopT : Engine::DialogLoopT` (13), the
  splash `: Engine::DialogLoaderLoopT` (13), two `: Engine::LoaderLoop` (12) — each a real class with retail's
  overrides (B37_RULES §3.1 lists exactly which slots each overrides and which it inherits). The 35 standalone
  `Stub_*` slots that re-implement a base body become INHERITED (D3.1; the Runtime base bodies are the ported
  ones). The four skipped base dtors run (D2, titled). The engine-side loop headers (`engine/types/{UpdateLoop,
  LoaderLoop,DialogLoaderLoopT,MainLoop}.h`) become aliases/forwarders and the `RtEngine::` PODs' 95 `m_` uses in 7
  files are renamed to the class members in the same block (B7-6 carry-over 2). `UpdateLoopVtable` dies.
- **Goal family (GOAL):** 5 `Goal` leaves (11 slots: slot 1 TypeName, 4 Compile, 7 Destruct, 9 `GetInstanceType`
  inherited except `TimeGoal`/`FinishTutorialGoal` which override it as the B7-4 RETSLOT shape, 10 `Construct`),
  2 `GoalInstance` leaves (7). The null slots 1/4/7 get retail's bodies by construction (D3.4) — titled.
  `Goal_FillInheritedVtableSlots` (goal_exe_helpers.cpp:210-235) dies.
- **Scenario (GOAL):** `: Achievement::Achievement`, 9 slots (the tree's 13 lose 9-12), parent literal
  `"::Achievement::Achievement"`, slot 8 `Construct`. The three QUALIFIED base calls (`scenario.cpp` dtor,
  `GetFileVersion`, `Serialize`) stay qualified.
- **`AchievementInstance`** (0x14015e350, built in `GameUpdateLoop::Load`, loader thread): an exe-inlined copy of
  the RUNTIME class — LOOP constructs `Achievement::AchievementInstance` (made instantiable by RB) instead of an
  exe table; the never-registered row question disappears.
- The tutorial `TutorialGoal`/`TutorialSetup` bases are real abstract classes (GOAL); `reg_tutorial_wave.cpp`'s
  registration of those two addresses onto the DIALOG tables (INVENTORY_B §1.14) is deleted with them.

### D9 — Retail-proved port bugs: each in its own titled behaviour-change commit in the owning block

| # | bug (retail evidence) | owner |
|---|---|---|
| 1 | **LIVE WRONG STAMP.** `code/Game/Action/createnaturalresource.cpp:155` and `createrawmaterial.cpp:226` stamp `ResolveAddr(Exe, 0x14015f700)` as "the Action base vtable" in their scalar dtors; 0x14015f700 is slot 6 of `ResidenceUI`'s table 0x14015f6d0. Retail stamps **0x14015faa0** (`CreateNaturalResource_ScalarDtor` 0x140022240 store at 0x14002226e; `CreateRawMaterial_ScalarDtor` 0x140022350 at 0x14002237e). The compiler dtor chain fixes it; if ACT lands the fix before converting, it is `Action::Vtable()` / 0x14015faa0 | ACT, first commit |
| 2 | `0x14015faa0` (the `Action` base table) is registered to `&g_ComponentEntity_StandaloneVtable[5]` (address_registry.cpp:909): only 7 of Action's 10 slots exist there and slot 2 answers ComponentEntity's type id; 19 files stamp it into fresh actions | ACT (register `Action::Vtable()`), K1 (ComponentEntity's own 4-slot table) |
| 3 | `Tool` 0x140161f10 and `DragTool` 0x140161c10 are UNREGISTERED: every tool dtor's base restamp writes a NULL vptr (9 + 6 sites) | TOOL (the compiler chain) |
| 4 | `EditTool`/`PathTool` tables are 13 slots; retail 14 | TOOL |
| 5 | `FinishTutorialGoal` slots 1/4/7 null; every Goal/External family slot 1 null; 15 External tables 4/7 null | GOAL, LOOP |
| 6 | `TutorialHighlightDialog` drops DialogInstance 3/5/8/9; the tutorial factories skip the base ctor (TutorialUI, TutorialGoalDialog, TutorialSetupDialog, TutorialHighlightDialog) | GOAL |
| 7 | 38 `DialogController` subclasses never run `~DialogController` | PNL |
| 8 | 66 components' `Inert_ScalarDtor` runs only `~Internal` (behaviour-neutral for the bases, D2); AbsorbResource / MarketLocation / MultiObject have no controller member and inert 16/17 | K1 (retire the helper), MI-A/MI-B (the three members) |
| 9 | controller member slot 8: retail `count != 0`, tree 0 (pilot kept 0); slot 1 writes -1 to `*desired` on a miss in retail, not in the tree | MI-A (with `TableController`, once, for all) |
| 10 | Description leaves' Reconstruct/Destruct are `return 0` stubs (retail: destroy+reconstruct / `DeletingDtor(0)`) | K1 |
| 11 | `FrustumOutlineElement`'s copy reads 12 entries past the Runtime Element table | VIS |
| 12 | `GeomDraw_Dtor` is EMPTY (retail 0x14001ecc0 frees the SoA arrays); the `Video::ImageBuffer` copy puts the COMPLETE dtor in slot 0 (a deleting call never frees) | LOOP, DLG |
| 13 | `AutoSaveDialog` registered twice; 11 `UI_DIALOG_STUB` names build a 48-nop object from the reflected factory | DLG, PNL |
| 14 | `engine/types/Component.h`'s slot list and `+0x08 next` were wrong; `ControllerComponent.h`'s fixed +0x28 sub-object was wrong (both now forwarders, pilot); `Dialogs.h:23`'s `SecondaryVtable = 0x14015b070` is AttendanceUI's table (zero readers — delete); `::TutorialSetupDialog` is defined twice (Dialogs.h:464 vs TutorialSetups.h:89 — a live ODR violation; GOAL keeps one) | pilot / DLG / GOAL |
| 15 | the tree's by-address readers of UNREGISTERED tables get 0 (FollowDialog, ToolbarDialog, ToolUI child, SpriteSheet/MaterialInstance copies, ModUploadDialog, SkyMesh MeshInstance, AchievementInstance, Pathfinder) — INVENTORY_B §1.11 | the table's block (each reader becomes the real class or `X::Vtable()`) |

### D10 — Order of conversion, and the END-of-B.3 retirement per class

**Within a block, per class:** (1) the class header (retail slot order, asserts, `DECLARE_EXE_TYPE`); (2) real
ctor/dtor/`DeletingDtor`; bodies moved into members (`self->x` → `x`), the old C entry points become one-line
doors ONLY where 0D §1.2 says they are reached by address; (3) `DEFINE_EXE_TYPE` replaces `REGISTER_TYPE` in place;
(4) the hand-built table, its fill helpers and its `Register*Vtable` go; (5) its `RegisterExeAddr(kVtable)` row is
kept serving `X::Vtable()` iff a by-address reader remains (D5), else deleted; (6) the class's lazy caches → class
statics (D4); (7) the POD in `engine/types` becomes a forwarder to the class header (never two definitions of one
name, B8-4); (8) unit test; static gate; each behaviour change its own titled commit.

**Across blocks:** RB before any block that derives a Runtime base with a door (DLG, PNL, LOOP, VIS, GOAL); the
pilot before everything; otherwise independent (D5). Waves in §5.

**END of B.3 (its own closing step, after step 8), per exe class X:** (a) X's slot-copy site gone (D5, six sites);
(b) every by-address reader of X's retail vtable converted → X's `RegisterExeAddr(kVtable)` deleted; (c) X's
`g_tid_X` type-id cell (88 `.data` rows) deleted when no `ResolveAddr(Exe, 0x1401c….)` reader remains; (d) the
FactoryRegistration list folds into `DEFINE_TYPE` if the type table is order-independent. **Per Runtime base B**
(0D §4.3's table is the list): B's mint vtable row retires when no exe class stamps or copies B's minted table (every
subclass converted, B's copy sites gone, no `ResolveExport("??_7B…")`/`RT_VTABLE_ONCE` left in `code/Game` +
`standalone/`); B's `RT_DELDTOR` row when every subclass has a compiler `DeletingDtor` and no deleting-dtor dispatch
reaches the row; B's `_typeInfo` row when no exe reader resolves `?_typeInfo@B…`. Order: slot-copy sites →
subclasses → exe vtable registrations → `g_tid_*` → mint vt rows → `RT_DELDTOR` rows → `_typeInfo` rows →
`Object::Base` last; then the singleton storage (carry-over 5) and the normal-virtual cutover (`DeletingDtor` →
`virtual ~T()`, `Destroy` → `delete`) in one mechanical pass once no hand-built table exists anywhere.

---

## 4. The rules, collected

| # | rule | § |
|---|---|---|
| B8-1 | Exe classes get REAL ctors (public on concrete, protected on abstract): retail's body minus vptr stores; the base's real ctor in the init list. A Runtime base with a door gets `X() { InitFields(); }` sharing ONE body with its door (RB does this once per base); a converted object is never built through a door | D1 |
| B8-2 | Slot 0 is `Object::Base* DeletingDtor(unsigned) override` on every exe class with a retail slot-0 body: `~X()` (protected, non-virtual) then `InternalMemFree` on `&1`. No `virtual ~T()`. `Object::Destroy(p)` replaces `CallSlot0(p,1)` | D2 |
| B8-3 | A class whose tree slot 0 skipped a NON-EMPTY base dtor (list D2), whose null/nop slots become retail's base bodies (D3.1, D3.4), whose reflected factory changes from a stub to the real class (D4), or whose PORT GAP body is transcribed: each is its OWN commit titled `… (behaviour: …)`, gated with resave ×5 | D2-D4, D9 |
| B8-4 | One definition per name: the `engine/types` POD of a converted class becomes a forwarder to `code/Game/<Dir>/<Class>.h` in the same commit (pilot: `Component.h`, `ControllerComponent.h`, `WellComponent.h`); shared POD headers (`Dialogs.h`, `Panels.h`, `ui/DialogInstance.h`, `ui/DialogController.h`) are coordinator-owned: a leaf block authors the per-class header and removes ITS struct from the shared header in a coordinator-applied hunk (B6-4.4) | §5, D10 |
| B8-5 | A by-address reader that still stamps a converted class's table (`X::Vtable()` served at the exe address, D5) must build the same bytes the class's ctor builds, including the `Object::Internal` table entry; the converting block checks each of its classes' readers (0D §1.3) | D1, D5 |
| B8-6 | Slot lists come from `B38_SLOTS.md` §6, names of slots ≥4 from the tree; inherit every `= parent` slot; declare past-retail slots nowhere; grep for dispatches past retail's count before deleting padding | D3 |
| B8-7 | `DECLARE_EXE_TYPE`/`DEFINE_EXE_TYPE` (exe_type.h), placed where `REGISTER_TYPE` was; literals from §1.3 EXACTLY; abstract bases keep `REGISTER_ABSTRACT_FACTORY` this step | D4 |
| B8-8 | Keep `RegisterExeAddr(kVtable, X::Vtable())` while a by-address reader exists; the last converter of the readers deletes it. Slot-body doors reached by address stay one-line QUALIFIED delegations | D5 |
| B8-9 | The ControllerComponent family has NO multiple inheritance: the controller is a member at retail's offset (asserted), slot 17 returns its address, slot 16 the class constant; state tables built at static init | D6, §0.1 |
| B8-10 | B6-5 for every member a block creates; no per-create fills survive; no lock added | D7 |
| B8-11 | External leaves: `Reconstruct` / `Destruct` / `Construct` triad (§2.2); Description slot 8 and `Goal::GetInstanceType` are RETSLOT (`Object::Type* M(Object::Type* ret) const`) | §2.2, D8 |
| B8-12 | The by-address reader list of a class is swept BY DEMANGLED OWNER and by hex (B37 §14.6), including `FooAddr::Vtable` constants and `engine_funcs.cpp`'s reads | D5 |
| B8-13 | Retail's createInPlace for `Object::Internal`-rooted exe classes returns null; the tree's in-place construction is KEPT this step (recorded, not changed) | D1 |
| B8-14 | Tests: per block one `tests/unit/<block>_abi_test.cpp` (the `exe_class_abi_test.cpp` model): sizeof, member offsets, slot order via PMF (adjustment must be 0), abstractness, no virtual dtor, one raw retail-order call per retslot/hidden-pointer member | §7 |

---

## 5. The blocks — FINAL

File sets: `B38_FILEMAP.md` (every `code/Game` file exactly once; 731 files, 0 unassigned; the pilot's new files
assigned). Coordinator-owned files and the lines each block needs in them: `B38_FILEMAP.md` §2.

| wave | block | classes | needs | rules / notes |
|---|---|---|---|---|
| 0 | **PILOT** (done, `b38-rules`) | `Component`, `ControllerComponent` (+ `Controller`), `Description`, `WellComponent`, `WellDescription`; `exe_type.h`; `TableController<Tag>` | — | §7 |
| 1 | **RB** Runtime bases (one owner) | the D1 `InitFields` split + concrete `DeletingDtor` where the exe instantiates the Runtime class directly: `UI::{DialogInstance, DialogController, DialogControllerConfig, Element, ElementDescription}`, `Engine::{UpdateLoop, LoaderLoop, DialogLoopT, DialogLoaderLoopT}`, `Achievement::{Achievement, AchievementInstance, Goal, GoalInstance}`, `Graphics::{AnimationEvent, Particle, ParticleInstance, Geometry, MeshInstance, SunLight, FrameCopyImageBuffer}`, `UI::{SpriteSheet, SpriteCapture}`, `Graphics::MaterialInstance`, `Video::ImageBuffer` | pilot | runtime/ files only (FILEMAP §1 RB); unit test per base (stamp order through a test subclass); no `code/Game` edit |
| 1 | **K1** descriptions + entity | 77 `Description` leaves, `ComponentDescription`, `EnvironmentDescription`, `CaptureBox`/`FrustumOutlineDescription` (parent `ElementDescription` — needs RB's ElementDescription: do those two LAST or hand them to VIS), `ComponentEntity`; moves `Description`'s members from welldescription.cpp to `description.cpp`; folds `Component_SerializeReimpl` into `Component::Persist`; retires `component_factory_base.*`/`controller_live_base.*` when their last user converts (with CIT/VIS/MI, coordinator at close) | pilot | D9 #2 (ComponentEntity's table), #10 (triad, titled); B8-11 |
| 1 | **ACT** actions | `Action` + 17 | pilot | D9 #1 FIRST (titled), #2; B8-5 for the 19 stampers of 0x14015faa0 and the WRK/CIT readers |
| 1 | **WRK** works | `Work`, `AutoPickupWork` + 21 | pilot | `WorkBase_Serialize_f7400` door stays (22 callers); 8 `ADD RCX,imm; JMP` member forwarders in Work tables are member calls, not thunks |
| 1 | **TOOL** tools | `Tool`, `DragTool`, `ToolSetting` + 9 (`RotationSetting`) | pilot | D9 #3, #4; the ToolUI child DialogInstance copy (`tool_base_toolui.cpp`) waits for RB and is TOOL's last commit; `DragTool::OnKey`/`CancelDrag` doors stay |
| 1 | **CIT** citizens/nature/animals | 19 `Component` leaves (FILEMAP) | pilot | the particle/tornado COMPONENTS only (the `Tornado` Particle is LOOP) |
| 2 | **DLG** dialogs | 19 (`MessageDialog` → `OptionInputBindDialog`, `OptionsDialog` literal) | RB | D4 (`AutoSaveDialog`, stub twins), D9 #12 (ImageBuffer copy), #13; `thumb_vtables.cpp` copy site |
| 2 | **PNL** panels | 42 (`SelectResourceUI` → 3, `DebugUI`, 4 configs) | RB | D2 (`~DialogController`, titled), `dialog_controller_vtable.cpp` `Apply()` copy site, 11 `UI_DIALOG_STUB` DC names |
| 2 | **LOOP** loops + world externals | 5 loop tables, 12 reflected (`ClimateGenerator` … `Weather`, `Tornado`), the 5 unreflected exe Runtime subclasses (pathfinder `Thread`, geometry 0x14015f110 / 0x140160240, Tornado `ParticleInstance`), the inlined Runtime copies it builds (SkyMesh, Sun, AchievementInstance) | RB | D8; `RtEngine::` POD rename (95 `m_` uses); `tornado.cpp` copy site; D9 #12 (GeomDraw) |
| 2 | **VIS** presentation components + UI/Graphics strays | 20 `Component` leaves incl. `StateMachineComponent` (member) and `StatusIconComponent` (Geometry member), `CaptureBoxElement`, `FrustumOutlineElement`, `AnimAudioEvent` | RB (for the 3 strays) | D6 (two non-controller members); `frustumoutlineelement.cpp` + `statusicon_geometry.cpp` copy sites; D9 #11 |
| 2 | **MI-A** controllers (town) | 13 `ControllerComponent` leaves | pilot | D6; D9 #8 (AbsorbResource member), #9 (`TableController` slot 8/slot 1, once, titled) |
| 3 | **GOAL** goals, tutorial, scenario | 27 (5 Goal + 2 GoalInstance + Scenario + 19 tutorial incl. 3 dialogs and `TutorialUI`) | RB, DLG, PNL merged | D8; D9 #5, #6, #14 (`TutorialSetupDialog` ODR) |
| 3 | **MI-B** controllers (production/trade) | `ResourceProduceComponent` + 6 RP leaves + 8 `ControllerComponent` leaves | MI-A merged (`TableController` fixes) | D6 (shared RP member), D9 #8 (MarketLocation, MultiObject members); slots 18-21 of CropField/Orchard/Pasture (D3.5) |
| 4 | fragments | coordinator-applied hunks in shared headers (`Dialogs.h`, `Panels.h`, the multi-block `reg_*.cpp`, `address_registry.cpp`, `factory_registrations_bulk.cpp`) | per merge | B8-4 |
| 4-5 | caller batches, by directory | converted-member calls replacing `ResolveAddr(Exe, …)`, raw slot calls, `engine_funcs` doors onto converted classes | all blocks | B8-12; `// B.3.8 bridge` casts marked |
| 6 | close | factory macros and helpers deleted; `engine/types` forwarders deleted where no includer spells them; numbers (`b37_session/CLOSE_NUMBERS.md` model) | | |

Every block: its own worktree branch; static gate per commit; NO game run by the block (the coordinator gates each
merge — game dir starves past three parties); forbidden: `git stash`/amend/rebase/push, sub-agents that edit,
`pacman`, unscoped searches, `getenv`, hand-edited `.inc`, null-check fixes of faithful crashes.

---

## 6. NOT VERIFIED

1. **Argument lists of Component slots 8, 9, 12-15 and of the controller member's slots** are the tree's C bodies'
   parameter lists, not read from retail callers (the pilot declares them; nothing the pilot converted uses them).
   Each block reads the caller (`EntityList` bucket ticks, `StateMachine::Tick` 0x1400a9660, `Command` from the
   panels) before relying on an argument. In particular whether `Tick`/`Update` receive a `System::Time` by value in
   a register (B7-5) or a pointer.
2. **The type table's order sensitivity.** D4 keeps the FactoryRegistration order to avoid the question; nobody
   measured whether reordering changes the pointer-free type table.
3. **`OptionsDialog`'s parent literal** — that `"public UI::DialogInstance"` and `"UI::DialogInstance"` strip to the
   same parent in the Runtime's `StripNamespace` is read from the ported body's comment, not re-run; DLG gates it.
4. **The controller member's slot 1 miss-arm** (`MOV dword [R8],-1`) was decoded from Well's member only (0x1400100c0);
   the other 23 member tables are assumed to share the generic body (B38_SLOTS: all 9 slots OWN per table, not
   compared byte for byte).
5. **`Destruct`'s shared body 0x140001640** is proved for the 119 tables that point at it; a leaf with its own slot 7
   body was not looked for.
6. **Which Runtime classes the 11 inlined copies need concrete** (RB): the list is B38_SLOTS §4's; 0x14015e290 and
   0x14015ecb0 were unidentified by 0B and named `SunLight` / `ParticleSystemInstance` by 0C from their imports only.
7. **Loader-thread reach through vtable dispatch** (0D §5): the component/description factory paths are INFERRED.
8. **This pass's Ghidra side effect:** one `disassemble_bytes` call at 0x140001640 (length 8) ran against the
   CURRENT program, which was `Runtime-x64.dll` (the tool takes no program argument) — an address outside that
   image; no rename, no annotation, nothing saved. Every other read passed `program=Application-x64.exe`; the bridge
   was never switched and stayed on `Runtime-x64.dll`.
9. **The pilot's runtime proof of D2 on a freed WellComponent** (demolish): the demo well was built, registered and
   the game quit; no well was demolished in the run, so `DeletingDtor` ran only at teardown if at all.

---

## 7. The pilot (branch `b38-rules`)

**Classes.** `Component` (16), `ControllerComponent` (18) + `ControllerComponent::Controller` (9, the interface),
`Description` (9), `WellComponent` (18, controller member at +0x28, 0x48), `WellDescription` (10, 0x20). Headers in
`code/Game/Component/` (the tree mirrors the exe's own directory layout: the header lives with its `.cpp`, named
after the class — decided here; the `engine/types` POD becomes a forwarder, B8-4). Members of the two bases in
`code/Game/Component/component.cpp`; `Description`'s in `welldescription.cpp` until K1 gives it `description.cpp`.
`code/Game/Object/exe_type.h`: `DECLARE_EXE_TYPE`, `DEFINE_EXE_TYPE`, `DEFINE_EXE_VTABLE`.
`controller_state_table.h`: `ControllerLive::TableController<Tag>`.

**Deleted:** `WellComponent_LiveVtable[48]`, its `FinishLiveVtable`/`ShutdownVSlot`/`OnRemoveVSlot`/
`GetDefaultState`/`CreateState` thunks and per-create fills, the `MakeInertTyped` factory; `g_WellDescription_Vtable[11]`
and its stubs; the three POD structs (`Component`, `ControllerComponent`, `WellComponent`).

**What it proved:** D1 (real ctor chains on the real `Object::Internal`/`External`; stamp order = retail's), D2
(`DeletingDtor` + protected dtor chains, no virtual dtor — `std::has_virtual_destructor` false for all), D4 (the
class owns its registration; the new-game `[ExtLoad] calls=5686` and the resave are unchanged, i.e. no type/record
drift), D5 (`Vtable()` == the object's vptr; `WellDescription`'s registration serves it; linker-checked), D6 (member
at +0x28, slot 17 → member, raw dispatch through the member's compiler vtable reaches the state handler with RCX =
owner — in the unit test AND live: `BANISHED_BUILD_DEMO_WELL=1` run logged `[Well] water source REGISTERED: slot=0
tile=(188,193)`, i.e. StateMachine → slot 17 → `TableController::Enter` → `WellComponent_EnterBuilt`).

**Gate results (verbatim):**
- `mingw32-make -j12 standalone-debug` EXIT 0 (no new warning in the recompiled set vs the baseline build);
  `standalone-perf` EXIT 0; `make unit` → `523 tests, 0 failures` (516 + 7 new in `exe_class_abi_test.cpp`).
- `dispatch_slot_map.py --check`: `checked 109 ResolveExport registrations, 0 name mismatch(es); 0 address(es) registered more than once`.
- `cutover_manifest.py --gate`: `cutover manifest gate: OK (59 rows; row set and every disposition match a fresh derivation)`.
- `rt_mint_gen.py --check`: `rt_data_mint_tables.inc is UP TO DATE (152 vtables, 104 BaseTypeInfo, 48 storage)`
  (then `git checkout -- standalone/rt_export_addrs.inc`).
- Runtime, new game + pause-menu quit (`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`):
  `BOOT_STATE_EXIT 0`, `EXE_EXIT 0`, crash log empty, no hang log, `[RtResolve] 0 misses`, 0 `[RtMint] TRAP`,
  `[ExtLoad] calls=5686 openFileFail=3`. Same with `BANISHED_BUILD_DEMO_WELL=1` (+ the `[Well]` line above).
- `tools/resave_gate/resave_gate.sh Gate`: exit 0, crash log 0 bytes; the worktree has no golden `.bin` (untracked),
  so the script cut a "NEW-GOLDEN" — its md5 is `757e48a15eac71eff097508da169c23c`, IDENTICAL to the tracked
  `golden/Gate.resaved.md5`, and `cmp` against main's `golden/Gate.resaved.bin` is identical: **PASS-equivalent**
  (the worktree golden was removed and the md5 file restored afterwards).

---

## 8. Coordinator's dated corrections from the blocks (2026-09-26, recorded by `b38-close`)

Each item names the block that found it. Where an item contradicts §0–§7 above, the item wins; the text above is
kept as the question pass wrote it.

> **ENDGAME B.1 decision 2 (question pass, §0.1)** — already corrected above: no multiple inheritance and no RTTI
> for game classes in the exe; the "second vptr" is an embedded member at a per-class offset. `ENDGAME.md` carries
> the one-line pointer (096d77eb).

> **D1's door list (block RB, fcadaf25 / 8ad21574 / 3cb5a0b7 series).** Three Runtime bases that D1 lists "with a
> ctor DOOR" have **no ctor door**: `UI::DialogController` (retail ctor 0x1800cd320: three zero stores, +0x28
> untouched), `UI::DialogControllerConfig` (empty ctor 0x18003b4a0) and `Graphics::AnimationEvent` (0x1800a2150 is
> `External()` + stamp only). `UI::DialogInstance` has a real public DEFAULT ctor (retail 0x180114ca0) besides the
> 6-argument one (0x180114870), and its slot 0 is CONCRETE (the exe instantiates it: table copy 0x14015f5a0, scalar
> deleting dtor 0x140020cb0). `~Particle` exists (retail 0x180103c70, ported by RB: frees each pool record's +0x18
> buffer, the record, then `_records`) and `~Light` exists (retail 0x180058d20, a bare tail into `~Geometry`).
> `~ParticleInstance` (0x1801041f0) was declared and undefined before RB.

> **B8-13 is wrong for the tools (block TOOL).** The Tool leaves' createInPlace (e.g. `BridgeTool` 0x14002d7e0:
> `TEST RCX,RCX; JNZ <ctor>; XOR EAX,EAX; RET`) DOES build in place — only the `Object::Internal`-rooted components,
> actions and works import `Internal::CreateInPlaceObject` (`XOR EAX,EAX; RET`). The tree's in-place entries are
> faithful for the tools.

> **Controller member slot 8 reads a "built" flag, not the state count (block MI-A report; verified by the close).**
> Well's member slot 8 0x140010260 is `XOR EAX,EAX; CMP dword [0x1401d2cf8],EAX; SETNZ AL; RET`. 0x1401d2cf8 is NOT
> the count: the state table's static initialiser 0x14014bbc0 builds the 16-entry, 0xf8-stride array at 0x1401d1d70
> and zeroes the COUNT at 0x1401d2cf0; the population initialiser 0x14014bc20 registers states 2..6 and then stores
> `1` to 0x1401d2cf8 (0x14014bc6e) — the only writer (xrefs: that store and the slot-8 read). §2.3's "HasStates:
> `CMP [count]`" and MI-A's commit 54b28f75 name the operand wrongly. The tree's `table.count != 0` answers the same
> for every table the tree builds (built ⇔ at least one state registered, D6: static init); a table registered empty
> would differ. Nothing in the tree dispatches member slot 8.

> **Tutorial dialog slot labels (block GOAL, 95e3a7e8 / bfa2b779 / 26120b98 / b7af21d0).** `TutorialHighlightDialog`
> (10 slots, 0xf0) overrides [4] OnOpened, [6] OnUpdateLayout, [7] OnUpdate and INHERITS 3/5/8/9 (Persist, OnClosed,
> OnElementEvent, OnSerializeSettings) — the old tree labels on its table were one class off. `TutorialUI` (11 slots,
> 0x60) is a `DialogController` subclass (slot 4 is `OnCreated`, the DialogController numbering, RULE B6-1.2), not a
> DialogInstance one. `TutorialGoalDialog` (10 slots, 0x40) and `TutorialSetupDialog` (12 slots, 0x78) are
> `TutorialGoal` / `TutorialSetup` subclasses — `Object::External`-rooted, not dialogs: slot 4 is the inherited
> `External::Compile`, slot 8 `TutorialGoal::IsComplete` resp. the TutorialSetup slots 6-10; the "Dialog" in the name
> is the dialog they open. The slot labels the tree carried before GOAL (DialogInstance names on all four tables) did
> not match these parents.

> **Ghidra ctor/dtor label swaps (blocks CIT, GOAL, MI-A, K1).** 0x140125d80 is `~NaturalResourceComponent`
> (restamp + `~Internal`, no own work); 0x1400e01a0 is `~TutorialHighlightDialog` (restamp only); 0x140044510 is
> `~AttendanceComponent` (frees the +0x28/+0x30 waiting queue); 0x1400c6970 (reported by a block, not re-read by
> the close). Also TOOL: 0x1400de950 is `~PathTool` (three inlined `~Array`), Ghidra labels it "Tool_CopyCtor". Ghidra's labels
> were not changed (no block renames in Ghidra); the class headers carry the right names.

> **`Animation.h` slot 8 naming (block VIS, 0034b3cd).** `runtime/types/graphics/Animation.h` names
> `Graphics::AnimationEvent`'s pure slot 8 `Construct()`. Retail's `AnimAudioEvent` slot 8 is the audio trigger
> 0x140087e40 and the leaf's own Construct is slot 9 (§2.2); the leaf declares slot 9 `ConstructInPlace` to avoid the
> clash. The Runtime header's name for slot 8 is wrong; renaming it is Runtime-side work (END of B.3).

> **FILEMAP misassignments (blocks PNL, DLG).** B38_FILEMAP §1 put `modbrowseui.cpp` and
> `dialog_controller_vtable.cpp` in DLG's set and `thumb_vtables.cpp` in PNL's, while §1.3 / D5 give `ModBrowseUI`
> (a `DialogController` subclass) and the `Apply()` copy site to PNL and the thumbnail ImageBuffer copy site to DLG.
> The blocks agreed the split between themselves (b9f2404c: PNL converted `ModBrowseUI` in its own header, DLG left
> `modbrowseui.cpp` untouched; GOAL finally deleted `dialog_controller_vtable.cpp` with TutorialUI, 95e3a7e8). No
> file was converted twice.

> **`GameUpdateLoop` +0x30 / +0x34 (caller batch CB6).** The fields are the map SEED (+0x30) and the map-SIZE index
> (+0x34); the tree's old names had them backwards (the Load body and `newgamedialog.cpp` establish it). Renamed
> `m_seed` / `m_mapSize`.

> **The `RtEngine::` PODs STAY (block LOOP).** D8's "the `RtEngine::` PODs … retire with them" is wrong for the
> RUNTIME side: the exe-side mirrors (`engine/types/{UpdateLoop,LoaderLoop,DialogLoaderLoopT,MainLoop}.h`) became
> forwarders and every `m_` use in the exe files was renamed, but the PODs themselves stay in
> `runtime/types/engine/UpdateLoop.h` — `UI::UserInterface` names the two dialog PODs as friends,
> `ui_userinterface_abi_test.cpp` pins them and the Runtime ctor/dtor doors stamp through them. They retire at the
> END of B.3 (§10).

> **The menu scene's factory bit (block LOOP, 723b0232 — root cause of the first LOOP merge's crash).** Once
> `~UpdateLoop` ran (the splash's chained dtor, 8e52ea3a), `ExternalFactory::Clean` evicted the menu scene: the
> standalone loads it from the splash on the main thread while the factory's clean-bit index still holds the
> SPLASH's bit, whereas retail loads it in `MenuDialogLoopT::Load` on the loader thread after `Loader::Run`
> published the MENU loop's bit. `MenuDialogLoopT_LoadSceneA1` now publishes the menu's bit for the load and
> restores it. Before D9 #12 the empty geometry dtor leaked the block and hid the eviction.

> **The resave golden was re-cut (coordinator, 758597e2).** LOOP's `~MenuDialogLoopT` running retail's
> `~DialogLoopT` chain frees the main menu's 128 UI objects before the game loads, as retail does: the resave's
> live-Internal-object count is 123,386 (was 123,514), same size, same 5,688 reference rows. Golden md5
> 757e48a1… → dd3893af…. New-game loader calls 5686 → 5683, openFileFail 3 → 0; tutorial 5776/15 → 5773/12.
> §7's `calls=5686` and the resave md5 are the PRE-LOOP values.

> **§1.2's unreflected loop tables are REFLECTED nowhere but NAMED now.** The five loop tables are the classes
> `GameUpdateLoop`, `MenuDialogLoopT`, `GameLoadDialogLoopT` (splash, 0x140158800), `InnerLoaderLoop` (0x140158870)
> and `GameLoaderLoop` (0x1401588d0); the unreflected Runtime subclasses are `PathfinderThread`, `WorldGeometry`
> (0x14015f110), `WeatherParticleGeometry` (0x140160240), `TornadoInstance` (0x1401601e0) and `StatusIconGeometry`
> (0x14015f0c0). None has a `BaseTypeInfo`.

### 8.1 §6 NOT VERIFIED — status at the close

1. **CLOSED** (blocks CIT, VIS, MI-A/B): Component slots 8/9 receive a POINTER to the elapsed `System::Time`, read
   from the retail callers (tree walk 0x1400c2e5d `MOV RDX,RBP`; bucket tick 0x1400c42af `LEA RDX,[RSP+0x40]`). The
   controller member's argument lists are §2.3's and every block that converted a controller read them from its
   member table.
2. **OPEN.** The type table's order sensitivity: D4 kept the FactoryRegistration order, so nobody needed to measure
   it; the coordinator's pointer-free type-table gate stayed equal at every merge except the intended
   `AutoSaveDialog` duplicate removal (DLG).
3. **CLOSED** (DLG, 99634e67): `StripNamespace` keeps what follows the LAST `:`, so `"public UI::DialogInstance"`
   and `"UI::DialogInstance"` both strip to `DialogInstance`; the tree's only reader of the unstripped literal is asked
   about `ControllerComponent` only. Gated with the type table and resave ×5.
4. **OPEN.** The member slot-1 miss-arm is fixed once in `TableController` for every Tag (MI-A, 54b28f75); the other
   23 member tables were still not compared byte for byte.
5. **OPEN.** No leaf with its own Destruct body was looked for.
6. **CLOSED** (RB): every Runtime class an exe-inlined copy needs is concrete — `DialogInstance`,
   `AchievementInstance`, `MeshInstance`, `SunLight`, `FrameCopyImageBuffer` (with `GeneratedImageBuffer`),
   `SpriteSheet`, `SpriteCapture`, `MaterialInstance`, `ImageBuffer`; LOOP builds SkyMesh / Sun / AchievementInstance
   with `new (p) NS::X()`.
7. **OPEN.** The loader-thread reach through vtable dispatch remains inferred (no instrument, DIRECTION §2).
8. Historical (the question pass's own side effect).
9. **OPEN.** No well was demolished in any gate run; the component `DeletingDtor` chains ran at teardown and in the
   leaf dtors MI-A transcribed (706058a1), but a live demolish of a converted controller was not observed.

---

## 9. The blocks, the batches and the close, as run

Every block ran on its own worktree branch under `b38_session/BLOCK_BRIEF.md` (block 0 under `BLOCK0_BRIEF.md`,
the caller/fold batches under `CALLER_BATCH_BRIEF.md`); every merge was gated by the coordinator on the merged tree
(`b38_session/gate.sh`). The measurements are `b38_session/CLOSE_NUMBERS.md`.

| order | block(s) | what it converted |
|---|---|---|
| 0 | block 0 (0A, 0B, 0C, 0D; `bc57c0a5`), the question pass + PILOT (`b38-rules`, merged `addb4dfd`) | inventories, `B38_SLOTS.md` + `step8_appendix_raw.txt`, the seam and file map; this sheet; `Component`, `ControllerComponent` (+ `Controller`), `Description`, `WellComponent`, `WellDescription`, `exe_type.h`, `TableController<Tag>` |
| 1 | RB part 1 (`3f090079`, UI bases for DLG/PNL/VIS), then RB (`65eafdc0`) | the D1 `InitFields` split and real ctors/dtors on the Runtime bases the exe derives or instantiates: UI (DialogInstance, DialogController, DialogControllerConfig, Element, ElementDescription, SpriteSheet, SpriteCapture), Engine loops, Achievement (4), Graphics (AnimationEvent, Particle, ParticleInstance, Geometry, MeshInstance, SunLight, FrameCopyImageBuffer + GeneratedImageBuffer, MaterialInstance), `Video::ImageBuffer` |
| 1 | CIT (`e61cae1a`), ACT (`45b8cc2e`), TOOL (`bb0ee047`), K1 (`fc92ee69`), WRK (`3eb4c02b`) | 19 Component leaves; `Action` + 17 (D9 #1 first); `Tool`/`DragTool`/`ToolSetting` + 9 (D9 #3, #4); 80 descriptions + `ComponentEntity` (D9 #2, #10); `Work` + `AutoPickupWork` + 21 |
| 2 | PNL (`03e6799f`), MI-A (`95128b08`), DLG (`877f68c3`), VIS (`e3f09481`) | 42 DialogController/Config subclasses (D9 #7); 13 town controllers (D9 #8, #9); 19 DialogInstance subclasses (D4 AutoSaveDialog, D9 #12, #13); 20 presentation components + 5 UI/Graphics strays (D9 #11) |
| 3 | MI-B (`73142f29`), GOAL (`9fb05e19`), LOOP (`7bdc92fd` UNDONE, then `a9937675` with fix `723b0232`) | `ResourceProduceComponent` + 14 controller leaves; goals, Scenario, 19 tutorial classes (D9 #5, #6, #14); the five loops, 11 world externals, Tornado, the geometries, the pathfinder thread; the menu-scene factory-bit fix |
| 4 | coordinator | resave golden re-cut (`758597e2`); CALLER_SURFACE (`aed7d537`), CALLER_BATCH_BRIEF (`e8f9da72`) |
| 5 | CB3 (`69c0bf3c`), CB1b (`98edc88f`), CB4 (`f3bec4e4`), CB6 (`f7f7181f`), CB5 (`bda29cbd`), CB1a (`8b282a76`), CB2 (`abae0ac2`) | fold + callers: Work/Action; Component m-z; tools + Player/Entity callers + 119 dead forwarders; loops/world to named fields; standalone callers, dead rows/stubs, `BANISHED_LIVE_TUTORIALINSTANCE_VT` retired; Component a-l; Dialog/Tutorial/Goal |
| 6 | close (`b38-close`, commits 1–5) | below |

The closing pass: (1) the `engine_funcs` zero-caller sweep by binding — 42 globals bound to a converted class's
code or table deleted whole, then the rows and doors only they read (27 rows, the DebugUI registrar and its 11
wrappers, 15 other doors, the empty menu registrars); (2) dead code — 90 reader-less rows of converted classes (8 vtable rows, 82 member rows),
the registrars and doors left empty or orphaned by them, `component_factory_base.*` / `controller_live_base.*`
(one harness use left), `HerdComponent_Startup`, stale banners; (3) §8; (4) `CLOSE_NUMBERS.md`; (5) §9–§11.

**Coordinator's gate table (2026-09-26, `b38_session/GATE_TABLE.md` verbatim). Every gate = debug + release + perf
build, warning diff vs a clean-rebuild baseline, unit, `dispatch_slot_map --check`, cutover manifest,
`rt_mint_gen --check`, new-game / tutorial / Load Game routes, pointer-free type table vs baseline, resave ×3 (×5
from CIT on).**

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
| close (b38-close, commits 1–5) | 736 | static gate per commit (below); the step close is the coordinator's |

The closing pass ran the static gate only, per commit: `standalone-debug`, `standalone-perf`, `unit` (736 / 0
failures), `dispatch_slot_map --check` (0), `cutover_manifest --gate` (OK, 59 rows), `rt_mint_gen --check` (UP TO
DATE), and a warning diff of both builds against a build of the branch point (591be5ed): 0 new. No game run.

## 10. Deliberately left (sizes measured at the close; `b38_session/CLOSE_NUMBERS.md`)

**PORT GAP slots: 142 markers** (Component 104, Dialog 30, Work 4, Game 2, standalone 2) — slots retail implements
whose tree body the conversion kept (D3.2); each transcription is its own titled behaviour commit. Of them, **38
name a dtor / free / leak**: the components, panels and works whose retail dtor frees members the tree still leaks
(PNL's 12 panel dtor frees; VIS's UI, Tracker, Interact, AutoPickup, Storage, StorageDisplay, Terraform, Highlight
dtors; WRK's FireWork/FishingWork; HerdComponent's +0x38 array; …). **49 STANDALONE DEVIATION markers** (Dialog 40:
PNL's slot overrides where the standalone differs on purpose).

**The lazy `X_ResolveFuncs` resolvers: 36 definitions, 137 calls** (`code/Game`), each a function-local or
file-static first-use resolve of exe/Runtime addresses into file statics, called from the members (WRK and the
action/work families). They are B6-5 surface where a loader-thread construction reaches them; converting each to a
namespace-scope resolve at registration is the END-of-B.3 / B.4 sweep.

**Type-id cells: 100 `g_tid_*` names and 111 `g_TypeId_*` names; 423 `ResolveAddr(Module::Exe, 0x1401c…/0x1401d…)`
sites** (D4/D10 (c): END of B.3). The concrete-class by-name lookups left: `Disease` (`citizen_updatehealth.cpp`),
`Toolbar` ×2 (`toolbar_vtable_standalone.cpp`, `tutorial_base.cpp`).

**Exe vtable rows kept, each with its reader (6):** `GameUpdateLoop` 0x14015e2f0, `GameLoaderLoop` 0x1401588d0,
`GameLoadDialogLoopT` 0x140158800, `MenuDialogLoopT` 0x14015ed00 — the door-then-stamp loop builders in
`mainmenudialog.cpp`, `newgamedialog.cpp`, `saveloaddialog.cpp`, `tutorialui.cpp`, `tutorial_next_scenario.cpp`
and `engine_funcs.h`'s inline `MenuDialogLoopT_Ctor` (18 sites, through `engine_funcs` `g_*_Vtable`); `StatGoal`
0x1401604c0 (`standalone/main.cpp` `RunFactorySelfTest`); the `Video::ImageBuffer` copy 0x14015c8c0
(`g_NoiseUI_ImageBufferVtable`, the ModDialog/NoiseUI/MapUI thumbnails; a Runtime-class copy site). Plus 237
reader-less rows whose symbol names no converted class (out of step 8's population).

**Hand-built tables left (3):** the dead `ControllerLive::Dispatcher<Tag>::vtbl` / `Vtable()` template in
`controller_state_table.h` (no caller: every controller embeds `TableController`), the ImageBuffer copy above, and
`factory_enum_stub.cpp`'s `g_EnumStub_Vtable` (not an exe class).

**The mint / `_typeInfo` / `RT_DELDTOR` rows of the step-8 Runtime bases (D10):** 23 mint `??_7` rows of the bases
RB touched (UI 7, Engine loops 4, Achievement 4, Graphics 7, `Video::ImageBuffer`) and 15 `?_typeInfo` rows stay in
`rt_data_mint_tables.inc`; up to 57 of the 137 `RT_DELDTOR` rows in `deleting_dtor.cpp` name one of those bases or a
Runtime subclass of theirs (substring count, an upper bound). They retire in D10's order at the END of B.3.
`RtUI::DialogController` and the `RtEngine::` loop PODs stay with them (§8).

**Singleton storage (carry-over 5 of step 7):** unchanged by step 8 — the 9 `CUTOVER_REQUIRED` manifest rows, the
`engine_funcs` storage globals, `?_singleton@SaveLoad@Save@@`.

**Unproved raw dispatches (CALLER_SURFACE §2's families, re-run on the close tree): 238 sites** (609 at 9fb05e19):
excluded by construction 136 (NOT-VPTR 80, RUNTIME 56); proved but left 27 (TYPE-ARRAY 23 — 7 of them in
`engine`, 2 SELF, 2 CTRL); candidates needing proof 75 (NAMED 26, TYPE-OTHER 24, OTHER 18, TOOL 7). Per directory:
Work 75 (55 NOT-VPTR), Dialog 58 (25 RUNTIME, 18 NOT-VPTR), Component 40, standalone 26, Player 14, Game 13,
engine 7, Tutorial 3, Goal 1, Object 1.

**Delegating members: 12** (CLOSE_NUMBERS §7; `CharacterComponent`'s 256-line body the largest).

**`engine_funcs` zero-reference globals not owned by a converted class (10):** `fn_BuildingList_CheckCondition`,
`fn_Building_RemoveBuyOrder`, `fn_CitizenBehavior_{CheckThresholds,CollectStats,CountResources}`,
`fn_CitizenList_AssignWorker`, `fn_Game_GetMonthFromTime`, `fn_Game_SpawnResources`, `fn_GetInputKeyName`,
`fn_Struct60B_Resize`; and 8 zero-reference type-id / Runtime-ctor slot globals (`g_TypeId_DialogController`,
`_ModelComponent`, `_PageToggleUI`, `_PickingComponent`, `_ProfessionDescription`, `fn_Element_Ctor`,
`fn_ElementDescription_Ctor/_Dtor`) — END of B.3 / B.4.

**Unused address constants:** the `XAddr::Vtable` / member constants in `engine/types/ui/{Dialogs,Panels}.h` and the
code/Game headers (CALLER_SURFACE G.7 counted 551 at 9fb05e19; CB4 deleted most with the 119 forwarders) — the
survivors are documentation, deleted with their headers.

**Retail slips transcribed, not fixed; tree in-place construction kept (B8-13, now proved faithful for the tools,
§8).**

## 11. Corrections from the closing pass (2026-09-26, `b38-close`)

1. **Controller member slot 8 (§2.3, D9 #9, MI-A 54b28f75) reads the table's BUILT flag**, not its count — re-read
   by the close (§8). The tree's `count != 0` is equivalent for every table it builds.
2. **CALLER_SURFACE G.3's "NOT dead" pair died with the batches.** `g_FollowDialog_Vtable` (CB2 converted
   `citizenui.cpp`) and `fn_ComponentBase_Serialize` (the 5 calls became `Component::Persist`) were at zero
   references by the close's branch point; commit 1 deleted both. The engine_funcs zero-reference population grew
   from 40 (step start) to 52 (branch point) because the batches converted callers but deleted no global (the
   brief's rule), exactly as in step 7.
3. **CB1b's "~86 users" of the inert helpers** were 23 files that still INCLUDED `component_factory_base.h` /
   `controller_live_base.h` and one real API use (`food_placement_harness.cpp`'s `InertStubSlot()` comparison) —
   an include is not a user. Both pairs are deleted (commit 2).
4. **The LEFTOVERS "CLOSE items" had mostly been done by the batches.** At the branch point the AchievementDialog /
   GoalDialog / MainMenuDialog rows, `reg_w2_lane1.cpp` (`g_PurecallVtable`), `reg_w2_lane2.cpp`,
   `reg_fruit_happiness_serialize.cpp`, `reg_cemetery_healer_serialize.cpp`, the Attendance / Forester /
   ResourceProduce rows, `RegisterAnimAudioEventVtable` / `RegisterCaptureBoxDescriptionVtable`, `UI_DIALOG_STUB`
   and the 10 MI-B Work vtable rows were already gone; only empty functions, doors and a few rows remained. The
   LEFTOVERS line numbers (`herdcomponent.cpp:1080`, `reg_code_lane5.cpp:81`, `toolbardescription.cpp:243`) had
   moved; the banners were found by content.
5. **A printf argument is not a caller, but a `(void*)fn_X` in a `_DEBUG` dump keeps a naive count at 1**
   (`fn_Geometry_Init`, `fn_Geometry_ChunkUpdate`, `g_TutorialInstanceVtable`). The close's sweep excludes a use on
   a `printf(` line whose argument is `(void*)NAME`; the DiagPrint rows went with their globals.
6. **Reader sweeps must see file-local constants.** A row is reader-less only if no other code-only occurrence of
   the literal exists anywhere AND no constant holding it is used (qualified `NS::X`, or bare within its own file).
   `rows.py` counts address-table initialisers and helper-wrapper literals (`R<>(0x…)`, `ResolveSlotPtr(0x…)`) as
   readers, so a row read through a computed address (`base + i*8`) would be missed — none was found.
7. **This pass's own miscount:** commit 2's message says the 82 member rows were "in 30 files"; they were in 34
   (42 files with the 8 vtable rows).
8. **The start-of-step tree was measured from `git archive 1d7866d4`** extracted into the scratch directory, not
   from a `git worktree add` (equivalent content; nothing to remove from the repository).
9. **`HerdComponent_Startup`** (the ctor tail the inert factory could not express) had been dead since CIT gave
   `HerdComponent` its real ctor (which stores +0x4c = -1); deleted in commit 2.
10. **Deleted `standalone/reg_*` registrars and boot output.** Commits 1 and 2 delete 117 `RegisterExeAddr` rows
    (27 + 90) and ten registrar files; the address-registry size the boot prints drops accordingly. No deleted row
    had a reader, so no `NOT IMPLEMENTED` line can appear; the coordinator's step close checks the stub set.
