// code/Game/Action/completeroad.cpp
// Clean C++ reimplementation of CompleteRoad action methods.
// Source: Game/Action/CompleteRoad.cpp (from profile build debug strings)
//
// CompleteRoad -- 0x30 bytes. Completes road construction.
// Inherits: Object::Internal -> Action (0x20) -> CompleteRoad
// Vtable at 0x14015fb70 (10 slots).
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "CompleteRoad.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- CompleteRoad-specific helpers)
// ============================================================

// IO::Serializer::Serialize(unsigned char) -- via export
typedef void (*fn_Serialize_UChar_t)(void* self, void* field, const char* name);
static fn_Serialize_UChar_t s_fn_Serialize_UChar = nullptr;

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
    s_fn_Serialize_UChar = reinterpret_cast<fn_Serialize_UChar_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAuchar@PEBD@Z"));

    bool ok = s_pGameState && s_fn_CompleteRoadTile && s_fn_Serialize_UChar &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int;

#ifdef _DEBUG
    printf("[CompleteRoad] ResolveFuncs: GameState=%p CompleteRoadTile=%p SerUChar=%p\n",
           (void*)s_pGameState, (void*)s_fn_CompleteRoadTile,
           (void*)s_fn_Serialize_UChar);
    if (!ok) printf("[CompleteRoad] ERROR: some function pointers not resolved!\n");
#endif

    s_completeroad_funcs_resolved = ok;
    return ok;
}

// ============================================================
// CompleteRoad::Serialize (0x1400c5880)
// Serializes: status(+0x18), int(+0x20), uchar(+0x28), int(+0x24)
// ============================================================
void CompleteRoad_Serialize_Impl(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    // Serialize status
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize road fields
    g_fn_Serializer_Ctor(ser2, stream, 0);
    g_fn_Serialize_Int(ser2, s + 0x20, nullptr);
    s_fn_Serialize_UChar(ser2, s + 0x28, nullptr);
    g_fn_Serialize_Int(ser2, s + 0x24, nullptr);
    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// CompleteRoad::AbstractImpl / _purecall (0x1400c5850)
// Calls road tile completion on terrain system.
// Returns 0 (action complete).
// ============================================================
int CompleteRoad_AbstractImpl_Impl(void* self, void* actionStack) {
    CompleteRoad_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = (uint8_t*)self;
    void* gameState = *s_pGameState;
    void* terrain = *(void**)((uint8_t*)gameState + 0xf8);

    s_fn_CompleteRoadTile(terrain,
                          *(int32_t*)(s + 0x20),
                          *(uint8_t*)(s + 0x28),
                          *(uint32_t*)(s + 0x24));
    return 0;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fb70 (10 slots), size 0x30, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CompleteRoad_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("CompleteRoad");
    return c;
}

static void* g_CompleteRoad_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&CompleteRoad_GetTypeId,           // +0x10 GetTypeId
    (void*)&CompleteRoad_Serialize_Impl,      // +0x18 Serialize
    (void*)&CompleteRoad_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&Action_BaseCheckEntity,  // +0x40 CheckEntity
    (void*)&Action_BaseGetTarget, // +0x48 GetTarget
};

extern "C" void RegisterCompleteRoadVtable() {
    RegisterExeAddr(0x14015fb70, (uintptr_t)g_CompleteRoad_Vtable);
}
extern "C" void* CompleteRoad_CreateInPlace(void* mem) {
    CompleteRoad_ResolveFuncs();
    return Action_InitInPlace(mem, 0x30, g_CompleteRoad_Vtable);
}
extern "C" void* CompleteRoad_Factory(void) {
    CompleteRoad_ResolveFuncs();
    return Action_Alloc(0x30, g_CompleteRoad_Vtable);
}
REGISTER_TYPE(CompleteRoad, "Action", &CompleteRoad_Factory, &CompleteRoad_CreateInPlace);
#endif // STANDALONE_EXE
