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
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdint>
#include <new>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- CompleteRoad-specific helpers)
// ============================================================

// FUN_14009c640 -- complete road tile
// Params: terrain*, tileIndex, direction, roadType
static void (*s_fn_CompleteRoadTile)(void* terrain, int tileIndex, uint8_t direction, uint32_t roadType) = nullptr;

// DAT_1401c3dd8 -- game state singleton pointer
static void** s_pGameState = nullptr;

static bool s_completeroad_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool CompleteRoad_ResolveFuncs() {
    if (s_completeroad_funcs_resolved) return true;

    s_pGameState = (void**)ResolveAddr(Module::Exe, 0x1401c3dd8);
    s_fn_CompleteRoadTile = reinterpret_cast<void(*)(void*, int, uint8_t, uint32_t)>(
        ResolveAddr(Module::Exe, 0x14009c640));
    bool ok = s_pGameState && s_fn_CompleteRoadTile;

#ifdef _DEBUG
    printf("[CompleteRoad] ResolveFuncs: GameState=%p CompleteRoadTile=%p\n",
           (void*)s_pGameState, (void*)s_fn_CompleteRoadTile);
    if (!ok) printf("[CompleteRoad] ERROR: some function pointers not resolved!\n");
#endif

    s_completeroad_funcs_resolved = ok;
    return ok;
}

// ============================================================
// CompleteRoad::Serialize (0x1400c5880)
// Serializes: status(+0x18), int(+0x20), uchar(+0x28), int(+0x24)
// ============================================================
void CompleteRoad::Persist(void* stream) {
    // Lazy-resolve, same reason AbstractImpl does it. Every in-flight CompleteRoad is
    // built by the COPY CTOR (0x1400c59d0, from work_act_perform.cpp), which bypasses
    // both the factory and CompleteRoad_ResolveFuncs -- so on the save path these
    // pointers were all still null and the first call was an EXEC of 0. It never
    // surfaced before because no CompleteRoad action had ever existed to be saved:
    // until roads started actually being built, the work item never got that far.
    // Same hole BuildRoad_ScalarDtor_Impl documents for the BuildRoad ctor path.
    CompleteRoad_ResolveFuncs();
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
    CompleteRoad_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    void* gameState = *s_pGameState;
    void* terrain = *(void**)((uint8_t*)gameState + 0xf8);

    s_fn_CompleteRoadTile(terrain,
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
#include "address_registry.h"

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

// [0] 0x140021f30: stamp 0x14015fb70, then Action's dtor, MemFree on &1.
Object::Base* CompleteRoad::DeletingDtor(unsigned flags) {
    this->~CompleteRoad();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

void* CompleteRoad::CreateNewObject() {
    CompleteRoad_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(CompleteRoad), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CompleteRoad());
}
void* CompleteRoad::CreateInPlaceObject(void* storage) {
    CompleteRoad_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CompleteRoad());
}

DEFINE_EXE_VTABLE(CompleteRoad, "_ZTV12CompleteRoad")

// B38 D5: 0x14015fb70 had one by-address reader, action_child_ctors2.cpp's
// door, which now builds the object with the real ctor — so the row is gone.
// The function stays until the coordinator's address_registry hunk drops its call.
extern "C" void RegisterCompleteRoadVtable() {}

DEFINE_EXE_TYPE(CompleteRoad, "Action")
