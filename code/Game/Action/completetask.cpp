// code/Game/Action/completetask.cpp
// Clean C++ reimplementation of CompleteTask action methods.
// Source: Game/Action/CompleteTask.cpp (from profile build debug strings)
//
// CompleteTask — 0x38 bytes. Completes a task/work order on a building.
// Inherits: Object::Internal -> Action (0x20) -> CompleteTask
// Ctor: 0x140021fc0 (factory), Vtable: 0x14015fbd0
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — it is portable for standalone exe.

#include "CompleteTask.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Gated construction-haul completion trace (BANISHED_BUILD_TRACE). Pinpoints whether
// the citizen's delivery ever reaches CompleteTask::AbstractImpl and whether the task
// manager (gameState+0x118) is live enough to drive work->OnComplete. Diagnostic only.
static bool CT_Trace() {
    static int t = -1;
    if (t < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        t = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return t == 1;
}

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — CompleteTask-specific helpers)
// ============================================================

// Object::InternalBasePtr::Serialize(this, serializer) — via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// FUN_1400f8760 — try complete task
static bool (*s_fn_TryCompleteTask)(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam) = nullptr;

// FUN_1400f8e30 — cleanup after task completion
static void (*s_fn_TaskCleanup)(void* taskMgr, int taskIdx) = nullptr;

// FUN_1400f8800 — task reassignment (used by CheckEntity)
static void (*s_fn_TaskReassign)(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam) = nullptr;

// TypeInfo addresses
static uint32_t* s_TypeId_1401c7158 = nullptr;       // DAT_1401c7158 — work entity type
static uint32_t* s_TypeId_ComponentEntity = nullptr;  // DAT_1401c4218 — ComponentEntity

// Game state singleton
static void** s_pGameState = nullptr;  // DAT_1401c3dd8

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool CompleteTask_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // IBP::Serialize via IAT
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // Task functions
    s_fn_TryCompleteTask = reinterpret_cast<bool(*)(void*, void*, int, void*, uint32_t)>(
        ResolveAddr(Module::Exe, 0x1400f8760));
    s_fn_TaskCleanup = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1400f8e30));
    s_fn_TaskReassign = reinterpret_cast<void(*)(void*, void*, int, void*, uint32_t)>(
        ResolveAddr(Module::Exe, 0x1400f8800));

    // TypeInfo addresses
    s_TypeId_1401c7158 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    s_TypeId_ComponentEntity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4218));

    // Game state singleton
    s_pGameState = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3dd8));

    bool ok = s_fn_IBP_Serialize && s_fn_TryCompleteTask && s_fn_TaskCleanup &&
              s_fn_TaskReassign && s_TypeId_1401c7158 && s_TypeId_ComponentEntity &&
              s_pGameState && fn_InternalBasePtr_CheckPtr && fn_IsDerivedFrom &&
              fn_InternalBasePtr_DeletePtr && fn_ObjectInternal_Dtor && fn_MemFreeStatic &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int && g_fn_Serializer_GetVersion;

#ifdef _DEBUG
    printf("[CompleteTask] ResolveFuncs: IBPSer=%p TryComplete=%p TaskCleanup=%p TaskReassign=%p\n",
           (void*)s_fn_IBP_Serialize, (void*)s_fn_TryCompleteTask,
           (void*)s_fn_TaskCleanup, (void*)s_fn_TaskReassign);
    printf("[CompleteTask]   TypeId1=%p TypeIdCE=%p GameState=%p\n",
           (void*)s_TypeId_1401c7158, (void*)s_TypeId_ComponentEntity,
           s_pGameState ? *s_pGameState : nullptr);
    if (!ok) printf("[CompleteTask] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ScalarDtor (vtable slot 0)
// Sets vtables, calls DeletePtr on entity ref, ~Internal, optionally free.
// ============================================================
void* CompleteTask_ScalarDtor_Impl(CompleteTask* self, uint32_t flags) {
    // Set CompleteTask vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, CompleteTaskAddr::Vtable);
    // Set Action base vtable (destructor chain)
    uintptr_t actionVtable = ResolveAddr(Module::Exe, 0x14015faa0);
    self->vtable = (void*)actionVtable;

    // DeletePtr on entity IBP at +0x10 if non-null
    if (self->entity != nullptr) {
        fn_InternalBasePtr_DeletePtr(&self->entity);
    }

    fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Serialize (0x1400c5be0, vtable slot 3)
// Serializes status, entity refs with type checks, task fields.
// ============================================================
void CompleteTask_Serialize_Impl(CompleteTask* self, void* stream) {
    uint8_t ser1[24];
    uint8_t ser2[24];

    // Serialize status (version 0)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, &self->status, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize entity refs + task fields (version 1)
    g_fn_Serializer_Ctor(ser2, stream, 1);

    // Work entity ref (+0x20) with type validation
    s_fn_IBP_Serialize(&self->workEntity, ser2);
    if (self->workEntity != nullptr) {
        if (!fn_IsDerivedFrom(self->workEntity, *s_TypeId_1401c7158)) {
            self->workEntity = nullptr;
        }
    }

    // Target entity ref (+0x28) with type validation
    s_fn_IBP_Serialize(&self->targetEntity, ser2);
    if (self->targetEntity != nullptr) {
        if (!fn_IsDerivedFrom(self->targetEntity, *s_TypeId_ComponentEntity)) {
            self->targetEntity = nullptr;
        }
    }

    // Task param (+0x34)
    g_fn_Serialize_Int(ser2, &self->taskParam, nullptr);

    // Task index (+0x30) — versioned, only in version >= 1
    int version = g_fn_Serializer_GetVersion(ser2);
    if (version > 0) {
        g_fn_Serialize_Int(ser2, &self->taskIndex, nullptr);
    }

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// AbstractImpl (0x1400c5b40, vtable slot 4)
// Resolves task index from work entity if needed, calls task completion.
// Returns 0 (action complete).
// ============================================================
int CompleteTask_AbstractImpl_Impl(CompleteTask* self, void* actionStack) {
    (void)actionStack;
    // Lazy resolve (action built via exe ctor path -> ResolveFuncs never ran -> s_fn_* null).
    if (!CompleteTask_ResolveFuncs()) return 0;
    if (CT_Trace()) {
        void* gs = s_pGameState ? *s_pGameState : nullptr;
        void* tm = gs ? *(void**)((uint8_t*)gs + 0x118) : nullptr;
        std::fprintf(stderr, "[BuildTrace] CompleteTask::AbstractImpl self=%p gameState=%p taskMgr(+0x118)=%p taskIdx=%d taskParam=0x%x\n",
                     (void*)self, gs, tm, self->taskIndex, (unsigned)self->taskParam);
        std::fflush(stderr);
    }
    // Task machinery (gameState+0x118) is inert in standalone; guard so this is a safe
    // no-op (the actual log production is in the PerformWork chop, not this bookkeeping).
    if (!s_pGameState || !*s_pGameState ||
        !*(void**)((uint8_t*)*s_pGameState + 0x118)) return 0;

    // Lazy-resolve task index from work entity
    if (self->taskIndex == 0x7FFFFFFF) {
        fn_InternalBasePtr_CheckPtr(&self->workEntity);
        uint8_t* workEnt = (uint8_t*)self->workEntity;
        self->taskIndex = *(int32_t*)(workEnt + 0x30);
    }

    int32_t taskIdx = self->taskIndex;
    if (taskIdx >= 0) {
        void* entityRef28 = self->targetEntity;
        void* entityRef20 = self->workEntity;
        void* gameState = *s_pGameState;
        void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

        bool completed = s_fn_TryCompleteTask(taskMgr, &entityRef20, taskIdx, &entityRef28, (uint32_t)self->taskParam);
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
void CompleteTask_CheckEntity_Impl(CompleteTask* self) {
    if (!CompleteTask_ResolveFuncs()) return;   // lazy resolve (exe-ctor-built action)
    if (!s_pGameState || !*s_pGameState ||
        !*(void**)((uint8_t*)*s_pGameState + 0x118)) return;   // inert task mgr -> no-op
    // Lazy-resolve task index
    if (self->taskIndex == 0x7FFFFFFF) {
        fn_InternalBasePtr_CheckPtr(&self->workEntity);
        uint8_t* workEnt = (uint8_t*)self->workEntity;
        self->taskIndex = *(int32_t*)(workEnt + 0x30);
    }

    void* entityRef28 = self->targetEntity;
    void* entityRef20 = self->workEntity;
    void* gameState = *s_pGameState;
    void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

    s_fn_TaskReassign(taskMgr, &entityRef20, self->taskIndex, &entityRef28, (uint32_t)self->taskParam);
}

// ============================================================
// GetTarget (0x1400c5a50, vtable slot 9)
// Returns true if param matches entity at +0x28 or parent entity at +0x20.
// ============================================================
bool CompleteTask_GetTarget_Impl(CompleteTask* self, void* param) {
    if (!CompleteTask_ResolveFuncs()) return false;   // lazy resolve (exe-ctor-built action)
    fn_InternalBasePtr_CheckPtr(param);
    void* paramVal = *(void**)param;

    // Check direct match with target entity
    if (self->targetEntity == paramVal) {
        return true;
    }

    // Check parent entity of work entity
    fn_InternalBasePtr_CheckPtr(&self->workEntity);
    void* parentEntity = *(void**)((uint8_t*)self->workEntity + 0x10);
    fn_InternalBasePtr_CheckPtr(param);
    return parentEntity == *(void**)param;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fbd0 (10 slots), size 0x38, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CompleteTask_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("CompleteTask");
    return c;
}

static void* g_CompleteTask_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&CompleteTask_GetTypeId,           // +0x10 GetTypeId
    (void*)&CompleteTask_Serialize_Impl,      // +0x18 Serialize
    (void*)&CompleteTask_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&CompleteTask_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&CompleteTask_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterCompleteTaskVtable() {
    RegisterExeAddr(0x14015fbd0, (uintptr_t)g_CompleteTask_Vtable);
}
extern "C" void* CompleteTask_CreateInPlace(void* mem) {
    CompleteTask_ResolveFuncs();
    return Action_InitInPlace(mem, 0x38, g_CompleteTask_Vtable);
}
extern "C" void* CompleteTask_Factory(void) {
    CompleteTask_ResolveFuncs();
    return Action_Alloc(0x38, g_CompleteTask_Vtable);
}
REGISTER_TYPE(CompleteTask, "Action", &CompleteTask_Factory, &CompleteTask_CreateInPlace);
#endif // STANDALONE_EXE
