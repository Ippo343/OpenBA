// Component GAP wave — batch B (citizenAI + misc people)
// Registers ONLY the closure-safe exe helper addresses -> our clean reimpls so the
// guarded ResolveAddr(Module::Exe, ...) call sites in code/Game/Component/ actually
// fire. The integrator wires RegisterComponentGap_B() into address_registry.cpp.
//
// Classification summary (see agent report for detail):
//   SAFE-PORTED  : 0x140115110/0x1401151a0/0x140115230 (Setup age-mesh, leaf),
//                  0x14009f990 (fishing brush, leaf), 0x14000cbd0 (entity find-by-type),
//                  0x140031d10 (ExternalPtr serialize via shared generic),
//                  0x140102b60 (controller rebuild, all-external closure — INTRICATE).
//   CALLER-BUG   : 0x140035ec0 was MID-FUNCTION inside FUN_140035d50; the true per-slot
//                  copy callee is 0x140007960 (closure-safe). Literal fixed in
//                  residencecomponent.cpp; we register 0x140007960. 0x140035ec0 is NOT
//                  registered.
//   WELL cluster (landed in the final bottom-up pass): 0x140091ba0 (remove-by-IBP) +
//                  its only unregistered callee 0x1400889e0 (pure leaf, grid-cell int-array
//                  compaction). Closure now bottoms out (FUN_140004680 reg, CheckPtr/IBP-ctor
//                  engine). Both ported in wellcomponent.cpp.
//   DEFER (NOT registered):
//                  0x140047200 CemeteryComponent::Draw (self-resolving passthrough -> recursion).
//   RESOLVED since this wave (registrations live elsewhere — do not re-add here):
//                  0x14010dca0 BuildCandidateStateList + 0x14010f1f0 ValidateAndEnterState and
//                      their helpers (GetHappiness/HasSufficientResources[2]/SortByPriority)
//                      -> reg_citizenai_candidatelist.cpp; all 14 CitizenAI_State_* handlers
//                      (14, not 15 — 0x14012ab00 is Storage_GetResourceCount2, a Gather2
//                      helper) -> reg_citizenai_handlers/2/3, gather, growup, leisure,
//                      housing_idle waves. Audited faithful vs Ghidra 2026-07-19.
#include "resolve.h"
#include <cstdint>

// ---- citizenaicomponent.cpp (CitizenComponent age-mesh setup helpers) ----
extern void CitizenComponent_SetupYoung_Reimpl(void* citizen);
extern void CitizenComponent_SetupAdult_Reimpl(void* citizen);
extern void CitizenComponent_SetupElder_Reimpl(void* citizen);

// ---- happinesscomponent.cpp (fishing brush leaf) ----
extern void FishingComponent_BrushTile(int* mgr, int state, int cx, int cy, int r, short delta);

// ---- storagedisplaycomponent.cpp (entity find-by-type) ----
extern void* StorageDisplay_EntityFindByType_cbd0(void* host, void* retbuf);

// ---- absorbresourcecomponent.cpp (ExternalPtr<Description>::Serialize) ----
extern void Component_Helper1d10_Reimpl(void* extRefField, void* serializer);

// ---- residencecomponent.cpp (per-slot inner pointer-array deep copy; true callee
//      of the corrected 0x140035ec0 -> 0x140007960 literal) ----
extern void ResidenceComponent_CopyInnerArray(void* dst, void* src);

// ---- wellcomponent.cpp (WELL cluster: remove-by-IBP + grid-cell compaction leaf) ----
extern void WellComp_UnregisterEntity_91ba0(int64_t manager, void* ibp);
extern void WellComp_RemoveCellEntry_889e0(int64_t manager, int x, int y, int value);
extern void WellComp_RegisterEntity_91cb0(int64_t manager, void* ibp);
extern void WellComp_AddCellEntry_88a80(int64_t manager, int x, int y, int value);
extern "C" char WellComponent_EnterBuilt(void* comp, void* sm);
extern "C" char WellComponent_EnterDemolish(void* comp, void* sm);

// ---- nomadcomponent.cpp (controller rebuild) ----
// DEFERRED by lead (NOT registered): 0x140102b60 has no unregistered FUN_ callees, but it
// instantiates a controller via Object::Factory::CreateObject and then drives its vtable
// (+0x20 Startup, +0x58 query, +0x28 teardown). That is the tool/dialog-controller-vtable
// bring-up territory that crashes in standalone (Player wave: g_ToolUI_ChildDialog_Vtable
// 0x14015f5a0) and was already skipped in Component wave 1. Static "no FUN_ callees" is
// necessary-but-not-sufficient — the indirect vtable calls are the hidden dependency.
// Body NomadComp_RebuildController_b60 is kept in nomadcomponent.cpp (dormant) but unwired.

void RegisterComponentGap_B() {
    // citizenaicomponent.cpp
    RegisterExeAddr(0x140115110, reinterpret_cast<uintptr_t>(&CitizenComponent_SetupYoung_Reimpl));
    RegisterExeAddr(0x1401151a0, reinterpret_cast<uintptr_t>(&CitizenComponent_SetupAdult_Reimpl));
    RegisterExeAddr(0x140115230, reinterpret_cast<uintptr_t>(&CitizenComponent_SetupElder_Reimpl));

    // happinesscomponent.cpp
    RegisterExeAddr(0x14009f990, reinterpret_cast<uintptr_t>(&FishingComponent_BrushTile));

    // storagedisplaycomponent.cpp
    RegisterExeAddr(0x14000cbd0, reinterpret_cast<uintptr_t>(&StorageDisplay_EntityFindByType_cbd0));

    // absorbresourcecomponent.cpp
    RegisterExeAddr(0x140031d10, reinterpret_cast<uintptr_t>(&Component_Helper1d10_Reimpl));

    // residencecomponent.cpp  (CALLER-BUG FIX: 0x140035ec0 -> 0x140007960)
    RegisterExeAddr(0x140007960, reinterpret_cast<uintptr_t>(&ResidenceComponent_CopyInnerArray));

    // wellcomponent.cpp  (WELL cluster — bottom-up closure landed)
    RegisterExeAddr(0x1400889e0, reinterpret_cast<uintptr_t>(&WellComp_RemoveCellEntry_889e0));
    RegisterExeAddr(0x140091ba0, reinterpret_cast<uintptr_t>(&WellComp_UnregisterEntity_91ba0));
    // The REGISTER half of the same pair (added 2026-08-22, fire lane): without these
    // the water-source array and the coarse cell index the FireWork search walks are
    // never written, so every fire reports buckets=0.
    RegisterExeAddr(0x140088a80, reinterpret_cast<uintptr_t>(&WellComp_AddCellEntry_88a80));
    RegisterExeAddr(0x140091cb0, reinterpret_cast<uintptr_t>(&WellComp_RegisterEntity_91cb0));
    RegisterExeAddr(0x14005cf20, reinterpret_cast<uintptr_t>(&WellComponent_EnterBuilt));
    RegisterExeAddr(0x14005cee0, reinterpret_cast<uintptr_t>(&WellComponent_EnterDemolish));
    // nomadcomponent.cpp 0x140102b60 DEFERRED (see note above) — not registered.
}
