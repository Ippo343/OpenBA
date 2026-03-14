# Work Module — Reimplementation Goals

**Directory:** `code/Game/Work/`
**Type headers:** `AutoPickupWork.h`, `ClearStorage.h`, `FireWork.h`, `FishingWork.h`, `ForesterWork.h`, `HarvestArea.h`, `HarvestOrchard.h`, `HerbalistWork.h`, `HuntWork.h`, `LocationWork.h`, `MarketWork.h`, `PastureWork.h`, `Pickup.h`, `TendArea.h`, `TradeWork.h`
**Source files:** None yet (empty directory)

---

## Overview

Work classes define what a citizen does at a workplace. Each Work type is created by a component (e.g., FishingComponent creates FishingWork) and pushed onto the citizen's ActionStack. Work classes typically create sequences of Actions (MoveTo → PerformWork → PickupRawMaterial → DropOff).

Work classes have a simpler vtable than components — primarily CreateWork/Update/GetWorkLocation.

---

## Classes

| Class | Vtable | Type Header | Reimpl Status | Priority |
|-------|--------|-------------|---------------|----------|
| AutoPickupWork | 0x140161a80 | AutoPickupWork.h | Not started | Low |
| BuildRoad | - | - | Not started | Medium |
| BuildStructure | - | - | Not started | Medium |
| ClearArea | - | - | Not started | Medium |
| ClearStorage | 0x1401611a0 | ClearStorage.h | Not started | Low |
| ConsumeProduce | - | - | Not started | Medium |
| FireWork | 0x1401612a0 | FireWork.h | Not started | Low |
| FishingWork | 0x140161320 | FishingWork.h | Not started | Medium |
| ForesterWork | 0x1401613a0 | ForesterWork.h | Not started | Medium |
| GatherRawMaterials | - | - | Not started | High |
| HarvestArea | 0x1401614a0 | HarvestArea.h | Not started | Medium |
| HarvestOrchard | 0x140161520 | HarvestOrchard.h | Not started | Medium |
| HerbalistWork | 0x1401615a0 | HerbalistWork.h | Not started | Low |
| HuntWork | 0x140161620 | HuntWork.h | Not started | Medium |
| LocationWork | 0x1401616a0 | LocationWork.h | Not started | Medium |
| MarketWork | 0x140161710 | MarketWork.h | Not started | Medium |
| PastureWork | 0x1401617a0 | PastureWork.h | Not started | Medium |
| Pickup | 0x140161810 | Pickup.h | Not started | Medium |
| PlantArea | - | - | Not started | Medium |
| PlantOrchard | - | - | Not started | Medium |
| TendArea | 0x140161990 | TendArea.h | Not started | Medium |
| TradeWork | 0x140161a10 | TradeWork.h | Not started | Medium |

---

## Missing Type Headers

These Work classes need type headers created from Ghidra decompilation:
- BuildRoad, BuildStructure, ClearArea, ConsumeProduce
- GatherRawMaterials, PlantArea, PlantOrchard

## Profile Source Paths

- `Work\BuildRoad.cpp`, `Work\BuildStructure.cpp`, `Work\ClearArea.cpp`, `Work\ClearStorage.cpp`
- `Work\ConsumeProduce.cpp`, `Work\FireWork.cpp`, `Work\FishingWork.cpp`
- `Work\GatherRawMaterials.cpp`, `Work\HarvestArea.cpp`, `Work\HarvestOrchard.cpp`
- `Work\HerbalistWork.cpp`, `Work\LocationWork.cpp`, `Work\Pickup.cpp`
- `Work\PlantArea.cpp`, `Work\PlantOrchard.cpp`, `Work\WorkList.cpp`

## Runtime Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| ActionStack push | Exe | Push actions for citizen to execute |
| Pathfinder | Exe (Game) | Route to work location |
| StorageComponent queries | Exe (Component) | Find/count resources |
| RawMaterialDescription | Exe (Component) | Resource type info |
