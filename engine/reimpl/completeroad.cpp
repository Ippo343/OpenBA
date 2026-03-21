// engine/reimpl/completeroad.cpp
// Hook wrappers for CompleteRoad action from Application-x64.exe.
//
// CompleteRoad -- 0x30 bytes. Completes road construction.
// Inherits: Object::Internal -> Action (0x20) -> CompleteRoad
// Vtable at 0x14015fb70 (10 slots).
//
// Vtable (10 slots):
//   [0]  +0x00  ScalarDtor    = 0x1400c49f0   (base Action, not hooked)
//   [1]  +0x08  VectorDtor    = 0x1400c49a0   (base Action, not hooked)
//   [2]  +0x10  BaseDtor      = 0x1400c4990   (base Action, not hooked)
//   [3]  +0x18  Serialize     = 0x1400c5880   <-- reimplemented
//   [4]  +0x20  AbstractImpl  = 0x1400c5850   <-- reimplemented (_purecall)
//   [5]  +0x28  Execute       = 0x1400c4a90   (base Action, not hooked)
//   [6]  +0x30  Enter         = 0x1400c4b90   (base Action, not hooked)
//   [7]  +0x38  Exit          = 0x1400c4a90   (base Action, not hooked)
//   [8]  +0x40  CheckEntity   = 0x1400c4a90   (base Action, not hooked)
//   [9]  +0x48  GetTarget     = 0x1400c4a70   (base Action, not hooked)

#include "CompleteRoad.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/completeroad_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool CompleteRoad_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;

// ============================================================
// CompleteRoad::Serialize (0x1400c5880) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "CompleteRoad::Serialize";
    CompleteRoad_Serialize_Impl(self, stream);
}

// ============================================================
// CompleteRoad::AbstractImpl / _purecall (0x1400c5850) -- reimplemented
// ============================================================
static int Reimpl_Purecall(void* self, void* actionStack) {
    g_lastHook = "CompleteRoad::Purecall";
    return CompleteRoad_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallCompleteRoadSerialize() {
    if (!CompleteRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CompleteRoad] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5880);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CompleteRoad] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteRoadSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCompleteRoadPurecall() {
    if (!CompleteRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CompleteRoad] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5850);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[CompleteRoad] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteRoadPurecall() { RemoveHook(&g_hookPurecall); }
