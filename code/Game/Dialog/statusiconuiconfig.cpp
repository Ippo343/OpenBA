// code/Game/Dialog/statusiconuiconfig.cpp
// Clean reimplementation of StatusIconUIConfig (the status-icon ribbon's config).
//
// StatusIconUIConfig (0x28 bytes) — extends UI::DialogControllerConfig. Registered
// by retail StatusIconUIConfig_Register 0x14014c840 into type-id slot
// DWORD_1401c56e0. Vtable 0x14015d900.
//
//   +0x20  bool _showOnMainPage
//
// Why it exists: a workplace dialog mounts the SAME "icons" ribbon twice
// (ToolKit/resource/Template/Pasture.rsc): once at "userGroup1" with no config and
// once at "userTitle2" with `_config = "CropField.rsc:titleBuildStatus"`, where
// CropField.rsc declares `StatusIconUIConfig titleBuildStatus { bool
// _showOnMainPage = false; }`. That single byte is what keeps the title-bar copy
// off the main info page — without it the stop-work icon renders twice.
//
// Retail bodies (all decompiled, not assumed):
//   Factory        0x140014250: MemAlloc(0x28) + DialogControllerConfig ctor +
//                  vtable + `this[0x20] = 0`  (ONE byte, not a qword).
//   CreateInPlace  0x1400142a0: same, no alloc.
//   vtable 0x14015d900 = { 0x140014320 dtor, 0x140014240 slot1, 0x140014230
//                  GetTypeId, 0x14007b450 GetVersion(=1), 0x14003a912,
//                  0x14007b400 Serialize, 0x1400142e0 reconstruct,
//                  0x140001640, 0x1400142a0 CreateInPlace }
//   Serialize      0x14007b400:
//                      IO::Serializer s(stream, 0);          // slot 0x140152ad8
//                      IO::Serializer::Serialize(s, (bool*)(this+0x20), nullptr);
//                                                            // slot 0x140152a58
//                      ~Serializer(s);                       // slot 0x140152ad0
//                      return 1;
//                  ONE BYTE off the stream. That is the whole point of promoting
//                  this class: the generic UI_DIALOG_STUB left slot 5 a StubNop,
//                  so the external-object load consumed ZERO bytes where the
//                  resource stream holds one — the external never resolved and
//                  every consumer saw a NULL config at controller+0x10.
//
// Replaces UI_DIALOG_STUB(StatusIconUIConfig, "UI::DialogControllerConfig", 0x28);
// that stub registration is removed to avoid a duplicate REGISTER_TYPE. Same
// structure as code/Game/Dialog/upgradeuiconfig.cpp.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void* g_StatusIconUIConfig_Vtable[9];

// IO::Serializer::Serialize(Serializer&, bool&, const char*) — exe dispatch slot
// 0x140152a58 (registered in standalone/address_registry.cpp).
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_SerializeBool = nullptr;

static void StatusIconUIConfig_ResolveFuncs() {
    if (s_fn_SerializeBool) return;
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_SerializeBool = *reinterpret_cast<fn_Serializer_SerializeBool_t*>(slot);
}

static void StatusIconUIConfig_Init(void* self) {
    if (fn_External_External) fn_External_External(self);
    *reinterpret_cast<void**>(self) = g_StatusIconUIConfig_Vtable;
    // Retail writes a single byte here (`this[0x20] = 0` on a char-typed this).
    *(reinterpret_cast<uint8_t*>(self) + 0x20) = 0;
}

static uint32_t StatusIconUIConfig_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StatusIconUIConfig");
    return cached;
}

// vtable +0x28 (0x14007b400): deserialize _showOnMainPage.
static bool StatusIconUIConfig_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return false;
    StatusIconUIConfig_ResolveFuncs();
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    if (s_fn_SerializeBool)
        s_fn_SerializeBool(serializer,
                           reinterpret_cast<bool*>(
                               reinterpret_cast<uint8_t*>(self) + 0x20),
                           nullptr);
    g_fn_Serializer_Dtor(serializer);
    return true;   // retail: MOV AL,1
}

static void* StatusIconUIConfig_Dtor(void* self, uint32_t flags) {
    *reinterpret_cast<void**>(self) = g_StatusIconUIConfig_Vtable;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

static int   StatusIconUIConfig_GetVersion(void*) { return 1; }   // 0x14007b450
static int   StatusIconUIConfig_Return0(void*)    { return 0; }
static void* StatusIconUIConfig_ReturnNull(void*, ...) { return nullptr; }

extern "C" void* StatusIconUIConfig_CreateInPlace(void* mem);

// vtable +0x30 (0x1400142e0): destroy-then-reconstruct in place.
static void* StatusIconUIConfig_Reconstruct(void* self) {
    if (fn_External_Dtor) fn_External_Dtor(self);
    return StatusIconUIConfig_CreateInPlace(self);
}

struct StatusIconUIConfig_VtblInit {
    StatusIconUIConfig_VtblInit() {
        g_StatusIconUIConfig_Vtable[0] = reinterpret_cast<void*>(&StatusIconUIConfig_Dtor);
        g_StatusIconUIConfig_Vtable[1] = reinterpret_cast<void*>(&StatusIconUIConfig_ReturnNull);
        g_StatusIconUIConfig_Vtable[2] = reinterpret_cast<void*>(&StatusIconUIConfig_GetTypeId);
        g_StatusIconUIConfig_Vtable[3] = reinterpret_cast<void*>(&StatusIconUIConfig_GetVersion);
        g_StatusIconUIConfig_Vtable[4] = reinterpret_cast<void*>(&StatusIconUIConfig_Return0);
        g_StatusIconUIConfig_Vtable[5] = reinterpret_cast<void*>(&StatusIconUIConfig_Serialize);
        g_StatusIconUIConfig_Vtable[6] = reinterpret_cast<void*>(&StatusIconUIConfig_Reconstruct);
        g_StatusIconUIConfig_Vtable[7] = reinterpret_cast<void*>(&StatusIconUIConfig_Return0);
        g_StatusIconUIConfig_Vtable[8] = reinterpret_cast<void*>(&StatusIconUIConfig_CreateInPlace);
    }
} g_StatusIconUIConfig_vtblInit;

extern "C" void* StatusIconUIConfig_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    StatusIconUIConfig_Init(p);
    return p;
}

extern "C" void* StatusIconUIConfig_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    StatusIconUIConfig_Init(mem);
    return mem;
}

REGISTER_TYPE(StatusIconUIConfig, "UI::DialogControllerConfig",
              &StatusIconUIConfig_Factory, &StatusIconUIConfig_CreateInPlace);
