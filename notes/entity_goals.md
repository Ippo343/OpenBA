# Entity Module — Reimplementation Goals

**Directory:** `code/Game/Entity/`
**Type headers:** `engine/types/ComponentEntity.h`
**Source files:** None yet (empty directory)

---

## Overview

The entity layer manages ComponentEntity objects (the ECS "entity" that owns components), CitizenBehavior (AI state machine driver), and EntityList (the global entity collection iterated every frame).

---

## Classes

| Class | Key Address | Type Header | Reimpl Status | Priority |
|-------|------------|-------------|---------------|----------|
| ComponentEntity | Vtable 0x14015fa78 | ComponentEntity.h (0x48) | Not started | High |
| EntityList | Update 0x1400c4090 | - | Partial (custom reimpl in GameUpdateLoop) | High |
| CitizenBehavior | - | - | Not started | High |

---

## Key Functions

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| EntityList::Update | 0x1400c4090 | Custom reimpl | Original hangs (spinlock). 5-bucket iteration works. Tree node iteration skipped. |
| EntityList::PostUpdate | TBD | Passthrough | Called from GameUpdateLoop step 5 |
| EntityList::PreUpdate | 0x1400c3bd0 | Called via trampoline | Tree walk |
| ComponentEntity::GetComponent<T> | inline | Template | Cast pattern used everywhere |
| Entity add/remove | TBD | Not started | |

## Profile Source Paths

- `Entity\CitizenBehavior.cpp`
- `Entity\EntityList.cpp`
- `Entity\ComponentEntity.h`

## Assertions

- `entity->_entityList == NULL` — entity not in any list
- `entity->_entityList != NULL` — entity must be in a list
- `child.IsPtrValid() && child->_parent.IsPtrNull()` — child linkage validation
- `child.IsPtrValid() && child->_parent == this` — parent linkage validation

## Runtime Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| Object::InternalPtr<T> | Runtime | Entity smart pointers |
| InternalBasePtr::CheckPtr | Runtime | Null safety checks |
| InternalBasePtr::DeletePtr | Runtime | Entity deletion |
