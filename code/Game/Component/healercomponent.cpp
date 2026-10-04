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
#include "object/TypeInfo.h"
#include "Component/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Entity/CitizenBehavior.h"

// FUN_140002cc0 -- ERASE-by-value from an InternalBasePtr array {ptr@+0, count@+8}.
// (The legacy name said "push"; the body at 0x140002cc0 scans for a matching
// resolved pointer, shifts the tail down and decrements the count -- the 1:1 port
// is cemeterycomponent.cpp's CemeteryListErase_Reimpl. It is the UNregister.)
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue);   // 0x140002cc0

// FUN_140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.
// DAT_1401c3dd8 (the game world) is engine_funcs' g_pGameWorldSingleton.

// ============================================================
// HealerComponent::OnRemove (vtable slot [7], 0x14004ae70) — the member below
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
// GameUpdateLoop::m_citizenList (+0xF0): the town manager, the CitizenBehavior, whose
// healer list is read by name.
static CitizenBehavior* TownManager() {
    return GameUpdateLoop::_singleton->m_citizenList;
}

char HealerComponent::StateBuiltEnter() {
    if (GameUpdateLoop::_singleton == nullptr) return 0;

    ::Object::InternalPtr<HealerComponent> self(this);

    CitizenBehavior* town = TownManager();
    if (!town) return 0;

    const int count = town->healerCount;
    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(&town->healers).SetSizeFill(count + 1, &self);

    {
        static int n = 0;
        if (n < 8) { ++n;
            fprintf(stderr, "[Healer] hospital REGISTERED: comp=%p townMgr=%p "
                            "healerCount=%d\n", static_cast<void*>(this), static_cast<void*>(town),
                    town->healerCount);
            fflush(stderr);
        }
    }
    return 0;
}

// HealerComponent state-3 "demolish" enter -- 0x14004adf0. Byte-identical to the
// slot-7 removal hook (0x14004ae70): wrap the component, call the erase.
char HealerComponent::StateDemolishEnter() {
    HealerComponent::OnRemove();
    return 0;
}

// The state table's handler adapters: the controller hands each one its owner
// (node+0x18), the HealerComponent.
static char Healer_StateBuiltEnter(HealerComponent* c, void* /*sm*/)    { return c->StateBuiltEnter(); }      // 0x14004ae30
static char Healer_StateDemolishEnter(HealerComponent* c, void* /*sm*/) { return c->StateDemolishEnter(); }   // 0x14004adf0

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
    auto& t = HealerCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 2, &Healer_StateBuiltEnter,
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 13, nullptr);
    TableRegisterState(t, 3, &Healer_StateDemolishEnter,
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

// [3] 0x14004aeb0 — EMPTY: Component::Persist (a direct CALL 0x1401189b0) plus two
// empty Serializer scopes (component section + controller section), the same
// pattern as WellComponent.
void HealerComponent::Persist(IO::Stream& stream) {
    Component::Persist(stream);
    { IO::Serializer scope(stream, 0); }
    { IO::Serializer scope(stream, 0); }
}

// [7] 0x14004ae70 — leave the town healer registry: wrap THIS component in an
// InternalBasePtr and erase it from *(GameWorld+0xF0)+0x70.
void HealerComponent::OnRemove() {
    ::Object::InternalPtr<HealerComponent> self(this);
    if (GameUpdateLoop::_singleton != nullptr) {
        CitizenBehavior* town = TownManager();
        if (town)
            CemeteryListErase_Reimpl(&town->healers, &self);   // 0x140002cc0
    }
}

// Doors kept for their by-address rows (B.3.8): standalone/reg_cemetery_healer_serialize.cpp
// (0x14004aeb0) and standalone/reg_town_services.cpp (0x14004ae70).

DEFINE_EXE_TYPE(HealerComponent, "ControllerComponent")
