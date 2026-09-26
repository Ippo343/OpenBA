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

#include "Component/destroycomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// FUN_140002340 -- serialize InternalBasePtr (field60)
typedef void (*fn_SerializeInternalBasePtr_t)(void* ptrField, void* serializer);
static fn_SerializeInternalBasePtr_t s_fn_SerializeInternalBasePtr = nullptr;

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// Type ID for building component type (DAT_1401c7628)
static uint32_t* s_pBuildingCompTypeId = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_SerializeInternalBasePtr = reinterpret_cast<fn_SerializeInternalBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    s_pBuildingCompTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7628));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// DestroyComponent::Shutdown (vtable slot [3], 0x14011DB50)
// ============================================================
// Full serialization of destroy state:
//   1. Component_Serialize
//   2. Empty serializer block (controller compat)
//   3. Serializer(version=0): field54 (int), field58 (int),
//      field51 (bool), field60 (InternalBasePtr via FUN_140002340),
//      destroyFlags at +0x50 (bool), field48 (Time)

void DestroyComponent_Shutdown(DestroyComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (controller format compat)
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Serialize destroy state fields
    alignas(8) uint8_t serializer2[24];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>(&self->field54), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>((uintptr_t)self + 0x58), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<bool*>((uintptr_t)self + 0x51), nullptr);   // B.3.4 bridge

    if (s_fn_SerializeInternalBasePtr) {
        s_fn_SerializeInternalBasePtr((void*)((uintptr_t)self + 0x60), serializer2);
    }

    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<bool*>(&self->destroyFlags), nullptr);   // B.3.4 bridge

    reinterpret_cast<System::Time*>((void*)&self->field48)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4b bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// DestroyComponent::Serialize (vtable slot [4], 0x14011DB00)
// ============================================================
// Copies mesh reference from building data. Reads _unk18,
// does IsDerivedFrom check, copies 8 bytes from building+0x20
// into field60.

void DestroyComponent_Serialize(DestroyComponent* self) {
    EnsureHelpers();

    void* baseObj = self->_description;
    if (!s_fn_IsDerivedFrom_d || !s_pBuildingCompTypeId) return;

    bool isDerived = s_fn_IsDerivedFrom_d(baseObj, *s_pBuildingCompTypeId);
    void* buildingData = isDerived ? baseObj : nullptr;

    if (buildingData) {
        // Copy mesh reference from building data +0x20 into field60
        *(uint64_t*)&self->field60 = *(uint64_t*)((uintptr_t)buildingData + 0x20);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void DestroyComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[DestroyComponent] Funcs resolved: Serialize=%p IsDerivedFrom=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IsDerivedFrom_d);
#endif
}

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

// [0] 0x140033dd0: ~DestroyComponent (0x14011dc20), MemFree on &1.
Object::Base* DestroyComponent::DeletingDtor(unsigned flags) {
    this->~DestroyComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
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

// [3] 0x14011db50 — save serialization.
void DestroyComponent::Persist(void* stream) { DestroyComponent_Shutdown(this, stream); }

// [4] 0x14011db00 — copies the DestroyDescription's default target (desc+0x20)
// into +0x60; dispatched at spawn by ComponentEntity_StartupComponents.
void DestroyComponent::Startup() { DestroyComponent_Serialize(this); }

// [15] 0x14011daf0: `if (msgCode == 0) ++*(int*)(self + 0x54);` — the demolition-
// progress credit, destroy-side twin of BuildComponent::OnMessage. The demolition
// action chain (BuildStructure_SetupBuildActions, altAnim=1) pushes a
// SendComponentMessage carrying DestroyComponent's type id; each completed
// PerformWork routes here and advances +0x54 toward the +0x58 target that gates the
// state-5 tick.
void DestroyComponent::OnMessage(int message, void* /*data*/, uint32_t /*arg0*/, uint32_t /*arg1*/) {
    if (message == 0) ++field54;
}

DEFINE_EXE_VTABLE(DestroyComponent, "_ZTV16DestroyComponent")
DEFINE_EXE_TYPE(DestroyComponent, "ControllerComponent")
