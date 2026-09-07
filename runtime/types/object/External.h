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
struct ExternalArrayElement {
    char*    owned;   // +0x00 — released with the String idiom by FUN_180006ac0
    uint64_t opaque;  // +0x08 — never touched by the destructor
};
static_assert(sizeof(ExternalArrayElement) == 0x10,
              "the destructor's element stride is 0x10 (FUN_180006ac0)");

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
using ResourceSet  = Collection::Set<ResourceEntry,  Collection::DefaultAllocator,
                                     Filename, Collection::AVLCompare>;
using ReferenceSet = Collection::Set<ReferenceEntry, Collection::DefaultAllocator,
                                     uint32_t, Collection::AVLCompare>;
static_assert(sizeof(ResourceSet::Node)  == 0x38, "ResourceEntry node is 0x38");
static_assert(offsetof(ResourceSet::Node, parent) == 0x18, "resource node parent @ +0x18");
static_assert(offsetof(ResourceSet::Node, left)   == 0x20, "resource node left @ +0x20");
static_assert(offsetof(ResourceSet::Node, right)  == 0x28, "resource node right @ +0x28");
static_assert(sizeof(ReferenceSet::Node) == 0x30, "ReferenceEntry node is 0x30");
static_assert(offsetof(ReferenceSet::Node, parent) == 0x10, "reference node parent @ +0x10");
static_assert(offsetof(ReferenceSet::Node, left)   == 0x18, "reference node left @ +0x18");
static_assert(offsetof(ReferenceSet::Node, right)  == 0x20, "reference node right @ +0x20");

struct ExternalFactory {
    int32_t      _cleanBitIndex;         // +0x00
    uint32_t     _cleanBitMask;          // +0x04 — see the note above
    int32_t      _generation;            // +0x08
    int32_t      _pad0c;                 // +0x0c
    ResourceSet  _resources;             // +0x10
    ReferenceSet _references;            // +0x20
    uint64_t     _opaque30;              // +0x30 — NOT PINNED
    uint64_t     _opaque38;              // +0x38 — NOT PINNED
    bool         _referenceModeEnabled;  // +0x40
    bool         _loadEnabled;           // +0x41
    bool         _simpleLoadEnabled;     // +0x42
};
static_assert(offsetof(ExternalFactory, _resources)            == 0x10, "_resources @ +0x10");
static_assert(offsetof(ExternalFactory, _references)           == 0x20, "_references @ +0x20");
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
// ============================================================
#include "TypeInfo.h"

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
