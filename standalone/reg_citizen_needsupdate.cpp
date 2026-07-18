// standalone/reg_citizen_needsupdate.cpp
// Registration for the CitizenComponent message-dispatcher brick (Economy M-D,
// citizen/eat-message half): code/Game/Component/citizen_needsupdate.cpp.
// Wire RegisterCitizenNeedsUpdate() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// VERIFIED addresses (disasm-verified against the retail CitizenComponent vtable
// 0x140162580 slot +0x78 = 0x140112260 and its case table):
//   0x140112260  CitizenComponent_NeedsUpdate  (switch(msg) dispatcher, vtable+0x78)
//   0x1401121e0  CitizenComp_helper_21e0       (msg 0: warm-house thirst-zero)
//
// Closure: sub-callees are already registered — 0x140008090 (reg_component_people),
// 0x14012ac30 (reg_citizenai_findhelpers), 0x1401152e0 EatFromStorage/"ReleaseWorkplace"
// (reg_component_people), Time::Time(float) dispatch slot 0x140152b00
// (address_registry), CheckPtr slot. msg 1/4/5 helpers (0x14010fde0 / 0x1401117a0 /
// 0x140111540) are intentionally NOT ported — the dispatcher resolves them (NULL in
// standalone) and skips with a one-time [EatMsg] trace.
//
// rdata float consts registered here (real retail values read with
// inspect_memory_content; not registered anywhere else):
//   0x140172114 = 0.5f   (msg-3 happiness increment)
//   0x140172234 = 300.0f (msg-6/7 Time scale)
#include "resolve.h"
#include <cstdint>

extern void CitizenComponent_NeedsUpdate(void*, int, uint64_t*, int, int);  // 0x140112260
extern void CitizenComp_helper_21e0(int64_t, void*);                        // 0x1401121e0

// rdata float-const storage (filled with the real exe values).
static const float k_rdata_172114 = 0.5f;
static const float k_rdata_172234 = 300.0f;

extern "C" void RegisterCitizenNeedsUpdate() {
    RegisterExeAddr(0x140112260, reinterpret_cast<uintptr_t>(&CitizenComponent_NeedsUpdate));
    RegisterExeAddr(0x1401121e0, reinterpret_cast<uintptr_t>(&CitizenComp_helper_21e0));
    // rdata float consts (real exe values; not registered elsewhere).
    RegisterExeAddr(0x140172114, reinterpret_cast<uintptr_t>(&k_rdata_172114));
    RegisterExeAddr(0x140172234, reinterpret_cast<uintptr_t>(&k_rdata_172234));
}
