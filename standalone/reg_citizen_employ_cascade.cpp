// standalone/reg_citizen_employ_cascade.cpp
// Brick M-C — register the town citizen-list registrar (0x140114000
// CitizenComponent_OnAdd), ported in code/Game/Component/citizen_employ_cascade.cpp.
//
// The four seat strategies (cbf0/ca20/c5f0/c4b0) and the FUN_1400896a0 shuffle are
// FILE-STATIC in the port and are reached ONLY through CitizenEmploy_SeatWorkers
// (called directly by the spawn wiring, not via ResolveAddr), so they need no
// standalone address registration — nothing in the exe dispatches to them in our
// build (dd10, their only caller, is UI-driven and not invoked in standalone).
// CitizenComponent_OnAdd is a vtable OnAdded slot (DATA-xref only); we register it
// so any future lifecycle dispatch resolves to our port, and we also call it
// directly at spawn to populate townMgr+0xa0.
//
// Its closure is already registered: FUN_140003110 (array add, reg_action_hunt),
// FindAvailableBuilding 0x14008d100 (reg_town_workplace_registry_reader),
// AssignHousing 0x140111ee0 (reg_component_gap_A), Entity_FindComponent_Storage
// 0x140008090 (reg_component_people), Entity_impl_7fb0 0x140087fb0 (reg_pf_road),
// CalcDistance 0x140008830 (reg_work_base).
#include "resolve.h"
#include <cstdint>

extern "C" void CitizenComponent_OnAdd(void* citizenComp);   // 0x140114000

void RegisterCitizenEmployCascade() {
    RegisterExeAddr(0x140114000, reinterpret_cast<uintptr_t>(&CitizenComponent_OnAdd));
}
