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
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

#include "object/Internal.h"
#include "Dialog/ResourceLimitUIConfig.h"

// ============================================================================
// ENDGAME B.3.8 PNL — ResourceLimitUIConfig as a REAL class (ResourceLimitUIConfig.h). The hand-built
// 9-slot table, its Init/Factory/CreateInPlace entry points and the lazy
// GetTypeId cache are gone; the compiler emits the table.
// ============================================================================

// [0] 0x140013880: ~ResourceLimitUIConfig (restamp + ~DialogControllerConfig + ~External, all
// the compiler's), MemFree on &1.
Object::Base* ResourceLimitUIConfig::DeletingDtor(unsigned flags) {
    this->~ResourceLimitUIConfig();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [4] The tree's table answered 0 here; retail inherits External::Compile.
// Kept as the tree had it until the titled behaviour commit.
Object::External::CompileResult ResourceLimitUIConfig::Compile(const System::Filename& /*path*/) {
    return 0;
}

// [5] 0x140077b10 — read this row's resource index.
bool ResourceLimitUIConfig::Serialize(void* stream) {
    IO::Serializer serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    serializer.Serialize(_resourceIndex, nullptr);
    return true;
}

// [6] 0x140013830 — destroy in place, construct again.
Object::External* ResourceLimitUIConfig::Reconstruct() {
    this->~ResourceLimitUIConfig();
    return new (this) ResourceLimitUIConfig();
}

// [7] PORT GAP (retail 0x140001640 = DeletingDtor(0)) — the tree's table held
// `return 0` here; kept until the titled behaviour commit. B38 D9.
void ResourceLimitUIConfig::Destruct() {}

// [8] 0x1400137e0 — placement construction (MSVC folded it with createInPlace).
Object::External* ResourceLimitUIConfig::Construct() { return new (this) ResourceLimitUIConfig(); }

// create 0x140013780: MemAlloc(0x28, 8), then the ctor.
void* ResourceLimitUIConfig::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ResourceLimitUIConfig), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ResourceLimitUIConfig());
}
// createInPlace 0x1400137e0.
void* ResourceLimitUIConfig::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ResourceLimitUIConfig());
}

DEFINE_EXE_VTABLE(ResourceLimitUIConfig, "_ZTV21ResourceLimitUIConfig")

// D5: no by-address reader of 0x14015d250 remains in the tree; the row is kept
// (serving the compiler table) only because reg_controlui.cpp calls this function —
// the closing pass deletes both.
extern "C" void RegisterResourceLimitUIConfigVtable() {
    RegisterExeAddr(0x14015d250, reinterpret_cast<uintptr_t>(ResourceLimitUIConfig::Vtable()));
}

DEFINE_EXE_TYPE(ResourceLimitUIConfig, "UI::DialogControllerConfig")
