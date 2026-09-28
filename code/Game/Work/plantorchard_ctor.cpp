// code/Game/Work/plantorchard_ctor.cpp
// PlantOrchard "from entity" constructor — retail 0x1400f5140.
//
// WHY A SEPARATE FILE: plantorchard.cpp already hosts the PlantOrchard vtable and every
// slot body, but NOT this constructor. It is the single unported leaf in state 9's
// transitive closure — the OrchardComponent state machine's state 9 and state 12 enter
// handlers both allocate 0x70 bytes and call this to build the planting work item. Kept
// out of plantorchard.cpp only to avoid an edit collision while that file is in use.
//
#include "Work/exe_bodies.h"
// ⚠ NOT to be confused with PlantOrchardAddr::Ctor (0x1400f50b0), which is the zero-arg
// deserialize/factory constructor and leaves +0x68 null. See the note in BuildActions.h.
//
// ABI (verified against the disassembly, not inferred):
//   PlantOrchard* __fastcall PlantOrchard_CtorFromEntity(
//       void* mem /*RCX*/, void** entityIBP /*RDX*/, void* treeType /*R8*/)
// Called as PlantOrchard_CtorFromEntity(MemAlloc(0x70, 8), &orchard->+0x10,
//                                      *(orchard+0x60)).
//
// Retail copies both incoming arguments to locals FIRST (`local_res20 = *param_2;
// local_res18 = param_3`) and hands Work_Ctor25 the address of that local, because
// Work_Ctor25 takes an InternalBasePtr* rather than a raw pointer. Reproduced exactly:
// passing &entityIBP straight through would hand it the caller's storage.

#include "Work/PlantOrchard.h"
#include "plantorchard_funcs.h"
#include <new>
#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>

namespace {
typedef void* (*fn_FindOrchard_t)(int64_t entity, void* outIBP);      // 0x140005440
typedef void  (*fn_ValidateRefs_t)(int64_t orchardComp);              // 0x140051830
// 0x14000a590 (Array<uint8>::SetSize) is the exe-side Collection::Array<T>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::U8> member call, so its resolved-pointer stand-in
// is gone.
// ENDGAME B.3.8: the file-level lazy Resolve() is gone (B38 D7) — Work_Ctor25 is the
// base ctor Work(void*), the stamp is the compiler's, and the two helpers are resolved
// where they are used.
}  // namespace

// The diag counter is bumped in the FACTORY ctor only, so this path — the state machine's,
// and the only one that actually runs in a live game — reported "constructed=0" while
// building work items every time. Count here too, or the counter reads as a failure of the
// thing it is meant to be measuring.
extern "C" void OrchardDiag_NotePlantConstructed();

// Retail copies the incoming IBP to a local and hands Work_Ctor25 its address; Work(void*)
// reads the pointer once, so passing the caller's IBP is the same read.
PlantOrchard::PlantOrchard(void** entityIBP, void* treeType) : Work(entityIBP) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // PlantOrchard's own fields. +0x60 is written as one 8-byte store, which clears BOTH
    // the count (+0x60) and the capacity (+0x64) of the byte-array triple at +0x58.
    *reinterpret_cast<uint64_t*>(s + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x60) = 0;
    *reinterpret_cast<void**>(s + 0x68)    = treeType;   // tree type, stored verbatim:
                                                         // no null test, no validation,
                                                         // no refcount — as retail does.

    auto findOrchard  = reinterpret_cast<fn_FindOrchard_t>(ResolveAddr(Module::Exe, 0x140005440));
    auto validateRefs = reinterpret_cast<fn_ValidateRefs_t>(ResolveAddr(Module::Exe, 0x140051830));
    int64_t orchardIBP = 0;
    if (findOrchard) findOrchard(*reinterpret_cast<int64_t*>(s + 0x10), &orchardIBP);
    if (!orchardIBP) return;          // SA: retail derefs unguarded below; a PlantOrchard is
                                      // only ever built for an entity that has an orchard, so
                                      // this can only fire if our caller is wrong. Returning
                                      // early keeps the work item well-formed but empty
                                      // rather than faulting on a null component.

    if (validateRefs) validateRefs(orchardIBP);

    // Size the per-slot validity array from the orchard's CURRENT slot count.
    // B.3.2 bridge: FUN_14000a590 == Collection::Array<uint8>::SetSize.
    const int slotCount = *reinterpret_cast<int32_t*>(orchardIBP + 0x58);
    GameColl::AsArray<GameColl::U8>(reinterpret_cast<void*>(s + 0x58)).SetSize(slotCount);

    // Zero each byte. Retail re-reads orchardComp->+0x58 and re-runs CheckPtr on every
    // iteration; the re-read is preserved because ValidateRefs/CheckPtr can in principle
    // shrink the component's slot array underneath the loop.
    for (int i = 0; i < *reinterpret_cast<int32_t*>(orchardIBP + 0x58); ++i) {
        uint8_t* base = *reinterpret_cast<uint8_t**>(s + 0x58);
        if (!base) break;
        base[i] = 0;
    }
}

// The door OrchardComponent's states 9 and 12 call (orchardcomponent_states.cpp, block
// MI-B): the real ctor on caller memory (B8-5).
extern "C" void* PlantOrchard_CtorFromEntity(void* mem, void** entityIBP, void* treeType) {
    // [STANDALONE] plantorchard.cpp's own function-pointer table is filled by
    // PlantOrchard_ResolveFuncs(). This is the state machine's path — it allocates and
    // builds the object directly — so wire it here, as before: the first slot the
    // framework dispatches on the fresh object is GetTarget (+0x50), which calls
    // s_fn_FindSubEntity with no guard. See reference_vcall_of_0_may_be_a_null_fnptr.
    PlantOrchard_ResolveFuncs();
    if (!mem) return mem;
    OrchardDiag_NotePlantConstructed();
    return static_cast<Object::Base*>(new (mem) PlantOrchard(entityIBP, treeType));
}
