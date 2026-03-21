// engine/reimpl/removeanimal.cpp
// Hook wrappers for RemoveAnimal action from Application-x64.exe.
//
// RemoveAnimal -- smallest action (0x28 bytes). Removes an animal entity from the world.
// Inherits: Object::Internal -> Action (0x20) -> RemoveAnimal
// Vtable at 0x14015ffe0 (10 slots).
//
// Overrides vs Action base:
//   Serialize   (0x1400ca870) -- ENABLED hook, reimplemented
//   AbstractImpl(0x1400ca7f0) -- DISABLED (RIP-relative MOV in prologue), reimplemented in clean code
//   CheckEntity (0x1400ca760) -- DISABLED (RIP-relative CALL in prologue), reimplemented in clean code
//   GetTarget   (0x1400ca720) -- ENABLED hook, reimplemented
//   Execute/Enter/Exit use base.

#include "RemoveAnimal.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/removeanimal_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool RemoveAnimal_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// RemoveAnimal::Serialize (0x1400ca870) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "RemoveAnimal::Serialize";
    RemoveAnimal_Serialize_Impl((RemoveAnimal*)self, stream);
}

// ============================================================
// RemoveAnimal::AbstractImpl (0x1400ca7f0) -- DISABLED hook (RIP-relative MOV)
// Reimplemented in clean code for standalone exe goal.
// Uses int return (bug class 7).
// ============================================================
static int Reimpl_Purecall(void* self, void* actionStack) {
    g_lastHook = "RemoveAnimal::Purecall";
    return RemoveAnimal_AbstractImpl_Impl((RemoveAnimal*)self, actionStack);
}

// ============================================================
// RemoveAnimal::CheckEntity (0x1400ca760) -- DISABLED hook (RIP-relative CALL)
// Reimplemented in clean code for standalone exe goal.
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    g_lastHook = "RemoveAnimal::CheckEntity";
    RemoveAnimal_CheckEntity_Impl((RemoveAnimal*)self);
}

// ============================================================
// RemoveAnimal::GetTarget (0x1400ca720) -- reimplemented
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "RemoveAnimal::GetTarget";
    return RemoveAnimal_GetTarget_Impl((RemoveAnimal*)self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallRemoveAnimalSerialize() {
    if (!RemoveAnimal_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveAnimal] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca870);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[RemoveAnimal] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveRemoveAnimalSerialize() { RemoveHook(&g_hookSerialize); }

void InstallRemoveAnimalPurecall() {
    if (!RemoveAnimal_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveAnimal] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca7f0);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[RemoveAnimal] Purecall hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveRemoveAnimalPurecall() { RemoveHook(&g_hookPurecall); }

void InstallRemoveAnimalCheckEntity() {
    if (!RemoveAnimal_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveAnimal] ERROR: Failed to resolve helpers for CheckEntity\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca760);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 15);
#ifdef _DEBUG
    printf("[RemoveAnimal] CheckEntity hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveRemoveAnimalCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallRemoveAnimalGetTarget() {
    if (!RemoveAnimal_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveAnimal] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca720);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[RemoveAnimal] GetTarget hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveRemoveAnimalGetTarget() { RemoveHook(&g_hookGetTarget); }
