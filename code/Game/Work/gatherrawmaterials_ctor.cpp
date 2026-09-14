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

#include "BuildActions.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstdint>

#include "Object/factory_registration.h"

// Base Work ctor. RCX=self, RDX=InternalBasePtr* (reads *param2 as the entity ptr).
typedef void* (*GRMC_WorkCtor25_t)(void* self, void* ibp);              // 0x1400f72e0
// FindComponent that returns the BuildComponent (holds BuildDescription at +0x18).
typedef void  (*GRMC_FindCompUnk_t)(uintptr_t entity, void* retIBP);    // 0x14000d6c0
// FindComponent that returns the site StorageComponent (type 0x1401c7e78).
typedef void* (*GRMC_FindCompStorage_t)(uintptr_t entity, void* retIBP); // 0x140006270
// 0x1400021d0 (int-array SetSize) is the exe-side Collection::Array<int32>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its two call sites below to
// GameColl::AsArray<GameColl::I32> member calls, so its resolved-pointer stand-in
// is gone.

// "BuildDescription" type-id (DWORD_1401c7040). Name verified via the .rdata
// writer BuildDescription_Register (0x14014e5f5). GetTypeIdByName in standalone
// (the raw .rdata global is null).
static uint32_t GRMC_BuildDescTypeId() {
    static uint32_t id = 0;
    if (id != 0 && id != 0xFFFFFFFFu) return id;
    id = ::Object::GetTypeIdByName("BuildDescription");
    return id;
}

extern "C" void* GatherRawMaterials_Ctor_Impl(void* mem, void* buildEntityIBP) {
    if (!mem) return mem;
    uintptr_t self = reinterpret_cast<uintptr_t>(mem);

    // --- Base Work ctor with a temp IBP holding the raw build-entity pointer ---
    // FLAG: Work_Ctor25 (0x1400f72e0) is NOT registered for standalone, so
    // ResolveAddr returns 0 and the Work base (+0x10..+0x40) is left
    // uninitialised. The lead should register 0x1400f72e0 so the base fields
    // (entity ptr, work-cell, sentinels) are set.
    void* entityRaw = buildEntityIBP ? *reinterpret_cast<void**>(buildEntityIBP) : nullptr;
    GRMC_WorkCtor25_t workCtor =
        reinterpret_cast<GRMC_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    if (workCtor) {
        void* tmpIbp = entityRaw;   // Work_Ctor25 reads *param2 as the entity ptr
        workCtor(mem, &tmpIbp);
    }

    // --- Set derived vtable (Work_Ctor25 set it to vtable_Work; override) ---
    uintptr_t vt = ResolveAddr(Module::Exe, 0x140161420);
    if (vt) *reinterpret_cast<uintptr_t*>(self) = vt;

    // --- Zero GatherRawMaterials fields (matches original, 8 bytes each) ---
    *reinterpret_cast<uint64_t*>(self + 0x48) = 0;   // int[] ptr
    *reinterpret_cast<uint64_t*>(self + 0x50) = 0;   // count + capacity
    *reinterpret_cast<uint64_t*>(self + 0x58) = 0;   // int[] ptr
    *reinterpret_cast<uint64_t*>(self + 0x60) = 0;   // count + capacity
    *reinterpret_cast<uint64_t*>(self + 0x68) = 0;   // BST root
    *reinterpret_cast<uint32_t*>(self + 0x70) = 0;   // BST count

    // --- Locate BuildComponent (-> BuildDescription at +0x18) on the build entity ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(buildEntityIBP);
    uintptr_t buildCompIBP = 0;
    GRMC_FindCompUnk_t findComp =
        reinterpret_cast<GRMC_FindCompUnk_t>(ResolveAddr(Module::Exe, 0x14000d6c0));
    if (findComp && buildEntityIBP)
        findComp(*reinterpret_cast<uintptr_t*>(buildEntityIBP), &buildCompIBP);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&buildCompIBP);
    void* descObj = buildCompIBP ? *reinterpret_cast<void**>(buildCompIBP + 0x18) : nullptr;
    uint32_t descTid = GRMC_BuildDescTypeId();
    void* desc = nullptr;
    if (descObj && fn_IsDerivedFrom && descTid && fn_IsDerivedFrom(descObj, descTid))
        desc = descObj;

    // --- Locate the site StorageComponent ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(buildEntityIBP);
    uintptr_t storage = 0;
    GRMC_FindCompStorage_t findStorage =
        reinterpret_cast<GRMC_FindCompStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    if (findStorage && buildEntityIBP)
        findStorage(*reinterpret_cast<uintptr_t*>(buildEntityIBP), &storage);

    // --- Size the two parallel int arrays to the material count ---
    // FLAG: original reads desc+0x28 unconditionally (assumes desc valid). We guard
    // desc==null -> matCount 0 so the arrays stay empty rather than deref null.
    int matCount = desc ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x28) : 0;
    // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize.
    GameColl::AsArray<GameColl::I32>(reinterpret_cast<void*>(self + 0x48)).SetSize(matCount);
    GameColl::AsArray<GameColl::I32>(reinterpret_cast<void*>(self + 0x58)).SetSize(matCount);

    // --- Seed field_48[i] = amount already present in site storage, field_58[i] = 0 ---
    int count50 = *reinterpret_cast<int*>(self + 0x50);   // == matCount after resize
    uintptr_t matArr = desc ? *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20) : 0;

    for (int i = 0; i < count50; i++) {
        void* matType = matArr ? *reinterpret_cast<void**>(matArr + static_cast<uintptr_t>(i) * 0x10)
                               : nullptr;

        // delivered = matching site-storage slot (total - reserved), else 0.
        int delivered = 0;
        if (storage) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storage);
            int slotCount = *reinterpret_cast<int*>(storage + 0x30);
            uintptr_t* slots = *reinterpret_cast<uintptr_t**>(storage + 0x28);
            if (slots) {
                for (int s = 0; s < slotCount; s++) {
                    if (reinterpret_cast<void*>(slots[s * 2]) == matType) {
                        int total    = static_cast<int>(slots[s * 2 + 1]);         // +0x08
                        int reserved = *reinterpret_cast<int*>(
                            reinterpret_cast<uintptr_t>(slots) +
                            static_cast<uintptr_t>(s) * 0x10 + 0xc);               // +0x0c
                        delivered = total - reserved;
                        break;
                    }
                }
            }
        }

        // NOTE: the exe stores the *current storage amount* into field_48[i] (NOT a
        // costQty shortfall) — verified from disassembly. field_58[i] starts at 0.
        uintptr_t arr48 = *reinterpret_cast<uintptr_t*>(self + 0x48);
        uintptr_t arr58 = *reinterpret_cast<uintptr_t*>(self + 0x58);
        if (arr48) *reinterpret_cast<int*>(arr48 + static_cast<uintptr_t>(i) * 4) = delivered;
        if (arr58) *reinterpret_cast<int*>(arr58 + static_cast<uintptr_t>(i) * 4) = 0;
    }

    return mem;
}
