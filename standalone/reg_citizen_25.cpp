// standalone/reg_citizen_25.cpp
// openfuncs step 2.5 — citizen needs-msg helpers (group c) + citizen-lifecycle
// quartet (group a). Wire RegisterCitizen25() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Bodies:
//   code/Game/Component/citizen_needsupdate.cpp        (msg 1/4/5 helpers)
//   code/Game/Component/citizencomponent_lifecycle.cpp (roster + sort + deferred init)
//
// addr          symbol                                group  one-liner
// -----------   -----------------------------------   -----  --------------------------
// 0x14010fde0   CitizenComp_helper_fde0               2.5c   msg 1: happiness credit
// 0x1401117a0   CitizenComp_helper_17a0               2.5c   msg 4: attendance/schedule budget
// 0x140111540   CitizenComp_helper_1540               2.5c   msg 5: countdown decay
// 0x140112040   CitizenComp_LeaveRoster_2040          2.5a   unlink from workplace roster
// 0x1401168a0   CitizenComp_JoinRoster_68a0           2.5a   insert into roster + assign
// 0x1400302a0   CitizenComp_SortByProfession_02a0     2.5a   profession-priority sort (introsort)
// 0x140117080   CitizenComp_DeferredInit_117080       2.5a   per-citizen deferred init
//
// Closure notes:
//   - msg-4 AssignState (0x140113af0) stays an accepted Level-2 gap (NOT ported): the
//     state-9 assign silently no-ops until it lands (see citizen_needsupdate.cpp).
//   - the sort's comparator/insertion-sort helpers (0x140110330 / 0x140030b90) are
//     reimplemented inside the std::sort predicate, not registered (see lifecycle .cpp).
//   - 0x140117080's name generator uses the clean C++ GenerateCitizenName rather than
//     ResolveAddr(0x1400a4c60).
//   - .rdata float consts reused (already registered): 0x14017225c=3600.0f
//     (reg_component_fruit), 0x140172234=300.0f (reg_citizen_needsupdate).
#include "resolve.h"
#include <cstdint>

// citizen_needsupdate.cpp
extern void CitizenComp_helper_fde0(int64_t self, int param);   // 0x14010fde0
extern void CitizenComp_helper_17a0(int64_t self);              // 0x1401117a0
extern void CitizenComp_helper_1540(int64_t self);              // 0x140111540

// citizencomponent_lifecycle.cpp
extern void CitizenComp_LeaveRoster_2040(int64_t self, void* param2);              // 0x140112040
extern void CitizenComp_JoinRoster_68a0(void* self, void* param2);                 // 0x1401168a0
extern void CitizenComp_SortByProfession_02a0(void* arr, int n, void* tmp);        // 0x1400302a0
extern void CitizenComp_DeferredInit_117080(int64_t self, void* pos, uint32_t gender,
                                            char isElder, int arg5, char forceEmploy,
                                            int64_t* parentIBP, int64_t spawnKey);  // 0x140117080

void RegisterCitizen25() {
    // group c — needs-msg helpers
    RegisterExeAddr(0x14010fde0, reinterpret_cast<uintptr_t>(&CitizenComp_helper_fde0));
    RegisterExeAddr(0x1401117a0, reinterpret_cast<uintptr_t>(&CitizenComp_helper_17a0));
    RegisterExeAddr(0x140111540, reinterpret_cast<uintptr_t>(&CitizenComp_helper_1540));
    // group a — citizen-lifecycle quartet
    RegisterExeAddr(0x140112040, reinterpret_cast<uintptr_t>(&CitizenComp_LeaveRoster_2040));
    RegisterExeAddr(0x1401168a0, reinterpret_cast<uintptr_t>(&CitizenComp_JoinRoster_68a0));
    RegisterExeAddr(0x1400302a0, reinterpret_cast<uintptr_t>(&CitizenComp_SortByProfession_02a0));
    RegisterExeAddr(0x140117080, reinterpret_cast<uintptr_t>(&CitizenComp_DeferredInit_117080));
}
