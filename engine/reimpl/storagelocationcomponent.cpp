// engine/reimpl/storagelocationcomponent.cpp
// Hook wrappers for StorageLocationComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/storagelocationcomponent.cpp.
//
// StorageLocationComponent (0x48 bytes) is a ControllerComponent subclass with no
// additional fields. Registers entity as storage location.
// Vtable at 0x140164880. Non-trivial overrides:
//   Shutdown  (0x14012d2a0) — Component_Serialize + two empty Serializer blocks
//   OnCreate  (0x14012d270) — register entity with game world storage location list

#include "StorageLocationComponent.h"
#include "Component/storagelocationcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(StorageLocationComponent* self, void* stream) {
    g_lastHook = "StorageLocationComponent::Shutdown";
    StorageLocationComponent_Shutdown(self, stream);
}

static void Reimpl_OnCreate(StorageLocationComponent* self) {
    g_lastHook = "StorageLocationComponent::OnCreate";
    StorageLocationComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStorageLocationComponentShutdown() {
    StorageLocationComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012d2a0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x40(4) + MOV RBX,RDX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[StorageLocationComponent] Shutdown hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageLocationComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallStorageLocationComponentOnCreate() {
    StorageLocationComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012d270);
    // Prologue: SUB RSP,0x28(4) + MOV RAX,[RCX+0x10](4) + MOV RCX,[0x1401c3dd8](7) = 15
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[StorageLocationComponent] OnCreate hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageLocationComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
