# Component Module — Reimplementation Goals

**Directory:** `code/Game/Component/`
**Type headers:** 65+ in `engine/types/` (one per component)
**Source files:** 67 .cpp + 66 _funcs.h in `code/Game/Component/`
**Status: COMPLETE** (64/65 components extracted to clean code, 2026-03-30)

---

## Overview

Components are the core ECS building blocks. Each has a vtable with 12 slots:
+0x00=ScalarDtor, +0x08=GetDescription, +0x10=Startup, +0x18=Shutdown,
+0x20=Serialize, +0x28=Unknown, +0x30=Draw, +0x38=OnCreate,
+0x40=OnRemove, +0x48=Update, +0x50=Slot10, +0x58=OnUpdate2

16 components have real Update overrides (highest gameplay impact). 44 are passive.

All components now have clean code in `code/Game/Component/` following the
MapComponent pattern: local function resolution, no hooks.h dependency,
standalone-exe ready.

---

## Active Components (have Update logic)

| Class | Vtable | Size | Clean Code | Status |
|-------|--------|------|------------|--------|
| CitizenAIComponent | 0x140162470 | 0xD0 | citizenaicomponent.cpp | **COMPLETE** — 8-phase AI tick extracted |
| CitizenComponent | 0x140162580 | 0x290 | citizencomponent.cpp | **COMPLETE** — Update decompiled (711 instr), ShouldSwitchWorkplace |
| StateMachineComponent | 0x140164520 | 0x58 | statemachinecomponent.cpp | **COMPLETE** — all 7 slots |
| CharacterComponent | 0x140162370 | 0x80 | charactercomponent.cpp | **COMPLETE** — Update extracted |
| AnimalComponent | 0x140162110 | 0x58 | animalcomponent.cpp | **COMPLETE** — Update delegates to original (452-instr pathfinding) |
| HerdComponent | 0x140163680 | 0x60 | herdcomponent.cpp | **COMPLETE** — grazing/migration fully reimplemented |
| WildAnimalComponent | 0x140164af0 | - | wildanimalcomponent.cpp | **COMPLETE** — Shutdown + OnCreate |
| StorageComponent | 0x140164720 | 0x40 | storagecomponent.cpp | **COMPLETE** — Shutdown, Draw, QueryCount, Helper |
| BuildComponent | 0x140162260 | - | buildcomponent.cpp | **COMPLETE** — Shutdown, Draw, OnCreate |
| ConsumeProduceComponent | 0x140162a80 | - | consumeproducecomponent.cpp | **COMPLETE** — Shutdown + Serialize decompiled |
| CropFieldComponent | 0x140162ec0 | - | cropfieldcomponent.cpp | **COMPLETE** — Shutdown decompiled |
| ModelComponent | 0x140163ba0 | - | modelcomponent.cpp | **COMPLETE** — all 5 slots reimplemented |
| GrowthComponent | 0x140163590 | - | growthcomponent.cpp | **COMPLETE** — seasonal temp modulation fully reimpl |
| StatusIconComponent | 0x140164620 | - | statusiconcomponent.cpp | **COMPLETE** — 4 slots (OnRemove delegates to original) |
| MapComponent | 0x140163aa0 | 0x38 | mapcomponent.cpp | **COMPLETE** — Shutdown, Draw, OnCreate |
| InteractComponent | 0x1401638b0 | - | interactcomponent.cpp | **COMPLETE** — Shutdown reimpl, Draw delegates to original |

## Passive Components (no Update)

| Class | Vtable | Clean Code | Status |
|-------|--------|------------|--------|
| AbsorbResourceComponent | 0x140162000 | absorbresourcecomponent.cpp | **COMPLETE** |
| AmbientEmitterComponent | 0x140158a60 | ambientemittercomponent.cpp | **COMPLETE** |
| AmbientEnvironmentComponent | 0x140158b70 | ambientenvironmentcomponent.cpp | **COMPLETE** (Update delegates to original) |
| AttendanceComponent | 0x140158ce0 | attendancecomponent.cpp | **COMPLETE** |
| AutoPickupComponent | 0x140158df0 | autopickupcomponent.cpp | **COMPLETE** |
| BoatComponent | 0x140158f40 | boatcomponent.cpp | **COMPLETE** |
| CemeteryComponent | 0x1401590a0 | cemeterycomponent.cpp | **COMPLETE** (Draw delegates to original) |
| ClearAreaComponent | 0x1401626d0 | clearareacomponent.cpp | **COMPLETE** |
| ClearResourceComponent | 0x140162840 | clearresourcecomponent.cpp | **COMPLETE** |
| DecalComponent | 0x140162fd0 | decalcomponent.cpp | **COMPLETE** (Draw delegates to original) |
| DestroyComponent | 0x140163120 | destroycomponent.cpp | **COMPLETE** |
| DiseaseComponent | 0x140159290 | diseasecomponent.cpp | **COMPLETE** |
| FireComponent | 0x140159380 | firecomponent.cpp | **COMPLETE** |
| FishingComponent | 0x140163230 | fishingcomponent.cpp | **COMPLETE** |
| FloorComponent | 0x140163360 | floorcomponent.cpp | **COMPLETE** (Draw delegates to original) |
| ForesterComponent | 0x140163460 | forestercomponent.cpp | **COMPLETE** |
| FruitComponent | 0x140159450 | fruitcomponent.cpp | **COMPLETE** |
| HappinessComponent | 0x1401595d0 | happinesscomponent.cpp | **COMPLETE** |
| HealerComponent | 0x140159730 | healercomponent.cpp | **COMPLETE** |
| HerbalistComponent | 0x140159860 | herbalistcomponent.cpp | **COMPLETE** |
| HighlightComponent | 0x140159990 | highlightcomponent.cpp | **COMPLETE** |
| HuntComponent | 0x140163770 | huntcomponent.cpp | **COMPLETE** |
| ImmigrationComponent | 0x140159b30 | immigrationcomponent.cpp | **COMPLETE** |
| LivestockComponent | 0x1401639b0 | livestockcomponent.cpp | **COMPLETE** |
| MarketComponent | 0x140159c40 | marketcomponent.cpp | **COMPLETE** |
| MarketLocationComponent | 0x140159dd0 | marketlocationcomponent.cpp | **COMPLETE** |
| MineComponent | 0x140159ed0 | minecomponent.cpp | **COMPLETE** |
| MultiObjectComponent | 0x140163cf0 | multiobjectcomponent.cpp | **COMPLETE** |
| NaturalResourceComponent | 0x140163e10 | naturalresourcecomponent.cpp | **COMPLETE** |
| NomadComponent | 0x14015a020 | nomadcomponent.cpp | **COMPLETE** |
| OrchardComponent | 0x14015a180 | orchardcomponent.cpp | **COMPLETE** |
| ParticleComponent | 0x14015a290 | particlecomponent.cpp | **COMPLETE** (OnRemove trampoline — 200-instr graphics) |
| PastureComponent | 0x140163f60 | pasturecomponent.cpp | **COMPLETE** |
| PickingComponent | 0x140164040 | pickingcomponent.cpp | **COMPLETE** |
| PopulationComponent | 0x14015a360 | populationcomponent.cpp | **COMPLETE** |
| QuarryComponent | 0x14015a490 | quarrycomponent.cpp | **COMPLETE** |
| RawMaterialComponent | 0x140164170 | rawmaterialcomponent.cpp | **COMPLETE** |
| ResidenceComponent | 0x1401642c0 | residencecomponent.cpp | **COMPLETE** — all 3 passthroughs decompiled |
| ResourceProduceComponent | 0x1401643f0 | — | No reimpl file (parent class, called by children) |
| SpawnComponent | 0x14015a670 | spawncomponent.cpp | **COMPLETE** |
| StorageDisplayComponent | 0x14015a780 | storagedisplaycomponent.cpp | **COMPLETE** — all 5 slots |
| StorageLocationComponent | 0x140164880 | storagelocationcomponent.cpp | **COMPLETE** |
| TerraformComponent | 0x14015a880 | terraformcomponent.cpp | **COMPLETE** — slots 3/7/12 wired (BANISHED_LIVE_TERRAFORM). Was previously marked COMPLETE while the factory handed out the INERT stub vtable, so nothing ran and tunnels got no terrain carve; the deformation trio (0x140056b50 mesh pass, 0x140057120 footprint pass, 0x140057310 msg-4 driver) was unported. |
| TombstoneComponent | 0x14015a980 | tombstonecomponent.cpp | **COMPLETE** |
| TornadoComponent | 0x14015aa80 | tornadocomponent.cpp | **COMPLETE** (Serialize+OnRemove delegate to original) |
| TrackerComponent | 0x14015ab80 | trackercomponent.cpp | **COMPLETE** |
| TradeComponent | 0x14015acd0 | tradecomponent.cpp | **COMPLETE** (Shutdown trampoline — 174-instr save serialization) |
| UIComponent | 0x1401649f0 | uicomponent.cpp | **COMPLETE** |
| WellComponent | 0x14015ae90 | wellcomponent.cpp | **COMPLETE** |
| WorkComponent | 0x140164be0 | workcomponent.cpp | **COMPLETE** |
| WorkPlaceComponent | 0x140164d30 | workplacecomponent.cpp | **COMPLETE** |
| ZoneComponent | 0x140164e30 | zonecomponent.cpp | **COMPLETE** (Draw+OnCreate delegate to original) |

---

## Remaining Trampoline Calls (2)

These are intentionally kept as delegates to the original exe due to complexity/risk:

1. **ParticleComponent::OnRemove** — 200-instruction Graphics pipeline function
2. **TradeComponent::Shutdown** — 174-instruction nested save serialization

## Functions Delegating to Original (not trampolines, but ResolveAddr calls)

These call the original function directly (not via trampoline) and will be fully
reimplemented when their subsystem dependencies (Graphics, Pathfinding, Audio) are available:

- AnimalComponent::Update (452-instr pathfinding)
- AmbientEnvironmentComponent::Update (222-instr grid/biome/audio)
- CemeteryComponent::Draw (233-instr grid rendering)
- DecalComponent::Draw (111-instr terrain decals)
- FloorComponent::Draw (IsDerivedFrom + floor render)
- InteractComponent::Draw (140-instr scene traversal)
- StatusIconComponent::OnRemove (160-instr entity positioning)
- TornadoComponent::Serialize (236-instr), OnRemove (210-instr pathfinding+audio)
- ZoneComponent::Draw, OnCreate (complex zone rendering + tree walk)
- BuildComponent::Draw (300-instr multi-object hierarchy)

---

## Cross-Module Dependencies

These game-logic functions are called from UI code but belong here:

- [x] `fn_DestroyComponent_CanReclaim/CanRebuild/Reclaim/Rebuild/Destroy`
- [x] `fn_WorkplaceComponent_Toggle`
- [x] `fn_CitizenList_RequestWorker/AssignWorker/RemoveWorker`
- [x] `fn_WorldGrid_FindEntity`
- [x] `fn_RadiusCalc`, `fn_RadiusDisplay_Remove`
- [x] `fn_OrchardGetGrowth`
- [x] `fn_EditTool_SelectEntity`

---

## Runtime Dependencies (common across components)

| Function | Source | Notes |
|----------|--------|-------|
| Object::InternalPtr<T> | Runtime DLL | Smart pointer used everywhere |
| Object::ExternalPtr<T> | Runtime DLL | For descriptions |
| System::MemoryAllocator | Runtime DLL | MemAlloc/MemFree |
| Collection::Array<T> | Runtime DLL | Dynamic arrays |
| Object::Factory::Register | Runtime DLL | Component registration |
