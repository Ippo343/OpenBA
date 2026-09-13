#pragma once
// ============================================================================
// runtime/types/collection/Collection.h — binary layouts for Collection::*
// ============================================================================
//
// Wave 2, Lane E of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2,
// and §4.3 risk 1, which ranks these layouts the single biggest risk in the
// whole port: they are header-only templates, inlined into every consumer, and
// they define ZERO exported symbols. Getting one wrong silently corrupts every
// class that embeds it.)
//
// EVIDENCE: every layout below was read out of a decompiled Runtime-x64.dll
// function, and the specific function and field access is named next to it.
// The full write-up, including confidence levels and what is NOT pinned, is
// docs/runtime_port/WAVE2_COLLECTION.md. Nothing here is a guess; anything that
// could not be pinned is spelled out as such in a comment.
//
// THE REAL CONTAINER SET
// ----------------------
// The profile build's source-tree strings list exactly two headers in this
// directory (docs/runtime_port/original_source_tree.txt):
//
//     c:\projects\buildy\code\runtime\Collection/Array.h
//     c:\projects\buildy\code\runtime\Collection/Allocator.h
//
// and the mangled names in the DLL name exactly five Collection entities:
//
//     Collection::Array<T>                            (44 exports mention it)
//     Collection::Set<T, Allocator, Key, Compare>     (2)
//     Collection::LinkedList<T, Allocator>            (1, plus ::Iterator)
//     Collection::DefaultAllocator                    (a struct, U-prefixed)
//     Collection::AVLCompare                          (a struct, U-prefixed)
//
// *** THERE IS NO Collection::Map. ***  PLAN.md §4.3 risk 1 and the Wave 2
// brief both say "Map is the AVL tree behind Map_Insert/Map_Find". That is our
// own invented Ghidra name, not the engine's. `Map_Insert` (0x18002a090) is
// Set<Pair<String,String>>::Insert: it builds a 16-byte {key,value} element on
// the stack and hands it to the ordinary Set insert. A map in this engine is a
// Set whose ELEMENT embeds the key, which is exactly why Set takes a separate
// Key and Compare template parameter. Do not port a `Map`.
//
// Utility::HashGrid<T> and System::MemoryAllocator::PointerHash<T> are also
// templates, but they live in `Utility` and `System`, not `Collection`. They
// are out of scope for this header (HashGrid is Wave 5 territory).
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <new>           // placement new, for Collection::Array's element ops
#include <type_traits>   // the traits rtcoll::SpecFor<T> reads

namespace Collection {

// ============================================================
// Collection::DefaultAllocator — STATELESS policy struct.
//
// Evidence that it carries no storage: every allocation and free reaches the
// global System::MemoryAllocator::_singleton by absolute address, and no
// allocating body ever reads a field of the container it is allocating for —
// FUN_18000f970 (Set node alloc), FUN_180034010 and 0x1800a76c0 (Array grow),
// and the free in CleanRemovedDialogs. A stateful allocator policy would have
// to be reached through the container. Size agrees: LinkedList is 0x18
// (head/tail/count) and Set is 0x10 (root/count), each with exactly 4 bytes of
// tail slack — room for a flag, not for a pointer.
//
// What it forwards to, from the node allocators inlined into FUN_18000f970
// (Set node alloc) and FUN_180034010 (Array grow):
//
//     EnterCriticalSection(MemoryAllocator::_singleton + 0x640);
//     p = System::MemoryAllocator::Allocate(_singleton, bytes, alignment);
//     LeaveCriticalSection(...);
//
// i.e. Allocate(n * sizeof(T), alignof(T)) under the allocator's own lock, and
// the symmetric MemoryAllocator::Free. The alignment argument is alignof(T),
// NOT sizeof(T): Array<int> passes 4 (0x1800a76c0), Array<ModInfo> — a 0x20
// struct — passes 8 (0x180034010), Set nodes pass 8 (0x18000f970).
// ============================================================
struct DefaultAllocator {};
static_assert(sizeof(DefaultAllocator) == 1, "policy struct, never stored");

// ============================================================
// Collection::AVLCompare — STATELESS comparison policy struct.
//
// Named by the mangled type of IO::PackageFile::BuildPackageFile's parameter:
//   ?$Set@VFilename@System@@UDefaultAllocator@Collection@@V12@UAVLCompare@4@
//                                                            ^^^^^^^^^^^^^^
// (`@4@` back-references `Collection`.) Same carries-no-storage argument as
// DefaultAllocator: the comparison is fully inlined at every call site and
// never reads a field of the Set.
//
// Semantics, from the inlined comparison in Set::Insert (FUN_18002a170) and
// Set::Find (0x18002ace0) for a String key: an ordinary lexicographic byte
// compare of the two key strings, with a NULL key ordering as "less than
// everything". Insert descends LEFT when newKey < nodeKey and RIGHT when
// nodeKey < newKey, and returns the EXISTING node when neither holds — Set
// never stores duplicate keys.
// ============================================================
struct AVLCompare {};
static_assert(sizeof(AVLCompare) == 1, "policy struct, never stored");

// ============================================================
// Collection::Array<T> — {data, size, capacity}, 16 bytes. CONFIRMED.
//
// The exe port has assumed this triple for months (engine/types/EntityList.h
// `IBPArray`, engine/types/MapGenerator.h `DynArray`); this is the independent
// confirmation from Runtime-x64.dll that the Wave 2 brief asked for.
//
// sizeof == 16 is pinned by Video::ShaderProgram::GetByteCode (0x18004e100),
// a one-line `return _byteCode[i]` over an Array<Array<unsigned char>> that
// compiles to `*(this+0x40) + i*0x10`. Field offsets are pinned by four
// independent inlined bodies, all agreeing:
//   DynArrayPush              0x18007a0d0  (elements 8 bytes)
//   Input::KeyMap::GetIdList  0x1800a76c0  (Array<int>, elements 4 bytes)
//   Array<ModInfo>::SetSize   0x180034010  (elements 0x20 bytes, non-trivial T)
//   Array<T>::SetSize(0)      0x180079c90  (the constant-folded Clear)
// each reading `+0x00` as the buffer pointer, `+0x08` as an int32 element
// count, `+0x0c` as an int32 capacity.
//
// The field is named `_size`, not `_count`: the profile build's Array.h assert
// text is "index >= 0 && index < _size" (string at file offset 0x199e48 in
// ToolKit/bin/x64/Runtime-x64-profile.dll, immediately after the
// "Index out of range" message and near the Collection/Array.h path string).
//
// GROWTH POLICY (identical in all four bodies above), for SetSize(n):
//     if (n == _size) return;                       // exact-equal is a no-op
//     old = _data; oldSize = _size; _size = n;
//     if (_capacity < n) {
//         _capacity = n;
//         if (n == oldSize + 1 && oldSize != 0)      // appending to a NON-EMPTY
//             _capacity = n * 3;                     // array trebles
//         _data = Allocate(_capacity * sizeof(T), alignof(T));
//     }
//     // copy-construct [0, oldSize) from `old` if we reallocated,
//     // default-construct [oldSize, n),
//     // destroy the elements that fell off, then Free(old) if we reallocated
//
// Consequences worth stating because they are easy to get wrong:
//  * capacity NEVER shrinks and is NEVER reduced by SetSize; Clear()
//    (= SetSize(0), 0x180079c90) keeps the buffer and only zeroes _size.
//  * the FIRST append to an empty array allocates capacity 1, not 3 — the `*3`
//    is gated on oldSize != 0.
//  * a bulk SetSize(n) allocates EXACTLY n; only the +1 append over-allocates,
//    and it sets capacity to 3*newSize (not 3*oldCapacity), so appending one at
//    a time from empty walks capacities 1, 6, 21, 66, 201, ...
//  * T is destroyed properly on shrink and on realloc (0x180034010 calls the
//    element-destroy helper FUN_1800341e0 for both the truncated tail and the
//    whole old buffer), and copied by copy-CONSTRUCTION on realloc (the
//    Array<ModInfo> path bumps the String refcounts at `str-4`).
// ============================================================
// ============================================================================
// ENDGAME B.3.2 step 1 — `Collection::Array<T>`, the real template
// ============================================================================
//
// Everything above is the layout evidence and it is unchanged: the three fields
// below are still `_data` / `_size` / `_capacity` at +0x00 / +0x08 / +0x0c and
// every LayoutProbe static_assert at the bottom of this header still holds.
// What changed on 2026-09-13 is that the class now carries retail's METHODS
// instead of being a bare header the C bodies poke at.
//
// THE TEMPLATE WRAPS THE PORTED BODIES; IT DOES NOT RE-DERIVE THEM.
// `SetSize` calls `rtcoll::SetSizeCore` and `~Array` calls
// `rtcoll::ArrayDtorImpl` — the same two bodies, driven by the same
// `rtcoll::ArraySpec` rows, that the 27 registered addresses have driven since
// Wave 9 lane Q. COLLECTION_INVENTORY.md §5.3 says why in one line: a body
// re-derived from Ghidra's C risks re-introducing an argument-count mismatch
// this port already solved (feedback_ghidra_dropped_args.md).
//
// WHICH SPEC DRIVES AN INSTANTIATION — the third template parameter.
//
//   * `Array<T>` (the default, `DefaultSpec<T>`) derives its spec from T's type
//     traits: `byteCopy = is_trivially_copyable<T>`, `destroy == nullptr =
//     is_trivially_destructible<T>`, `init == nullptr =
//     is_trivially_default_constructible<T>` (retail's empty `else` arm, i.e.
//     the new tail is left UNINITIALISED), `assign` = placement copy-construct.
//     That mapping is not new design: it NAMES what the hand-derived rows
//     already encode.
//
//   * `Array<uint8_t, DefaultAllocator, ExplicitSpec<&kSpec_X>>` keeps a
//     HAND-DERIVED row. Every one of the registered addresses uses this form,
//     because a trait cannot express what those rows say: a fill of -1
//     (kSpec_I32Neg1, 0x18007b960), an identity-Transform init
//     (kSpec_Transform, 0x180059140), a PARTIAL init that deliberately leaves
//     bytes of the new element untouched (kSpec_SpriteDraw 0x180078520,
//     kSpec_SpriteIndexZ 0x1800370c0, kSpec_TableCell10 0x1800799a0,
//     kSpec_Window 0x180024c10, kSpec_CaptureEntryZ 0x18004ba90,
//     kSpec_EffectSlot 0x180051390, kSpec_AudioChannel70 0x180051020,
//     kSpec_Rec14 0x180020e90, kSpec_IdExtPtr10 0x180059930,
//     kSpec_SfxSample18 0x180052e60, kSpec_MetaEntry20 0x180034010), or a
//     byteCopy=true row that ALSO carries a destroy — a combination C++ traits
//     cannot produce at all, since `is_trivially_copyable` implies a trivial
//     destructor (kSpec_PkgEntry 0x1800298f0, kSpec_StringExt 0x180061480).
//     Step 1 therefore replaces NO spec: the shims name their row and the
//     trait-derived form is used only by new typed instantiations (the unit
//     tests today, converted callers in step 2).
//
// WHAT IS DELIBERATELY *NOT* ON THE CLASS
//   * No `Serialize`. None of the 339 registered rows is one; callers serialize
//     array contents by iterating (COLLECTION_INVENTORY.md §4, hazard 6).
//   * No `Sort`. The packfile introsort (0x1800264a0) is one hand-transcribed
//     algorithm whose tie behaviour is save-observable; it stays a free
//     function (hazard 7).
//   * No move constructor / move assignment. `SetSizeCore`'s realloc path is
//     copy-THEN-destroy-then-free in retail's element order and a move would
//     change the String/Filename/ExternalPtr refcount sequences the resave gate
//     observes (hazard 2).
//   * Copy construction and copy assignment are `= default`, i.e. the SHALLOW
//     memberwise copy the unconverted tree already relies on when it copies an
//     overlay struct that embeds an Array. Retail's deep `Array<T>::operator=`
//     is 0x18001cdf0 / `rtcoll::ArrayAssignCore` (array_destroy.cpp) and stays
//     reachable under its own address; it becomes this class's `operator=` in
//     step 2, when the last bitwise copier is gone. They are DEFAULTED rather
//     than left implicit so that declaring `~Array` does not make every copy a
//     -Wdeprecated-copy-dtor warning.
//   * The three fields stay PUBLIC. 419 sites across the tree name
//     `Collection::Array<...>` and read `_data`/`_size`/`_capacity` directly;
//     they become private with the last of them (B31_RULES step 2).
// ============================================================================

}  // namespace Collection  (reopened below; `rtcoll` is a global namespace)

namespace rtcoll {

// The element operations of one instantiation. Retail's `ElemDestroy` really
// does take an ignored `this` in RCX (array.cpp's ArraySpec banner).
using ElemAssign  = void (*)(uint8_t* dst, const uint8_t* src);
using ElemInit    = void (*)(uint8_t* dst);
using ElemDestroy = void (*)(void* ignoredThis, uint8_t* base, int count);

// MOVED HERE FROM runtime/src/collection/array_internal.h (ENDGAME B.3.2 step
// 1) so `Collection::Array`'s inline bodies can name it. The per-field
// commentary — why the written extents are per ARM and not per TYPE, and what
// the probe seeding range is for — is still in array_internal.h, next to the
// `kSpec_*` rows it describes.
struct ArraySpec {
    const char* name;
    uint32_t    stride;
    int         align;
    ElemAssign  assign;    // never null
    ElemInit    init;      // null => retail leaves the new tail untouched
    ElemDestroy destroy;   // null => trivially destructible T
    bool        byteCopy;  // true when `assign` is a pure byte copy

    uint32_t    assignUnwrittenLo, assignUnwrittenHi;
    uint32_t    initUnwrittenLo,   initUnwrittenHi;

    uint32_t    seedZeroLo, seedZeroHi;
};

// ---------------------------------------------------------------------------
// SpecFor<T> — the trait-derived row
// ---------------------------------------------------------------------------
// One `static const ArraySpec` per element type, built at first use. The three
// element operations are function templates rather than lambdas so that each
// has a readable name in a crash log.
template <class T>
void TAssign(uint8_t* dst, const uint8_t* src) {
    // Retail's realloc arm COPY-CONSTRUCTS into the new buffer and destroys the
    // old elements afterwards; placement copy-construct is that, exactly.
    new (static_cast<void*>(dst)) T(*reinterpret_cast<const T*>(src));
}
template <class T>
void TInit(uint8_t* dst) {
    new (static_cast<void*>(dst)) T();
}
template <class T>
void TDestroy(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<T*>(base + static_cast<int64_t>(i) * sizeof(T))->~T();
}

template <class T>
const ArraySpec* SpecFor() {
    static const ArraySpec spec = {
        "Array<T> (trait-derived)",
        static_cast<uint32_t>(sizeof(T)),
        static_cast<int>(alignof(T)),
        &TAssign<T>,
        // A trivially default-constructible T reproduces retail's EMPTY else
        // arm: the new tail is left holding whatever the allocation contained.
        // Writing `TInit<T>` for a trivial T instead would ZERO the tail — a
        // different array.
        std::is_trivially_default_constructible<T>::value ? nullptr : &TInit<T>,
        std::is_trivially_destructible<T>::value ? nullptr : &TDestroy<T>,
        std::is_trivially_copyable<T>::value,
        /*assign unwritten*/ 0, 0,
        /*init   unwritten*/ 0, 0,
        /*seed zero       */ 0, 0 };
    return &spec;
}

}  // namespace rtcoll

namespace Collection {

// The two spec policies. `DefaultSpec` is the third template argument's
// default; `ExplicitSpec` names a hand-derived row (see the banner).
template <class T>
struct DefaultSpec {
    static const rtcoll::ArraySpec* Get() { return rtcoll::SpecFor<T>(); }
};
template <const rtcoll::ArraySpec* SPEC>
struct ExplicitSpec {
    static const rtcoll::ArraySpec* Get() { return SPEC; }
};

template <class T,
          class Allocator  = DefaultAllocator,
          class SpecPolicy = DefaultSpec<T>>
class Array {
public:
    // Retail's ctor is the three zero stores every inlined copy begins with.
    Array() : _data(nullptr), _size(0), _capacity(0) {}
    // Shallow, and deliberately so — see the banner.
    Array(const Array&)            = default;
    Array& operator=(const Array&) = default;

    // ~Array — 0x18001c750 and its ten twins for a trivial element; the J2
    // family (array_destroy.cpp) for one with a destroy step. Element destroy
    // first, then the buffer: that is the order both retail families use.
    ~Array();

    void SetSize(int newSize);                      // ?SetSize@..@QEAAXH@Z     (2-arg form)
    void SetSize(int newSize, const T& fill);       // ?SetSize@..@QEAAXHAEBT@Z (3-arg form)
    // The 3-argument form with an untyped fill, for the instantiations whose
    // element "type" is a hand-derived spec rather than a C++ type.
    void SetSizeFill(int newSize, const void* fill);

    // 0x180079c90 — the constant-folded SetSize(0). Buffer and capacity kept.
    void Clear() { SetSize(0); }

    int GetSize() const     { return _size; }
    int GetCapacity() const { return _capacity; }

    T&       operator[](int index)       { return _data[index]; }
    const T& operator[](int index) const { return _data[index]; }

    T*       GetData()       { return _data; }
    const T* GetData() const { return _data; }

    static const rtcoll::ArraySpec* Spec() { return SpecPolicy::Get(); }

    // PUBLIC during the migration — see the banner.
    T*      _data;       // +0x00: element buffer, or null when capacity == 0
    int32_t _size;       // +0x08: live element count
    int32_t _capacity;   // +0x0c: allocated element count; never shrinks
};

}  // namespace Collection

// The two ported bodies and the allocator boundary, declared here rather than
// in array_internal.h so that the class above can call them. Both are defined
// in runtime/src/collection/array.cpp, unchanged.
namespace rtcoll {

// The 16-byte container header. `uint8_t` is chosen so `_data` is already the
// byte cursor every body wants; sizeof(Array<T>) is T-independent.
using ArrayHdr = Collection::Array<uint8_t>;

void* MemAlloc(uint64_t bytes, int align);
void  MemFree(void* p);

void* ArrayDtorImpl(ArrayHdr* a);
void  SetSizeCore(ArrayHdr* a, int newSize, const uint8_t* fill,
                  const ArraySpec* spec);

}  // namespace rtcoll

namespace Collection {

template <class T, class Allocator, class SpecPolicy>
inline Array<T, Allocator, SpecPolicy>::~Array() {
    const rtcoll::ArraySpec* spec = Spec();
    if (spec->destroy && _size > 0)
        spec->destroy(nullptr, reinterpret_cast<uint8_t*>(_data), _size);
    rtcoll::ArrayDtorImpl(reinterpret_cast<rtcoll::ArrayHdr*>(this));
}

template <class T, class Allocator, class SpecPolicy>
inline void Array<T, Allocator, SpecPolicy>::SetSize(int newSize) {
    rtcoll::SetSizeCore(reinterpret_cast<rtcoll::ArrayHdr*>(this), newSize,
                        nullptr, Spec());
}

template <class T, class Allocator, class SpecPolicy>
inline void Array<T, Allocator, SpecPolicy>::SetSize(int newSize, const T& fill) {
    rtcoll::SetSizeCore(reinterpret_cast<rtcoll::ArrayHdr*>(this), newSize,
                        reinterpret_cast<const uint8_t*>(&fill), Spec());
}

template <class T, class Allocator, class SpecPolicy>
inline void Array<T, Allocator, SpecPolicy>::SetSizeFill(int newSize, const void* fill) {
    rtcoll::SetSizeCore(reinterpret_cast<rtcoll::ArrayHdr*>(this), newSize,
                        static_cast<const uint8_t*>(fill), Spec());
}

// ============================================================
// Collection::LinkedList<T, Allocator> — doubly linked, head + tail + count.
//
// Pinned by UI::UserInterface::CleanRemovedDialogs (0x18011f790), which unlinks
// a node from the dialog list embedded at UserInterface+0x00. Every branch of
// that unlink names one field:
//   node == *(this+0x00)  -> *(this+0x00) = node[+0x10]   head, and next=+0x10
//   node == *(this+0x08)  -> *(this+0x08) = node[+0x08]   tail, and prev=+0x08
//   node[+0x08] != 0      -> *(node[+0x08] + 0x10) = node[+0x10]  prev->next
//   node[+0x10] != 0      -> *(node[+0x10] + 0x08) = node[+0x08]  next->prev
//   *(int*)(this+0x10) -= 1                                       count
// and the search loop compares `*node` against the dialog pointer (element at
// +0x00) while advancing through node[+0x10].
//
// The container is 0x18 (24) bytes: head +0x00, tail +0x08, count +0x10, and
// UserInterface's next member — an Array — starts at +0x18. The 4 bytes at
// +0x14 are never touched (see _pad14 below).
//
// Iterator is EIGHT BYTES and is just the node pointer: the whole body of
// UI::UserInterface::GetFirstDialog (0x180031150), which returns
// LinkedList<...>::Iterator by value, is `*(void**)retval = *(void**)this`.
//
// Node allocation/free goes through DefaultAllocator exactly like Set's does —
// CleanRemovedDialogs frees the unlinked node with MemoryAllocator::Free under
// the +0x640 critical section.
// ============================================================
template <class T, class Allocator = DefaultAllocator>
struct LinkedList {
    struct Node {
        T     element;   // +0x00
        Node* prev;      // align8(sizeof(T)) + 0x00
        Node* next;      // align8(sizeof(T)) + 0x08
    };
    struct Iterator {
        Node* node;      // +0x00 — the entire iterator
    };

    Node*   _head;       // +0x00
    Node*   _tail;       // +0x08
    int32_t _count;      // +0x10
    int32_t _pad14;      // +0x14: NOT PINNED — never read or written by any
                         //        decompiled body; present so sizeof == 0x18.
                         //        (Read "0x10" until Wave 7 lane C: a typo, and
                         //        one the same comment block contradicts 26
                         //        lines up, which states 0x18 correctly. The
                         //        arithmetic settles it — head 8 + tail 8 +
                         //        count 4 + pad 4 = 0x18 — and it is now pinned
                         //        by a static_assert in
                         //        runtime/types/ui/UserInterface.h.)
};

// ============================================================
// Collection::Set<T, Allocator, Key, Compare> — an AVL tree. THE "Map".
//
// Four template parameters, from the two mangled names that spell it out:
//   ?$Set@VFilename@System@@UDefaultAllocator@Collection@@V12@UAVLCompare@4@
//     = Set<System::Filename, Collection::DefaultAllocator,
//           System::Filename, Collection::AVLCompare>
//   ?$Set@UCachedParseData@TextReader@IO@@UDefaultAllocator@Collection@@
//        U123@UAVLCompare@5@
//     = Set<IO::TextReader::CachedParseData, DefaultAllocator,
//           IO::TextReader::CachedParseData, AVLCompare>
// In both, Key == T: the key is the element (or its leading sub-object).
//
// CONTAINER, 0x10 bytes:
//   +0x00 root node pointer   — read as `*param_1` by Insert (FUN_18002a170)
//                               and Find (0x18002ace0)
//   +0x08 int32 node count    — Insert does `*(int*)(param_1+8) += 1` on every
//                               successful insert
//   +0x0c NOT PINNED (padding)
// Independently corroborated on the exe side by engine/types/EntityList.h,
// which has TWO adjacent Sets: root +0x68 / count +0x70, root +0x78 / count
// +0x80 — a 0x10 stride, live in-game for months.
//
// NODE = { T element; Node* parent; Node* left; Node* right;
//          int16 balance; int16 height; uint32 pad; }
//      = align8(sizeof(T)) + 0x20 bytes.
//
// Element FIRST, so the pointer tail floats with sizeof(T). Three independent
// instantiations, three different element sizes, all agreeing:
//   T = 8    (exe EntityList tree)              node 0x28; parent +0x08,
//                                               left +0x10, right +0x18,
//                                               balance +0x20, height +0x22
//   T = 0x10 (Pair<String,String>, Map_Insert)  node 0x30; parent +0x10,
//                                               left +0x18, right +0x20,
//                                               balance +0x28, height +0x2a
//   T = 0x18 (Input::KeyMap key set, 1800a76c0) node 0x38; parent +0x18,
//                                               left +0x20, right +0x28
//
// The 0x10 case is exact: FUN_18000f970 allocates 0x30 bytes with alignment 8
// and hands them to the node constructor FUN_18000fb00, which writes
//   [0]=key [1]=value [2]=parent [3]=left(0) [4]=right(0)
// and zeroes the DWORD at +0x28, i.e. balance and height together.
//
// BALANCE AND HEIGHT, from the retrace/rebalance walk FUN_18002a310:
//   height(node)  = max(left  ? left->height  + 1 : 0,
//                       right ? right->height + 1 : 0)
//   balance(node) = rightHeight - leftHeight        // NOTE THE SIGN
//   balance > +1 -> rotate left  (right-heavy), preceded by a right rotation
//                   of the right child when that child's balance < 0
//   balance < -1 -> rotate right (left-heavy),  preceded by a left rotation
//                   of the left child when that child's balance > 0
// A leaf has height 0. The walk climbs via `parent` to the root. The two
// rotation helpers for the 0x10-element instantiation are FUN_18002a590 (left)
// and FUN_18002a6c0 (right).
//
// LOOKUP (0x18002ace0) descends left while searchKey < nodeKey and right while
// nodeKey < searchKey, returning the node when neither holds and null on
// falling off the tree. IN-ORDER TRAVERSAL (inlined in 0x1800a76c0) is the
// textbook parent-pointer successor walk, no threading.
//
// Because the pointer tail's offsets are a function of sizeof(T), MSVC emits a
// SEPARATE copy of insert/find/rebalance per instantiation with the offsets
// baked in. Do not assume one ported Set body serves two element types.
// ============================================================
template <class T,
          class Allocator = DefaultAllocator,
          class Key       = T,
          class Compare   = AVLCompare>
struct Set {
    struct Node {
        T        element;   // +0x00
        Node*    parent;    // align8(sizeof(T)) + 0x00
        Node*    left;      // align8(sizeof(T)) + 0x08
        Node*    right;     // align8(sizeof(T)) + 0x10
        int16_t  balance;   // align8(sizeof(T)) + 0x18  right height - left height
        int16_t  height;    // align8(sizeof(T)) + 0x1a  leaf == 0
        uint32_t _padTail;  // align8(sizeof(T)) + 0x1c  zeroed together with
                            //   balance and height as one dword by the node ctor
    };
    struct Iterator {
        Node* node;         // +0x00 — NOT PINNED. No Set::Iterator is exported
                            //   or returned by value anywhere in the DLL; this
                            //   is shaped after LinkedList::Iterator, which IS
                            //   pinned. Treat as a GUESS until a consumer pins it.
    };

    Node*   _root;          // +0x00
    int32_t _count;         // +0x08
    int32_t _pad0c;         // +0x0c: NOT PINNED — never read or written
};

// ============================================================================
// Layout checks. Templates cannot be static_assert'ed in the abstract, so the
// three element widths the DLL evidence actually covers are instantiated here
// and checked field by field.
// ============================================================================
namespace LayoutProbe {

struct E8  { uint64_t a; };            // 8  — exe EntityList tree element
struct E16 { void* k; void* v; };      // 16 — Map_Insert's {key,value} pair
struct E24 { uint64_t a, b, c; };      // 24 — Input::KeyMap's key-set element

using A8 = Array<E8>;
static_assert(sizeof(A8) == 0x10,              "Collection::Array must be 0x10 bytes");
static_assert(offsetof(A8, _data)     == 0x00, "Array::_data @ +0x00");
static_assert(offsetof(A8, _size)     == 0x08, "Array::_size @ +0x08");
static_assert(offsetof(A8, _capacity) == 0x0c, "Array::_capacity @ +0x0c");
static_assert(sizeof(Array<uint8_t>)  == 0x10, "sizeof(Array<T>) is T-independent");
static_assert(sizeof(Array<E24>)      == 0x10, "sizeof(Array<T>) is T-independent");

using L8 = LinkedList<E8>;
static_assert(sizeof(L8) == 0x18,               "Collection::LinkedList must be 0x18 bytes");
static_assert(offsetof(L8, _head)  == 0x00,     "LinkedList::_head @ +0x00");
static_assert(offsetof(L8, _tail)  == 0x08,     "LinkedList::_tail @ +0x08");
static_assert(offsetof(L8, _count) == 0x10,     "LinkedList::_count @ +0x10");
static_assert(sizeof(L8::Iterator) == 0x08,     "LinkedList::Iterator is one pointer");
static_assert(sizeof(L8::Node) == 0x18,         "LinkedList::Node(T=8) is 0x18");
static_assert(offsetof(L8::Node, prev) == 0x08, "LinkedList::Node::prev @ +0x08 for T=8");
static_assert(offsetof(L8::Node, next) == 0x10, "LinkedList::Node::next @ +0x10 for T=8");

using S8  = Set<E8>;
using S16 = Set<E16>;
using S24 = Set<E24>;
static_assert(sizeof(S8) == 0x10,           "Collection::Set must be 0x10 bytes");
static_assert(offsetof(S8, _root)  == 0x00, "Set::_root @ +0x00");
static_assert(offsetof(S8, _count) == 0x08, "Set::_count @ +0x08");

// T = 8: matches engine/types/EntityList.h EntityTreeNode exactly.
static_assert(sizeof(S8::Node) == 0x28,            "Set::Node(T=8) is 0x28");
static_assert(offsetof(S8::Node, parent)  == 0x08, "Set::Node::parent @ +0x08 for T=8");
static_assert(offsetof(S8::Node, left)    == 0x10, "Set::Node::left @ +0x10 for T=8");
static_assert(offsetof(S8::Node, right)   == 0x18, "Set::Node::right @ +0x18 for T=8");
static_assert(offsetof(S8::Node, balance) == 0x20, "Set::Node::balance @ +0x20 for T=8");
static_assert(offsetof(S8::Node, height)  == 0x22, "Set::Node::height @ +0x22 for T=8");

// T = 16: matches the 0x30-byte allocation in FUN_18000f970 and the offsets
// baked into Map_Find (0x18002ace0) and the rebalance FUN_18002a310.
static_assert(sizeof(S16::Node) == 0x30,            "Set::Node(T=0x10) is 0x30");
static_assert(offsetof(S16::Node, parent)  == 0x10, "Set::Node::parent @ +0x10 for T=0x10");
static_assert(offsetof(S16::Node, left)    == 0x18, "Set::Node::left @ +0x18 for T=0x10");
static_assert(offsetof(S16::Node, right)   == 0x20, "Set::Node::right @ +0x20 for T=0x10");
static_assert(offsetof(S16::Node, balance) == 0x28, "Set::Node::balance @ +0x28 for T=0x10");
static_assert(offsetof(S16::Node, height)  == 0x2a, "Set::Node::height @ +0x2a for T=0x10");

// T = 24: matches the traversal offsets inlined into Input::KeyMap::GetIdList.
static_assert(sizeof(S24::Node) == 0x38,           "Set::Node(T=0x18) is 0x38");
static_assert(offsetof(S24::Node, parent) == 0x18, "Set::Node::parent @ +0x18 for T=0x18");
static_assert(offsetof(S24::Node, left)   == 0x20, "Set::Node::left @ +0x20 for T=0x18");
static_assert(offsetof(S24::Node, right)  == 0x28, "Set::Node::right @ +0x28 for T=0x18");

}  // namespace LayoutProbe
}  // namespace Collection
