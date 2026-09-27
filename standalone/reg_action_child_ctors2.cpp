// standalone/reg_action_child_ctors2.cpp
// Registration for the B3 leaf Action copy-ctors + ConsumeRawMaterial/PickupStored
// helpers (code/Game/Action/action_child_ctors2.cpp). Wired via RegisterActionChildCtors2Batch().
// Closure-safe (all sub-callees registered); faithful Ghidra-verified ports.
//
// (ENDGAME B.3.9 TR: the RawMaterialDescription / ConsumeProduce* / ComponentEntity
// type-id storage cells this file used to register and fill are gone; the readers use
// X::TypeId() of the class at the cell.)
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
}
