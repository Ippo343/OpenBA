// standalone/reg_action_child_ctors2.cpp
// Registration for the B3 leaf Action copy-ctors + ConsumeRawMaterial/PickupStored
// helpers (code/Game/Action/action_child_ctors2.cpp). Wired via RegisterActionChildCtors2Batch().
// Closure-safe (all sub-callees registered); faithful Ghidra-verified ports.
//
// Also fills the RawMaterialDescription type-id global 0x1401c7cb8 — read by ca520's
// component-array scan AND by several already-landed functions (action_base_rawmat,
// action_pf_*, StorageComponent_QueryCount) that previously saw it null. Filled via
// GetTypeIdByName AFTER Engine::Startup (Object::Factory::Startup assigns the real CRC
// id only then), called from game_main.cpp alongside the other FillComponentTypeIdGlobals_*.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Entity/ComponentEntity.h"
#include "Component/ConsumeProduceComponent.h"
#include "Component/ConsumeProduceDescription.h"
#include "Component/RawMaterialDescription.h"

extern void* ActChild_CompleteRoadCopyCtor_c59d0(void*, uint32_t, uint32_t, uint8_t); // 0x1400c59d0
extern void* ActChild_HuntAnimalCopyCtor_c8400(void*, uint64_t*, uint64_t*);          // 0x1400c8400
extern void* ActChild_DropOffRawCopyCtor_c7cd0(void*, uint64_t*, uint64_t*, uint64_t, uint32_t, uint8_t); // 0x1400c7cd0
extern void  Entity_ConsumeListAppend_64b0(int64_t, int64_t, int);                    // 0x1400c64b0
extern void* ActChild_ConsumeRawCopyCtor_c63b0(void*, uint64_t*, int64_t, uint64_t, uint8_t); // 0x1400c63b0
extern void  ActChild_TransferToCarrier_ca230(int64_t, int64_t, int);                 // 0x1400ca230
extern void* ActChild_PickupStoredCopyCtor_ca520(void*, uint64_t*, uint64_t*, int64_t, int, uint8_t); // 0x1400ca520
extern void* ActChild_ConsumeRawCtor3_c6440(void*, uint64_t*);                        // 0x1400c6440
extern void* Entity_FindConsumeProduce_b6f0(int64_t, void*);                          // 0x14000b6f0
extern uint64_t WeightedRandomIndex_119470(int64_t);                                  // 0x140119470

// Type-id globals filled after Engine::Startup.
static uint32_t g_tid_RawMaterialDescription = 0;  // 0x1401c7cb8
// 0x1401c7350 = ConsumeProduceComponent (writer ConsumeProduceComponent_RegisterType
// 0x14014eaf5) — CORRECTED 2026-07-19; was filled as "StateMachineComponent", a wrong
// CRC. The b6f0 finder scans for THIS id (it finds the ConsumeProduceComponent; the
// real StateMachineComponent slot is 0x1401c7d98).
static uint32_t g_tid_ConsumeProduceComponent = 0; // 0x1401c7350 (bef70 component-find leaf)
static uint32_t g_tid_ConsumeProduceDescription = 0; // 0x1401c7388 (bef70 component search)
// 0x1401c5d70 ComponentEntity (writer ComponentEntity_RegisterType 0x14014cff5) — the
// shared "is this ref a ComponentEntity" Serialize validator id, read by the Action
// family (PlayAnimation/CompleteTask/AnimateNaturalResource/CreateRawMaterial/
// SendComponentMessage/...) and many UI Serialize paths. Unfilled it also kept several
// Action ResolveFuncs `ok` gates false → per-call re-resolve log spam ([PlayAnimation]
// "ERROR: some function pointers not resolved!").
static uint32_t g_tid_ComponentEntity = 0;         // 0x1401c5d70

extern "C" void FillTypeIdGlobals_actionchild() {
    g_tid_RawMaterialDescription = RawMaterialDescription::TypeId();
    g_tid_ConsumeProduceComponent = ConsumeProduceComponent::TypeId();
    g_tid_ConsumeProduceDescription = ConsumeProduceDescription::TypeId();
    g_tid_ComponentEntity        = ComponentEntity::TypeId();
}

void RegisterActionChildCtors2Batch() {
    RegisterExeAddr(0x1400c59d0, reinterpret_cast<uintptr_t>(&ActChild_CompleteRoadCopyCtor_c59d0));
    RegisterExeAddr(0x1400c8400, reinterpret_cast<uintptr_t>(&ActChild_HuntAnimalCopyCtor_c8400));
    RegisterExeAddr(0x1400c7cd0, reinterpret_cast<uintptr_t>(&ActChild_DropOffRawCopyCtor_c7cd0));
    RegisterExeAddr(0x1400c64b0, reinterpret_cast<uintptr_t>(&Entity_ConsumeListAppend_64b0));
    RegisterExeAddr(0x1400c63b0, reinterpret_cast<uintptr_t>(&ActChild_ConsumeRawCopyCtor_c63b0));
    RegisterExeAddr(0x1400ca230, reinterpret_cast<uintptr_t>(&ActChild_TransferToCarrier_ca230));
    RegisterExeAddr(0x1400ca520, reinterpret_cast<uintptr_t>(&ActChild_PickupStoredCopyCtor_ca520));
    RegisterExeAddr(0x1400c6440, reinterpret_cast<uintptr_t>(&ActChild_ConsumeRawCtor3_c6440));
    RegisterExeAddr(0x14000b6f0, reinterpret_cast<uintptr_t>(&Entity_FindConsumeProduce_b6f0));
    RegisterExeAddr(0x140119470, reinterpret_cast<uintptr_t>(&WeightedRandomIndex_119470));
    // type-id global storage (values filled later by FillTypeIdGlobals_actionchild)
    RegisterExeAddr(0x1401c7cb8, reinterpret_cast<uintptr_t>(&g_tid_RawMaterialDescription));
    RegisterExeAddr(0x1401c7350, reinterpret_cast<uintptr_t>(&g_tid_ConsumeProduceComponent));
    RegisterExeAddr(0x1401c7388, reinterpret_cast<uintptr_t>(&g_tid_ConsumeProduceDescription));
    RegisterExeAddr(0x1401c5d70, reinterpret_cast<uintptr_t>(&g_tid_ComponentEntity));
}
