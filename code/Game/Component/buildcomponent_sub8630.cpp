// code/Game/Component/buildcomponent_sub8630.cpp
// Clean C++ port of BuildDescription_helper_sub_8630 @ 0x140108630.
//
// "Have all required construction materials been delivered to the site?"
// Returns true when, for every material in the BuildComponent's BuildDescription
// material list, the site StorageComponent holds at least (costQty * multiplier)
// units; returns false as soon as any material falls short.
//
// This is the gate the state-1 enter handler uses:
//   false -> spawn a GatherRawMaterials work (keep hauling materials)
//   true  -> transition to the build state.
//
// Faithful offset-by-offset port. Verified against Ghidra disassembly of
// 0x140108630 and its callees (Entity_FindComponent_Storage @0x140006270,
// InternalBasePtr::CheckPtr, Object::Base::IsDerivedFrom).
//
// BuildComponent layout used here:
//   +0x10  entity InternalBasePtr (site entity, owns the site StorageComponent)
//   +0x18  BuildDescription*  (type "BuildDescription", DWORD_1401c7040)
//   +0x4c  int  material multiplier  (required = costQty * multiplier)
// BuildDescription layout:
//   +0x20  material array base  { void* rawMaterial @0x00, int costQty @0x08 } stride 0x10
//   +0x28  int  material count
// StorageComponent (site) layout:
//   +0x28  slot array base      { void* type @0x00, int total @0x08, int reserved @0x0c } stride 0x10
//   +0x30  int  slot count
//   delivered(material) = slot.total - slot.reserved for the matching slot type
//

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Object/factory_registration.h"

// Entity_FindComponent_Storage @ 0x140006270 — returns the site StorageComponent
// (type-id 0x1401c7e78) into the retIBP buffer. Registered in standalone
// (reg_work_base.cpp -> WorkBase_FindComponent_6270).
typedef void* (*BC8630_FindCompStorage_t)(uintptr_t entity, void* retIBP);

// Type-id for "BuildDescription" (DWORD_1401c7040). The .rdata BaseTypeInfo is
// created by BuildDescription_Register (0x14014e5f5):
//   Object::BaseTypeInfo::BaseTypeInfo(&DWORD_1401c7040,"BuildDescription",...)
// so the name is verified. Read via GetTypeIdByName (the raw
// .rdata global resolves to null in the standalone exe).
static uint32_t BC8630_BuildDescTypeId() {
    static uint32_t id = 0;
    if (id != 0 && id != 0xFFFFFFFFu) return id;
    id = ::Object::GetTypeIdByName("BuildDescription");
    return id;
}

extern "C" bool BuildComponent_HaveAllMaterials(void* buildComp) {
    if (!buildComp) return false;
    uintptr_t self = reinterpret_cast<uintptr_t>(buildComp);

    // --- BuildDescription at BuildComponent+0x18, type-checked ---
    void* descObj = *reinterpret_cast<void**>(self + 0x18);
    uint32_t descTid = BC8630_BuildDescTypeId();
    void* desc = nullptr;
    if (descObj && fn_IsDerivedFrom && descTid && fn_IsDerivedFrom(descObj, descTid))
        desc = descObj;
    // Original derefs desc unconditionally; if desc were null it would loop over
    // count at desc+0x28 (crash). Guard: no BuildDescription => no required
    // materials => treat as "all delivered".
    if (!desc) return true;
    uintptr_t descAddr = reinterpret_cast<uintptr_t>(desc);

    // --- Find the site StorageComponent via BuildComponent+0x10 entity IBP ---
    uintptr_t storage = 0;
    BC8630_FindCompStorage_t findStorage =
        reinterpret_cast<BC8630_FindCompStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    if (findStorage)
        findStorage(*reinterpret_cast<uintptr_t*>(self + 0x10), &storage);
    // FLAG: if findStorage is unavailable (unregistered exe addr in standalone),
    // storage stays 0, every material reads delivered=0, and we return false as
    // soon as a material has required>0 (i.e. keep gathering). Safe default — it
    // never falsely reports materials complete. 0x140006270 IS registered today.

    int matCount   = *reinterpret_cast<int*>(descAddr + 0x28);
    int multiplier = *reinterpret_cast<int*>(self + 0x4c);
    uintptr_t matArr = *reinterpret_cast<uintptr_t*>(descAddr + 0x20);

    for (int i = 0; i < matCount; i++) {
        uintptr_t matEntry = matArr + static_cast<uintptr_t>(i) * 0x10;
        void* matType = *reinterpret_cast<void**>(matEntry);       // +0x00 rawMaterial
        int   costQty = *reinterpret_cast<int*>(matEntry + 8);      // +0x08 costQty

        // Delivered amount = matching site-storage slot's (total - reserved), else 0.
        int delivered = 0;
        if (storage) {
            int slotCount = *reinterpret_cast<int*>(storage + 0x30);
            uintptr_t* slots = *reinterpret_cast<uintptr_t**>(storage + 0x28);
            if (slots) {
                for (int s = 0; s < slotCount; s++) {
                    if (reinterpret_cast<void*>(slots[s * 2]) == matType) {
                        int total    = static_cast<int>(slots[s * 2 + 1]);  // +0x08
                        int reserved = *reinterpret_cast<int*>(
                            reinterpret_cast<uintptr_t>(slots) +
                            static_cast<uintptr_t>(s) * 0x10 + 0xc);        // +0x0c
                        delivered = total - reserved;
                        break;
                    }
                }
            }
        }

        int required = costQty * multiplier;
        // Trace ONLY when the delivered amount CHANGES (this gate is polled every frame in
        // state 1; an unthrottled per-frame fflush floods the log and stalls the game while
        // the site waits for materials). Keyed by (buildComp,slot) so each site's progress
        // still shows, without the per-frame spam.
        if (delivered < required)
            return false;   // this material is not fully delivered yet
    }

    return true;   // every material satisfied (or matCount <= 0)
}
