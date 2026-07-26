// standalone/reg_citizen_onremove.cpp
// CitizenComponent::OnRemove (0x140113ca0) — the removed-from-world teardown,
// ported in code/Game/Component/citizen_onremove.cpp.
//
// The exe has NO call xrefs to this address; all four are DATA (the CitizenComponent
// vtable slot 7 at 0x1401625b8 plus three sibling vtables). Registration therefore
// changes nothing on its own — what actually turns the teardown on is the slot-7
// install in citizencomponent.cpp, gated by BANISHED_LIVE_CITIZEN_ONREMOVE. We still
// register the address so any future path that resolves the retail vtable entry
// lands on our port rather than on 0.
//
// Closure — every callee is ported AND registered (verified before writing the port,
// because "ported but unregistered" is exactly how the pause button ended up calling
// address 0 in 330710b):
//   0x1401176e0 HideTool             reg_action_complete.cpp
//   0x140115a50 ReleasePartnerSlot   reg_component_gap_A.cpp
//   0x1401159b0 ReleaseLink59b0      reg_component_gap_A.cpp
//   0x140111650 ReleaseLink1650      reg_component_gap_A.cpp
//   0x1401163d0 AssignWorkplace      reg_citizen_assignworkplace.cpp
//   0x140112040 LeaveRoster_2040     reg_citizen_25.cpp
//   0x140111fa0 sub_1fa0             reg_component_people.cpp
//   0x140111ee0 AssignHousing        reg_component_gap_A.cpp
//   0x140116dd0 HandleLifeEvent      reg_citizen_lifeevent_handler.cpp
//   0x14008c3d0 AssignWorker_c3d0    reg_townhallui.cpp
//   0x140113af0 PostLifeEvent        reg_eventlog_citizen.cpp
//   0x140103480 TradeWork_3480       reg_component_people.cpp
//   0x140002cc0 list erase           reg_component_visuals.cpp
#include "resolve.h"
#include <cstdint>

// Sibling registry audit (same "vtable slot 7 is misnamed OnCreate" shape):
//   townMgr+0x20  residences     — COVERED: ResidenceComponent slot 7 (0x14012a0e0) is
//                                  installed (residencecomponent.cpp) and removes it
//   townMgr+0x100 profession BST — COVERED: WorkPlaceComponent slot 7 is installed
//                                  (workplacecomponent.cpp) and removes it
//   townMgr+0xd0  build sites    — HOLED: BuildComponent::OnRemove (0x140108cb0, ported
//                                  as BuildComponent_OnCreate in buildcomponent.cpp) was
//                                  neither registered nor installed, so only a site that
//                                  reaches state 8 is de-registered. Registered below;
//                                  the slot-7 install is opt-in (BANISHED_LIVE_BUILD_ONREMOVE).
struct BuildComponent;
extern "C" void CitizenComponent_OnRemove(void* citizenComp);   // 0x140113ca0
void BuildComponent_OnCreate(BuildComponent* self);             // 0x140108cb0 (really OnRemove)

void RegisterCitizenOnRemove() {
    RegisterExeAddr(0x140113ca0, reinterpret_cast<uintptr_t>(&CitizenComponent_OnRemove));
    RegisterExeAddr(0x140108cb0, reinterpret_cast<uintptr_t>(&BuildComponent_OnCreate));
}
