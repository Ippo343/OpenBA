// ============================================================================
// runtime/src/ui/dialog_factory.cpp — WAVE 11 LANE OBJINT, THE CREATION-PATH PROBE
//
//   ?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ        retail 0x180038530
//   ?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z retail 0x180038610
// ============================================================================
//
// WHY THIS FILE EXISTS. It is not here to bank a row. `WAVE10E_LANE_FINAL.md`
// left `??_7Dialog@UI@@6B@` unpromotable with a written EXPIRY:
//
//   > The row becomes promotable and required the moment
//   > ?CreateNewObject@Dialog@UI@@ (0x180038530) or
//   > ?CreateInPlaceObject@Dialog@UI@@ (0x180038610) is ported and registered.
//   > From that point every resource-loaded UI::Dialog carries whatever
//   > ResolveExport("??_7Dialog@UI@@6B@") returns.
//
// `WAVE11_KICKOFF.md` repeated it as "mechanical and already written down", and
// the lane brief repeated it again. THIS FILE IS THE EXPERIMENT THAT TESTS IT,
// and the prediction it was written to test is that the expiry is WRONG — that
// porting and registering the factory is NECESSARY BUT NOT SUFFICIENT, for a
// reason that is visible in the binary before the run.
//
// ----------------------------------------------------------------------------
// THE STATIC FACT THAT MOTIVATES THE PREDICTION
// ----------------------------------------------------------------------------
// `?CreateNewObject@Dialog@UI@@` and `?CreateInPlaceObject@Dialog@UI@@` have
// **ZERO CODE XREFS** in Runtime-x64.dll. Every reference to either is DATA:
//
//   0x1801faaa8  ?_typeInfo@Dialog@UI@@ + 0x20   (factoryFn)
//   0x1801faab0  ?_typeInfo@Dialog@UI@@ + 0x28   (inPlaceFactoryFn)
//   0x1801b2a44 / 0x1801b282c   .pdata unwind
//   0x18018e09c / 0x18018dfe0   relocations
//   Entry Point [EXTERNAL]      the export table
//
// So the ONE route into either body is an indirect call through a function
// pointer sitting in the DLL's own `.data`, put there by the DLL's static
// initialiser with an in-module `LEA`. That is the same class of thing as a
// `rel32` and an in-DLL vtable stamp: **it is not a door.** Not the import
// table, not the port registry, not a detour. `RegisterRuntimeExport` on the
// mangled name cannot reach it, and neither can `RegisterRuntimeAddr`.
//
// The reader is generic and our tree already owns a copy of it:
// `?CreateObject@Factory@Object@@` (runtime/src/object/type_system.cpp:352)
// ends in `info->factoryFn()`, and `?AddController@DialogInstance@UI@@`
// (runtime/src/ui/dialog_create.cpp:551) does the same by hand. Both are OUR
// code, so both ARE doors — but only for whatever `BaseTypeInfo*` they happen
// to be holding, and that node comes from the AVL that `Factory::Startup`
// builds by walking `?_firstTypeInfo@BaseTypeInfo@Object@@`, which
// runtime/src/object/type_system.cpp:79 resolves BY NAME. By name means the
// DLL's chain head, so every node in that tree is a DLL node, and every
// `factoryFn` in it is the DLL's raw address.
//
// Three conditions therefore have to hold for our factory to run, and the
// published expiry names only the first:
//
//   1. our body exists and is registered              <- this file
//   2. something calls through a BaseTypeInfo whose factoryFn is OUR pointer
//      -> needs the MINTED ?_typeInfo@Dialog@UI@@ row, not the DLL's node
//   3. the chain/AVL the creator walks contains that minted node
//      -> blocked by the `_firstTypeInfo` split brain (WAVE11.md, class A)
//
// ----------------------------------------------------------------------------
// HOW THE PREDICTION IS MEASURED RATHER THAN ARGUED
// ----------------------------------------------------------------------------
// An argument from xrefs is still an argument. Two counters turn it into a
// reading, and they are deliberately INDEPENDENT of each other:
//
//   [DlgFactory] attendance   incremented at the top of each of our two
//                             bodies. It answers "did anything ever call the
//                             port?" — the attendance counter this project has
//                             twice had to add after mistaking SAFE-to-stamp
//                             for a route (reference_a_green_readiness_screen_
//                             is_not_a_route.md).
//
// ----------------------------------------------------------------------------
// ARMING — NONE. BOTH BODIES REGISTER UNCONDITIONALLY. (corrected 2026-09-12)
// ----------------------------------------------------------------------------
// This file used to be opt-in, on the argument below that nothing could reach
// either body anyway. The cutover boot disproved the premise — the MINTED
// `?_typeInfo@Dialog@UI@@` reaches it and died on the unported-factory trap —
// so the flag and its control are gone. See DIRECTION.md section 2.3.
//
// ----------------------------------------------------------------------------
// THE ABORT-TRAP RISK, ENUMERATED RATHER THAN FOOTNOTED
// ----------------------------------------------------------------------------
// The brief asks which of the 104 minted `BaseTypeInfo` factory abort traps
// this probe could reach. The answer is **NONE, and the reason is structural**:
// the traps (`RtMintTiTrapStub_*` in standalone/rt_data_mint_tables.inc) are
// only ever loaded into a minted `BaseTypeInfo` row, and a minted row is only
// installed by `reg_mint_promoted.cpp`. This file
// registers two FUNCTION symbols and mints no data, promotes no row, and does
// not touch `_firstTypeInfo`. Row 24's own two traps
// (`RtMintTiTrapStub_24_0/_24_1`, rt_data_mint_tables.inc:1816-1817) are the
// only ones this subject could ever be adjacent to, and arming this file makes
// them neither more nor less reachable than they were.
//
// ----------------------------------------------------------------------------
// THE ALLOCATOR IS ON THIS PATH, SAID EXPLICITLY
// ----------------------------------------------------------------------------
// `CreateNewObject` allocates: retail inlines `System::MemAlloc(0x88, 8)`
// (0x1800c7a50) — lock `_singleton@MemoryAllocator + 0x640`, call
// `?Allocate@MemoryAllocator@System@@` (0x1800c6e60), unlock. This port reaches
// it through `ResolveAddr(Module::Runtime, 0x1800c7a50)`, which consults the
// port registry FIRST, so our allocator answers. One heap is used either way.
//
// `CreateInPlaceObject` allocates NOTHING — it is placement construction on a
// caller-owned buffer — so the in-place half of the probe is allocator-free.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Dialog.h"

// ============================================================================
// ENDGAME B.3.9 block UI-D (B39_RULES D2): the pair is the class's OWN statics,
// `UI::Dialog::CreateNewObject` / `CreateInPlaceObject`, building a real
// `UI::Dialog` with `::new`. The attendance report (`[DlgFactory]`) goes with
// the two doors it counted.
// ============================================================================
namespace UI {

// ?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z   0x180038610
// Retail's body is the `??0Dialog@UI@@` constructor INLINED (0x180038626..
// 0x180038696, the same stores, the +0x44 hole). THE NULL CHECK IS RETAIL'S:
// 0x180038621 `TEST RCX,RCX / JZ 0x18003869b`, the null branch returns 0.
Object::Base* Dialog::CreateInPlaceObject(void* p) {
    if (p == nullptr) return nullptr;      // 180038621 / 18003869b
    return ::new (p) Dialog();             // 180038696 MOV RAX,RCX
}

// ?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ           0x180038530
// The inlined `MemAlloc(0x88, 8)` (0x180038561..0x180038571: `MOV EDX,0x88 ;
// LEA R8D,[RDX-0x80]`), then the inlined ctor. The null check at 0x18003858a
// is retail's and covers an allocation failure.
Object::Base* Dialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Dialog), 8);
    if (p == nullptr) return nullptr;      // 18003858a / 180038601
    return ::new (p) Dialog();             // 1800385fc MOV RAX,RDI
}

}  // namespace UI

int RtUiDialogFactoryRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateInPlaceObject@Dialog@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::Dialog::CreateInPlaceObject);
    RegisterRuntimeAddr(0x180038610, (void*)&UI::Dialog::CreateInPlaceObject);  ++n;

    RegisterRuntimeExport("?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::Dialog::CreateNewObject);
    RegisterRuntimeAddr(0x180038530, (void*)&UI::Dialog::CreateNewObject);      ++n;
    return n;
}
