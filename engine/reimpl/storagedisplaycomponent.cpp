// engine/reimpl/storagedisplaycomponent.cpp
// Reimplementation of StorageDisplayComponent lifecycle methods from Application-x64.exe.
//
// StorageDisplayComponent is a direct Component subclass (0x38 bytes) with heavy
// rendering involvement. Six non-trivial vtable overrides:
//
// Methods reimplemented:
//   - Shutdown (0x140056970) — Component_Serialize + empty Serializer block
//   - Serialize (0x140056930) — just RET 0x0 (3 bytes, unhookable no-op)
//   - Draw (0x140056730) — PASSTHROUGH: 111 instructions, complex rendering
//     (grid layout, transforms, geometry placement for storage display objects)
//   - OnCreate (0x1400566b0) — remove display objects from scene
//   - Update (0x1400566a0) — JMP thunk to FUN_1400563e0 (complex display update)
//     PASSTHROUGH: ~100 instructions, state machine lookups, proportional display
//   - OnUpdate2 (0x140056940) — calls Draw vtable + display rebuild
//
// Strategy: Shutdown and OnCreate are simple enough. Draw, Update, OnUpdate2
// are complex rendering functions — use passthrough.

#include "StorageDisplayComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;
static HookInfo g_hookUpdate;
static HookInfo g_hookOnUpdate2;

// ============================================================
// Original function typedefs for passthrough
// ============================================================
typedef void (*fn_StorageDisplay_Draw_t)(StorageDisplayComponent* self);
typedef void (*fn_StorageDisplay_Update_t)(StorageDisplayComponent* self);
typedef void (*fn_StorageDisplay_OnUpdate2_t)(StorageDisplayComponent* self);

// ============================================================
// Local helper function pointers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// Graphics::Scene::RemoveObject(scene, objectId) via IAT
typedef void (*fn_Scene_RemoveObject_t)(void* scene, uint32_t objectId);
static fn_Scene_RemoveObject_t fn_Scene_RemoveObject = nullptr;

// DAT_1401c3dd8 — game state singleton pointer
static void** g_pGameState = nullptr;

// ============================================================
// Shutdown (0x140056970)
// Simple: Component_Serialize + one empty Serializer block (version 0)
// ============================================================
static void Reimpl_Shutdown(StorageDisplayComponent* self, void* stream) {
    g_lastHook = "StorageDisplayComponent::Shutdown";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[StorageDisplayComponent::Shutdown] self=%p stream=%p\n",
               (void*)self, stream);
        callCount++;
    }
#endif

    fn_Component_Serialize(self, stream);

    uint8_t serializerBuf[16];
    fn_Serializer_Ctor(serializerBuf, stream, 0);
    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Draw (0x140056730) — PASSTHROUGH
// Complex grid-based rendering: computes positions from building
// dimensions, creates transforms with yaw/pitch offsets, sets
// geometry transforms for each display object. 111 instructions.
// ============================================================
static void Reimpl_Draw(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::Draw";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[StorageDisplayComponent::Draw] self=%p count=%d (passthrough)\n",
               (void*)self, *(int*)((uintptr_t)self + 0x30));
        callCount++;
    }
#endif

    ((fn_StorageDisplay_Draw_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// OnCreate (0x1400566b0)
// Iterates display objects, removes each from the graphics scene
// if its scene ID is valid (>= 0). Resets each to -1 after removal.
// Display objects are 0x128-byte entries in array at +0x28, count at +0x30.
// Scene ID is at entry+0x120.
// ============================================================
static void Reimpl_OnCreate(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::OnCreate";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[StorageDisplayComponent::OnCreate] self=%p count=%d\n",
               (void*)self, *(int*)((uintptr_t)self + 0x30));
        callCount++;
    }
#endif

    int count = *(int*)((uintptr_t)self + 0x30);
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)self + 0x28);

    // Get the graphics scene from game state
    uintptr_t gameState = (uintptr_t)*g_pGameState;
    void* scene = *(void**)(gameState + 0x60);

    for (int i = 0; i < count; i++) {
        uintptr_t entry = arrayBase + (uintptr_t)i * 0x128;
        uint32_t sceneId = *(uint32_t*)(entry + 0x120);

        if ((int32_t)sceneId >= 0) {
            fn_Scene_RemoveObject(scene, sceneId);
            *(uint32_t*)(entry + 0x120) = 0xFFFFFFFF; // Reset to -1
        }
    }
}

// ============================================================
// Update (0x1400566a0) — JMP thunk to 0x1400563e0 — PASSTHROUGH
// Complex display update: queries StateMachineComponent on the entity,
// iterates states, looks up building data via IsDerivedFrom,
// proportionally allocates display objects across resource types.
// ============================================================
static void Reimpl_Update(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[StorageDisplayComponent::Update] self=%p (passthrough)\n", (void*)self);
        callCount++;
    }
#endif

    ((fn_StorageDisplay_Update_t)g_hookUpdate.trampoline)(self);
}

// ============================================================
// OnUpdate2 (0x140056940)
// Calls this->vtable->Draw() then FUN_1400563e0 (display rebuild).
// Essentially: Draw + Update combined for refresh.
// PASSTHROUGH since it chains to complex rendering code.
// ============================================================
static void Reimpl_OnUpdate2(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::OnUpdate2";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[StorageDisplayComponent::OnUpdate2] self=%p (passthrough)\n", (void*)self);
        callCount++;
    }
#endif

    ((fn_StorageDisplay_OnUpdate2_t)g_hookOnUpdate2.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStorageDisplayComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor) {
#ifdef _DEBUG
        printf("[StorageDisplayComponent] ERROR: failed to resolve Shutdown helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140056970);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV RBX,RDX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] Shutdown hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallStorageDisplayComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140056730);
    // Prologue: 7 PUSHes (1+1+1+2+2+2+2=11) + SUB RSP,0xb0(7) = 18
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 19);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] Draw hooked at %p (prologue=18, passthrough)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallStorageDisplayComponentOnCreate() {
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // Graphics::Scene::RemoveObject — resolve via IAT at [0x140151360]
    uintptr_t iatRemoveObj = ResolveAddr(Module::Exe, 0x140151360);
    if (iatRemoveObj) fn_Scene_RemoveObject = *reinterpret_cast<fn_Scene_RemoveObject_t*>(iatRemoveObj);

    if (!g_pGameState || !fn_Scene_RemoveObject) {
#ifdef _DEBUG
        printf("[StorageDisplayComponent] ERROR: failed to resolve OnCreate helpers\n");
        printf("[StorageDisplayComponent]   g_pGameState = %p\n", (void*)g_pGameState);
        printf("[StorageDisplayComponent]   Scene_RemoveObject = %p\n", (void*)fn_Scene_RemoveObject);
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400566b0);
    // Prologue: MOV [RSP+0x10],RSI(5) + PUSH RDI(1) + SUB RSP,0x20(4) + XOR EDI,EDI(2) = 12
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] OnCreate hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallStorageDisplayComponentUpdate() {
    // Update is a JMP thunk at 0x1400566a0 -> 0x1400563e0
    // We hook 0x1400563e0 directly (the actual function)
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400563e0);
    // Check prologue of FUN_1400563e0
    // Prologue of FUN_1400563e0: PUSH RSI(1)+PUSH R15(2)+SUB RSP,0x88(7)+MOV R15,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 14);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] Update hooked at %p (prologue=14, passthrough)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentUpdate() { RemoveHook(&g_hookUpdate); }

void InstallStorageDisplayComponentOnUpdate2() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140056940);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RAX,[RCX](3) + MOV RBX,RCX(3) = 12
    InstallHook(target, (void*)Reimpl_OnUpdate2, &g_hookOnUpdate2, 12);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] OnUpdate2 hooked at %p (prologue=12, passthrough)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentOnUpdate2() { RemoveHook(&g_hookOnUpdate2); }
