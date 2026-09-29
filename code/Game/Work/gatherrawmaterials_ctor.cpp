// code/Game/Work/gatherrawmaterials_ctor.cpp
// Clean C++ port of GatherRawMaterials_Ctor @ 0x1400ec710.
//
// Constructs a GatherRawMaterials Work (size 0x78, vtable 0x140161420) over a
// caller-allocated 0x78 block, given the build-site entity InternalBasePtr.
// The exe calls it as: GatherRawMaterials_Ctor(MemAlloc(0x78), &buildEntityIBP).
//
// Layout produced (see engine/types/BuildActions.h : GatherRawMaterials):
//   +0x00 vtable = 0x140161420
//   +0x10..0x40 Work base (initialised by Work_Ctor25 @0x1400f72e0)
#include "Work/exe_bodies.h"
//   +0x48 int[] ptr  / +0x50 count(=matCount)  "amount already present per material"
//   +0x58 int[] ptr  / +0x60 count(=matCount)  "delivered per material" (all 0)
//   +0x68 BST root (0) / +0x70 count (0)
//
// For each material in the build entity's BuildDescription, field_48[i] is seeded
// with the amount ALREADY in the site StorageComponent (slot.total - slot.reserved),
// and field_58[i] with 0.  (Execute() treats field_48 as the per-material
// already-assigned/present count and field_58 as the in-flight build count.)
//
// Faithful offset-by-offset port. Verified against Ghidra disassembly of
// 0x1400ec710 and callees: Work_Ctor25 @0x1400f72e0, FUN_14000d6c0 @0x14000d6c0
// (find BuildComponent), Entity_FindComponent_Storage @0x140006270,
// FUN_1400021d0 @0x1400021d0 (int-array resize; sets ptr@+0 and count@+8).
//
// Consistent with the GatherRawMaterials struct + field conventions used by
// code/Game/Work/gatherrawmaterials.cpp (which we do NOT modify).

#include "Work/GatherRawMaterials.h"
#include "Work/gatherrawmaterials_funcs.h"
#include <new>
#include "object/TypeInfo.h"
#include "Dialog/dialog_exe_helpers.h"   // B3R: Dialog_FindComponent_d6c0
#include "Component/coll_bridge.h"
#include <cstdint>
#include "Component/BuildDescription.h"   // ENDGAME B.3.8 CB3: BuildDescription::TypeId()
#include "Component/BuildComponent.h"
#include "Component/StorageComponent.h"
#include "Entity/ComponentEntity.h"

#include "Object/factory_registration.h"

// FindComponent that returns the BuildComponent (holds BuildDescription at +0x18):
// 0x14000d6c0 Dialog_FindComponent_d6c0 (Dialog/dialog_exe_helpers.h).
// FindComponent that returns the site StorageComponent (type 0x1401c7e78):
// 0x140006270 WorkBase_FindComponent_6270 (Work/exe_bodies.h). Both called by name (B3R).
// 0x1400021d0 (int-array SetSize) is the exe-side Collection::Array<int32>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its two call sites below to
// GameColl::AsArray<GameColl::I32NoInit> member calls, so its resolved-pointer stand-in
// is gone.

// "BuildDescription" type-id (DWORD_1401c7040). Name verified via the .rdata
// writer BuildDescription_Register (0x14014e5f5). GetTypeIdByName in standalone
// (the raw .rdata global is null).
// ENDGAME B.3.8: read per call (the function-local lazy cache is gone, B38 D7).
static uint32_t GRMC_BuildDescTypeId() {
    return BuildDescription::TypeId();
}

// The base ctor is Work(buildEntityIBP) = Work_Ctor25 (0x1400f72e0) on a temp IBP
// holding the raw build-entity pointer (it reads *param only, into field_10); the two
// stamps are the compiler's.
GatherRawMaterials::GatherRawMaterials(void* buildEntityIBP) : Work(buildEntityIBP) {
    // --- Zero GatherRawMaterials fields (retail: 8 bytes each; here the named fields in
    // address order) ---
    field_48 = nullptr;   // int[] ptr
    field_50 = 0;         // count + capacity
    field_54 = 0;
    field_58 = nullptr;   // int[] ptr
    field_60 = 0;         // count + capacity
    field_64 = 0;
    field_68 = nullptr;   // BST root
    field_70 = 0;         // BST count

    // --- Locate BuildComponent (-> BuildDescription at +0x18) on the build entity ---
    // (the entity is *buildEntityIBP, which the Work base stored in field_10)
    BuildComponent* buildComp = nullptr;
    if (buildEntityIBP)
        buildComp = field_10->FindComponent<BuildComponent>();

    Description* descObj = buildComp ? buildComp->_description : nullptr;
    uint32_t descTid = GRMC_BuildDescTypeId();
    BuildDescription* desc = nullptr;
    if (descObj && descTid && Object::IsDerivedFromNullSafe(descObj, descTid))
        desc = static_cast<BuildDescription*>(descObj);

    // --- Locate the site StorageComponent ---
    StorageComponent* storage = nullptr;
    if (buildEntityIBP)
        storage = field_10->FindComponent<StorageComponent>();

    // --- Size the two parallel int arrays to the material count ---
    // FLAG: original reads desc+0x28 unconditionally (assumes desc valid). We guard
    // desc==null -> matCount 0 so the arrays stay empty rather than deref null.
    int matCount = desc ? desc->_costCount : 0;
    // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize (retail
    // GatherRawMaterials_Ctor calls it at 1400ec7e0 and 1400ec7ed). I32NoInit — a grown
    // tail is left as the allocation found it, like retail. Safe: the seeding loop right
    // below (retail 1400ec800..1400ec87f) writes BOTH arrays at every index i < +0x50
    // before either is read.
    GameColl::AsArray<GameColl::I32NoInit>(&field_48).SetSize(matCount);
    GameColl::AsArray<GameColl::I32NoInit>(&field_58).SetSize(matCount);

    // --- Seed field_48[i] = amount already present in site storage, field_58[i] = 0 ---
    int count50 = field_50;   // == matCount after resize
    const BuildCost* matArr = desc ? desc->_costs : nullptr;

    for (int i = 0; i < count50; i++) {
        ComponentDescription* matType = matArr ? matArr[i].material : nullptr;

        // delivered = matching site-storage slot (total - reserved), else 0.
        int delivered = 0;
        if (storage) {
            int slotCount = storage->slotCount;
            const StorageComponent::Item* slots = storage->storageSlots;
            if (slots) {
                for (int s = 0; s < slotCount; s++) {
                    if (slots[s]._resource == matType) {
                        int total    = slots[s]._count;     // +0x08
                        int reserved = slots[s]._reserve;   // +0x0c
                        delivered = total - reserved;
                        break;
                    }
                }
            }
        }

        // NOTE: the exe stores the *current storage amount* into field_48[i] (NOT a
        // costQty shortfall) — verified from disassembly. field_58[i] starts at 0.
        if (field_48) field_48[i] = delivered;
        if (field_58) field_58[i] = 0;
    }

}
// The by-address door (0x1400ec710, standalone/reg_gatherrawmaterials_ctor.cpp; also
// named by buildcomponent_state1.cpp): the real ctor on caller memory (B8-5).
extern "C" void* GatherRawMaterials_Ctor_Impl(void* mem, void* buildEntityIBP) {
    if (!mem) return mem;
    return static_cast<Object::Base*>(new (mem) GatherRawMaterials(buildEntityIBP));
}
