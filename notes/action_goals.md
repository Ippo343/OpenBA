# Action Module — Reimplementation Goals

**Directory:** `code/Game/Action/`
**Type headers:** `engine/types/Action.h`, `AnimateNaturalResource.h`, `BuildActions.h`, `CompleteRoad.h`, `CompleteTask.h`, `ConsumeRawMaterial.h`, `CreateNaturalResource.h`, `CreateRawMaterial.h`, `DropOffRawMaterial.h`, `HuntAnimal.h`, `MoveTo.h`, `PerformWork.h`, `PickupRawMaterial.h`, `PickupStoredRawMaterial.h`, `PlayAnimation.h`, `RemoveAnimal.h`, `RemoveNaturalResource.h`, `SendComponentMessage.h`, `ShowTool.h`
**Source files:** None yet (empty directory)

---

## Overview

Actions are short-lived state machine operations pushed onto an ActionStack by CitizenAI/Work classes. Each Action has 3 virtual methods: AbstractImpl (+0x20), Execute (+0x28), Exit (+0x38). The action executor at `0x1400c4c50` drives them.

**Bug class 6 & 7 apply**: Action reimpl functions must use `int` return type (not `bool`) and forward RDX/R8 registers correctly. See CLAUDE.md.

---

## Classes (17 action types + 7 build sub-actions)

### Core Actions
| Class | Vtable | Type Header | Reimpl Status | Priority |
|-------|--------|-------------|---------------|----------|
| Action (base) | 0x14015faa0 | Action.h | Not started | - |
| MoveTo | 0x14015fe40 | MoveTo.h | Not started | High |
| PerformWork | 0x14015fea0 | PerformWork.h | Not started | High |
| ConsumeRawMaterial | 0x14015fc20 | ConsumeRawMaterial.h | Not started | High |
| CreateRawMaterial | 0x14015fd10 | CreateRawMaterial.h | Not started | Medium |
| CreateNaturalResource | 0x14015fca0 | CreateNaturalResource.h | Not started | Medium |
| DropOffRawMaterial | 0x14015fd80 | DropOffRawMaterial.h | Not started | Medium |
| PickupRawMaterial | 0x14015ff10 | PickupRawMaterial.h | Not started | Medium |
| PickupStoredRawMaterial | 0x14015ff80 | PickupStoredRawMaterial.h | Not started | Medium |
| RemoveAnimal | 0x14015ffe0 | RemoveAnimal.h | Not started | Low |
| RemoveNaturalResource | 0x140160050 | RemoveNaturalResource.h | Not started | Low |
| SendComponentMessage | 0x1401600c0 | SendComponentMessage.h | Not started | Low |
| AnimateNaturalResource | 0x14015fb10 | AnimateNaturalResource.h | Not started | Low |
| CompleteRoad | 0x14015fb70 | CompleteRoad.h | Not started | Low |
| CompleteTask | 0x14015fbd0 | CompleteTask.h | Not started | Low |
| HuntAnimal | 0x14015fde0 | HuntAnimal.h | Not started | Medium |
| PlayAnimation | 0x140158950 | PlayAnimation.h | Not started | Low |
| ShowTool | 0x1401589b0 | ShowTool.h | Not started | Low |

### Build Sub-Actions (from BuildActions.h)
| Class | Vtable | Reimpl Status |
|-------|--------|---------------|
| BuildRoadAction | 0x140161020 | Not started |
| BuildStructureAction | 0x1401610a0 | Not started |
| ClearAreaAction | 0x140161110 | Not started |
| DestroyAction | 0x140161220 | Not started |
| PlantAction | 0x140161420 | Not started |
| TerraformAction | 0x140161890 | Not started |
| ZoneAction | 0x140161910 | Not started |

---

## Runtime Dependencies

| Function | Source | Resolution |
|----------|--------|------------|
| Action executor | 0x1400c4c50 | Exe — drives AbstractImpl/Execute/Exit |
| ActionStack push/pop | TBD | Exe — stack management |
| Pathfinding::FindPath | 0x1400bf790 | Exe — used by MoveTo |
| Component queries | various | Exe — GetComponent<T> pattern |

---

## Per-Function Checklist Template

For each action class:
- [ ] Decompile in Ghidra (Ctor, AbstractImpl, Execute, Exit)
- [ ] Verify type header has correct layout + static_asserts
- [ ] Write clean C++ in `code/Game/Action/`
- [ ] Update hook wrapper in `engine/reimpl/` to call clean code
- [ ] Test with `mingw32-make deploy`
