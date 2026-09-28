#pragma once
// ============================================================================
// runtime/types/object/External.h — Object::External / ExternalFactory /
//                                   ExternalList / ExternalPtr layouts
// ============================================================================
//
// Wave 3, lane C of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2).
// Every offset below was read out of a decompiled Runtime-x64.dll function
// (image base 0x180000000, read 2026-09-06) and the naming function is quoted
// next to it. The full write-up, with the per-field evidence table and the
// confidence levels, is docs/runtime_port/WAVE3_LANE_C.md.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
//
// WHAT THIS FAMILY IS
// -------------------
// An `Object::External` is one loaded resource file: a path plus whatever the
// concrete subclass parsed out of it. `Object::ExternalFactory` is the global
// resource table — a singleton at 0x1801fc6c8 that owns two AVL trees, one
// keyed by path and one keyed by a serialisation "reference id".
// `Object::ExternalList` is itself an External whose payload is a flat array
// of other Externals (the mod list, the help-topic list, the scenario list,
// the map-generator list, ...).
//
// `Object::ExternalPtr<T>` is ONE RAW POINTER, eight bytes, with no refcount:
// ExternalList::Find (0x1800bf700) returns one by value and its whole payload
// write is a single `*retval = _data[i]` qword store, and the not-found path is
// `*retval = 0`. Ownership lives in the factory, never in the pointer.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"
#include "../system/Filename.h"
// ENDGAME B.3.3 step 5: hoisted from the `#include "TypeInfo.h"` that sits at
// FILE SCOPE INSIDE `namespace RtObject` further down (the Wave 3 integration
// block). That placement only ever worked because every real TU reached
// TypeInfo.h first; a TU that reached External.h first would have landed
// `Object::Base` in `RtObject::Object::Base` and this file's own `namespace
// Object` block would then have failed to find it. The lower include is now a
// `#pragma once` no-op and is left in place with its explanatory banner.
#include "TypeInfo.h"

namespace RtObject {

using RtSystem::Filename;

// ============================================================
// Object::ExternalPtr<T> — one raw pointer, no refcount.
//
// PINNED by ?Find@ExternalList@Object@@QEBA?AV?$ExternalPtr@VExternal@Object@@@2@...
// (0x1800bf700), which returns one BY VALUE through a hidden return slot and
// whose entire payload write is `*(qword*)retval = list->_data[i]` on a hit and
// `*(qword*)retval = 0` on a miss. Nothing bumps a refcount on the way out.
// ============================================================
template <class T>
struct ExternalPtr {
    T* ptr;   // +0x00
};
static_assert(sizeof(ExternalPtr<void>) == 8, "ExternalPtr<T> is one pointer");

// ============================================================
// Object::External — vtable + path + a 0x10-stride owning array.
//
// PINS
//   ??0External@Object@@QEAA@XZ  (0x1800bc9c0) stamps ??_7Base@Object@@6B@ and
//     then ??_7External@Object@@6B@ over +0x00 and zeroes +0x08, +0x10, +0x18.
//     Base is therefore 8 bytes — a vtable pointer and nothing else — and
//     External adds exactly 0x18.
//   ?GetPath@External@Object@@QEBA?AVFilename@System@@XZ (0x180006250) is one
//     line: `Filename::Filename(retval, (Filename*)(this + 8))`. So +0x08 is a
//     System::Filename, i.e. the same refcounted char* String.h describes.
//   ??1External@Object@@UEAA@XZ  (0x1800bc950) hands `this + 0x10` to
//     FUN_180006b80, which is the Array<T> teardown: destroy [0, _size) via
//     FUN_180006ac0, Free the buffer under the allocator's +0x640 lock, then
//     zero +0x00 and the size/capacity qword together. FUN_180006ac0 walks with
//     a stride of 0x10 (`param_2 = param_2 + 2` on a longlong*) and releases
//     only the FIRST qword of each element with the String release idiom.
//
// So +0x10 is a Collection::Array<T> whose T is 0x10 bytes and whose leading
// member is a String/Filename. WHAT that element is has NOT been pinned: no
// decompiled body in this lane's scope writes it, only the destructor reads it.
// It is declared here as an opaque 0x10-byte pair so the sizes are right and
// the destructor can be transcribed faithfully; naming it is future work.
// (`Object\External.cpp` exists in the profile build's source-tree strings, so
// a name is recoverable — see docs/runtime_port/original_source_tree.txt.)
//
// VIRTUAL SLOTS used by this lane (byte offsets into the vtable):
//   +0x10  GetTypeId() -> uint32           (ExternalFactory::FindNext, SaveText)
//   +0x28  Serialize(IO::Stream&)          (ExternalFactory::Add, SaveText)
// Slot +0x28 is named by two independent uses: SaveText (0x1800bb9d0) drives it
// with an IO::TextFileStream to write the object out as text, and
// ExternalFactory::Add (0x1800be570) drives it with an IO::VisitorStream to
// walk the object's resource references. See the FindOrLoad caching note in
// runtime/src/object/external.cpp.
// ============================================================
// WAVE 27 LANE FRONTIER: the element is now pinned by THREE independent
// writers, and they do not contradict each other - they split +0x08 differently
// because they are doing different jobs.
//   * FUN_180006ac0 (the range destroy, now ported as kD_Str_10) touches ONLY
//     +0x00 and steps 0x10 (`ADD RBX,0x10` at 0x180006b5f).
//   * FUN_18002c6a0 (`Array<ExternalArrayElement>::SetSize`, now ported as
//     kSpec_ExtArrayElem) copies +0x00 through System::Filename's copy ctor
//     0x1800067d0 and then +0x08..+0x0f as ONE qword (0x18002c779).
//   * Object::External::Load's dependency replay serializes +0x00 as a String
//     and then TWO int32s, at +0x08 and +0x0c (0x1800bc7fc / 0x1800bc811).
// So the tail IS two int32s; the array simply never needs to know that. The
// field stays spelled as one qword because that is what the ARRAY writes, and
// external_load.cpp is where the split is made, with its own citation.
struct ExternalArrayElement {
    char*    owned;   // +0x00 — a System::Filename: released with the String
                      //         idiom by FUN_180006ac0, copied (and delimiter-
                      //         normalised) by 0x1800067d0
    uint64_t opaque;  // +0x08 — two int32s to the serializer; never touched by
                      //         the destructor, copied whole by SetSize
};
static_assert(sizeof(ExternalArrayElement) == 0x10,
              "the destructor's element stride is 0x10 (FUN_180006ac0), and "
              "SetSize's SHL RCX,0x4 at 0x18002c70a agrees");
static_assert(offsetof(ExternalArrayElement, owned)  == 0x00,
              "ExternalArrayElement::owned @ +0x00 (MOV RAX,[RBX] at 0x180006b0a)");
static_assert(offsetof(ExternalArrayElement, opaque) == 0x08,
              "ExternalArrayElement::opaque @ +0x08 (MOV [R14+0x8],RAX at 0x18002c77d)");

struct External {
    void*                              _vftable;   // +0x00
    Filename                           _path;      // +0x08
    Collection::Array<ExternalArrayElement> _refs; // +0x10 data, +0x18 size, +0x1c capacity
};
static_assert(sizeof(External) == 0x20, "Object::External is 0x20 bytes");
static_assert(offsetof(External, _vftable) == 0x00, "External vtable @ +0x00");
static_assert(offsetof(External, _path)    == 0x08, "External::_path @ +0x08 (GetPath 0x180006250)");
static_assert(offsetof(External, _refs)    == 0x10, "External::_refs @ +0x10 (~External 0x1800bc950)");

// ============================================================
// Object::ExternalFactory — the global resource table.
//
// Singleton: ?_singleton@ExternalFactory@Object@@0PEAV12@EA at 0x1801fc6c8 (note
// the `0PEAV12@EA` spelling -- the obvious `1PEAV12@A` guess is not an export),
// assigned by the constructor ??0ExternalFactory@Object@@QEAA@XZ (0x1800be7e0),
// which is also the field-by-field pin for everything below: it writes a qword
// of zero at +0x00, an int32 at +0x08, a qword at +0x10, an int32 at +0x18, a
// qword at +0x20, an int32 at +0x28, qwords at +0x30 and +0x38, an int16 at
// +0x40 (so +0x40 and +0x41 are two adjacent bools) and a byte at +0x42.
//
//  +0x00 int32  _cleanBitIndex
//        Object::ExternalFactory::Clean (0x1800be310) reads it as a SIGNED int
//        and uses it two ways: when >= 0 it clears bit `1 << (idx & 31)` from
//        each entry's usage mask and evicts the entries whose mask reaches 0;
//        when < 0 it evicts EVERYTHING unconditionally. Add (0x1800be570) sets
//        the same bit on the entry it touches.
//  +0x04 uint32 _cleanBitMask
//        *** CORRECTED BY WAVE 8 LANE A. This field was declared `_pad04` and
//        described as "zeroed with +0x00 as one qword". The zeroing claim is
//        true — the ctor does write one qword — but the word is not padding:
//        it is the ALLOCATION MASK for the 32 clean-bit slots, and the pair of
//        functions that own it is in Engine, which is why Wave 3 saw no writer.
//          Engine::UpdateLoop::UpdateLoop (0x1800ddd30) scans it with BT for
//          the lowest CLEAR bit, sets that bit, and stores the bit's INDEX in
//          UpdateLoop::_loadBit (+0x20), or stores -1 when all 32 are taken.
//          Engine::UpdateLoop::~UpdateLoop (0x1800dddb0) writes that index into
//          _cleanBitIndex above, calls Clean(), and then clears the bit here.
//        So the engine supports at most 32 simultaneously live UpdateLoops, and
//        an UpdateLoop's "load bit" is a slot rented out of this word.
//  +0x08 int32  _generation
//        Clean increments it once per sweep. This is the counter the exe's
//        ResolveExternalByName cache compares against — see the ExternalRef
//        layout comment in engine/funcs/engine_funcs.h, which describes "+0x08:
//        cached generation counter (compared against ExternalFactory+0x08)".
//        That comment is CONFIRMED by this port.
//  +0x10 Set<ResourceEntry> _resources    root +0x10, count +0x18
//  +0x20 Set<ReferenceEntry> _references  root +0x20, count +0x28
//  +0x30, +0x38  NOT PINNED by this lane. Zeroed by the ctor; no function in
//        this lane's scope reads or writes either. Declared as two opaque
//        qwords so the later offsets are right.
//  +0x40 bool _referenceModeEnabled  — EnableReferenceMode 0x1800bcf40 writes,
//        IsReferenceModeEnabled 0x1800bf0f0 reads.
//  +0x41 bool _loadEnabled           — ?EnableLoad@…@QEAAX_N@Z (0x1800bcaf0).
//        FindOrLoad's CACHE-HIT branch calls Add() again only when this is set.
//  +0x42 bool _simpleLoadEnabled     — ?EnableSimpleLoad@…@QEAAX_N@Z (0x1800be9c0).
// ============================================================

// One row of the path-keyed tree. sizeof == 0x18, so the Collection::Set node
// tail lands at +0x18 parent / +0x20 left / +0x28 right — exactly the offsets
// FindNext (0x1800be9d0), Clean (0x1800be310) and SerializeReferenceTable
// (0x1800bcb60) all walk.
struct ResourceEntry {
    Filename  path;         // +0x00 — the Set key; compared with _stricmp
    External* external;     // +0x08 — Find/FindNext return this
    uint32_t  usageMask;    // +0x10 — Add ORs in 1 << factory->_cleanBitIndex
    uint32_t  referenceId;  // +0x14 — GetReferenceId returns this; Add seeds -1
};
static_assert(sizeof(ResourceEntry) == 0x18, "ResourceEntry is 0x18 bytes");
static_assert(offsetof(ResourceEntry, external)    == 0x08, "ResourceEntry::external @ +0x08");
static_assert(offsetof(ResourceEntry, usageMask)   == 0x10, "ResourceEntry::usageMask @ +0x10");
static_assert(offsetof(ResourceEntry, referenceId) == 0x14, "ResourceEntry::referenceId @ +0x14");

// One row of the id-keyed tree, rebuilt from the stream by
// SerializeReferenceTable. sizeof == 0x10, so the node tail lands at +0x10
// parent / +0x18 left / +0x20 right — the offsets GetPathForReferenceId
// (0x1800bf1d0) descends through (`piVar5[6]` and `piVar5[8]` on an int*).
struct ReferenceEntry {
    uint32_t id;    // +0x00 — the Set key, compared with plain integer <
    uint32_t _pad;  // +0x04
    Filename path;  // +0x08 — GetPathForReferenceId copies this out
};
static_assert(sizeof(ReferenceEntry) == 0x10, "ReferenceEntry is 0x10 bytes");
static_assert(offsetof(ReferenceEntry, path) == 0x08, "ReferenceEntry::path @ +0x08");

// The two Set instantiations. NOTE THE WAVE 2 TRAP (WAVE2.md "Notes for later
// waves", reference_one_bst_find_two_node_layouts.md): the node's pointer tail
// offsets are a function of sizeof(T), so these two trees have DIFFERENT node
// layouts and MSVC emits a separate find/insert/rebalance for each. One ported
// Set body cannot serve both.
// (ENDGAME B.3.2 step 1: `Collection::Set` is now a class and its parameter
// order is <T, Compare, Allocator, Key> — Compare second, because that is the
// one a caller varies. Retail's mangled name spells it <T, Allocator, Key,
// Compare>; the four parameters and their meanings are unchanged.)
using ResourceSet  = Collection::Set<ResourceEntry,  Collection::AVLCompare,
                                     Collection::DefaultAllocator, Filename>;
using ReferenceSet = Collection::Set<ReferenceEntry, Collection::AVLCompare,
                                     Collection::DefaultAllocator, uint32_t>;
static_assert(sizeof(ResourceSet::Node)  == 0x38, "ResourceEntry node is 0x38");
static_assert(offsetof(ResourceSet::Node, parent) == 0x18, "resource node parent @ +0x18");
static_assert(offsetof(ResourceSet::Node, left)   == 0x20, "resource node left @ +0x20");
static_assert(offsetof(ResourceSet::Node, right)  == 0x28, "resource node right @ +0x28");
static_assert(sizeof(ReferenceSet::Node) == 0x30, "ReferenceEntry node is 0x30");
static_assert(offsetof(ReferenceSet::Node, parent) == 0x10, "reference node parent @ +0x10");
static_assert(offsetof(ReferenceSet::Node, left)   == 0x18, "reference node left @ +0x18");
static_assert(offsetof(ReferenceSet::Node, right)  == 0x20, "reference node right @ +0x20");

// ============================================================
// Object::ExternalFactory's PATH STACK — +0x30/+0x38, PINNED BY WAVE 9 LANE
// RRES. This block used to read `_opaque30 / _opaque38 — NOT PINNED`, and that
// gap was the stated blocker for `ExpandRelativePath` in both WAVE9_LANE_RES.md
// §5 and WAVE9_LANE_FNHOOK.md §4.3.
//
// It is a plain `Collection::Array<PathStackEntry>` — _data +0x30, _size +0x38,
// _capacity +0x3c — and three bodies pin it independently:
//
//   ?GetPathStackFilename@ExternalFactory@Object@@QEBA?AVFilename@System@@XZ
//   (0x1800bcf60):
//       CMP dword ptr [RCX + 0x38],0      ; the SIZE, read as an int32
//       JNZ ...                            ; empty stack -> return a null Filename
//       MOV EAX,dword ptr [RCX + 0x38]
//       DEC EAX ; MOVSXD RDX,EAX ; SHL RDX,0x4   ; index (size-1) * 0x10
//       ADD RDX,0x8                              ; ... + 8  <-- the SECOND field
//       ADD RDX,qword ptr [RCX + 0x30]           ; ... + _data
//       CALL 0x1800067d0                         ; Filename copy ctor
//
//   ?GetPathStackDirectory@...@ (0x1800bcfd0) is the SAME body without the
//   `ADD RDX,0x8`, so the FIRST field is the directory.
//
//   ?PushPath@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z (0x1800bd0e0)
//   builds a 0x10-byte local out of two ADJACENT stack slots — [RBP-0x18] takes
//   a `Filename` copy of GetPathDirectory(path) and [RBP-0x10] takes
//   `GetPathFilename(path,true) + "." + GetPathExtension(path)` — and hands
//   `&[RBP-0x18]` to 0x18002e220 with RCX = `this + 0x30` and EDX = size + 1.
//   That is `Array<PathStackEntry>::Resize(size+1, entry)`, and it fixes the
//   stride at 0x10 and the field order from the WRITE side.
//
// So the entry is two Filenames, directory first. The two halves are read by
// separate accessors, so it is spelled as a struct rather than Filename[2].
// ============================================================
struct PathStackEntry {
    Filename directory;  // +0x00 — GetPathStackDirectory copies this out
    Filename filename;   // +0x08 — GetPathStackFilename copies this out
};
static_assert(sizeof(PathStackEntry) == 0x10,
              "the path-stack stride is 0x10 (SHL RDX,0x4 in both accessors)");
static_assert(offsetof(PathStackEntry, filename) == 0x08,
              "GetPathStackFilename's extra `ADD RDX,0x8`");

struct ExternalFactory {
    int32_t      _cleanBitIndex;         // +0x00
    uint32_t     _cleanBitMask;          // +0x04 — see the note above
    int32_t      _generation;            // +0x08
    int32_t      _pad0c;                 // +0x0c
    ResourceSet  _resources;             // +0x10
    ReferenceSet _references;            // +0x20
    // +0x30 _data, +0x38 _size, +0x3c _capacity. The constructor's two zeroed
    // qwords at +0x30/+0x38 are exactly this container's three fields.
    Collection::Array<PathStackEntry> _pathStack;
    bool         _referenceModeEnabled;  // +0x40
    bool         _loadEnabled;           // +0x41
    bool         _simpleLoadEnabled;     // +0x42
};
static_assert(offsetof(ExternalFactory, _resources)            == 0x10, "_resources @ +0x10");
static_assert(offsetof(ExternalFactory, _references)           == 0x20, "_references @ +0x20");
static_assert(offsetof(ExternalFactory, _pathStack)            == 0x30, "_pathStack @ +0x30");
static_assert(offsetof(ExternalFactory, _pathStack) + offsetof(Collection::Array<PathStackEntry>, _size)
              == 0x38, "path-stack SIZE @ +0x38 (GetPathStack*'s `CMP dword [RCX+0x38],0`)");
static_assert(offsetof(ExternalFactory, _referenceModeEnabled) == 0x40, "_referenceMode @ +0x40");
static_assert(offsetof(ExternalFactory, _loadEnabled)          == 0x41, "_loadEnabled @ +0x41");
static_assert(offsetof(ExternalFactory, _simpleLoadEnabled)    == 0x42, "_simpleLoad @ +0x42");

// ============================================================
// Object::ExternalList — an External whose payload is an array of External*.
//
// The two pinned offsets are EXACTLY where Collection::Array<External*> would
// put them (_data +0x20, _size +0x28, _capacity +0x2c), so this is almost
// certainly that container rather than a hand-rolled pair. It is declared as
// the plain fields below because only two of the three are pinned: nothing in
// this lane's scope reads or writes +0x2c, and asserting a capacity field that
// no decompiled body touches would be a guess dressed as a layout.
//
// PINS, both from ?Find@ExternalList@Object@@…(0x1800bf700):
//   the loop bound is `*(int*)(this + 0x28)`
//   the element read is `*(qword*)(*(qword*)(this + 0x20) + i * 8)`
// and ?GetResourceCount@ExternalList@Object@@QEBAHXZ (0x180007650) is the
// one-line `return *(int*)(this + 0x28)`.
//
// The base sub-object is External, so +0x00..+0x1f is the External above and
// the derived part starts exactly at +0x20. The vtable is exported as
// ??_7ExternalList@Object@@6B@ (0x180154800).
//
// +0x2c is NOT PINNED. `_capacity` is the plausible reading, given both the
// offsets above and every other array in this engine, but no decompiled body in
// scope touches it.
// ============================================================
struct ExternalList {
    External   _base;      // +0x00
    External** _resources; // +0x20
    int32_t    _count;     // +0x28
    int32_t    _pad2c;     // +0x2c — NOT PINNED; probably Array<T>::_capacity
};
static_assert(offsetof(ExternalList, _resources) == 0x20, "ExternalList::_resources @ +0x20");
static_assert(offsetof(ExternalList, _count)     == 0x28, "ExternalList::_count @ +0x28");

// ============================================================
// Factory type-map offsets — MERGED INTO LANE A'S HEADER (Wave 3 integration).
//
// ExternalFactory::FindNext (0x1800be9d0) inlines an IsDerivedFrom walk over
// Object::Factory's type map, and External::SaveText (0x1800bb9d0) reads a type
// NAME out of the same map. This lane originally declared six offsets locally,
// transcribed without interpretation, rather than edit Lane A's file mid-wave.
//
// Lane A independently pinned the same structure from the other side (Factory::
// Startup, CreateObject, GetTypeIdFromName, IsDerivedFrom) and the two readings
// agree field for field, so the local block is gone and TypeInfo.h is the one
// source of truth:
//
//   this lane's `element`       = RtObject::TypeNode  (the node base)
//   kTypeId        +0x00        = TypeNode::key
//   kLeft +0x38 / kRight +0x40  = TypeNode::left / ::right
//   kAncestorChain +0x08        = TypeNode::value, a TypeRecord
//   kAncestorNext  +0x10        = TypeRecord::parent — the "ancestor chain"
//                                 this lane saw walked to null IS the
//                                 base-class chain
//   kTypeInfoPtr   +0x10        = TypeRecord::info, and its +0x08 is
//                                 BaseTypeInfo::name — so the pointer this lane
//                                 declined to name IS a BaseTypeInfo*
//
// ENDGAME B.3.3 step 7: the `#include "TypeInfo.h"` that used to sit here — and
// that fired from INSIDE `namespace RtObject`, which is why internal_class.h was
// ever a separate header — is deleted. Step 5 hoisted it to line 43 at global
// scope, so this one had been inert ever since.
// ============================================================

namespace FactoryTypeNodeOffsets {
    // Kept as names so external.cpp's byte-offset arithmetic reads unchanged;
    // every value is now DERIVED from Lane A's measured layout, not restated.
    inline constexpr int kTypeId        = offsetof(RtObject::TypeNode, key);
    inline constexpr int kAncestorChain = offsetof(RtObject::TypeNode, value);
    inline constexpr int kAncestorNext  = offsetof(RtObject::TypeRecord, parent);
    inline constexpr int kTypeInfoPtr   = offsetof(RtObject::TypeNode, value)
                                        + offsetof(RtObject::TypeRecord, info);
    inline constexpr int kLeft          = offsetof(RtObject::TypeNode, left);
    inline constexpr int kRight         = offsetof(RtObject::TypeNode, right);
}

// The values Lane C transcribed from ExternalFactory's disassembly, asserted
// against Lane A's independently measured layout. Two readings of the same
// bytes from two different call sites; if either ever moves, this fails loudly.
static_assert(FactoryTypeNodeOffsets::kTypeId        == 0x00, "type-map key");
static_assert(FactoryTypeNodeOffsets::kAncestorChain == 0x08, "type-map value");
static_assert(FactoryTypeNodeOffsets::kAncestorNext  == 0x10, "TypeRecord::parent");
static_assert(FactoryTypeNodeOffsets::kTypeInfoPtr   == 0x10, "TypeRecord::info");
static_assert(FactoryTypeNodeOffsets::kLeft          == 0x38, "type-map left");
static_assert(FactoryTypeNodeOffsets::kRight         == 0x40, "type-map right");

}  // namespace RtObject

// ============================================================================
// ENDGAME B.3.1 step 1 — the REAL class, under retail's name.
// ============================================================================
//
// `RtObject::ExternalPtr<T>` above stays declared: it is the POD that
// not-yet-converted structs (Achievement::Goal::_stringTable, ...) and the C
// bodies still spell, and B31_RULES step 1 keeps every existing vocabulary
// working. `Object::ExternalPtr<T>` is the same eight bytes with retail's
// members on them; step 2 moves the callers over one directory at a time and
// the POD is deleted when its last user is gone.
//
// TRIVIALLY COPYABLE AND TRIVIALLY DESTRUCTIBLE, ON PURPOSE AND BY ASSERTION.
// `Collection::Array<{String, ExternalPtr}>` bulk-moves these eight bytes with
// a plain qword copy and an explicit "NO refcount touch"
// (runtime/src/collection/array.cpp:983). A user-provided copy constructor or
// destructor here would be SILENTLY BYPASSED by every array resize in the
// engine. There is nothing to run on copy in any case: the pointer owns
// nothing -- `Object::ExternalFactory`'s two AVL trees own every `External*`
// (docs/runtime_port/endgame_b/SMARTPTR_INVENTORY.md sections 1, 5.2, 5.4).
// This is a non-owning view, not a `shared_ptr`.
//
// RETAIL ADDRESSES. The only non-trivial member is `Serialize`, and it is ONE
// 748-byte body emitted 22 times plus ONE 818-byte array body emitted 8 times;
// the per-T difference is the two `MOV EDX,[rip+?_typeInfo@T@...]` loads and
// nothing else. Re-verified 2026-09-13 across every registered address with
// tools/extptr_clone_check.py (bounded to the real function size, because the
// tool's own "stop at the first RET" cut over-runs a body that ends in a tail
// JMP and then reports a bogus instruction-count mismatch):
//   22/22 single-pointer  identical=193  position-only=20  REAL=2
//    8/8  array           identical=212  position-only=20  REAL=2
// every REAL pair being the type-info load. ONE BODY, confirmed for the whole
// family, not just the four the pairwise tool had covered.
//
//   single-pointer form, `void Serialize(IO::Serializer&)`, per T:
//     0x1800042a0 UI::StringTable          0x180038f90 UI::ElementDescription
//     0x180076610 UI::Dialog               0x180005dc0 Achievement::Goal
//     0x180004590 UI::SpriteSheet          0x180014520 Graphics::MaterialInstance
//     0x18005d0f0 Video::ImageBuffer       0x180061100 Graphics::Material
//     0x180014230 Graphics::GraphicsMesh   0x1800756d0 UI::DialogControllerConfig
//     0x180051790 Audio::SoundEffect       0x18005b840 Graphics::AnimationGroup
//     0x180006d10 Achievement::Achievement 0x180055090 Video::VertexLayout
//     0x1800542a0 Video::VertexProgram     0x180055380 Video::PixelProgram
//     0x180055670 Video::RenderState       0x18003f1e0 UI::Font
//     0x1800594a0 Graphics::AnimationEvent 0x180076e90 UI::CharacterSet
//     0x180052a50 Audio::SoundBuffer       0x18003dc90 Object::ExternalList
//   array form, `static void SerializeArray(IO::Serializer&, int, ExternalPtr*)`:
//     0x18002f170 Object::External         0x180005220 Achievement::Goal
//     0x1800493d0 UI::ElementDescription   0x18005d4a0 Graphics::MaterialInstance
//     0x1800605e0 Video::VideoProgram      0x18003e710 UI::Font
//     0x18003eb60 UI::SpriteSheet          0x1800667f0 Collision2D::Surface
//
// There is NO `CheckPtr`, NO `AssignId` and NO `Clone` for this family: neither
// the export table nor any decompiled body defines one, because there is no
// refcount to check or clone (SMARTPTR_INVENTORY.md section 1).
// ============================================================================

#include <type_traits>

namespace rt { namespace io { struct Serializer; } }

// B.3.4 STREAMS (5/6) — `ExternalFactory::SerializeReferenceTable` takes
// retail's own `IO::Stream&`. A FORWARD DECLARATION and not an include of
// runtime/types/io/Stream.h, deliberately: the parameter is a reference, so an
// incomplete type is all the declaration needs, and this header is included by
// most of runtime/src/object/. Pulling the whole IO stream family in here would
// put `IO::Stream`, `IO::MemoryStream` and the Compressed branch into every one
// of those translation units to satisfy one parameter — and, worse for section
// 7.2, it would arm the bare-name grep hazard in files that have no other
// reason to see those classes. The one file that forwards the call
// (external_class_doors.cpp) includes Stream.h itself.
namespace IO { class Stream; }

namespace Object {

// The one shared body, defined in runtime/src/object/external_ptr_serialize.cpp
// next to the dependency doors it needs (GetPath / FindOrLoad / IsDerivedFrom /
// GetReferenceId / GetPathForReferenceId / String::Serialize, all still reached
// through RT_EXPORT_ONCE because none of those classes is converted yet --
// SMARTPTR_INVENTORY.md section 4). `typeInfo` is this instantiation's
// `?_typeInfo@T@...` record; its FIRST DWORD is the type id and it is read at
// call time, as retail does (the id is 0xFFFFFFFF until Factory::Startup).
void ExternalPtrSerializeBody(RtObject::External** self, rt::io::Serializer& ser,
                              const void* typeInfo);
void ExternalPtrSerializeArrayBody(rt::io::Serializer& ser, int count,
                                   RtObject::External** elems, const void* typeInfo);

template <class T>
class ExternalPtr {
public:
    // Retail emits no out-of-line constructor for this template -- a bare
    // pointer member is initialised inline at every construction site, and
    // `ExternalList::Find` (0x1800bf700) writes the not-found case as a single
    // `*retval = 0` qword store. Zeroing is therefore retail's default.
    ExternalPtr() : _ptr(nullptr) {}
    ExternalPtr(T* p) : _ptr(p) {}

    // Copy, assign and destroy are the compiler's, and MUST stay trivial -- see
    // the banner. Retail had no move (C++03) and a pointer steal would be
    // observationally identical to a copy here, so none is added.
    ExternalPtr(const ExternalPtr&) = default;
    ExternalPtr& operator=(const ExternalPtr&) = default;
    ~ExternalPtr() = default;

    ExternalPtr& operator=(T* p) { _ptr = p; return *this; }

    T*   Get() const        { return _ptr; }
    T*   operator->() const { return _ptr; }
    T&   operator*()  const { return *_ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

    bool operator==(const ExternalPtr& o) const { return _ptr == o._ptr; }
    bool operator!=(const ExternalPtr& o) const { return _ptr != o._ptr; }

    // ?Serialize@?$ExternalPtr@...@Object@@QEAAXAEAVSerializer@IO@@@Z -- the one
    // template body; the only per-T instruction is the `typeInfo` load, which
    // the caller supplies until `T` is a real C++ class with a `_typeInfo`
    // static of its own (ENDGAME section B.3 step 3, decision 4).
    void Serialize(rt::io::Serializer& ser, const void* typeInfo) {
        // TRANSITIONAL CAST (B31_RULES "not-yet-converted structs"): none of the
        // 26 `T`s is a C++ class in this tree yet, so the shared body is typed
        // over `Object::External**`. It goes when `Object::External`'s hierarchy
        // is converted (ENDGAME section B.3 step 3).
        ExternalPtrSerializeBody(reinterpret_cast<RtObject::External**>(&_ptr),
                                 ser, typeInfo);
    }

    static void SerializeArray(rt::io::Serializer& ser, int count,
                               ExternalPtr* elems, const void* typeInfo) {
        ExternalPtrSerializeArrayBody(
            ser, count, reinterpret_cast<RtObject::External**>(elems), typeInfo);
    }

    T* _ptr;   // +0x00 -- public: the array bulk-copy paths reach these bytes
               //         directly and retail's own layout is one bare pointer.
};

static_assert(sizeof(ExternalPtr<RtObject::External>) == 8,
              "Object::ExternalPtr<T> is one pointer (ExternalList::Find 0x1800bf700)");
// ...and for every other T, including one that is only forward-declared, which
// is what most of the 26 instantiations are in this tree today.
struct ExternalPtrLayoutProbe;
static_assert(sizeof(ExternalPtr<ExternalPtrLayoutProbe>) == 8, "...for every T");
static_assert(std::is_trivially_copyable<ExternalPtr<RtObject::External> >::value,
              "Array<T>::operator= memcpys these; a real copy ctor would be skipped "
              "(runtime/src/collection/array.cpp:983)");
static_assert(std::is_trivially_destructible<ExternalPtr<RtObject::External> >::value,
              "the array range-destroy never calls one; a real dtor would be skipped");

// ############################################################################
// ENDGAME B.3.3 step 5 (EXTERNAL) — the real classes, under retail's names.
//
// Everything in `namespace RtObject` above STAYS and stays TRIVIALLY
// DESTRUCTIBLE where it already was: B31_RULES step 1 keeps the old PODs
// declared until their last caller converts, and `RtObject::External` in
// particular is held by ported bodies as `RtObject::External tmp;` with
// `RtObject_External_Dtor(&tmp)` called BY HAND — giving that POD a destructor
// would make the compiler run a second one (the `RtSystem::String` double-free
// lesson, B31_RULES, restated in B33_RULES §7).
//
// What follows is the same bytes as real C++, cross-asserted field for field
// against the PODs so the two descriptions cannot drift.
//
// -------------------------------------------------------------------------
// THE SLOT LISTS ARE MEASURED. `??_7External@Object@@6B@` @ 0x1801547a0, read
// out of Runtime-x64.dll on 2026-09-18 (8 qwords, then a zero qword before the
// next object — B33_RULES §1.4 independently bounds it at eight):
//
//   [0] +0x00  0x180006640  vector deleting dtor (elem 0x20, ~External
//                           0x1800bc950) — deleting_dtor.cpp's own row agrees
//   [1] +0x08  0x1800066c0  `MOV RAX,qword[0x1801f9b40]; RET`
//                           = ?TypeName@External@Object@@UEBAPEBDXZ
//   [2] +0x10  0x1800066d0  `MOV EAX,dword[0x1801f9b38]; RET`
//                           = ?Type@External@Object@@UEBAIXZ
//   [3] +0x18  0x18007ec28  _purecall -> GetFileVersion
//   [4] +0x20  0x1800066e0  `MOV EAX,0x1; RET`
//   [5] +0x28  0x18007ec28  _purecall -> Serialize
//   [6] +0x30  0x18007ec28  _purecall -> Reconstruct
//   [7] +0x38  0x18007ec28  _purecall -> Destruct
//
// *** SLOT 4 HAS A RETAIL NAME, AND B33_RULES §1.4 IS OUT OF DATE ON IT. ***
// §1.4 says "name UNKNOWN, keep it as Slot4()". It is
// `?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z`,
// and that is not a guess: `Get xrefs to 0x1800066e0` shows an `[EXTERNAL]
// Entry Point` reference, i.e. the address IS in the DLL's export table, and
// the DLL-era residue traces resolved that exact mangled string to it
// (docs/runtime_port/wave9_lanes.txt:263, wave9_residue_*.txt). The tree has
// carried the name since Wave 9 lane Y — runtime/src/object/external.cpp
// registers it by name AND at 0x1800066e0, and code/Game/Goal/
// goal_exe_helpers.cpp:218 resolves it by name. Renaming it `Slot4()` would
// throw away a measured export name and contradict decision 5 ("names are the
// original ones"), so the retail name is kept and §1.4 is what needs the
// correction.
//
// SLOTS 6 AND 7 ARE NAMED BY POSITION, NOT BY A BODY. They are `_purecall` in
// External itself, so the names come from the one derived vtable this file also
// owns: `??_7ExternalList@Object@@6B@` @ 0x180154800 puts `Reconstruct`
// (0x180007570) at [6] and `Destruct` (0x180007640) at [7], both exported under
// those names. A pure slot's name can only ever be read off an override, and
// this is the override; runtime/src/object/external_list.cpp has carried the
// same reading since VTABLE BATCH 9.
//
// NO VIRTUAL DESTRUCTOR ANYWHERE (decision 2). Slot 0 is
// `virtual Base* DeletingDtor(unsigned)`; deletion is `Object::Destroy(p)`.
// `tests/unit/object_vtable_abi_test.cpp` is the regression test, and it is
// also what licenses these classes to coexist with the MINTED
// `??_7External@Object@@6B@` that ~80 unconverted files still stamp: an object
// wearing the minted table and an object wearing GCC's are interchangeable
// slot for slot. Nothing here retires a `RT_DELDTOR` row (B33_RULES §6.1: the
// row is what FILLS slot 0 of the minted table).
// ############################################################################

class ExternalFactory;

// ============================================================
// Object::External — one loaded resource file. 0x20 bytes.
// ============================================================
class External : public Base {
public:
    // `?A W4CompileResult@External@Object@@` — returned BY VALUE, but a `W4`
    // enum is four bytes, so it comes back in EAX with NO hidden return slot
    // (reference_hidden_retslot_wild_write.md checked, not assumed). The
    // ENUMERATOR NAMES ARE NOT RECOVERABLE from the binary — only the value 1
    // that the base's body returns — so this is spelled as the 32-bit integer
    // it is rather than an enum with invented names.
    using CompileResult = uint32_t;

    // ??0External@Object@@QEAA@XZ  retail 0x1800bc9c0. Retail stamps Base's
    // vptr and then External's; a real constructor chain does exactly that for
    // free, which is why the two stamps are not transcribed here.
    External();

    // ---- the eight slots, in measured order ----
    Base*       DeletingDtor(unsigned flags) override;          // [0] 0x180006640
    const char* TypeName() const override;                      // [1] 0x1800066c0
    uint32_t    Type() const override;                          // [2] 0x1800066d0
    virtual int32_t       GetFileVersion() const = 0;           // [3] pure
    virtual CompileResult Compile(const System::Filename& path);// [4] 0x1800066e0
    virtual bool          Serialize(void* stream) = 0;          // [5] pure
    virtual External*     Reconstruct() = 0;                    // [6] pure
    virtual void          Destruct() = 0;                       // [7] pure

    // ?GetPath@External@Object@@QEBA?AVFilename@System@@XZ  retail 0x180006250
    System::Filename GetPath() const;

    // ?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z
    //   retail 0x1800bb9d0, ordinal 364. ENDGAME B.3.3 step 6A: added because
    //   the caller batch found a call site for it and the member was missing;
    //   the body is unchanged and stays in external_savetext_win.cpp, so this
    //   is a one-line spelling over `RtObject_External_SaveText` exactly like
    //   every other member in external_class_doors.cpp.
    //
    //   THE SECOND PARAMETER IS A `Filename`, NOT A `String`: the mangled name
    //   spells `AEBVFilename@System@@` and then `0`, the backreference to the
    //   SAME type, so both arguments are `System::Filename const&`. Returns
    //   `_N` (bool) in AL — no hidden return slot.
    bool SaveText(const System::Filename& path, const System::Filename& name);

    // The static companion retail's reflection block emits. NOT `DECLARE_TYPE`
    // — see the banner on ExternalTypeInfoRecord() below.
    static uint32_t TypeId();

protected:
    // ??1External@Object@@UEAA@XZ  retail 0x1800bc950. NON-VIRTUAL; reached by
    // DeletingDtor, and by ~ExternalList / ~CompiledExternal as a base dtor.
    ~External();

public:
    // ---- the 0x18 bytes External adds, spelled as RtObject::External spells
    // them, and the two spellings are NOT interchangeable taste:
    //
    //  * `_path` keeps the POD `RtSystem::Filename`, which is trivially
    //    destructible. `System::Filename` is not, and a member of that type
    //    would make the compiler release the path a SECOND time after
    //    ~External's transcribed release — the `RtSystem::String` double-free
    //    lesson (B31_RULES), one level in. ~External releases it by hand
    //    through the one `Sys(...)` bridge, exactly as the C body does.
    //
    //  * `_refs` keeps `Collection::Array<ExternalArrayElement>`, which DOES
    //    have a real `~Array`, and that is deliberate: retail's ~External is a
    //    CALL to FUN_180006b80 -> FUN_180006ac0, i.e. "release each element's
    //    leading String, then free the buffer and zero the header". The second
    //    half IS `~Array` for a trivially-destructible element (its
    //    `spec->destroy` is null, so it runs `ArrayDtorImpl` and nothing
    //    else). So ~External's body does the FIRST half only and the
    //    compiler-emitted member destructor does the second — retail's own
    //    decomposition, with each half run exactly once.
    RtSystem::Filename                              _path;   // +0x08
    Collection::Array<RtObject::ExternalArrayElement> _refs;  // +0x10
};
static_assert(sizeof(External) == 0x20, "Object::External is 0x20 bytes");
static_assert(sizeof(External) == sizeof(RtObject::External),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(External, _path) == offsetof(RtObject::External, _path), "alias _path");
static_assert(offsetof(External, _refs) == offsetof(RtObject::External, _refs), "alias _refs");
static_assert(offsetof(External, _path) == 0x08, "External::_path @ +0x08 (GetPath 0x180006250)");
static_assert(offsetof(External, _refs) == 0x10, "External::_refs @ +0x10 (~External 0x1800bc950)");

// ============================================================
// Object::CompiledExternal — adds no bytes; only a vtable and a destructor.
//
// `??1CompiledExternal@Object@@UEAA@XZ` (0x1800bb980) is seven instructions:
// re-stamp `??_7CompiledExternal@Object@@6B@` (0x180154a30), then TAIL-JMP
// ~External. The re-stamp is not tidying — it is what makes the base dtor
// dispatch on CompiledExternal's own table — and a real destructor chain
// reproduces it for free. `deleting_dtor.cpp:297` records the element size as
// 0x20, i.e. the same as External's: this class adds no data.
// ============================================================
class CompiledExternal : public External {
public:
    // ??0CompiledExternal@Object@@QEAA@XZ  retail 0x1800bb920: Base's and
    // External's vptr, +0x08/+0x10/+0x18 zero (External(), the compiler's),
    // then CompiledExternal's vptr (the compiler's). No store of its own.
    CompiledExternal() = default;

    // [0] 0x18000b7e0 — MSVC's VECTOR deleting destructor (`MOV EDX,0x20` =
    // the element size, `LEA R9,[0x1800bb980]` = ~CompiledExternal), both arms
    // exactly as the retired RT_DELDTOR row ran them. The class's OWN slot 0
    // (ENDGAME B.3.9 OBJ, B39_RULES D1.3): an inherited External::DeletingDtor
    // would run ~External and skip ~CompiledExternal. INLINE so `make unit`
    // can drive both arms.
    Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<CompiledExternal*>(self + static_cast<int64_t>(i) * 0x20)->~CompiledExternal();
            if (flags & 1u) TypeTreeFree(self - 8);
            return reinterpret_cast<Base*>(self - 8);
        }
        this->~CompiledExternal();
        if (flags & 1u) TypeTreeFree(this);
        return this;
    }
    // [1] ?TypeName@CompiledExternal@Object@@UEBAPEBDXZ 0x18000b6b0
    //     `MOV RAX,[_typeInfo+8]`; [2] ?Type@CompiledExternal@Object@@UEBAIXZ
    //     0x18000b690 `MOV EAX,[_typeInfo]` — the class's own record, not
    //     External's (the compiler table used to inherit External's two).
    const char* TypeName() const override;
    uint32_t    Type() const override;

    // ?TypeId@CompiledExternal@Object@@SAIXZ 0x18000b6a0 — `MOV EAX,[_typeInfo]`
    // over the (still minted) record, through the Object accessor slot
    // (external_class.cpp; B39_RULES D6 step 1).
    static uint32_t TypeId();
protected:
    ~CompiledExternal();     // 0x1800bb980
};
static_assert(sizeof(CompiledExternal) == 0x20,
              "RT_DELDTOR(0x18000b7e0, 0x1800bb980, 0x020, \"Object::CompiledExternal\")");

// ============================================================
// Object::ExternalList — an External whose payload is an array of External*.
//
// `??_7ExternalList@Object@@6B@` @ 0x180154800, NINE slots (read 2026-09-18):
//   [0] 0x180007800 vdtor (elem 0x30, ~ExternalList 0x1800076c0)
//   [1] 0x180007370 TypeName        [2] 0x180007350 Type
//   [3] 0x1800bfa10 GetFileVersion (`MOV EAX,3; RET`)
//   [4] 0x1800066e0 External::Compile — INHERITED, the same body
//   [5] 0x1800bfa20 Serialize       [6] 0x180007570 Reconstruct
//   [7] 0x180007640 Destruct        [8] 0x1800075e0 Construct
// so `Construct` is ExternalList's OWN new virtual: External's table stops at
// eight. sizeof is 0x30 (the deleting dtor's `MOV EDX,0x30`).
//
// ENDGAME B.3.9 OBJ (B39_RULES D1/D2): CONCRETE, and every live ExternalList
// wears the COMPILER's table. `CreateNewObject` / `CreateInPlaceObject`
// (retail's factory pair, registered by name and VA from external_list.cpp)
// are MemAlloc + `::new (p) ExternalList()`, replacing the FACTREST step
// table that stamped the minted `??_7ExternalList@Object@@6B@`. The minted
// row, its RT_DELDTOR row 0x180007800, the ??1 door and the seven slot doors
// are retired (tools/rt_mint_retired/obj.txt).
// ============================================================
class ExternalList : public External {
public:
    // ??0ExternalList — retail's stores (the inlined half of the factory pair
    // 0x1800074c3..f4 / Construct 0x1800075e0): Base's and External's vptr and
    // +0x08/+0x10/+0x18 zero (External(), the compiler's), ExternalList's vptr
    // (the compiler's), then +0x20 and +0x28 zero as two qwords (the member
    // initialisers).
    ExternalList() : External(), _resources(nullptr), _count(0), _pad2c(0) {}

    // [0] 0x180007800 — MSVC's VECTOR deleting destructor, both arms exactly
    // as the retired RT_DELDTOR row ran them (deleting_dtor.cpp
    // DeletingDtorCore; `MOV EDX,0x30` = the element size, `LEA R9,
    // [0x1800076c0]` = ~ExternalList): `flags & 2` reads the SIGNED int32
    // count at `this - 8`, destroys the elements DOWNWARD, frees `this - 8` on
    // `flags & 1` and returns it; otherwise ~ExternalList, free on `flags & 1`.
    // Nothing `new[]`s an ExternalList, so the array arm is the faithful
    // unreachable it is in Object::Internal's. INLINE so `make unit` can drive
    // both arms; the key function is TypeName (external_class_doors.cpp).
    Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<ExternalList*>(self + static_cast<int64_t>(i) * 0x30)->~ExternalList();
            if (flags & 1u) TypeTreeFree(self - 8);
            return reinterpret_cast<Base*>(self - 8);
        }
        this->~ExternalList();
        if (flags & 1u) TypeTreeFree(this);
        return this;
    }
    const char*     TypeName() const override;               // [1] 0x180007370
    uint32_t        Type() const override;                   // [2] 0x180007350
    int32_t         GetFileVersion() const override;         // [3] 0x1800bfa10
    bool            Serialize(void* stream) override;        // [5] 0x1800bfa20
    External*       Reconstruct() override;                  // [6] 0x180007570
    void            Destruct() override;                     // [7] 0x180007640
    virtual External* Construct();                           // [8] 0x1800075e0

    // ?GetResourceCount@ExternalList@Object@@QEBAHXZ   retail 0x180007650
    int32_t GetResourceCount() const { return _count; }

    // ?Find@ExternalList@Object@@QEBA?AV?$ExternalPtr@VExternal@Object@@@2@AEBVFilename@System@@VType@2@@Z
    //   retail 0x1800bf700.  *** THE `Type` PARAMETER IS NEVER READ *** — R9 is
    //   dead in the whole body. Kept because the slot's signature demands it.
    ExternalPtr<External> Find(const System::Filename& path, uint32_t type) const;

    // ?TypeId@ExternalList@Object@@SAIXZ 0x180007360 — `MOV EAX,[_typeInfo]`
    // over the (still minted) record, through the Object accessor slot
    // (external_class.cpp; B39_RULES D6 step 1).
    static uint32_t TypeId();

    // Retail's factory pair (B39_RULES D2), defined in external_list.cpp.
    static Base* CreateNewObject();                    // 0x180007460
    static Base* CreateInPlaceObject(void* place);     // 0x180007510

protected:
    // ??1ExternalList 0x1800076c0. NON-VIRTUAL (no virtual dtor until the
    // normal-virtual cutover); the compiler chains ~External.
    ~ExternalList();        // 0x1800076c0

public:
    External** _resources;  // +0x20
    int32_t    _count;      // +0x28
    int32_t    _pad2c;      // +0x2c — NOT PINNED; probably Array<T>::_capacity
};
static_assert(sizeof(ExternalList) == 0x30,
              "RT_DELDTOR(0x180007800, 0x1800076c0, 0x030, \"Object::ExternalList\")");
static_assert(offsetof(ExternalList, _resources) == offsetof(RtObject::ExternalList, _resources), "alias _resources");
static_assert(offsetof(ExternalList, _count)     == offsetof(RtObject::ExternalList, _count),     "alias _count");
static_assert(offsetof(ExternalList, _resources) == 0x20, "ExternalList::_resources @ +0x20");
static_assert(offsetof(ExternalList, _count)     == 0x28, "ExternalList::_count @ +0x28");

// ============================================================
// The two tree row types and the path-stack entry, under retail's names.
//
// They are ALIASES, not re-declarations: `RtObject::ResourceEntry`,
// `ReferenceEntry` and `PathStackEntry` are already fully pinned above, the
// three ported Set families (`rtresset`, `rtrefset`, external_set.h) name them
// directly, and a second declaration of the same fields would be one more
// thing that can drift. B31_RULES asks for the retail NAME; it does not ask for
// a copy of the bytes.
// ============================================================
using ResourceEntry  = RtObject::ResourceEntry;
using ReferenceEntry = RtObject::ReferenceEntry;
using PathStackEntry = RtObject::PathStackEntry;
using ResourceSet    = RtObject::ResourceSet;
using ReferenceSet   = RtObject::ReferenceSet;

namespace detail {

// Retail's tree comparisons all go through the CRT's `_stricmp`, in the "C"
// locale (the DLL never calls setlocale), so plain ASCII case folding. Spelled
// out here for the reason runtime/src/system/filename.cpp spells it out: a
// runtime/ header must not depend on a CRT whose locale behaviour differs
// between toolchains (PLAN.md §4.1).
inline int StriCmpAscii(const char* a, const char* b) {
    for (;;) {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + 32);
        if (ca != cb) return static_cast<int>(ca) - static_cast<int>(cb);
        if (ca == 0)  return 0;
    }
}

// Byte-identical to `System::Filename::operator<` (0x180026cb0), which is what
// every walk site inlines: a null right-hand side orders below everything, a
// null left-hand side orders below any non-null, otherwise case-insensitive.
inline bool FilenameLess(const char* a, const char* b) {
    if (b == nullptr) return false;
    if (a == nullptr) return true;
    return StriCmpAscii(a, b) < 0;
}

// *** TWO NODE LAYOUTS, TWO FINDS — reference_one_bst_find_two_node_layouts.md.
// The resource tree's element is 0x18 wide so its node tail is parent +0x18 /
// left +0x20 / right +0x28; the reference tree's element is 0x10 wide so its
// tail is +0x10 / +0x18 / +0x20. A find written against the wrong layout
// returns the right answer for a key that happens to be the ROOT and is
// permanently wrong one level down, which is why the unit test drives at least
// three keys at two depths through BOTH of these.
//
// These are the bodies runtime/src/object/external.cpp has inlined since Wave 3
// (`ResourceSetFind`, and the integer descent in GetPathForReferenceId), lifted
// here unchanged so the class and the C shims cannot descend differently. They
// are header-inline and touch nothing but pointers, so `make unit` can drive
// them against a hand-built tree with no allocator and no registry.
//
// Each find comes in TWO spellings: one over the tree ROOT and one over the
// `Set` header that forwards to it. That is not convenience — a `Collection::
// Set` object has a real `~Set` that reaches `Collection::detail::SetNodeFree`
// in set_core.cpp, which stands on the address registry and cannot link into
// `make unit`. The root overload is what lets the test drive these bodies at
// all; the Set overload is what the ported call sites use.
inline ResourceSet::Node* ResourceFind(ResourceSet::Node* root, const char* key) {
    ResourceSet::Node* n = root;
    while (n != nullptr) {
        if (FilenameLess(key, n->element.path.data))      n = n->left;
        else if (FilenameLess(n->element.path.data, key)) n = n->right;
        else                                              return n;
    }
    return nullptr;
}
inline ResourceSet::Node* ResourceFind(const ResourceSet* set, const char* key) {
    return ResourceFind(set->_root, key);
}
inline ResourceSet::Node* ResourceLeftmost(ResourceSet::Node* n) {
    while (n->left != nullptr) n = n->left;
    return n;
}
inline ResourceSet::Node* ResourceSuccessor(ResourceSet::Node* n) {
    if (n->right != nullptr) return ResourceLeftmost(n->right);
    ResourceSet::Node* p = n->parent;
    while (p != nullptr && p->right == n) { n = p; p = p->parent; }
    return p;
}
// The reference tree descends on a plain integer compare, not on `_stricmp` —
// the one Set family in this engine that does (reference_set.h's banner).
inline ReferenceSet::Node* ReferenceFind(ReferenceSet::Node* root, uint32_t id) {
    ReferenceSet::Node* n = root;
    while (n != nullptr) {
        if (id < n->element.id)      n = n->left;
        else if (n->element.id < id) n = n->right;
        else                         return n;
    }
    return nullptr;
}
inline ReferenceSet::Node* ReferenceFind(const ReferenceSet* set, uint32_t id) {
    return ReferenceFind(set->_root, id);
}

}  // namespace detail

// ============================================================
// Object::ExternalFactory — the global resource table. NOT an Object::Base:
// `??0ExternalFactory@Object@@QEAA@XZ` (0x1800be7e0) writes no vptr, and the
// class has no exported `??_7`. So no vtable, no reflection record, no
// `DECLARE_TYPE`.
//
// Layout is `RtObject::ExternalFactory`'s, field for field and asserted against
// it. The class is DECLARED here and its members are one-line spellings over
// the already-ported C bodies, which is the whole of what this step changes
// about them:
//
// *** `Add` IS DELIBERATELY A PURE SPELLING CHANGE. *** B33_RULES §0 makes the
// loader-thread AVL-rotate race on `ExternalFactory::Add` its own
// investigation: no lock, no retry, no ordering change, no null check, and no
// silent removal either. `Add` below therefore forwards to the UNCHANGED body
// in runtime/src/object/external_add.cpp rather than taking it over — the
// instruction sequence inside the tree mutation is byte-for-byte the one that
// is under investigation, with one extra (inlinable) call frame in front of it.
// The same reasoning covers `Clean`, `FindOrLoad`, `SerializeReferenceTable`,
// `PushPath`/`PopPath` and `ForceUnload`, every one of which mutates the same
// two trees.
// ============================================================
class ExternalFactory {
public:
    ExternalFactory();

    // ?Get@ExternalFactory@Object@@SAPEAV12@XZ — `return _singleton;`.
    static ExternalFactory* Get();

    // ?_singleton@ExternalFactory@Object@@0PEAV12@EA — retail 0x1801fc6c8 (note
    // the `0PEAV12@EA` spelling; the obvious `1PEAV12@A` is not an export).
    // The CLASS STATIC since ENDGAME B.3.9 OBJ (B39_RULES D7): defined in
    // external_class_doors.cpp, and RtObjectPortInit redirects the minted row
    // onto it, so the name door, the address door and this member are the
    // same eight bytes. Written by the ctor door (engine/singleton_ctors.cpp).
    // PUBLIC while foreign readers still reach the cell through the doors.
    static ExternalFactory* _singleton;

    // ---- the read-only half (ported bodies, runtime/src/object/external.cpp)
    const External* Find(const System::Filename& path) const;      // 0x1800bdeb0
    const External* FindNext(uint32_t typeId, const External* prev) const;  // 0x1800be9d0
    uint32_t         GetReferenceId(const External* external) const;        // 0x1800bf100
    System::Filename GetPathForReferenceId(uint32_t id) const;              // 0x1800bf1d0
    bool             IsReferenceModeEnabled() const { return _referenceModeEnabled; }  // 0x1800bf0f0
    void             EnableReferenceMode(bool enable) { _referenceModeEnabled = enable; }  // 0x1800bcf40
    void             EnableLoad(bool enable)       { _loadEnabled = enable; }        // 0x1800bcaf0
    void             EnableSimpleLoad(bool enable) { _simpleLoadEnabled = enable; }  // 0x1800be9c0

    // ---- the path stack (external_path_stack.cpp / external_path_win.cpp)
    void             PushPath(const System::Filename& path);   // 0x1800bd0e0
    void             PopPath();                                // 0x1800bd040
    System::Filename GetPathStackFilename() const;             // 0x1800bcf60
    System::Filename GetPathStackDirectory() const;            // 0x1800bcfd0
    // ?ExpandRelativePath@ExternalFactory@Object@@QEBA?AVFilename@System@@AEBV34@_N@Z
    // — the trailing `_N` is a FOURTH argument after the hidden return slot.
    System::Filename ExpandRelativePath(const System::Filename& path,
                                        bool appendIdentifier) const;

    // ---- the mutating half. Spelling only; see the banner on `Add`.
    void            Add(External* external);                   // 0x1800be570
    const External* FindOrLoad(const System::Filename& path);  // 0x1800bf320
    void            Clean();                                   // 0x1800be310
    void            ForceUnload(const System::Filename& path);

    // ?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z
    //   retail 0x1800bcb60 — DECLARED AS OF ENDGAME B.3.4 STREAMS (5/6).
    //
    // It was held back by a TYPE, not by a decision about the method: retail's
    // parameter is `IO::Stream&`, and until block 4 there was no such class —
    // only two independently declared duplicate PODs (`rt::io::Stream` in
    // Serializer.h, which the ported entry point takes, and `RtIo::Stream` in
    // Stream.h), neither of which could be named from another translation unit
    // without dragging one header's private spelling into this one.
    // `IO::Stream` now exists and replaces both (B34_RULES section 1.5.2), so
    // the declaration below is finally retail's own signature.
    //
    // IT IS A FORWARDER, like every other member of the mutating half — see the
    // banner above and external_class_doors.cpp. Its body tears down and
    // rebuilds `_references`, the same tree the `ExternalFactory::Add`
    // loader-thread AVL-rotate race runs in, and B34_BRIEFS section D binds it:
    // the instruction sequence in external_reftable.cpp is untouched, with one
    // extra (inlinable) call frame in front of it.
    void            SerializeReferenceTable(IO::Stream& stream);

    // ?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z
    //   retail 0x1800bec10 — a STATIC returning by value.
    static System::Filename GetCompiledPath(const System::Filename& path);

    // ---- the 0x43 bytes, in retail order (ctor 0x1800be7e0 pins every one) --
    int32_t      _cleanBitIndex;         // +0x00
    uint32_t     _cleanBitMask;          // +0x04 — the 32 UpdateLoop slots
    int32_t      _generation;            // +0x08
    int32_t      _pad0c;                 // +0x0c
    ResourceSet  _resources;             // +0x10
    ReferenceSet _references;            // +0x20
    Collection::Array<PathStackEntry> _pathStack;   // +0x30 data, +0x38 size
    bool         _referenceModeEnabled;  // +0x40
    bool         _loadEnabled;           // +0x41
    bool         _simpleLoadEnabled;     // +0x42
};
static_assert(sizeof(ExternalFactory) == sizeof(RtObject::ExternalFactory),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(ExternalFactory, _cleanBitIndex) == offsetof(RtObject::ExternalFactory, _cleanBitIndex), "alias _cleanBitIndex");
static_assert(offsetof(ExternalFactory, _cleanBitMask)  == offsetof(RtObject::ExternalFactory, _cleanBitMask),  "alias _cleanBitMask");
static_assert(offsetof(ExternalFactory, _generation)    == offsetof(RtObject::ExternalFactory, _generation),    "alias _generation");
static_assert(offsetof(ExternalFactory, _resources)     == offsetof(RtObject::ExternalFactory, _resources),     "alias _resources");
static_assert(offsetof(ExternalFactory, _references)    == offsetof(RtObject::ExternalFactory, _references),    "alias _references");
static_assert(offsetof(ExternalFactory, _pathStack)     == offsetof(RtObject::ExternalFactory, _pathStack),     "alias _pathStack");
static_assert(offsetof(ExternalFactory, _referenceModeEnabled) == offsetof(RtObject::ExternalFactory, _referenceModeEnabled), "alias _referenceMode");
static_assert(offsetof(ExternalFactory, _loadEnabled)          == offsetof(RtObject::ExternalFactory, _loadEnabled),          "alias _loadEnabled");
static_assert(offsetof(ExternalFactory, _simpleLoadEnabled)    == offsetof(RtObject::ExternalFactory, _simpleLoadEnabled),    "alias _simpleLoad");
static_assert(offsetof(ExternalFactory, _resources)  == 0x10, "_resources @ +0x10");
static_assert(offsetof(ExternalFactory, _references) == 0x20, "_references @ +0x20");
static_assert(offsetof(ExternalFactory, _pathStack)  == 0x30, "_pathStack @ +0x30");
static_assert(offsetof(ExternalFactory, _pathStack)
              + offsetof(Collection::Array<PathStackEntry>, _size) == 0x38,
              "path-stack SIZE @ +0x38 (GetPathStack*'s `CMP dword [RCX+0x38],0`)");
static_assert(offsetof(ExternalFactory, _referenceModeEnabled) == 0x40, "_referenceMode @ +0x40");
static_assert(offsetof(ExternalFactory, _loadEnabled)          == 0x41, "_loadEnabled @ +0x41");
static_assert(offsetof(ExternalFactory, _simpleLoadEnabled)    == 0x42, "_simpleLoad @ +0x42");

// ############################################################################
// WHY THERE IS NO `DECLARE_TYPE(External)` / `DEFINE_TYPE(...)` HERE.
//
// `External`, `ExternalList` and `CompiledExternal` DO each have a retail
// reflection block — `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` @
// 0x1801f9b38, `…@ExternalList@…` @ 0x1801f9be0, `…@CompiledExternal@…` @
// 0x1801f9b00 — so B33_RULES §9 step 5 would have the macro emit them. It
// CANNOT, in this step, and the reason is a file-set boundary rather than a
// design one:
//
//   * unlike the 305 `REGISTER_TYPE` sites in code/Game/, these three records
//     are not built by our code at all. They come out of the MINT
//     (standalone/rt_data_mint*.inc), `standalone/cutover_manifest.inc:65`
//     carries the CompiledExternal one as `CUTOVER_REQUIRED`, and
//     `MintStorage()` splices the minted nodes into
//     `BaseTypeInfo::_firstTypeInfo`;
//   * roughly twenty live sites read them BY NAME through `RT_DATA_SLOT`
//     (runtime/src/engine/mainloop_video.cpp:343,
//     runtime/src/engine/updateloop_loadresources.cpp:126,
//     runtime/src/io/modification_setupfilesystem.cpp:403,
//     runtime/src/object/{external_list,external_load,external_ptr_serialize,
//     object_typeinfo_accessors}.cpp, code/Game/Game/gameupdateloop_load.cpp);
//   * a `DEFINE_TYPE` would therefore construct a SECOND `BaseTypeInfo` for the
//     same name and head-insert it into the same chain. `Factory::Startup`
//     CRCs the name, so both records claim the same typeId and the second
//     insert hits the equal-key arm — the "split brain" hazard
//     standalone/rt_data_mint.cpp's own banner names, and exactly the shape
//     runtime/src/object/external.cpp:659-670 refuses for these two symbols.
//
// Retiring the mint rows in favour of the macro means editing
// standalone/rt_data_mint.cpp and re-running tools/rt_mint_gen.py, both outside
// this agent's file set, and it is the same move step 3 made for
// `_firstTypeInfo` (redirect, don't duplicate). Until then the three classes
// read the ONE existing record through the existing door, which is what the C
// bodies already do, and `Type()` / `TypeName()` / `TypeId()` answer from it
// byte for byte. Handing this to step 6/7 rather than guessing at it is the
// "faithful partial beats a complete-looking conversion" rule.
// ############################################################################
const BaseTypeInfo* ExternalTypeInfoRecord();          // ?_typeInfo@External@Object@@
const BaseTypeInfo* ExternalListTypeInfoRecord();      // ?_typeInfo@ExternalList@Object@@
const BaseTypeInfo* CompiledExternalTypeInfoRecord();  // ?_typeInfo@CompiledExternal@Object@@

}  // namespace Object
