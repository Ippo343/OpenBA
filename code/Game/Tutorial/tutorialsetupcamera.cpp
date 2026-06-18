// code/Game/Tutorial/tutorialsetupcamera.cpp
// Clean reimplementation of TutorialSetupCamera vtable methods.
//
// TutorialSetupCamera (0x30 bytes), vtable at 0x140160ca0.
// Extends TutorialSetup (which extends Object::External).
//
// Fields (beyond External base at +0x08..+0x1F):
//   +0x20: int32 cameraX       — cell X offset within chunk
//   +0x24: int32 cameraChunkRow — chunk row index
//   +0x28: int32 cameraY       — cell Y offset within chunk
//   +0x2C: int32 cameraChunkCol — chunk column index
//
// Slot8 (Apply) computes absolute camera position:
//   absX = cameraChunkRow * mapCellSize + cameraX
//   absY = cameraChunkCol * mapCellSize + cameraY
// Then writes to the camera object at GameWorld+0x120.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Tutorial/tutorialsetupcamera_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Local function pointers resolved once
// ============================================================

// IO::Serializer::Serialize(this, int*, name) — dispatch slot 0x140152ac8
typedef void (*fn_Serializer_SerializeInt_t)(void* serializer, void* intPtr, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

static bool s_funcsResolved = false;

static void EnsureFuncsResolved() {
    if (s_funcsResolved) return;
    s_funcsResolved = true;

    // Resolve Serializer::Serialize(int*) from dispatch slot
    uintptr_t* slot = reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ac8));
    if (slot && *slot)
        s_fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(*slot);

#ifdef _DEBUG
    printf("[TutorialSetupCamera] SerializeInt=%p\n", (void*)s_fn_Serializer_SerializeInt);
#endif
}

// ============================================================
// TutorialSetupCamera::Dtor (vtable +0x00, 0x140028530)
// ============================================================
// Scalar deleting destructor.
// 1. Set vtable to TutorialSetupCamera
// 2. Set vtable to TutorialSetup (base class dtor chain)
// 3. Call External::~External(self)
// 4. If (flags & 1), free memory
// 5. Return self
void* TutorialSetupCamera_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialSetupCamera (this class)
    uintptr_t cameraVtable = ResolveAddr(Module::Exe, TutorialSetupCameraAddr::Vtable);
    *(uintptr_t*)s = cameraVtable;

    // Set vtable to TutorialSetup (base class dtor)
    uintptr_t setupVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
    *(uintptr_t*)s = setupVtable;

    // Call External::~External
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TutorialSetupCamera::Serialize (vtable +0x28, 0x1400e2120)
// ============================================================
// Serializes 4 int32 fields: cameraX, cameraChunkRow, cameraY, cameraChunkCol.
// Creates Serializer(stream, version=0), serializes fields, destroys serializer.
void TutorialSetupCamera_Serialize(void* self, void* stream) {
    EnsureFuncsResolved();
    uintptr_t s = (uintptr_t)self;

    // Create serializer
    uint8_t serializer[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize 4 int32 fields
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, reinterpret_cast<void*>(s + 0x20), nullptr); // cameraX
        s_fn_Serializer_SerializeInt(serializer, reinterpret_cast<void*>(s + 0x24), nullptr); // cameraChunkRow
        s_fn_Serializer_SerializeInt(serializer, reinterpret_cast<void*>(s + 0x28), nullptr); // cameraY
        s_fn_Serializer_SerializeInt(serializer, reinterpret_cast<void*>(s + 0x2C), nullptr); // cameraChunkCol
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// TutorialSetupCamera::Cleanup (vtable +0x30, 0x1400284d0)
// ============================================================
// Destroys and reinitializes: calls Dtor(flags=0) via vtable, then
// calls External::External to re-init base, sets Camera vtable, resets fields.
void TutorialSetupCamera_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call Dtor via vtable with flags=0 (no free)
    uintptr_t vtable = *(uintptr_t*)s;
    typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
    fn_Dtor_t dtorFn = *reinterpret_cast<fn_Dtor_t*>(vtable);
    if (dtorFn) {
        dtorFn(self, 0);
    }

    // Re-init External base
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain: TutorialSetup then TutorialSetupCamera
    uintptr_t setupVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
    *(uintptr_t*)s = setupVtable;

    uintptr_t cameraVtable = ResolveAddr(Module::Exe, TutorialSetupCameraAddr::Vtable);
    *(uintptr_t*)s = cameraVtable;

    // Reset fields to -1
    *(int64_t*)(s + 0x20) = -1LL;
    *(int64_t*)(s + 0x28) = -1LL;
}

// ============================================================
// TutorialSetupCamera::Slot8 / Apply (vtable +0x40, 0x1400e20d0)
// ============================================================
// Sets camera position from the 4 stored fields.
// Computes absolute position: absX = chunkRow * cellSize + cellX
//                             absY = chunkCol * cellSize + cellY
// Writes to camera object (GameWorld+0x120) fields +0xa4, +0xa8, +0xac.
void TutorialSetupCamera_Slot8(void* self) {
    uintptr_t s = (uintptr_t)self;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // Get map cell size from WorldGrid (+0xF8 is pointer to grid, first int is cell size)
    void** gridPtr = *reinterpret_cast<void***>(gameWorld + 0xF8);
    if (!gridPtr) return;
    int32_t cellSize = *reinterpret_cast<int32_t*>(gridPtr);

    // Get camera object from GameWorld+0x120
    uintptr_t camera = *reinterpret_cast<uintptr_t*>(gameWorld + 0x120);
    if (!camera) return;

    int32_t cellX = *(int32_t*)(s + 0x20);
    int32_t chunkRow = *(int32_t*)(s + 0x24);
    int32_t cellY = *(int32_t*)(s + 0x28);
    int32_t chunkCol = *(int32_t*)(s + 0x2C);

    // Compute absolute camera position
    int32_t absX = chunkRow * cellSize + cellX;
    int32_t absY = chunkCol * cellSize + cellY;

    // Write to camera object
    *(int32_t*)(camera + 0xA8) = absX;
    *(uint8_t*)(camera + 0xA4) = 1;   // Flag: camera position is set / needs update
    *(int32_t*)(camera + 0xAC) = absY;
}

// ============================================================
// TutorialSetupCamera::Ctor (vtable +0x58, 0x140028470)
// ============================================================
// Construct from raw memory. Null-check, call External::External,
// set vtable chain, init fields to -1.
void* TutorialSetupCamera_Ctor(void* self) {
    if (!self) return nullptr;

    uintptr_t s = (uintptr_t)self;

    // Init External base
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain: TutorialSetup then TutorialSetupCamera
    uintptr_t setupVtable = ResolveAddr(Module::Exe, TutorialSetupAddr::Vtable);
    *(uintptr_t*)s = setupVtable;

    uintptr_t cameraVtable = ResolveAddr(Module::Exe, TutorialSetupCameraAddr::Vtable);
    *(uintptr_t*)s = cameraVtable;

    // Init fields to -1
    *(int64_t*)(s + 0x20) = -1LL;
    *(int64_t*)(s + 0x28) = -1LL;

    return self;
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160ca0 (9 slots, Object::External base), size 0x30, parent "TutorialSetup".
// GetFileVersion (+0x18) = 1. MusicList pattern (musiclist.cpp).
// ============================================================
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialSetupCamera_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialSetupCamera");
    return c;
}
static int TutorialSetupCamera_GetFileVersion(void*) { return 1; }

static void* g_TutorialSetupCamera_Vtable[9] = {
    (void*)&TutorialSetupCamera_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialSetupCamera_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialSetupCamera_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialSetupCamera_Serialize,       // +0x28 Serialize
    (void*)&TutorialSetupCamera_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialSetupCamera_Ctor,              // +0x40 Ctor/CreateInPlace
};

extern "C" void RegisterTutorialSetupCameraVtable() {
    RegisterExeAddr(0x140160ca0, (uintptr_t)g_TutorialSetupCamera_Vtable);
}
extern "C" void* TutorialSetupCamera_CreateInPlace(void* mem) {
    return TutorialSetupCamera_Ctor(mem);
}
extern "C" void* TutorialSetupCamera_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    return p ? TutorialSetupCamera_Ctor(p) : nullptr;
}
REGISTER_TYPE(TutorialSetupCamera, "TutorialSetup", &TutorialSetupCamera_Factory, &TutorialSetupCamera_CreateInPlace);
#endif // STANDALONE_EXE
