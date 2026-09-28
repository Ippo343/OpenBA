// code/Game/Component/entity_finders_disaster.cpp
// Two shared Entity::FindComponent<T> accessors that the disaster lanes need and
// that nothing had registered.
//
// ── Why this file exists ────────────────────────────────────────────────────
// Found 2026-08-23 chasing "a tornado crossed my houses and nothing burned".
// TornadoComponent's per-entity strike (`Tornado_StrikeEntityCounted`) picks its
// arm from six component lookups, and the BUILDING arm is gated on
// `buildComp != 0 && fireComp != 0`. `findFire` resolves 0x140005840, which was
// registered NOWHERE — so `ResolveAddr` handed back 0, the finder was skipped,
// `fireComp` stayed 0 forever, and the arm could not be taken by any tornado
// however well aimed. The CITIZEN arm (0x140009790) was dead the same way.
//
// The trace made this hard to see rather than easy: every damage tick printed
// `building=0 building-NOFIRE=0 animal=0 citizen=0`, and a prior investigation had
// recorded "building=0 is NOT a finding — both forced paths ran through forest,
// never through town". That reasoning was sound when it was written and wrong by
// the time it mattered: it checked that `fireIgnite`/`fireIgniteFinish` resolved,
// but not that the FINDER feeding the gate did. An unresolved leaf two steps
// earlier produces exactly the same silence. tornadocomponent.cpp now reports the
// finders' resolution once at startup so the two can never look alike again.
//
// ── Shape ───────────────────────────────────────────────────────────────────
// Both are the standard member-return FindComponent: walk the entity's
// InternalBasePtr array (ptr @entity+0x30, count @+0x38), CheckPtr each slot,
// compare the object's GetTypeId (vtable +0x10) against a .rdata type-id global,
// and on the first match re-CheckPtr + IsDerivedFrom before constructing retIBP
// from it. Identical to ActRm_FindComponent_223c0 and friends; only the type-id
// global differs.
//
//   0x140005840  -> DWORD_1401c41e0  "FireComponent"     (reg_fire.cpp fills it)
//   0x140009790  -> DWORD_1401c7158  "CitizenComponent"  (reg_citizenai_typeids.cpp)
//
// Both globals are registered AND filled post-Startup already — the finders were
// the only missing half.
//
//   0x140002280  -> DWORD_1401c3e60  "AmbientEmitterComponent"
//
// The third finder is PORTED BELOW BUT DELIBERATELY NOT REGISTERED. Its old
// "NOT ported here" note blamed the unregistered type-id global DWORD_1401c3e60.
// That reason is FALSE — the finder keys on AmbientEmitterComponent::TypeId()
// (ENDGAME B.3.9 CB; it used a by-name fallback before), and
// ambientenvironmentcomponent.cpp:68 already resolves "AmbientEmitterComponent"
// by that same name.
//
// The note's CONCLUSION was right for a different reason, measured 2026-08-30 by
// registering it and crashing on the first founding-housing assignment of a new
// game (ACCESS_VIOLATION, READ of 0x10, in CitizenComponent_helper_a710+0x3be):
//
//   AmbientEmitterComponent is INERT in standalone — its OnAdded (FUN_140042e10)
//   is a no-op stub, so the emitter array at component+0x38 is never allocated.
//   ambientenvironmentcomponent.cpp:145-155 already documents this and works
//   around it for its own path. Retail a710 derefs *(emData+0x38) and reads
//   +0x10 with NO guard (verified in the 0x14012a710 decompile), so the moment
//   the finder starts returning a component, that deref faults on 0x10.
//
// This is the "a feature's dependencies only surface once you turn it on" case.
// Do NOT add a null check at the call sites: the deref is faithful and the
// component is genuinely half-built. Register 0x140002280 only after
// AmbientEmitterComponent::OnAdded (0x140042e10) is ported so +0x38 is real.
// The finder itself is kept below, correct and ready, so that port is one line away.

#include <new>
#include "object/TypeInfo.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/FireComponent.h"      // ENDGAME B.3.9 C1: 0x1401c41e0 cell class
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 C1: 0x1401c7158 cell class
#include "Component/AmbientEmitterComponent.h"   // ENDGAME B.3.9 CB: 0x1401c3e60 cell class
#include "Component/exe_bodies.h"

namespace {

void* FindByTypeId(uintptr_t entity, void* retIBP, uint32_t typeId) {
    void* found = nullptr;
    if (entity && typeId) {
        const int n = *reinterpret_cast<int*>(entity + 0x38);
        const uintptr_t base = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; ++i) {
            void** ibp = reinterpret_cast<void**>(base + static_cast<uintptr_t>(i) * 8);
            void* obj = *ibp;
            if (!obj) continue;
            const uint32_t t = reinterpret_cast<Object::Base*>(obj)->Type();   // B.3.8 bridge (slot 2)
            if (t == typeId) {
                void* o = *ibp;
                if (::Object::IsDerivedFromNullSafe(o, typeId)) found = o;
                break;
            }
        }
    }
    new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return retIBP;
}

}  // namespace

// FUN_140005840 — Entity::FindComponent<FireComponent>
// ENDGAME B.3.9 C1 (B39_RULES.md D5): 0x1401c41e0 is FireComponent's registered
// type-id cell (INVENTORY_C Table 1) -- read BY CELL as the class's static.
extern "C" void* Entity_FindComponent_Fire_5840(uintptr_t entity, void* retIBP) {
    return FindByTypeId(entity, retIBP, FireComponent::TypeId());
}

// FUN_140009790 — Entity::FindComponent<CitizenComponent>
// 0x1401c7158 is CitizenComponent's registered type-id cell.
extern "C" void* Entity_FindComponent_Citizen_9790(uintptr_t entity, void* retIBP) {
    return FindByTypeId(entity, retIBP, CitizenComponent::TypeId());
}

// FUN_140002280 — Entity::FindComponent<AmbientEmitterComponent>
// Callers: the fire crackle start/stop (firecomponent_ignite.cpp), the tornado's
// audio arm (tornadocomponent.cpp), and the occupied-building emitter upkeep on
// worker assign/unassign (citizen_assignworkplace.cpp). All were guarded, so the
// only symptom was silence.
//
// ENDGAME B.3.9 CB: 0x1401c3e60 (AmbientEmitterComponent, unregistered) is read as
// the class static. The old ResolveTid(cell, name, cache) fallback already produced
// the same id by name, and this finder is registered NOWHERE (see the file header:
// register 0x140002280 only after AmbientEmitterComponent::OnAdded 0x140042e10 is
// ported), so the conversion is inert.
extern "C" void* Entity_FindComponent_AmbientEmitter_2280(uintptr_t entity, void* retIBP) {
    return FindByTypeId(entity, retIBP, AmbientEmitterComponent::TypeId());   // cell 0x1401c3e60
}
