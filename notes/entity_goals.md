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
| ComponentEntity | Vtable 0x14015fa78 | ComponentEntity.h (0x48) | **Deep reimpl** (Dtor, Serialize, Slot5, Slot8, Ctors, RefreshFlags, Startup/Shutdown) | High |
| EntityList | Update 0x1400c4090 | EntityList.h (0xE0) | **Deep reimpl** (Update, PreUpdate, PostUpdate, TreeNodeHandler, etc.) | High |
| CitizenBehavior | Update 0x14008c2b0 | CitizenBehavior.h (0x1C0) | **Partial reimpl** (Ctor, Update, ShuffleSchools, ShuffleEntities, FindAny) | High |

---

## ComponentEntity — Status: Deep Reimpl Complete (2026-03-28)

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| Dtor | 0x140021a00 | **Reimplemented** | Scalar deleting destructor |
| GetTypeInfo | 0x1400219b0 | Inline stub | 8b, unhookable |
| GetTypeId | 0x1400219a0 | Inline stub | 7b, unhookable |
| Serialize | 0x1400c3140 | **Reimplemented** | Hook DISABLED (bug class), clean code exists |
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

---

## EntityList — Status: Deep Reimpl Complete (2026-03-28)

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| Update | 0x1400c4090 | **Custom reimpl** | Original hangs (spinlock). Extracted from gameupdateloop.cpp |
| PreUpdate | 0x1400c3bd0 | Passthrough | Tree walk |
| PostUpdate | 0x1400c43d0 | Passthrough | Post-update cleanup |
| TreeNodeHandler | 0x1400c2de0 | **Reimplemented** | Inline — component vtable +0x40 dispatch |
| PostUpdateNodeHandler | 0x1400c2c70 | Passthrough | Component vtable +0x50 dispatch |
| RemoveEntity | 0x1400c38e0 | Passthrough | Entity removal from buckets/trees |
| FlagAggregation | 0x1400c3470 | Passthrough | Flag OR + staging array insertion |
| Serialize | 0x1400c44a0 | Passthrough | Complex save/load |

### Key findings:
- Component flags are at byte offset +0x20 (not +0x04)
- EntityList is 0xE0 bytes, not 0xE8 as originally estimated
- 5-bucket round-robin with binary tree overlay
- Not directly hooked — called from GameUpdateLoop::Update

---

## CitizenBehavior — Status: Partial Reimpl (2026-03-28)

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| Constructor | (inline in Load) | **Reimplemented** | 0x1C0 bytes, no vtable |
| Update | 0x14008c2b0 | **Reimplemented** | Timer-based dispatch to subsystems |
| ShuffleSchools | 0x1400896a0 | **Reimplemented** | Fisher-Yates shuffle |
| ShuffleEntities | 0x140089470 | **Reimplemented** | Per-profession entity shuffle |
| FindAny | 0x14008c4b0 | **Reimplemented** | Citizen lookup by profession |
| CountResources | 0x140089ed0 | Cataloged | Very large, delegates |
| CheckThresholds | 0x14008c100 | Cataloged | Resource shortage monitoring |
| CollectStats | 0x14008a730 | Cataloged | Population statistics |
| ProcessPaths | 0x1400d6f00 | Cataloged | Path graph computation |
| BuildPathGraph | 0x1400d70c0 | Cataloged | Path distance graph |
| FindIdle | 0x14008cbf0 | Cataloged | Already in engine_funcs |
| FindWorking | 0x14008ca20 | Cataloged | Already in engine_funcs |
| FindStudent | 0x14008c5f0 | Cataloged | Already in engine_funcs |
| RequestWorker | engine_funcs | Existing | Already resolved |
| AssignWorker | engine_funcs | Existing | Already resolved |
| RemoveWorker | engine_funcs | Existing | Already resolved |

### Key findings:
- Plain struct (no vtable), lives at GameUpdateLoop+0xF0
- Profession BST at +0x100, 21-entry resource array at +0x170
- PathGraph sub-object (0x28 bytes) at +0x1B8
- Timer-based subsystem dispatch (CountResources, CollectStats, ProcessPaths each on different intervals)

---

## Remaining Work

1. **CitizenBehavior large functions**: CountResources, CollectStats, CheckThresholds, ProcessPaths, BuildPathGraph — all cataloged but not yet reimplemented (complex, many dependencies)
2. **CitizenBehavior Find* functions**: FindIdle, FindWorking, FindStudent — signatures known, need clean reimpl
3. **EntityList passthroughs**: PreUpdate, PostUpdate, RemoveEntity, FlagAggregation, Serialize — all call trampolines, need decompilation
4. **ComponentEntity Slot3 hook**: Clean code exists but hook is DISABLED. Could enable via InstallHookVTable
