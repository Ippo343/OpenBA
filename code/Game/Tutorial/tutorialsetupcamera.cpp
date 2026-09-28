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

#include "Tutorial/TutorialSetupCamera.h"
#include "engine_funcs.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400e2200 (inlined in create 0x140028400 / createInPlace 0x140028470):
// External(), the TutorialSetup and TutorialSetupCamera stamps (the
// compiler's), then two qword stores of -1 over the four fields.
TutorialSetupCamera::TutorialSetupCamera()
    : cameraX(-1), cameraChunkRow(-1), cameraY(-1), cameraChunkCol(-1) {}

// ============================================================
// TutorialSetupCamera::Serialize (vtable +0x28, 0x1400e2120)
// ============================================================
// Serializes 4 int32 fields: cameraX, cameraChunkRow, cameraY, cameraChunkCol.
bool TutorialSetupCamera::Serialize(void* stream) {
    alignas(8) uint8_t serializer[32];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(cameraX, nullptr);          // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(cameraChunkRow, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(cameraY, nullptr);          // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(cameraChunkCol, nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// TutorialSetupCamera::Reconstruct [6] 0x1400284d0 / Destruct [7] 0x140001640
// ============================================================
Object::External* TutorialSetupCamera::Reconstruct() {
    this->~TutorialSetupCamera();
    return ::new (static_cast<void*>(this)) TutorialSetupCamera();
}
void TutorialSetupCamera::Destruct() { this->~TutorialSetupCamera(); }

// ============================================================
// TutorialSetupCamera::Begin (vtable +0x40, 0x1400e20d0)
// ============================================================
// absX = chunkRow * cellSize + cellX; absY = chunkCol * cellSize + cellY,
// written to the camera object (GameWorld+0x120) +0xa8 / +0xac, flag +0xa4.
void TutorialSetupCamera::Begin() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // Get map cell size from WorldGrid (+0xF8 is pointer to grid, first int is cell size)
    void** gridPtr = *reinterpret_cast<void***>(gameWorld + 0xF8);
    if (!gridPtr) return;
    int32_t cellSize = *reinterpret_cast<int32_t*>(gridPtr);

    // Get camera object from GameWorld+0x120
    uintptr_t camera = *reinterpret_cast<uintptr_t*>(gameWorld + 0x120);
    if (!camera) return;

    // Compute absolute camera position
    int32_t absX = cameraChunkRow * cellSize + cameraX;
    int32_t absY = cameraChunkCol * cellSize + cameraY;

    // Write to camera object
    *(int32_t*)(camera + 0xA8) = absX;
    *(uint8_t*)(camera + 0xA4) = 1;   // Flag: camera position is set / needs update
    *(int32_t*)(camera + 0xAC) = absY;
}

// ============================================================
// TutorialSetupCamera::Construct (vtable +0x58, 0x140028470)
// ============================================================
Object::External* TutorialSetupCamera::Construct() {
    return ::new (static_cast<void*>(this)) TutorialSetupCamera();
}

// create 0x140028400: MemAlloc(0x30, 8), then the (inlined) ctor.
void* TutorialSetupCamera::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialSetupCamera), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialSetupCamera());
}
// createInPlace 0x140028470.
void* TutorialSetupCamera::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialSetupCamera());
}

DEFINE_EXE_TYPE(TutorialSetupCamera, "TutorialSetup")
