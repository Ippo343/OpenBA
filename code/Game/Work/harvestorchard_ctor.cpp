// code/Game/Work/harvestorchard_ctor.cpp
// HarvestOrchard "from entity" constructor — retail HarvestOrchard_Ctor (0x1400ee060).
//
// WHY A SEPARATE FILE: harvestorchard.cpp hosts the vtable and every slot body but not
// this constructor — the same split plantorchard_ctor.cpp / tendarea_ctor.cpp exist for.
// It is the last unported leaf in OrchardComponent state 11's closure: the state-11 enter
// (0x140050800) builds the harvest work item with
// `HarvestOrchard_Ctor(MemAlloc(0x68, 8), &orchard->+0x10)` — note NO tree-type argument,
// unlike PlantOrchard_Ctor.
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

#include "harvestorchard_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>

namespace {

typedef void* (*fn_Work_Ctor25_t)(void* self, void* entityIBP);       // 0x1400f72e0
typedef void* (*fn_FindOrchard_t)(int64_t entity, void* outIBP);      // 0x140005440
// 0x14000a590 (Array<uint8>::SetSize) is the exe-side Collection::Array<T>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::U8> member call, so its resolved-pointer stand-in
// is gone.

fn_Work_Ctor25_t s_WorkCtor25  = nullptr;
fn_FindOrchard_t s_FindOrchard = nullptr;
uintptr_t        s_vtableHarvest    = 0;   // 0x140161520
uintptr_t        s_vtableAutoPickup = 0;   // 0x140161380
bool             s_resolved    = false;

void Resolve() {
    if (s_resolved) return;
    s_WorkCtor25       = reinterpret_cast<fn_Work_Ctor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_FindOrchard      = reinterpret_cast<fn_FindOrchard_t>(ResolveAddr(Module::Exe, 0x140005440));
    s_vtableHarvest    = ResolveAddr(Module::Exe, 0x140161520);
    s_vtableAutoPickup = ResolveAddr(Module::Exe, 0x140161380);
    s_resolved = s_WorkCtor25 && s_FindOrchard && s_vtableHarvest;
    if (!s_resolved) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[HarvestOrchardCtor] UNRESOLVED:%s%s%s\n",
                s_WorkCtor25    ? "" : " Work_Ctor25",
                s_FindOrchard   ? "" : " FindComponent_Orchard",
                s_vtableHarvest ? "" : " vtable_HarvestOrchard (0x140161520)");
            std::fflush(stderr);
        }
    }
}

}  // namespace

extern "C" void OrchardDiag_NoteHarvestConstructed();

extern "C" void* HarvestOrchard_CtorFromEntity(void* mem, void** entityIBP) {
    Resolve();
    // [STANDALONE] harvestorchard.cpp's own function-pointer table is filled by
    // HarvestOrchard_ResolveFuncs(), which is only called from its two FACTORY entry
    // points (the deserialize path). This is the state machine's path — it allocates and
    // builds the object directly — so without this call every s_fn_* over there stays
    // null and the first slot the scheduler dispatches on the fresh object calls through
    // 0. Exactly the crash PlantOrchard hit on this same shape (commit 7731647).
    HarvestOrchard_ResolveFuncs();
    if (!mem) return mem;
    OrchardDiag_NoteHarvestConstructed();

    uintptr_t s = reinterpret_cast<uintptr_t>(mem);

    void* entityLocal = entityIBP ? *entityIBP : nullptr;   // retail's local copy
    if (s_WorkCtor25) s_WorkCtor25(mem, &entityLocal);
    if (s_vtableAutoPickup) *reinterpret_cast<uintptr_t*>(s) = s_vtableAutoPickup;
    if (s_vtableHarvest)    *reinterpret_cast<uintptr_t*>(s) = s_vtableHarvest;

    *reinterpret_cast<uint64_t*>(s + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x58) = 0;   // byte-array base
    *reinterpret_cast<uint64_t*>(s + 0x60) = 0;   // count (+0x60) + capacity (+0x64)

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    int64_t orchardIBP = 0;
    if (s_FindOrchard) s_FindOrchard(*reinterpret_cast<int64_t*>(s + 0x10), &orchardIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&orchardIBP);
    if (!orchardIBP) return mem;      // SA: retail derefs unguarded below; a HarvestOrchard
                                      // is only ever built for an entity that has an
                                      // orchard, so this can only fire if our caller is
                                      // wrong. Same guard plantorchard_ctor.cpp carries.

    // B.3.2 bridge: FUN_14000a590 == Collection::Array<uint8>::SetSize.
    GameColl::AsArray<GameColl::U8>(reinterpret_cast<void*>(s + 0x58))
        .SetSize(*reinterpret_cast<int32_t*>(orchardIBP + 0x58));

    // One byte per orchard slot: 1 = there is a tree here to pick, 0 = hole. Retail
    // re-runs CheckPtr on the orchard IBP every iteration and loops on the WORK item's
    // own count (+0x60) — the value the resize just produced — not the component's.
    const int n = *reinterpret_cast<int32_t*>(s + 0x60);
    for (int i = 0; i < n; ++i) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&orchardIBP);
        uint8_t* base = *reinterpret_cast<uint8_t**>(s + 0x58);
        if (!base) break;
        uintptr_t slots = *reinterpret_cast<uintptr_t*>(orchardIBP + 0x50);
        base[i] = (slots && *reinterpret_cast<uint64_t*>(
                       slots + static_cast<uintptr_t>(i) * 0x28 + 8) != 0) ? 1 : 0;
    }
    return mem;
}
