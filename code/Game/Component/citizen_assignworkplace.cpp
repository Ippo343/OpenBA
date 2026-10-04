// code/Game/Component/citizen_assignworkplace.cpp
// Phase-4 economy: faithful 1:1 port of CitizenComponent::AssignWorkplace (SetHome), so
// standalone mode can set a citizen's home (+0x238 / +0x240) and keep the residence's
// family roster in sync.
//
//   0x1401163d0  CitizenComponent_AssignWorkplace  (set +0x238/+0x240; re-roster; recurse family)
//   (the roster: ResidenceComponent::RemoveResident 0x14012a550 / AddResident 0x14012a710 /
//    CountResidents 0x140129ff0, residencecomponent.cpp)
//
// DORMANT: no caller in code/Game yet (the lead wires it in a later brick). It must be
// crash-safe and byte-faithful now. The roster bookkeeping (find/compact/decrement in
// a550; grow-array append + qsort in a710; the count sum in 129ff0) is the FAITHFUL CORE
// and runs unconditionally. Every rendering/audio side effect is INERT — in standalone
// the Audio::Scene (*(gameWorld+0x78)) is often null and FindComponent can return a
// null-wrapped IBP — so those branches are guarded to no-op crash-safe, mirroring the
// established inert-branch pattern (code/Game/Component/citizencomponent.cpp roster read,
// reference_citizencomponent_field_map).
//
// Verified against Application-x64.exe disassembly (Ghidra):
//   AssignWorkplace ABI  RCX=self, RDX=workplaceIBP, R8D=slotIndex (0x1401163e2 MOV EBP,R8D)
//   a550/a710 ABI        RCX=storageComp, EDX=slotIndex, R8=selfIBP
//   Roster table         storage+0x50 -> array of {int64 data; int32 count; int32 cap} (stride 0x10)
//                        storage+0x58 = profession count (129ff0 loop bound)
//   Worker element       raw 8-byte component pointer (find stride 8; compact stride 8)
//   Citizen fields       +0x238 workplace IBP, +0x240 slotIndex, +0x260 family head,
//                        +0x268 family next, +0x258 (=600) parent/guardian
//   Dispatch slots       0x140152ab0 CheckPtr, 0x140152a90 IBP::ctor (via engine globals),
//                        0x140152a70 Vector3::operator==, 0x140152a00 MoveEmitter,
//                        0x140152a08 SetEmitterVolume, 0x140152a10 StopEffect3D,
//                        0x140152a18 GetEmitterCount, 0x140152a20 RemoveEmitter
//

#include <new>
#include "math/Math.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>   // qsort
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "audio/Audio.h"       // ENDGAME B.3.7 callers (K4)
#include "Component/exe_bodies.h"
#include "Component/CitizenComponent.h"     // B.4 W1 COMP-b: SetHome is a member
#include "Component/ResidenceComponent.h"
#include "Entity/ComponentEntity.h"          // FindComponent<T>
#include "Component/firecomponent_funcs.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_constants.h"

// The residence roster helpers this file used to hold (0x14012a550 / 0x14012a710 /
// 0x140129ff0 / the 0x140129e20 comparator) are ResidenceComponent members now
// (residencecomponent.cpp): their receiver is the ResidenceComponent.

// Forward decl (extern "C" so the standalone registry can take its address).
extern "C" void CitizenComponent_AssignWorkplace(void* self, void* workplaceIBP, int slotIndex);

// ============================================================
// CitizenComponent::SetHome (0x1401163d0; tree and Ghidra: "AssignWorkplace")
//   home = the new home entity (the residence), slot = the household slot in it.
// If the assignment actually changes (+0x238 or +0x240 differ): remove from the old
// residence's household roster, store the new home + slot, add to the new roster, then
// apply the same assignment to every child on the +0x260/+0x268 circular ring.
// The rosters are ResidenceComponent's (RemoveResident 0x14012a550 / AddResident 0x14012a710).
// ============================================================
void CitizenComponent::SetHome(ComponentEntity* newHome, int32_t slotIndex) {
    void* selfV = this;
    ComponentEntity** pWorkplace = &home;  // current home
    int32_t* pSlot = &homeSlot;
    ComponentEntity* newWp = newHome;

    if (*pWorkplace == newWp && *pSlot == slotIndex)
        return;   // no change

    // [diag] A citizen losing its home is how "everyone froze after a load"
    // starts: +0x238 goes null, the AI has nothing to plan, and the town starves.
    // Only the UN-assignment is interesting, and only its CALLER identifies which
    // rule decided the home was invalid -- so report the return address and let nm
    // name it (image base 0x140000000).
    if (*pWorkplace != nullptr && newWp == nullptr) {
        static int n = 0;
        if (n < 25) {
            ++n;
            std::fprintf(stderr, "[Unassign] #%d citizen=%p oldWp=%p slot %d->%d "
                         "caller=%p (static 0x%llx)\n", n, selfV,
                         (void*)*pWorkplace, *pSlot, slotIndex,
                         __builtin_return_address(0),
                         (unsigned long long)((uintptr_t)__builtin_return_address(0)   // CAST-OK: ptr_int
                             - (uintptr_t)GetModuleHandleW(nullptr) + 0x140000000ULL));   // CAST-OK: ptr_int
            std::fflush(stderr);
        }
    }

    // --- Remove from the old residence roster. ---
    if (*pWorkplace != nullptr) {
        ResidenceComponent* storage = (*pWorkplace)->FindComponent<ResidenceComponent>();
        Object::InternalPtr<CitizenComponent> selfIBP(this);
        // Guard: a force-assigned / partially-built home may lack a ResidenceComponent
        // (null); the original relies on the invariant that an assigned home always
        // carries one. Skip the roster op crash-safe if absent.
        if (storage != nullptr)
            storage->RemoveResident(*pSlot, &selfIBP);   // 0x14012a550
    }

    // --- Store the new home + slot. ---
    *pWorkplace = newWp;
    *pSlot = slotIndex;

    // --- Add to the new residence roster. ---
    if (*pWorkplace != nullptr) {
        ResidenceComponent* storage = (*pWorkplace)->FindComponent<ResidenceComponent>();
        Object::InternalPtr<CitizenComponent> selfIBP(this);
        if (storage != nullptr)
            storage->AddResident(*pSlot, &selfIBP);   // 0x14012a710
    }

    // --- Propagate to the children on the +0x260 circular ring. ---
    // head = _firstChild; each node's next = _nextSibling; loop until next wraps to head.
    CitizenComponent* node = _firstChild;
    if (node != nullptr) {
        do {
            node->SetHome(newHome, *pSlot);
            node = node->_nextSibling;
        } while (node != _firstChild);
    }
}

// B4 FORWARD: 3 outside caller(s) (Component/residencecomponent.cpp, Game/starttown.cpp,
// standalone/death_harness.cpp)
extern "C" void CitizenComponent_AssignWorkplace(void* selfV, void* workplaceIBP, int slotIndex) {
    reinterpret_cast<CitizenComponent*>(selfV)->SetHome(   // LEFT: B4 FORWARD (wave X)
        *reinterpret_cast<ComponentEntity**>(workplaceIBP), slotIndex);   // LEFT: B4 FORWARD (wave X)
}
