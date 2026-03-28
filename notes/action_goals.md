# Action, Work, Goal & Tutorial Reimplementation Status

**Last updated:** 2026-03-28

## Summary

| Category | Total | Done | Partial | Remaining |
|----------|-------|------|---------|-----------|
| Actions | 17 | 17 | 0 | — |
| Work | 22 | 22 | 0 | — |
| Goals | 7 | 7 | 0 | — |
| Tutorial | 19 | 19 | 0 | — |
| **TOTAL** | **65** | **65** | **0** | **100% complete** |

---

## Actions (17 classes)

Clean code: `code/Game/Action/` | Hooks: `engine/reimpl/`

| # | Class | Methods Hooked | Status | Notes |
|---|-------|---------------|--------|-------|
| 1 | AnimateNaturalResource | Serialize, AbstractImpl, Execute, GetTarget | DONE | |
| 2 | CompleteRoad | Serialize, AbstractImpl | DONE | |
| 3 | CompleteTask | Serialize, AbstractImpl, CheckEntity, GetTarget | DONE | |
| 4 | ConsumeRawMaterial | Serialize, AbstractImpl, CheckEntity | DONE | |
| 5 | CreateNaturalResource | Serialize, AbstractImpl, GetTarget | DONE | |
| 6 | CreateRawMaterial | Serialize, AbstractImpl | DONE | |
| 7 | DropOffRawMaterial | Serialize, AbstractImpl | DONE | |
| 8 | HuntAnimal | Serialize, Enter (+ GetTarget in clean code only) | DONE | |
| 9 | MoveTo | Serialize, AbstractImpl, Enter, Exit, CheckEntity, Execute | DONE | Execute via vtable hook (bug class 3 prologue) |
| 10 | PerformWork | Serialize, AbstractImpl, Execute, Exit, CheckEntity | DONE | |
| 11 | PickupRawMaterial | Serialize, AbstractImpl, GetTarget | DONE | |
| 12 | PickupStoredRawMaterial | Serialize, AbstractImpl, CheckEntity | DONE | |
| 13 | PlayAnimation | Serialize, AbstractImpl, Execute, GetTarget | DONE | |
| 14 | RemoveAnimal | Serialize, AbstractImpl, CheckEntity, GetTarget | DONE | 2 hooks disabled (RIP-relative), clean code exists for all |
| 15 | RemoveNaturalResource | Serialize, AbstractImpl, CheckEntity, GetTarget | DONE | |
| 16 | SendComponentMessage | Serialize, AbstractImpl, GetTarget | DONE | |
| 17 | ShowTool | Serialize, AbstractImpl, CheckEntity, GetTarget | DONE | |

---

## Work Classes (22 classes) — ALL DONE

Clean code: `code/Game/Work/` | Hooks: `engine/reimpl/`

| # | Class | Key Methods | Status |
|---|-------|------------|--------|
| 1 | AutoPickupWork | Slot8/Cleanup, Ctor | DONE |
| 2 | BuildRoad | Ctor, ScalarDtor, Serialize, Execute, OnComplete, OnCancel, Cleanup, Validate | DONE |
| 3 | BuildStructure | Ctor, ScalarDtor, Serialize, Execute, OnComplete, OnCancel, Cleanup | DONE |
| 4 | ClearArea | Ctor, ScalarDtor, Serialize, Execute, OnComplete, Cleanup, GetTarget | DONE |
| 5 | ClearStorage | Serialize, Slot4-Slot10 | DONE |
| 6 | ConsumeProduce | Ctor, ScalarDtor, Serialize | DONE |
| 7 | FireWork | Serialize, Slot4, Slot6, Slot8, Slot9 | DONE |
| 8 | FishingWork | Serialize, Slot4-Slot10 | DONE |
| 9 | ForesterWork | ScalarDtor, Serialize, Slot4, Slot8-Slot10 | DONE |
| 10 | GatherRawMaterials | Ctor, ScalarDtor, Serialize, Execute, OnComplete, OnCancel, Cleanup | DONE |
| 11 | HarvestArea | ScalarDtor, Serialize, Slot4-Slot10 | DONE |
| 12 | HarvestOrchard | ScalarDtor, Serialize, Slot4-Slot10 | DONE |
| 13 | HerbalistWork | ScalarDtor, Serialize, Slot4, Slot8-Slot10 | DONE |
| 14 | HuntWork | ScalarDtor, Serialize, Slot4, Slot8-Slot10 | DONE |
| 15 | LocationWork | Serialize, Slot4, Slot6-Slot10 | DONE |
| 16 | MarketWork | Serialize, Slot4, Slot6, Slot8-Slot10 | DONE |
| 17 | PastureWork | ScalarDtor, Serialize, Slot4, Slot7-Slot10 | DONE |
| 18 | Pickup | Serialize, Slot4, Execute, Slot6-Slot10 | DONE |
| 19 | PlantArea | Ctor, ScalarDtor, Serialize, Slot4, OnComplete, OnCancel, Cleanup, Validate, GetTarget | DONE |
| 20 | PlantOrchard | Ctor, ScalarDtor, Serialize, Slot4, OnComplete, OnCancel, Cleanup, Validate, GetTarget | DONE |
| 21 | TendArea | ScalarDtor, Serialize, Slot4, Slot6, Slot8-Slot10 | DONE |
| 22 | TradeWork | ScalarDtor, Serialize, Slot4, Slot6, Slot8-Slot10 | DONE |

---

## Goal Classes (7 classes) — ALL DONE

Clean code: `code/Game/Goal/` | Hooks: `engine/reimpl/`

| # | Class | Methods | Status |
|---|-------|---------|--------|
| 1 | BuildingGoal | Dtor, Slot5, Slot6, Slot8, CreateInstance | DONE |
| 2 | PopulationGoal | Dtor, Slot5, Slot6, Slot8, CreateInstance | DONE |
| 3 | StatGoal | Dtor, Slot5, Slot6, Slot8, Slot10 | DONE |
| 4 | TimeGoal | Dtor, Slot5, Slot6, Slot10, Slot11 | DONE |
| 5 | TimeGoalInstance | Dtor, Slot3, Slot5, Slot6 | DONE |
| 6 | FinishTutorialGoal | Dtor, Slot5, Slot6, CreateInstance, Slot11 | DONE |
| 7 | FinishTutorialInstance | Dtor, Slot3, Slot4, Slot5, Slot6 | DONE |

---

## Tutorial Classes (19 classes)

Clean code: `code/Game/Tutorial/` | Hooks: `engine/reimpl/`

| # | Class | Methods | Status | Notes |
|---|-------|---------|--------|-------|
| 1 | Tutorial | Dtor, Serialize, Cleanup, Ctor | DONE | |
| 2 | TutorialGoal | Dtor, Serialize | DONE | |
| 3 | TutorialInstance | Dtor, Serialize | DONE | |
| 4 | TutorialStep | Dtor, Serialize, Cleanup, Init | DONE | Init uses vtable patch |
| 5 | TutorialGoalButton | Dtor, Serialize, Cleanup, Check, Ctor | DONE | |
| 6 | TutorialGoalCount | Dtor, Serialize, Cleanup, Check, Ctor | DONE | Check/Init use vtable patch |
| 7 | TutorialGoalGameSpeed | Dtor, Serialize, Cleanup, Check, Ctor | DONE | Check uses vtable patch |
| 8 | TutorialGoalDialog | ScalarDtor, Shutdown, OnEvent, OnInit, OnDestroy | DONE | All vtable-patched |
| 9 | TutorialUI | ScalarDtor, Serialize, Startup, OnRemove | DONE | |
| 10 | TutorialImage | All slots | DONE | |
| 11 | TutorialImageHotkey | All slots | DONE | |
| 12 | TutorialImageInputKey | All slots | DONE | |
| 13 | TutorialImageToolbarIcon | All slots | DONE | |
| 14 | TutorialSetup | All slots | DONE | |
| 15 | TutorialSetupCamera | All slots | DONE | |
| 16 | TutorialSetupGameSpeed | All slots | DONE | |
| 17 | TutorialSetupRestrictPlayer | All slots | DONE | |
| 18 | TutorialHighlightDialog | All slots | DONE | |
| 19 | TutorialSetupDialog | All slots | DONE | |

---

## Remaining Items

None — all 65 classes fully reimplemented as of 2026-03-28.
