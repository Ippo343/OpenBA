// ============================================================================
// runtime/src/ui/layoutdescription.cpp — UI::LayoutDescription, whole class
// ============================================================================
//
// Vtable batch 2, the last class in it and the one with the most machinery
// underneath. Derives straight from UI::ElementDescription.
//
// ??_7LayoutDescription@UI@@6B@ (retail 0x180156cc0), ten slots:
//   0  deleting dtor      0x1800431d0  deleting_dtor.cpp row (sizeof 0x78)
//   1  TypeName           0x180042d40  here
//   2  Type               0x180042d20  here
//   3  GetFileVersion     0x1800d3a00  here  (returns 0x304)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d3940  here
//   6  Reconstruct        0x180042fc0  here
//   7  Destruct           0x180043100  here
//   8  CreateInstance     0x1800d33a0  here
//   9  Construct          0x180043060  here
// plus ??1LayoutDescription@UI@@UEAA@XZ 0x1800d3a10 and the entry-array
// serializer 0x180043f50 (unexported; registered BY ADDRESS ONLY).
//
// THE CONSTRUCTOR IS INLINED, twice (Construct 0x180043076.. and Reconstruct
// 0x180042fde..), so there is no ??0 export to register. Both copies were
// compared instruction by instruction for this port and agree.
//
// LAYOUT, sizeof 0x78 (`MOV EDX,0x78` at 0x1800431ef). ElementDescription
// (0x68) plus ONE array:
//     +0x68  Array<Entry>.data, +0x70 _size, +0x74 _capacity
//
// Entry is 0x38 bytes and is the thing this class is about
// (runtime/types/ui/LayoutDescription.h names it field by field):
//     +0x00  ExternalPtr<UI::ElementDescription>   the cell's content
//     +0x08  System::String      this entry's OWN name — what siblings look up
//     +0x10  System::String   \  the names of the two entries this one is
//     +0x18  System::String   /  positioned RELATIVE TO (X, then Y)
//     +0x20  uint32  +0x24 uint32  +0x28 int32  +0x2c int32  +0x30 bool
//     +0x31..+0x37 never written by anything
// (B.3.6 block C4 correction: this banner used to put the OWN name at +0x18
// and the two relative names at +0x08/+0x10. CreateInstance's own lookup —
// `LEA R11,[R14+0x8]` at 0x1800d3645 for the candidates, `MOV RBX,[R14+R8+0x10]`
// at 0x1800d361d for the wanted name, gated by `CMP dword [R14+R8+0x20],0` —
// and the body below always said +0x08 / +0x10 / +0x18 as above.)
//
// FOUR COLLECTION HELPERS, all appended to the collection files rather than
// written here (that is where the family lives, and the specs there carry the
// unwritten-byte windows an oracle must not compare):
//   runtime/src/collection/array.cpp
//     kSpec_LayoutEntry38 + J3_ENTRY2 for 0x180043d80 (Array<Entry>::SetSize)
//     kSpec_LayoutInfo44  + J3_ENTRY3 for 0x180043b40 (the ELEMENT's 0x44-byte
//                                                      per-cell info array)
//   runtime/src/collection/array_destroy.cpp
//     J2_ENTRY for 0x180043a00 (~Array<Entry>) and the single-element
//     destructor 0x180044050, plus the element copy-assign 0x180043980
//
// ??0LayoutElement@UI@@ is NOT called here: CreateInstance inlines the element
// construction (??0Element, the LayoutElement vtable, two zeroed qwords).
//
// ENDGAME B.3.6 block C4: the layout moved to runtime/types/ui/
// LayoutDescription.h / LayoutElement.h (the real classes). Every slot body
// below is a MEMBER; each registered door is a one-line QUALIFIED delegation;
// the dtor stays a DOOR (B4b-2). RULE B6-5: every export the members reach is
// cached in a namespace-scope std::atomic slot, never in a function-local static.
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/LayoutDescription.h"
#include "../../types/ui/LayoutElement.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

std::atomic<void*> g_vtBase{nullptr};
std::atomic<void*> g_vtExternal{nullptr};
std::atomic<void*> g_vtElementDescription{nullptr};
std::atomic<void*> g_vtLayoutDescription{nullptr};
std::atomic<void*> g_vtLayoutElement{nullptr};
std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_memFree{nullptr};
std::atomic<void*> g_externalDtor{nullptr};

void* VtBase()               { return AtomicExport(g_vtBase, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return AtomicExport(g_vtExternal, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return AtomicExport(g_vtElementDescription, "??_7ElementDescription@UI@@6B@"); }
void* VtLayoutDescription()  { return AtomicExport(g_vtLayoutDescription, "??_7LayoutDescription@UI@@6B@"); }
void* VtLayoutElement()      { return AtomicExport(g_vtLayoutElement, "??_7LayoutElement@UI@@6B@"); }
const uint8_t* LayoutDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@LayoutDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
using MemFreeFn = void (*)(void*);
MemFreeFn MemFree() {
    return reinterpret_cast<MemFreeFn>(AtomicExport(
        g_memFree, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;
    int32_t _pad0c;
    void*   encodingFactory;
};
struct Serializer {
    Stream* stream;
    void*   readerWriter;
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

// The inlined constructor, from 0x180043076..0x1800430ea (Construct's copy).
void ConstructBody(UI::LayoutDescription* self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 180043076
    *v = VtExternal();                                          // 180043080
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 180043095
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 180043098
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18004309c
    *v = VtElementDescription();                                // 1800430a0
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800430aa
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800430ae
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800430b2
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800430b6  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800430b9  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800430c0  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800430c8
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800430cc
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800430d0
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800430d4
    *v = VtLayoutDescription();                                 // 1800430d8
    self->_entries._data     = nullptr;                         // 1800430e2
    self->_entries._size     = 0;                               // 1800430e6  one qword
    self->_entries._capacity = 0;
}

// Retail's String comparison, inlined at FOUR places in CreateInstance and
// written once here. It is NOT strcmp: the two cached LENGTHS at `data[-8]` are
// compared first, and only then the bytes, and the byte walk stops at the first
// NUL on EITHER side. Returns true when the two compare equal.
//
// A zero-length table entry short-circuits to "equal" the moment the lengths
// match, which is what makes an empty name match another empty name.
bool StringEquals(const char* have, const char* want) {
    const int32_t haveLen = have ? *reinterpret_cast<const int32_t*>(have - 8) : 0;
    const int32_t wantLen = want ? *reinterpret_cast<const int32_t*>(want - 8) : 0;
    if (haveLen != wantLen) return false;
    if (have == nullptr || *reinterpret_cast<const int32_t*>(have - 8) == 0) return true;
    if (want == nullptr) return false;

    int64_t k = 0;
    char c = have[0];
    if (c == want[0]) {
        const char* w = want;
        while (c != '\0' && *w != '\0') {
            ++k;
            ++w;
            c = have[w - want];
            if (c != *w) break;
        }
    }
    return have[k] == want[k];
}

bool NamesNothing(const char* s) {
    return s == nullptr || *reinterpret_cast<const int32_t*>(s - 8) == 0;
}

}  // namespace

// ============================================================================
// The entry-array serializer, retail 0x180043f50 — UNEXPORTED, address-only.
// Not a member: it is the `Array<Entry>` element serializer, not a
// LayoutDescription method.
// ============================================================================
// `void SerializeEntries(Serializer* ser, int count, Entry* data)`
// RCX = the {stream, readerWriter} PAIR, EDX = count, R8 = the elements.
//
// Per entry, in this order:
//   +0x00  ExternalPtr<ElementDescription>::Serialize(&e, pair)  0x180038f90
//   +0x08  System::String::Serialize(&e, pair)                   0x1800c9930
//   +0x10  System::String::Serialize
//   +0x18  System::String::Serialize
//   +0x20  kScalarUInt   +0x24 kScalarUInt   (vt+0x98, NOT the int slot)
//   +0x28  kScalarInt    +0x2c kScalarInt    (vt+0xa0)
//   +0x30  kScalarBool
// The READER is read out of the pair (`MOV RCX,[RDI+8]`) ONCE PER FIELD, not
// hoisted, and the body returns immediately when count <= 0 (`TEST EDX,EDX;
// JLE` is the first instruction, before the prologue).
extern "C" void RtUi_LayoutDescription_SerializeEntries(void* serv, int32_t count,
                                                        UI::LayoutDescription::Entry* e) {
    if (count <= 0) return;                                    // 180043f50
    Serializer* ser = static_cast<Serializer*>(serv);

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    using ScalarFn   = void (*)(void* rw, void* value, const char* name);

    for (int32_t i = 0; i < count; ++i, ++e) {
        RtAddr<FieldSerFn>(0x180038f90ull)(&e->desc, ser);         // 180043f77
        RtAddr<FieldSerFn>(0x1800c9930ull)(&e->name, ser);         // 180043f83
        RtAddr<FieldSerFn>(0x1800c9930ull)(&e->relativeToX, ser);  // 180043f8e
        RtAddr<FieldSerFn>(0x1800c9930ull)(&e->relativeToY, ser);  // 180043f9a

        void** rvt = VTableOf(ser->readerWriter);              // re-read each field
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(
            ser->readerWriter, &e->horizontal, nullptr);
        rvt = VTableOf(ser->readerWriter);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(
            ser->readerWriter, &e->vertical, nullptr);
        rvt = VTableOf(ser->readerWriter);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(
            ser->readerWriter, &e->offsetX, nullptr);
        rvt = VTableOf(ser->readerWriter);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(
            ser->readerWriter, &e->offsetY, nullptr);
        rvt = VTableOf(ser->readerWriter);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(
            ser->readerWriter, &e->force, nullptr);
    }
}

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t LayoutDescription::Type() const {
    const uint8_t* ti = LayoutDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* LayoutDescription::TypeName() const {
    const uint8_t* ti = LayoutDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t LayoutDescription::GetFileVersion() const { return 0x304; }

// 0x180043060 — the null test stays in the door; the ctor is INLINED.
Object::External* LayoutDescription::Construct() {
    ConstructBody(this);
    return this;                                               // 1800430ea
}
// 0x180042fc0 — slot 0 (VIRTUAL, the minted row) with flag 0, then the
// inlined ctor.
Object::External* LayoutDescription::Reconstruct() {
    DeletingDtor(0);                                           // 180042fd7
    ConstructBody(this);
    return this;                                               // 180043052
}
// 0x180043100
void LayoutDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@LayoutDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d3940
// ============================================================================
bool LayoutDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 1800d395a (qualified)

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800d398a

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);

    int32_t n = _entries._size;                                // 1800d398f
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                               // 1800d39b1 (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x180043d80ull)(&_entries, n);       // 1800d39bf
    }

    using EntriesSerFn = void (*)(Serializer* ser, int32_t count, void* data);
    RtAddr<EntriesSerFn>(0x180043f50ull)(&ser, n, _entries._data);   // 1800d39d1

    if (rw != nullptr) {                                       // 1800d39df
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@LayoutDescription@UI@@...   retail 0x1800d33a0
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = Dialog*.
//
// LayoutElement is 0xd0 bytes (`MOV EDX,0xd0` at 0x1800d33ee); its construction
// is INLINED (??0Element, the LayoutElement vtable, then +0xc0 and +0xc8
// zeroed).
//
// *** THIS BODY IS A DEPENDENCY RESOLVER, AND THAT IS WHY IT IS SHAPED LIKE
//     THIS. *** Every entry may name up to two OTHER entries it is positioned
// relative to (`relativeToX` +0x10 and `relativeToY` +0x18; the entry's own
// name is `name`, +0x08). An entry can only be instantiated once the entries it
// names already have a record in the element, so the body runs:
//
//   `order` = Array<uint32> of one slot per entry, all initialised to -1
//             (0x180023830, the three-argument fill form == kSpec_Fill4)
//
//   PASS 1 — every entry with NO dependency (both gates zero, or both
//            relative-name Strings empty) is placed immediately, in index order.
//
//   PASS 2 — repeat over all still-unplaced entries; an entry is placed when
//            BOTH of its named dependencies resolve to an already-placed entry
//            (or it names none). OUTER LOOP repeats while `placed < count`.
//
// *** THE OUTER LOOP HAS NO PROGRESS CHECK. *** If a cycle or a dangling name
// makes a pass place nothing, retail loops forever. That is retail's, it is not
// "fixed" here, and it is the reason a malformed layout .rsc hangs rather than
// drawing wrong.
//
// `order[i]` doubles as the placed flag (-1 == not placed) and as the VALUE
// stored into the resolved entry's record: it receives the element's current
// record count at the moment the entry is placed.
//
// THE LOOKUPS COMPARE STRINGS AGAINST EVERY *PLACED* ENTRY'S OWN NAME (`name`,
// +0x08) and skip the entry being resolved (`uVar19 != uVar17`). The sentinel
// handling is layered:
//   -1  the field named nothing (its gate at +0x20 / +0x24 is zero, or the name
//       String is empty)   -> not a blocker
//   -2  the field named something that is not placed yet
//       -> `-2 < x` is the test that lets -1 through and stops -2, which is why
//          both sentinels exist and why they are not interchangeable.
Object::InternalPtr<Element>* LayoutDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xd0, 8);   // 1800d33ee (inlined)

    LayoutElement* elem = nullptr;
    if (raw != nullptr) {                                      // 1800d341c
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);            // 1800d3421 ??0Element
        *reinterpret_cast<void**>(raw) = VtLayoutElement();    // 1800d3427
        elem = reinterpret_cast<LayoutElement*>(raw);          // B.3.6 bridge
        elem->_childRecords._data     = nullptr;               // 1800d3434
        elem->_childRecords._size     = 0;                     // 1800d343b (one qword)
        elem->_childRecords._capacity = 0;
    }

    void* slot = elem;                                         // 1800d344e
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(this, &slot);   // 1800d3459

    // The order table: one uint32 per entry, all -1.
    struct { int32_t* data; int32_t size; int32_t capacity; } order = { nullptr, 0, 0 };
    {
        const int32_t minusOne = -1;                           // 1800d346a
        using FillSetSizeFn = void (*)(void* array, int n, const void* fill);
        RtAddr<FillSetSizeFn>(0x180023830ull)(&order, _entries._size, &minusOne);  // 1800d347d
    }

    const Entry* const entriesBase = _entries._data;
    int32_t placed = 0;                                        // [RBP+0x7f]

    // ---- one entry's placement, shared by both passes ----------------------
    // (0x1800d34e9..0x1800d359f and 0x1800d37cb..0x1800d3883 are the same
    // sequence with a different record source; written once.)
    auto place = [&](int32_t index, const Entry* entry, int32_t relA, int32_t relB) {
        order.data[index] = elem->_childRecords._size;          // 1800d34e9
        ++placed;                                              // 1800d34f4

        void* childDesc = entriesBase[index].desc;             // 1800d3500
        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, &slot, childDesc);

        Element* child = static_cast<Element*>(*made);         // 1800d3514
        if (child != nullptr) child->_parent = elem;           // 1800d351c

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(&elem->_children,
                                       elem->_children.GetSize() + 1, made);   // 1800d3533

        for (Element* n = elem; n != nullptr; n = n->_parent)  // 1800d3540..1800d3554
            n->_flags |= kElementFlagNeedsSizeCalc;

        // The 0x44-byte record. RETAIL WRITES ONLY ITS FIRST 0x24 BYTES
        // (0x1800d3568..0x1800d3581) and the array assign copies all 0x44, so
        // bytes 0x24..0x43 of every appended record are retail's stack residue
        // (as are +0x19..+0x1b). Reproduced with an uninitialised local: what is
        // faithful is the WIDTH of the copy, not our residue.
        LayoutElement::ChildRecord info;                       // [RBP-0x49]
        info.alignX  = entry->horizontal;                      // 1800d3568
        info.alignY  = entry->vertical;                        // 1800d356b
        info.refX    = relA;                                   // 1800d356e / 1800d385a
        info.refY    = relB;                                   //            / 1800d385d
        info.offsetX = entry->offsetX;                         // 1800d3576
        info.offsetY = entry->offsetY;                         // 1800d3579
        info.force   = entry->force;                           // 1800d357d
        const uint64_t zero = 0;
        __builtin_memcpy(&info.sizeX, &zero, sizeof zero);     // 1800d3581  UNALIGNED QWORD

        using InfoPushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<InfoPushFn>(0x180043b40ull)(&elem->_childRecords,
                                           elem->_childRecords._size + 1,
                                           &info);             // 1800d359a
    };

    // ---- PASS 1: the entries that depend on nothing ------------------------
    {
        const int32_t count = _entries._size;
        for (int32_t i = 0; i < count; ++i) {                  // 1800d348b / 1800d35b0
            const Entry* e = _entries._data + i;               // 1800d34b0

            const bool noFlags = (e->horizontal == 0 && e->vertical == 0);    // 1800d34b7
            const bool noNames = NamesNothing(e->relativeToX) && NamesNothing(e->relativeToY);
            if (!(noFlags || noNames)) continue;               // 1800d35a2

            // -1, -1: this entry is positioned relative to nothing.
            place(i, e, -1, -1);                               // 1800d356e MOV qword -1
        }
    }

    // ---- PASS 2: repeat until everything is placed -------------------------
    // NO PROGRESS CHECK — see the banner.
    while (placed < _entries._size) {                          // 1800d35c5 / 1800d38be
        const int32_t count = _entries._size;
        for (int32_t i = 0; i < count; ++i) {                  // 1800d35d7 / 1800d38ad
            if (order.data[i] >= 0) continue;                  // 1800d35f4  already placed

            const Entry* const entries = _entries._data;
            const Entry* e = entries + i;

            // --- the FIRST named dependency -------------------------------
            int32_t relA = -2;                                 // 1800d3603
            {
                const char* want = e->relativeToX;
                if (e->horizontal == 0 || NamesNothing(want)) {
                    relA = -1;                                 // 1800d36d3  names nothing
                } else {
                    for (int32_t j = 0; j < count; ++j) {      // 1800d3650 / 1800d36c3
                        if (order.data[j] < 0) continue;       // not placed yet
                        if (j == i) continue;                  // 1800d3657
                        if (StringEquals(entries[j].name, want)) {   // 1800d366d..1800d36b8
                            relA = order.data[j];              // 1800d36ca
                            break;
                        }
                    }
                }
            }

            // --- the SECOND named dependency ------------------------------
            int32_t relB;
            {
                const char* want = e->relativeToY;
                if (e->vertical == 0 || NamesNothing(want)) {
                    relB = -1;                                 // 1800d37a6  names nothing
                } else {
                    relB = -2;                                 // 1800d37af (the local's
                                                               //            initial -2)
                    for (int32_t j = 0; j < count; ++j) {      // 1800d3720 / 1800d3795
                        if (order.data[j] < 0) continue;
                        if (j == i) continue;                  // 1800d3727
                        if (StringEquals(entries[j].name, want)) {   // 1800d373d..1800d3788
                            relB = order.data[j];              // 1800d379c
                            break;
                        }
                    }
                }
            }

            // `-2 < x` — lets -1 (names nothing) through and stops -2 (names
            // something not yet placed). The two sentinels are not the same.
            if (relA < -1 || relB < -1) continue;              // 1800d37b9 / 1800d37c2

            place(i, e, relA, relB);                           // 1800d37cb..1800d3883
        }
    }

    *reinterpret_cast<void**>(ret) = elem;                     // 1800d38cf

    // The order table's buffer is freed through MemoryAllocator::Free under the
    // allocator's own critical section (0x1800d38d7..0x1800d3906), the same
    // sequence ?MemFree@MemoryAllocator@System@@ is.
    if (order.data != nullptr) MemFree()(order.data);          // 1800d38d2
    order.data = nullptr;                                      // 1800d390c
    order.size = 0;                                            // 1800d3910
    order.capacity = 0;

    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
using UI::LayoutDescription;

extern "C" uint32_t RtUi_LayoutDescription_Type(const LayoutDescription* self) {
    return self->UI::LayoutDescription::Type();
}
extern "C" const char* RtUi_LayoutDescription_TypeName(const LayoutDescription* self) {
    return self->UI::LayoutDescription::TypeName();
}
extern "C" int32_t RtUi_LayoutDescription_GetFileVersion(const LayoutDescription* self) {
    return self->UI::LayoutDescription::GetFileVersion();
}
extern "C" void* RtUi_LayoutDescription_Construct(LayoutDescription* self) {
    if (self == nullptr) return nullptr;                       // 180043071
    return self->UI::LayoutDescription::Construct();
}
extern "C" void* RtUi_LayoutDescription_Reconstruct(LayoutDescription* self) {
    return self->UI::LayoutDescription::Reconstruct();
}
extern "C" void RtUi_LayoutDescription_Destruct(LayoutDescription* self) {
    self->UI::LayoutDescription::Destruct();
}
extern "C" bool RtUi_LayoutDescription_Serialize(LayoutDescription* self, void* stream) {
    return self->UI::LayoutDescription::Serialize(stream);
}
// RDX is the caller's slot; the member writes it and hands it back (B6-2).
extern "C" void** RtUi_LayoutDescription_CreateInstance(const LayoutDescription* self,
                                                        void** ret, const void* dialog) {
    return reinterpret_cast<void**>(self->UI::LayoutDescription::CreateInstance(
        reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),        // B.3.6 bridge
        static_cast<const UI::Dialog*>(dialog)));
}

// ============================================================================
// ??1LayoutDescription@UI@@UEAA@XZ   0x1800d3a10 — a DOOR (B4b-2)
// ============================================================================
// Restamp, destroy the entry Array through 0x180043a00 — the variant that DOES
// visit the elements (three String releases each) — then the base's _textName
// at +0x60 under the base vtable, then tail JMP.
extern "C" void RtUi_LayoutDescription_Dtor(void* selfv) {
    LayoutDescription* self = reinterpret_cast<LayoutDescription*>(selfv);   // B.3.6 bridge
    void** v = reinterpret_cast<void**>(selfv);

    *v = VtLayoutDescription();                                // 1800d3a26

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180043a00ull)(&self->_entries);      // 1800d3a34

    *v = VtElementDescription();                               // 1800d3a3a

    char*& text = self->_textName;
    if (text != nullptr) {                                     // 1800d3a48
        --*reinterpret_cast<int32_t*>(text - 4);
        if (*reinterpret_cast<int32_t*>(text - 4) == 0) {
            text = text - 8;
            MemFree()(text);
        } else {
            text = nullptr;                                    // 1800d3a66
        }
    }

    using ExtDtorFn = void (*)(void*);
    reinterpret_cast<ExtDtorFn>(AtomicExport(
        g_externalDtor, "??1External@Object@@UEAA@XZ"))(selfv);   // 1800d3a76 JMP
}

int RtUiLayoutDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@LayoutDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_LayoutDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180042d20ull, (void*)&RtUi_LayoutDescription_Type);

    RegisterRuntimeExport("?TypeName@LayoutDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_LayoutDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180042d40ull, (void*)&RtUi_LayoutDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@LayoutDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_LayoutDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d3a00ull, (void*)&RtUi_LayoutDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_LayoutDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180043060ull, (void*)&RtUi_LayoutDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@LayoutDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_LayoutDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180042fc0ull, (void*)&RtUi_LayoutDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@LayoutDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_LayoutDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180043100ull, (void*)&RtUi_LayoutDescription_Destruct);

    RegisterRuntimeExport("??1LayoutDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_LayoutDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d3a10ull, (void*)&RtUi_LayoutDescription_Dtor);

    RegisterRuntimeExport("?Serialize@LayoutDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_LayoutDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d3940ull, (void*)&RtUi_LayoutDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@LayoutDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_LayoutDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d33a0ull, (void*)&RtUi_LayoutDescription_CreateInstance);

    // ADDRESS ONLY: the entry-array serializer has no export name.
    RegisterRuntimeAddr(0x180043f50ull,
                        (void*)&RtUi_LayoutDescription_SerializeEntries);   ++n;

    return n;
}
