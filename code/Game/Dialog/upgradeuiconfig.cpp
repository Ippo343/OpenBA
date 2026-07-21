// code/Game/Dialog/upgradeuiconfig.cpp
// Clean reimplementation of UpgradeUIConfig (the upgrade panel's config resource).
//
// UpgradeUIConfig (0x28 bytes) — extends UI::DialogControllerConfig (byte-wise
// just Object::External's 0x20 base; the subclass adds one field).
// Registered by retail 0x14014ca00 into type-id slot DWORD_1401c5868.
// Vtable at 0x14015df60. NO "UpgradeDescription" class exists in retail — this
// config IS the upgrade definition:
//
//   +0x20  Object::ExternalPtr<ComponentEntity> — the upgrade TARGET BUILDING
//          template (e.g. the stone house). UpgradeUI's click handler
//          (0x140086c90) copies it into DestroyComponent+0x60 before pumping
//          the state machine; state-5 tick 0x14011c120 clones it via
//          ComponentEntity_CopyCtor to spawn the replacement construction site.
//
// Factory        0x140016c00: MemAlloc(0x28) + base ctor + vtable + clear +0x20
// CreateInPlace  0x140016c60: same, no alloc
// Serialize      0x140086fb0 (vtable +0x28): IO::Serializer(stream, 0) +
//                ExternalPtr<ComponentEntity>::Serialize on +0x20 — ported via
//                the canonical ExternalPtrSerializeGeneric body.
// GetVersion     (vtable +0x18, 0x140087000): return 1.
// GetTypeId      (vtable +0x10, 0x140016be0): return DWORD_1401c5868.
//
// Replaces the UI_DIALOG_STUB(UpgradeUIConfig, ...) zero-fill stub, whose +0x20
// stayed null — the upgrade button could never resolve a target.
//
// Decompiled from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "Game/serialize_externals.h"
#include "Object/factory_registration.h"
#include "address_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static const uintptr_t kUpgradeUIConfigVtable = 0x14015df60;

static void* g_UpgradeUIConfig_Vtable[9];

static void UpgradeUIConfig_Init(void* self) {
    if (fn_External_External) fn_External_External(self);
    *reinterpret_cast<void**>(self) = g_UpgradeUIConfig_Vtable;
    *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self) + 0x20) = nullptr;
}

static uint32_t UpgradeUIConfig_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("UpgradeUIConfig");
    return cached;
}

// vtable +0x28 (0x140086fb0): deserialize the target-template reference.
static void UpgradeUIConfig_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    ExternalPtrSerializeGeneric(
        reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self) + 0x20),
        serializer, g_TypeId_ComponentEntity, "ComponentEntity");
    g_fn_Serializer_Dtor(serializer);
    if (std::getenv("BANISHED_DESTROY_TRACE")) {
        std::fprintf(stderr, "[UpgradeUIConfig] Serialize: cfg=%p target(+0x20)=%p\n",
                     self, *reinterpret_cast<void**>(
                         reinterpret_cast<uint8_t*>(self) + 0x20));
        std::fflush(stderr);
    }
}

static void* UpgradeUIConfig_Dtor(void* self, uint32_t flags) {
    *reinterpret_cast<void**>(self) = g_UpgradeUIConfig_Vtable;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

static int   UpgradeUIConfig_GetVersion(void*) { return 1; }   // 0x140087000
static int   UpgradeUIConfig_Return0(void*)    { return 0; }
static void* UpgradeUIConfig_ReturnNull(void*, ...) { return nullptr; }

extern "C" void* UpgradeUIConfig_CreateInPlace(void* mem);

// vtable +0x30 (0x140016cb0): destroy-then-reconstruct in place.
static void* UpgradeUIConfig_Reconstruct(void* self) {
    if (fn_External_Dtor) fn_External_Dtor(self);
    return UpgradeUIConfig_CreateInPlace(self);
}

struct UpgradeUIConfig_VtblInit {
    UpgradeUIConfig_VtblInit() {
        g_UpgradeUIConfig_Vtable[0] = reinterpret_cast<void*>(&UpgradeUIConfig_Dtor);
        g_UpgradeUIConfig_Vtable[1] = reinterpret_cast<void*>(&UpgradeUIConfig_ReturnNull);
        g_UpgradeUIConfig_Vtable[2] = reinterpret_cast<void*>(&UpgradeUIConfig_GetTypeId);
        g_UpgradeUIConfig_Vtable[3] = reinterpret_cast<void*>(&UpgradeUIConfig_GetVersion);
        g_UpgradeUIConfig_Vtable[4] = reinterpret_cast<void*>(&UpgradeUIConfig_Return0);
        g_UpgradeUIConfig_Vtable[5] = reinterpret_cast<void*>(&UpgradeUIConfig_Serialize);
        g_UpgradeUIConfig_Vtable[6] = reinterpret_cast<void*>(&UpgradeUIConfig_Reconstruct);
        g_UpgradeUIConfig_Vtable[7] = reinterpret_cast<void*>(&UpgradeUIConfig_Return0);
        g_UpgradeUIConfig_Vtable[8] = reinterpret_cast<void*>(&UpgradeUIConfig_CreateInPlace);
    }
} g_UpgradeUIConfig_vtblInit;

extern "C" void RegisterUpgradeUIConfigVtable() {
    RegisterExeAddr(kUpgradeUIConfigVtable,
                    reinterpret_cast<uintptr_t>(g_UpgradeUIConfig_Vtable));
}

extern "C" void* UpgradeUIConfig_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    UpgradeUIConfig_Init(p);
    return p;
}

extern "C" void* UpgradeUIConfig_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    UpgradeUIConfig_Init(mem);
    return mem;
}

REGISTER_TYPE(UpgradeUIConfig, "UI::DialogControllerConfig",
              &UpgradeUIConfig_Factory, &UpgradeUIConfig_CreateInPlace);
