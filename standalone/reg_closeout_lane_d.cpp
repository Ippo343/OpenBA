// standalone/reg_closeout_lane_d.cpp
// Exe close-out, lane D — residual-gap triage (docs/waves/exe_closeout/lane_d.md).
//
// Every registration in this file is argued in that document. Nothing else in the
// exe close-out may be registered here; other lanes own their own reg_*.cpp files.
//
//   0x14001fdb0  Struct60B sub-array deep copy, stride 0x14  (PLAN 1.2 #15)
//   0x14001fe60  Struct60B sub-array deep copy, stride 0x02  (PLAN 1.2 #15)
//
// Both are leaves of Struct60B's copy ctor (code/Game/Game/serialize_struct60b.cpp).
// They were the two rows in the "14 unregistered .text addresses" list that our own
// engine_funcs actually resolves and then never gets (fn_Struct60B_SubArrayCopyStride14
// / _Stride02, engine_funcs.cpp:4947,4949 — null since the day they were written, so
// only the inline fallback in the copy ctor ever ran). Registering them binds those two
// pointers to the SAME body the fallback uses, so no code path changes; what changes is
// that the pointers stop lying and the census rows close. Retail returns dst in RAX and
// both wrappers do the same.
//
//   0x140128f50  PickingComponent::OnAdded — the pick-mesh REFRESH (PLAN 1.1)
//
// The census (BANISHED_UNREG_CENSUS=1, 2026-09-06) shows exactly 3 boot-time requests
// for 0x140128f50, one per consumer: buildcomponent.cpp:344, buildcomponent_state8.cpp:127
// and destroycomponent_state6.cpp:128. All three null-guard and SKIP, and the third even
// logs "pick-mesh rebuild SKIPPED -- 0x140128f50 NOT registered". So the pickable volume
// never followed the model's construction/collapse frames: it stayed pinned to whichever
// sub-mesh was current at OnAddedPass time. The port has existed all along
// (PickingComponent_RegisterMesh, pickingcomponent.cpp:167) and is a faithful refresh --
// it opens with the same OnCreate teardown retail's `(**(code**)(*param_1 + 0x38))()`
// does, so the `pickingId < 0` guard that follows is always satisfied.
//
// Retail's param (0x140128f50 decompile) is the PickingComponent OBJECT: it reads the
// vtable at *param_1, the owner IBP at param_1+2, the pick description at param_1[3],
// the id at param_1[5] and the userData at param_1[6]. Two of the three consumers passed
// exactly that; buildcomponent_state8.cpp passed `entSlot`, the ADDRESS of the
// InternalBasePtr, at four call sites -- one indirection too many, which would have made
// the object pointer be read as a vtable on the very first refresh. That is why this
// address could not simply be registered: it was dead code hiding a wrong argument.
// Fixed in that file (deref) as part of this registration.
//
// Deliberately NOT registered here (see lane_d.md for the evidence per address):
//   0x140002280  Entity::FindComponent<AmbientEmitterComponent> — measured leave
//   0x1400938d0  Weather::DrawUpdate            — double-write harm
//   0x14009e8c0  Game::BuildPathfindingSystems  — double-build harm
//   0x1400f2f00  Pickup::Execute — registering it makes the dispatcher call ITSELF
//   0x1400fcc90  EditTool deselect — would wipe the selection outline every frame
//   0x140101ee0  EntityUI::Startup — double-inserts the 8 tool-mode AVL nodes
//   0x1401026a0  EntityUI::Update  — double-drives camera + tool every frame
#include "resolve.h"
#include <cstdint>

extern "C" void* Reimpl_Struct60B_SubArrayCopyStride14(void* dst, const void* src);
extern "C" void* Reimpl_Struct60B_SubArrayCopyStride02(void* dst, const void* src);
void PickingComponent_RegisterMesh(void* pc);   // pickingcomponent_funcs.h

void RegisterCloseoutLaneDBatch() {
    RegisterExeAddr(0x14001fdb0,
                    reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_SubArrayCopyStride14));
    RegisterExeAddr(0x14001fe60,
                    reinterpret_cast<uintptr_t>(&Reimpl_Struct60B_SubArrayCopyStride02));
    RegisterExeAddr(0x140128f50,
                    reinterpret_cast<uintptr_t>(&PickingComponent_RegisterMesh));
}

// Self-registering, so this file needs no edit to standalone/address_registry.cpp
// (three other close-out lanes are editing that file in parallel). The registry is a
// function-local static std::unordered_map in resolve_standalone.cpp and is never
// cleared, so a pre-main insert is safe and survives AddressRegistryInit; the same
// static-initializer pattern REGISTER_TYPE already uses. Both consumers of these two
// addresses read them from EngineFuncsInit, which runs from WinMain — long after this.
namespace {
struct CloseoutLaneDAutoRegister {
    CloseoutLaneDAutoRegister() { RegisterCloseoutLaneDBatch(); }
};
const CloseoutLaneDAutoRegister g_closeoutLaneDAutoRegister;
}  // namespace
