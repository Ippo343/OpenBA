// code/Game/Work/plantorchard_ctor.cpp
// PlantOrchard "from entity" constructor — retail 0x1400f5140.
//
// WHY A SEPARATE FILE: plantorchard.cpp already hosts the PlantOrchard vtable and every
// slot body, but NOT this constructor. It is the single unported leaf in state 9's
// transitive closure — the OrchardComponent state machine's state 9 and state 12 enter
// handlers both allocate 0x70 bytes and call this to build the planting work item. Kept
// out of plantorchard.cpp only to avoid an edit collision while that file is in use.
//
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

#include "BuildActions.h"
#include "plantorchard_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

namespace {

typedef void* (*fn_Work_Ctor25_t)(void* self, void* entityIBP);       // 0x1400f72e0
typedef void* (*fn_FindOrchard_t)(int64_t entity, void* outIBP);      // 0x140005440
typedef void  (*fn_ValidateRefs_t)(int64_t orchardComp);              // 0x140051830
typedef void  (*fn_ResizeBytes_t)(void* arrayTriple, int newCount);   // 0x14000a590

fn_Work_Ctor25_t  s_WorkCtor25   = nullptr;
fn_FindOrchard_t  s_FindOrchard  = nullptr;
fn_ValidateRefs_t s_ValidateRefs = nullptr;
fn_ResizeBytes_t  s_ResizeBytes  = nullptr;
uintptr_t         s_vtable       = 0;
bool              s_resolved     = false;

void Resolve() {
    if (s_resolved) return;
    s_WorkCtor25   = reinterpret_cast<fn_Work_Ctor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_FindOrchard  = reinterpret_cast<fn_FindOrchard_t>(ResolveAddr(Module::Exe, 0x140005440));
    s_ValidateRefs = reinterpret_cast<fn_ValidateRefs_t>(ResolveAddr(Module::Exe, 0x140051830));
    s_ResizeBytes  = reinterpret_cast<fn_ResizeBytes_t>(ResolveAddr(Module::Exe, 0x14000a590));
    s_vtable       = ResolveAddr(Module::Exe, PlantOrchardAddr::Vtable);
    s_resolved = s_WorkCtor25 && s_FindOrchard && s_ValidateRefs && s_ResizeBytes && s_vtable;
    if (!s_resolved) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[PlantOrchardCtor] UNRESOLVED:%s%s%s%s%s\n",
                s_WorkCtor25   ? "" : " Work_Ctor25",
                s_FindOrchard  ? "" : " FindComponent_Orchard",
                s_ValidateRefs ? "" : " OrchardComponent_ValidateRefs",
                s_ResizeBytes  ? "" : " ResizeByteArray",
                s_vtable       ? "" : " vtable_PlantOrchard");
            std::fflush(stderr);
        }
    }
}

}  // namespace

// The diag counter is bumped in the FACTORY ctor only, so this path — the state machine's,
// and the only one that actually runs in a live game — reported "constructed=0" while
// building work items every time. Count here too, or the counter reads as a failure of the
// thing it is meant to be measuring.
extern "C" void OrchardDiag_NotePlantConstructed();

extern "C" void* PlantOrchard_CtorFromEntity(void* mem, void** entityIBP, void* treeType) {
    Resolve();
    // [STANDALONE] plantorchard.cpp's own function-pointer table is filled by
    // PlantOrchard_ResolveFuncs(), which until now was only called from the two FACTORY
    // entry points (the deserialize path). This constructor is the state machine's path —
    // it allocates and builds the object directly — so every s_fn_* over there stayed null.
    // The first slot the framework dispatches on the fresh object is GetTarget (+0x50),
    // called by the work scheduler's insert; it calls s_fn_FindSubEntity(entity, out) with
    // no guard, so a null there is a call-of-0 with the ORCHARD ENTITY in RCX — which is
    // exactly the crash this path produced. Resolve here so both creation paths arrive at a
    // fully-wired object. See reference_vcall_of_0_may_be_a_null_fnptr.
    PlantOrchard_ResolveFuncs();
    if (!mem) return mem;
    OrchardDiag_NotePlantConstructed();

    uintptr_t s = reinterpret_cast<uintptr_t>(mem);

    // Retail's two locals, in retail's order.
    void* entityLocal   = entityIBP ? *entityIBP : nullptr;
    void* treeTypeLocal = treeType;

    if (s_WorkCtor25) s_WorkCtor25(mem, &entityLocal);
    if (s_vtable) *reinterpret_cast<uintptr_t*>(s) = s_vtable;

    // PlantOrchard's own fields. +0x60 is written as one 8-byte store, which clears BOTH
    // the count (+0x60) and the capacity (+0x64) of the byte-array triple at +0x58.
    *reinterpret_cast<uint64_t*>(s + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x60) = 0;
    *reinterpret_cast<void**>(s + 0x68)    = treeTypeLocal;   // tree type, stored verbatim:
                                                              // no null test, no validation,
                                                              // no refcount — as retail does.

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    int64_t orchardIBP = 0;
    if (s_FindOrchard) s_FindOrchard(*reinterpret_cast<int64_t*>(s + 0x10), &orchardIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&orchardIBP);
    if (!orchardIBP) return mem;      // SA: retail derefs unguarded below; a PlantOrchard is
                                      // only ever built for an entity that has an orchard, so
                                      // this can only fire if our caller is wrong. Returning
                                      // early keeps the work item well-formed but empty
                                      // rather than faulting on a null component.

    if (s_ValidateRefs) s_ValidateRefs(orchardIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&orchardIBP);

    // Size the per-slot validity array from the orchard's CURRENT slot count.
    const int slotCount = *reinterpret_cast<int32_t*>(orchardIBP + 0x58);
    if (s_ResizeBytes) s_ResizeBytes(reinterpret_cast<void*>(s + 0x58), slotCount);

    // Zero each byte. Retail re-reads orchardComp->+0x58 and re-runs CheckPtr on every
    // iteration; the re-read is preserved because ValidateRefs/CheckPtr can in principle
    // shrink the component's slot array underneath the loop.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&orchardIBP);
    for (int i = 0; i < *reinterpret_cast<int32_t*>(orchardIBP + 0x58); ++i) {
        uint8_t* base = *reinterpret_cast<uint8_t**>(s + 0x58);
        if (!base) break;
        base[i] = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&orchardIBP);
    }
    return mem;
}
