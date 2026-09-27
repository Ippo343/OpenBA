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
#include "resolve.h"
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
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — CompleteTask-specific helpers)
// ============================================================

// FUN_1400f8760 — try complete task
static bool (*s_fn_TryCompleteTask)(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam) = nullptr;

// FUN_1400f8e30 — cleanup after task completion
static void (*s_fn_TaskCleanup)(void* taskMgr, int taskIdx) = nullptr;

// FUN_1400f8800 — task reassignment (used by CheckEntity)
static void (*s_fn_TaskReassign)(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam) = nullptr;

// TypeInfo addresses
static uint32_t* s_TypeId_1401c7158 = nullptr;       // DAT_1401c7158 — work entity type
static uint32_t* s_TypeId_ComponentEntity = nullptr;  // DAT_1401c5d70 — ComponentEntity
    // (was mistranscribed 0x1401c4218 = FireDescription; retail CompleteTask serialize reads c5d70)

// Game state singleton
static void** s_pGameState = nullptr;  // DAT_1401c3dd8

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool CompleteTask_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Task functions
    s_fn_TryCompleteTask = reinterpret_cast<bool(*)(void*, void*, int, void*, uint32_t)>(
        ResolveAddr(Module::Exe, 0x1400f8760));
    s_fn_TaskCleanup = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1400f8e30));
    s_fn_TaskReassign = reinterpret_cast<void(*)(void*, void*, int, void*, uint32_t)>(
        ResolveAddr(Module::Exe, 0x1400f8800));

    // TypeInfo globals used ONLY by Serialize (defensive ref-type validation on load).
    // These are raw .rdata addresses -> NULL in the standalone exe. Heal them by name so
    // save/load validation still works, and — crucially — so they do NOT gate the exec
    // paths below (AbstractImpl/CheckEntity/GetTarget never touch them).
    s_TypeId_1401c7158 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    s_TypeId_ComponentEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5d70));
    static uint32_t s_healCitizen = 0, s_healCompEntity = 0;
    if (!s_TypeId_1401c7158) {
        s_healCitizen = CitizenComponent::TypeId();   // workEntity (+0x20)
        s_TypeId_1401c7158 = &s_healCitizen;
    }
    if (!s_TypeId_ComponentEntity) {
        s_healCompEntity = ComponentEntity::TypeId(); // targetEntity (+0x28)
        s_TypeId_ComponentEntity = &s_healCompEntity;
    }

    // Game state singleton (== g_pGameWorldSingleton; the task manager is *(this+0x118)).
    s_pGameState = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Exec-critical gate. The action methods (AbstractImpl/CheckEntity/GetTarget) must run
    // whenever the task-manager BST helpers, the game-state singleton, and the shared
    // InternalBasePtr ops are live — they never reference the Serialize-only pointers
    // (IBP::Serialize dispatch slot + the raw .rdata type-ids + the IO::Serializer fns).
    // Gating them on those Serialize deps is exactly what silently disabled the entire
    // completion path in the standalone exe (both are null there), so the task manager
    // was reachable for POST but never for complete/cancel.
    bool execOk = s_fn_TryCompleteTask && s_fn_TaskCleanup && s_fn_TaskReassign &&
                  s_pGameState && fn_IsDerivedFrom &&
                  fn_InternalBasePtr_DeletePtr && fn_ObjectInternal_Dtor && fn_MemFreeStatic;

#ifdef _DEBUG
    bool serializeOk = s_TypeId_1401c7158 && s_TypeId_ComponentEntity;
    printf("[CompleteTask] ResolveFuncs: execOk=%d serializeOk=%d TryComplete=%p TaskCleanup=%p TaskReassign=%p\n",
           (int)execOk, (int)serializeOk, (void*)s_fn_TryCompleteTask,
           (void*)s_fn_TaskCleanup, (void*)s_fn_TaskReassign);
    printf("[CompleteTask]   TypeId1=%p TypeIdCE=%p GameState=%p\n",
           (void*)s_TypeId_1401c7158, (void*)s_TypeId_ComponentEntity,
           s_pGameState ? *s_pGameState : nullptr);
    if (!execOk) printf("[CompleteTask] ERROR: exec-critical pointers not resolved!\n");
#endif

    s_funcs_resolved = execOk;
    return execOk;
}

// ============================================================
// Serialize (0x1400c5be0, vtable slot 3)
// Serializes status, entity refs with type checks, task fields.
// ============================================================
void CompleteTask::Persist(void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    CompleteTask_ResolveFuncs();
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
    if (this->workEntity != nullptr && s_TypeId_1401c7158 && *s_TypeId_1401c7158) {
        if (!fn_IsDerivedFrom(this->workEntity, *s_TypeId_1401c7158)) {
            this->workEntity = nullptr;
        }
    }

    // Target entity ref (+0x28) with type validation
    reinterpret_cast<Object::InternalBasePtr*>(&this->targetEntity)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (this->targetEntity != nullptr && s_TypeId_ComponentEntity && *s_TypeId_ComponentEntity) {
        if (!fn_IsDerivedFrom(this->targetEntity, *s_TypeId_ComponentEntity)) {
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
    // Lazy resolve (action built via exe ctor path -> ResolveFuncs never ran -> s_fn_* null).
    if (!CompleteTask_ResolveFuncs()) return 0;
    // Task machinery (gameState+0x118) is inert in standalone; guard so this is a safe
    // no-op (the actual log production is in the PerformWork chop, not this bookkeeping).
    if (!s_pGameState || !*s_pGameState ||
        !*(void**)((uint8_t*)*s_pGameState + 0x118)) return 0;

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
        void* gameState = *s_pGameState;
        void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

        bool completed = s_fn_TryCompleteTask(taskMgr, &entityRef20, taskIdx, &entityRef28, (uint32_t)this->taskParam);
        if (completed) {
            s_fn_TaskCleanup(taskMgr, taskIdx);
        }
    }
    return 0;
}

// ============================================================
// CheckEntity (0x1400c5ac0, vtable slot 8)
// Resolves task index, calls task reassignment.
// ============================================================
void CompleteTask::CheckEntity() {
    if (!CompleteTask_ResolveFuncs()) return;   // lazy resolve (exe-ctor-built action)
    if (!s_pGameState || !*s_pGameState ||
        !*(void**)((uint8_t*)*s_pGameState + 0x118)) return;   // inert task mgr -> no-op
    // Lazy-resolve task index
    if (this->taskIndex == 0x7FFFFFFF) {
        uint8_t* workEnt = static_cast<uint8_t*>(workEntity);
        this->taskIndex = *(int32_t*)(workEnt + 0x30);
    }

    void* entityRef28 = this->targetEntity;
    void* entityRef20 = this->workEntity;
    void* gameState = *s_pGameState;
    void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

    s_fn_TaskReassign(taskMgr, &entityRef20, this->taskIndex, &entityRef28, (uint32_t)this->taskParam);
}

// ============================================================
// GetTarget (0x1400c5a50, vtable slot 9)
// Returns true if param matches entity at +0x28 or parent entity at +0x20.
// ============================================================
bool CompleteTask::GetTarget(void* param) {
    if (!CompleteTask_ResolveFuncs()) return false;   // lazy resolve (exe-ctor-built action)
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

// [0] 0x140022030
Object::Base* CompleteTask::DeletingDtor(unsigned flags) {
    this->~CompleteTask();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400c5be0
// [4] 0x1400c5b40
// [8] 0x1400c5ac0
// [9] 0x1400c5a50

void* CompleteTask::CreateNewObject() {
    CompleteTask_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(CompleteTask), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CompleteTask());
}
void* CompleteTask::CreateInPlaceObject(void* storage) {
    CompleteTask_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CompleteTask());
}

DEFINE_EXE_VTABLE(CompleteTask, "_ZTV12CompleteTask")

// B38 D5: every by-address reader of 0x14015fbd0 was inside this block and now
// builds the object with the real ctor, so the row is gone. The function stays
// until the coordinator's address_registry hunk drops its call.
extern "C" void RegisterCompleteTaskVtable() {}

DEFINE_EXE_TYPE(CompleteTask, "Action")
