// code/Game/Action/completetask.cpp
// Clean C++ reimplementation of CompleteTask action methods.
// Source: Game/Action/CompleteTask.cpp (from profile build debug strings)
//
// CompleteTask — 0x38 bytes. Completes a task/work order on a building.
// Inherits: Object::Internal -> Action (0x20) -> CompleteTask
// Ctor: 0x140021fc0 (factory), Vtable: 0x14015fbd0
//
// Decompiled from Application-x64.exe using Ghidra.

#include "CompleteTask.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"   // ActCt_* task helpers
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include "Component/CitizenComponent.h"   // ENDGAME B.3.8 CB3: CitizenComponent::TypeId()
#include "Entity/ComponentEntity.h"   // ENDGAME B.3.8 CB3: ComponentEntity::TypeId()

#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (heal .rdata type-ids)
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B


// ============================================================
// Helpers, called by name (B3R)
//   0x1400f8760 ActCt_TryCompleteTask_8760 -- try complete task
//   0x1400f8e30 ActCt_TaskCleanup_8e30     -- cleanup after task completion
//   0x1400f8800 ActCt_TaskReassign_8800    -- task reassignment (used by CheckEntity)
// DAT_1401c3dd8 (the game state singleton; the task manager is *(it+0x118)) is read
// through g_pGameWorldSingleton, the engine_funcs global bound to the same cell.
// ============================================================

// ============================================================
// Serialize (0x1400c5be0, vtable slot 3)
// Serializes status, entity refs with type checks, task fields.
// ============================================================
void CompleteTask::Persist(void* stream) {
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status (version 0)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<bool*>(&this->_status), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity refs + task fields (version 1)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Work entity ref (+0x20) with type validation. The .rdata type-id may be unavailable
    // in the standalone exe (a Serialize-only dep, not part of the exec gate) — validation
    // is simply skipped when the type-id can't be resolved.
    reinterpret_cast<Object::InternalBasePtr*>(&this->workEntity)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (this->workEntity != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(this->workEntity, CitizenComponent::TypeId())) {
            this->workEntity = nullptr;
        }
    }

    // Target entity ref (+0x28) with type validation
    reinterpret_cast<Object::InternalBasePtr*>(&this->targetEntity)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (this->targetEntity != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(this->targetEntity, ComponentEntity::TypeId())) {
            this->targetEntity = nullptr;
        }
    }

    // Task param (+0x34)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&this->taskParam), nullptr);   // B.3.4 bridge

    // Task index (+0x30) — versioned, only in version >= 1
    int version = reinterpret_cast<IO::Serializer*>(ser2)->GetVersion();   // B.3.4 bridge
    if (version > 0) {
        reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&this->taskIndex), nullptr);   // B.3.4 bridge
    }

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// AbstractImpl (0x1400c5b40, vtable slot 4)
// Resolves task index from work entity if needed, calls task completion.
// Returns 0 (action complete).
// ============================================================

int  CompleteTask::AbstractImpl(void* actionStack) {
    (void)actionStack;
    // Task machinery (gameState+0x118) is inert in standalone; guard so this is a safe
    // no-op (the actual log production is in the PerformWork chop, not this bookkeeping).
    if (GameUpdateLoop::_singleton == nullptr ||
        !*(void**)((uint8_t*)GameUpdateLoop::_singleton + 0x118)) return 0;

    // Lazy-resolve task index from work entity
    if (this->taskIndex == 0x7FFFFFFF) {
        uint8_t* workEnt = static_cast<uint8_t*>(workEntity);
        this->taskIndex = *(int32_t*)(workEnt + 0x30);
    }

    // taskIdx < 0 is EXPECTED, not a gap. The build-material haul tree
    // (GatherRawMaterials_Serialize 0x1400ec406) stamps CompleteTask+0x30 from the
    // Work item's +0x20 field ("pathType", reused as workId), which is -1 for hauls
    // that are NOT registered as a task-manager BST node — i.e. a building's ongoing
    // "needs N stone" that any number of builders may service. Retail runs the very
    // same no-op here (its own epilogue guards `-1 < Work+0x20` before setting the
    // citizen +0x88 flag, and citizenai_gather stamps citizen+0x30 = -1). Delivery is
    // credited via the BUILDCTRL MsgReceive path, not this manager completion. Verified
    // 2026-07-22 against the live BUILD_TRACE (taskIdx=-1, taskMgr live) — do not "fix".
    int32_t taskIdx = this->taskIndex;
    if (taskIdx >= 0) {
        void* entityRef28 = this->targetEntity;
        void* entityRef20 = this->workEntity;
        void* gameState = GameUpdateLoop::_singleton;
        void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

        bool completed = ActCt_TryCompleteTask_8760(taskMgr, &entityRef20, taskIdx, &entityRef28, (uint32_t)this->taskParam);   // 0x1400f8760
        if (completed) {
            ActCt_TaskCleanup_8e30(taskMgr, taskIdx);   // 0x1400f8e30
        }
    }
    return 0;
}

// ============================================================
// CheckEntity (0x1400c5ac0, vtable slot 8)
// Resolves task index, calls task reassignment.
// ============================================================
void CompleteTask::CheckEntity() {
    if (GameUpdateLoop::_singleton == nullptr ||
        !*(void**)((uint8_t*)GameUpdateLoop::_singleton + 0x118)) return;   // inert task mgr -> no-op
    // Lazy-resolve task index
    if (this->taskIndex == 0x7FFFFFFF) {
        uint8_t* workEnt = static_cast<uint8_t*>(workEntity);
        this->taskIndex = *(int32_t*)(workEnt + 0x30);
    }

    void* entityRef28 = this->targetEntity;
    void* entityRef20 = this->workEntity;
    void* gameState = GameUpdateLoop::_singleton;
    void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

    ActCt_TaskReassign_8800(taskMgr, &entityRef20, this->taskIndex, &entityRef28, (uint32_t)this->taskParam);   // 0x1400f8800
}

// ============================================================
// GetTarget (0x1400c5a50, vtable slot 9)
// Returns true if param matches entity at +0x28 or parent entity at +0x20.
// ============================================================
bool CompleteTask::GetTarget(void* param) {
    void* paramVal = *(void**)param;

    // Check direct match with target entity
    if (this->targetEntity == paramVal) {
        return true;
    }

    // Check parent entity of work entity
    void* parentEntity = *(void**)(static_cast<uint8_t*>(workEntity) + 0x10);
    return parentEntity == *(void**)param;
}

// ============================================================
// class CompleteTask (CompleteTask.h) — ENDGAME B.3.8 ACT.
// The hand-built g_CompleteTask_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140021fc0 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
CompleteTask::CompleteTask()
    : workEntity(nullptr),
      targetEntity(nullptr),
      taskIndex(0),
      taskParam(0) {}

// [3] 0x1400c5be0
// [4] 0x1400c5b40
// [8] 0x1400c5ac0
// [9] 0x1400c5a50

void* CompleteTask::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CompleteTask), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CompleteTask());
}
void* CompleteTask::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CompleteTask());
}


DEFINE_EXE_TYPE(CompleteTask, "Action")
