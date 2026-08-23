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
// 0x14011c530 contents-scatter is now ported (destroycomponent_states.cpp) but is
// called directly in-file by state-5 enter/tick, never by exe address — so it needs
// no registration here.
//
// State 6 (collapse/rubble) is now ported too — destroycomponent_state6.cpp:
//   0x14011d4d0 state-6 enter    (cancel citizen actions, empty storage, char it)
//   0x14011d400 state-6 update   (coin-flip extra rubble step)
//   0x14011d490 state-6 tick     (burn countdown -> rubble step)
//   0x14011d5d0 rubble step      (mesh variant down one, decal + materials refresh)
//   0x140125020 material re-point (ModelComponent -> burnt material set)
//   0x14000b630 FindComponent<DecalComponent>   (was unregistered)
//   0x140089e30 TownManager: abort citizen actions for an entity
//   0x1400c52c0 ActionStack: abort actions targeting an entity
// plus the two Graphics dispatch slots those need (GetMaterialList /
// SetMaterialInstance). All gated at runtime by BANISHED_LIVE_DESTROY_COLLAPSE.
//
// Deliberately NOT registered (deferred; keep NOT-IMPLEMENTED tracking honest):
//   0x140128f50 PickingComponent pick-mesh rebuild (state 6 skips it, logged once),
//   0x14011bcc0/d390/d2a0/d1a0 transition actions, 0x14012ea20 (in-TU only),
//   0x140086fb0 config Serialize (reached via the registered vtable, never by address).
//
// The workplace pair the state machine drives (0x14012f670 worker release / 0x14012f520
// state-4 builder re-seat) now lives in reg_destroy_workers.cpp, gated at runtime by
// BANISHED_LIVE_DESTROY_WORKERS.

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

// State 6 — collapse/rubble (code/Game/Component/destroycomponent_state6.cpp).
extern "C" char  Destroy_State6Enter_d4d0(void* data, void* sm);
extern "C" char  Destroy_State6Update_d400(void* data, void* sm);
extern "C" char  Destroy_State6Tick_d490(void* data, void* sm, void* time);
extern "C" void  Destroy_RubbleStep_d5d0(void* destroyComp);
extern "C" void  ModelComp_SetMaterialSet_125020(void* modelComp, int materialSetIdx);
extern "C" void* Destroy_FindDecalComponent_b630(uintptr_t entity, void* retIBP);
extern "C" void  TownMgr_AbortCitizenActionsFor_89e30(void* townMgr, void** pEntityIbp);
extern "C" void  ActionStack_AbortForEntity_c52c0(void* stack, void** pEntityIbp);

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

    // ---- state 6: collapse / rubble --------------------------------------
    RegisterExeAddr(0x14011d4d0, reinterpret_cast<uintptr_t>(&Destroy_State6Enter_d4d0));
    RegisterExeAddr(0x14011d400, reinterpret_cast<uintptr_t>(&Destroy_State6Update_d400));
    RegisterExeAddr(0x14011d490, reinterpret_cast<uintptr_t>(&Destroy_State6Tick_d490));
    RegisterExeAddr(0x14011d5d0, reinterpret_cast<uintptr_t>(&Destroy_RubbleStep_d5d0));
    RegisterExeAddr(0x140125020, reinterpret_cast<uintptr_t>(&ModelComp_SetMaterialSet_125020));
    RegisterExeAddr(0x14000b630, reinterpret_cast<uintptr_t>(&Destroy_FindDecalComponent_b630));
    RegisterExeAddr(0x140089e30, reinterpret_cast<uintptr_t>(&TownMgr_AbortCitizenActionsFor_89e30));
    RegisterExeAddr(0x1400c52c0, reinterpret_cast<uintptr_t>(&ActionStack_AbortForEntity_c52c0));

    // The two Graphics dispatch slots the rubble step reads (GraphicsMesh::
    // GetMaterialList / MeshInstance::SetMaterialInstance) live with the rest of the
    // slot table in address_registry.cpp — RegisterDispatchSlot is file-local there.
}
