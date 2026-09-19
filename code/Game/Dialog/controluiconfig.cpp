// code/Game/Dialog/controluiconfig.cpp
// Clean reimplementation of ControlUIConfig (the config resource behind every
// generic ControlUI button — pasture work/split/empty, and any other .rsc button
// bound to a component command).
//
// ControlUIConfig (0x28 bytes) — extends UI::DialogControllerConfig (Object::External's
// 0x20 base plus two 4-byte fields). Vtable at 0x14015b3e0.
//
//   +0x20  int          COMMAND ID passed to the target component's vtable[14].
//                       For PastureComponent: work = 2, split = 1, empty = 0.
//                       Also the key ControlUI feeds to vtable[13] (the can-command /
//                       checked-state query) in Slot4/Slot7.
//   +0x24  Object::Type TARGET COMPONENT type — which component on the selected
//                       entity the button drives. ControlUI::Slot4 hands this to
//                       FindComponentByTypeId and caches the result at ControlUI+0x38.
//
// Factory       0x140010cd0: MemAlloc(0x28) + base ctor + vtable + zero 8 bytes at
//                            +0x20 (ONE 8-byte store covering BOTH fields).
// CreateInPlace 0x140010d30: same, no alloc.
// Serialize     0x14005fb70 (vtable idx5, Ghidra "ControlUI_vslot17" — the name lies,
//                            it is a ControlUIConfig method):
//                   IO::Serializer(stream, 0)          slot 0x140152ad8
//                   Serialize(int&,  cfg+0x20)         slot 0x140152ac8
//                   Object::Type::Serialize(cfg+0x24)  slot 0x1401516b0
//                   ~Serializer                        slot 0x140152ad0
//                   return 1
//               NOTE the int slot is 0xAC8, NOT the 0xA50 uint overload other
//               call sites use — verified by `disassemble_function 0x14005fb70 CALL`.
// GetVersion    (idx3): return 1.
//
// Replaces UI_DIALOG_STUB(ControlUIConfig, ...), whose idx5 was StubNop — so the
// config stayed zero-filled, every button's command read as 0 ("empty"), and its
// target component type read as 0. Together with the ControlUI stub (both were
// no-ops) that is why the pasture work/split buttons did nothing at all.
//
// Decompiled from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "address_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

namespace {

constexpr uintptr_t kControlUIConfigVtable = 0x14015b3e0;
constexpr int       kSlots = 9;

void* g_vt[kSlots];

typedef void (*fn_Type_Serialize_t)(void* type, void* serializer);
fn_Type_Serialize_t s_fn_Type_Serialize = nullptr;
bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
    uintptr_t slot = ResolveAddr(Module::Exe, 0x1401516b0);
    if (slot) s_fn_Type_Serialize = *reinterpret_cast<fn_Type_Serialize_t*>(slot);
    s_resolved = true;
}


void Init(void* self) {
    if (fn_External_External) fn_External_External(self);
    *reinterpret_cast<void**>(self) = g_vt;
    // Retail stores ONE zero qword here, covering +0x20 (command) and +0x24 (type).
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(self) + 0x20) = 0;
}

uint32_t GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ControlUIConfig");
    return cached;
}

// vtable idx5 (0x14005fb70) — read the command id and the target component type.
char Serialize(void* self, void* stream) {
    EnsureResolved();
    if (!self) return 1;
    uint8_t* c = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(c + 0x20), nullptr);   // B.3.4 bridge
    if (s_fn_Type_Serialize)
        s_fn_Type_Serialize(c + 0x24, serializer);
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
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

extern "C" void* ControlUIConfig_CreateInPlace(void* mem);

namespace {

// idx6: destroy-then-reconstruct in place.
void* Reconstruct(void* self) {
    if (fn_External_Dtor) fn_External_Dtor(self);
    return ControlUIConfig_CreateInPlace(self);
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
        g_vt[8] = reinterpret_cast<void*>(&ControlUIConfig_CreateInPlace);
    }
} g_vtblInit;

}  // namespace

extern "C" void RegisterControlUIConfigVtable() {
    RegisterExeAddr(kControlUIConfigVtable, reinterpret_cast<uintptr_t>(g_vt));
}

extern "C" void* ControlUIConfig_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    Init(p);
    return p;
}

extern "C" void* ControlUIConfig_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    Init(mem);
    return mem;
}

REGISTER_TYPE(ControlUIConfig, "UI::DialogControllerConfig",
              &ControlUIConfig_Factory, &ControlUIConfig_CreateInPlace);
