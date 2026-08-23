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
// NOT ported here: 0x140002280 (FindComponent<AmbientEmitterComponent>, the
// tornado's audio arm). Its type-id global DWORD_1401c3e60 is itself unregistered,
// so porting the finder alone would leave the arm just as dead while looking fixed.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>

namespace {

// A .rdata type-id global that never got seeded reads back 0 / 0xFFFFFFFF, and an
// equality test against it matches nothing — silently deleting the search. Fall
// back to the name registry, as action_base_rawmat.cpp does.
uint32_t ResolveTid(uintptr_t ghidraGlobal, const char* name, uint32_t* cache) {
    uintptr_t p = ResolveAddr(Module::Exe, ghidraGlobal);
    uint32_t t = p ? *reinterpret_cast<uint32_t*>(p) : 0;
    if (t != 0 && t != 0xFFFFFFFFu) return t;
    if (*cache == 0) *cache = ::Object::GetTypeIdByName(name);
    return *cache;
}

void* FindByTypeId(uintptr_t entity, void* retIBP, uint32_t typeId) {
    void* found = nullptr;
    if (entity && typeId) {
        const int n = *reinterpret_cast<int*>(entity + 0x38);
        const uintptr_t base = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; ++i) {
            void** ibp = reinterpret_cast<void**>(base + static_cast<uintptr_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
            void* obj = *ibp;
            if (!obj) continue;
            const uint32_t t = (*reinterpret_cast<uint32_t (**)(void*)>(
                *reinterpret_cast<uintptr_t*>(obj) + 0x10))(obj);
            if (t == typeId) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
                void* o = *ibp;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(o, typeId)) found = o;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    else *reinterpret_cast<void**>(retIBP) = found;
    return retIBP;
}

}  // namespace

// FUN_140005840 — Entity::FindComponent<FireComponent>
extern "C" void* Entity_FindComponent_Fire_5840(uintptr_t entity, void* retIBP) {
    static uint32_t cache = 0;
    return FindByTypeId(entity, retIBP, ResolveTid(0x1401c41e0, "FireComponent", &cache));
}

// FUN_140009790 — Entity::FindComponent<CitizenComponent>
extern "C" void* Entity_FindComponent_Citizen_9790(uintptr_t entity, void* retIBP) {
    static uint32_t cache = 0;
    return FindByTypeId(entity, retIBP, ResolveTid(0x1401c7158, "CitizenComponent", &cache));
}
