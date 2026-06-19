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
// DEFERRED (unregistered exe sub-callees -> complex defers, NOT registered):
//   0x1400302a0  helper_02a0      -> calls 0x140110330 + 0x140030b90 (both unregistered)
//   0x140112040  sub_2040         -> calls 0x1401163d0 AssignWorkplace (unregistered, Level-2)
//   0x1401168a0  helper_68a0      -> calls 0x1401163d0 AssignWorkplace (unregistered, Level-2)
//   0x140117080  FUN_140117080    -> calls 0x140111ee0/115230/115110/0x1400a4c60 (unregistered)
//   (plus the 10 KNOWN citizencomponent lifecycle Level-2 defers per the addendum)
//
// attendancecomponent (worker-release link helpers consumed by ProcessQueue):
//   0x140111650 Entity_impl_1650 -> AttendanceComponent_ReleaseLink1650  (CLOSURE-SAFE:
//                 callees 0x140002cc0/0x140002f90/0x14012c3f0 all registered)
//   0x1401159b0 Entity_impl_59b0 -> AttendanceComponent_ReleaseLink59b0  (CLOSURE-SAFE,
//                 identical body, slot +0x228 vs +0x230)
//   0x140115a50 FUN_140115a50    -> DEFER: calls 0x140111840 TryFindPartner (unregistered,
//                 KNOWN citizencomponent Level-2 lifecycle defer) -> NOT registered.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// Reimpl bodies live in code/Game/Component/citizencomponent.cpp (non-static).
struct CitizenComponent;
extern char     CitizenComp_CheckWorkplaceValid(CitizenComponent* self, void* ibp);
extern int64_t* CitizenComp_Entity_impl_ef40(void* a, int64_t* outRes, void* ibp, int cat);
// Re-verify pass (snapshot2): these two were KNOWN-defers but are actually closure-safe.
extern void     CitizenComp_ConsumeFood(CitizenComponent* self);   // 0x140112990 (true leaf)
extern void     CitizenComp_ConsumeFuel(CitizenComponent* self);   // 0x140112ae0 (callees 7fb0/8090 reg)

// Reimpl bodies live in code/Game/Component/attendancecomponent.cpp (non-static).
extern void AttendanceComponent_ReleaseLink1650(void* workerP);
extern void AttendanceComponent_ReleaseLink59b0(void* workerP);

void RegisterComponentGap_A() {
    // citizencomponent.cpp
    RegisterExeAddr(0x1401158d0, reinterpret_cast<uintptr_t>(&CitizenComp_CheckWorkplaceValid));
    RegisterExeAddr(0x14008ef40, reinterpret_cast<uintptr_t>(&CitizenComp_Entity_impl_ef40));
    RegisterExeAddr(0x140112990, reinterpret_cast<uintptr_t>(&CitizenComp_ConsumeFood));   // re-verify: leaf
    RegisterExeAddr(0x140112ae0, reinterpret_cast<uintptr_t>(&CitizenComp_ConsumeFuel));   // re-verify: 7fb0/8090 reg

    // attendancecomponent.cpp
    RegisterExeAddr(0x140111650, reinterpret_cast<uintptr_t>(&AttendanceComponent_ReleaseLink1650));
    RegisterExeAddr(0x1401159b0, reinterpret_cast<uintptr_t>(&AttendanceComponent_ReleaseLink59b0));
}
#endif // STANDALONE_EXE
