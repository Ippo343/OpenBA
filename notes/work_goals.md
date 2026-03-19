# Work Module — Reimplementation Goals

**Directory:** `code/Game/Work/`
**Type headers:** `AutoPickupWork.h`, `ClearStorage.h`, `FireWork.h`, `FishingWork.h`, `ForesterWork.h`, `HarvestArea.h`, `HarvestOrchard.h`, `HerbalistWork.h`, `HuntWork.h`, `LocationWork.h`, `MarketWork.h`, `PastureWork.h`, `Pickup.h`, `TendArea.h`, `TradeWork.h`
**Source files:** `autopickupwork.cpp`, `buildroad.cpp`, `buildstructure.cpp`, `cleararea.cpp`, `clearstorage.cpp`, `consumeproduce.cpp`, `firework.cpp`, `fishingwork.cpp`, `foresterwork.cpp`, `gatherrawmaterials.cpp`, `harvestarea.cpp`, `harvestorchard.cpp`, `herbalistwork.cpp`, `huntwork.cpp`, `locationwork.cpp`, `marketwork.cpp`, `pasturework.cpp`, `pickup.cpp`, `plantarea.cpp`, `plantorchard.cpp`, `tendarea.cpp`, `tradework.cpp`

---

## Overview

Work classes define what a citizen does at a workplace. Each Work type is created by a component (e.g., FishingComponent creates FishingWork) and pushed onto the citizen's ActionStack. Work classes typically create sequences of Actions (MoveTo → PerformWork → PickupRawMaterial → DropOff).

Work classes have a simpler vtable than components — primarily CreateWork/Update/GetWorkLocation.

---

## Classes

| Class | Vtable | Type Header | Reimpl Status | Priority |
|-------|--------|-------------|---------------|----------|
| AutoPickupWork | 0x140160fa0 | AutoPickupWork.h | **100%** — all vtable slots reimpl'd | Low |
| BuildRoad | 0x140161020 | BuildActions.h | **100%** — all vtable slots reimpl'd (GetTarget unhookable=bug class 2) | Medium |
| BuildStructure | 0x1401610a0 | BuildActions.h | **100%** — all vtable slots reimpl'd | Medium |
| ClearArea | 0x140161110 | BuildActions.h | **100%** — all vtable slots reimpl'd (Validate unhookable=bug class 3) | Medium |
| ClearStorage | 0x1401611a0 | ClearStorage.h | **100%** — all vtable slots reimpl'd (Slot9 unhookable=bug class 3) | Low |
| ConsumeProduce | 0x140161220 | BuildActions.h | **100%** — all vtable slots reimpl'd | Medium |
| FireWork | 0x1401612a0 | FireWork.h | **100%** — all vtable slots reimpl'd (Slot9 unhookable=bug class 3) | Low |
| FishingWork | 0x140161320 | FishingWork.h | **100%** — all vtable slots reimpl'd | Medium |
| ForesterWork | 0x1401613a0 | ForesterWork.h | **100%** — all vtable slots reimpl'd (Slot6 unhookable=bug class 1) | Medium |
| GatherRawMaterials | 0x140161420 | BuildActions.h | **100%** — all vtable slots reimpl'd (Validate/GetTarget unhookable=bug class 2/3) | High |
| HarvestArea | 0x1401614a0 | HarvestArea.h | **100%** — all vtable slots reimpl'd (Slot9 unhookable=bug class 2) | Medium |
| HarvestOrchard | 0x140161520 | HarvestOrchard.h | **100%** — all vtable slots reimpl'd (Slot9 unhookable=bug class 2) | Medium |
| HerbalistWork | 0x1401615a0 | HerbalistWork.h | **100%** — all vtable slots reimpl'd (Slot6 unhookable=bug class 1, Slot9 unhookable=bug class 2) | Low |
| HuntWork | 0x140161620 | HuntWork.h | **100%** — all vtable slots reimpl'd (Slot9 unhookable=bug class 3, Slot6 unhookable) | Medium |
| LocationWork | 0x1401616a0 | LocationWork.h | **100%** — all vtable slots reimpl'd | Medium |
| MarketWork | 0x140161710 | MarketWork.h | **100%** — all vtable slots reimpl'd (Slot4/6/8/9 unhookable=bug classes 2/3) | Medium |
| PastureWork | 0x1401617a0 | PastureWork.h | **100%** — all vtable slots reimpl'd (Slot9 unhookable=bug class 3, Slot6 unhookable) | Medium |
| Pickup | 0x140161810 | Pickup.h | **100%** — all vtable slots reimpl'd (Slot4/8/10 unhookable=bug classes 2/3/Jcc) | Medium |
| PlantArea | 0x140161890 | BuildActions.h | **100%** — all vtable slots reimpl'd | Medium |
| PlantOrchard | 0x140161910 | BuildActions.h | **100%** — all vtable slots reimpl'd | Medium |
| TendArea | 0x140161990 | TendArea.h | **100%** — all vtable slots reimpl'd (Slot6/8/9 unhookable=bug classes 2/3/Jcc) | Medium |
| TradeWork | 0x140161a10 | TradeWork.h | **100%** — all vtable slots reimpl'd (Slot4/6/8/9 unhookable=bug classes 2/3/Jcc) | Medium |

**Work module: 22/22 classes at 100% vtable coverage.** All deferred slots from Stages 1-5 completed in Stage 6.

### Stage 6 completion log (deferred slots from Stages 1-5)

All slots below now have clean C++ implementations for standalone exe completeness.
Passthrough hooks switched to call clean code; unhookable/too-small slots are standalone exe only.

| Class | Slot | Address | Name | Status |
|-------|------|---------|------|--------|
| ClearStorage | 0 | 0x14002a100 | ScalarDtor | Done (standalone) |
| ClearStorage | 6 | 0x1400e6c30 | OnComplete | Done (hook→clean) |
| ClearStorage | 8 | 0x1400e72a0 | CancelAll | Done (hook→clean) |
| ClearStorage | 9 | 0x1400e69f0 | Validate | Done (standalone, bug class 3) |
| ClearStorage | 10 | 0x1400e7330 | GetTarget | Done (hook→clean) |
| FireWork | 6 | 0x1400e9600 | OnComplete | Done (hook→clean) |
| FireWork | 7 | 0x1400e95f0 | OnCancel | Done (standalone) |
| FireWork | 8 | 0x1400e99a0 | CancelAll | Done (hook→clean) |
| FireWork | 10 | 0x1400e9a30 | GetTarget | Done (standalone) |
| FishingWork | 3 | 0x1400ea910 | Serialize | Done (hook→clean) |
| FishingWork | 6 | 0x1400e9eb0 | OnComplete | Done (hook→clean) |
| FishingWork | 7 | 0x1400e9e80 | OnCancel | Done (hook→clean) |
| FishingWork | 8 | 0x1400ea830 | CancelAll | Done (hook→clean) |
| FishingWork | 10 | 0x1400ea8b0 | GetTarget | Done (hook→clean) |
| ForesterWork | 0 | 0x14002A370 | ScalarDtor | Done (hook→clean) |
| ForesterWork | 3 | 0x1400EB970 | Serialize | Done (hook→clean) |
| ForesterWork | 6 | 0x1400EAEA0 | Slot6 | Done (standalone, bug class 1) |
| ForesterWork | 7 | 0x1400EAE90 | Slot7 | Done (standalone) |
| ForesterWork | 8 | 0x1400EB8A0 | CancelAll | Done (hook→clean) |
| ForesterWork | 9 | 0x1400EAC60 | RemoveInvalid | Done (hook→clean, 164 instructions) |
| ForesterWork | 10 | 0x1400EB910 | GetTarget | Done (hook→clean) |
| GatherRawMaterials | 0 | 0x14002a410 | ScalarDtor | Done (hook→clean) |
| BuildStructure | 0 | 0x140029680 | ScalarDtor | Done (hook→clean) |
| BuildRoad | 10 | 0x1400e4960 | GetTarget | Done (standalone, bug class 2) |
| ClearArea | 9 | 0x1400e5b10 | Validate | Done (standalone, bug class 3) |
| ClearArea | 7 | 0x1400e5e00 | OnCancel | Done (standalone) |
| GatherRawMaterials | 9 | 0x1400ebb50 | Validate | Done (standalone, bug class 3) |
| GatherRawMaterials | 10 | 0x1400ec510 | GetTarget | Done (standalone, bug class 2) |
| GetPriority | 0x140029860 | shared | Done — BuildRoad, ClearArea, FishingWork, ForesterWork, HarvestArea, HarvestOrchard, HerbalistWork |
| GetPriority | 0x14002a0f0 | ClearStorage/ConsumeProduce | Done |
| GetPriority | 0x14002a220 | FireWork | Done |
| GetPriority | 0x14002a400 | GatherRawMaterials | Done |

---

## Missing Type Headers

None — all Work classes now have type headers.

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
