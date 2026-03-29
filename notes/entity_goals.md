# Entity Module — Reimplementation Goals

**Directory:** `code/Game/Entity/`
**Type headers:** `engine/types/ComponentEntity.h`, `engine/types/EntityList.h`, `engine/types/CitizenBehavior.h`
**Source files:** `componententity.cpp`, `entitylist.cpp`, `citizenbehavior.cpp` + `_funcs.h` headers

---

## Overview

The entity layer manages ComponentEntity objects (the ECS "entity" that owns components), EntityList (the global entity collection iterated every frame), and CitizenBehavior (population management — worker assignment, profession tracking, statistics).

---

## Classes

| Class | Key Address | Type Header | Reimpl Status | Priority |
|-------|------------|-------------|---------------|----------|
| ComponentEntity | Vtable 0x14015fa78 | ComponentEntity.h (0x48) | **Complete** | Done |
| EntityList | Update 0x1400c4090 | EntityList.h (0xE0) | **Complete** | Done |
| CitizenBehavior | Update 0x14008c2b0 | CitizenBehavior.h (0x1C0) | **Near-complete** (3 delegate) | Done |

---

## ComponentEntity — Status: Deep Reimpl Complete (2026-03-28)

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| Dtor | 0x140021a00 | **Reimplemented** | Scalar deleting destructor |
| GetTypeInfo | 0x1400219b0 | Inline stub | 8b, unhookable |
| GetTypeId | 0x1400219a0 | Inline stub | 7b, unhookable |
| Serialize | 0x1400c3140 | **Reimplemented** | Hook RE-ENABLED via InstallHookVTable (2026-03-29) |
| Slot5 | 0x140021c90 | **Reimplemented** | Secondary destructor variant |
| Slot8 | 0x1400c4b40 | **Reimplemented** | Entity ID serializer |
| CtorDefault | 0x1400c3620 | **Reimplemented** | All fields initialized |
| CtorFromDescription | 0x1400c3720 | **Reimplemented** | Factory-based component creation |
| RefreshEntityFlags | 0x1400c3470 | **Reimplemented** | Flag aggregation + EntityList bucket registration |
| ShutdownComponents | 0x1400c3000 | **Reimplemented** | Calls component vtable+0x28 |
| StartupComponents | 0x1400c30a0 | **Reimplemented** | Calls component vtable+0x20 |

### Key findings:
- Slot9 (0x14003addc) is CRT `_purecall`, not entity-specific
- Dtor body at 0x1400c3550 was mislabeled as "Ctor" in Ghidra
- Slot5 uses secondary vtable 0x14015faa0
- **Slot3 hook fix (2026-03-29)**: Changed from InstallHook (prologue patching) to InstallHookVTable (vtable swap) — fixes bug class 8 (SEH cookie corruption). Disabled hooks: 4 → 3.

---

## EntityList — Status: Deep Reimpl Complete (2026-03-29)

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| Update | 0x1400c4090 | **Custom reimpl** | Original hangs (spinlock). Extracted from gameupdateloop.cpp |
| PreUpdate | 0x1400c3bd0 | **Reimplemented** | Processes staging arrays, inserts/removes from update tree |
| PostUpdate | 0x1400c43d0 | **Reimplemented** | Calls PostUpdateStaging, then tree walk with PostUpdateNodeHandler |
| PostUpdateStaging | 0x1400c3ab0 | **Reimplemented** | Same pattern as PreUpdate for post-update tree |
| TreeNodeHandler | 0x1400c2de0 | **Reimplemented** | Inline — component vtable +0x40 dispatch |
| PostUpdateNodeHandler | 0x1400c2c70 | **Reimplemented** | Component vtable +0x50 dispatch (mirrors TreeNodeHandler) |
| RemoveEntity | 0x1400c38e0 | **Reimplemented** | Swap-remove from bucket + master array, BST remove from both trees |
| FlagAggregation | 0x1400c3470 | **Delegates** | Same function as ComponentEntity_RefreshEntityFlags |
| Serialize | 0x1400c44a0 | **Reimplemented** | Full save/load: staging flush, time/IBP/tree/bucket serialization |

### Key findings:
- Component flags are at byte offset +0x20 (not +0x04)
- EntityList is 0xE0 bytes, not 0xE8 as originally estimated
- 5-bucket round-robin with binary tree overlay
- EntityTreeNode is 0x28 bytes (has AVL balance/height fields beyond the 0x20 basic layout)
- FlagAggregation (0x1400c3470) confirmed same address as ComponentEntity_RefreshEntityFlags
- Not directly hooked — called from GameUpdateLoop::Update

---

## CitizenBehavior — Status: Near-Complete (2026-03-29)

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| Constructor | (inline in Load) | **Reimplemented** | 0x1C0 bytes, no vtable |
| Update | 0x14008c2b0 | **Reimplemented** | Timer-based dispatch to subsystems, all calls now direct |
| ShuffleSchools | 0x1400896a0 | **Reimplemented** | Fisher-Yates shuffle |
| ShuffleEntities | 0x140089470 | **Reimplemented** | Per-profession entity shuffle |
| FindAny | 0x14008c4b0 | **Reimplemented** | Citizen lookup by profession |
| CountResources | 0x140089ed0 | **Reimplemented** | Two-phase resource tally across buildings, 21-bit profession flag mapping |
| CheckThresholds | 0x14008c100 | **Reimplemented** | 5% shortage thresholds, notification events, 120s cooldown |
| CollectStats | 0x14008a730 | **Delegates** | 600+ line mechanical function, delegates to original |
| ProcessPaths | 0x1400d6f00 | **Reimplemented** | Up to 7 path calcs/call, BST distance cache, Euclidean fallback |
| BuildPathGraph | 0x1400d70c0 | **Reimplemented** | Collects building grid IDs, generates entity-building pairs |
| FindIdle | 0x14008cbf0 | **Delegates** | Complex nested loops, delegates to fn_CitizenList_FindIdle |
| FindWorking | 0x14008ca20 | **Delegates** | Path distance comparison, delegates to fn_CitizenList_FindWorking |
| FindStudent | 0x14008c5f0 | **Delegates** | School iteration, delegates to fn_CitizenList_FindStudent |
| RequestWorker | engine_funcs | Existing | Already resolved |
| AssignWorker | engine_funcs | Existing | Already resolved |
| RemoveWorker | engine_funcs | Existing | Already resolved |

### Key findings:
- Plain struct (no vtable), lives at GameUpdateLoop+0xF0
- Profession BST at +0x100, 21-entry resource array at +0x170
- PathGraph sub-object (0x28 bytes) at +0x1B8
- Timer-based subsystem dispatch (CountResources, CollectStats, ProcessPaths each on different intervals)
- CountResources: RawMaterialFlags at +0x5c map to profession entries via non-sequential bit ordering
- CheckThresholds: profession 3 (Fisherman) combines with profession 9 (Stonecutter) for threshold check
- 6 new engine_funcs added for path processing (Entity_GetGridId, PathBST_Insert/Free, PathPairs_Clear, etc.)

---

## Remaining Work

1. **CitizenBehavior delegates**: CollectStats (~600 lines, mechanical), FindIdle/FindWorking/FindStudent (complex nested loops with path distance) — all delegate to original. Could be reimplemented for standalone exe completeness but are low priority.
2. **All entity module hooks are now enabled** except the 3 remaining disabled hooks (Modification::SetupFileSystem, WorkComponent::Shutdown, FinishTutorialInstance::Slot5 — unrelated to entity module).
