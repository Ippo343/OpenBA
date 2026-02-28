// engine/reimpl/floorcomponent.cpp
// Reimplementation of FloorComponent lifecycle methods from Application-x64.exe.
//
// FloorComponent — Component subclass (0x30 bytes).
// Fields: floorFlag (byte +0x28), floorId (int32 +0x2C, init -1).
// Manages floor/ground plane rendering for buildings.
//
// Vtable at 0x140163360:
//   [3] Shutdown        = 0x14011F560  <-- HOOKED (serialize floorFlag as bool)
//   [6] Draw            = 0x14011F340  <-- PASSTHROUGH (IsDerivedFrom check + render)
//   [7] OnCreate        = 0x14011F2A0  <-- HOOKED (remove existing floor mesh)
//  [11] OnUpdate2       = 0x14011F540  <-- PASSTHROUGH (12-byte stub, too small for trampoline)
//   Other slots: NO-OP

#include "FloorComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t fn_Serializer_SerializeBool = nullptr;

// InternalBasePtr::CheckPtr — dispatch at [0x140152ab0]
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// FUN_140004680 — get entity data (owner -> entity data ptr)
typedef void* (*fn_GetEntityData_t)(void* owner, void* outPtr);
static fn_GetEntityData_t fn_GetEntityData = nullptr;

// FUN_14009cba0 — remove floor mesh from picking scene
typedef void (*fn_RemoveFloorMesh_t)(void* scene, uint32_t floorId,
    int x, int y, int w, int h);
static fn_RemoveFloorMesh_t fn_RemoveFloorMesh = nullptr;

static void** g_pGameState = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Shutdown (0x14011F560)
// Serializes base component, then serializes floorFlag (bool at +0x28).
// ============================================================
static void Reimpl_Shutdown(FloorComponent* self, void* stream) {
    fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    fn_Serializer_Ctor(serializer, stream, 0);
    fn_Serializer_SerializeBool(serializer, (bool*)&self->floorFlag, nullptr);
    fn_Serializer_Dtor(serializer);
}

// ============================================================
// Draw (0x14011F340) — PASSTHROUGH
// Checks IsDerivedFrom on _unk18, conditionally calls FUN_14011F390.
// ============================================================
static void Passthrough_Draw(FloorComponent* self) {
    typedef void (*orig_t)(FloorComponent*);
    ((orig_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// OnCreate (0x14011F2A0)
// If floorId >= 0, removes the existing floor mesh from the picking
// scene, then resets floorId to -1 and floorFlag to 0.
// ============================================================
static void Reimpl_OnCreate(FloorComponent* self) {
    if (self->floorId >= 0) {
        int* pScene = *(int**)((uintptr_t)*g_pGameState + 0xF8);
        fn_CheckPtr((void*)((uintptr_t)self + 0x10));  // check owner ptr

        // Get entity data from owner
        uintptr_t entityData = 0;
        fn_GetEntityData(*(void**)((uintptr_t)self + 0x10), &entityData);
        fn_CheckPtr(&entityData);

        // Extract coordinates from entity data
        int16_t x = *(int16_t*)(entityData + 0x28);
        int16_t y = *(int16_t*)(entityData + 0x2a);
        int16_t w = *(int16_t*)(entityData + 0x2c);
        int16_t h = *(int16_t*)(entityData + 0x2e);

        fn_RemoveFloorMesh(pScene, (uint32_t)self->floorId, (int)x, (int)y, (int)w, (int)h);

        self->floorId = -1;
        self->floorFlag = 0;
    }
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    fn_GetEntityData = reinterpret_cast<fn_GetEntityData_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    fn_RemoveFloorMesh = reinterpret_cast<fn_RemoveFloorMesh_t>(
        ResolveAddr(Module::Exe, 0x14009cba0));

    uintptr_t pCtorSlot = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t pDtorSlot = ResolveAddr(Module::Exe, 0x140152ad0);
    uintptr_t pSerBoolSlot = ResolveAddr(Module::Exe, 0x140152a58);
    uintptr_t pCheckPtrSlot = ResolveAddr(Module::Exe, 0x140152ab0);
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(pCtorSlot));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(pDtorSlot));
    fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(pSerBoolSlot));
    fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        *reinterpret_cast<uintptr_t*>(pCheckPtrSlot));

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor
        && fn_Serializer_SerializeBool && fn_CheckPtr && fn_GetEntityData
        && fn_RemoveFloorMesh;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFloorComponentShutdown() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[FloorComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011F560);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[FloorComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveFloorComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallFloorComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011F340);
    InstallHook(target, (void*)Passthrough_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[FloorComponent] Draw hooked at %p (passthrough)\n", (void*)target);
#endif
}
void RemoveFloorComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallFloorComponentOnCreate() {
    if (!fn_CheckPtr || !fn_GetEntityData || !fn_RemoveFloorMesh) {
        if (!ResolveHelpers()) {
#ifdef _DEBUG
            printf("[FloorComponent] ERROR: Failed to resolve OnCreate helpers\n");
#endif
            return;
        }
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011F2A0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[FloorComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveFloorComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

// NOTE: OnUpdate2 (0x14011F540) is a 12-byte stub function. The entire function
// body is exactly 12 bytes (CMP+JNZ+RET), making it unhookable — the trampoline
// would save the entire function but have nowhere to jump back to. This method
// is a conditional call into FUN_14011F390 (the complex floor mesh placement),
// which would need its own reimpl. Skipped for now.
