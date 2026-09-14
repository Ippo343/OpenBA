// ============================================================================
// runtime/src/object/factory_dtors.cpp — the three Object factory destructors
//                                        (Wave 11, lane OBJLIFE)
// ============================================================================
//
//   ??1Factory@Object@@QEAA@XZ          retail 0x1800071a0 -> body 0x180008110
//   ??1InternalFactory@Object@@QEAA@XZ  retail 0x18002f860
//   ??1ExternalFactory@Object@@QEAA@XZ  retail 0x1800be760
//
// All three are `QEAA` (non-virtual) in the shipped export table — checked
// against Runtime-x64.dll's own export directory, not assumed, because the
// wrong spelling resolves NULL silently:
//
//     0x1800071a0 ??1Factory@Object@@QEAA@XZ
//     0x18002f860 ??1InternalFactory@Object@@QEAA@XZ
//     0x1800be760 ??1ExternalFactory@Object@@QEAA@XZ
//
// ----------------------------------------------------------------------------
// THE CHECK, PORTED BEFORE THE CODE
// ----------------------------------------------------------------------------
// The lane brief warned that a dtor which only frees its block instead of
// destroying every element is a leak shipped under a false byte-identity
// claim. That is not a hypothetical for this row set — ONE of these three
// really does iterate and destroy, and the other two really do only free:
//
//   ??1Factory          ITERATES AND DESTROYS. 0x180008110 walks the whole
//                       TypeNode tree (0x180008250 recurses left +0x38 then
//                       right +0x40), destroys each node's
//                       `value.children` Array (node+0x20) and frees the node.
//                       A port that only zeroed `root` would leak the entire
//                       type registry — every TypeNode and every child array.
//
//   ??1InternalFactory  ONLY FREES. Two Array buffers, no element destruction:
//                       `_objects` holds NON-OWNING Internal* (each Internal
//                       removes itself in ~Internal), `_persistIds` holds
//                       int32. Destroying elements here would be a double
//                       free, not a fix.
//
//   ??1ExternalFactory  BOTH, per member. `_pathStack` destroys its elements
//                       (two Strings each) then frees the buffer;
//                       `_references` and `_resources` walk their trees and
//                       release ONE String per node.
//
// Each verdict below is written from the disassembly at the address named on
// the function, not transcribed from another lane's recipe.
//
// ----------------------------------------------------------------------------
// WHAT THIS FILE DOES *NOT* CLAIM
// ----------------------------------------------------------------------------
// *** THE ONLY CALLER OF ALL THREE, IN OUR WHOLE TREE, IS
//     `RtEngine_Engine_Shutdown` (runtime/src/engine/engine_lifecycle.cpp),
//     AND THAT BODY ITSELF HAS calls=0. *** game_main.cpp runs its own
// `ShutdownEngineStaged()` instead and never calls the real Engine::Shutdown.
// So no gameplay run this wave can execute a single line below, and the
// `calls=` column for these three addresses will read 0 for the reason
// "nothing in the tree calls the caller" — which is one of the four causes,
// stated so nobody has to guess which (reference_uncalled_port_is_not_verified,
// reference_calls_zero_can_mean_always_took_the_uncovered_branch).
//
// "No route" is not "no oracle", so the verification here is a SYNTHESISED
// differential run — see factory_dtors_probe.cpp. What it can and cannot
// reach is stated there rather than implied by a green line.
//
// ----------------------------------------------------------------------------
// HELPERS REACHED THROUGH THE PORT-AWARE ADDRESS DOOR
// ----------------------------------------------------------------------------
// Four of the callees below are already ported and REGISTERED by other lanes.
// They are reached with `ResolveAddr(Module::Runtime, …)`, which consults the
// port registry FIRST (reference_resolveaddr_consults_port_registry_first), so
// these are our bodies at run time and stay ours at cutover. They are NOT
// re-registered here: a duplicate registration is last-binding-wins and would
// silently make one of the two bodies dead code
// (reference_grep_for_the_port_before_writing_it).
//
//   0x1800093e0  Collection::Array<T*>::~Array   owner runtime/src/collection/avl_trees.cpp
//   0x180009c30  SetNodeFree, arg 1 IGNORED      owner runtime/src/system/allocator.cpp
//   0x18001c750  Collection::Array<T>::~Array    owner runtime/src/collection/array.cpp
//   0x18002fb70  Collection::Array<T>::~Array    owner runtime/src/collection/array.cpp
//                (the SEVENTEENTH J1 address — added there by this lane, with
//                 the byte proof in the comment at its J1_ENTRY)
//
// `SetNodeFree`'s FIRST PARAMETER IS IGNORED by the callee, and the two call
// sites in ~Factory pass whatever RCX last held. That is reproduced rather
// than tidied: a one-argument version would free the ignored `this`.
//
// No C++ exceptions anywhere in this file (PLAN.md §4.3 risk 5).
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
// *** INCLUDE ORDER IS LOAD-BEARING AND THAT IS A TRAP, NOT A STYLE CHOICE. ***
// runtime/types/object/External.h #includes TypeInfo.h at its line 322, which
// is INSIDE its own `namespace RtObject { … }` block. So whichever of the two
// is seen first decides whether the reflection types are `RtObject::TypeNode`
// or `RtObject::RtObject::TypeNode` — the include guard makes the second
// inclusion a no-op, so the loser's spelling silently wins. TypeInfo.h is
// pulled in FIRST here to get the un-nested spelling. Reordering these three
// lines will not fail to compile; it will change what `TypeNode` means.
#include "../../types/object/TypeInfo.h"
#include "../../types/object/Internal.h"
#include "../../types/object/External.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"   // the ONE transitional POD<->class bridge

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// for the reason array.cpp:127 gives: resolve.h drags in <windows.h>, whose
// <windef.h> #defines `near`/`far` away. This declaration must stay
// byte-identical to resolve.h's; the linker checks the two.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtObjFactoryDtor {

using RtObject::TypeNode;
using RtObject::ExternalFactory;
using RtObject::PathStackEntry;
using RtSystem::Sys;

// ---------------------------------------------------------------------------
// Local views. These are deliberately NOT added to the shared type headers:
// each is the two or three fields the destructor touches, and asserting a
// fuller layout here would be asserting something this lane did not measure.
// ---------------------------------------------------------------------------

// The Set header that `Object::Factory` begins with. `~Factory` writes exactly
// these two fields at the end — `MOV [RBX],RAX` (qword) and `MOV [RBX+8],EAX`
// (DWORD, not a qword).
struct FactorySetHdr {
    TypeNode* root;    // +0x00
    int32_t   count;   // +0x08
};

// The Set header of `ExternalFactory::_resources` and `::_references`. Same
// shape, and the same two closing stores (`MOV [RBX],RSI` / `MOV [RBX+8],ESI`).
struct ObjSetHdr {
    void*   root;      // +0x00
    int32_t count;     // +0x08
};

// `Collection::Array<PathStackEntry>` as ~ExternalFactory reads it.
struct PathStackArray {
    PathStackEntry* data;      // +0x00
    int32_t         size;      // +0x08
    int32_t         capacity;  // +0x0c
};

// ---------------------------------------------------------------------------
// The four already-ported callees, through the port-aware address door.
// Each pointer is cached in a function-local static AFTER a successful
// resolve only, so a miss during early init is retried rather than frozen
// (reference_the_macro_expands_to_the_prescribed_fix).
// ---------------------------------------------------------------------------

// NOT CONVERTED (ENDGAME B.3.2 step 2): 0x1800093e0 is `rtavl::PtrArrayDtor`,
// a trivial J1-equivalent `Array<T*>::~Array` (avl_trees.cpp's own banner on
// this address). Its two callers below operate on `TypeNode::value.children`
// (`TypeRecord::{children, childCount, childCapacity}`,
// runtime/types/object/TypeInfo.h) — three independently named scalar fields,
// not a real `Collection::Array<TypeRecord*>` member, the same split-layout
// situation `video/video_resources.cpp`'s `Device` hits. `TypeRecord` is used
// by name (`->children`, `->childCount`) throughout type_system.cpp, so
// folding the three fields into one Array member is struct-layout work with
// wide fallout, not a caller conversion — stays a by-address call.
static void PtrArrayDtor_1800093e0(void* arrayHdr) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800093e0));
    fn(arrayHdr);
}

// arg 1 IS IGNORED by the callee. Kept in the signature for exactly that
// reason: dropping it would read the wrong register on every call.
static void SetNodeFree_180009c30(void* ignoredThis, void* node) {
    using Fn = void (*)(void*, void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180009c30));
    fn(ignoredThis, node);
}

// ENDGAME B.3.2 step 2 — `ArrayDtor_18001c750` and `ArrayDtor_18002fb70`
// (0x18001c750, 0x18002fb70) are GONE: their one caller, RtObject_
// InternalFactory_Dtor below, now calls `.~Array()` directly on the two real
// `Collection::Array<T>` members. Both rows stay registered in array.cpp for
// the rest of the tree.

// ============================================================================
// 0x180008250 — the recursive half of ~Factory's tree teardown.
// ============================================================================
//
//     if (!node) return;                            ; TEST RDX,RDX / JZ
//     TypeSubtreeFree(this, node->left);            ; [RDX+0x38]
//     TypeSubtreeFree(this, node->right);           ; [RBX+0x40]
//     ~Array<TypeRecord*>(&node->value.children);   ; LEA RCX,[RBX+8] / ADD RCX,0x18
//     SetNodeFree(this, node);                      ; 0x180009c30
//
// `LEA RCX,[RBX+8]` then `ADD RCX,0x18` is node+0x20, which is
// `TypeNode::value.children` — the Array<TypeRecord*> whose three fields are
// TypeRecord::{children, childCount, childCapacity}. The two-step LEA is MSVC
// materialising `&node->value` for the EH funclet's spill slot and then
// stepping to the member; it is one address, not two objects.
//
// *** THIS IS THE "ITERATES AND DESTROYS" HALF. *** Every node's child array
// is freed before the node is. Collapsing this to `root = nullptr` would leak
// one TypeNode plus one heap buffer per registered type.
//
// RECURSION is retail's, and its depth is the AVL tree's height over the
// registered type count (a few hundred types => under 20 frames). An explicit
// stack would be a different program with a different overflow point.
// ============================================================================
void TypeSubtreeFree(void* set, TypeNode* node) {
    if (node == nullptr)
        return;
    TypeSubtreeFree(set, node->left);
    TypeSubtreeFree(set, node->right);
    PtrArrayDtor_1800093e0(reinterpret_cast<uint8_t*>(node) + 0x20);
    SetNodeFree_180009c30(set, node);
}

// ============================================================================
// ??1Factory@Object@@QEAA@XZ   retail 0x1800071a0, body 0x180008110
// ============================================================================
//
// 0x1800071a0 is an MSVC EH-frame thunk — it spills RCX, writes the -2 unwind
// state slot, and tail-jumps to 0x180008110. The frame slots have no effect on
// the object, so the thunk is not reproduced as a separate function; this body
// IS 0x180008110 and both addresses are registered onto it.
//
// 0x180008110 is the ROOT case of TypeSubtreeFree, open-coded: identical
// instructions with the leading null test moved to a branch that still runs
// the two closing stores.
//
//     root = this->root;
//     if (root) {
//         TypeSubtreeFree(root->left);      ; [RDI+0x38], `this` in RCX
//         TypeSubtreeFree(this, root->right);
//         ~Array<TypeRecord*>(&root->value.children);
//         SetNodeFree(this, root);
//     }
//     this->root  = nullptr;               ; qword
//     this->count = 0;                     ; DWORD — not a second qword
//
// The `MOV [RBX+8],EAX` is a 32-bit store. Widening it to a qword would clear
// four bytes retail leaves alone, and this lane did not measure what is at
// +0x0c.
//
// NOTE ON THE FIRST RECURSIVE CALL: retail's `CALL 0x180008250` at 0x180008138
// is reached with RCX still holding `this` from the prologue, and the second
// with an explicit `MOV RCX,RBX`. Both therefore pass `this`, which is the
// argument SetNodeFree ignores anyway.
// ============================================================================
void RtObject_Factory_Dtor(void* self) {
    FactorySetHdr* s = static_cast<FactorySetHdr*>(self);
    TypeNode* root = s->root;
    if (root != nullptr) {
        TypeSubtreeFree(self, root->left);
        TypeSubtreeFree(self, root->right);
        PtrArrayDtor_1800093e0(reinterpret_cast<uint8_t*>(root) + 0x20);
        SetNodeFree_180009c30(self, root);
    }
    s->root  = nullptr;
    s->count = 0;
}

// ============================================================================
// ??1InternalFactory@Object@@QEAA@XZ   retail 0x18002f860
// ============================================================================
//
//     ~Array<int32>(this + 0x10);        ; CALL 0x18001c750   (_persistIds)
//     ~Array<Internal*>(this + 0x00);    ; JMP  0x18002fb70   (_objects)
//
// That is the whole body. Reverse declaration order over
// `{ objects@+0x00, persistIds@+0x10, random@+0x20 }`, with `random` trivially
// destructible — matching runtime/types/object/Internal.h's asserted layout
// exactly.
//
// *** THIS IS AN "ONLY FREES" DTOR AND THAT IS CORRECT. *** `_objects` holds
// `Internal*` that it does NOT own: every Internal registers itself in
// `??0Internal` and removes itself by swap-remove in `??1Internal`, so by the
// time the factory dies the array is either empty or holds objects some other
// owner will destroy. Adding a "destroy each element" loop here — the tidy
// symmetry a reviewer might ask for — would be a double free on every live
// object, not a leak fix.
//
// 0x18002fb70 is the SEVENTEENTH member of the J1 `Array<T>::~Array` family.
// That is a measured claim: normalising both bodies' RIP displacements and
// rel32s to their ABSOLUTE targets leaves 29 of 29 instructions identical to
// the canonical 0x18001c750, with the only textual difference an INTERNAL
// forward branch whose encoded displacement (0x36) is the same byte on both
// sides (reference_position_only_diffs_hide_one_body). Both resolve the same
// four external operands: 0x1801fc718, 0x18014b390, 0x1800c6c00, 0x18014b388.
// ============================================================================
void RtObject_InternalFactory_Dtor(void* self) {
    // ENDGAME B.3.2 step 2 — both calls are now direct member destructors:
    // `InternalFactory::persistIds` (Array<int32_t>) and `::objects`
    // (Array<Internal*>) are already real `Collection::Array<T>` members
    // (runtime/types/object/Internal.h), and both 0x18001c750 and 0x18002fb70
    // are J1 bodies (trivial destroy), which a trivially destructible
    // `int32_t`/`Internal*` element's trait-derived `~Array()` IS byte for
    // byte. Reverse declaration order is kept: persistIds (+0x10) before
    // objects (+0x00), matching the banner above.
    auto* factory = static_cast<::RtObject::InternalFactory*>(self);
    factory->persistIds.~Array();   // retail 0x18001c750
    factory->objects.~Array();      // retail 0x18002fb70
}

// ============================================================================
// 0x18002e8d0 — destroy a run of PathStackEntry.
// ============================================================================
//
//     if (n <= 0) return;                    ; TEST R8D,R8D / JLE
//     for (i = 0; i < n; ++i, p += 0x10) {
//         Release(&p->filename);             ; LEA RAX,[RBX+8]  — SECOND first
//         Release(&p->directory);            ; [RBX]
//     }
//
// Reverse member order within the element, which is what a compiler-generated
// `~PathStackEntry` emits over `{ Filename directory; Filename filename; }`.
// The stride is 0x10, pinned independently by both path-stack accessors'
// `SHL RDX,0x4` (runtime/types/object/External.h).
//
// The release idiom at both slots is byte-for-byte `RtString_ReleaseInline`
// (runtime/src/system/string.cpp) including its asymmetry: on the freeing
// branch the slot is left holding `data - 8`, on the sharing branch it is
// nulled. Retail's extra `JE` after `ADD [RAX],-8` — which skips the free when
// `data - 8` is zero — is subsumed by MemFree's own null test, which lives
// inside 0x1800c7ad0's first instruction.
//
// `n` is passed as the array's `_size`, so the elements beyond `_size` up to
// `_capacity` are NOT released. That is retail's contract and this keeps it.
//
// ARG 1 IS IGNORED (retail spills RCX and never reads it again); kept in the
// signature for the same reason SetNodeFree's is.
// ============================================================================
void PathStackDestroyRange(void* /*ignoredThis*/, PathStackEntry* p, int n) {
    if (n <= 0)
        return;
    for (int i = 0; i < n; ++i, ++p) {
        // ENDGAME B.3.1 step 2: retail's inlined release IS `~Filename`
        // (0x180006980), asymmetry included. `PathStackEntry` is a
        // not-yet-converted struct, so its two halves keep the POD type and
        // come through the one transitional bridge. +0x08 (filename) goes
        // BEFORE +0x00 (directory), which is retail's order.
        RtSystem::Sys(p->filename).~Filename();
        RtSystem::Sys(p->directory).~Filename();
    }
}

// ============================================================================
// 0x18002e9e0 — ~Array<PathStackEntry>.
// ============================================================================
//
//     DestroyRange(this, this->_data, this->_size);   ; 0x18002e8d0
//     <the canonical J1 free, open-coded>             ; identical to 0x18001c750
//
// The second half is the J1 body inlined rather than called, with the same
// four absolute operands. It is written out here rather than routed through
// the J1 registry because retail inlined it: routing it would add an indirect
// call retail does not make, and the J1 addresses are per-ADDRESS diff sites.
//
// *** THIS IS THE "DESTROYS ITS ELEMENTS" MEMBER of ~ExternalFactory. ***
// ============================================================================
void PathStackArrayDtor(void* self) {
    PathStackArray* a = static_cast<PathStackArray*>(self);
    PathStackDestroyRange(a, a->data, a->size);
    RtSystem::RtMemFree(a->data);   // retail's `if (p)` lives inside 0x1800c7ad0
    a->data     = nullptr;
    a->size     = 0;
    a->capacity = 0;
}

// ============================================================================
// 0x18002ebf0 — free the `_references` subtree.
// ============================================================================
//
//     if (!node) return;
//     ReferenceSubtreeFree(this, node->left);    ; [RDX+0x18]
//     ReferenceSubtreeFree(this, node->right);   ; [RBX+0x20]
//     Release(node + 0x08);                      ; the SECOND element slot
//     SetNodeFree(this, node);                   ; 0x180009c30
//
// *** THE SLOT RELEASED HERE IS +0x08, NOT +0x00, AND THAT IS THE WHOLE
//     DIFFERENCE FROM ITS SIBLING. *** avl_trees.cpp's `SubtreeFree`
// (0x180008340) is the same 0x30-node body releasing +0x00 and explicitly NOT
// +0x08, with a comment warning that a "tidy" symmetric release of the second
// slot would corrupt the heap. This instantiation is the mirror image: its
// String is the value, not the key. Releasing both slots in either body would
// be a heap corruption, and releasing the wrong one is a leak plus a wild
// decrement. They are separate functions in retail for exactly this reason and
// are kept separate here.
// ============================================================================
void ReferenceSubtreeFree(void* set, void* node) {
    if (node == nullptr)
        return;
    uint8_t* n = static_cast<uint8_t*>(node);
    ReferenceSubtreeFree(set, *reinterpret_cast<void**>(n + 0x18));
    ReferenceSubtreeFree(set, *reinterpret_cast<void**>(n + 0x20));
    // ~Filename (0x180006980) over `ReferenceEntry::path`, addressed by offset
    // because the node is raw container bytes here.
    reinterpret_cast<System::Filename*>(n + 0x08)->~Filename();
    SetNodeFree_180009c30(set, node);
}

// ============================================================================
// 0x18002eea0 — free the `_resources` subtree.
// ============================================================================
//
//     if (!node) return;
//     ResourceSubtreeFree(this, node->left);     ; [RDX+0x20]
//     ResourceSubtreeFree(this, node->right);    ; [RBX+0x28]
//     Release(node + 0x00);                      ; the FIRST element slot
//     SetNodeFree(this, node);                   ; 0x180009c30
//
// A LARGER NODE than the two above: its children live at +0x20/+0x28 rather
// than +0x18/+0x20, so its element pair is 0x20 bytes wide. The released slot
// is +0x00 — the key String — and nothing at +0x08..+0x1f is touched.
//
// The three redundant `MOV [RSP+0x48],RBX` stores at 0x18002eed9-0x18002eee3
// are the EH funclet's spill slot being re-written for three consecutive
// destruction states. They have no effect on the object and are not
// reproduced.
// ============================================================================
void ResourceSubtreeFree(void* set, void* node) {
    if (node == nullptr)
        return;
    uint8_t* n = static_cast<uint8_t*>(node);
    ResourceSubtreeFree(set, *reinterpret_cast<void**>(n + 0x20));
    ResourceSubtreeFree(set, *reinterpret_cast<void**>(n + 0x28));
    // ~Filename over `ResourceEntry::path`, the key at +0x00.
    reinterpret_cast<System::Filename*>(n)->~Filename();
    SetNodeFree_180009c30(set, node);
}

// ============================================================================
// ??1ExternalFactory@Object@@QEAA@XZ   retail 0x1800be760
// ============================================================================
//
//     ~Array<PathStackEntry>(this + 0x30);          ; CALL 0x18002e9e0
//     ReferenceSubtreeFree(this+0x20, [this+0x20]); ; CALL 0x18002ebf0
//     this->_references.root  = nullptr;            ; MOV [RBX],RSI
//     this->_references.count = 0;                  ; MOV [RBX+8],ESI  (DWORD)
//     ResourceSubtreeFree(this+0x10, [this+0x10]);  ; CALL 0x18002eea0
//     this->_resources.root   = nullptr;
//     this->_resources.count  = 0;                  ; DWORD
//
// Reverse declaration order over `{ _resources@+0x10, _references@+0x20,
// _pathStack@+0x30 }` — exactly the layout External.h asserts. Note there is
// NO vptr store: ExternalFactory is not polymorphic, which is also why its
// export is `QEAA` and not `UEAA`.
//
// THE `this` PASSED TO EACH SUBTREE FREE IS THE SET, NOT THE FACTORY: retail
// loads `RCX = RBX` where RBX is `this+0x20` / `this+0x10`. It reaches
// SetNodeFree, which ignores it — but a port that passed the factory would be
// relying on that, and this one does not.
//
// The two closing store PAIRS are qword-then-DWORD, like ~Factory's. Nothing
// at +0x0c of either Set header is written.
// ============================================================================
void RtObject_ExternalFactory_Dtor(void* self) {
    uint8_t* p = static_cast<uint8_t*>(self);

    PathStackArrayDtor(p + 0x30);

    ObjSetHdr* refs = reinterpret_cast<ObjSetHdr*>(p + 0x20);
    ReferenceSubtreeFree(refs, refs->root);
    refs->root  = nullptr;
    refs->count = 0;

    ObjSetHdr* res = reinterpret_cast<ObjSetHdr*>(p + 0x10);
    ResourceSubtreeFree(res, res->root);
    res->root  = nullptr;
    res->count = 0;
}

}  // namespace RtObjFactoryDtor

// ============================================================================
// Registration
// ============================================================================
//
// Both the mangled NAME and the Ghidra ADDRESS for each row: our call site
// (engine_lifecycle.cpp) resolves by name through RT_EXPORT_ONCE, and the
// address door is what keeps the row alive at cutover
// (reference_by_address_door_dies_at_cutover). ~Factory gets TWO addresses,
// the EH thunk and the body it tail-jumps to, because a caller that reached
// either one in retail must reach ours.
//
// The three helper addresses this file OWNS are registered too — without them
// the ported bodies would be the only thing in the tree that can reach those
// nodes, and any future by-address caller would silently take the DLL.
// 0x18002fb70 is NOT registered here: it belongs to the J1 family and is
// registered by its owner, runtime/src/collection/array.cpp.
// ============================================================================
// external_path_stack.cpp's port of 0x18002e8d0 -- the one body behind that
// address door. See the note at the registration below.
extern "C" void RtObject_PathStackDestroyRange_18002e8d0(void*, uint8_t*, int);

int RtObjectFactoryDtorsRegister() {
    using namespace RtObjFactoryDtor;
    int n = 0;

    RegisterRuntimeExport("??1Factory@Object@@QEAA@XZ",
                          (void*)&RtObject_Factory_Dtor); ++n;
    RegisterRuntimeAddr(0x1800071a0, (void*)&RtObject_Factory_Dtor);  // EH thunk
    RegisterRuntimeAddr(0x180008110, (void*)&RtObject_Factory_Dtor);  // real body

    RegisterRuntimeExport("??1InternalFactory@Object@@QEAA@XZ",
                          (void*)&RtObject_InternalFactory_Dtor); ++n;
    RegisterRuntimeAddr(0x18002f860, (void*)&RtObject_InternalFactory_Dtor);

    RegisterRuntimeExport("??1ExternalFactory@Object@@QEAA@XZ",
                          (void*)&RtObject_ExternalFactory_Dtor); ++n;
    RegisterRuntimeAddr(0x1800be760, (void*)&RtObject_ExternalFactory_Dtor);

    // Helpers owned by this file.
    RegisterRuntimeAddr(0x180008250, (void*)&TypeSubtreeFree);
    // 0x18002e8d0 -- ONE ADDRESS, ONE REGISTERED BODY.
    //
    // external_path_stack.cpp ports the same retail function as
    // RtObject_PathStackDestroyRange_18002e8d0 and registers this address too,
    // under a DIFFERENT switch (EXTERNAL+PATHSTACK, where this block is
    // FACTORYDTOR), and it registers LAST, so its body has been the one behind
    // this address all along. The two bodies agree instruction for instruction
    // -- same 0x10 stride, same filename-before-directory release order, same
    // RtString_ReleaseInline -- so nothing was observably wrong, but two
    // spellings of one retail function is one edit away from a silent
    // divergence, and the address door must not depend on which switch is on.
    //
    // So this registration now names the SAME body the other file registers.
    // `PathStackDestroyRange` below stays as this file's own helper for
    // PathStackArrayDtor's inlined call; it is no longer an address door.
    RegisterRuntimeAddr(0x18002e8d0, (void*)&RtObject_PathStackDestroyRange_18002e8d0);
    RegisterRuntimeAddr(0x18002e9e0, (void*)&PathStackArrayDtor);
    RegisterRuntimeAddr(0x18002ebf0, (void*)&ReferenceSubtreeFree);
    RegisterRuntimeAddr(0x18002eea0, (void*)&ResourceSubtreeFree);

    printf("[RtPort]   Object::factory dtors: %d exports + 5 helper addresses "
           "registered. NOTE: their only caller in this tree is "
           "Engine::Shutdown, which itself has calls=0 -- see "
           "docs/runtime_port/WAVE11_LANE_OBJLIFE.md before reading a 0 here "
           "as a regression.\n", n);
    return n;
}
