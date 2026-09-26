// code/Game/Component/healercomponent.cpp
// Clean reimplementation of HealerComponent lifecycle methods from Application-x64.exe.
//
// HealerComponent (0x48 bytes) extends ControllerComponent with no additional fields.
// Constructor sets flags |= 0x2.
//
// Vtable at 0x140159730:
//   [3] Shutdown  = 0x14004aeb0 -- two empty serializer blocks
//   [7] OnCreate  = 0x14004ae70 -- registers with healer list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/healercomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// FUN_140002cc0 -- ERASE-by-value from an InternalBasePtr array {ptr@+0, count@+8}.
// (The legacy name here said "push"; the body at 0x140002cc0 scans for a matching
// resolved pointer, shifts the tail down and decrements the count -- see the 1:1 port
// in cemeterycomponent.cpp CemeteryListErase_Reimpl. It is the UNregister.)
typedef void (*fn_ArrayEraseBasePtr_t)(void* arrayBase, void* internalBasePtr);
static fn_ArrayEraseBasePtr_t s_fn_ArrayEraseBasePtr = nullptr;

// FUN_140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.

// Global: DAT_1401c3dd8 -- pointer to game world/population manager
static void** s_pWorldManager = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_fn_ArrayEraseBasePtr = reinterpret_cast<fn_ArrayEraseBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_pWorldManager = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_initialized = true;
}

// ============================================================
// HealerComponent::Shutdown (vtable slot [3], 0x14004aeb0)
// ============================================================
// Same pattern as WellComponent: Component_Serialize + two empty serializer blocks.

void HealerComponent_Shutdown(HealerComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Two empty serializer blocks (component section + controller section)
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    alignas(8) uint8_t serializer2[24];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// HealerComponent::OnRemove (vtable slot [7], 0x14004ae70)
// ============================================================
// UNregisters from the TownManager healer list at *(GameWorld+0xF0)+0x70.
//
// Ghidra labels this slot OnCreate; the retail body is an ERASE, and the matching
// REGISTER is the state-2 enter at the bottom of this file -- the same OnCreate /
// OnRemove off-by-one every ControllerComponent in this tree carries.
//
// Two fixes vs. the first port, both read straight off 0x14004ae70:
//   * retail wraps `self` (the COMPONENT: `mov rdx,rcx`), not `self->owner`. An IBP
//     built from the owner entity identifies the wrong object, so the erase scan
//     could never match -- exactly the bug WellComponent::OnRemove carried until the
//     fire lane went live.
//   * 0x140002cc0 is the erase, not a push. It was being called as if it appended.
// Latent until now: slot 7 was never installed while the component was inert.

void HealerComponent_OnRemove(HealerComponent* self) {
    EnsureHelpers();

    uint8_t basePtr[24] = {};
    if (fn_InternalBasePtr_Ctor) {
        new (basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    }

    if (s_fn_ArrayEraseBasePtr && s_pWorldManager && *s_pWorldManager) {
        void* gameWorld = *s_pWorldManager;
        uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(gameWorld) + 0xF0);
        if (townMgr)
            s_fn_ArrayEraseBasePtr(reinterpret_cast<void*>(townMgr + 0x70), basePtr);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HealerComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HealerComponent] Funcs resolved: Serialize=%p Erase=%p WorldMgr=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ArrayEraseBasePtr,
           (void*)s_pWorldManager);
#endif
}

// ============================================================
// HealerComponent state-2 "built" enter -- 0x14004ae30
// ============================================================
// REGISTERS the hospital in the TownManager healer list:
//     InternalBasePtr::Ctor(&ibp, self)            // the COMPONENT, not the owner
//     town = *(*0x1401c3dd8 + 0xf0)
//     ArrayPush(town + 0x70, town[0x78] + 1, &ibp)
// Retail returns 0 in AL so the state machine's Enter walk continues past this node.
//
// This is the whole reason a built hospital was invisible to the town: with the
// component inert nothing ever entered state 2, so the healer array at townMgr+0x70
// stayed empty for the life of the save and the sick had nowhere to be sent.
extern "C" char HealerComponent_EnterBuilt(void* comp, void* /*sm*/) {
    HealerComponent_InitFuncs();
    if (!fn_InternalBasePtr_Ctor ||
        !s_pWorldManager || !*s_pWorldManager) return 0;

    uint8_t ibp[24] = {};
    new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge

    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*s_pWorldManager) + 0xF0);
    if (!townMgr) return 0;

    const int count = *reinterpret_cast<int*>(townMgr + 0x78);
    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(townMgr + 0x70))
        .SetSizeFill(count + 1, ibp);

    {
        static int n = 0;
        if (n < 8) { ++n;
            fprintf(stderr, "[Healer] hospital REGISTERED: comp=%p townMgr=%p "
                            "healerCount=%d\n", comp, reinterpret_cast<void*>(townMgr),
                    *reinterpret_cast<int*>(townMgr + 0x78));
            fflush(stderr);
        }
    }
    return 0;
}

// HealerComponent state-3 "demolish" enter -- 0x14004adf0. Byte-identical to the
// slot-7 removal hook (0x14004ae70): wrap the component, call the erase.
extern "C" char HealerComponent_EnterDemolish(void* comp, void* /*sm*/) {
    HealerComponent_OnRemove(reinterpret_cast<HealerComponent*>(comp));
    return 0;
}

// ============================================================================
// ENDGAME B.3.8 MI-A — HealerComponent as a REAL class (HealerComponent.h).
//
// Before: a 48-slot hand-built `HealerComponent_LiveVtable` filled at every
// create (FillTypedVtable + FinishLiveVtable), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<HealerCtrlTag> table stamped into raw bytes at +0x28.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/HealerComponent.h"
#include "Component/controller_state_table.h"

using HealerCtrlDispatch = ::ControllerLive::Dispatcher<HealerCtrlTag>;

// The retail table, transcribed from 0x14004b020 call for call. Its RegisterState
// instantiation (0x1400066e0) zero-fills the trans30/trans38/update columns, so every
// handler but Enter is null -- verified in the callee, not assumed from the call site.
// Built ONCE at static-init time, as retail's static initializer does (B38 D6/D7).
static void HealerComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = HealerCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&HealerComponent_EnterBuilt),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 13, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&HealerComponent_EnterDemolish),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}
static const bool s_healerStateTableBuilt = (HealerComponent_PopulateStateTable(), true);

// 0x14004af70. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; then `_flags |= 2` (0x82).
HealerComponent::HealerComponent() : _controller(this) {
    OrFlagsInCtor(0x2);
}

// [0] 0x140006660: ~HealerComponent (0x14004af10), MemFree on &1.
Object::Base* HealerComponent::DeletingDtor(unsigned flags) {
    this->~HealerComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x14004af10: restamp + ~Internal, no own work.
HealerComponent::~HealerComponent() = default;

// create 0x140006490: MemAlloc(0x48, 8), then the ctor.
void* HealerComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HealerComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HealerComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* HealerComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HealerComponent());
}

// [3] 0x14004aeb0. Slot 3 is the first call into the class on a load; the body
// resolves its own helpers.
void HealerComponent::Persist(void* stream) { HealerComponent_Shutdown(this, stream); }

// [7] 0x14004ae70 — leave the town healer registry.
void HealerComponent::OnRemove() { HealerComponent_OnRemove(this); }

DEFINE_EXE_VTABLE(HealerComponent, "_ZTV15HealerComponent")
DEFINE_EXE_TYPE(HealerComponent, "ControllerComponent")
