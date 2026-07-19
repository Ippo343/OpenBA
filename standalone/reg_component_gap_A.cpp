// Component GAP wave — batch A (citizencomponent + attendancecomponent)
//
// Registers the closure-safe exe helper addresses from Batch A so the guarded
// ResolveAddr(Module::Exe, ...) call sites in code/Game/Component/ fire in
// standalone mode. The integrator wires RegisterComponentGap_A() into
// standalone/address_registry.cpp.
//
// Closure-safe (verified — every exe sub-callee is in notes/_reg_snapshot.txt or a
// Runtime export / engine_funcs pointer):
//   0x1401158d0  CitizenComponent::CheckWorkplaceValid -> CitizenComp_CheckWorkplaceValid
//                  callees: 0x140087fb0(reg), 0x1400a00f0(reg), CheckPtr slot(runtime)
//   0x14008ef40  Entity_impl_ef40 (pick free production slot) -> CitizenComp_Entity_impl_ef40
//                  callees: 0x140006270(reg), IsDerivedFrom(engine), Rand(runtime), CheckPtr slot
//
// FORMERLY DEFERRED — now PORTED + registered by standalone/reg_citizen_25.cpp
// (openfuncs step 2.5a); their former blockers all landed (AssignWorkplace 0x1401163d0,
// SetupYoung/Elder 115110/115230, AssignHousing 0x140111ee0), and 0x140110330 /
// 0x140030b90 fold into the sort's std::sort predicate, name-gen uses clean C++:
//   0x1400302a0  helper_02a0   -> CitizenComp_SortByProfession_02a0
//   0x140112040  sub_2040      -> CitizenComp_LeaveRoster_2040
//   0x1401168a0  helper_68a0   -> CitizenComp_JoinRoster_68a0
//   0x140117080  FUN_140117080 -> CitizenComp_DeferredInit_117080
//   (the 10 KNOWN citizencomponent lifecycle Level-2 defers per the addendum remain)
//
// attendancecomponent (worker-release link helpers consumed by ProcessQueue):
//   0x140111650 Entity_impl_1650 -> AttendanceComponent_ReleaseLink1650  (CLOSURE-SAFE:
//                 callees 0x140002cc0/0x140002f90/0x14012c3f0 all registered)
//   0x1401159b0 Entity_impl_59b0 -> AttendanceComponent_ReleaseLink59b0  (CLOSURE-SAFE,
//                 identical body, slot +0x228 vs +0x230)
//   0x140115a50 FUN_140115a50    -> AttendanceComponent_ReleasePartnerSlot, REGISTERED at
//                 line ~99 below (TryFindPartner 0x140111840 landed in the cascade round).
#include "resolve.h"
#include <cstdint>

// Reimpl bodies live in code/Game/Component/citizencomponent.cpp (non-static).
struct CitizenComponent;
extern char     CitizenComp_CheckWorkplaceValid(CitizenComponent* self, void* ibp);
extern int64_t* CitizenComp_Entity_impl_ef40(void* a, int64_t* outRes, void* ibp, int cat);
// Re-verify pass (snapshot2): these two were KNOWN-defers but are actually closure-safe.
extern void     CitizenComp_ConsumeFood(CitizenComponent* self);   // 0x140112990 (true leaf)
extern void     CitizenComp_ConsumeFuel(CitizenComponent* self);   // 0x140112ae0 (callees 7fb0/8090 reg)
// Cascade round (snapshot3): workplace 12f7e0/12fa20 + 002e10 registration unblocked
// the housing/workplace/storage/partner finder chain. Landed bottom-up:
//   sub_40f0 (002ed0/122e20/002e10/0a00f0) -> sub_1930 (sub_40f0/087fb0/008830) ->
//   FindWorkplace/FindStorage/TryFindPartner (sub_1930/sub_40f0/003110) + AssignHousing.
extern void     CitizenComp_AssignHousing(CitizenComponent* self, int64_t* param2); // 0x140111ee0
extern bool     CitizenComp_sub_40f0(int64_t building, void* citizenIBP);           // 0x1400440f0
extern int64_t* CitizenComp_sub_1930(int64_t self, int64_t* outBest, void* exclIBP, int64_t key); // 0x140111930
extern char     CitizenComp_FindWorkplace(CitizenComponent* self);                 // 0x140111590
extern void     CitizenComp_FindStorage(CitizenComponent* self);                   // 0x1401116f0
extern char     CitizenComp_TryFindPartner(CitizenComponent* self, int64_t* param2); // 0x140111840

// Reimpl bodies live in code/Game/Component/attendancecomponent.cpp (non-static).
extern void AttendanceComponent_ReleaseLink1650(void* workerP);
extern void AttendanceComponent_ReleaseLink59b0(void* workerP);
// Cascade round: now closure-safe (TryFindPartner 0x140111840 landed this round).
extern void AttendanceComponent_ReleasePartnerSlot(void* workerP, char reroll); // 0x140115a50

// ConsumeFuel (warmth) .rdata constants — raw .rdata addrs return NULL via
// ResolveAddr in standalone, so citizencomponent.cpp's `?:` fallbacks (all 0.0)
// were used. c0ec=0 in particular zeroed the ENTIRE heating-demand term
// (fVar5 = ... * c0ec * cap), pinning every citizen's warmth at max regardless of
// weather or firewood. Real values read from Application-x64.exe .rdata via Ghidra:
//   0x1401720ec = 0x3E000000 = 0.125f  (heating-demand scale — the critical one)
//   0x140172168 = 0x3FE0000000000000 = 0.5 (double; warmth decay step when demand<cur)
//   0x1401721a8 = 0x41000000 = 8.0f    (warmth flag-bit-1 penalty)
static const float  k_rdata_1720ec = 0.125f;
static const double k_rdata_172168 = 0.5;
static const float  k_rdata_1721a8 = 8.0f;
// ConsumeFood (happiness/variety) constant — same 0.0-fallback degeneracy as the
// warmth ones. `c100`=`DAT_140172100` is subtracted from the food-comfort
// accumulator (+0x50) as the "stale/insufficient-variety food" penalty; NULL in
// standalone -> fallback 0.0 nullified the penalty. Real .rdata value (Ghidra):
//   0x140172100 = 0x3E800000 = 0.25f
// (uv scale 0x14017214c = 1.0f already matches its fallback; c180 0x140172180 and
//  hungerDiv 0x14017225c are registered elsewhere — only 172100 was missing.)
static const float  k_rdata_172100 = 0.25f;

void RegisterComponentGap_A() {
    // citizencomponent.cpp
    RegisterExeAddr(0x1401158d0, reinterpret_cast<uintptr_t>(&CitizenComp_CheckWorkplaceValid));
    RegisterExeAddr(0x14008ef40, reinterpret_cast<uintptr_t>(&CitizenComp_Entity_impl_ef40));
    RegisterExeAddr(0x140112990, reinterpret_cast<uintptr_t>(&CitizenComp_ConsumeFood));   // re-verify: leaf
    RegisterExeAddr(0x140112ae0, reinterpret_cast<uintptr_t>(&CitizenComp_ConsumeFuel));   // re-verify: 7fb0/8090 reg
    // Warmth demand/decay constants (see note above) — real .rdata values.
    RegisterExeAddr(0x1401720ec, reinterpret_cast<uintptr_t>(&k_rdata_1720ec));
    RegisterExeAddr(0x140172168, reinterpret_cast<uintptr_t>(&k_rdata_172168));
    RegisterExeAddr(0x1401721a8, reinterpret_cast<uintptr_t>(&k_rdata_1721a8));
    RegisterExeAddr(0x140172100, reinterpret_cast<uintptr_t>(&k_rdata_172100));

    // cascade round (snapshot3): citizen-lifecycle finder chain (bottom-up)
    RegisterExeAddr(0x1400440f0, reinterpret_cast<uintptr_t>(&CitizenComp_sub_40f0));       // reachability test
    RegisterExeAddr(0x140111930, reinterpret_cast<uintptr_t>(&CitizenComp_sub_1930));       // closest-building scan
    RegisterExeAddr(0x140111590, reinterpret_cast<uintptr_t>(&CitizenComp_FindWorkplace));
    RegisterExeAddr(0x1401116f0, reinterpret_cast<uintptr_t>(&CitizenComp_FindStorage));
    RegisterExeAddr(0x140111840, reinterpret_cast<uintptr_t>(&CitizenComp_TryFindPartner));
    RegisterExeAddr(0x140111ee0, reinterpret_cast<uintptr_t>(&CitizenComp_AssignHousing));

    // attendancecomponent.cpp
    RegisterExeAddr(0x140111650, reinterpret_cast<uintptr_t>(&AttendanceComponent_ReleaseLink1650));
    RegisterExeAddr(0x1401159b0, reinterpret_cast<uintptr_t>(&AttendanceComponent_ReleaseLink59b0));
    RegisterExeAddr(0x140115a50, reinterpret_cast<uintptr_t>(&AttendanceComponent_ReleasePartnerSlot)); // cascade: TryFindPartner landed
}
