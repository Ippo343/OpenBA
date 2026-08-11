// code/Game/Work/tendarea_ctor.cpp
// TendArea "from entity" constructor — retail TendArea_CopyCtor (0x1400f5a80).
//
// WHY A SEPARATE FILE: tendarea.cpp hosts the TendArea vtable and every slot body, but
// not this constructor — the same split plantorchard_ctor.cpp exists for. It is the one
// unported leaf in OrchardComponent state 10's closure: both the state-10 enter
// (0x140050e30) and its per-frame seasonal re-post (0x140050c00) build the tending work
// item with `TendArea_CopyCtor(MemAlloc(0x60, 8), &orchard->+0x10, 1)`.
//
// ABI, read from the disassembly at 0x1400f5a80 (not inferred):
//   TendArea* __fastcall TendArea_CtorFromEntity(void* mem /*RCX*/,
//                                                void** entityIBP /*RDX*/,
//                                                int kind /*R8D*/)
//   MOV RAX,[RDX] ; MOV [RSP+0x48],RAX ; LEA RDX,[RSP+0x48] ; CALL 0x1400f72e0
//   LEA RAX,[0x140161990] ; MOV [RDI],RAX        ; vtable
//   XOR ECX,ECX ; MOV [RDI+0x48],RCX ; MOV [RDI+0x50],ECX ; MOV [RDI+0x58],EBX
//
// Retail copies the incoming IBP to a local FIRST and hands Work_Ctor25 the address of
// that local, because Work_Ctor25 takes an InternalBasePtr* rather than a raw pointer.
// Reproduced exactly: passing the caller's `entityIBP` through would hand it the
// caller's storage.

#include "tendarea_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

namespace {

typedef void* (*fn_Work_Ctor25_t)(void* self, void* entityIBP);   // 0x1400f72e0

fn_Work_Ctor25_t s_WorkCtor25 = nullptr;
uintptr_t        s_vtable     = 0;
bool             s_resolved   = false;

void Resolve() {
    if (s_resolved) return;
    s_WorkCtor25 = reinterpret_cast<fn_Work_Ctor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_vtable     = ResolveAddr(Module::Exe, 0x140161990);
    s_resolved   = s_WorkCtor25 && s_vtable;
    if (!s_resolved) {
        static bool reported = false;
        if (!reported) {
            reported = true;
            std::fprintf(stderr, "[TendAreaCtor] UNRESOLVED:%s%s\n",
                         s_WorkCtor25 ? "" : " Work_Ctor25",
                         s_vtable     ? "" : " vtable_TendArea (0x140161990)");
            std::fflush(stderr);
        }
    }
}

}  // namespace

extern "C" void* TendArea_CtorFromEntity(void* mem, void** entityIBP, int kind) {
    Resolve();
    // [STANDALONE] tendarea.cpp's own function-pointer table is filled by
    // TendArea_ResolveFuncs(), which is only called from its two FACTORY entry points.
    // This is the state machine's path — it allocates and builds the object directly — so
    // without this call every s_fn_* over there stays null and the first slot the work
    // scheduler dispatches on the fresh object calls through 0. That is exactly the bug
    // PlantOrchard hit on this same shape (commit 7731647); the class whose lazy Resolve()
    // lives only in its factory is dead on any hand-rolled ctor path.
    TendArea_ResolveFuncs();
    if (!mem) return mem;

    uintptr_t s = reinterpret_cast<uintptr_t>(mem);

    void* entityLocal = entityIBP ? *entityIBP : nullptr;   // retail's local copy
    if (s_WorkCtor25) s_WorkCtor25(mem, &entityLocal);
    if (s_vtable) *reinterpret_cast<uintptr_t*>(s) = s_vtable;

    *reinterpret_cast<uint64_t*>(s + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x50) = 0;
    *reinterpret_cast<int32_t*> (s + 0x58) = kind;
    return mem;
}
