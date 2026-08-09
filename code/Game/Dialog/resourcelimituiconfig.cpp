// code/Game/Dialog/resourcelimituiconfig.cpp
// Clean reimplementation of ResourceLimitUIConfig — the per-row config behind each
// Town Hall resource-limit spinner.
//
// ResourceLimitUIConfig (0x28 bytes) — extends UI::DialogControllerConfig.
// Vtable at 0x14015d250.
//
//   +0x20  int  RESOURCE INDEX for this row:
//               Log=0 Stone=1 Iron=2 Herbs=3 Tools=4 Fuel=5 Food=6 Clothing=7
//               Alcohol=8 Coal=9 Textile=10  ->  townMgr(gw+0xf0)+0x170[idx*0x20+8]
//
// Factory       0x140013780: MemAlloc(0x28) + base ctor + vtable + zero +0x20 (4 bytes)
// CreateInPlace 0x1400137e0
// Serialize     0x140077b10 (vtable idx5, Ghidra "ResourceLimitUI_vslot17" — the name
//               lies, it is a ResourceLimitUIConfig method):
//                   IO::Serializer(stream, 0)   slot 0x140152ad8
//                   Serialize(int&, cfg+0x20)
//                   ~Serializer                 slot 0x140152ad0
//                   return 1
//               Retail INLINED the int serialize into the stream's own virtual call
//               (`MOV RCX,[RSP+0x30]; CALL [RAX+0x98]` — serializer+8 is the stream
//               impl, 0x98/8 = vtable idx 19). Dispatch slot 0x140152ac8 IS that
//               non-inlined helper; ControlUIConfig's Serialize (0x14005fb70) calls it
//               directly and was verified live this session, so we use the slot.
//
// WHY THIS EXISTS (2026-07-31): ResourceLimitUIConfig was a UI_DIALOG_STUB whose idx5
// was StubNop, so +0x20 stayed 0 for every row. The ResourceLimitUI promotion worked
// around that by deriving the resource index from DISPATCH ORDER, keyed on the
// controller pointer in an 11-entry static registry
// (ui_dialog_stub_factories.cpp IdxFor). That workaround is unsound across dialog
// rebuilds: reopening the Town Hall allocates fresh row controllers, and when the
// allocator REUSES an address from the previous build, IdxFor finds a stale match and
// returns the wrong index — desyncing every row after it. That is the user-reported
// "the food limit shows a different value each time I open and close the dialog".
//
// The workaround's stated premise — "standalone creates these config objects but never
// runs their field-deserialize, and controller+0x10 is always null" — is FALSE as of
// this session: promoting ControlUIConfig the same way produced real deserialized
// values ([ControlUIConfig] Serialize with cmd=0/1/2/3 and real target type ids), which
// is only possible if the framework both creates AND deserializes controller configs.
//
// Decompiled from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "address_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr uintptr_t kVtable = 0x14015d250;
constexpr int       kSlots  = 9;

void* g_vt[kSlots];

bool Trace() {
    static int t = -1;
    if (t < 0) {
        const char* e = std::getenv("BANISHED_LIMIT_TRACE");
        t = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return t == 1;
}

void Init(void* self) {
    if (fn_External_External) fn_External_External(self);
    *reinterpret_cast<void**>(self) = g_vt;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x20) = 0;
}

uint32_t GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ResourceLimitUIConfig");
    return cached;
}

// vtable idx5 (0x140077b10) — read this row's resource index.
char Serialize(void* self, void* stream) {
    if (!self || !g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return 1;
    uint8_t* c = reinterpret_cast<uint8_t*>(self);
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    if (fn_Ser_Slot_SerInt_AC8)
        fn_Ser_Slot_SerInt_AC8(serializer, reinterpret_cast<int32_t*>(c + 0x20), nullptr);
    g_fn_Serializer_Dtor(serializer);
    if (Trace()) {
        std::fprintf(stderr, "[ResourceLimitUIConfig] Serialize: cfg=%p resourceIndex(+0x20)=%d\n",
                     self, *reinterpret_cast<int32_t*>(c + 0x20));
        std::fflush(stderr);
    }
    return 1;
}

void* Dtor(void* self, uint32_t flags) {
    *reinterpret_cast<void**>(self) = g_vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

int   GetVersion(void*) { return 1; }
int   Return0(void*)    { return 0; }
void* ReturnNull(void*, ...) { return nullptr; }

}  // namespace

extern "C" void* ResourceLimitUIConfig_CreateInPlace(void* mem);

namespace {

void* Reconstruct(void* self) {
    if (fn_External_Dtor) fn_External_Dtor(self);
    return ResourceLimitUIConfig_CreateInPlace(self);
}

struct VtblInit {
    VtblInit() {
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[1] = reinterpret_cast<void*>(&ReturnNull);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[3] = reinterpret_cast<void*>(&GetVersion);
        g_vt[4] = reinterpret_cast<void*>(&Return0);
        g_vt[5] = reinterpret_cast<void*>(&Serialize);
        g_vt[6] = reinterpret_cast<void*>(&Reconstruct);
        g_vt[7] = reinterpret_cast<void*>(&Return0);
        g_vt[8] = reinterpret_cast<void*>(&ResourceLimitUIConfig_CreateInPlace);
    }
} g_vtblInit;

}  // namespace

extern "C" void RegisterResourceLimitUIConfigVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_vt));
}

extern "C" void* ResourceLimitUIConfig_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    Init(p);
    return p;
}

extern "C" void* ResourceLimitUIConfig_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    Init(mem);
    return mem;
}

REGISTER_TYPE(ResourceLimitUIConfig, "UI::DialogControllerConfig",
              &ResourceLimitUIConfig_Factory, &ResourceLimitUIConfig_CreateInPlace);
