// engine/reimpl/completeroad.cpp
// Reimplementation of CompleteRoad action methods from Application-x64.exe.
//
// CompleteRoad — 0x30 bytes. Completes road construction.
// Inherits: Object::Internal -> Action (0x20) -> CompleteRoad
// Vtable at 0x14015fb70 (10 slots).
//
// Overrides vs Action base:
//   Serialize  (0x1400c5880) — serializes status, int(+0x20), bool(+0x28), int(+0x24)
//   _purecall  (0x1400c5850) — calls road completion via terrain system
//   Execute/Enter/Exit/CheckEntity/GetTarget all use base implementations.
//
// Field layout (0x30 bytes):
//   +0x20: int32_t  roadTileIndex
//   +0x24: int32_t  roadType
//   +0x28: uint8_t  roadDirection
//   +0x29-0x2F: padding

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serialize_Bool_t)(void* self, void* field, const char* name);
static fn_Serialize_Bool_t fn_Serialize_Bool = nullptr;
typedef void (*fn_Serialize_Int_t)(void* self, void* field, const char* name);
static fn_Serialize_Int_t fn_Serialize_Int = nullptr;
typedef void (*fn_Serialize_UChar_t)(void* self, void* field, const char* name);
static fn_Serialize_UChar_t fn_Serialize_UChar = nullptr;

// FUN_14009c640 — complete road tile
// Params: terrain*, tileIndex, direction, roadType
typedef void (*fn_CompleteRoadTile_t)(void* terrain, int tileIndex, uint8_t direction, uint32_t roadType);
static fn_CompleteRoadTile_t fn_CompleteRoadTile = nullptr;

static void** g_pGameState = nullptr; // DAT_1401c3dd8

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;

// ============================================================
// CompleteRoad::Serialize (0x1400c5880)
// Serializes: status(+0x18), int(+0x20), uchar(+0x28), int(+0x24)
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[16];
    uint8_t ser2[24];

    // Serialize status
    fn_Serializer_Ctor(ser1, stream, 0);
    fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    fn_Serializer_Dtor(ser1);

    // Serialize road fields
    fn_Serializer_Ctor(ser2, stream, 0);
    fn_Serialize_Int(ser2, s + 0x20, nullptr);
    fn_Serialize_UChar(ser2, s + 0x28, nullptr);
    fn_Serialize_Int(ser2, s + 0x24, nullptr);
    fn_Serializer_Dtor(ser2);
}

// ============================================================
// CompleteRoad::_purecall (0x1400c5850)
// Calls road tile completion on terrain system.
// Returns 0 (action complete).
// ============================================================
static uint64_t Reimpl_Purecall(void* self) {
    uint8_t* s = (uint8_t*)self;
    void* gameState = *g_pGameState;
    void* terrain = *(void**)((uint8_t*)gameState + 0xf8);

    fn_CompleteRoadTile(terrain,
                        *(int32_t*)(s + 0x20),
                        *(uint8_t*)(s + 0x28),
                        *(uint32_t*)(s + 0x24));
    return 0;
}

// ============================================================
// Install / Remove
// ============================================================

static void ResolveHelpers() {
    g_pGameState = (void**)ResolveAddr(Module::Exe, 0x1401c3dd8);
    fn_CompleteRoadTile = (fn_CompleteRoadTile_t)ResolveAddr(Module::Exe, 0x14009c640);
    fn_Serializer_Ctor = (fn_Serializer_Ctor_t)ResolveExport("?Serializer@IO@@QEAA@PEAVStream@1@H@Z");
    fn_Serializer_Dtor = (fn_Serializer_Dtor_t)ResolveExport("??1Serializer@IO@@QEAA@XZ");
    fn_Serialize_Bool = (fn_Serialize_Bool_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    fn_Serialize_Int = (fn_Serialize_Int_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAint@PEBD@Z");
    fn_Serialize_UChar = (fn_Serialize_UChar_t)ResolveExport("?Serialize@Serializer@IO@@QEAAXAEAuchar@PEBD@Z");
}

void InstallCompleteRoadSerialize() {
    ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5880);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CompleteRoad] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteRoadSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCompleteRoadPurecall() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5850);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[CompleteRoad] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteRoadPurecall() { RemoveHook(&g_hookPurecall); }
