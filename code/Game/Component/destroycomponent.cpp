// code/Game/Component/destroycomponent.cpp
// Clean reimplementation of DestroyComponent lifecycle methods from Application-x64.exe.
//
// DestroyComponent (0x68 bytes) -- ControllerComponent subclass.
// Manages building demolition/destruction process.
// Fields: field48 (Time), destroyFlags (uint16 +0x50), field51 (bool),
//         field54 (uint32), field58 (ptr), field60 (entity ref / mesh ID).
//
// Vtable at 0x140163120. Two non-trivial overrides:
//   Shutdown  (0x14011DB50) -- serialize all destroy state fields
//   Serialize (0x14011DB00) -- OnCreate: copy mesh ref from building data
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/DestroyComponent.h"
#include "Component/DestroyDescription.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================================
// ENDGAME B.3.8 MI-A — DestroyComponent as a REAL class (DestroyComponent.h).
//
// Before: a 48-slot hand-built `DestroyComponent_LiveVtable` filled at every
// create (FillTypedVtable + FinishLiveVtable), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<DestroyCtrlTag> table stamped into raw bytes at +0x28.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/DestroyComponent.h"

// The upgrade/demolish controller: the entity's StateMachineComponent links the
// member and the 0x14011dfa0 pump (UpgradeUI click) drives states 3->4->5
// (demolish -> haul-away -> refund + stone-house spawn), destroycomponent_states.cpp.
// Retail slot 16 returns 4 (0x140033c00 `MOV EAX,4`). That is only safe under
// retail's first-controller consult order, where the build controller precedes
// destroy in every building template; the standalone linker's destroyOutOfOrder
// rule (statemachinecomponent.cpp) keeps it that way. The state table is built
// ONCE at static-init time, as retail's static initializer does (B38 D6/D7).
extern "C" void DestroyCtrl_PopulateTable();
static const bool s_destroyStateTableBuilt = (DestroyCtrl_PopulateTable(), true);

// 0x14011dc80. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48 = 0, +0x50 = 0 (word),
// +0x54 = 0 (qword: +0x54 and the low half of +0x58), +0x60 = 0. No flag OR.
DestroyComponent::DestroyComponent()
    : _controller(this), field48(nullptr), destroyFlags(0), _pad52{}, field54(0),
      field58(nullptr), field60(nullptr) {}

// 0x14011dc20: restamp + ~Internal, no own work.
DestroyComponent::~DestroyComponent() = default;

// create 0x140033bc0: MemAlloc(0x68, 8), then the ctor.
void* DestroyComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(DestroyComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) DestroyComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* DestroyComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) DestroyComponent());
}

// [3] 0x14011db50 — save serialization: Component::Persist (a direct CALL
// 0x1401189b0), an empty controller scope, then one scope carrying field54 (int),
// +0x58 (int), +0x51 (bool), the +0x60 ExternalPtr (0x140002340), destroyFlags
// (+0x50, bool) and the Time at +0x48.
void DestroyComponent::Persist(void* stream) {
    Component::Persist(stream);
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge (stream is void* in Internal::Persist)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);         // B.3.8 bridge
    ser.Serialize(*reinterpret_cast<int*>(&field54), nullptr);
    ser.Serialize(*reinterpret_cast<int*>(&field58), nullptr);
    ser.Serialize(*reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(&destroyFlags) + 1), nullptr);   // +0x51
    if (fn_ExternalPtr_BuildSerialize)
        fn_ExternalPtr_BuildSerialize(&field60, &ser);
    ser.Serialize(*reinterpret_cast<bool*>(&destroyFlags), nullptr);
    reinterpret_cast<System::Time*>(&field48)->Serialize(ser);   // B.3.8 bridge (field48 is a Time)
}

// [4] 0x14011db00 — copies the DestroyDescription's default target (desc+0x20)
// into +0x60; dispatched at spawn by ComponentEntity_StartupComponents.
void DestroyComponent::Startup() {
    void* desc = _description;
    if (!fn_IsDerivedFrom) return;
    if (fn_IsDerivedFrom(desc, DestroyDescription::TypeId()))
        field60 = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(desc) + 0x20);
}

// [15] 0x14011daf0: `if (msgCode == 0) ++*(int*)(self + 0x54);` — the demolition-
// progress credit, destroy-side twin of BuildComponent::OnMessage. The demolition
// action chain (BuildStructure_SetupBuildActions, altAnim=1) pushes a
// SendComponentMessage carrying DestroyComponent's type id; each completed
// PerformWork routes here and advances +0x54 toward the +0x58 target that gates the
// state-5 tick.
void DestroyComponent::OnMessage(int message, void* /*data*/, uint32_t /*arg0*/, uint32_t /*arg1*/) {
    if (message == 0) ++field54;
}

DEFINE_EXE_TYPE(DestroyComponent, "ControllerComponent")
