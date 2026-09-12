// ============================================================================
// runtime/src/object/external.cpp — Object::External / ExternalFactory /
//                                   ExternalList          (Wave 3, lane C)
// ============================================================================
//
// Byte-faithful port of the decompilation at the retail address named above
// each function (Runtime-x64.dll, image base 0x180000000, read 2026-09-06).
// Layout evidence and per-field pins: runtime/types/object/External.h.
// Wave write-up, including everything this lane deliberately did NOT port:
// docs/runtime_port/WAVE3_LANE_C.md.
//
// WHAT THIS FAMILY IS. `External` is one loaded resource file: a path plus a
// subclass payload. `ExternalFactory` is the global resource table (singleton
// 0x1801fc6c8) and owns two AVL trees — `_resources`, keyed by path, and
// `_references`, keyed by the reference id the save stream uses in place of a
// path. `ExternalList` is an External whose payload is a flat array of other
// Externals (the mod list, the help topics, the scenarios, the map generators).
//
// WHAT IS LIVE HERE, AND WHY THAT SPLIT
// -------------------------------------
// The factory's two trees are DLL-owned global state, and registration
// redirects only OUR call sites — the DLL's own internal calls still land in
// the DLL (PLAN.md §4.1 decision 2). So both implementations drive the same
// tables, and the only safe shape is Wave 2's allocator shape: operate on the
// LIVE singleton's own tables, byte-faithfully, never a private replica.
// Everything ported below obeys that: nothing here keeps state of its own.
//
// Ported and registered, all READ-ONLY on the factory and all with a real diff
// oracle behind them:
//     External::GetPath
//     ExternalFactory::Find / FindNext / GetReferenceId / GetPathForReferenceId
//     ExternalFactory::IsReferenceModeEnabled
//     ExternalList::Find / GetResourceCount
// Ported and registered, trivially-verifiable writers with no oracle possible:
//     External::External / ~External          (one vtable stamp + one release)
//     ExternalFactory::EnableReferenceMode    (a single byte store)
//
// NOT ported — see the write-up for the full argument:
//     ExternalFactory::FindOrLoad, ExternalFactory::Clean
//         Both rewrite the resource tree. Porting them means porting Set
//         insert/erase/rebalance for a 0x18-wide element plus ExpandRelativePath,
//         External::Load and Add — a large body of code that this wave has no
//         way to prove, since running both implementations would mutate the one
//         shared tree twice. Reversed and written up; NOT registered.
//     ExternalFactory::SerializeReferenceTable, External::SaveText
//         Both reach IO::Stream / IO::Serializer / IO::TextFileStream and the
//         filesystem, which is Wave 4. Reversed and written up; NOT registered.
//         (Wave 2 took the same decision for Time::AsSeconds, except that one
//         landed as a forwarder; a forwarder is declined here because it would
//         add indirection and a census row for zero behaviour change.)
//
// ABI. Wave 1 defect 1 / reference_hidden_retslot_wild_write.md: a member that
// returns a class BY VALUE takes a HIDDEN RETURN SLOT in RDX and every real
// argument shifts right. Three functions in this lane do:
//     External::GetPath                    (this, ret)
//     ExternalFactory::GetPathForReferenceId (this, ret, id)
//     ExternalList::Find                   (this, ret, path, type)
// Each returns the slot in RAX. The existing engine/funcs/engine_funcs.h
// typedefs for all three were checked against this and are CORRECT.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdio>
#include <cstring>

#include "../../types/object/External.h"
#include "../../funcs/rt_port.h"
#include "../system/string.h"

namespace RtObject {

using RtSystem::String;
using RtSystem::StringLength;
using RtSystem::StringRefCount;
using RtSystem::RtString_AdoptInline;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtSystem_String_Set;
using RtSystem::RtSystem_String_Concat;
using RtSystem::RtSystem_String_ConcatCStr;

}  // namespace RtObject

// Ported in Wave 2 (runtime/src/system/filename.cpp); declared here rather than
// pulled from a header because that lane's file exposes none of its own.
namespace RtSystem {
void      RtSystem_Filename_AdjustDelimiters(Filename* self);
Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other);
Filename* RtSystem_Filename_GetPathIdentifer(const Filename* self, Filename* ret);
}  // namespace RtSystem

namespace RtObject {

using RtSystem::RtSystem_Filename_AdjustDelimiters;
using RtSystem::RtSystem_Filename_CtorCopy;
using RtSystem::RtSystem_Filename_GetPathIdentifer;

// ============================================================
// Local helpers
// ============================================================

// Retail's tree comparisons all go through the CRT's _stricmp. That is
// locale-sensitive in MSVC and the DLL never calls setlocale, so it runs in the
// "C" locale where _stricmp is plain ASCII case folding. Spelled out here for
// the same reason filename.cpp spells it out: runtime/src must not depend on a
// CRT whose locale behaviour differs between toolchains (PLAN.md §4.1).
static int StriCmpAscii(const char* a, const char* b) {
    for (;;) {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + 32);
        if (ca != cb) return static_cast<int>(ca) - static_cast<int>(cb);
        if (ca == 0)  return 0;
    }
}

// The ordering the resource tree is built with, inlined at every walk site
// (FUN_18002dd70, FindNext 0x1800be9d0, Clean 0x1800be310, Add 0x1800be570).
// It is byte-identical to System::Filename::operator< (0x180026cb0): a null
// right-hand side orders below everything, a null left-hand side orders below
// any non-null, otherwise case-insensitive.
static inline bool FilenameLess(const char* a, const char* b) {
    if (b == nullptr) return false;
    if (a == nullptr) return true;
    return StriCmpAscii(a, b) < 0;
}

// Collection::Set<ResourceEntry, …>::Find, out-of-lined by MSVC as
// FUN_18002dd70 for this one element width. Node tail offsets are baked in per
// instantiation (WAVE2.md "the Set node trap"), so this body serves the 0x18
// element and nothing else.
static ResourceSet::Node* ResourceSetFind(const ResourceSet* set, const char* key) {
    ResourceSet::Node* n = set->_root;
    while (n != nullptr) {
        if (FilenameLess(key, n->element.path.data))      n = n->left;
        else if (FilenameLess(n->element.path.data, key)) n = n->right;
        else                                              return n;
    }
    return nullptr;
}

// The in-order successor walk both FindNext and SerializeReferenceTable inline:
// descend the right subtree to its leftmost node, else climb parents while we
// are the right child. Transcribed from 0x1800be9d0's tail.
static ResourceSet::Node* ResourceSetLeftmost(ResourceSet::Node* n) {
    while (n->left != nullptr) n = n->left;
    return n;
}
static ResourceSet::Node* ResourceSetSuccessor(ResourceSet::Node* n) {
    if (n->right != nullptr) return ResourceSetLeftmost(n->right);
    ResourceSet::Node* p = n->parent;
    while (p != nullptr && p->right == n) { n = p; p = p->parent; }
    return p;
}

// The two exported vtables the ctor/dtor stamp. Wave 2 finding 1: the DLL
// exports 147 ??_7 symbols, so a ported ctor can name the vtable instead of
// mirroring slots out of a live object. External's case is the easy one for the
// same reason Thread's was — the port does not replace any virtual, it only has
// to write the address retail writes.
static void* VtableExternal() {
    static void* v = ResolveExport("??_7External@Object@@6B@");   // 0x1801547a0
    return v;
}
static void* VtableBase() {
    static void* v = ResolveExport("??_7Base@Object@@6B@");       // 0x180154788
    return v;
}

// ============================================================
// Object::External — construction and destruction. NOT DIFFED.
//
// An oracle is impossible for both: running the original as well would stamp
// the vtable twice (harmless) and release the path's refcount twice (a double
// free at refcount 1). They are short enough to verify by reading.
// ============================================================

// ??0External@Object@@QEAA@XZ  retail 0x1800bc9c0
//
// Retail stamps the BASE vtable and then immediately overwrites it with
// External's. That is the inlined Object::Base constructor followed by
// External's own stamp, and it is transcribed rather than folded away: a class
// with a virtual base-class call in its constructor would observe both.
External* RtObject_External_Ctor(External* self) {
    self->_vftable = VtableBase();
    self->_vftable = VtableExternal();
    self->_path.data      = nullptr;
    self->_refs._data     = nullptr;
    self->_refs._size     = 0;
    self->_refs._capacity = 0;
    return self;
}

// ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z
//                                                        retail 0x1800066e0
//
// WAVE 9 LANE Y, 2026-09-08. Slot 4 of ??_7External@Object@@6B@ (0x1801547a0),
// and until now a NAMED TRAP in the mint. The whole retail body, verbatim:
//
//     1800066e0  MOV EAX,0x1
//     1800066e5  RET
//
// It is `External`'s virtual DEFAULT: the base class cannot compile anything,
// so it answers CompileResult(1) and every resource type that can compile
// overrides the slot. `?A` in the mangling means the enum is returned BY VALUE,
// but a `W4` enum is 4 bytes, so it comes back in EAX and there is NO hidden
// return slot — the arg count matches the name (this, const Filename&) with
// nothing shifted (reference_hidden_retslot_wild_write.md checked, not assumed;
// the returning-a-CLASS-by-value rule that bites GetPath below does not apply
// to a scalar enum).
//
// `path` is genuinely dead: RDX is never read in the two instructions. It is
// kept in the signature because the vtable slot's type demands it.
//
// ORACLE: RT_DIFF_SCALAR against the shipped body. The function is pure — no
// stores, no calls, no reads of `this` — so replaying it is free and the
// comparison is exact rather than a shape check.
uint32_t RtObject_External_Compile(External* self, const void* path) {
    RT_DIFF_BEGIN("Object::External::Compile");
    const uint32_t ours = 1u;
    RT_DIFF_SCALAR(ours,
        RtOriginal<uint32_t (*)(External*, const void*)>(
            "?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z")(
                self, path));
    return ours;
}

// ??1External@Object@@UEAA@XZ  retail 0x1800bc950
//
// Order matters and is retail's: stamp External's vtable (so a virtual called
// during teardown dispatches to External's override, not the subclass's), tear
// down the +0x10 array, release the path, then revert to the Base vtable.
//
// The array teardown is FUN_180006b80 -> FUN_180006ac0: walk `_size` elements
// at a stride of 0x10 releasing only the leading String of each, free the
// buffer, then zero `_data` and the size/capacity qword together. Capacity is
// therefore cleared here even though Collection::Array's own SetSize never
// shrinks it (WAVE2_COLLECTION.md §4 consequence 1) — this is a destructor, not
// a resize, and it zeroes both halves of that qword in one store.
// *** WAVE 27 LANE FRONTIER, 2026-09-12: 0x180006b80 AND 0x180006ac0 ARE NOW
// PORTED AND REGISTERED *** in runtime/src/collection/array_destroy.cpp (as
// `RtColl_ArrayDtorElem_180006b80` and `RtColl_DestroyStrings_180006ac0`, both
// under kD_Str_10). Retail's ~External at 0x1800bc950 does not inline them - it
// CALLs 0x180006b80 at 0x1800bc974 - so the loop below is now a SECOND live
// copy of logic that also exists behind a registered address.
//
// It is deliberately LEFT AS IS rather than converted to a ResolveAddr call,
// and the reason is not taste: the registered body sits behind
// BANISHED_RT_COLLECTION + BANISHED_RT_ARRAY_DESTROY, and routing this dtor
// through the registry would make it an EXEC-of-0 at cutover whenever either
// gate is off, which is strictly worse than a duplicate. The two copies are
// behaviourally identical (both are Body B with stride 0x10, slot at +0x00);
// if they ever diverge, THIS comment is where to start, and the shape to
// beware of is the shadowed-address defect array_destroy.cpp records against
// lane UIW. ADDRESSEE: whoever arms the collection gates unconditionally.
void RtObject_External_Dtor(External* self) {
    self->_vftable = VtableExternal();

    // FUN_180006ac0 — destroy [0, _size).
    ExternalArrayElement* e = self->_refs._data;
    for (int i = 0; i < self->_refs._size; ++i, ++e) {
        char* p = e->owned;
        if (p != nullptr) {
            if (--StringRefCount(p) == 0) {
                e->owned = p - 8;
                if (e->owned != nullptr)
                    RtSystem::RtMemFree(e->owned);
            } else {
                e->owned = nullptr;
            }
        }
    }
    if (self->_refs._data != nullptr)
        RtSystem::RtMemFree(self->_refs._data);
    self->_refs._data     = nullptr;
    self->_refs._size     = 0;
    self->_refs._capacity = 0;

    RtString_ReleaseInline(reinterpret_cast<String*>(&self->_path));

    self->_vftable = VtableBase();
}

// ============================================================
// Object::External::GetPath — DIFFED
// ============================================================

// ?GetPath@External@Object@@QEBA?AVFilename@System@@XZ  retail 0x180006250
//
// HIDDEN RETURN SLOT: (this in RCX, ret in RDX). One line in retail — the
// Filename copy constructor from `this->_path`, which shares the buffer and
// bumps its refcount; it does NOT copy the characters.
//
// Diffable because ours and theirs both copy from the same `_path`, so the two
// results are the SAME char* and an 8-byte compare is exact. The oracle call
// takes one extra reference, which the scratch release below gives back.
Filename* RtObject_External_GetPath(const External* self, Filename* ret) {
    RT_DIFF_BEGIN("Object::External::GetPath");

    RtSystem_Filename_CtorCopy(ret, &self->_path);

    if (rt_diff_on__) {
        Filename theirs{nullptr};
        RtOriginal<Filename* (*)(const External*, Filename*)>(
            "?GetPath@External@Object@@QEBA?AVFilename@System@@XZ")(self, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Filename));
        RtString_ReleaseInline(reinterpret_cast<String*>(&theirs));
    }
    return ret;
}

// ============================================================
// Object::ExternalFactory — the read-only half
// ============================================================

// ?IsReferenceModeEnabled@ExternalFactory@Object@@QEBA_NXZ  retail 0x1800bf0f0
bool RtObject_ExternalFactory_IsReferenceModeEnabled(const ExternalFactory* self) {
    RT_DIFF_BEGIN("Object::ExternalFactory::IsReferenceModeEnabled");
    bool ours = self->_referenceModeEnabled;
    RT_DIFF_SCALAR(ours,
        RtOriginal<bool (*)(const ExternalFactory*)>(
            "?IsReferenceModeEnabled@ExternalFactory@Object@@QEBA_NXZ")(self));
    return ours;
}

// ?EnableReferenceMode@ExternalFactory@Object@@QEAAX_N@Z  retail 0x1800bcf40
//
// NOT DIFFED — a store into live factory state. It is one byte and idempotent,
// but running the original as well would write it twice, and a diff site whose
// only content is "we both wrote the same constant" is the `calls=0`-shaped
// non-evidence WAVE2 finding 5 warns about.
void RtObject_ExternalFactory_EnableReferenceMode(ExternalFactory* self, bool enable) {
    self->_referenceModeEnabled = enable;
}

// ?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@AEBVFilename@System@@@Z
//   retail 0x1800bdeb0                                             — DIFFED
//
// Look a path up in the resource tree. Three things here are not obvious:
//
//  1. **A path with no `:identifier` suffix gets `":resource"` appended.**
//     Retail calls Filename::GetPathIdentifer and, when the result is null or
//     empty, concatenates the literal at 0x180155ae0 — `":resource"` — through
//     the `operator+(String const&, char const*)` overload (0x1800c92d0). The
//     same convention appears in ExternalList::Find, which uses the
//     String&-vs-String& overload (0x1800c9440) instead. So `"Dialog/Foo.rsc"`
//     and `"Dialog/Foo.rsc:resource"` are the SAME key; the suffix names a
//     sub-resource inside the file.
//  2. **AdjustDelimiters is called five times** — twice on the working copy,
//     once more after the concatenation, then twice on a second copy. Each call
//     is idempotent and each is transcribed. Note that AdjustDelimiters rewrites
//     the buffer IN PLACE and the working copy SHARES the caller's buffer, so
//     `Find` normalises the caller's Filename as a side effect. That is retail's
//     behaviour and callers depend on it (see the ctor note in filename.cpp).
//  3. **The tree is case-INSENSITIVE.** ExternalList::Find, three functions
//     below, is case-SENSITIVE over the same kind of path. That asymmetry is
//     real, not a transcription slip.
const External* RtObject_ExternalFactory_Find(const ExternalFactory* self,
                                              const Filename* path) {
    RT_DIFF_BEGIN("Object::ExternalFactory::Find");

    const External* ours = nullptr;

    Filename key{nullptr};
    RtString_AdoptInline(reinterpret_cast<String*>(&key), path->data);
    RtSystem_Filename_AdjustDelimiters(&key);
    RtSystem_Filename_AdjustDelimiters(&key);

    Filename ident{nullptr};
    RtSystem_Filename_GetPathIdentifer(path, &ident);
    if (ident.data == nullptr || StringLength(ident.data) == 0) {
        String cat{nullptr};
        RtSystem_String_ConcatCStr(&cat, reinterpret_cast<const String*>(&key), ":resource");
        // The inlined String::operator=, WITHOUT the trailing AdjustDelimiters
        // that Filename::operator= carries — retail calls it separately, once.
        if (key.data != cat.data) {
            RtString_ReleaseInline(reinterpret_cast<String*>(&key));
            key.data = cat.data;
            if (key.data != nullptr)
                ++StringRefCount(key.data);
        }
        RtSystem_Filename_AdjustDelimiters(&key);
        RtString_ReleaseInline(&cat);
    }

    Filename probe{nullptr};
    RtString_AdoptInline(reinterpret_cast<String*>(&probe), key.data);
    RtSystem_Filename_AdjustDelimiters(&probe);
    RtSystem_Filename_AdjustDelimiters(&probe);

    ResourceSet::Node* node = ResourceSetFind(&self->_resources, probe.data);
    RtString_ReleaseInline(reinterpret_cast<String*>(&probe));

    if (node != nullptr)
        ours = node->element.external;

    RtString_ReleaseInline(reinterpret_cast<String*>(&ident));
    RtString_ReleaseInline(reinterpret_cast<String*>(&key));

    RT_DIFF_SCALAR(static_cast<const void*>(ours),
        static_cast<const void*>(
            RtOriginal<const External* (*)(const ExternalFactory*, const Filename*)>(
                "?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@AEBVFilename@System@@@Z")(
                    self, path)));
    return ours;
}

// ?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z
//   retail 0x1800bf100                                             — DIFFED
//
// *** RETAIL DEREFERENCES THE FOUND NODE WITHOUT A NULL CHECK. ***
// The tail is literally
//     MOV EAX, dword ptr [RBX + 0x14]   ; RBX = the node Set::Find returned
//     OR ECX, -1 / CMP EAX, ECX / CMOVZ EAX, ECX
// with no test of RBX. An External that is not in the resource tree therefore
// faults reading 0x14. Left unguarded on purpose (CLAUDE.md: "Don't 'fix' a
// faithful crash with a null check") — a guard would not restore the behaviour,
// it would skip it, and every current caller passes an External the factory
// owns. If this ever faults at 0x14, the bug is upstream: something handed us
// an External the factory never Add()ed.
//
// The CMOVZ pair is a genuine no-op — `x == -1 ? -1 : x`. Transcribed as the
// plain read it compiles to.
//
// Note the double copy: retail takes GetPath's Filename and then copy-constructs
// a SECOND Filename from it before searching, releasing both. Transcribed.
uint32_t RtObject_ExternalFactory_GetReferenceId(const ExternalFactory* self,
                                                 const External* external) {
    RT_DIFF_BEGIN("Object::ExternalFactory::GetReferenceId");

    Filename got{nullptr};
    RtObject_External_GetPath(external, &got);
    Filename key{nullptr};
    RtSystem_Filename_CtorCopy(&key, &got);

    ResourceSet::Node* node = ResourceSetFind(&self->_resources, key.data);

    RtString_ReleaseInline(reinterpret_cast<String*>(&key));
    RtString_ReleaseInline(reinterpret_cast<String*>(&got));

    uint32_t ours = node->element.referenceId;   // unguarded, exactly as retail

    RT_DIFF_SCALAR(ours,
        RtOriginal<uint32_t (*)(const ExternalFactory*, const External*)>(
            "?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z")(self, external));
    return ours;
}

// ?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z
//   retail 0x1800bf1d0                                             — DIFFED
//
// HIDDEN RETURN SLOT: (this in RCX, ret in RDX, id in R8D). The existing
// fn_ExternalFactory_GetPathForReferenceId typedef in engine_funcs.h:5758 has
// all three and is correct.
//
// Ordinary integer-keyed AVL descent over `_references`. On a miss retail does
// NOT leave the slot untouched — it copy-constructs from a null temporary, so
// the caller gets a valid empty Filename. Both branches are transcribed, since
// the empty-on-miss result is what the load path checks.
Filename* RtObject_ExternalFactory_GetPathForReferenceId(const ExternalFactory* self,
                                                         Filename* ret, uint32_t id) {
    RT_DIFF_BEGIN("Object::ExternalFactory::GetPathForReferenceId");

    ReferenceSet::Node* n = self->_references._root;
    while (n != nullptr) {
        if (id < n->element.id)      n = n->left;
        else if (n->element.id < id) n = n->right;
        else                         break;
    }

    if (n != nullptr) {
        Filename tmp{nullptr};
        RtString_AdoptInline(reinterpret_cast<String*>(&tmp), n->element.path.data);
        RtSystem_Filename_AdjustDelimiters(&tmp);
        RtSystem_Filename_AdjustDelimiters(&tmp);
        RtSystem_Filename_CtorCopy(ret, &tmp);
        RtString_ReleaseInline(reinterpret_cast<String*>(&tmp));
    } else {
        Filename empty{nullptr};
        RtSystem_Filename_CtorCopy(ret, &empty);
    }

    if (rt_diff_on__) {
        Filename theirs{nullptr};
        RtOriginal<Filename* (*)(const ExternalFactory*, Filename*, uint32_t)>(
            "?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z")(
                self, &theirs, id);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Filename));
        RtString_ReleaseInline(reinterpret_cast<String*>(&theirs));
    }
    return ret;
}

// ?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z
//   retail 0x1800be9d0                                             — DIFFED
//
// Iterate the resource tree in path order, returning the next External whose
// type id IS OR DERIVES FROM `typeId`. `prev == null` starts at the leftmost
// node; otherwise retail re-FINDS `prev` by its path (not by pointer) and
// resumes from its in-order successor. Re-finding by path means a resource that
// was evicted between two calls restarts the walk from wherever its key would
// have sorted; that is retail's behaviour, not an accident of this transcript.
//
// THE DERIVED-TYPE TEST IS LANE A's TERRITORY. Retail inlines an IsDerivedFrom
// walk over Object::Factory's type map here. This port transcribes the raw
// offsets (see FactoryTypeNodeOffsets in External.h) WITHOUT claiming what the
// fields are called. Two consequences worth stating:
//   * `Factory::_singleton` is read by address, since Lane A owns the accessor.
//   * Retail does NOT null-check the type-map lookup: a type id absent from the
//     map falls out of the search loop with a null node and is then read at
//     +0x08. Faithful, unguarded, same argument as GetReferenceId above.
// WAVE 9 LANE Y, 2026-09-08 — this used to be resolved ONCE inside
// RtObjectExternalRegister() and cached here. That is the frozen-cache defect
// (WAVE9.md, "caches a null forever on the first call"), and the DLL-absent
// boot MEASURED it: `RtObjectPortInit()` runs at resolve_standalone.cpp:325 and
// `RtDataMintInit()` at :405, so at cutover the resolve happened EIGHTY LINES
// of init before the minted storage for `?_singleton@Factory@Object@@0PEAV12@EA`
// was registered, and the cache froze on null for the life of the process.
// Attempt 04 names it in its own words:
//     [RtResolveEarly]   BYPASS?   caller=…  "?_singleton@Factory@Object@@0PEAV12@EA"
//     [RtUnresolved] 1  …  ?_singleton@Factory@Object@@0PEAV12@EA
// The symptom would NOT have been a crash. `factoryRoot` null makes FindNext's
// inlined IsDerivedFrom walk zero nodes, so every derived-type query silently
// finds nothing — a wrong answer with a healthy-looking pointer, which is the
// failure mode this file's own header warns about one section down.
//
// RtResolveCached caches a HIT and RETRIES a miss, naming it once, so the mint
// registering later is picked up. Deliberately NOT resolved at registration
// time: nothing here may assume an init ordering it does not control.
static void* g_factorySingletonCache = nullptr;   // Runtime 0x1801fc6e8

static void** FactorySingletonSlot() {
    return reinterpret_cast<void**>(RtResolveCached(
        "?_singleton@Factory@Object@@0PEAV12@EA", &g_factorySingletonCache));
}

const External* RtObject_ExternalFactory_FindNext(const ExternalFactory* self,
                                                  uint32_t typeId,
                                                  const External* prev) {
    RT_DIFF_BEGIN("Object::ExternalFactory::FindNext");

    ResourceSet::Node* cursor = nullptr;

    if (prev == nullptr) {
        if (self->_resources._root != nullptr)
            cursor = ResourceSetLeftmost(self->_resources._root);
    } else {
        Filename got{nullptr};
        RtObject_External_GetPath(prev, &got);
        Filename key{nullptr};
        RtSystem_Filename_CtorCopy(&key, &got);

        ResourceSet::Node* found = ResourceSetFind(&self->_resources, key.data);

        RtString_ReleaseInline(reinterpret_cast<String*>(&key));
        RtString_ReleaseInline(reinterpret_cast<String*>(&got));

        if (found != nullptr)
            cursor = ResourceSetSuccessor(found);
    }

    const External* ours = nullptr;
    void** pFactorySingleton = FactorySingletonSlot();
    const uint8_t* factoryRoot =
        pFactorySingleton ? *reinterpret_cast<const uint8_t* const*>(*pFactorySingleton)
                          : nullptr;

    while (cursor != nullptr) {
        External* candidate = cursor->element.external;
        // vtable +0x10 == GetTypeId()
        typedef uint32_t (*GetTypeIdFn)(const External*);
        GetTypeIdFn getTypeId = *reinterpret_cast<GetTypeIdFn*>(
            reinterpret_cast<uint8_t*>(candidate->_vftable) + 0x10);
        uint32_t actual = getTypeId(candidate);

        if (actual == typeId) { ours = candidate; break; }

        // Inlined IsDerivedFrom: locate `actual` in Factory's type map, then
        // walk its ancestor chain looking for `typeId`.
        const uint8_t* node = factoryRoot;
        while (node != nullptr) {
            uint32_t nodeId = *reinterpret_cast<const uint32_t*>(node);
            if (actual < nodeId)
                node = *reinterpret_cast<const uint8_t* const*>(node + FactoryTypeNodeOffsets::kLeft);
            else if (nodeId < actual)
                node = *reinterpret_cast<const uint8_t* const*>(node + FactoryTypeNodeOffsets::kRight);
            else
                break;
        }
        const uint8_t* chain = node + FactoryTypeNodeOffsets::kAncestorChain;  // unguarded
        bool derived = false;
        do {
            if (*reinterpret_cast<const uint32_t*>(chain) == typeId) { derived = true; break; }
            chain = *reinterpret_cast<const uint8_t* const*>(
                chain + FactoryTypeNodeOffsets::kAncestorNext);
        } while (chain != nullptr);
        if (derived) { ours = candidate; break; }

        cursor = ResourceSetSuccessor(cursor);
    }

    RT_DIFF_SCALAR(static_cast<const void*>(ours),
        static_cast<const void*>(
            RtOriginal<const External* (*)(const ExternalFactory*, uint32_t, const External*)>(
                "?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z")(
                    self, typeId, prev)));
    return ours;
}

// ============================================================
// Object::ExternalList
// ============================================================

// ?GetResourceCount@ExternalList@Object@@QEBAHXZ  retail 0x180007650  — DIFFED
int RtObject_ExternalList_GetResourceCount(const ExternalList* self) {
    RT_DIFF_BEGIN("Object::ExternalList::GetResourceCount");
    int ours = self->_count;
    RT_DIFF_SCALAR(ours,
        RtOriginal<int (*)(const ExternalList*)>(
            "?GetResourceCount@ExternalList@Object@@QEBAHXZ")(self));
    return ours;
}

// ?Find@ExternalList@Object@@QEBA?AV?$ExternalPtr@VExternal@Object@@@2@AEBVFilename@System@@VType@2@@Z
//   retail 0x1800bf700                                             — DIFFED
//
// HIDDEN RETURN SLOT: (this in RCX, ret in RDX, path in R8, type in R9).
//
// *** THE `Type` PARAMETER IS NEVER READ. *** R9 is dead in the whole body: the
// list is searched by path alone and the first path match is returned whatever
// its type. A caller that passes a type expecting it to filter gets no filtering.
//
// Two more things worth naming:
//   * The `":resource"` suffix convention again, but through the String&/String&
//     operator+ (0x1800c9440) rather than the char* overload Find uses.
//   * The comparison is a LINEAR scan and is **case-SENSITIVE** — length first
//     (the `str-8` header), then a plain byte strcmp. `ExternalFactory::Find`
//     over the same kind of path is case-INSENSITIVE. Both are transcribed as
//     found; do not "harmonise" them.
ExternalPtr<External>* RtObject_ExternalList_Find(const ExternalList* self,
                                                  ExternalPtr<External>* ret,
                                                  const Filename* path,
                                                  void* /*type — unused by retail*/) {
    RT_DIFF_BEGIN("Object::ExternalList::Find");

    Filename ident{nullptr};
    RtSystem_Filename_GetPathIdentifer(path, &ident);
    const bool noIdentifier = (ident.data == nullptr || StringLength(ident.data) == 0);
    RtString_ReleaseInline(reinterpret_cast<String*>(&ident));

    char* key = nullptr;
    if (noIdentifier) {
        Filename suffix{nullptr};
        RtSystem_String_Set(reinterpret_cast<String*>(&suffix), ":resource");
        RtSystem_Filename_AdjustDelimiters(&suffix);

        String cat{nullptr};
        RtSystem_String_Concat(&cat, reinterpret_cast<const String*>(path),
                               reinterpret_cast<const String*>(&suffix));
        if (cat.data != nullptr) { ++StringRefCount(cat.data); key = cat.data; }

        RtString_ReleaseInline(&cat);
        RtString_ReleaseInline(reinterpret_cast<String*>(&suffix));
    } else {
        if (path->data != nullptr) { ++StringRefCount(path->data); key = path->data; }
    }

    ret->ptr = nullptr;
    for (int i = 0; i < self->_count; ++i) {
        Filename got{nullptr};
        RtObject_External_GetPath(self->_resources[i], &got);

        const int keyLen = key       ? StringLength(key)      : 0;
        const int gotLen = got.data  ? StringLength(got.data) : 0;

        bool equal;
        if (keyLen != gotLen) {
            equal = false;
        } else if (key == nullptr || StringLength(key) == 0) {
            equal = true;                       // two empty paths match
        } else if (got.data == nullptr) {
            equal = false;                      // retail's `iVar8 = 1` arm
        } else {
            equal = (std::strcmp(key, got.data) == 0);
        }

        RtString_ReleaseInline(reinterpret_cast<String*>(&got));

        if (equal) { ret->ptr = self->_resources[i]; break; }
    }

    if (key != nullptr) {
        if (--StringRefCount(key) == 0)
            RtSystem::RtMemFree(key - 8);
    }

    if (rt_diff_on__) {
        ExternalPtr<External> theirs{nullptr};
        RtOriginal<ExternalPtr<External>* (*)(const ExternalList*, ExternalPtr<External>*,
                                              const Filename*, void*)>(
            "?Find@ExternalList@Object@@QEBA?AV?$ExternalPtr@VExternal@Object@@@2@"
            "AEBVFilename@System@@VType@2@@Z")(self, &theirs, path, nullptr);
        RT_DIFF_BYTES(ret, &theirs, sizeof(ExternalPtr<External>));
        // ExternalPtr holds no reference (see External.h), so nothing to release.
    }
    return ret;
}

}  // namespace RtObject

// ============================================================================
// Registration
// ============================================================================
//
// BOTH the mangled name AND the retail address are registered for every
// function, following Wave 2's string.cpp/filename.cpp. Name-only registration
// would be inert at any site that resolved by address — and this lane has two
// such sites: code/Game/Tutorial/tutorialgoaldialog.cpp reaches the External
// ctor and dtor through ResolveAddr(Module::Runtime, 0x1800bc9c0 / 0x1800bc950),
// never through their names. That is WAVE2 finding 4 seen from the other side.
//
// All 17 names in this lane's scope were checked against the shipped DLL's
// export table with pefile: **all 17 exist**, so this lane has no phantom-name
// defect of the WAVE1-defect-3 kind.
//
// THE TWO `_typeInfo` DATA SYMBOLS ARE DELIBERATELY NOT REGISTERED:
//     ?_typeInfo@External@Object@@0VBaseTypeInfo@2@A      0x1801f9b38
//     ?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A  0x1801f9be0
// They are static BaseTypeInfo OBJECTS, not code, and the DLL has already
// linked them into Object::Factory's `_firstTypeInfo` chain and assigned them
// runtime type ids. Registering our own storage under those names would hand
// every one of our call sites a SECOND, unregistered BaseTypeInfo whose type id
// no DLL-built object would ever match — the type-id filter failure mode of
// reference_unregistered_typeid_filter_zeroes_the_target.md, with a healthy
// non-null pointer to reassure the reader. They stay on the DLL until the whole
// reflection system moves (Lane A), which is the atomic-per-class rule of
// PLAN.md §4.1 decision 5.
// ============================================================================

int RtObjectExternalRegister() {
    using namespace RtObject;

    // Object::Factory::_singleton is NOT resolved here any more — see
    // FactorySingletonSlot() above. Resolving it at registration time froze a
    // null at cutover, because the mint that supplies it registers 80 lines of
    // init later.

    int n = 0;

    RegisterRuntimeExport("??0External@Object@@QEAA@XZ",
                          (void*)&RtObject_External_Ctor); ++n;
    RegisterRuntimeExport("??1External@Object@@UEAA@XZ",
                          (void*)&RtObject_External_Dtor); ++n;
    RegisterRuntimeExport("?GetPath@External@Object@@QEBA?AVFilename@System@@XZ",
                          (void*)&RtObject_External_GetPath); ++n;
    RegisterRuntimeExport("?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z",
                          (void*)&RtObject_External_Compile); ++n;
    RegisterRuntimeExport("?EnableReferenceMode@ExternalFactory@Object@@QEAAX_N@Z",
                          (void*)&RtObject_ExternalFactory_EnableReferenceMode); ++n;
    RegisterRuntimeExport("?IsReferenceModeEnabled@ExternalFactory@Object@@QEBA_NXZ",
                          (void*)&RtObject_ExternalFactory_IsReferenceModeEnabled); ++n;
    RegisterRuntimeExport("?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@AEBVFilename@System@@@Z",
                          (void*)&RtObject_ExternalFactory_Find); ++n;
    RegisterRuntimeExport("?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z",
                          (void*)&RtObject_ExternalFactory_FindNext); ++n;
    RegisterRuntimeExport("?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z",
                          (void*)&RtObject_ExternalFactory_GetReferenceId); ++n;
    RegisterRuntimeExport("?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z",
                          (void*)&RtObject_ExternalFactory_GetPathForReferenceId); ++n;
    RegisterRuntimeExport("?GetResourceCount@ExternalList@Object@@QEBAHXZ",
                          (void*)&RtObject_ExternalList_GetResourceCount); ++n;
    RegisterRuntimeExport("?Find@ExternalList@Object@@QEBA?AV?$ExternalPtr@VExternal@Object@@@2@"
                          "AEBVFilename@System@@VType@2@@Z",
                          (void*)&RtObject_ExternalList_Find); ++n;

    RegisterRuntimeAddr(0x1800bc9c0, (void*)&RtObject_External_Ctor);
    RegisterRuntimeAddr(0x1800bc950, (void*)&RtObject_External_Dtor);
    RegisterRuntimeAddr(0x180006250, (void*)&RtObject_External_GetPath);
    RegisterRuntimeAddr(0x1800066e0, (void*)&RtObject_External_Compile);
    RegisterRuntimeAddr(0x1800bcf40, (void*)&RtObject_ExternalFactory_EnableReferenceMode);
    RegisterRuntimeAddr(0x1800bf0f0, (void*)&RtObject_ExternalFactory_IsReferenceModeEnabled);
    RegisterRuntimeAddr(0x1800bdeb0, (void*)&RtObject_ExternalFactory_Find);
    RegisterRuntimeAddr(0x1800be9d0, (void*)&RtObject_ExternalFactory_FindNext);
    RegisterRuntimeAddr(0x1800bf100, (void*)&RtObject_ExternalFactory_GetReferenceId);
    RegisterRuntimeAddr(0x1800bf1d0, (void*)&RtObject_ExternalFactory_GetPathForReferenceId);
    RegisterRuntimeAddr(0x180007650, (void*)&RtObject_ExternalList_GetResourceCount);
    RegisterRuntimeAddr(0x1800bf700, (void*)&RtObject_ExternalList_Find);

    // The Wave 3 banner named four deliberate omissions. All four have since
    // been ported by other files and this line was still claiming otherwise:
    //   Clean                     external_set.cpp        (Wave 9 lane RES)
    //   FindOrLoad                external_compiled.cpp   (Wave 11 lane OBJEXT)
    //   SaveText                  external_savetext_win.cpp (Wave 11 f/u 2)
    //   SerializeReferenceTable   external_reftable.cpp   (Wave 11 f/u 3, AVL)
    // A stale banner is how a lane gets sent to port what the tree already has
    // (reference_grep_for_the_port_before_writing_it.md), so it is corrected
    // here rather than left for the next reader.
    printf("[RtPort]   External: %d exports. The four rows this line used to "
           "call \"deliberately NOT ported\" -- Clean, FindOrLoad, SaveText, "
           "SerializeReferenceTable -- are ALL ported now, in external_set.cpp, "
           "external_compiled.cpp, external_savetext_win.cpp and "
           "external_reftable.cpp respectively.\n", n);
    return n;
}
