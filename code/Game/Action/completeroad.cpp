// code/Game/Action/completeroad.cpp
// Clean C++ reimplementation of CompleteRoad action methods.
// Source: Game/Action/CompleteRoad.cpp (from profile build debug strings)
//
// CompleteRoad -- 0x30 bytes. Completes road construction.
// Inherits: Object::Internal -> Action (0x20) -> CompleteRoad
// Vtable at 0x14015fb70 (10 slots).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "CompleteRoad.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/exe_bodies.h"   // Pf_FinalizeRoadTile_9c640
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <new>

// ============================================================
// Helpers, called by name (B3R)
//   0x14009c640 Pf_FinalizeRoadTile_9c640 -- complete road tile (terrain*, tileIndex,
//               direction, roadType)
// DAT_1401c3dd8 (the game state singleton) is read through g_pGameWorldSingleton, the
// engine_funcs global bound to the same cell.
// ============================================================

// ============================================================
// CompleteRoad::Serialize (0x1400c5880)
// Serializes: status(+0x18), int(+0x20), uchar(+0x28), int(+0x24)
// ============================================================
void CompleteRoad::Persist(void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize road fields
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x20), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<unsigned char*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(s + 0x24), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// CompleteRoad::AbstractImpl / _purecall (0x1400c5850)
// Calls road tile completion on terrain system.
// Returns 0 (action complete).
// ============================================================

int CompleteRoad::AbstractImpl(void* actionStack) {                     // [4] 0x1400c5850
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    void* gameState = GameUpdateLoop::_singleton;
    void* terrain = *(void**)((uint8_t*)gameState + 0xf8);

    Pf_FinalizeRoadTile_9c640(static_cast<unsigned int*>(terrain),   // 0x14009c640
                              roadTileIndex,
                              roadDirection,
                              *(uint32_t*)(s + 0x24));
    return 0;
}

// ============================================================
// class CompleteRoad (CompleteRoad.h) — ENDGAME B.3.8 ACT. The hand-built
// g_CompleteRoad_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits the
// table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

// create 0x140021ec0 (inlined ctor). The tree's factory zeroed every field
// above +0x10; kept (B38 B8-13 / D1: no behaviour change in this step).
CompleteRoad::CompleteRoad()
    : roadTileIndex(0), roadType(0), roadDirection(0), _pad29{} {}

// 0x1400c59d0 (ActChild_CompleteRoadCopyCtor_c59d0 is its registered door):
// +0x20 = tileIndex (R8D), +0x24 = roadType (EDX), +0x28 = direction (R9B).
CompleteRoad::CompleteRoad(uint32_t roadTypeArg, uint32_t tileIndex, uint8_t direction)
    : roadTileIndex(static_cast<int32_t>(tileIndex)),
      roadType(static_cast<int32_t>(roadTypeArg)),
      roadDirection(direction) {}

void* CompleteRoad::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CompleteRoad), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CompleteRoad());
}
void* CompleteRoad::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CompleteRoad());
}


DEFINE_EXE_TYPE(CompleteRoad, "Action")
