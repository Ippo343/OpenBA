// ============================================================================
// runtime/src/io/modification.cpp — WAVE 9 LANE MOD
// IO::Modification's mods-menu cluster, plus the Mod-entry lifecycle triple
// ============================================================================
//
// WHY THIS FILE EXISTS AND WHY IT IS DIFFERENT FROM EVERY OTHER LANE
//
// Every other Wave 9 lane was handed a list of SYMBOL NAMES that
// unsatisfied_export_scan / the old DLL-era switch could see. This one came out of
// the address-door census (docs/runtime_port/wave9_unregistered_addr_doors.csv,
// ce1d8e9 corrected by 4ef55b6): every `ResolveAddr(Module::Runtime, 0x…)` in
// the tree minus every address anything registers. Ten of the 62 unregistered
// doors are the mods cluster, all resolved in engine/funcs/engine_funcs.cpp.
//
// A by-address door is the deepest form of the problem this wave keeps
// rediscovering: with the DLL absent it resolves to null, the stored function
// pointer is CALLED, and nothing in the run ever mentioned a name. That is
// exactly the failure that walled attempt 13 (the allocator) and attempt 14
// (IO::PackageFile's ctor).
//
// ---------------------------------------------------------------------------
// WHAT IS PORTED HERE
// ---------------------------------------------------------------------------
//
//   | retail      | symbol / role                                  |
//   |-------------|------------------------------------------------|
//   | 0x18000da90 | ?GetCount@Modification@IO@@QEBAHXZ             |
//   | 0x18000da80 | ?GetModification@Modification@IO@@QEBAAEBUMod… |
//   | 0x1800aeab0 | ?Enable@Modification@IO@@QEAAXH_N@Z            |
//   | 0x1800aeb20 | ?Swap@Modification@IO@@QEAAXHH@Z               |
//   | 0x1800b05f0 | ?SaveList@Modification@IO@@QEAAXXZ             |
//   | 0x1800101e0 | Mod::Mod(const Mod&)          (unexported)     |
//   | 0x180026ef0 | Mod::operator=(const Mod&)    (unexported)     |
//   | 0x180011460 | Mod::~Mod()                   (unexported)     |
//   | 0x180010110 | Array<Mod>::operator=         (unexported)     |
//
// plus the seven unexported container internals those five reach, which have
// no name in either shipped build and are therefore RegisterRuntimeAddr ONLY:
//
//   0x180010b80  Array<Mod>::~Array          (per-element ~Mod, then free)
//   0x180011110  Array<int32>::operator=     0x1800111a0  Array<int32>::~Array
//   0x180011220  Array<uint8>::operator=     0x1800112a0  Array<uint8>::~Array
//   0x1800091f0  Array<Filename>::operator=  0x1800098d0  Array<Filename>::~Array
//   0x1800104f0  Set<Filename> clone-subtree
//   0x180010470  Set<Filename> destroy-subtree
//   0x180010570  Set<Filename> node alloc+init
//   0x180009c30  Set<Filename> node free
//
// Every one of those addresses was grepped for an existing
// `RegisterRuntimeAddr(0x…` across runtime/ before being claimed here, exactly
// as lane FIND and lane PKG did, so no other lane's registration is shadowed.
//
// ---------------------------------------------------------------------------
// WHAT IS *NOT* PORTED, AND WHY  (the honest half of the deliverable)
// ---------------------------------------------------------------------------
//
// 0x1800af5f0  ?RefreshList@Modification@IO@@QEAAXXZ — HELD BACK.
//   ~450 instructions that stack-construct an IO::PackageFile (vtable
//   0x180155700), call its Open (0x1800b46b0) / GetName-family accessors
//   (0x180025310/50/90, 0x180025450/530/580) and Close (0x1800b4540), then
//   build a Filename-keyed set per mod and sort the array. IO::PackageFile is
//   the CURRENT cutover wall (WAVE9.md attempt 14: `[RtResolve] MISS
//   "??0PackageFile@IO@@QEAA@XZ" -> NULL`, VCALL-of-0 on the all-zero object).
//   Porting RefreshList before PackageFile would produce a body that cannot be
//   exercised and cannot be diffed. It is blocked on a NAMED dependency that
//   another lane already owns, which is the one blocker shape
//   reference_holdback_blockers_are_stale_check_them_first.md says to state
//   plainly rather than work around.
//
// 0x18002a090  the string-keyed map insert — HELD BACK, and NOT this lane's.
//   It is a two-instruction-deep wrapper: it addrefs the key and value into a
//   stack Pair<String,String>, calls 0x18002a170 (117 instructions: the real
//   insert, which allocates through 0x18000f970 and rebalances through
//   0x18002a310), then releases both. runtime/src/system/configuration.cpp:315
//   already recorded the same disposition — "a red-black insert, a Collection
//   lane's work". This lane CORRECTS one word of that entry: configuration.cpp
//   calls it "unreachable on our boots", which is true of ITS call site (an
//   empty command line) but NOT of the address. persistence_registry_win.cpp:266
//   reaches the same address from ReadSettings, which every boot runs. So the
//   address is live even though the call site that documented it is not — see
//   WAVE9_LANE_MOD.md. It is left to the Collection lane rather than written
//   blind here: an AVL/red-black rebalance written from a paginated
//   disassembly, with no populated tree to test it against, is precisely the
//   silent-corruption case reference_uniform_population_is_not_a_verification.md
//   is about.
//
// ---------------------------------------------------------------------------
// ONE DELIBERATE DEVIATION FROM THE INSTRUCTION STREAM
// ---------------------------------------------------------------------------
// Retail INLINES the allocator lock at every allocation and free in this
// cluster: `MOV RBX,[0x1801fc718]; ADD RBX,0x640; EnterCriticalSection;
// Allocate/Free(_singleton, …); LeaveCriticalSection`. That is byte-for-byte
// what the composed statics ?MemAlloc@MemoryAllocator@System@@ (0x1800c7a50)
// and ?MemFree@ (0x1800c7ad0) do — allocator.cpp's RegisterAllocatorAddresses()
// (WAVE9.md attempt 14) registers both — so this file calls those instead of
// re-inlining the critical section eleven times. Same lock, same singleton,
// same order; only the inlining differs. Doing it the other way would mean
// eleven independent copies of the lock sequence, which is eleven chances to
// get the pairing wrong for zero behavioural gain.
//
// The null test on free is INSIDE 0x1800c7ad0 (`TEST RCX,RCX; JZ ret` is its
// first instruction), which is why the bodies below hand it possibly-null
// pointers exactly as retail does.
//
// ---------------------------------------------------------------------------
// DIFFABILITY, PER FUNCTION
// ---------------------------------------------------------------------------
//   GetCount / GetModification  full oracle; both are const reads of two
//                               fields and produce independent results.
//   Enable                      full BYTE oracle. Enable writes exactly two
//                               bytes and allocates nothing, so the diff can
//                               run ours, photograph the whole 0x80 record plus
//                               the 0x28 header, RESTORE the pre-image, run
//                               theirs, and memcmp. Restoring is only safe
//                               because nothing was allocated — see Swap for
//                               the case where it is not.
//   Swap                        SHADOW, by value. It cannot be replayed and its
//                               bytes cannot be restored: retail's swap deep
//                               copies the two Arrays and CLONES the Set (new
//                               heap nodes), so putting the old bytes back
//                               would leak them. The shadow instead asserts the
//                               post-condition against a pre-image: the shared
//                               (refcounted) members must have exchanged
//                               POINTERS exactly, and the deep-copied members
//                               must have exchanged VALUES. That is not
//                               tautological — it fails on a wrong offset, a
//                               skipped member, or a deep copy that lost
//                               elements.
//   SaveList                    the mutation it makes to the object is
//                               `mods[i].order = i`, which is IDEMPOTENT and
//                               allocates nothing, and its Persistence writes
//                               are idempotent too. So it IS replayable: run
//                               ours, photograph the order column, restore the
//                               pre-image, run theirs, compare the columns.
//                               Retail's run is the one that lands, so a diff
//                               run's live behaviour is retail's.
//   the lifecycle triple + the container internals
//                               NOT DIFFED, and not diffable: each one mutates
//                               a live object through the shared heap, so
//                               running the original as an oracle would double
//                               free at refcount 1. They are verified
//                               INDIRECTLY, through Swap's shadow — Swap is
//                               nothing but three calls into the triple, so a
//                               green Swap shadow is a statement about them.
// ============================================================================

#include <cstdio>
#include <cstring>
#include <cstddef>    // offsetof — the Set<Filename> node-layout asserts below
#include <new>        // placement new into freshly allocated Filename storage

#include "../../types/io/Modification.h"
#include "../../types/io/Persistence.h"   // ENDGAME B.3.9 IO: the class static
#include "../../types/system/Filename.h"
#include "../../types/collection/Collection.h"   // ENDGAME B.3.2 step 2 — see below
#include "../../types/system/MemoryAllocator.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"

// ENDGAME B.3.1 step 2: the Filename copy-ctor/AdjustDelimiters entry points
// this file used to forward-declare here (0x1800067d0 / 0x1800c6970) are gone
// from this translation unit. Every use was one of Filename.h's documented
// idioms (a copy ctor, a Filename<-Filename assign-then-normalise) and is now
// spelled as the class itself through the pod_bridge.h `Sys()` bridge --
// `Mod::file` and `ModSetNode::key` are `RtSystem::Filename` MEMBERS of
// not-yet-converted structs (B31_RULES.md), so they are reached through the
// bridge rather than changing Modification.h's field types.

namespace RtIO {

using RtSystem::Filename;
using RtSystem::String;
using RtSystem::Sys;      // the transitional POD->class bridge (pod_bridge.h)

// ---------------------------------------------------------------------------
// The allocator boundary. See the banner: the composed statics, by address,
// with the same never-cache-a-null contract array.cpp uses (a caller that runs
// before RegisterAllocatorAddresses() must not freeze a null in for the life of
// the process).
// ---------------------------------------------------------------------------
}  // namespace RtIO

namespace RtIO {

// B3R RT-IO: the per-file ModMemAlloc / ModMemFree wrappers (which went through
// `rtcoll::DoorAlloc`/`DoorFree`, i.e. these same two statics) are gone; every
// site calls System::MemoryAllocator::MemAlloc / MemFree, the allocator the
// `Collection::Set<System::Filename, …>` of modification_refresh.cpp also uses,
// and the one BANISHED_REDZONE's instrument seat sits on.

// The String header's refcount word (String.h's accessor).
static inline int32_t& RefCount(char* s) {
    return RtSystem::StringRefCount(s);
}

// The release-then-adopt sequence retail inlines at every String member of Mod
// (e.g. 0x180026f0a..0x180026f46). NOTE the difference from
// RtString_AdoptInline: retail stores `src` UNCONDITIONALLY and only then tests
// it, so when src is null the destination is left NULL rather than left holding
// the `d - 8` that the freeing branch of the release wrote. Reusing
// RtString_AdoptInline here would have kept a dangling block base in the field.
void AssignShared(char** dst, char* src) {
    char* cur = *dst;
    if (cur == src) return;
    if (cur != nullptr) {
        if (--RefCount(cur) == 0) {
            *dst = cur - 8;
            ::System::MemoryAllocator::MemFree(*dst);
        } else {
            *dst = nullptr;
        }
    }
    *dst = src;
    if (src != nullptr) ++RefCount(src);
}

// The copy-construct half (0x18001020b..0x180010219): the destination has
// already been zeroed by the caller, so there is nothing to release.
static void AdoptShared(char** dst, char* src) {
    *dst = nullptr;
    if (src != nullptr) {
        *dst = src;
        ++RefCount(src);
    }
}

// ============================================================================
// Collection internals — the seven unexported helpers the triple reaches
// ============================================================================

// The four ~Array bodies (0x1800111a0, 0x1800112a0, and the buffer-free tail of
// 0x1800098d0 / 0x180010b80) are the same six instructions with different
// element cleanup in front. `[RDI+8]` is ONE eight-byte store covering size AND
// capacity, so capacity IS reset here.
template <class T>
static void* ArrayFreeBuffer(ModArrayOf<T>* a) {
    ::System::MemoryAllocator::MemFree(a->data);          // retail's `if (p)` lives inside 0x1800c7ad0
    a->data = nullptr;
    a->size = 0;
    a->capacity = 0;
    return nullptr;               // retail leaves RAX == 0
}

// 0x1800111a0 — Collection::Array<int32>::~Array
void* RtColl_ArrayI32Dtor_1800111a0(void* a) {
    return ArrayFreeBuffer(static_cast<ModArray*>(a));   // LEFT: SIG RtColl_ArrayI32Dtor_1800111a0 (void* core entry; PackageFile / vertexlayout callers, wave X)
}
// 0x1800112a0 — Collection::Array<uint8>::~Array
void* RtColl_ArrayU8Dtor_1800112a0(void* a) {
    return ArrayFreeBuffer(static_cast<ModArray*>(a));   // LEFT: SIG RtColl_ArrayU8Dtor_1800112a0 (void* core entry; PackageFile / vertexlayout callers, wave X)
}

// 0x180009af0, already ported by lane FIND (filesystem_find_win.cpp) and
// registered there. Declared, not re-implemented: two copies of a release loop
// is how a refcount idiom drifts.
void DestroyFilenameArray(void* ignoredThis, uint8_t* base, int count);

// 0x1800098d0 — Collection::Array<Filename>::~Array. Releases every element
// through 0x180009af0 first, then frees the buffer.
void* RtColl_ArrayFilenameDtor_1800098d0(void* av) {
    ModArray* a = static_cast<ModArray*>(av);   // LEFT: SIG RtColl_ArrayFilenameDtor_1800098d0 (void* core entry: callers hand Collection::Array<Filename>, wave X)
    DestroyFilenameArray(a, static_cast<uint8_t*>(a->data), a->size);   // CAST-OK: elem_stride
    return ArrayFreeBuffer(a);
}

// The growth rule shared by all four Array::operator= bodies, read at
// 0x18001013d..0x180010171 and identical (modulo stride/align) at 0x180011125,
// 0x180011235 and 0x18000921d:
//
//     int oldSize = dst->size;
//     dst->size = src->size;
//     if (dst->size > dst->capacity) {
//         dst->capacity = dst->size;
//         if (dst->size == oldSize + 1 && oldSize != 0)
//             dst->capacity = 3 * dst->size;          // LEA EAX,[RCX+RCX*2]
//         dst->data = MemAlloc((int64_t)dst->capacity * stride, align);
//     }
//
// The `oldSize` read happens BEFORE the store (`MOV ECX,[RDI+8]` then
// `MOV EDX,[RBX+8]` then `MOV [RBX+8],ECX`) — the push-one-element case is
// detected against the size the array had on entry.
template <class T>
static void ArrayAssignGrow(ModArrayOf<T>* dst, const ModArrayOf<T>* src,
                            uint32_t stride, int align) {
    const int32_t oldSize = dst->size;
    dst->size = src->size;
    if (dst->size > dst->capacity) {
        dst->capacity = dst->size;
        if (dst->size == oldSize + 1 && oldSize != 0)
            dst->capacity = dst->size + dst->size * 2;
        dst->data = static_cast<T*>(::System::MemoryAllocator::MemAlloc(
            static_cast<uint64_t>(static_cast<int64_t>(dst->capacity)) * stride, align));
    }
}

// The copy loop's `ADD R9,RDX; JZ skip` is a test on `dst->data + i*stride`,
// not on `dst->data` — transcribed rather than simplified, because it is the
// only thing standing between a failed MemAlloc and a write through null, and
// it is also why the loop re-reads dst->size on every iteration.

// 0x180011110 — Collection::Array<int32>::operator=
void* RtColl_ArrayI32Assign_180011110(void* dstv, const void* srcv) {
    ModArrayOf<uint32_t>* dst = static_cast<ModArrayOf<uint32_t>*>(dstv);   // LEFT: SIG RtColl_ArrayI32Assign_180011110 (void* core entry, wave X)
    const ModArrayOf<uint32_t>* src = static_cast<const ModArrayOf<uint32_t>*>(srcv);   // LEFT: SIG as above
    ArrayFreeBuffer(dst);                       // CALL 0x1800111a0
    ArrayAssignGrow(dst, src, 4, 4);
    for (int32_t i = 0; i < dst->size; ++i) {
        uint32_t* d = dst->data + i;
        if (d == nullptr) continue;
        d[0] = src->data[i];
    }
    return dst;
}

// 0x180011220 — Collection::Array<uint8>::operator=
void* RtColl_ArrayU8Assign_180011220(void* dstv, const void* srcv) {
    ModArrayOf<uint8_t>* dst = static_cast<ModArrayOf<uint8_t>*>(dstv);   // LEFT: SIG RtColl_ArrayU8Assign_180011220 (void* core entry, wave X)
    const ModArrayOf<uint8_t>* src = static_cast<const ModArrayOf<uint8_t>*>(srcv);   // LEFT: SIG as above
    ArrayFreeBuffer(dst);                       // CALL 0x1800112a0
    ArrayAssignGrow(dst, src, 1, 1);
    for (int32_t i = 0; i < dst->size; ++i) {
        uint8_t* d = dst->data + i;
        if (d == nullptr) continue;
        *d = src->data[i];
    }
    return dst;
}

// 0x1800091f0 — Collection::Array<Filename>::operator=. Unlike the two above it
// carries its own self-assignment early-out (`CMP RCX,RDX; JNZ`), because its
// callers do not all guard.
void* RtColl_ArrayFilenameAssign_1800091f0(void* dstv, const void* srcv) {
    ModArrayOf<Filename>* dst = static_cast<ModArrayOf<Filename>*>(dstv);   // LEFT: SIG RtColl_ArrayFilenameAssign_1800091f0 (void* core entry, wave X)
    const ModArrayOf<Filename>* src = static_cast<const ModArrayOf<Filename>*>(srcv);   // LEFT: SIG as above
    if (dst == src) return dst;
    // ENDGAME B.3.2 step 2 — retail 0x1800098d0; see the identical conversion's
    // rationale on Mod::~Mod's +0x70 site below (trait-derived, no explicit spec).
    reinterpret_cast<Collection::Array<System::Filename>&>(*dst).~Array();
    ArrayAssignGrow(dst, src, 8, 8);
    for (int32_t i = 0; i < dst->size; ++i) {
        Filename* d = dst->data + i;
        if (d == nullptr) continue;
        // CALL 0x1800067d0 — Filename::Filename(Filename const&). `d` is inside
        // the buffer ArrayAssignGrow just (re)allocated: raw, uninitialised
        // bytes, so placement-construct into it.
        new (&Sys(*d)) System::Filename(Sys(src->data[i]));
    }
    return dst;
}

// 0x180010570 — Set<Filename> node alloc + init. RCX (the owning set) is
// spilled at 0x180010570 and NEVER READ AGAIN — the same shape lane FIND
// documented for DestroyFilenameArray, and the reason this signature carries an
// ignored first argument at all.
static ModSetNode* SetNodeNew(void* /*ignoredSet*/, const Filename* key,
                              ModSetNode* parent) {
    ModSetNode* n = static_cast<ModSetNode*>(::System::MemoryAllocator::MemAlloc(0x28, 8));
    if (n == nullptr) return nullptr;
    // CALL 0x1800067d0 — Filename copy ctor, into the freshly allocated node's
    // raw `key` bytes.
    new (&Sys(n->key)) System::Filename(Sys(*key));
    n->parent = parent;
    n->left = nullptr;
    n->right = nullptr;
    n->w20 = 0;                                           // MOV dword [RDI+0x20],EAX
    n->w22 = 0;
    return n;
}

// The Set<Filename> instantiation whose node walk this file needs. Only
// `DestroySubtree` is used from it, and that member touches nothing but `Node`,
// `T::~T` and the allocator door — never `Compare`, never `KeyTraits`, never
// `AvlOps` — so the compare policy here is immaterial and does NOT have to be
// (and deliberately is not) `modification_refresh.cpp`'s `FilenameICompare`:
// the keyed machinery lives in that file and stays there.
using ModSetT = Collection::Set<System::Filename>;
static_assert(sizeof(ModSetT::Node) == sizeof(ModSetNode),
              "Set<Filename>::Node is ModSetNode's 0x28 bytes");
static_assert(offsetof(ModSetT::Node, parent) == offsetof(ModSetNode, parent), "parent");
static_assert(offsetof(ModSetT::Node, left)   == offsetof(ModSetNode, left),   "left");
static_assert(offsetof(ModSetT::Node, right)  == offsetof(ModSetNode, right),  "right");

// 0x1800104f0 — Set<Filename> clone-subtree. The `MOV word ptr [RAX+0x22]`
// immediately after the allocation is UNGUARDED in retail: a failed node
// allocation writes through null. Transcribed, not guarded — the project's rule
// is that a faithful crash names an upstream problem (an exhausted heap here),
// and a guard would silently produce a half-built tree instead.
void* RtColl_SetCloneSubtree_1800104f0(void* set, const void* srcv) {
    const ModSetNode* src = static_cast<const ModSetNode*>(srcv);
    ModSetNode* dup = SetNodeNew(set, &src->key, nullptr);
    dup->w22 = src->w22;             // copied in THIS order: +0x22 then +0x20
    dup->w20 = src->w20;
    if (src->left != nullptr) {
        ModSetNode* c = static_cast<ModSetNode*>(
            RtColl_SetCloneSubtree_1800104f0(set, src->left));
        dup->left = c;
        c->parent = dup;
    }
    if (src->right != nullptr) {
        ModSetNode* c = static_cast<ModSetNode*>(
            RtColl_SetCloneSubtree_1800104f0(set, src->right));
        dup->right = c;
        c->parent = dup;
    }
    return dup;
}

// 0x180010470 — Set<Filename> destroy-subtree. Post-order, then the key's
// release, then the node. The null test is at entry, which is why every caller
// hands it a possibly-null child.
//
// ENDGAME B.3.2 closing pass: FOLDED onto `Collection::Set<Filename>::
// DestroySubtree`, which is the same walk in the same order —
//   if (!n) return; left; right; element.~T(); Free(n);
// — and the same free: the template's `Alloc::Free` for `DefaultAllocator` is
// `SetNodeFree` -> `rtsetavl::SetNodeFree(nullptr, n)` -> `rtcoll::DoorFree()`,
// which is byte-for-byte where this file's own `ModMemFree` went. The node's
// `key` is `Node::element` at +0x00 (static_asserted above), so the Filename
// release is the same dtor on the same bytes. The set argument stays in the
// signature because retail's 0x180010470 takes it and ignores it.
void RtColl_SetDestroySubtree_180010470(void* set, void* nodev) {
    (void)set;
    ModSetT::DestroySubtree(static_cast<ModSetT::Node*>(nodev));
}

// ============================================================================
// The Mod-entry lifecycle triple
// ============================================================================

// 0x1800101e0 — Mod::Mod(const Mod&)
//
// Retail zeroes each member immediately before filling it, member by member, in
// declaration order, rather than zeroing the whole record up front. That is
// reproduced literally: the Array members are zeroed and then handed to
// operator=, which begins by FREEING dst->data — so the zero is what stops the
// copy ctor from freeing the caller's uninitialised stack.
void* RtIo_Mod_CopyCtor_1800101e0(Mod* dst, const Mod* src) {

    AdoptShared(&dst->name, src->name);
    AdoptShared(&dst->author, src->author);
    AdoptShared(&dst->description, src->description);

    dst->i18 = src->i18;
    dst->i1c = src->i1c;
    dst->order = src->order;
    dst->i24 = src->i24;
    dst->i28 = src->i28;
    dst->i2c = src->i2c;

    // 0x180010265: retail advances BOTH pointers by 0x30 and then compares
    // them, which is a self-copy guard on the sub-object, not on the record.
    dst->ints.data = nullptr;
    dst->ints.size = 0;
    dst->ints.capacity = 0;
    if (&dst->ints != &src->ints)
        RtColl_ArrayI32Assign_180011110(&dst->ints, &src->ints);

    dst->bytes.data = nullptr;
    dst->bytes.size = 0;
    dst->bytes.capacity = 0;
    if (&dst->bytes != &src->bytes)
        RtColl_ArrayU8Assign_180011220(&dst->bytes, &src->bytes);

    dst->enabled = src->enabled;

    // CALL 0x1800067d0 — Filename copy ctor (zeroes, adopts, AdjustDelimiters).
    // `dst->file` is raw, uninitialised bytes here (part of the Mod the caller
    // just zeroed member-by-member), so the value is placement-constructed.
    new (&Sys(dst->file)) System::Filename(Sys(src->file));

    dst->set.root = nullptr;
    dst->set.count = 0;
    if (&src->set != &dst->set) {
        if (src->set.root != nullptr)
            dst->set.root = static_cast<ModSetNode*>(
                RtColl_SetCloneSubtree_1800104f0(&dst->set, src->set.root));
        dst->set.count = src->set.count;
    }

    dst->files.data = nullptr;
    dst->files.size = 0;
    dst->files.capacity = 0;
    RtColl_ArrayFilenameAssign_1800091f0(&dst->files, &src->files);

    return dst;
}

// 0x180026ef0 — Mod::operator=(const Mod&)
void* RtIo_Mod_Assign_180026ef0(Mod* dst, const Mod* src) {

    AssignShared(&dst->name, src->name);
    AssignShared(&dst->author, src->author);
    AssignShared(&dst->description, src->description);

    dst->i18 = src->i18;
    dst->i1c = src->i1c;
    dst->order = src->order;
    dst->i24 = src->i24;
    dst->i28 = src->i28;
    dst->i2c = src->i2c;

    if (&dst->ints != &src->ints)
        RtColl_ArrayI32Assign_180011110(&dst->ints, &src->ints);
    if (&dst->bytes != &src->bytes)
        RtColl_ArrayU8Assign_180011220(&dst->bytes, &src->bytes);

    dst->enabled = src->enabled;

    // The Filename at +0x58 gets the plain shared assign and THEN
    // AdjustDelimiters (0x18002704a CALL 0x1800c6970) — not the copy ctor,
    // because the destination already owns a string. That PAIR is
    // `Filename::operator=(const Filename&)`, 0x180006770 (Filename.h
    // DELIMITERS note): self-guard on the buffer, release, adopt, normalise.
    Sys(dst->file) = Sys(src->file);

    if (&src->set != &dst->set) {
        if (dst->set.root != nullptr) {
            RtColl_SetDestroySubtree_180010470(&dst->set, dst->set.root);
            dst->set.root = nullptr;
            dst->set.count = 0;
        }
        if (src->set.root != nullptr)
            dst->set.root = static_cast<ModSetNode*>(
                RtColl_SetCloneSubtree_1800104f0(&dst->set, src->set.root));
        dst->set.count = src->set.count;
    }

    RtColl_ArrayFilenameAssign_1800091f0(&dst->files, &src->files);
    return dst;
}

// 0x180011460 — Mod::~Mod(). Reverse declaration order, as MSVC emits.
void RtIo_Mod_Dtor_180011460(Mod* self) {

    // ENDGAME B.3.2 step 2 — +0x70 retail 0x1800098d0. `System::Filename` is a
    // real class (B.3.1), so `SpecFor<System::Filename>()` builds EXACTLY the
    // row this shim hand-derives: assign is a placement copy-construct
    // (filesystem_find_win.cpp's `AssignFilename`) and destroy is a per-element
    // `~Filename()` loop (`DestroyFilenameArray`) — the same two bodies, so the
    // trait-derived `Collection::Array<System::Filename>` needs no explicit
    // spec. `files` stays the `ModArray` POD (Mod is not yet a class), so this
    // is the transitional bridge, not a field re-type.
    reinterpret_cast<Collection::Array<System::Filename>&>(self->files).~Array();

    RtColl_SetDestroySubtree_180010470(&self->set, self->set.root);   // +0x60
    self->set.root = nullptr;
    self->set.count = 0;

    // +0x58 — retail's Filename dtor (0x180006980), which is String::Release()
    // on the same bytes (filename.cpp's ~Filename); byte-identical to the
    // refcount code this replaced.
    Sys(self->file).~Filename();

    // ENDGAME B.3.2 step 2 — +0x40/+0x30, retail 0x1800112a0 / 0x180011110.
    // `uint8_t`/`int32_t` are trivially destructible, so `SpecFor<T>()` sets
    // `destroy = nullptr` and the class's dtor is exactly `ArrayFreeBuffer`
    // (`MemFree(_data)` then zero the three fields) through the same
    // `rtcoll::MemFree` door (0x1800c7ad0) — no explicit spec needed.
    reinterpret_cast<Collection::Array<uint8_t>&>(self->bytes).~Array();   // +0x40
    reinterpret_cast<Collection::Array<int32_t>&>(self->ints).~Array();   // +0x30

    char** shared[3] = { &self->description, &self->author, &self->name };
    for (int i = 0; i < 3; ++i) {
        char* s = *shared[i];
        if (s == nullptr) continue;
        if (--RefCount(s) == 0) {
            *shared[i] = s - 8;
            ::System::MemoryAllocator::MemFree(*shared[i]);
        } else {
            *shared[i] = nullptr;
        }
    }
}

// 0x180010b80 — Collection::Array<Mod>::~Array
void* RtColl_ArrayModDtor_180010b80(void* av) {
    ModArrayOf<Mod>* a = static_cast<ModArrayOf<Mod>*>(av);   // LEFT: SIG RtColl_ArrayModDtor_180010b80 (void* core entry, wave X)
    Mod* p = a->data;
    for (int32_t i = a->size; i > 0; --i, ++p)
        RtIo_Mod_Dtor_180011460(p);
    return ArrayFreeBuffer(a);
}

// 0x180010110 — Collection::Array<Mod>::operator=, resolved in our tree as
// fn_ModArrayCopy (engine_funcs.cpp:1747) and again as ModArrayCopy()
// (runtime/src/engine/loops.cpp:230). Both doors now land here.
void* RtColl_ArrayModAssign_180010110(void* dstv, const void* srcv) {
    ModArrayOf<Mod>* dst = static_cast<ModArrayOf<Mod>*>(dstv);   // LEFT: SIG RtColl_ArrayModAssign_180010110 (void* core entry, wave X)
    const ModArrayOf<Mod>* src = static_cast<const ModArrayOf<Mod>*>(srcv);   // LEFT: SIG as above
    if (dst == src) return dst;
    RtColl_ArrayModDtor_180010b80(dst);          // CALL 0x180010b80
    ArrayAssignGrow(dst, src, 0x80, 8);
    for (int32_t i = 0; i < dst->size; ++i) {
        Mod* d = dst->data + i;
        if (d == nullptr) continue;
        RtIo_Mod_CopyCtor_1800101e0(d, src->data + i);
    }
    return dst;
}

// ============================================================================
// The five Modification entry points
// ============================================================================

// ?GetCount@Modification@IO@@QEBAHXZ   retail 0x18000da90
//     MOV EAX,[RCX+0x10] ; RET

// ?GetModification@Modification@IO@@QEBAAEBUMod@12@H@Z   retail 0x18000da80
//     MOVSXD RAX,EDX ; SHL RAX,7 ; ADD RAX,[RCX+8] ; RET
//
// MOVSXD, not a zero extension: a NEGATIVE index strides BACKWARDS off the
// array. Ghidra's C prints this as a zero extension
// (reference_ghidra_c_output_vs_disassembly.md), which would silently turn
// index -1 into an 8-GB forward offset. `static_cast<int64_t>` is the sign
// extension, and it is load-bearing.

// ?Enable@Modification@IO@@QEAAXH_N@Z   retail 0x1800aeab0
//
//     MOV RAX,[RCX+8] ; MOVSXD RDX,EDX ; MOV byte [RCX],1
//     SHL RDX,7 ; MOV byte [RDX+RAX+0x50],R8B ; RET
//
// Note that the dirty flag is set BEFORE the bounds-free indexed store, and
// unconditionally — Enable(i, x) marks the list dirty even when x is what the
// mod already held. Nothing in our tree depends on that, but a "no-op returns
// early" tidy-up would be a real behaviour change to IsDirty.

// ?Swap@Modification@IO@@QEAAXHH@Z   retail 0x1800aeb20
//
//     if (a == b) return;                      // CMP EDX,EBX ; JZ epilogue
//     this->dirty = 1;
//     Mod tmp;  Mod::Mod(&tmp, &mods[a]);      // 0x1800101e0
//     mods[a] = mods[b];                       // 0x180026ef0
//     mods[b] = tmp;                           // 0x180026ef0
//     tmp.~Mod();                              // 0x180011460
//
// The equality test is on the two INDICES and happens before the dirty flag is
// touched, so Swap(i, i) is a complete no-op. `b` is sign-extended (MOVSXD
// RBX,R8D) before the shift and `a` separately (MOVSXD RDI,EDX); both strides
// are `<< 7`.

}  // namespace RtIO

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS — `IO::Modification`'s member bodies
// ============================================================================
//
// A member of `IO::X` cannot be DEFINED inside `namespace RtIO` — `IO` and
// `RtIO` are siblings, so the definition has to appear in a namespace enclosing
// the class (B34_BRIEFS §A.2). This block therefore sits AFTER
// `}  // namespace RtIO`, and reaches that namespace's helpers as `RtIO::X` —
// which works for its anonymous-namespace and `static` members too, in this
// same translation unit.
//
// Inside this block a bare `Modification`, `Mod` or `Filename` would bind to
// whatever `namespace IO` sees first, so every POD is spelled `RtIO::` and every
// real class `System::` (B34_RULES §7.2, THE GREP HAZARD).
//
// NO QUALIFIED `IO::Modification::` CALL IS NEEDED IN THE DOORS ABOVE, and that
// is worth saying because a reader coming from blocks 1-2 will look for it:
// §A.1's recursion hazard is about a POLYMORPHIC receiver whose live instances
// wear a MINTED table whose slots resolve back to the doors. `IO::Modification`
// has no vtable at all (Modification.h's banner), so a member call on it is
// already a direct call.
// ============================================================================

namespace IO {

// ?GetCount@Modification@IO@@QEBAHXZ                        retail 0x18000da90
//     MOV EAX,[RCX+0x10] ; RET
// ?_singleton@Modification@IO@@2PEAV12@EA (0x1801fc6b0) -- the class static
// (ENDGAME B.3.9 IO, D7); RtIoPortInit redirects the minted row onto it.
Modification* Modification::_singleton = nullptr;

int Modification::GetCount() const {
    return _count;
}

// ?GetModification@Modification@IO@@QEBAAEBUMod@12@H@Z      retail 0x18000da80
//     MOVSXD RAX,EDX ; SHL RAX,7 ; ADD RAX,[RCX+8] ; RET
//
// MOVSXD, not a zero extension: a NEGATIVE index strides BACKWARDS off the
// array. Ghidra's C prints this as a zero extension
// (reference_ghidra_c_output_vs_disassembly.md), which would silently turn
// index -1 into an 8-GB forward offset. `static_cast<int64_t>` is the sign
// extension, and it is load-bearing. Unbounded, exactly as retail is.
const RtIO::Mod& Modification::GetModification(int index) const {
    return _mods[index];   // a signed int index: MOVSXD, then the 0x80 stride
}

// ?Enable@Modification@IO@@QEAAXH_N@Z                       retail 0x1800aeab0
//
//     MOV RAX,[RCX+8] ; MOVSXD RDX,EDX ; MOV byte [RCX],1
//     SHL RDX,7 ; MOV byte [RDX+RAX+0x50],R8B ; RET
//
// The dirty flag is set BEFORE the bounds-free indexed store, and
// unconditionally — Enable(i, x) marks the list dirty even when x is what the
// mod already held. Nothing in our tree depends on that, but a "no-op returns
// early" tidy-up would be a real behaviour change to IsDirty.
void Modification::Enable(int index, bool value) {
    _dirty = 1;
    _mods[index].enabled = static_cast<uint8_t>(value);
}

// ?Swap@Modification@IO@@QEAAXHH@Z                          retail 0x1800aeb20
//
//     if (a == b) return;                      // CMP EDX,EBX ; JZ epilogue
//     this->dirty = 1;
//     Mod tmp;  Mod::Mod(&tmp, &mods[a]);      // 0x1800101e0
//     mods[a] = mods[b];                       // 0x180026ef0
//     mods[b] = tmp;                           // 0x180026ef0
//     tmp.~Mod();                              // 0x180011460
//
// The equality test is on the two INDICES and happens before the dirty flag is
// touched, so Swap(i, i) is a complete no-op. `b` is sign-extended (MOVSXD
// RBX,R8D) before the shift and `a` separately (MOVSXD RDI,EDX); both strides
// are `<< 7`, which is what `RtIO::Mod*` arithmetic gives (sizeof is 0x80).
void Modification::Swap(int a, int b) {
    if (a == b) return;

    _dirty = 1;

    RtIO::Mod tmp;   // a POD: uninitialised stack, as retail's
    RtIO::RtIo_Mod_CopyCtor_1800101e0(&tmp, _mods + a);
    RtIO::RtIo_Mod_Assign_180026ef0(_mods + a, _mods + b);
    RtIO::RtIo_Mod_Assign_180026ef0(_mods + b, &tmp);
    RtIO::RtIo_Mod_Dtor_180011460(&tmp);
}

// ?SaveList@Modification@IO@@QEAAXXZ                        retail 0x1800b05f0
//
//     if (!this->savingSettings) { Array<Mod>::operator=(&savedMods, &mods);
//                                  return; }
//     Persistence* p = *(0x1801fc6b8);
//     for (int i = 0; i < this->count; ++i) {
//         mods[i].order = i;
//         Filename tmp = mods[i].file.GetPathFilename(true);   // 0x1800c6740
//         char* nameRef = tmp.data;  addref;                   // then release tmp
//         p->Set("ModEnabled_", nameRef, mods[i].enabled);     // 0x1800b8380
//         p->Set("ModIndex_",   nameRef, mods[i].order);       // 0x1800b80c0
//         release nameRef;
//     }
//
// The two literals are at 0x1801557f8 ("ModEnabled_") and 0x180155808
// ("ModIndex_"), read out of .rdata; they are the same prefixes RefreshList
// formats back with "%s%s" (0x1801551f8) when it reads the settings in.
//
// `mods[i].order` is re-read from the record for the second Set rather than
// reusing the loop counter — retail does `MOV R9D,[RBP+R14+0x20]`. Identical in
// value; transcribed as written so a future change to the first store cannot
// silently desynchronise the two.
void Modification::SaveList() {
    if (!_savingSettings) {
        // 0x1800b0619: LEA RDX,[RCX+8] ; ADD RCX,0x18 ; CALL 0x180010110
        RtIO::RtColl_ArrayModAssign_180010110(&_savedMods, &_mods);
        return;
    }

    ::IO::Persistence* p = ::IO::Persistence::_singleton;   // ENDGAME B.3.9 IO (D7): the class static

    for (int i = 0; i < _count; ++i) {
        RtIO::Mod* m = _mods + i;
        m->order = i;

        // GetPathFilename's by-value return constructs `tmp` in place (the
        // hidden return slot). `m->file` is a `RtSystem::Filename` MEMBER of the
        // not-yet-converted `Mod`, so it is reached through a cast.
        char* ref;
        {
            System::Filename tmp(reinterpret_cast<const System::Filename*>(&m->file)   // B.3.x bridge
                                     ->GetPathFilename(true));

            // 0x1800b0676..0x1800b06be: take a REFERENCE to the returned
            // buffer, then release the temporary. `ref` outlives the scope
            // deliberately; the inner block's close is retail's release.
            ref = tmp._data;
            if (ref != nullptr) ++RtIO::RefCount(ref);
        }  // ~tmp releases here, exactly where retail's release sat

        // The door decides the overload: ?Set@..@QEAAXPEBD0_N@Z (bool), then
        // ?Set@..@QEAAXPEBD0H@Z (int).
        p->Set("ModEnabled_", ref, static_cast<bool>(m->enabled != 0));
        p->Set("ModIndex_", ref, static_cast<int>(m->order));

        if (ref != nullptr) {
            if (--RtIO::RefCount(ref) == 0) ::System::MemoryAllocator::MemFree(ref - 8);
        }
    }

}


}  // namespace IO
