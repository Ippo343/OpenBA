// standalone/reg_destroy_sm.cpp
// Registration wave — DestroyComponent state machine + UpgradeUIConfig
// (the wood→stone upgrade path; see code/Game/Component/destroycomponent_states.cpp).
//
// Registered handler impls (faithful ports, all gated at RUNTIME by
// BANISHED_LIVE_DESTROY via the component factory — registering here only
// makes the addresses resolvable):
//   0x14011d7b0  state-3 enter (begin demolition, branch 4/6)
//   0x14011cfd0  state-4 enter (queue ClearStorage haul-away)
//   0x14011cec0  state-4 tick  (storage drained -> 5)
//   0x14011cc70  state-5 enter (half-refund + queue BuildStructure)
//   0x14011c120  state-5 tick  (replacement spawn from DestroyComponent+0x60)
//   0x14011da80  storage-empty test
//   0x14011e600  upgrade-target-differs test
//   0x14011e5b0  upgrade-target-is-current test
//   0x14015df60  vtable_UpgradeUIConfig (upgradeuiconfig.cpp static vtable)
//
// Deliberately NOT registered (deferred; keep NOT-IMPLEMENTED tracking honest):
//   0x14011c530 contents-scatter, 0x14011d4d0/d400/d490 state-6 rubble path,
//   0x14011d5d0/0x140125020 rubble mesh helpers,
//   0x14011bcc0/d390/d2a0/d1a0 transition actions, 0x14012ea20 (in-TU only),
//   0x14012f670 worker release, 0x140086fb0 config Serialize (reached via the
//   registered vtable, never by address).

#include "resolve.h"
#include <cstdint>

extern "C" char Destroy_State3Enter_d7b0(void* data, void* sm);
extern "C" char Destroy_State4Enter_cfd0(void* data, void* sm);
extern "C" char Destroy_State4Tick_cec0(void* data, void* sm, void* time);
extern "C" char Destroy_State5Enter_cc70(void* data, void* sm);
extern "C" char Destroy_State5Tick_c120(void* data, void* sm, void* time);
extern "C" char Destroy_StorageEmpty_da80(int64_t comp);
extern "C" char Destroy_TargetDiffers_e600(int64_t comp);
extern "C" char Destroy_TargetIsCurrent_e5b0(int64_t comp);
extern "C" void RegisterUpgradeUIConfigVtable();

void RegisterDestroySmBatch() {
    RegisterExeAddr(0x14011d7b0, reinterpret_cast<uintptr_t>(&Destroy_State3Enter_d7b0));
    RegisterExeAddr(0x14011cfd0, reinterpret_cast<uintptr_t>(&Destroy_State4Enter_cfd0));
    RegisterExeAddr(0x14011cec0, reinterpret_cast<uintptr_t>(&Destroy_State4Tick_cec0));
    RegisterExeAddr(0x14011cc70, reinterpret_cast<uintptr_t>(&Destroy_State5Enter_cc70));
    RegisterExeAddr(0x14011c120, reinterpret_cast<uintptr_t>(&Destroy_State5Tick_c120));
    RegisterExeAddr(0x14011da80, reinterpret_cast<uintptr_t>(&Destroy_StorageEmpty_da80));
    RegisterExeAddr(0x14011e600, reinterpret_cast<uintptr_t>(&Destroy_TargetDiffers_e600));
    RegisterExeAddr(0x14011e5b0, reinterpret_cast<uintptr_t>(&Destroy_TargetIsCurrent_e5b0));
    RegisterUpgradeUIConfigVtable();
}
