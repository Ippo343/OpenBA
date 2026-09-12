// ============================================================================
// runtime/src/engine/closure_batch2b.cpp — cutover closure batch 2b
// ============================================================================
//
//   0x18004da00  Engine_Application_ScalarDeletingDtor   116 B  (unexported)
//
// `~Application` immediately followed by `operator delete(this)` — a SCALAR
// deleting destructor, and NOT a member of the `??_E` vector family
// runtime/src/object/deleting_dtor.cpp owns (no flags argument, no array
// cookie, no vector branch).
//
// runtime/src/engine/engine_lifecycle.cpp:972 already identified this address
// and works around it by calling the two halves separately
// (`??1Application@0@QEAA@XZ` then MemFree) — with a note that the exported
// destructor "writes the two adjacent zeroes in the opposite order to the
// inlined copy". This file ports the inlined copy itself, so that ordering
// difference stops being a departure anyone has to remember.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// The two Collection::Set primitives the inlined `~Application` uses on its
// member set at +0x18. Both are unexported internals reached by ADDRESS:
//   0x18000a4f0  the RECURSIVE subtree free — `if (n) { rec(n->left);
//                rec(n->right); freeNode(n); }`, left at +0x18 and right at
//                +0x20. It is its own caller, so porting it is a separate job.
//   0x180009c30  the single-node free.
void SetFreeSubtree(void* set, void* node) {
    using Fn = void (*)(void*, void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18000a4f0));
    fn(set, node);
}
void SetFreeNode(void* set, void* node) {
    using Fn = void (*)(void*, void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180009c30));
    fn(set, node);
}
void GuardedFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180006a10));
    fn(p);
}

}  // namespace

// ============================================================================
// Transcription, from the body:
//
//     set  = this + 0x18;                  // Collection::Set<...>
//     root = *(void**)set;
//     if (root) {
//         SetFreeSubtree(set, root->left);   // root + 0x18
//         SetFreeSubtree(set, root->right);  // root + 0x20
//         SetFreeNode(set, root);
//     }
//     *(void**)(this + 0x18) = 0;          // root  = null
//     *(uint32_t*)(this + 0x20) = 0;       // count = 0   <-- a DWORD
//     operator delete(this);               // 0x180006a10
//     return this;
//
// The ROOT ITSELF is freed with the non-recursive helper after its two subtrees
// — not by handing the root to the recursive one. Same number of frees either
// way; reproduced because it is one fewer call than the tidy version and a
// profile or a watchpoint can see the difference.
//
// The count store is a DWORD at +0x20, not a qword, and the pointer store at
// +0x18 is a qword: the same Set header shape runtime/types/io/Persistence.h
// records for its own settings map (root at +0x00, an int32 count at +0x08).
// ============================================================================
extern "C" void* RtEngine_Application_ScalarDeletingDtor(void* self) {
    uint8_t* p = static_cast<uint8_t*>(self);
    void* set = p + 0x18;
    if (void* root = *reinterpret_cast<void**>(set)) {
        SetFreeSubtree(set, *reinterpret_cast<void**>(static_cast<uint8_t*>(root) + 0x18));
        SetFreeSubtree(set, *reinterpret_cast<void**>(static_cast<uint8_t*>(root) + 0x20));
        SetFreeNode(set, root);
    }
    *reinterpret_cast<void**>(p + 0x18) = nullptr;
    *reinterpret_cast<uint32_t*>(p + 0x20) = 0;
    GuardedFree(self);
    return self;
}

int RtClosure2bEngineRegister() {
    RegisterRuntimeAddr(0x18004da00, (void*)&RtEngine_Application_ScalarDeletingDtor);
    return 1;
}
