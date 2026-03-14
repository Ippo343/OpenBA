# Component Module — Reimplementation Goals

**Directory:** `code/Game/Component/`
**Type headers:** 60+ in `engine/types/` (one per component)
**Source files:** None yet (empty directory)

---

## Overview

Components are the core ECS building blocks. Each has a vtable with 12 slots:
+0x00=ScalarDtor, +0x08=GetDescription, +0x10=Startup, +0x18=Shutdown,
+0x20=Serialize, +0x28=Unknown, +0x30=Draw, +0x38=OnCreate,
+0x40=OnRemove, +0x48=Update, +0x50=Slot10, +0x58=OnUpdate2

16 components have real Update overrides (highest gameplay impact). 44 are passive.

---

## Active Components (have Update logic — highest priority)

| Class | Vtable | Size | Type Header | Reimpl Status |
|-------|--------|------|-------------|---------------|
| CitizenAIComponent | 0x140162470 | 0xD0 | CitizenAIComponent.h | Not started |
| CitizenComponent | 0x140162580 | 0x290 | CitizenComponent.h | Not started |
| StateMachineComponent | 0x140164520 | 0x58 | StateMachineComponent.h | Not started |
| CharacterComponent | 0x140162370 | 0x80 | CharacterComponent.h | Not started |
| AnimalComponent | 0x140162110 | 0x58 | AnimalComponent.h | Not started |
| HerdComponent | 0x140163680 | 0x60 | HerdComponent.h | Not started |
| WildAnimalComponent | 0x140164af0 | - | WildAnimalComponent.h | Not started |
| StorageComponent | 0x140164720 | 0x40 | StorageComponent.h | Not started |
| BuildComponent | 0x140162260 | - | BuildComponent.h | Not started |
| ConsumeProduceComponent | 0x140162a80 | - | ConsumeProduceComponent.h | Not started |
| CropFieldComponent | 0x140162ec0 | - | CropFieldComponent.h | Not started |
| ModelComponent | 0x140163ba0 | - | ModelComponent.h | Not started |
| GrowthComponent | 0x140163590 | - | GrowthComponent.h | Not started |
| StatusIconComponent | 0x140164620 | - | StatusIconComponent.h | Not started |
| MapComponent | 0x140163aa0 | - | MapComponent.h | Not started |
| InteractComponent | 0x1401638b0 | - | InteractComponent.h | Not started |

## Passive Components (no Update — lower priority)

| Class | Vtable | Type Header | Notes |
|-------|--------|-------------|-------|
| AbsorbResourceComponent | 0x140162000 | AbsorbResourceComponent.h | |
| AmbientEmitterComponent | 0x140158a60 | AmbientEmitterComponent.h | |
| AmbientEnvironmentComponent | 0x140158b70 | AmbientEnvironmentComponent.h | |
| AttendanceComponent | 0x140158ce0 | AttendanceComponent.h | |
| AutoPickupComponent | 0x140158df0 | AutoPickupComponent.h | |
| BoatComponent | 0x140158f40 | BoatComponent.h | |
| CemeteryComponent | 0x1401590a0 | CemeteryComponent.h | |
| ClearAreaComponent | 0x1401626d0 | ClearAreaComponent.h | |
| ClearResourceComponent | 0x140162840 | ClearResourceComponent.h | |
| DecalComponent | 0x140162fd0 | DecalComponent.h | |
| DestroyComponent | 0x140163120 | DestroyComponent.h | |
| DiseaseComponent | 0x140159290 | DiseaseComponent.h | |
| FireComponent | 0x140159380 | FireComponent.h | |
| FishingComponent | 0x140163230 | FishingComponent.h | |
| FloorComponent | 0x140163360 | FloorComponent.h | |
| ForesterComponent | 0x140163460 | ForesterComponent.h | |
| FruitComponent | 0x140159450 | FruitComponent.h | |
| HappinessComponent | 0x1401595d0 | HappinessComponent.h | |
| HealerComponent | 0x140159730 | HealerComponent.h | |
| HerbalistComponent | 0x140159860 | HerbalistComponent.h | |
| HighlightComponent | 0x140159990 | HighlightComponent.h | |
| HuntComponent | 0x140163770 | HuntComponent.h | |
| ImmigrationComponent | 0x140159b30 | ImmigrationComponent.h | |
| LivestockComponent | 0x1401639b0 | LivestockComponent.h | |
| MarketComponent | 0x140159c40 | MarketComponent.h | |
| MarketLocationComponent | 0x140159dd0 | MarketLocationComponent.h | |
| MineComponent | 0x140159ed0 | MineComponent.h | |
| MultiObjectComponent | 0x140163cf0 | MultiObjectComponent.h | |
| NaturalResourceComponent | 0x140163e10 | NaturalResourceComponent.h | |
| NomadComponent | 0x14015a020 | NomadComponent.h | |
| OrchardComponent | 0x14015a180 | OrchardComponent.h | |
| ParticleComponent | 0x14015a290 | ParticleComponent.h | |
| PastureComponent | 0x140163f60 | PastureComponent.h | |
| PickingComponent | 0x140164040 | PickingComponent.h | |
| PopulationComponent | 0x14015a360 | PopulationComponent.h | |
| QuarryComponent | 0x14015a490 | QuarryComponent.h | |
| RawMaterialComponent | 0x140164170 | RawMaterialComponent.h | |
| ResidenceComponent | 0x1401642c0 | ResidenceComponent.h | |
| ResourceProduceComponent | 0x1401643f0 | ResourceProduceComponent.h | |
| SpawnComponent | 0x14015a670 | SpawnComponent.h | |
| StorageDisplayComponent | 0x14015a780 | StorageDisplayComponent.h | |
| StorageLocationComponent | 0x140164880 | StorageLocationComponent.h | |
| TerraformComponent | 0x14015a880 | TerraformComponent.h | |
| TombstoneComponent | 0x14015a980 | TombstoneComponent.h | |
| TornadoComponent | 0x14015aa80 | TornadoComponent.h | |
| TrackerComponent | 0x14015ab80 | TrackerComponent.h | |
| TradeComponent | 0x14015acd0 | TradeComponent.h | |
| UIComponent | 0x1401649f0 | UIComponent.h | |
| WellComponent | 0x14015ae90 | WellComponent.h | |
| WorkComponent | 0x140164be0 | WorkComponent.h | |
| WorkPlaceComponent | 0x140164d30 | WorkPlaceComponent.h | |
| ZoneComponent | 0x140164e30 | ZoneComponent.h | |

---

## Cross-Module Dependencies from component_goals.md (old)

These are game-logic functions called from UI code but belong here:

- [ ] `fn_DestroyComponent_CanReclaim/CanRebuild/Reclaim/Rebuild/Destroy`
- [ ] `fn_WorkplaceComponent_Toggle`
- [ ] `fn_CitizenList_RequestWorker/AssignWorker/RemoveWorker`
- [ ] `fn_WorldGrid_FindEntity`
- [ ] `fn_RadiusCalc`, `fn_RadiusDisplay_Remove`
- [ ] `fn_OrchardGetGrowth`
- [ ] `fn_EditTool_SelectEntity`

---

## Runtime Dependencies (common across components)

| Function | Source | Notes |
|----------|--------|-------|
| Object::InternalPtr<T> | Runtime DLL | Smart pointer used everywhere |
| Object::ExternalPtr<T> | Runtime DLL | For descriptions |
| System::MemoryAllocator | Runtime DLL | MemAlloc/MemFree |
| Collection::Array<T> | Runtime DLL | Dynamic arrays |
| Object::Factory::Register | Runtime DLL | Component registration |
