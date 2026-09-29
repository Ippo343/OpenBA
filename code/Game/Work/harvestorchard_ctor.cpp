// code/Game/Work/harvestorchard_ctor.cpp
// HarvestOrchard "from entity" constructor — retail HarvestOrchard_Ctor (0x1400ee060).
//
// WHY A SEPARATE FILE: harvestorchard.cpp hosts the vtable and every slot body but not
// this constructor — the same split plantorchard_ctor.cpp / tendarea_ctor.cpp exist for.
// It is the last unported leaf in OrchardComponent state 11's closure: the state-11 enter
// (0x140050800) builds the harvest work item with
// `HarvestOrchard_Ctor(MemAlloc(0x68, 8), &orchard->+0x10)` — note NO tree-type argument,
// unlike PlantOrchard_Ctor.
#include "Work/exe_bodies.h"
//
// ABI, read from the decompile at 0x1400ee060 (not inferred):
//   HarvestOrchard* __fastcall HarvestOrchard_CtorFromEntity(void* mem /*RCX*/,
//                                                            void** entityIBP /*RDX*/)
// Retail copies the incoming IBP to a local FIRST and hands Work_Ctor25 the address of
// that local, because Work_Ctor25 takes an InternalBasePtr* rather than a raw pointer.
//
// The tail is what makes the harvest work item meaningful: it sizes its own per-slot byte
// array from the orchard's CURRENT slot count and marks 1 for every slot that has a tree
// bound (orchard slot +0x08 non-null), 0 for every hole. That byte array is the harvester's
// worklist.
//
// ⚠ Retail writes the vtable TWICE — first vtable_AutoPickupWork, then
// vtable_HarvestOrchard. That is the inlined base constructor, not a bug; reproduced.

#include "Work/HarvestOrchard.h"
#include "harvestorchard_funcs.h"
#include <new>
#include "object/TypeInfo.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include "Component/OrchardComponent.h"
#include "Entity/ComponentEntity.h"

namespace {
// 0x140005440 is WorkAcc_FindComponent_5440 (Work/exe_bodies.h), called by name (B3R).
// 0x14000a590 (Array<uint8>::SetSize) is the exe-side Collection::Array<T>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::U8> member call, so its resolved-pointer stand-in
// is gone.
// ENDGAME B.3.8: the file-level lazy Resolve() is gone (B38 D7) — Work_Ctor25 is the
// base ctor Work(void*), the two stamps are the compiler's (the old AutoPickupWork
// stamp read 0x140161380, an RTTI descriptor, and was always skipped), and the finder
// is resolved where it is used.
}  // namespace

extern "C" void OrchardDiag_NoteHarvestConstructed();

// Retail copies the incoming IBP to a local and hands Work_Ctor25 its address; Work(void*)
// reads the pointer once, so passing the caller's IBP is the same read.
HarvestOrchard::HarvestOrchard(void** entityIBP) : AutoPickupWork(entityIBP) {
    _field48 = nullptr;
    _field50 = 0;
    _field58 = nullptr;   // byte-array base
    _field60 = 0;         // count (+0x60) + capacity (+0x64): one qword store in retail
    _field64 = 0;

    // The entity's OrchardComponent (0x140005440)
    OrchardComponent* orchard = field_10->FindComponent<OrchardComponent>();
    if (!orchard) return;             // SA: retail derefs unguarded below; a HarvestOrchard
                                      // is only ever built for an entity that has an
                                      // orchard, so this can only fire if our caller is
                                      // wrong. Same guard plantorchard_ctor.cpp carries.

    // B.3.2 bridge: FUN_14000a590 == Collection::Array<uint8>::SetSize.
    GameColl::AsArray<GameColl::U8>(&_field58).SetSize(orchard->slotCount);

    // One byte per orchard slot: 1 = there is a tree here to pick, 0 = hole. Retail
    // re-runs CheckPtr on the orchard IBP every iteration and loops on the WORK item's
    // own count (+0x60) — the value the resize just produced — not the component's.
    const int n = _field60;
    for (int i = 0; i < n; ++i) {
        bool* base = _field58;
        if (!base) break;
        OrchardSlot* slots = orchard->slots;
        base[i] = (slots && slots[i].tree != nullptr) ? 1 : 0;
    }
}

// The door OrchardComponent's state 11 calls (orchardcomponent_states.cpp, block MI-B):
// the real ctor on caller memory (B8-5).
extern "C" void* HarvestOrchard_CtorFromEntity(void* mem, void** entityIBP) {
    if (!mem) return mem;
    OrchardDiag_NoteHarvestConstructed();
    return static_cast<Object::Base*>(new (mem) HarvestOrchard(entityIBP));
}
