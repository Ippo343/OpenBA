// engine/reimpl/completetask.cpp
// Reimplementation of CompleteTask action methods from Application-x64.exe.
//
// CompleteTask — 0x38 bytes. Completes a task/work order on a building.
// Inherits: Object::Internal -> Action (0x20) -> CompleteTask
// Vtable at 0x14015fbd0 (10 slots).
//
// Overrides vs Action base:
//   Serialize   (0x1400c5be0) — serializes status, entity(+0x20) DAT_1401c7158, entity(+0x28) ComponentEntity, int(+0x34), versioned int(+0x30)
//   _purecall   (0x1400c5b40) — resolves task index, calls task completion + cleanup
//   CheckEntity (0x1400c5ac0) — resolves task index, calls task reassignment
//   GetTarget   (0x1400c5a50) — matches +0x28 or entity parent at +0x20
//   Execute uses base. Enter/Exit use base.
//
// Field layout (0x38 bytes):
//   +0x20: void*    workEntity     (entity ref, DAT_1401c7158 type)
//   +0x28: void*    targetEntity   (entity ref, ComponentEntity type)
//   +0x30: int32_t  taskIndex      (defaults to 0x7FFFFFFF = unresolved)
//   +0x34: int32_t  taskParam

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

typedef void (*fn_CheckPtr_t)(void* ibp);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

typedef void (*fn_IBP_Serialize_t)(void* ibp, void* serializer);
static fn_IBP_Serialize_t fn_IBP_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serialize_Bool_t)(void* self, void* field, const char* name);
static fn_Serialize_Bool_t fn_Serialize_Bool = nullptr;
typedef void (*fn_Serialize_Int_t)(void* self, void* field, const char* name);
static fn_Serialize_Int_t fn_Serialize_Int = nullptr;
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// FUN_1400f8760 — try complete task
typedef bool (*fn_TryCompleteTask_t)(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam);
static fn_TryCompleteTask_t fn_TryCompleteTask = nullptr;

// FUN_1400f8e30 — cleanup after task completion
typedef void (*fn_TaskCleanup_t)(void* taskMgr, int taskIdx);
static fn_TaskCleanup_t fn_TaskCleanup = nullptr;

// FUN_1400f8800 — task reassignment (used by CheckEntity)
typedef void (*fn_TaskReassign_t)(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam);
static fn_TaskReassign_t fn_TaskReassign = nullptr;

static uint32_t* g_TypeInfo_DAT_1401c7158 = nullptr;
static uint32_t* g_TypeInfo_ComponentEntity = nullptr;
static void** g_pGameState = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// CompleteTask::Serialize (0x1400c5be0)
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status
    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    // Serialize entity refs + task fields (versioned)
    fn_Serializer_Ctor(ser2, stream, 1);
    fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *g_TypeInfo_DAT_1401c7158)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }
    fn_IBP_Serialize(s + 0x28, ser2);
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *g_TypeInfo_ComponentEntity)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }
    fn_Serialize_Int(ser2, s + 0x34, nullptr);
    int version = fn_Serializer_GetVersion(ser2);
    if (version > 0) {
        fn_Serialize_Int(ser2, s + 0x30, nullptr);
    }
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// CompleteTask::_purecall (0x1400c5b40)
// Resolves task index from work entity if needed, calls task completion.
// Returns 0 (action complete).
// ============================================================
static uint64_t Reimpl_Purecall(void* self) {
    uint8_t* s = (uint8_t*)self;

    // Lazy-resolve task index from work entity
    if (*(int32_t*)(s + 0x30) == 0x7FFFFFFF) {
        fn_CheckPtr(s + 0x20);
        *(int32_t*)(s + 0x30) = *(int32_t*)(*(uint8_t**)(s + 0x20) + 0x30);
    }

    int32_t taskIdx = *(int32_t*)(s + 0x30);
    if (taskIdx >= 0) {
        void* entityRef20 = *(void**)(s + 0x28);
        void* entityRef28 = *(void**)(s + 0x20);
        void* gameState = *g_pGameState;
        void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

        bool completed = fn_TryCompleteTask(taskMgr, &entityRef28, taskIdx, &entityRef20, *(uint32_t*)(s + 0x34));
        if (completed) {
            fn_TaskCleanup(taskMgr, taskIdx);
        }
    }
    return 0;
}

// ============================================================
// CompleteTask::CheckEntity (0x1400c5ac0)
// Resolves task index, calls task reassignment.
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    uint8_t* s = (uint8_t*)self;

    // Lazy-resolve task index
    if (*(int32_t*)(s + 0x30) == 0x7FFFFFFF) {
        fn_CheckPtr(s + 0x20);
        *(int32_t*)(s + 0x30) = *(int32_t*)(*(uint8_t**)(s + 0x20) + 0x30);
    }

    void* entityRef20 = *(void**)(s + 0x28);
    void* entityRef28 = *(void**)(s + 0x20);
    void* gameState = *g_pGameState;
    void* taskMgr = *(void**)((uint8_t*)gameState + 0x118);

    fn_TaskReassign(taskMgr, &entityRef28, *(int32_t*)(s + 0x30), &entityRef20, *(uint32_t*)(s + 0x34));
}

// ============================================================
// CompleteTask::GetTarget (0x1400c5a50)
// Returns true if param matches entity at +0x28 or entity at +0x20's parent.
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    uint8_t* s = (uint8_t*)self;

    fn_CheckPtr(param);
    void* paramVal = *(void**)param;

    if (*(void**)(s + 0x28) == paramVal) {
        return true;
    }

    fn_CheckPtr(s + 0x20);
    void* parentEntity = *(void**)(*(uint8_t**)(s + 0x20) + 0x10);
    fn_CheckPtr(param);
    return parentEntity == *(void**)param;
}

// ============================================================
// Install / Remove
// ============================================================

static void ResolveHelpers() {
    // CheckPtr is called indirectly via IAT at [0x140152ab0]
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    g_pGameState = (void**)ResolveAddr(Module::Exe, 0x1401c3dd8);
    g_TypeInfo_DAT_1401c7158 = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7158);
    g_TypeInfo_ComponentEntity = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4218);
    fn_TryCompleteTask = (fn_TryCompleteTask_t)ResolveAddr(Module::Exe, 0x1400f8760);
    fn_TaskCleanup = (fn_TaskCleanup_t)ResolveAddr(Module::Exe, 0x1400f8e30);
    fn_TaskReassign = (fn_TaskReassign_t)ResolveAddr(Module::Exe, 0x1400f8800);
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_Serialize_Int = (fn_Serialize_Int_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAint@PEBD@Z");
    fn_Serializer_GetVersion = (fn_Serializer_GetVersion_t)ResolveExport("?GetVersion@Serializer@IO@@QEAAHXZ");
    fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
}

void InstallCompleteTaskSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5be0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[CompleteTask] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCompleteTaskPurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5b40);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[CompleteTask] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskPurecall() { RemoveHook(&g_hookPurecall); }

void InstallCompleteTaskCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5ac0);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[CompleteTask] CheckEntity hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallCompleteTaskGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5a50);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[CompleteTask] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskGetTarget() { RemoveHook(&g_hookGetTarget); }
