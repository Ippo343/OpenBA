# B.3 step 9 — block 0C: type-id cells and the reflection fold (inventory, read-only)

Agent 0C, 2026-09-27, main @ 3fc6d9f6. Nothing built or run. Answers `NEXT_SESSION.md` §3b item 4 and
`B38_RULES.md` §6 item 2 (the order measurement is DESIGNED here; the coordinator runs it — §C).
Scripts: `C:/Users/USPC/AppData/Local/Temp/claude/b39/0C/{scan,cells,tidtable,rtti,final,mkpatch}.py`.
Method at the end ("How measured").

## Answer first

**Type ids are NOT order-dependent.** Retail `Factory::Startup` (Runtime 0x1800c0630) gives each record
`typeId = ~CRC32(name widened to UTF-16LE)` (inline CRC 0x1800c0674..0x1800c06df over `_crcTable`
0x1801f9080, store `MOV [RBX],EDX` at 0x1800c06e1), inserts it into an AVL set KEYED BY THAT ID
(`CALL 0x180030140` at 0x1800c071f), then walks the set IN ORDER (leftmost descent 0x1800c073b..0x1800c0753)
to link parents and append children. So an id is a pure function of the name: neither chain order nor a
name sort. The port is the same (`runtime/src/object/type_reflection.cpp:362-476`). Chain order can only
matter where two records share a key: the equal-key rule keeps the FIRST-inserted node
(`runtime/types/collection/Collection.h:1292`). Today there is exactly ONE such pair — `"Internal"`
(minted row `kTypeInfoStorage[52]`, `standalone/rt_data_mint_tables.inc:4459`, and
`DEFINE_TYPE(Object::Internal, "Base", nullptr)`, `runtime/src/object/internal_class.cpp:351`) — and
415 distinct names give 415 distinct CRCs (no collision). Children arrays are filled from the in-order walk,
so they are sorted by child id whatever the chain order is.

**What is persistent depends on id VALUES, never on order.** A save writes each `Object::Internal`'s
`Type()` as a uint32 (`runtime/src/object/internal.cpp:665-683`) and the load creates it by that id
(`InternalFactory::PersistObject`, `runtime/src/object/internal_class.cpp:808-833`); shipped `.rsc` files
carry type ids that `External::Load` reads (`runtime/src/object/external_load.cpp:331-344`). Those are CRCs
of the NAME, so the name spelling is the ABI and the order is not. Nothing persists chain order.

**What in the gate sees order:** the pointer-free type table (`b38_session/gate.sh:126-127`, `sort -u`)
contains the `[GameMain]   typeId=… name=…` lines, which print the FIRST TEN chain records
(`code/Game/Game/game_main.cpp:499-503`). Sorting hides their order but not their MEMBERSHIP: reversing
the exe list replaces those ten lines. That is a log artefact, not a behaviour. `[Probe … ] #n` lines
(`standalone/main.cpp:669-690`, first five, with indices) also show order but are not in the table.

## Counts

| item | count | how |
|---|---:|---|
| distinct `0x1401c…` addresses in code (no `0x1401d…` address exists in code) | 159 | cells.py |
| of them retail `BaseTypeInfo` records (type-id cells) | 148 | matched to `step8_appendix_raw.txt` SECTION R (310 `REG` rows) |
| of them NOT type ids | 11 | 3 singletons (0x1401c3dd8/3de0/3de8), 2 LUTs (0x1401c22d0/23d0), 6 `+8` NAME-field reads |
| type-id cells registered (`RegisterExeAddr` row or registrar table row) | 135 | tidtable.py |
| type-id cells UNREGISTERED that still have readers | 13 (26 sites) | ResolveAddr answers 0 |
| registered cells with ZERO readers | 7 | delete at once |
| code-only reader sites of `0x1401c…` addresses | 477 | 441 on type-id cells + 36 on the 11 others |
| … `ResolveAddr(Module::Exe, 0x1401c…)` form | 344 (308 type-id) + 76 `engine_funcs` `g_TypeId_` initialisers | the brief's "423" is this 420 ± the non-type-id cells |
| … per-file wrapper forms (`R<>`, `RExe`, `Tid`, `DerefTid`, `ResolveTid`, `ResolveTypeId`, `ResolveExeTypeId`, `PopulationTypeId`, `rd`, `MW_TypeId`, `SerializeValidatedIBP`) + 1 named constexpr | 57 | |
| type-id reader sites by dir | Component 147, Work 100, engine 79, Action 55, Player 17, Goal 16, Game 15, Dialog 9, Tutorial 3 (143 files) | |
| `g_TypeId_*` identifiers | 102 (76 initialised from an exe cell, the rest from an exe IAT `?_typeInfo@` import, a Runtime address, or a standalone fallback) | the brief's 111 was not reproduced; grep below |
| reads of those globals (excl. decl/definition/initialiser lines) | 821 | uses.json |
| `g_TypeId_*` whose NAME is a different class than its cell | 23 (Table 3, flag) | |
| `g_TypeId_*` with zero readers | 7 | |
| `g_tid_*` identifiers (the standalone storage) | 100 | |
| exe registrations | 311 = 297 `DEFINE_EXE_TYPE` + 13 `REGISTER_ABSTRACT_FACTORY` + 1 `REGISTER_TYPE(StandaloneFactoryTest)` | = boot's `[FactoryReg] Registered 311` |
| minted Runtime `_typeInfo` records | 104 | `rt_data_mint_tables.inc` |
| Runtime `DEFINE_TYPE` records | 1 (`Object::Internal`) | chain = 311 + 104 + 1 = 416 = `Post-Startup _firstTypeInfo: 416` |
| code references to a Runtime `?_typeInfo@` record | 339 (runtime 183, standalone 113, engine 27, code/Game 16) | rtti.py (Table 5) |

## Table 1 — one row per exe `.data` cell (every `0x1401c…` address the code names)

| cell | retail class (BaseTypeInfo ctor) | g_tid_ storage | g_TypeId_ global(s) | writer (RegisterExeAddr / table row) | readers by address (dir:n) | reads via g_TypeId_ | replacement |
|---|---|---|---|---|---|---:|---|
| 0x1401c22d0 | ? (not a retail BaseTypeInfo) | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | ? |
| 0x1401c23d0 | road index table (NOT a type id) | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | n/a (not a type id; singleton/table cell) |
| 0x1401c3dd8 | GameWorld singleton (NOT a type id) | - | - | standalone/address_registry.cpp:1514 | 22 (Action:2 Component:17 Work:1 engine:2) | 0 | n/a (not a type id; singleton/table cell) |
| 0x1401c3de0 | GameSettings/Debug ptr (NOT a type id) | - | - | standalone/address_registry.cpp:1515 | 3 (Game:1 engine:2) | 0 | n/a (not a type id; singleton/table cell) |
| 0x1401c3de8 | KeyMap singleton (NOT a type id) | - | - | standalone/address_registry.cpp:1513 | 1 (engine:1) | 0 | n/a (not a type id; singleton/table cell) |
| 0x1401c3e60 | AmbientEmitterComponent | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Component:1) | 0 | AmbientEmitterComponent::TypeId() |
| 0x1401c3f40 | AttendanceComponent | g_tid_AttendanceComponent | g_TypeId_AttendanceComponent | standalone/reg_economy_typeids.cpp:331 | 4 (Component:3 engine:1) | 7 | AttendanceComponent::TypeId() |
| 0x1401c3f78 | AttendanceDescription | g_tid_AttendanceDescription | g_TypeId_AttendanceDescription | standalone/reg_economy_typeids.cpp:330 | 3 (Component:2 engine:1) | 6 | AttendanceDescription::TypeId() |
| 0x1401c3fb0 | AutoPickupComponent | - | - | **none (unregistered: ResolveAddr answers 0)** | 2 (Component:1 Work:1) | 0 | AutoPickupComponent::TypeId() |
| 0x1401c3fe8 | AutoPickupDescription | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Component:1) | 0 | AutoPickupDescription::TypeId() |
| 0x1401c4020 | BoatComponent | g_tid_BoatComponent | - | standalone/reg_tradeui_typeids.cpp:128 | 1 (Component:1) | 0 | BoatComponent::TypeId() |
| 0x1401c4090 | CemeteryComponent | g_tid_CemeteryComponent | g_TypeId_CemeteryComponent | standalone/reg_town_services.cpp:70 | 1 (engine:1) | 10 | CemeteryComponent::TypeId() |
| 0x1401c40c8 | CemeteryDescription | g_tid_CemeteryDescription | - | standalone/reg_citizen_lifeevent_handler.cpp:56 | 1 (Component:1) | 0 | CemeteryDescription::TypeId() |
| 0x1401c4100 | CreateFencedDescription | g_tid_CreateFencedDescription | g_TypeId_CreateFencedDescription | standalone/reg_pipeline_typeids.cpp:103 | 1 (engine:1) | 3 | CreateFencedDescription::TypeId() |
| 0x1401c4138 | CreatePathDescription | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Game:1) | 0 | CreatePathDescription::TypeId() |
| 0x1401c4170 | DiseaseComponent | g_tid_DiseaseComponent | - | standalone/reg_component_cropdisease.cpp:95 | 1 (Game:1) | 0 | DiseaseComponent::TypeId() |
| 0x1401c41a8 | DiseaseDescription | g_tid_DiseaseDescription | - | standalone/reg_component_cropdisease.cpp:96 | 0 () | 0 | DiseaseDescription::TypeId() |
| 0x1401c41e0 | FireComponent | g_tid_FireComponent | g_TypeId_FireComponent | standalone/reg_fire.cpp:79 | 3 (Component:1 Work:1 engine:1) | 1 | FireComponent::TypeId() |
| 0x1401c4218 | FireDescription | g_tid_FireDescription | - | standalone/reg_fire.cpp:78 | 1 (Action:1) | 0 | FireDescription::TypeId() |
| 0x1401c4250 | FruitComponent | g_tid_FruitComponent | - | standalone/reg_component_fruit.cpp:71 | 2 (Component:1 Work:1) | 0 | FruitComponent::TypeId() |
| 0x1401c4288 | FruitDescription | g_tid_FruitDescription | - | standalone/reg_component_fruit.cpp:70 | 3 (Component:2 Work:1) | 0 | FruitDescription::TypeId() |
| 0x1401c42c0 | HappinessComponent | - | - | **none (unregistered: ResolveAddr answers 0)** | 2 (Component:1 Game:1) | 0 | HappinessComponent::TypeId() |
| 0x1401c42f8 | HappinessDescription | g_tid_HappinessDescription | - | standalone/reg_town_services.cpp:69 | 2 (Component:2) | 0 | HappinessDescription::TypeId() |
| 0x1401c4330 | HealerComponent | g_tid_HealerComponent | - | code/Game/Game/citizenlist_serialize.cpp:864 | 0 () | 0 | HealerComponent::TypeId() |
| 0x1401c43a0 | HerbalistComponent | - | - | standalone/reg_food_typeid.cpp:57 | 1 (Work:1) | 0 | HerbalistComponent::TypeId() |
| 0x1401c43d8 | HerbalistDescription | - | - | standalone/reg_food_typeid.cpp:58 | 1 (Work:1) | 0 | HerbalistDescription::TypeId() |
| 0x1401c4480 | ImmigrationComponent | - | g_TypeId_ImmigrationComponent | standalone/reg_unreg_lane4.cpp:111 | 1 (engine:1) | 4 | ImmigrationComponent::TypeId() |
| 0x1401c44f0 | MarketComponent | g_tid_MarketComponent | - | standalone/reg_market_typeid.cpp:90 | 1 (Work:1) | 0 | MarketComponent::TypeId() |
| 0x1401c4528 | MarketDescription | g_tid_MarketDescription | - | standalone/reg_market_typeid.cpp:91 | 1 (Work:1) | 0 | MarketDescription::TypeId() |
| 0x1401c46b0 | OrchardComponent | g_tid_OrchardComponent | g_TypeId_OrchardComponent | standalone/reg_orchard_typeid.cpp:81 | 5 (Work:4 engine:1) | 1 | OrchardComponent::TypeId() |
| 0x1401c46e8 | OrchardDescription | g_tid_OrchardDescription | - | standalone/reg_orchard_typeid.cpp:82 | 2 (Component:1 Work:1) | 0 | OrchardDescription::TypeId() |
| 0x1401c4720 | ParticleComponent | g_tid_ParticleComponent | - | standalone/reg_tornado.cpp:51 | 0 () | 0 | ParticleComponent::TypeId() |
| 0x1401c4758 | ParticleDescription | g_tid_ParticleDescription | - | standalone/reg_w2_lane3.cpp:62 | 1 (Component:1) | 0 | ParticleDescription::TypeId() |
| 0x1401c4790 | PopulationComponent | g_tid_PopulationComponent | - | standalone/reg_animal_serialize.cpp:40 | 3 (Action:1 Component:1 Game:1) | 0 | PopulationComponent::TypeId() |
| 0x1401c47c8 | PopulationDescription | g_tid_PopulationDescription | g_TypeId_DrawableType | standalone/reg_herd_live.cpp:63 | 3 (Component:2 engine:1) | 4 | PopulationDescription::TypeId() |
| 0x1401c4870 | RadiusDescription | g_tid_RadiusDescription | g_TypeId_RadiusComponent | standalone/reg_economy_typeids.cpp:317 | 7 (Component:2 Work:4 engine:1) | 10 | RadiusDescription::TypeId() |
| 0x1401c48a8 | SelectDescription | g_tid_SelectDescription | g_TypeId_SelectDescription | standalone/reg_pipeline_typeids.cpp:106 | 2 (Goal:1 engine:1) | 20 | SelectDescription::TypeId() |
| 0x1401c4918 | SpawnDescription | g_tid_SpawnDescription | - | standalone/reg_component_spawn.cpp:43 | 1 (Component:1) | 0 | SpawnDescription::TypeId() |
| 0x1401c49f8 | TerraformDescription | g_tid_TerraformDescription | - | standalone/reg_economy_typeids.cpp:336 | 1 (Component:1) | 0 | TerraformDescription::TypeId() |
| 0x1401c4a30 | TombstoneComponent | g_tid_TombstoneComponent | g_TypeId_TombstoneComponent | standalone/reg_citizen_lifeevent_handler.cpp:55 | 4 (Component:2 Goal:1 engine:1) | 7 | TombstoneComponent::TypeId() |
| 0x1401c4a68 | TombstoneDescription | g_tid_TombstoneDescription | - | standalone/reg_component_tombstone.cpp:43 | 1 (Component:1) | 0 | TombstoneDescription::TypeId() |
| 0x1401c4b10 | TrackerComponent | g_tid_TrackerComponent | g_TypeId_TrackerComponent | standalone/reg_tracker_typeid.cpp:55 | 3 (Action:1 Goal:1 engine:1) | 5 | TrackerComponent::TypeId() |
| 0x1401c4b80 | TradeComponent | g_tid_TradeComponent | g_TypeId_TradeComponent_FC | standalone/reg_tradeui_typeids.cpp:124 | 2 (Work:1 engine:1) | 5 | TradeComponent::TypeId() |
| 0x1401c4bb8 | TradeDescription | g_tid_TradeDescription | - | standalone/reg_tradeui_typeids.cpp:125 | 3 (Component:1 Dialog:1 engine:1) | 0 | TradeDescription::TypeId() |
| 0x1401c4bf0 | ValueDescription | g_tid_ValueDescription | - | standalone/reg_tradeui_typeids.cpp:126 | 1 (engine:1) | 0 | ValueDescription::TypeId() |
| 0x1401c4c28 | WellComponent | g_tid_WellComponent | - | code/Game/Game/citizenlist_serialize.cpp:865 | 0 () | 0 | WellComponent::TypeId() |
| 0x1401c4d08 | AutoSaveDialog | g_tid_AutoSaveDialog | g_TypeId_AutoSaveDialogType | standalone/reg_autosave.cpp:47 | 1 (engine:1) | 3 | AutoSaveDialog::TypeId() |
| 0x1401c4e20 | ControlUIConfig | g_tid_ControlUIConfig | g_TypeId_ControlComponent | standalone/reg_controlui.cpp:45 | 1 (engine:1) | 9 | ControlUIConfig::TypeId() |
| 0x1401c4f00 | EntityUI | - | g_TypeId_PageToggleUI | standalone/reg_unreg_lane4.cpp:122 | 4 (Dialog:1 Player:1 engine:2) | 0 | EntityUI::TypeId() |
| 0x1401c5210 | ModBrowseUI | - | g_TypeId_BrowserController | standalone/reg_unreg_lane4.cpp:127 | 1 (engine:1) | 2 | ModBrowseUI::TypeId() |
| 0x1401c5520 | ResourceLimitUIConfig | - | g_TypeId_ResourceLimitType | standalone/reg_unreg_lane4.cpp:130 | 1 (engine:1) | 3 | ResourceLimitUIConfig::TypeId() |
| 0x1401c5638 | CaptureBoxElement | - | g_TypeId_CaptureBoxElement | standalone/reg_unreg_lane4.cpp:137 | 1 (engine:1) | 2 | CaptureBoxElement::TypeId() |
| 0x1401c56e0 | StatusIconUIConfig | - | g_TypeId_DialogController | standalone/reg_unreg_lane4.cpp:144 | 1 (engine:1) | 0 | StatusIconUIConfig::TypeId() |
| 0x1401c5788 | ToolUI | - | g_TypeId_ToolUI | standalone/reg_unreg_lane4.cpp:149 | 4 (Dialog:1 Player:2 engine:1) | 10 | ToolUI::TypeId() |
| 0x1401c5868 | UpgradeUIConfig | - | g_TypeId_UpgradeUIConfig | standalone/reg_unreg_lane4.cpp:154 | 1 (engine:1) | 8 | UpgradeUIConfig::TypeId() |
| 0x1401c5910 | ClimateGenerator | - | g_TypeId_ClimateGenerator | standalone/reg_unreg_lane4.cpp:159 | 2 (Goal:1 engine:1) | 6 | ClimateGenerator::TypeId() |
| 0x1401c5948 | Disease | - | g_TypeId_DiseaseDescription | standalone/reg_unreg_lane4.cpp:168 | 2 (Component:1 engine:1) | 5 | Disease::TypeId() |
| 0x1401c59b8 | MapGenerator | - | g_TypeId_MapGenerator | standalone/reg_unreg_lane4.cpp:172 | 1 (engine:1) | 9 | MapGenerator::TypeId() |
| 0x1401c5a60 | NameDatabase | - | g_TypeId_NameDatabase | standalone/reg_unreg_lane4.cpp:176 | 1 (engine:1) | 3 | NameDatabase::TypeId() |
| 0x1401c5a98 | Profession | g_tid_Profession | g_TypeId_ProfessionDescription | standalone/reg_tutorial_typeid.cpp:84 | 6 (Component:2 Game:1 Goal:1 Tutorial:1 engine:1) | 0 | Profession::TypeId() |
| 0x1401c5b08 | Scenario | g_tid_Scenario | g_TypeId_Scenario | standalone/reg_autosave.cpp:48 | 1 (engine:1) | 6 | Scenario::TypeId() |
| 0x1401c5b40 | TerrainGenerator | - | g_TypeId_TerrainGenerator | standalone/reg_unreg_lane4.cpp:180 | 2 (Goal:1 engine:1) | 6 | TerrainGenerator::TypeId() |
| 0x1401c5d38 | ToolbarDialog | g_tid_ToolbarDialog | g_TypeId_ToolbarDialogParent | standalone/reg_economy_typeids.cpp:335 | 1 (engine:1) | 1 | ToolbarDialog::TypeId() |
| 0x1401c5d70 | ComponentEntity | g_tid_ComponentEntity | g_TypeId_ComponentEntity | standalone/reg_action_child_ctors2.cpp:68 | 12 (Action:6 Component:5 engine:1) | 189 | ComponentEntity::TypeId() |
| 0x1401c5da8 | Action | - | - | standalone/reg_unreg_lane4.cpp:203 | 1 (Component:1) | 0 | NONE today: Action is REGISTER_ABSTRACT_FACTORY (needs a static; D4) |
| 0x1401c5e50 | CompleteTask | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Work:1) | 0 | CompleteTask::TypeId() |
| 0x1401c5f38 | DropOffRawMaterial (+8(name field)) | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | n/a: +8 is the NAME field of DropOffRawMaterial's record; reader wants DropOffRawMaterial::TypeId()? (verify) |
| 0x1401c5f68 | HuntAnimal | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | HuntAnimal::TypeId() |
| 0x1401c5f70 | HuntAnimal (+8(name field)) | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | n/a: +8 is the NAME field of HuntAnimal's record; reader wants HuntAnimal::TypeId()? (verify) |
| 0x1401c5fe0 | PerformWork (+8(name field)) | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | n/a: +8 is the NAME field of PerformWork's record; reader wants PerformWork::TypeId()? (verify) |
| 0x1401c6080 | RemoveAnimal | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | RemoveAnimal::TypeId() |
| 0x1401c6088 | RemoveAnimal (+8(name field)) | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | n/a: +8 is the NAME field of RemoveAnimal's record; reader wants RemoveAnimal::TypeId()? (verify) |
| 0x1401c60c0 | RemoveNaturalResource (+8(name field)) | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Action:1) | 0 | n/a: +8 is the NAME field of RemoveNaturalResource's record; reader wants RemoveNaturalResource::TypeId()? (verify) |
| 0x1401c6128 | Toolbar | - | g_TypeId_ToolbarItem | standalone/reg_unreg_lane4.cpp:207 | 1 (engine:1) | 6 | Toolbar::TypeId() |
| 0x1401c6198 | Weather | - | - | standalone/reg_unreg_lane4.cpp:212 | 1 (engine:1) | 0 | Weather::TypeId() |
| 0x1401c6240 | FinishTutorialGoal | g_tid_FinishTutorialGoal | g_TypeId_FinishTutorialGoal | standalone/reg_tutorial_typeid.cpp:81 | 1 (engine:1) | 5 | FinishTutorialGoal::TypeId() |
| 0x1401c6278 | FinishTutorialInstance | g_tid_FinishTutorialInstance | - | standalone/reg_tutorial_typeid.cpp:82 | 0 () | 0 | FinishTutorialInstance::TypeId() |
| 0x1401c62e8 | TimeGoal | - | - | standalone/reg_unreg_lane4.cpp:218 | 0 () | 0 | TimeGoal::TypeId() |
| 0x1401c63c8 | RotationSetting | g_tid_RotationSetting | - | standalone/reg_pipeline_typeids.cpp:105 | 2 (Player:2) | 0 | RotationSetting::TypeId() |
| 0x1401c6668 | TutorialInstance | g_tid_TutorialInstance | - | standalone/reg_tutorial_typeid.cpp:83 | 1 (Tutorial:1) | 0 | TutorialInstance::TypeId() |
| 0x1401c6860 | BuildRoad | g_tid_BuildRoad | - | standalone/reg_economy_typeids.cpp:328 | 2 (Player:1 Work:1) | 0 | BuildRoad::TypeId() |
| 0x1401c6be0 | Pickup | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Work:1) | 0 | Pickup::TypeId() |
| 0x1401c6cf8 | Work | g_tid_Work | - | code/Game/Game/goals_serialize.cpp:877 | 0 () | 0 | NONE today: Work is REGISTER_ABSTRACT_FACTORY (needs a static; D4) |
| 0x1401c6dd8 | EditTool | - | g_TypeId_EditTool | standalone/reg_unreg_lane4.cpp:221 | 1 (engine:1) | 9 | EditTool::TypeId() |
| 0x1401c6eb8 | Tool | - | g_TypeId_ToolController | standalone/reg_unreg_lane4.cpp:227 | 2 (Component:1 engine:1) | 3 | NONE today: Tool is REGISTER_ABSTRACT_FACTORY (needs a static; D4) |
| 0x1401c6ef0 | ToolSetting | - | g_TypeId_ToolSetting | standalone/reg_unreg_lane4.cpp:230 | 2 (Component:1 engine:1) | 3 | NONE today: ToolSetting is REGISTER_ABSTRACT_FACTORY (needs a static; D4) |
| 0x1401c6f98 | AnimalComponent | g_tid_AnimalComponent | - | standalone/reg_component_animal.cpp:57 | 8 (Action:2 Component:5 Work:1) | 0 | AnimalComponent::TypeId() |
| 0x1401c6fd0 | AnimalDescription | g_tid_AnimalDescription | g_TypeId_AnimalDescription | standalone/reg_work_tail_serialize.cpp:62 | 7 (Action:1 Component:3 Work:2 engine:1) | 2 | AnimalDescription::TypeId() |
| 0x1401c7008 | BuildComponent | - | g_TypeId_BuildComponent_FC | standalone/reg_unreg_lane4.cpp:234 | 5 (Dialog:1 Game:1 Goal:1 Work:1 engine:1) | 3 | BuildComponent::TypeId() |
| 0x1401c7040 | BuildDescription | - | g_TypeId_ResourceComponent | standalone/reg_unreg_lane4.cpp:252 | 4 (Component:1 Goal:1 Work:1 engine:1) | 13 | BuildDescription::TypeId() |
| 0x1401c7078 | CharacterComponent | g_tid_CharacterComponent | - | standalone/reg_animal_update.cpp:60 | 4 (Action:2 Component:2) | 0 | CharacterComponent::TypeId() |
| 0x1401c70b0 | CharacterDescription | g_tid_CharacterDescription | - | standalone/reg_animal_update.cpp:61 | 4 (Action:2 Component:2) | 0 | CharacterDescription::TypeId() |
| 0x1401c70e8 | CitizenAIComponent | - | g_TypeId_CitizenAIComponent | **none (unregistered: ResolveAddr answers 0)** | 12 (Action:1 Component:2 Work:8 engine:1) | 7 | CitizenAIComponent::TypeId() |
| 0x1401c7158 | CitizenComponent | g_tid_CitizenComponent | g_TypeId_CitizenComponent_FC | standalone/reg_citizenai_typeids.cpp:83 | 28 (Action:8 Component:11 Work:8 engine:1) | 23 | CitizenComponent::TypeId() |
| 0x1401c7190 | CitizenDescription | g_tid_CitizenDescription | g_TypeId_CitizenDescription | standalone/reg_economy_typeids.cpp:322 | 6 (Action:1 Component:1 Game:1 Work:2 engine:1) | 24 | CitizenDescription::TypeId() |
| 0x1401c71c8 | ClearAreaComponent | g_tid_ClearAreaComponent | g_TypeId_ClearAreaComponent_FC | standalone/reg_cleararea_typeids.cpp:62 | 1 (engine:1) | 3 | ClearAreaComponent::TypeId() |
| 0x1401c7200 | ClearAreaDescription | g_tid_ClearAreaDescription | - | standalone/reg_cleararea_typeids.cpp:63 | 2 (Component:2) | 0 | ClearAreaDescription::TypeId() |
| 0x1401c72a8 | Component | g_tid_ComponentBase | g_TypeId_ComponentBase | standalone/reg_animal_serialize.cpp:41 | 1 (engine:1) | 7 | NONE today: Component is REGISTER_ABSTRACT_FACTORY (needs a static; D4) |
| 0x1401c72e0 | Description | - | - | standalone/reg_unreg_lane4.cpp:259 | 1 (Component:1) | 0 | NONE today: Description is REGISTER_ABSTRACT_FACTORY (needs a static; D4) |
| 0x1401c72e8 | Description (+8(name field)) | - | - | **none (unregistered: ResolveAddr answers 0)** | 3 (Component:3) | 0 | n/a: +8 is the NAME field of Description's record; reader wants Description::TypeId()? (verify) |
| 0x1401c7318 | ComponentDescription | g_tid_ComponentDescription | g_TypeId_ComponentDescription_FC | standalone/reg_tradeui_typeids.cpp:127 | 3 (Goal:1 Tutorial:1 engine:1) | 41 | ComponentDescription::TypeId() |
| 0x1401c7350 | ConsumeProduceComponent | g_tid_ConsumeProduceComponent | g_TypeId_ConsumeProduceComponent_FC | standalone/reg_action_child_ctors2.cpp:66 | 3 (Action:1 Work:1 engine:1) | 4 | ConsumeProduceComponent::TypeId() |
| 0x1401c7388 | ConsumeProduceDescription | g_tid_ConsumeProduceDescription | g_TypeId_ConsumeProduceComponent | standalone/reg_action_child_ctors2.cpp:67 | 4 (Component:1 Work:2 engine:1) | 11 | ConsumeProduceDescription::TypeId() |
| 0x1401c73c0 | ControllerComponent | - | - | standalone/reg_unreg_lane4.cpp:264 | 1 (Component:1) | 0 | NONE today: ControllerComponent is REGISTER_ABSTRACT_FACTORY (needs a static; D4) |
| 0x1401c73f8 | CreateBridgeDescription | g_tid_CreateBridgeDescription | g_TypeId_CreateBridgeDescription | standalone/reg_economy_typeids.cpp:324 | 1 (engine:1) | 2 | CreateBridgeDescription::TypeId() |
| 0x1401c7430 | CreateDragDescription | g_tid_CreateDragDescription | g_TypeId_CreatePlacementDescription | standalone/reg_economy_typeids.cpp:329 | 3 (Player:2 engine:1) | 2 | CreateDragDescription::TypeId() |
| 0x1401c7468 | CreatePlacedDescription | g_tid_CreatePlacedDescription | g_TypeId_SizeDescription | standalone/reg_pipeline_typeids.cpp:99 | 1 (engine:1) | 12 | CreatePlacedDescription::TypeId() |
| 0x1401c74a0 | CreateRemovalDescription | g_tid_CreateRemovalDescription | g_TypeId_RemovalDescription | standalone/reg_economy_typeids.cpp:326 | 1 (engine:1) | 2 | CreateRemovalDescription::TypeId() |
| 0x1401c74d8 | CreateRoadDescription | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Player:1) | 0 | CreateRoadDescription::TypeId() |
| 0x1401c7510 | CropFieldComponent | - | - | standalone/reg_food_typeid.cpp:59 | 3 (Work:3) | 0 | CropFieldComponent::TypeId() |
| 0x1401c7548 | CropFieldDescription | - | - | standalone/reg_food_typeid.cpp:60 | 2 (Component:1 Work:1) | 0 | CropFieldDescription::TypeId() |
| 0x1401c7580 | DecalComponent | g_tid_DecalComponent | - | standalone/reg_pipeline_typeids.cpp:102 | 1 (Component:1) | 0 | DecalComponent::TypeId() |
| 0x1401c75b8 | DecalDescription | - | - | standalone/reg_unreg_lane4.cpp:269 | 3 (Component:2 Player:1) | 0 | DecalDescription::TypeId() |
| 0x1401c75f0 | DestroyComponent | g_tid_DestroyComponent | g_TypeId_DestroyComponent_FC | standalone/reg_economy_typeids.cpp:327 | 5 (Goal:1 Player:2 Work:1 engine:1) | 1 | DestroyComponent::TypeId() |
| 0x1401c7628 | DestroyDescription | g_tid_DestroyDescription | g_TypeId_DestroyComponent | standalone/reg_economy_typeids.cpp:334 | 3 (Component:2 engine:1) | 10 | DestroyDescription::TypeId() |
| 0x1401c7660 | FishingComponent | - | - | standalone/reg_food_typeid.cpp:61 | 2 (Work:2) | 0 | FishingComponent::TypeId() |
| 0x1401c7698 | FishingDescription | - | - | standalone/reg_food_typeid.cpp:62 | 2 (Component:1 Work:1) | 0 | FishingDescription::TypeId() |
| 0x1401c7708 | FloorDescription | - | - | standalone/reg_unreg_lane4.cpp:273 | 1 (Component:1) | 0 | FloorDescription::TypeId() |
| 0x1401c7740 | ForesterComponent | g_tid_ForesterComponent | - | standalone/reg_economy_typeids.cpp:315 | 1 (Work:1) | 0 | ForesterComponent::TypeId() |
| 0x1401c7778 | ForesterDescription | g_tid_ForesterDescription | - | standalone/reg_economy_typeids.cpp:316 | 1 (Work:1) | 0 | ForesterDescription::TypeId() |
| 0x1401c77b0 | GrowthComponent | g_tid_GrowthComponent | - | standalone/reg_growth_typeid.cpp:61 | 2 (Action:2) | 0 | GrowthComponent::TypeId() |
| 0x1401c77e8 | GrowthDescription | g_tid_GrowthDescription | - | standalone/reg_component_spawn.cpp:44 | 4 (Component:1 Work:3) | 0 | GrowthDescription::TypeId() |
| 0x1401c7820 | HerdComponent | g_tid_HerdComponent | - | standalone/reg_component_animal.cpp:56 | 3 (Component:2 Game:1) | 0 | HerdComponent::TypeId() |
| 0x1401c7858 | HerdDescription | g_tid_HerdDescription | - | standalone/reg_component_animal.cpp:55 | 3 (Action:1 Component:2) | 0 | HerdDescription::TypeId() |
| 0x1401c7890 | HuntComponent | - | - | standalone/reg_food_typeid.cpp:63 | 2 (Action:1 Work:1) | 0 | HuntComponent::TypeId() |
| 0x1401c78c8 | HuntDescription | - | - | standalone/reg_food_typeid.cpp:64 | 2 (Action:1 Work:1) | 0 | HuntDescription::TypeId() |
| 0x1401c7900 | InteractComponent | g_tid_InteractComponent | g_TypeId_InteractComponent_FC | standalone/reg_work_tail_fire.cpp:40 | 7 (Component:3 Work:3 engine:1) | 4 | InteractComponent::TypeId() |
| 0x1401c7938 | InteractDescription | g_tid_InteractDescription | g_TypeId_InteractDescription_FC | standalone/reg_economy_typeids.cpp:308 | 10 (Action:2 Component:1 Work:6 engine:1) | 5 | InteractDescription::TypeId() |
| 0x1401c7970 | LivestockComponent | g_tid_LivestockComponent | - | standalone/reg_component_animal.cpp:59 | 3 (Component:2 Game:1) | 0 | LivestockComponent::TypeId() |
| 0x1401c79a8 | LivestockDescription | g_tid_LivestockDescription | - | standalone/reg_component_animal.cpp:60 | 3 (Component:2 Dialog:1) | 0 | LivestockDescription::TypeId() |
| 0x1401c79e0 | MapComponent | g_tid_MapComponent | g_TypeId_MapComponent_FC | standalone/reg_economy_typeids.cpp:309 | 7 (Component:1 Game:2 Goal:1 Player:1 Work:1 engine:1) | 6 | MapComponent::TypeId() |
| 0x1401c7a18 | MapDescription | g_tid_MapDescription | g_TypeId_AreaDescription | standalone/reg_pipeline_typeids.cpp:104 | 3 (Component:2 engine:1) | 18 | MapDescription::TypeId() |
| 0x1401c7a50 | ModelComponent | g_tid_ModelComponent | g_TypeId_ModelComponent, g_TypeId_ModelComponent_FC | standalone/reg_pipeline_typeids.cpp:100 | 7 (Action:1 Component:4 engine:2) | 5 | ModelComponent::TypeId() |
| 0x1401c7a88 | ModelDescription | g_tid_ModelDescription | g_TypeId_FenceDescription | standalone/reg_pipeline_typeids.cpp:101 | 11 (Action:3 Component:6 Player:1 engine:1) | 10 | ModelDescription::TypeId() |
| 0x1401c7ac0 | MultiObjectComponent | g_tid_MultiObjectComponent | g_TypeId_MultiObjectComponent | standalone/reg_economy_typeids.cpp:325 | 3 (Component:2 engine:1) | 3 | MultiObjectComponent::TypeId() |
| 0x1401c7b30 | NaturalResourceComponent | g_tid_NaturalResourceComponent | - | standalone/reg_component_nature.cpp:150 | 7 (Component:5 Work:2) | 0 | NaturalResourceComponent::TypeId() |
| 0x1401c7b68 | NaturalResourceDescription | g_tid_NaturalResourceDescription | - | standalone/reg_economy_typeids.cpp:318 | 8 (Action:2 Component:2 Dialog:1 Work:3) | 0 | NaturalResourceDescription::TypeId() |
| 0x1401c7ba0 | PastureComponent | g_tid_PastureComponent | g_TypeId_PastureComponent | standalone/reg_work_tail_serialize.cpp:63 | 4 (Component:1 Work:2 engine:1) | 2 | PastureComponent::TypeId() |
| 0x1401c7bd8 | PastureDescription | g_tid_PastureDescription | - | standalone/reg_work_tail_serialize.cpp:61 | 1 (Work:1) | 0 | PastureDescription::TypeId() |
| 0x1401c7c10 | PickingComponent | - | g_TypeId_PickingComponent | standalone/reg_unreg_lane4.cpp:277 | 3 (Component:2 engine:1) | 0 | PickingComponent::TypeId() |
| 0x1401c7c80 | RawMaterialComponent | g_tid_RawMaterialComponent | g_TypeId_NaturalResourceDesc | standalone/reg_economy_typeids.cpp:323 | 5 (Action:2 Component:2 Work:1) | 2 | RawMaterialComponent::TypeId() |
| 0x1401c7cb8 | RawMaterialDescription | g_tid_RawMaterialDescription | g_TypeId_RawMaterialDescription | standalone/reg_action_child_ctors2.cpp:65 | 25 (Action:7 Component:7 Dialog:1 Game:1 Goal:1 Work:7 engine:1) | 91 | RawMaterialDescription::TypeId() |
| 0x1401c7cf0 | ResidenceComponent | g_tid_ResidenceComponent | g_TypeId_ResidenceComponent | standalone/reg_economy_typeids.cpp:314 | 4 (Component:3 engine:1) | 11 | ResidenceComponent::TypeId() |
| 0x1401c7d28 | ResidenceDescription | g_tid_ResidenceDescription | - | standalone/reg_economy_typeids.cpp:332 | 3 (Component:3) | 0 | ResidenceDescription::TypeId() |
| 0x1401c7d98 | StateMachineComponent | g_tid_StateMachineComponent | g_TypeId_StateMachineComponent_FC | standalone/reg_economy_typeids.cpp:333 | 3 (Goal:1 Player:1 engine:1) | 10 | StateMachineComponent::TypeId() |
| 0x1401c7e08 | StatusIconComponent | g_tid_StatusIconComponent | g_TypeId_StatusIconComponent | standalone/reg_economy_typeids.cpp:312 | 3 (Component:1 Work:1 engine:1) | 14 | StatusIconComponent::TypeId() |
| 0x1401c7e40 | StatusIconDescription | g_tid_StatusIconDescription | g_TypeId_MeshInstanceComponent | standalone/reg_economy_typeids.cpp:313 | 2 (Component:1 engine:1) | 3 | StatusIconDescription::TypeId() |
| 0x1401c7e78 | StorageComponent | g_tid_StorageComponent | g_TypeId_StorageComponent_FC | standalone/reg_economy_typeids.cpp:321 | 11 (Action:1 Component:2 Game:1 Goal:1 Work:5 engine:1) | 23 | StorageComponent::TypeId() |
| 0x1401c7eb0 | StorageDescription | - | - | standalone/reg_unreg_lane4.cpp:281 | 2 (Component:1 Game:1) | 0 | StorageDescription::TypeId() |
| 0x1401c7ee8 | StorageLocationComponent | g_tid_StorageLocationComponent | g_TypeId_UnknownComp_7ee8 | standalone/reg_market_typeid.cpp:92 | 3 (Work:2 engine:1) | 1 | StorageLocationComponent::TypeId() |
| 0x1401c7f20 | StorageLocationDescription | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Work:1) | 0 | StorageLocationDescription::TypeId() |
| 0x1401c7f58 | ToolbarDescription | - | g_TypeId_ToolbarDescription_FC | standalone/reg_unreg_lane4.cpp:285 | 2 (Player:1 engine:1) | 20 | ToolbarDescription::TypeId() |
| 0x1401c7f90 | UIDescription | - | g_TypeId_UIDescription | standalone/reg_unreg_lane4.cpp:291 | 2 (Dialog:1 engine:1) | 16 | UIDescription::TypeId() |
| 0x1401c7fc8 | UIComponent | - | - | **none (unregistered: ResolveAddr answers 0)** | 1 (Dialog:1) | 0 | UIComponent::TypeId() |
| 0x1401c8000 | WildAnimalComponent | g_tid_WildAnimalComponent | - | standalone/reg_component_animal.cpp:58 | 5 (Action:2 Component:2 Game:1) | 0 | WildAnimalComponent::TypeId() |
| 0x1401c8070 | WorkComponent | g_tid_WorkComponent | g_TypeId_WorkComponent | standalone/reg_economy_typeids.cpp:320 | 6 (Component:2 Player:1 Work:2 engine:1) | 16 | WorkComponent::TypeId() |
| 0x1401c80a8 | WorkDescription | g_tid_WorkDescription | g_TypeId_WorkDescription | standalone/reg_economy_typeids.cpp:319 | 2 (Work:1 engine:1) | 8 | WorkDescription::TypeId() |
| 0x1401c80e0 | WorkPlaceComponent | g_tid_WorkPlaceComponent | g_TypeId_WorkplaceComponent | standalone/reg_economy_typeids.cpp:310 | 5 (Component:3 Goal:1 engine:1) | 15 | WorkPlaceComponent::TypeId() |
| 0x1401c8118 | WorkPlaceDescription | g_tid_WorkPlaceDescription | - | standalone/reg_economy_typeids.cpp:311 | 3 (Action:1 Component:1 Goal:1) | 0 | WorkPlaceDescription::TypeId() |

### Notes on Table 1

- **Writer.** Every registered cell is `RegisterExeAddr(cell, &storage)` in a `standalone/reg_*.cpp`
  (or `code/Game/Game/{citizenlist,goals}_serialize.cpp`, or a registrar table: `reg_unreg_lane4.cpp` 30
  rows `{addr,"Name",0}`, `reg_food_typeid.cpp` 8, `reg_tutorial_typeid.cpp` 4) at `AddressRegistryInit`;
  the VALUE is written after `Engine::Startup` by the `Fill*` calls at `code/Game/Game/game_main.cpp:511-542`
  (`g_tid_X = X::TypeId()` or `GetTypeIdByName("X")`). Nothing writes a cell in `Factory::Startup` — in
  retail the cell IS the class's `BaseTypeInfo` record and Startup writes its +0; in the tree the cell is a
  standalone copy of the record's id. The class column is retail's (the `BaseTypeInfo` ctor call in the
  exe's `*_RegisterType`); every tree fill agrees with it (0 mismatches). The mis-bindings are in the
  NAMES of the `g_TypeId_*` globals (Table 3, 23 flagged; a few flags are harmless aliases —
  `ComponentBase`→`Component`, `ToolbarItem`→`Toolbar` — the rest name another class:
  `g_TypeId_ResourceComponent`=BuildDescription, `g_TypeId_FenceDescription`=ModelDescription,
  `g_TypeId_AreaDescription`=MapDescription (18 readers), `g_TypeId_SizeDescription`=CreatePlacedDescription,
  `g_TypeId_DiseaseDescription`=Disease, `g_TypeId_ProfessionDescription`=Profession,
  `g_TypeId_DrawableType`=PopulationDescription, `g_TypeId_ToolController`=Tool,
  `g_TypeId_DialogController`=StatusIconUIConfig, `g_TypeId_PageToggleUI`=EntityUI,
  `g_TypeId_NaturalResourceDesc`=RawMaterialComponent, …). One `g_tid_` misnomer: `g_tid_ComponentBase`
  (0x1401c72a8) = `Component`. **A caller batch converts BY CELL (Table 1's class), never by the global's name.**
- **Replacement.** `X::TypeId()` exists for 297 classes (`DECLARE_EXE_TYPE`). Seven cells have no static:
  the abstract bases `Action` 0x1401c5da8, `Work` 0x1401c6cf8, `Tool` 0x1401c6eb8, `ToolSetting` 0x1401c6ef0,
  `Component` 0x1401c72a8, `Description` 0x1401c72e0, `ControllerComponent` 0x1401c73c0 (they are
  `REGISTER_ABSTRACT_FACTORY`, `code/Game/Object/factory_registrations_bulk.cpp:94-192`; their `Type()` is
  `GetTypeIdByName("X")`). The fold must give the 13 abstract bases the same static (§B.4) before those
  cells can go.
- **Unregistered cells with readers (13 type-id cells, 26 sites)** answer 0 / a null pointer today where
  retail has a CRC. Two are deliberate: `CitizenAIComponent` 0x1401c70e8 (12 sites) and `HappinessComponent`
  0x1401c42c0 (2) had opt-in fills deleted with their flags (`standalone/reg_citizenai_typeids.cpp:77-83`,
  "ENDGAME C.2"). Converting ANY of these 26 sites to `X::TypeId()` is a BEHAVIOUR CHANGE (e.g. the ~20
  `SendComponentMessage` action ctors start carrying a real target type): each is its own titled commit,
  gated with resave ×5.
- **`+8` readers (6 cells, 8 sites) are not type ids.** 0x1401c5f38/5f70/5fe0/6088/60c0 (`DropOffRawMaterial`,
  `HuntAnimal` ×2 names, `PerformWork`, `RemoveAnimal`, `RemoveNaturalResource` — `code/Game/Action/*.cpp`,
  named `s_typeInfo_X` / `s_dat_VectorDtorReturn`) and 0x1401c72e8 (`Description`+8, called
  "TypeInfo_ComponentEntity" in `multiobjectcomponent.cpp:91`, `nomadcomponent.cpp:447`,
  `pickingcomponent.cpp:217`) are the record's NAME pointer. Retail's only xrefs are the class's slot 1
  `TypeName` (0x1401c5f38 ← 0x1400225f0 `DropOffRawMaterial_Slot1`; 0x1401c72e8 ← 0x140031c60), read by
  `get_xrefs_to`. All are unregistered, so every guard built on them is permanently false
  (`multiobjectcomponent.cpp:253`'s `IsDerivedFrom(ptr, ComponentEntity)` never runs). Each needs a read
  of the retail caller before conversion; not measured further here.

## Table 2 — type-id reader sites by file (caller batches cut by directory)

Every code-only `0x1401c…` occurrence that is not a writer row, including the 11 non-type-id cells (the
cells column shows the last 5 hex digits). `engine_funcs g_TypeId_ initialiser` rows are the 76 globals;
their 821 reads are Table 3.

| file | sites | cells read | forms |
|---|---:|---|---|
| code/Game/Action/action_base_cnr.cpp | 4 | c77b0 c7a50 c7a88 c7b68 | wrapper R×4 |
| code/Game/Action/action_base_complete.cpp | 2 | c7a88 c7cb8 | ResolveAddr(Exe)×2 |
| code/Game/Action/action_base_hunt.cpp | 3 | c6f98 c78c8 c8000 | ResolveAddr(Exe)×3 |
| code/Game/Action/action_base_move.cpp | 2 | c7078 c70b0 | ResolveAddr(Exe)×2 |
| code/Game/Action/action_base_rawmat.cpp | 3 | c4b10 c7c80 c7cb8 | ResolveAddr(Exe)×3 |
| code/Game/Action/action_child_ctors2.cpp | 2 | c7350 c7cb8 | ResolveAddr(Exe)×2 |
| code/Game/Action/action_pf_ctors.cpp | 4 | c70b0 c7190 c7938 c7cb8 | ResolveAddr(Exe)×4 |
| code/Game/Action/action_pf_geom.cpp | 1 | c22d0 | ResolveAddr(Exe)×1 |
| code/Game/Action/action_pf_road.cpp | 2 | c23d0 c7938 | ResolveAddr(Exe)×2 |
| code/Game/Action/action_pf_roots_drop.cpp | 1 | c70e8 | ResolveAddr(Exe)×1 |
| code/Game/Action/action_pf_roots_misc.cpp | 2 | c6f98 c7858 | wrapper R×2 |
| code/Game/Action/animatenaturalresource.cpp | 1 | c5d70 | ResolveAddr(Exe)×1 |
| code/Game/Action/completeroad.cpp | 1 | c3dd8 | ResolveAddr(Exe)×1 |
| code/Game/Action/completetask.cpp | 3 | c3dd8 c5d70 c7158 | ResolveAddr(Exe)×3 |
| code/Game/Action/createnaturalresource.cpp | 2 | c5d70 c77b0 | ResolveAddr(Exe)×2 |
| code/Game/Action/createrawmaterial.cpp | 5 | c5d70 c7158 c7c80 c7cb8 c8118 | ResolveAddr(Exe)×5 |
| code/Game/Action/dropoffrawmaterial.cpp | 2 | c5f38 c7158 | ResolveAddr(Exe)×2 |
| code/Game/Action/huntanimal.cpp | 6 | c5f68 c5f70 c6fd0 c7158 c7890 c8000 | ResolveAddr(Exe)×6 |
| code/Game/Action/moveto.cpp | 2 | c4790 c7078 | ResolveAddr(Exe)×2 |
| code/Game/Action/performwork.cpp | 1 | c5fe0 | ResolveAddr(Exe)×1 |
| code/Game/Action/pickuprawmaterial.cpp | 1 | c7158 | ResolveAddr(Exe)×1 |
| code/Game/Action/pickupstoredrawmaterial.cpp | 3 | c7158 c7cb8 c7e78 | ResolveAddr(Exe)×3 |
| code/Game/Action/playanimation.cpp | 1 | c5d70 | ResolveAddr(Exe)×1 |
| code/Game/Action/removeanimal.cpp | 3 | c4218 c6080 c6088 | ResolveAddr(Exe)×3 |
| code/Game/Action/removenaturalresource.cpp | 3 | c60c0 c7158 c7b68 | ResolveAddr(Exe)×3 |
| code/Game/Action/sendcomponentmessage.cpp | 1 | c5d70 | ResolveAddr(Exe)×1 |
| code/Game/Action/showtool.cpp | 3 | c7158 c7a88 c7cb8 | ResolveAddr(Exe)×3 |
| code/Game/Component/absorbresourcecomponent.cpp | 1 | c7c80 | ResolveAddr(Exe)×1 |
| code/Game/Component/animalcomponent.cpp | 2 | c6fd0 c70b0 | ResolveAddr(Exe)×2 |
| code/Game/Component/animalwave_serialize.cpp | 5 | c4790 c47c8 c5d70 c7ba0 c8000 | ResolveAddr(Exe)×5 |
| code/Game/Component/attendancecomponent.cpp | 2 | c7158 c80e0 | ResolveAddr(Exe)×2 |
| code/Game/Component/attendancecomponent_states.cpp | 1 | c3f78 | ResolveAddr(Exe)×1 |
| code/Game/Component/autopickupcomponent.cpp | 1 | c3fe8 | ResolveAddr(Exe)×1 |
| code/Game/Component/boatcomponent_route.cpp | 3 | c4020 c7938 c7a50 | ResolveAddr(Exe)×3 |
| code/Game/Component/buildcomponent.cpp | 4 | c7a50 c7a88 c7ac0 c7c10 | ResolveAddr(Exe)×4 |
| code/Game/Component/cemeterycomponent.cpp | 2 | c3dd8 c4a30 | ResolveAddr(Exe)×2 |
| code/Game/Component/charactercomponent_serialize.cpp | 4 | c5da8 c70b0 c7a50 c7c10 | ResolveAddr(Exe)×4 |
| code/Game/Component/citizen_lifeevent_handler.cpp | 3 | c40c8 c4a30 c7900 | ResolveAddr(Exe)×3 |
| code/Game/Component/citizen_reassign_workers.cpp | 2 | c7cf0 c7e78 | wrapper RExe×2 |
| code/Game/Component/citizenai_actionctors.cpp | 4 | c42c0 c70e8 c7158 c7cb8 | ResolveAddr(Exe)×4 |
| code/Game/Component/citizenai_handlers.cpp | 1 | c70e8 | ResolveAddr(Exe)×1 |
| code/Game/Component/citizenaicomponent.cpp | 3 | c7cb8 c7cf0 c7e78 | ResolveAddr(Exe)×3 |
| code/Game/Component/citizencomponent.cpp | 8 | c3f78 c7158 c7900 c7cb8 c7cf0 c7d28 c8070 c80e0 | ResolveAddr(Exe)×8 |
| code/Game/Component/citizencomponent_lifecycle.cpp | 1 | c8070 | ResolveAddr(Exe)×1 |
| code/Game/Component/citizencomponent_serialize.cpp | 16 | c3f40 c5948 c5a98 c5d70 c7078 c7158 c7190 c7e08 | ResolveAddr(Exe)×4, wrapper ?×12 |
| code/Game/Component/clearareacomponent_states.cpp | 5 | c7200 c7b30 c7b68 c7cb8 | wrapper DerefTid×5 |
| code/Game/Component/component.cpp | 1 | c72e0 | ResolveAddr(Exe)×1 |
| code/Game/Component/consumeproducecomponent.cpp | 1 | c7388 | ResolveAddr(Exe)×1 |
| code/Game/Component/cropfieldcomponent.cpp | 3 | c5d70 c7548 c7b30 | ResolveAddr(Exe)×3 |
| code/Game/Component/cropfieldcomponent_states.cpp | 2 | c7b68 c7cb8 | ResolveAddr(Exe)×2 |
| code/Game/Component/decalcomponent.cpp | 3 | c3dd8 c75b8 | ResolveAddr(Exe)×3 |
| code/Game/Component/destroycomponent_state6.cpp | 3 | c7580 c7628 c7a88 | ResolveAddr(Exe)×3 |
| code/Game/Component/destroycomponent_states.cpp | 4 | c7040 c7628 c7a88 c7cb8 | ResolveAddr(Exe)×4 |
| code/Game/Component/entity_finders_disaster.cpp | 3 | c3e60 c41e0 c7158 | wrapper ResolveTid×3 |
| code/Game/Component/fishingcomponent_produce.cpp | 2 | c4870 c7698 | ResolveAddr(Exe)×2 |
| code/Game/Component/floorcomponent.cpp | 3 | c3dd8 c7708 c79e0 | ResolveAddr(Exe)×3 |
| code/Game/Component/fruitcomponent.cpp | 2 | c3dd8 c4288 | ResolveAddr(Exe)×2 |
| code/Game/Component/fruitcomponent_serialize.cpp | 1 | c7b30 | ResolveAddr(Exe)×1 |
| code/Game/Component/growthcomponent.cpp | 1 | c3dd8 | ResolveAddr(Exe)×1 |
| code/Game/Component/growthcomponent_serialize.cpp | 2 | c7a50 c7b30 | ResolveAddr(Exe)×2 |
| code/Game/Component/happinesscomponent.cpp | 5 | c3dd8 c42f8 c4870 c7cb8 | ResolveAddr(Exe)×5 |
| code/Game/Component/herdcomponent.cpp | 7 | c3dd8 c6f98 c6fd0 c7078 c7858 c7a88 c8000 | ResolveAddr(Exe)×7 |
| code/Game/Component/interactcomponent.cpp | 2 | c7900 c7ac0 | ResolveAddr(Exe)×2 |
| code/Game/Component/livestockcomponent.cpp | 1 | c79a8 | ResolveAddr(Exe)×1 |
| code/Game/Component/mapcomponent.cpp | 1 | c7a18 | ResolveAddr(Exe)×1 |
| code/Game/Component/modelcomponent.cpp | 1 | c7a88 | ResolveAddr(Exe)×1 |
| code/Game/Component/multiobjectcomponent.cpp | 2 | c3dd8 c72e8 | ResolveAddr(Exe)×2 |
| code/Game/Component/nomadcomponent.cpp | 4 | c3dd8 c6eb8 c6ef0 c72e8 | ResolveAddr(Exe)×4 |
| code/Game/Component/orchardcomponent.cpp | 6 | c3dd8 c4250 c4288 c46e8 c5d70 c7b30 | ResolveAddr(Exe)×6 |
| code/Game/Component/particlecomponent.cpp | 1 | c4758 | ResolveAddr(Exe)×1 |
| code/Game/Component/pasturecomponent.cpp | 3 | c6f98 c6fd0 c7970 | ResolveAddr(Exe)×3 |
| code/Game/Component/pasturecomponent_spawn.cpp | 2 | c6f98 c7970 | ResolveAddr(Exe)×2 |
| code/Game/Component/pasturecomponent_states.cpp | 4 | c3fb0 c6f98 c79a8 c7c80 | wrapper Tid×4 |
| code/Game/Component/pickingcomponent.cpp | 1 | c72e8 | ResolveAddr(Exe)×1 |
| code/Game/Component/populationcomponent.cpp | 4 | c3dd8 c47c8 c7820 c7858 | ResolveAddr(Exe)×1, wrapper PopulationTypeId×3 |
| code/Game/Component/rawmaterialcomponent.cpp | 1 | c3dd8 | ResolveAddr(Exe)×1 |
| code/Game/Component/residence_heattick.cpp | 1 | c7d28 | ResolveAddr(Exe)×1 |
| code/Game/Component/residencecomponent.cpp | 2 | c7158 c7d28 | ResolveAddr(Exe)×2 |
| code/Game/Component/spawncomponent.cpp | 2 | c4918 c77e8 | ResolveAddr(Exe)×2 |
| code/Game/Component/statemachinecomponent.cpp | 1 | c73c0 | ResolveAddr(Exe)×1 |
| code/Game/Component/statusiconcomponent.cpp | 1 | c7e40 | ResolveAddr(Exe)×1 |
| code/Game/Component/storagecomponent.cpp | 1 | c7eb0 | ResolveAddr(Exe)×1 |
| code/Game/Component/storagedisplaycomponent.cpp | 2 | c3dd8 c7a88 | ResolveAddr(Exe)×2 |
| code/Game/Component/storagelocationcomponent.cpp | 1 | c3dd8 | ResolveAddr(Exe)×1 |
| code/Game/Component/terraformcomponent.cpp | 1 | c49f8 | ResolveAddr(Exe)×1 |
| code/Game/Component/tombstonecomponent.cpp | 2 | c3dd8 c4a68 | ResolveAddr(Exe)×2 |
| code/Game/Component/tradecomponent.cpp | 1 | c4bb8 | ResolveAddr(Exe)×1 |
| code/Game/Component/tradecomponent_livestock.cpp | 1 | c6f98 | ResolveAddr(Exe)×1 |
| code/Game/Component/wellcomponent.cpp | 1 | c3dd8 | ResolveAddr(Exe)×1 |
| code/Game/Component/wildanimalcomponent.cpp | 1 | c7820 | ResolveAddr(Exe)×1 |
| code/Game/Component/workplacecomponent.cpp | 3 | c7158 c80e0 c8118 | ResolveAddr(Exe)×3 |
| code/Game/Component/zonecomponent.cpp | 2 | c3dd8 c7a18 | ResolveAddr(Exe)×2 |
| code/Game/Dialog/dialog_exe_helpers.cpp | 1 | c7008 | ResolveAddr(Exe)×1 |
| code/Game/Dialog/edittool_select.cpp | 4 | c4f00 c5788 c7f90 c7fc8 | wrapper ResolveTypeId×4 |
| code/Game/Dialog/tradeui_checkout.cpp | 4 | c4bb8 c79a8 c7b68 c7cb8 | ResolveAddr(Exe)×4 |
| code/Game/Game/citizenlist_serialize.cpp | 7 | c4170 c42c0 c4790 c7008 c7820 c7970 c79e0 | ResolveAddr(Exe)×7 |
| code/Game/Game/gameupdateloop.cpp | 2 | c3de0 c4138 | ResolveAddr(Exe)×2 |
| code/Game/Game/professionlist.cpp | 1 | c5a98 | ResolveAddr(Exe)×1 |
| code/Game/Game/starttown.cpp | 1 | c8000 | ResolveAddr(Exe)×1 |
| code/Game/Game/town_storage_seed.cpp | 4 | c79e0 c7cb8 c7e78 c7eb0 | wrapper Tid×4 |
| code/Game/Game/unreg_lane5_musicplayer.cpp | 1 | c7190 | ResolveAddr(Exe)×1 |
| code/Game/Goal/statvaluedispatch.cpp | 16 | c48a8 c4a30 c4b10 c5910 c5a98 c5b40 c7008 c7040 c7318 c75f0 c79e0 c7cb8 c7d98 c7e78 c80e0 c8118 | ResolveAddr(Exe)×16 |
| code/Game/Player/edittool.cpp | 2 | c4f00 c5788 | ResolveAddr(Exe)×2 |
| code/Game/Player/fencedtool.cpp | 1 | c63c8 | ResolveAddr(Exe)×1 |
| code/Game/Player/keystone_a_removalcalcpath.cpp | 5 | c6860 c7430 c75f0 c79e0 c8070 | ResolveAddr(Exe)×5 |
| code/Game/Player/placementtool.cpp | 1 | c63c8 | ResolveAddr(Exe)×1 |
| code/Game/Player/roadtool.cpp | 2 | c74d8 c75b8 | ResolveAddr(Exe)×2 |
| code/Game/Player/tool_base_path.cpp | 3 | c7430 c75f0 c7d98 | ResolveAddr(Exe)×3 |
| code/Game/Player/tool_base_tile.cpp | 1 | c7a88 | ResolveAddr(Exe)×1 |
| code/Game/Player/tool_base_toolui.cpp | 1 | c7f58 | ResolveAddr(Exe)×1 |
| code/Game/Player/tool_base_trade.cpp | 1 | c5788 | ResolveAddr(Exe)×1 |
| code/Game/Tutorial/finishtutorialinstance.cpp | 1 | c6668 | ResolveAddr(Exe)×1 |
| code/Game/Tutorial/tutorialgoalcount.cpp | 2 | c5a98 c7318 | ResolveAddr(Exe)×2 |
| code/Game/Work/BuildRoad.h | 1 | c6860 | named constexpr×1 |
| code/Game/Work/build_actions.cpp | 3 | c7008 c70e8 c75f0 | wrapper rd×3 |
| code/Game/Work/buildroad.cpp | 1 | c7158 | ResolveAddr(Exe)×1 |
| code/Game/Work/buildstructure.cpp | 1 | c7158 | ResolveAddr(Exe)×1 |
| code/Game/Work/cleararea.cpp | 1 | c7c80 | engine_funcs g_TypeId_ initialiser×1 |
| code/Game/Work/consumeproduce.cpp | 1 | c7158 | ResolveAddr(Exe)×1 |
| code/Game/Work/firework.cpp | 2 | c7190 c7938 | ResolveAddr(Exe)×2 |
| code/Game/Work/firework_search.cpp | 1 | c7900 | ResolveAddr(Exe)×1 |
| code/Game/Work/fishingwork.cpp | 6 | c4870 c7158 c7660 c7698 c7e78 c8070 | ResolveAddr(Exe)×6 |
| code/Game/Work/foresterwork.cpp | 8 | c4870 c7740 c7778 c77e8 c7b30 c7b68 c7cb8 c80a8 | ResolveAddr(Exe)×8 |
| code/Game/Work/gatherrawmaterials.cpp | 3 | c7040 c7938 c7cb8 | ResolveAddr(Exe)×3 |
| code/Game/Work/harvestarea.cpp | 1 | c7548 | ResolveAddr(Exe)×1 |
| code/Game/Work/harvestorchard.cpp | 2 | c4288 c46e8 | ResolveAddr(Exe)×2 |
| code/Game/Work/herbalistwork.cpp | 3 | c43a0 c43d8 c4870 | ResolveAddr(Exe)×3 |
| code/Game/Work/huntwork.cpp | 1 | c78c8 | ResolveAddr(Exe)×1 |
| code/Game/Work/locationwork.cpp | 2 | c7158 c7938 | ResolveAddr(Exe)×2 |
| code/Game/Work/marketwork.cpp | 10 | c3dd8 c44f0 c4528 c7350 c7900 c7938 c79e0 c7cb8 c7e78 c7ee8 | ResolveAddr(Exe)×1, wrapper ?×9 |
| code/Game/Work/marketwork_bst_b290.cpp | 1 | c7158 | ResolveAddr(Exe)×1 |
| code/Game/Work/pasturework.cpp | 3 | c6fd0 c7ba0 c7bd8 | ResolveAddr(Exe)×3 |
| code/Game/Work/pickup.cpp | 3 | c6be0 c7158 c7cb8 | ResolveAddr(Exe)×3 |
| code/Game/Work/pickup_gate.cpp | 4 | c7cb8 c7e78 c7ee8 c7f20 | ResolveAddr(Exe)×4 |
| code/Game/Work/plantarea.cpp | 1 | c7510 | ResolveAddr(Exe)×1 |
| code/Game/Work/plantorchard.cpp | 1 | c46b0 | ResolveAddr(Exe)×1 |
| code/Game/Work/tendarea.cpp | 2 | c46b0 c7510 | ResolveAddr(Exe)×2 |
| code/Game/Work/tradework.cpp | 3 | c7938 c7cb8 c7e78 | ResolveAddr(Exe)×3 |
| code/Game/Work/work_act_clear.cpp | 2 | c70e8 c7b68 | ResolveAddr(Exe)×2 |
| code/Game/Work/work_act_consume.cpp | 2 | c70e8 c7388 | ResolveAddr(Exe)×2 |
| code/Game/Work/work_act_hunt.cpp | 1 | c70e8 | ResolveAddr(Exe)×1 |
| code/Game/Work/work_act_path.cpp | 2 | c70e8 c7158 | ResolveAddr(Exe)×2 |
| code/Game/Work/work_act_perform.cpp | 2 | c41e0 c70e8 | ResolveAddr(Exe)×2 |
| code/Game/Work/work_act_rawmat.cpp | 3 | c46b0 c70e8 c7660 | ResolveAddr(Exe)×3 |
| code/Game/Work/work_base_accessor.cpp | 4 | c3fb0 c46b0 c7510 c7cb8 | ResolveAddr(Exe)×4 |
| code/Game/Work/work_base_actionctor.cpp | 2 | c70e8 c7b68 | ResolveAddr(Exe)×2 |
| code/Game/Work/work_base_reimpl.cpp | 7 | c4b80 c5e50 c7900 c7b30 c7e08 c7e78 c8070 | ResolveAddr(Exe)×7 |
| code/Game/Work/work_base_walkpos.cpp | 2 | c7190 c7938 | ResolveAddr(Exe)×2 |
| code/Game/Work/work_leaf_gather.cpp | 2 | c77e8 | ResolveAddr(Exe)×2 |
| code/Game/Work/work_leaf_hunt.cpp | 7 | c4250 c4870 c6f98 c6fd0 c7388 c7890 c7ba0 | ResolveAddr(Exe)×7 |
| engine/funcs/engine_funcs.cpp | 83 | c3dd8 c3de0 c3de8 c3f40 c3f78 c4090 c4100 c41e0 c4480 c46b0 c47c8 c4870 c48a8 c4a30 c4b10 c4b80 c4bb8 c4bf0 c4d08 c4e20 c4f00 c5210 c5520 c5638 c56e0 c5788 c5868 c5910 c5948 c59b8 c5a60 c5a98 c5b08 c5b40 c5d38 c5d70 c6128 c6198 c6240 c6dd8 c6eb8 c6ef0 c6fd0 c7008 c7040 c70e8 c7158 c7190 c71c8 c72a8 c7318 c7350 c7388 c73f8 c7430 c7468 c74a0 c75f0 c7628 c7900 c7938 c79e0 c7a18 c7a50 c7a88 c7ac0 c7ba0 c7c10 c7cb8 c7cf0 c7d98 c7e08 c7e40 c7e78 c7ee8 c7f58 c7f90 c8070 c80a8 c80e0 | engine_funcs g_TypeId_ initialiser×75, ResolveAddr(Exe)×8 |
| engine/funcs/engine_funcs.h | 1 | c4f00 | wrapper ResolveExeTypeId×1 |

## Table 3 — the `g_TypeId_*` globals (engine_funcs and others): cell, class AT the cell, readers

| g_TypeId_ global | initialiser address(es) | retail class at that address | readers (excl. decl/init) | by dir | NAME ≠ CLASS |
|---|---|---|---:|---|---|
| g_TypeId_AnimalDescription | 0x1401c6fd0 | AnimalDescription | 2 | Dialog:2 |  |
| g_TypeId_AreaDescription | 0x1401c7a18 | MapDescription | 18 | Component:4 Player:14 | **YES** |
| g_TypeId_AttendanceComponent | 0x1401c3f40 | AttendanceComponent | 7 | Dialog:5 Game:1 engine:1 |  |
| g_TypeId_AttendanceDescription | 0x1401c3f78 | AttendanceDescription | 6 | Dialog:6 |  |
| g_TypeId_AutoSaveDialogType | 0x1401c4d08 | AutoSaveDialog | 3 | Dialog:3 |  |
| g_TypeId_BrowserController | 0x1401c5210 | ModBrowseUI | 2 | Dialog:2 | **YES** |
| g_TypeId_BuildComponent_FC | 0x1401c7008 | BuildComponent | 3 | Dialog:2 engine:1 |  |
| g_TypeId_ButtonElement | (none found) | - | 6 | Dialog:5 engine:1 |  |
| g_TypeId_CaptureBoxElement | 0x1401c5638 | CaptureBoxElement | 2 | Dialog:2 |  |
| g_TypeId_CemeteryComponent | 0x1401c4090 | CemeteryComponent | 10 | Dialog:5 Game:1 engine:4 |  |
| g_TypeId_CheckElement | (none found) | - | 6 | Dialog:5 engine:1 |  |
| g_TypeId_CheckboxElement | (none found) | - | 7 | Dialog:6 engine:1 |  |
| g_TypeId_CitizenAIComponent | 0x1401c70e8 | CitizenAIComponent | 7 | Dialog:6 engine:1 |  |
| g_TypeId_CitizenComponent_FC | 0x1401c7158 | CitizenComponent | 23 | Action:1 Component:3 Dialog:7 Game:4 Work:7 engine:1 |  |
| g_TypeId_CitizenDescription | 0x1401c7190 | CitizenDescription | 24 | Dialog:17 Entity:4 Game:3 |  |
| g_TypeId_ClearAreaComponent_FC | 0x1401c71c8 | ClearAreaComponent | 3 | Dialog:2 engine:1 |  |
| g_TypeId_ClimateGenerator | 0x1401c5910 | ClimateGenerator | 6 | Dialog:6 |  |
| g_TypeId_ComboElement | (none found) | - | 13 | Dialog:12 engine:1 |  |
| g_TypeId_ComponentBase | 0x1401c72a8 | Component | 7 | Entity:6 engine:1 | **YES** |
| g_TypeId_ComponentDescription_FC | 0x1401c7318 | ComponentDescription | 41 | Dialog:26 Game:1 Tutorial:6 engine:8 |  |
| g_TypeId_ComponentEntity | 0x1401c5d70 | ComponentEntity | 189 | Action:11 Component:5 Dialog:154 Entity:1 Game:10 Tutorial:4 Work:4 |  |
| g_TypeId_ConsumeProduceComponent | 0x1401c7388 | ConsumeProduceDescription | 11 | Component:2 Dialog:7 Work:2 | **YES** |
| g_TypeId_ConsumeProduceComponent_FC | 0x1401c7350 | ConsumeProduceComponent | 4 | Work:3 engine:1 |  |
| g_TypeId_ControlComponent | 0x1401c4e20 | ControlUIConfig | 9 | Dialog:9 | **YES** |
| g_TypeId_CreateBridgeDescription | 0x1401c73f8 | CreateBridgeDescription | 2 | Player:2 |  |
| g_TypeId_CreateFencedDescription | 0x1401c4100 | CreateFencedDescription | 3 | Player:3 |  |
| g_TypeId_CreatePlacementDescription | 0x1401c7430 | CreateDragDescription | 2 | Player:2 | **YES** |
| g_TypeId_DestroyComponent | 0x1401c7628 | DestroyDescription | 10 | Dialog:7 engine:3 | **YES** |
| g_TypeId_DestroyComponent_FC | 0x1401c75f0 | DestroyComponent | 1 | engine:1 |  |
| g_TypeId_Dialog | (none found) | - | 21 | Dialog:15 Player:2 engine:4 |  |
| g_TypeId_DialogController | 0x1401c56e0 | StatusIconUIConfig | 0 |  | **YES** |
| g_TypeId_DiseaseDescription | 0x1401c5948 | Disease | 5 | Dialog:5 | **YES** |
| g_TypeId_DrawableType | 0x1401c47c8 | PopulationDescription | 4 | Dialog:4 | **YES** |
| g_TypeId_EditElement | (none found) | - | 3 | Dialog:2 engine:1 |  |
| g_TypeId_EditTool | 0x1401c6dd8 | EditTool | 9 | Dialog:4 engine:5 |  |
| g_TypeId_Element | (none found) | - | 24 | Dialog:21 Tutorial:2 engine:1 |  |
| g_TypeId_FenceDescription | 0x1401c7a88 | ModelDescription | 10 | Player:10 | **YES** |
| g_TypeId_FinishTutorialGoal | 0x1401c6240 | FinishTutorialGoal | 5 | Tutorial:4 engine:1 |  |
| g_TypeId_FireComponent | 0x1401c41e0 | FireComponent | 1 | engine:1 |  |
| g_TypeId_HelpButtonElem | exref 0x1401514f8 | exref 0x1401514f8 | 21 | Dialog:19 engine:2 |  |
| g_TypeId_HelpComboElem | exref 0x140151690 | exref 0x140151690 | 9 | Dialog:8 engine:1 |  |
| g_TypeId_HelpContainerElem | exref 0x1401517f8 | exref 0x1401517f8 | 6 | Dialog:5 engine:1 |  |
| g_TypeId_HelpDocTextElem | exref 0x1401517f0 | exref 0x1401517f0 | 6 | Dialog:5 engine:1 |  |
| g_TypeId_HelpFirstElem | exref 0x1401517e8 | exref 0x1401517e8 | 6 | Dialog:5 engine:1 |  |
| g_TypeId_ImageElement | (none found) | - | 29 | Dialog:24 Tutorial:4 engine:1 |  |
| g_TypeId_ImmigrationComponent | 0x1401c4480 | ImmigrationComponent | 4 | Dialog:3 engine:1 |  |
| g_TypeId_InteractComponent_FC | 0x1401c7900 | InteractComponent | 4 | Dialog:3 engine:1 |  |
| g_TypeId_InteractDescription_FC | 0x1401c7938 | InteractDescription | 5 | Component:3 Dialog:2 |  |
| g_TypeId_LabelElement | (none found) | - | 10 | Dialog:9 engine:1 |  |
| g_TypeId_MapComponent_FC | 0x1401c79e0 | MapComponent | 6 | Dialog:2 engine:4 |  |
| g_TypeId_MapGenerator | 0x1401c59b8 | MapGenerator | 9 | Dialog:9 |  |
| g_TypeId_Material | (none found) | - | 10 | Dialog:9 engine:1 |  |
| g_TypeId_MeshInstanceComponent | 0x1401c7e40 | StatusIconDescription | 3 | engine:3 | **YES** |
| g_TypeId_ModelComponent | 0x1401c7a50 | ModelComponent | 0 |  |  |
| g_TypeId_ModelComponent_FC | 0x1401c7a50 | ModelComponent | 5 | Dialog:4 engine:1 |  |
| g_TypeId_MultiObjectComponent | 0x1401c7ac0 | MultiObjectComponent | 3 | Dialog:2 engine:1 |  |
| g_TypeId_NameDatabase | 0x1401c5a60 | NameDatabase | 3 | Dialog:3 |  |
| g_TypeId_NaturalResourceDesc | 0x1401c7c80 | RawMaterialComponent | 2 | Work:2 | **YES** |
| g_TypeId_NumberEditElement | (none found) | - | 9 | Dialog:8 engine:1 |  |
| g_TypeId_NumberElement | (none found) | - | 13 | Dialog:12 engine:1 |  |
| g_TypeId_OrchardComponent | 0x1401c46b0 | OrchardComponent | 1 | engine:1 |  |
| g_TypeId_PageToggleUI | 0x1401c4f00 | EntityUI | 0 |  | **YES** |
| g_TypeId_PastureComponent | 0x1401c7ba0 | PastureComponent | 2 | Game:1 engine:1 |  |
| g_TypeId_PickingComponent | 0x1401c7c10 | PickingComponent | 0 |  |  |
| g_TypeId_ProfessionDescription | 0x1401c5a98 | Profession | 0 |  | **YES** |
| g_TypeId_ProgressElement | (none found) | - | 28 | Dialog:27 engine:1 |  |
| g_TypeId_RadiusComponent | 0x1401c4870 | RadiusDescription | 10 | Dialog:2 Player:8 | **YES** |
| g_TypeId_RawMaterialDescription | 0x1401c7cb8 | RawMaterialDescription | 91 | Action:4 Component:23 Dialog:49 Entity:4 Work:4 engine:7 |  |
| g_TypeId_RemovalDescription | 0x1401c74a0 | CreateRemovalDescription | 2 | Player:2 | **YES** |
| g_TypeId_ResidenceComponent | 0x1401c7cf0 | ResidenceComponent | 11 | Dialog:5 Entity:6 |  |
| g_TypeId_ResourceComponent | 0x1401c7040 | BuildDescription | 13 | Dialog:13 | **YES** |
| g_TypeId_ResourceLimitType | 0x1401c5520 | ResourceLimitUIConfig | 3 | Dialog:3 | **YES** |
| g_TypeId_Scenario | 0x1401c5b08 | Scenario | 6 | Dialog:3 Tutorial:3 |  |
| g_TypeId_SelectDescription | 0x1401c48a8 | SelectDescription | 20 | Dialog:16 engine:4 |  |
| g_TypeId_SizeDescription | 0x1401c7468 | CreatePlacedDescription | 12 | Player:12 | **YES** |
| g_TypeId_SpinnerElement | (none found) | - | 4 | Tutorial:3 engine:1 |  |
| g_TypeId_StateMachineComponent | (none found) | - | 0 |  |  |
| g_TypeId_StateMachineComponent_FC | 0x1401c7d98 | StateMachineComponent | 10 | Dialog:9 engine:1 |  |
| g_TypeId_StatusIconComponent | 0x1401c7e08 | StatusIconComponent | 14 | Dialog:10 Work:3 engine:1 |  |
| g_TypeId_StorageComponent | (none found) | - | 0 |  |  |
| g_TypeId_StorageComponent_FC | 0x1401c7e78 | StorageComponent | 23 | Dialog:7 Entity:2 Work:10 engine:4 |  |
| g_TypeId_StringTable | (none found) | - | 25 | Dialog:19 Game:5 engine:1 |  |
| g_TypeId_TabElement | (none found) | - | 13 | Dialog:12 engine:1 |  |
| g_TypeId_TableElement | (none found) | - | 13 | Dialog:12 engine:1 |  |
| g_TypeId_TerrainGenerator | 0x1401c5b40 | TerrainGenerator | 6 | Dialog:6 |  |
| g_TypeId_TextElement | (none found) | - | 40 | Dialog:29 Tutorial:8 engine:1 standalone:2 |  |
| g_TypeId_TombstoneComponent | 0x1401c4a30 | TombstoneComponent | 7 | Dialog:7 |  |
| g_TypeId_ToolController | 0x1401c6eb8 | Tool | 3 | Dialog:1 Player:2 | **YES** |
| g_TypeId_ToolSetting | 0x1401c6ef0 | ToolSetting | 3 | Dialog:1 Player:2 |  |
| g_TypeId_ToolUI | 0x1401c5788 | ToolUI | 10 | Dialog:10 |  |
| g_TypeId_ToolbarDescription_FC | 0x1401c7f58 | ToolbarDescription | 20 | Dialog:17 Tutorial:3 |  |
| g_TypeId_ToolbarDialogParent | 0x1401c5d38 | ToolbarDialog | 1 | Dialog:1 |  |
| g_TypeId_ToolbarExternal | (none found) | - | 2 | Game:1 engine:1 |  |
| g_TypeId_ToolbarItem | 0x1401c6128 | Toolbar | 6 | Dialog:3 Tutorial:3 | **YES** |
| g_TypeId_TrackerComponent | 0x1401c4b10 | TrackerComponent | 5 | Dialog:4 engine:1 |  |
| g_TypeId_TradeComponent_FC | 0x1401c4b80 | TradeComponent | 5 | Dialog:4 engine:1 |  |
| g_TypeId_UIDescription | 0x1401c7f90 | UIDescription | 16 | Dialog:16 |  |
| g_TypeId_UnknownComp_7ee8 | 0x1401c7ee8 | StorageLocationComponent | 1 | engine:1 | **YES** |
| g_TypeId_UpgradeUIConfig | 0x1401c5868 | UpgradeUIConfig | 8 | Dialog:8 |  |
| g_TypeId_WorkComponent | 0x1401c8070 | WorkComponent | 16 | Dialog:8 Game:3 Work:4 engine:1 |  |
| g_TypeId_WorkDescription | 0x1401c80a8 | WorkDescription | 8 | Dialog:7 engine:1 |  |
| g_TypeId_WorkplaceComponent | 0x1401c80e0 | WorkPlaceComponent | 15 | Component:1 Dialog:13 engine:1 |  |

## Table 4 — the `g_tid_*` storage globals: direct readers by name

| g_tid_ storage | direct readers by name (file:n, excl. decl + its own fill) |
|---|---|
| g_tid_AnimalComponent | standalone/reg_component_animal.cpp:2 |
| g_tid_AnimalDescription | standalone/reg_work_tail_serialize.cpp:2 |
| g_tid_AttendanceComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_AttendanceDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_AutoSaveDialog | standalone/reg_autosave.cpp:2 |
| g_tid_BoatComponent | standalone/reg_tradeui_typeids.cpp:3 |
| g_tid_BuildRoad | standalone/reg_economy_typeids.cpp:2 |
| g_tid_CemeteryComponent | standalone/reg_town_services.cpp:2 |
| g_tid_CemeteryDescription | standalone/reg_citizen_lifeevent_handler.cpp:2 |
| g_tid_CharacterComponent | standalone/reg_animal_update.cpp:3 |
| g_tid_CharacterDescription | standalone/reg_animal_update.cpp:3 |
| g_tid_CitizenAIComponent | standalone/reg_citizenai_typeids.cpp:2 |
| g_tid_CitizenComponent | standalone/reg_citizenai_typeids.cpp:3 |
| g_tid_CitizenDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_ClearAreaComponent | standalone/reg_cleararea_typeids.cpp:4 |
| g_tid_ClearAreaDescription | standalone/reg_cleararea_typeids.cpp:4 |
| g_tid_ComponentBase | standalone/reg_animal_serialize.cpp:3 |
| g_tid_ComponentDescription | standalone/reg_tradeui_typeids.cpp:3 |
| g_tid_ComponentEntity | standalone/reg_action_child_ctors2.cpp:2 |
| g_tid_ConsumeProduceComponent | standalone/reg_action_child_ctors2.cpp:2 |
| g_tid_ConsumeProduceDescription | standalone/reg_action_child_ctors2.cpp:2 |
| g_tid_ControlUIConfig | standalone/reg_controlui.cpp:2 |
| g_tid_CreateBridgeDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_CreateDragDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_CreateFencedDescription | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_CreatePlacedDescription | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_CreateRemovalDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_DecalComponent | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_DestroyComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_DestroyDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_DiseaseComponent | standalone/reg_component_cropdisease.cpp:2 |
| g_tid_DiseaseDescription | standalone/reg_component_cropdisease.cpp:2 |
| g_tid_FinishTutorialGoal | standalone/reg_tutorial_typeid.cpp:2 |
| g_tid_FinishTutorialInstance | standalone/reg_tutorial_typeid.cpp:2 |
| g_tid_FireComponent | code/Game/Component/firecomponent_ignite.cpp:2 standalone/reg_fire.cpp:1 |
| g_tid_FireDescription | code/Game/Component/firecomponent.cpp:1 code/Game/Component/firecomponent_ignite.cpp:3 standalone/reg_fire.cpp:1 |
| g_tid_ForesterComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_ForesterDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_FruitComponent | standalone/reg_component_fruit.cpp:2 |
| g_tid_FruitDescription | standalone/reg_component_fruit.cpp:2 |
| g_tid_GrowthComponent | standalone/reg_growth_typeid.cpp:5 |
| g_tid_GrowthDescription | standalone/reg_component_spawn.cpp:2 |
| g_tid_HappinessComponent | standalone/reg_citizenai_typeids.cpp:2 |
| g_tid_HappinessDescription | standalone/reg_town_services.cpp:2 |
| g_tid_HealerComponent | code/Game/Game/citizenlist_serialize.cpp:2 |
| g_tid_HerdComponent | standalone/reg_component_animal.cpp:2 |
| g_tid_HerdDescription | standalone/reg_component_animal.cpp:2 |
| g_tid_InteractComponent | standalone/reg_work_tail_fire.cpp:2 |
| g_tid_InteractDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_LivestockComponent | standalone/reg_component_animal.cpp:2 |
| g_tid_LivestockDescription | standalone/reg_component_animal.cpp:2 |
| g_tid_MapComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_MapDescription | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_MarketComponent | standalone/reg_market_typeid.cpp:5 |
| g_tid_MarketDescription | standalone/reg_market_typeid.cpp:5 |
| g_tid_ModelComponent | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_ModelDescription | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_MultiObjectComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_NaturalResourceComponent | standalone/reg_component_nature.cpp:2 |
| g_tid_NaturalResourceDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_OrchardComponent | standalone/reg_orchard_typeid.cpp:5 |
| g_tid_OrchardDescription | standalone/reg_orchard_typeid.cpp:5 |
| g_tid_ParticleComponent | standalone/reg_tornado.cpp:6 |
| g_tid_ParticleDescription | standalone/reg_w2_lane3.cpp:3 |
| g_tid_PastureComponent | standalone/reg_work_tail_serialize.cpp:2 |
| g_tid_PastureDescription | standalone/reg_work_tail_serialize.cpp:2 |
| g_tid_PopulationComponent | standalone/reg_animal_serialize.cpp:3 |
| g_tid_PopulationDescription | standalone/reg_herd_live.cpp:1 |
| g_tid_Profession | standalone/reg_tutorial_typeid.cpp:2 |
| g_tid_RadiusDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_RawMaterialComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_RawMaterialDescription | standalone/reg_action_child_ctors2.cpp:2 |
| g_tid_ResidenceComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_ResidenceDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_RotationSetting | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_Scenario | standalone/reg_autosave.cpp:2 |
| g_tid_SelectDescription | standalone/reg_pipeline_typeids.cpp:2 |
| g_tid_SpawnDescription | standalone/reg_component_spawn.cpp:2 |
| g_tid_StateMachineComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_StatusIconComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_StatusIconDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_StorageComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_StorageLocationComponent | standalone/reg_market_typeid.cpp:5 |
| g_tid_TerraformDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_TombstoneComponent | standalone/reg_citizen_lifeevent_handler.cpp:2 |
| g_tid_TombstoneDescription | standalone/reg_component_tombstone.cpp:2 |
| g_tid_ToolbarDialog | standalone/reg_economy_typeids.cpp:3 |
| g_tid_TrackerComponent | standalone/reg_tracker_typeid.cpp:5 |
| g_tid_TradeComponent | standalone/reg_tradeui_typeids.cpp:3 |
| g_tid_TradeDescription | standalone/reg_tradeui_typeids.cpp:3 |
| g_tid_TradeWork_CitizenComponent | code/Game/Work/tradework_serialize.cpp:4 standalone/reg_work_tail_trade.cpp:1 |
| g_tid_TutorialInstance | standalone/reg_tutorial_typeid.cpp:2 |
| g_tid_ValueDescription | standalone/reg_tradeui_typeids.cpp:3 |
| g_tid_WellComponent | code/Game/Game/citizenlist_serialize.cpp:2 |
| g_tid_WildAnimalComponent | standalone/reg_component_animal.cpp:2 |
| g_tid_Work | code/Game/Game/goals_serialize.cpp:7 |
| g_tid_WorkComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_WorkDescription | standalone/reg_economy_typeids.cpp:2 |
| g_tid_WorkPlaceComponent | standalone/reg_economy_typeids.cpp:2 |
| g_tid_WorkPlaceDescription | standalone/reg_economy_typeids.cpp:2 |

## Table 5 — Runtime `?_typeInfo@` records: every reference by binding (name literal, exe IAT slot, Runtime address)

| Runtime record | refs | by dir | code/Game + engine sites (file:line form) |
|---|---:|---|---|
| `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` | 6 | Game:1 runtime:3 standalone:2 | code/Game/Game/gameupdateloop_load.cpp:85 name ResolveExport |
| `AchievementInstance::Achievement` | 3 | Game:1 runtime:2 | code/Game/Game/gameupdateloop_serialize.cpp:177 name ResolveExport |
| `AnimatedImageDescription::UI` | 1 | runtime:1 |  |
| `AnimatedImageElement::UI` | 1 | runtime:1 |  |
| `AnimationEvent::Graphics` | 2 | runtime:2 |  |
| `AnimationGroup::Graphics` | 2 | runtime:2 |  |
| `BorderDescription::UI` | 1 | runtime:1 |  |
| `BorderElement::UI` | 3 | runtime:1 standalone:2 |  |
| `ButtonDescription::UI` | 1 | runtime:1 |  |
| `ButtonElement::UI` | 8 | Dialog:1 engine:3 runtime:2 standalone:2 | code/Game/Dialog/helpui.cpp:223 name ?<br>engine/funcs/engine_funcs.cpp:1032 IAT read<br>engine/funcs/engine_funcs.cpp:1429 IAT read<br>engine/funcs/engine_funcs.cpp:1717 IAT read |
| `CharacterSet::UI` | 2 | runtime:2 |  |
| `CheckDescription::UI` | 1 | runtime:1 |  |
| `CheckElement::UI` | 8 | engine:2 runtime:2 standalone:4 | engine/funcs/engine_funcs.cpp:1025 IAT read<br>engine/funcs/engine_funcs.cpp:1295 IAT read |
| `ClipRegionDescription::UI` | 1 | runtime:1 |  |
| `ClipRegionElement::UI` | 7 | runtime:5 standalone:2 |  |
| `CollisionMesh::Collision2D` | 1 | runtime:1 |  |
| `ComboDescription::UI` | 1 | runtime:1 |  |
| `ComboDialog::UI` | 1 | runtime:1 |  |
| `ComboElement::UI` | 6 | Dialog:1 engine:2 runtime:1 standalone:2 | code/Game/Dialog/helpui.cpp:220 name ?<br>engine/funcs/engine_funcs.cpp:1305 IAT read<br>engine/funcs/engine_funcs.cpp:1426 IAT read |
| `CompiledExternal::Object` | 3 | runtime:2 standalone:1 |  |
| `ContainerDescription::UI` | 1 | runtime:1 |  |
| `ContainerElement::UI` | 6 | Dialog:1 engine:1 runtime:2 standalone:2 | code/Game/Dialog/helpui.cpp:222 name ?<br>engine/funcs/engine_funcs.cpp:1428 IAT read |
| `Cursor::UI` | 1 | runtime:1 |  |
| `Dialog::UI` | 9 | Dialog:2 engine:1 runtime:3 standalone:3 | code/Game/Dialog/mainmenudialog.cpp:89 IAT read<br>code/Game/Dialog/mod_dialog_vtables.cpp:103 IAT read<br>engine/funcs/engine_funcs.cpp:896 Runtime addr |
| `DialogController::UI` | 4 | runtime:2 standalone:2 |  |
| `DialogControllerConfig::UI` | 2 | runtime:2 |  |
| `DialogInstance::UI` | 3 | runtime:1 standalone:2 |  |
| `DocumentTextDescription::UI` | 1 | runtime:1 |  |
| `DocumentTextElement::UI` | 5 | Dialog:1 engine:1 runtime:1 standalone:2 | code/Game/Dialog/helpui.cpp:221 name ?<br>engine/funcs/engine_funcs.cpp:1427 IAT read |
| `EdgeMesh::Graphics` | 4 | runtime:2 standalone:2 |  |
| `EditDescription::UI` | 1 | runtime:1 |  |
| `EditElement::UI` | 4 | engine:1 runtime:1 standalone:2 | engine/funcs/engine_funcs.cpp:1709 IAT read |
| `Element::UI` | 6 | engine:1 runtime:1 standalone:4 | engine/funcs/engine_funcs.cpp:1039 IAT read |
| `ElementDescription::UI` | 3 | runtime:3 |  |
| `ExtensionCompiler::Object` | 1 | runtime:1 |  |
| `External::Object` | 11 | Goal:1 runtime:6 standalone:4 | code/Game/Goal/goal_exe_helpers.cpp:97 IAT read |
| `ExternalCompiler::Object` | 1 | runtime:1 |  |
| `ExternalList::Object` | 8 | Game:2 runtime:4 standalone:2 | code/Game/Game/gameupdateloop_load.cpp:231 name ?<br>code/Game/Game/gameupdateloop_load.cpp:287 name ? |
| `Font::UI` | 3 | runtime:3 |  |
| `FrameCopyImageBuffer::Graphics` | 4 | runtime:2 standalone:2 |  |
| `GeneratedImageBuffer::Graphics` | 4 | runtime:2 standalone:2 |  |
| `Goal::Achievement` | 3 | runtime:3 |  |
| `GoalInstance::Achievement` | 6 | runtime:4 standalone:2 |  |
| `GraphDescription::UI` | 1 | runtime:1 |  |
| `GraphElement::UI` | 3 | runtime:1 standalone:2 |  |
| `GraphicsMesh::Graphics` | 8 | engine:1 runtime:3 standalone:4 | engine/funcs/engine_funcs.cpp:2503 IAT read |
| `GroupDescription::UI` | 1 | runtime:1 |  |
| `GroupElement::UI` | 2 | runtime:2 |  |
| `ImageBuffer::Video` | 5 | runtime:3 standalone:2 |  |
| `ImageDescription::UI` | 1 | runtime:1 |  |
| `ImageElement::UI` | 8 | Player:2 engine:1 runtime:1 standalone:4 | code/Game/Player/tool_base_createbar.cpp:118 IAT read<br>code/Game/Player/tool_base_createbar.cpp:121 name ResolveExport<br>engine/funcs/engine_funcs.cpp:1046 IAT read |
| `Internal::Object` | 3 | runtime:2 standalone:1 |  |
| `LabelDescription::UI` | 1 | runtime:1 |  |
| `LabelElement::UI` | 7 | engine:1 runtime:2 standalone:4 | engine/funcs/engine_funcs.cpp:1762 IAT read |
| `LayoutDescription::UI` | 1 | runtime:1 |  |
| `LayoutElement::UI` | 1 | runtime:1 |  |
| `ListDescription::UI` | 1 | runtime:1 |  |
| `ListElement::UI` | 3 | runtime:3 |  |
| `Material::Graphics` | 8 | Game:1 engine:1 runtime:2 standalone:4 | code/Game/Game/gameupdateloop_load.cpp:201 name ?<br>engine/funcs/engine_funcs.cpp:1818 IAT read |
| `MaterialInstance::Graphics` | 6 | engine:1 runtime:3 standalone:2 | engine/funcs/engine_funcs.cpp:2511 IAT read |
| `NumberDescription::UI` | 1 | runtime:1 |  |
| `NumberEditDescription::UI` | 1 | runtime:1 |  |
| `NumberEditElement::UI` | 8 | engine:1 runtime:3 standalone:4 | engine/funcs/engine_funcs.cpp:1767 IAT read |
| `NumberElement::UI` | 5 | engine:1 runtime:2 standalone:2 | engine/funcs/engine_funcs.cpp:1752 IAT read |
| `PackageFile::IO` | 1 | runtime:1 |  |
| `PageDescription::UI` | 1 | runtime:1 |  |
| `PageElement::UI` | 1 | runtime:1 |  |
| `Particle::Graphics` | 4 | engine:1 runtime:1 standalone:2 | engine/funcs/engine_funcs.cpp:2505 IAT read |
| `ParticleSystem::Graphics` | 3 | runtime:3 |  |
| `PickingMesh::Picking` | 1 | runtime:1 |  |
| `PixelProgram::Video` | 5 | runtime:3 standalone:2 |  |
| `PointList::Pathfinding` | 1 | runtime:1 |  |
| `ProgressDescription::UI` | 1 | runtime:1 |  |
| `ProgressElement::UI` | 4 | engine:1 runtime:1 standalone:2 | engine/funcs/engine_funcs.cpp:955 IAT read |
| `RadioDescription::UI` | 1 | runtime:1 |  |
| `RadioElement::UI` | 1 | runtime:1 |  |
| `ReflectionImageBuffer::Graphics` | 4 | runtime:2 standalone:2 |  |
| `RenderState::Video` | 5 | runtime:3 standalone:2 |  |
| `RibbonDescription::UI` | 1 | runtime:1 |  |
| `RibbonElement::UI` | 4 | runtime:2 standalone:2 |  |
| `ScrollDescription::UI` | 1 | runtime:1 |  |
| `ScrollElement::UI` | 9 | Dialog:1 engine:1 runtime:3 standalone:4 | code/Game/Dialog/helpui.cpp:219 name ?<br>engine/funcs/engine_funcs.cpp:1425 IAT read |
| `ShaderProgram::Video` | 1 | runtime:1 |  |
| `SliderDescription::UI` | 1 | runtime:1 |  |
| `SliderElement::UI` | 3 | runtime:1 standalone:2 |  |
| `SoundBuffer::Audio` | 2 | runtime:2 |  |
| `SoundEffect::Audio` | 2 | runtime:2 |  |
| `SpinnerDescription::UI` | 1 | runtime:1 |  |
| `SpinnerElement::UI` | 4 | engine:1 runtime:1 standalone:2 | engine/funcs/engine_funcs.cpp:1772 IAT read |
| `SpriteCapture::UI` | 3 | runtime:3 |  |
| `SpriteSheet::UI` | 5 | runtime:3 standalone:2 |  |
| `StringTable::UI` | 4 | Dialog:1 engine:1 runtime:2 | code/Game/Dialog/helpui.cpp:224 name ?<br>engine/funcs/engine_funcs.cpp:1412 IAT read |
| `Surface::Collision2D` | 3 | runtime:3 |  |
| `TabDescription::UI` | 1 | runtime:1 |  |
| `TabElement::UI` | 4 | engine:1 runtime:1 standalone:2 | engine/funcs/engine_funcs.cpp:1644 IAT read |
| `TableDescription::UI` | 1 | runtime:1 |  |
| `TableElement::UI` | 4 | engine:1 runtime:1 standalone:2 | engine/funcs/engine_funcs.cpp:1747 IAT read |
| `TextDescription::UI` | 1 | runtime:1 |  |
| `TextElement::UI` | 8 | engine:1 runtime:3 standalone:4 | engine/funcs/engine_funcs.cpp:1757 IAT read |
| `ToolTipDialog::UI` | 3 | runtime:1 standalone:2 |  |
| `VertexLayout::Video` | 5 | runtime:3 standalone:2 |  |
| `VertexProgram::Video` | 5 | runtime:3 standalone:2 |  |
| `VideoProgram::Video` | 5 | runtime:3 standalone:2 |  |

## Table 6 — by-name type lookups left in `code/Game` (`GetTypeIdByName` / `ResolveTypeIdByNameLazy`)

`GetTypeIdByName` walks the EXE list only (`code/Game/Object/factory_registration.cpp:52-70`); the lazy one
asks the live Factory, then the exe list (`:130-191`). Code-only call sites in `code/Game`, 23 lines
(the brief's 17 is not reproduced; the difference is the 2 inside `factory_registration.{h,cpp}` and how
the 6 dynamic-name helpers are counted):

| site | name | why left |
|---|---|---|
| `Action/action.cpp:23`, `Component/component.cpp:42`, `:113`, `Component/description.cpp:22`, `Component/resourceproducecomponent_states.cpp:389`, `Dialog/selectresourceui.cpp:311`, `Player/dragtool.cpp:43`, `Player/rotationsetting.cpp:30` (ToolSetting), `Player/tool.cpp:19`, `Tutorial/tutorialgoal.cpp:29`, `Tutorial/tutorialsetup.cpp:29`, `Work/autopickupwork.cpp:35`, `Work/work_base_standalone.cpp:48` | the 13 abstract bases' own `Type()` | `REGISTER_ABSTRACT_FACTORY` has no `DECLARE_EXE_TYPE` (B38 D4, B8-7) |
| `Game/goals_serialize.cpp:116` | "Work" → `g_tid_Work` (cell 0x1401c6cf8, 0 readers) | abstract base; the cell is dead |
| `Component/citizen_updatehealth.cpp:117` | "Disease" | `Disease` HAS `DEFINE_EXE_TYPE` (`Game/disease.cpp:105`): convertible now (carry-over 13) |
| `Dialog/toolbar_vtable_standalone.cpp:45`, `Tutorial/tutorial_base.cpp:146` (lazy) | "Toolbar" | `Toolbar` HAS `DEFINE_EXE_TYPE` (`Game/toolbar.cpp:164`): convertible now |
| `Player/tool_base_createbar.cpp:124` | "ImageElement" | a RUNTIME class on the exe-only list: always 0, the third fallback after the IAT slot 0x140151500 and `ResolveExport("?_typeInfo@ImageElement…")` — dead code |
| `Component/charactercomponent.cpp:118`, `citizencomponent.cpp:2969`, `entity_finders_disaster.cpp:78`, `populationcomponent.cpp:326`, `Dialog/edittool_select.cpp:89`, `Dialog/toolbardialog.cpp:768` | a `const char*` argument | helper wrappers keyed by name (`ResolveTid(addr,"X",&cache)` style); each call site's literal names a class with `X::TypeId()` except where noted in Table 2 |
| `Object/factory_registration.h:112` (`WriteComponentTypeId`), `factory_registration.cpp:157` | argument | the framework itself |

`ResolveTypeIdByNameLazy` has 34 further code/Game call sites (36 code-only lines in all, incl. the two Toolbar ones) (Dialog panels, `serialize_*`), almost all on
RUNTIME element names (`ProgressElement`, `CheckElement`, …) — those are Runtime-class ids and go with the
Runtime fold (Table 5), not with the exe cells.

## B. Reflection today

### B(i) exe classes — `DEFINE_EXE_TYPE` over the FactoryRegistration list
1. `DECLARE_EXE_TYPE(X)` (`code/Game/Object/exe_type.h:53-69`): slot 1 `TypeName()` →
   `_registration.typeName`; slot 2 `Type()` → `TypeId()` = first dword of `_registration.storage`;
   `CreateNewObject`/`CreateInPlaceObject`/`Vtable()` declared; `static FactoryRegistration _registration`.
2. `DEFINE_EXE_TYPE(X, "Parent")` (`:71-77`) defines `_registration` → the ctor
   (`factory_registration.cpp:16-23`) head-inserts it into the file-static `g_pending` at CRT init. No
   Runtime call happens then.
3. `InvokeAllRegistrations()` (`factory_registration.cpp:25-50`), called at `standalone/main.cpp:1467`
   (after `RuntimePortInit()` at `:1398`, which runs the mint), walks `g_pending` and runs
   `fn_BaseTypeInfo_Ctor(&r->storage, …)` — `ResolveExport("??0BaseTypeInfo@Object@@QEAA@…")`
   (`engine/funcs/engine_funcs.cpp:2074`), i.e. the ported ctor `type_reflection.cpp:243-259`, which
   head-inserts into `BaseTypeInfo::_firstTypeInfo` (`:228`, a plain static) and strips the parent after the
   last `:` (`:269-282`). `g_pending` is newest-first and the ctor head-inserts again, so the 311 exe records
   enter the chain in CRT-CONSTRUCTION order, in front of everything already there.
4. The exe-side helpers keep walking `g_pending` for their answers: `GetTypeIdByName` (`:52`),
   `GetTypeNameById` (`:84`), `GetTypeInfoByName` (`:97`), `IsTypeDerivedFromByName` (`:198`),
   `ForEachRegistration` (`:77`; used by `standalone/dlgctrl_route.cpp:324`, the only ORDER-visible consumer
   besides logs).

### B(ii) Runtime classes — the 104 minted `_typeInfo` records
1. Rows: `kMintTypeInfos[]` in `standalone/rt_data_mint_tables.inc:4251…` (generated by
   `tools/rt_mint_gen.py`), each `{symbol, dllVa, &kTypeInfoStorage[i], name, parent, ext, factory name/va,
   trap, inPlace name/va, trap}`.
2. `MintTypeInfos()` (`standalone/rt_data_mint.cpp:290-393`) fills each record: `typeId` = the CRC NOW
   (`:328`, not the sentinel — Startup would write the same value), name, stripped parent, factory pointers
   from the port registry or a trap stub (`:334-342`), then links the 104 PARENTS-FIRST by an O(n²)
   topological pass (`:357-392`) into the private `g_mintTypeInfoHead`.
3. `MintStorage()` (`:540-583`) SPLICES that chain in FRONT of `_firstTypeInfo` (`:571-578`) when it meets
   the `?_firstTypeInfo@` storage row, whose own 8 bytes are inert (the doors route to the plain static,
   `:635-636`). `RtDataMintInit()` (`:854-860`) ← `RuntimePortInit()` (`standalone/resolve_standalone.cpp:393`).
   At that moment the chain holds only CRT-init `DEFINE_TYPE` records, i.e. `Object::Internal`.
4. **Chain at `Factory::Startup` = [311 exe, CRT order] → [104 minted, parents-first] → [`DEFINE_TYPE`
   Internal].** The minted `Internal` is walked first and wins the node; the second adds nothing
   (`internal_class.cpp:332-350` says so).
5. Readers of `?_typeInfo@X` (Table 5, 339 references to 103 records): runtime 183 (the classes' own
   `Type`/`TypeName` accessors — `object_typeinfo_accessors.cpp`, `graphics_typeinfo_accessors.cpp`,
   `achievement_typeinfo_accessors.cpp`, one per `runtime/src/ui/*element.cpp`/`*description.cpp` via
   `AtomicExport`/`RtResolveCached`/`RT_DATA_SLOT`, the 30 `EXTPTR_THUNK`s in `external_ptr_serialize.cpp`,
   `TypeIdOfStatic` in `ui/dialog_create.cpp`/`combodialog_ctor.cpp`); standalone 113 (`reg_mint_promoted.cpp`
   29 `RegisterRuntimeExport` + 29 promotion storage, `reg_element_typeids.cpp` 21 IAT + 21 name,
   `reg_unreg_lane3.cpp`, `reg_unreg_lane1.cpp`, `cutover_manifest.inc` 2); engine 27 (26 IAT-slot exrefs
   `ResolveAddr(Module::Exe, 0x14015…)` → `*slot` + `g_TypeId_Dialog = ResolveAddr(Runtime, 0x1801faa88)`,
   `engine_funcs.cpp:895`); code/Game 16 (`helpui.cpp` 6, `gameupdateloop_load.cpp` 4,
   `tool_base_createbar.cpp` 2, `gameupdateloop_serialize.cpp`, `mainmenudialog.cpp`, `mod_dialog_vtables.cpp`,
   `goal_exe_helpers.cpp`). The exe IAT has 42 `?_typeInfo@` imports (`dispatch_slot_map.py` dump).

### B(iii) `Factory::Startup`, ids, the AVL tree
`Object::Factory::Startup` (`type_reflection.cpp:362-476`) = retail `?Startup@Factory@Object@@QEAA_NXZ`
0x1800c0630 (ordinal 4096). Pass 1 (`:369-392` = 0x1800c065b..0x1800c0735): head read ONCE, for each record
`typeId = TypeNameCRC(name)` (`:193-213` = `?GetTypeFromNameCRC@` 0x1800c0930's algorithm, inlined in
retail) and `_types.Insert({id, info})` (= retail 0x180030140, `Collection::Set` AVL keyed by id; equal key
keeps the existing node, `Collection.h:1292`). Pass 2 (`:395-473`): in-order walk; `parent` = find(CRC of the
stripped parent name) — a missing parent faults at 0x28 as retail does — and the node is appended to the
parent's `children` (inlined `Array::SetSize`). The name→id map is not a tree of names: `GetTypeIdFromName`
(`:492`, retail 0x1800c0580) CRCs the name and answers it iff the id is in the set.

### B(iv) the remaining registration macros
- `REGISTER_TYPE`: ONE use left, `code/Game/Object/factory_registration.cpp:245`
  (`StandaloneFactoryTest`, parent null, not a retail type). The other "uses" are the macro definitions in
  `factory_registration.h:176-188` and `factory_registrations_bulk.cpp:64-73` (`REGISTER_NULL_FACTORY{,_FULL}`
  are defined and UNUSED).
- `REGISTER_ABSTRACT_FACTORY`: 13 rows, `factory_registrations_bulk.cpp:94,120,142,156,163,185-192`
  (SelectResourceUI, Action, TutorialGoal, TutorialSetup, AutoPickupWork, Work, DragTool, Tool,
  ToolSetting, Component, Description, ControllerComponent, ResourceProduceComponent). Factory =
  `AbstractFactory(name)` logging stub, in-place = `NullInPlace`.

### B.4 Proposal — ONE `DEFINE_TYPE` for Runtime and exe classes
Use the Runtime's macro as it stands (`runtime/types/object/TypeInfo.h:638-668`) for both populations:
- **Members** per class: `static BaseTypeInfo _typeInfo` (the record; CRT-init head-insert via the ported
  ctor), `static uint32_t TypeId()`, `static const BaseTypeInfo& GetTypeInfo()`, slot 1 `TypeName()`, slot 2
  `Type()`, `static Base* CreateNewObject()`, `static Base* CreateInPlaceObject(void*)`, private
  `_TypeName()`/`_ParentTypeName()` (retail's own exported statics, e.g. `?_TypeName@Achievement@1@CAPEBDXZ`).
  `DEFINE_TYPE(Class, "ParentLiteral", ext)` — the parent stays a STRING, copied from B38 §1.3 exactly
  (quirks such as `"public UI::DialogInstance"` survive `StripNamespace`), `ext` = `"ndb"` for NameDatabase,
  null elsewhere. The exe's `Vtable()` is NOT part of reflection: keep `DEFINE_EXE_VTABLE` as its own line
  until the last `RegisterExeAddr(kVtable)` row goes, then delete it.
- **Abstract bases** (13 exe + the Runtime abstracts) get `DECLARE_TYPE` too, with retail's factory: the exe
  registers a `_purecall`-shaped factory for them — `CreateNewObject` = the abort/log stub, in-place = null
  (B8-13 notes retail's createInPlace returns null for `Internal`-rooted classes). That gives the 7 abstract
  cells and the 13 `GetTypeIdByName` `Type()` bodies their `X::TypeId()`.
- **`StandaloneFactoryTest`**: delete (not retail; it only enlarges the chain by one).
- **Order.** Static-init order across TUs is unspecified (in practice: object order on the link line, and
  within a TU declaration order). With CRC ids, a keyed set and in-order child arrays it does not matter,
  PROVIDED no two records share a name. So the fold must (a) delete the minted `Internal` row in the same
  commit that makes `Object::Internal` the only record (then the one duplicate is gone), (b) add a check that
  fails loudly on a duplicate CRC — Startup's `Insert` silently keeps the first today. A unit test over
  every `_typeInfo` (walk `_firstTypeInfo`, CRC each name, assert unique) is the cheap form.
- **Timing change** (the real risk, not order): exe records exist from CRT init instead of from
  `InvokeAllRegistrations`. Before it, `X::TypeId()` answers 0 today (zeroed storage) and would answer
  `0xFFFFFFFF` (ctor sentinel) after the fold. Any reader that runs between CRT init and `Factory::Startup`
  and tests `== 0` changes. Not measured; the per-class lazy caches were retired in step 8 and the `Fill*`
  calls run post-Startup (`game_main.cpp:511`), so the exposure is the boot probes in `standalone/main.cpp`.
- **What depends on values/order, for the gate:** ids VALUES are persisted (saves, `.rsc`) — names must not
  change; chain ORDER is seen only by `[GameMain]   typeId=` (in the typetable), `[Probe … ] #n`,
  `ForEachRegistration` order (`dlgctrl_route.cpp`), and the duplicate winner. The fold should replace the
  "first ten" dump with an order-free one (e.g. the `[OrderProbe]` lines below, sorted) and re-cut the
  typetable baseline in writing.
- The exe helpers over `g_pending` (`GetTypeIdByName`, `GetTypeInfoByName`, `GetTypeNameById`,
  `IsTypeDerivedFromByName`, `ForEachRegistration`, `InvokeAllRegistrations`) and `fn_BaseTypeInfo_Ctor` die
  with the list; the few survivors are re-expressed over `Factory` (`GetTypeIdFromName`, `IsDerivedFrom`).

## C. The order measurement (for the coordinator)

Patches (unified diffs against HEAD 3fc6d9f6, CRLF-exact, each `git apply --check` rc=0 on the clean tree;
**NOT compiled** — block 0 builds nothing). All live in `C:/Users/USPC/AppData/Local/Temp/claude/b39/0C/`.
Each contains the same pointer-free DUMP hunk in `code/Game/Game/game_main.cpp` (after the first-ten dump):
per chain record `[OrderProbe] id=… name=… parent=… winner=self|other children=n: ids…` (the tree's answer,
children in the tree's own array order) and `[OrderProbeChain] #nnn name` (walk order). Apply ONE at a time.

| file | change | what it isolates |
|---|---|---|
| `order_probe_dump.patch` | dump only | the BASELINE run |
| `order_probe.patch` (A) | `InvokeAllRegistrations` drains `g_pending` in reverse (`code/Game/Object/factory_registration.cpp`) | exe registration order (the 311) |
| `order_probe_mint.patch` (B) | `MintTypeInfos` head-inserts forward → the 104 minted walk CHILDREN-FIRST (`standalone/rt_data_mint.cpp`) | minted record order |
| `order_probe_dup.patch` (C) | `MintStorage` APPENDS the minted chain after the CRT-init records instead of splicing in front | the one duplicate (`Internal`): which record wins |

Run each with the DEBUG build (the perf build compiles `printf` away) through `b38_session/gate.sh`'s routes
(new game) + resave ×3. Compare against the dump-only baseline:
1. `grep '^\[OrderProbe\] ' standalone_debug.log | LC_ALL=C sort | md5sum` — the order-free tree.
2. `grep '^\[OrderProbeChain\]'` — must DIFFER (proves the reorder took effect).
3. the gate's `typetable.txt` (already sorted), and `Post-Startup _firstTypeInfo: 416 entries, 416 …`.
4. resave md5 vs the golden (`tools/resave_gate/resave_gate.sh`), and new-game `calls=5683`.

Expected, and what each outcome means for the fold:
- **A:** (1) identical; (3) differs ONLY in the ten `[GameMain]   typeId=` lines (the first ten chain records
  change), counts equal; (4) equal. ⇒ exe registration order is irrelevant; the fold may let static-init order
  decide, after re-cutting the typetable. If (1) differs: an order dependence this inventory missed — the fold
  must preserve the order (keep a single ordered list). If (4) differs: STOP, something persistent depends on
  order — localise with `reftable_diff.py`.
- **B:** (1), (3), (4) identical ⇒ the mint's parents-first sort (`rt_data_mint.cpp:346-392`) is unnecessary,
  which is what Startup's two-pass shape says (parents are linked after every insert). A difference would
  mean pass 2 is not what the port says.
- **C:** (1) differs in exactly the two `name=Internal` lines (`winner` swaps self/other); (3), (4) identical ⇒
  the duplicate is benign; the fold removes it anyway. A trap or a load difference ⇒ something creates
  `Internal` by id through the factory (minted row's factory is `RtMintTiTrapStub_52_0`; `DEFINE_TYPE`'s is
  retail's `return nullptr`).

## Retirement consequences

**Delete at once (no reader):** the 7 registered, reader-less cells — `DiseaseDescription` 0x1401c41a8,
`HealerComponent` 0x1401c4330, `ParticleComponent` 0x1401c4720, `WellComponent` 0x1401c4c28,
`FinishTutorialInstance` 0x1401c6278, `TimeGoal` 0x1401c62e8, `Work` 0x1401c6cf8 — with their `g_tid_`
storage and fill lines (re-grep Table 4 first: a `g_tid_` read by name counts); the 7 zero-reader
`g_TypeId_` globals (`DialogController`, `ModelComponent`, `PageToggleUI`, `PickingComponent`,
`ProfessionDescription`, `StateMachineComponent`, `StorageComponent`) with their initialisers;
`BuildRoadAddr::TypeInfo` (`code/Game/Work/BuildRoad.h:42`, no reader); the dead `ImageElement` fallback
(`tool_base_createbar.cpp:124`).

**Needs a conversion first:** the 415 reader sites on the 135 registered type-id cells (Table 2, by
directory) and the 821 reads of the 76 cell-initialised `g_TypeId_` globals → `X::TypeId()` of Table 1's
CLASS. The last reader of a cell deletes its `RegisterExeAddr` / table row, its `g_tid_` storage and its fill;
the `reg_*_typeid*.cpp` / `reg_economy_typeids.cpp` / `reg_unreg_lane4.cpp` registrars empty out and go.
Behaviour-changing (titled, resave ×5): the 26 sites on the 13 UNREGISTERED cells (0 today, a CRC after);
the 8 `+8` sites (a read of the retail caller decides what they become). The 7 abstract-base cells wait for
`DECLARE_TYPE` on the abstract bases. The Runtime side: the 42 IAT `?_typeInfo@` imports read in engine/
standalone/code/Game and the by-name `?_typeInfo@` doors (Table 5) → `NS::X::TypeId()` once each Runtime
class is `DEFINE_TYPE`d; the minted `_typeInfo` row retires when its readers are gone (D10).

**Must stay (not type ids):** 0x1401c3dd8 / 3de0 / 3de8 (singleton storage — carry-over 5, its own task),
0x1401c22d0 / 0x1401c23d0 (retail constant tables, unregistered; both readers fall back to an in-tree copy).
Of the mint's 48 storage cells only `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA`
(`rt_data_mint_tables.inc:4690`, already inert — routed to the plain static) belongs to reflection; it goes
with the mint; `?_singleton@Factory@Object@@` (`:4711`) is singleton storage.

## Not measured
- Which ten exe records currently head the chain (CRT construction order is decided by the link line; read it
  from a run's `[OrderProbeChain]` / `[GameMain]   typeId=` lines).
- Whether `ExternalFactory`'s reference table (`external_reftable.cpp:69,247,385`, "the id, once per entry")
  stores a TYPE id or a reference id — not read; either way a value, not an order.
- The retail callers of the 8 `+8` reads and of the 26 unregistered-cell reads (what the correct value is).
- The pre-Startup reader exposure of the fold's timing change (0 → 0xFFFFFFFF before `Factory::Startup`).
- Whether the probe patches compile (applied with `--check` only).
- The brief's 111 `g_TypeId_` / 17 `GetTypeIdByName` / 423 site counts were not reproduced exactly; the counts
  above carry their method.

## How measured
- `scan.py` loads every `*.cpp/*.h/*.inc/*.hpp/*.c` under `code engine runtime standalone dx11 tests/unit`,
  skipping `standalone/build*`, `tests/unit/build`, `.claude/worktrees`, and blanks comments and string
  contents (line numbers kept), so a comment or printf string is never a reader.
- `tidtable.py`: every `0[xX]1401[cCdD][0-9a-fA-F]{4}` on a code line. Writers = `RegisterExeAddr(0x1401c…, …)`
  rows + registrar table rows (`{0x1401c…, "Name"` / `{"Name", &g, 0x1401c…}`); readers = every other
  occurrence, classed as `ResolveAddr(Module::Exe, …)`, `engine_funcs` initialiser (the `g_TypeId_X = …;`
  statement containing the address), per-file wrapper, or constexpr. Retail class = SECTION R of
  `b38_session/step8_appendix_raw.txt` (`REG … ti=0x… name="…"`, 310 rows); `ti+8` flagged. Fills =
  `g = X::TypeId()` / `g = GetTypeIdByName("X")`. `g_TypeId_*`/`g_tid_*` uses = every token occurrence minus
  declarations, definitions and initialiser lines.
- `rtti.py`: `"?_typeInfo@…"` literals on code lines (excluding `rt_data_mint_tables.inc` /
  `rt_export_addrs.inc`), `0x14015…` addresses that `tools/dispatch_slot_map.py` maps to a `?_typeInfo@`
  import, and `0x1801f…` addresses equal to a minted record (or +8).
- Registrations: `^\s*DEFINE_EXE_TYPE(_EXT)?\s*\(` (297), `REGISTER_ABSTRACT_FACTORY(` rows (13), `REGISTER_TYPE(`
  code lines (1); names checked unique, parents closed, CRC-32 (UTF-16LE) of all 416 chain names (415
  distinct; the one repeat is `Internal`).
- Retail: Ghidra `disassemble_function 0x1800c0630` (program=Runtime-x64.dll, 80 of 212 instructions),
  `get_xrefs_to 0x1401c72e8` and `0x1401c5f38` (program=Application-x64.exe). No rename, no annotation, no
  save, no `switch_program`.
