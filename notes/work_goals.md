# Work Module — Reimplementation Goals

**Directory:** `code/Game/Work/`
**Type headers:** `AutoPickupWork.h`, `ClearStorage.h`, `FireWork.h`, `FishingWork.h`, `ForesterWork.h`, `HarvestArea.h`, `HarvestOrchard.h`, `HerbalistWork.h`, `HuntWork.h`, `LocationWork.h`, `MarketWork.h`, `PastureWork.h`, `Pickup.h`, `TendArea.h`, `TradeWork.h`
**Source files:** `autopickupwork.cpp`, `buildroad.cpp`, `buildstructure.cpp`, `cleararea.cpp`, `clearstorage.cpp`, `consumeproduce.cpp`, `firework.cpp`, `fishingwork.cpp`, `foresterwork.cpp`, `gatherrawmaterials.cpp`, `harvestarea.cpp`, `harvestorchard.cpp`, `herbalistwork.cpp`, `huntwork.cpp`, `locationwork.cpp`, `marketwork.cpp`, `pasturework.cpp`

---

## Overview

Work classes define what a citizen does at a workplace. Each Work type is created by a component (e.g., FishingComponent creates FishingWork) and pushed onto the citizen's ActionStack. Work classes typically create sequences of Actions (MoveTo → PerformWork → PickupRawMaterial → DropOff).

Work classes have a simpler vtable than components — primarily CreateWork/Update/GetWorkLocation.

---

## Classes

| Class | Vtable | Type Header | Reimpl Status | Priority |
|-------|--------|-------------|---------------|----------|
| AutoPickupWork | 0x140160fa0 | AutoPickupWork.h | Deep reimpl (5/5 non-abstract) | Low |
| BuildRoad | 0x140161020 | BuildActions.h | **COMPLETE** (all hookable slots reimplemented) | Medium |
| BuildStructure | 0x1401610a0 | BuildActions.h | **COMPLETE** (all hookable slots reimplemented) | Medium |
| ClearArea | 0x140161110 | BuildActions.h | **COMPLETE** (all hookable slots reimplemented) | Medium |
| ClearStorage | 0x1401611a0 | ClearStorage.h | **COMPLETE** (Slot9 disabled=bug class 3, clean C++ ready) | Low |
Stage 2:
| ConsumeProduce | 0x140161220 | BuildActions.h | **COMPLETE** (all methods reimpl'd, Execute not hooked — standalone exe only) | Medium |
| FireWork | 0x1401612a0 | FireWork.h | **COMPLETE** (all methods reimpl'd, Slot6/Slot9 disabled=bug class 2/3) | Low |
| FishingWork | 0x140161320 | FishingWork.h | **COMPLETE** (all 7 hooked methods reimpl'd) | Medium |
| ForesterWork | 0x1401613a0 | ForesterWork.h | **COMPLETE** (all methods reimpl'd, Slot9 disabled=bug class 3) | Medium |
Stage 3:
| GatherRawMaterials | 0x140161420 | BuildActions.h | **COMPLETE** (7 methods reimpl'd, Validate/GetTarget not hooked=bug class 2/3) | High |
| HarvestArea | 0x1401614a0 | HarvestArea.h | **COMPLETE** (8 methods reimpl'd, Slot9 disabled=bug class 2) | Medium |
| HarvestOrchard | 0x140161520 | HarvestOrchard.h | **COMPLETE** (8 methods reimpl'd, Slot9 disabled=bug class 2) | Medium |
| HerbalistWork | 0x1401615a0 | HerbalistWork.h | **COMPLETE** (8 methods reimpl'd, Slot6 unhookable=bug class 1, Slot9 disabled=bug class 2) | Low |
Stage 4:
| HuntWork | 0x140161620 | HuntWork.h | **COMPLETE** (all methods reimpl'd, Slot9 disabled=bug class 3, Slot6 unhookable) | Medium |
| LocationWork | 0x1401616a0 | LocationWork.h | **COMPLETE** (all 7 hooked methods reimpl'd) | Medium |
| MarketWork | 0x140161710 | MarketWork.h | **COMPLETE** (all methods reimpl'd, Slot4/6/8/9 disabled=bug classes 2/3, Slot4 calls original) | Medium |
| PastureWork | 0x1401617a0 | PastureWork.h | **COMPLETE** (all methods reimpl'd, Slot9 disabled=bug class 3, Slot6 unhookable) | Medium |
Stage 5 (new classes):
| Pickup | 0x140161810 | Pickup.h | Not started | Medium |
| PlantArea | - | - | Not started | Medium |
| PlantOrchard | - | - | Not started | Medium |
| TendArea | 0x140161990 | TendArea.h | Not started | Medium |
| TradeWork | 0x140161a10 | TradeWork.h | Not started | Medium |

Stage 6 (deferred slots from Stages 1-3 — standalone exe completeness):

### Passthrough hooks (have hook, calling trampoline — need clean C++)

| Class | Slot | Address | Name | Notes |
|-------|------|---------|------|-------|
| ClearStorage | 6 | 0x1400e6c30 | Slot6 | |
| ClearStorage | 8 | 0x1400e72a0 | Slot8/CancelAll | |
| ClearStorage | 9 | 0x1400e69f0 | Slot9/Validate | Bug class 3, complex |
| ClearStorage | 10 | 0x1400e7330 | Slot10/GetTarget | |
| FireWork | 6 | 0x1400e9600 | Slot6 | 20-byte thunk |
| FireWork | 8 | 0x1400e99a0 | Slot8/CancelAll | |
| FishingWork | 3 | 0x1400ea910 | Serialize | Complex array serialization |
| FishingWork | 6 | 0x1400e9eb0 | Slot6 | |
| FishingWork | 7 | 0x1400e9e80 | Slot7 | |
| FishingWork | 8 | 0x1400ea830 | Slot8/CancelAll | |
| FishingWork | 10 | 0x1400ea8b0 | Slot10/GetTarget | |
| ForesterWork | 0 | 0x14002A370 | ScalarDtor | |
| ForesterWork | 3 | 0x1400EB970 | Serialize | Tree serialization |
| ForesterWork | 8 | 0x1400EB8A0 | Slot8/CancelAll | |
| ForesterWork | 9 | 0x1400EAC60 | Slot9/RemoveInvalid | 164 instructions |
| ForesterWork | 10 | 0x1400EB910 | Slot10/GetTarget | |
| GatherRawMaterials | 0 | 0x14002a410 | ScalarDtor | |
| BuildStructure | 0 | 0x140029680 | ScalarDtor | |

### Unhookable / disabled hooks (need clean C++ for standalone exe only)

| Class | Slot | Address | Name | Bug Class | Notes |
|-------|------|---------|------|-----------|-------|
| BuildRoad | 10 | 0x1400e4960 | GetTarget | 2 (RIP-relative) | |
| ClearArea | 9 | 0x1400e5b10 | Validate | 3 (RSP capture) | |
| ForesterWork | 6 | 0x1400EAEA0 | Slot6 | 1 (RIP-relative CALL) | |
| GatherRawMaterials | 9 | 0x1400ebb50 | Validate | 3 (RSP capture) | |
| GatherRawMaterials | 10 | 0x1400ec510 | GetTarget | 2 (RIP-relative) | |

### Too small to hook (need clean C++ for standalone exe only)

| Class | Slot | Address | Name | Size | Notes |
|-------|------|---------|------|------|-------|
| AutoPickupWork | 0 | 0x1400294E0 | ScalarDtor | ~19 bytes | |
| AutoPickupWork | 3 | 0x1400E3F80 | Serialize | 9 bytes | Bug class 1 |
| AutoPickupWork | 4 | 0x1400F7540 | Execute | 3 bytes | `xor al,al; ret` — shared by ALL AutoPickupWork-derived classes |
| ClearArea | 7 | 0x1400e5e00 | OnCancel | too small | |
| ClearStorage | 0 | 0x14002a100 | ScalarDtor | tiny | |
| ForesterWork | 7 | 0x1400EAE90 | Slot7 | 9 bytes | Thunk: `add rcx,0x48; jmp` |
| FireWork | 7 | 0x1400e95f0 | Slot7 | unknown | Not found in Ghidra |
| FireWork | 10 | 0x1400e9a30 | Slot10 | unknown | Not found in Ghidra |
| BuildRoad | 11 | 0x140029860 | GetPriority | 9 bytes | Shared by many classes |
| ClearArea | 11 | 0x140029860 | GetPriority | 9 bytes | Same address as above |

### Trivial shared slots (identical across most/all Work classes)

These addresses are reused across many classes. Implement once for standalone exe:

| Slot | Address | Name | Size | Shared By |
|------|---------|------|------|-----------|
| 5 | 0x1400F7540 | Execute (trivial) | 3 bytes | All AutoPickupWork derivatives (xor al,al; ret) |
| 11 | 0x140029860 | GetPriority | 9 bytes | BuildRoad, ClearArea, FishingWork, ForesterWork, HarvestArea, HarvestOrchard, HerbalistWork |
| 1 | varies | VectorDtor | 7-8 bytes | All 13 classes (trivial: calls BaseDtor + conditional free) |
| 2 | varies | BaseDtor | 7 bytes | All 13 classes (trivial: sets vtable, calls ~Internal dtor) |

---

## Missing Type Headers

These Work classes need type headers created from Ghidra decompilation:
- PlantArea, PlantOrchard

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
