// ============================================================================
// runtime/src/ui/tabledescription.cpp — UI::TableDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7TableDescription@UI@@6B@ (retail 0x1801592c0), ten slots:
//   0  deleting dtor      0x180079210  deleting_dtor.cpp row (sizeof 0x98)
//   1  TypeName           0x180078ed0  here
//   2  Type               0x180078eb0  here
//   3  GetFileVersion     0x18011e6e0  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x18011e590  here
//   6  Reconstruct        0x180079050  here
//   7  Destruct           0x180079080  here
//   8  CreateInstance     0x18011e350  here
//   9  Construct          0x180079070  here
// plus ??0TableDescription@UI@@QEAA@XZ 0x18011e7a0 (out of line) and
// ??1TableDescription@UI@@UEAA@XZ 0x18011e6f0.
//
// LAYOUT, sizeof 0x98 (`MOV EDX,0x98` at 0x18007922f). ElementDescription
// (0x68) plus:
//     +0x68  int32   _gridA   ctor 1     kScalarInt; -> element +0xc4
//     +0x6c  int32   _gridB   ctor 1     kScalarInt; -> element +0xc0
//     +0x70  int32   _i70     ctor 0     kScalarInt; -> element +0xc8
//     +0x74  *** NEVER WRITTEN, NEVER READ ***
//     +0x78  Array<Cell>{int32 col, int32 row, ExternalPtr<ElementDescription>}
//     +0x80  _size, +0x84 _capacity
//     +0x88  (zeroed by the ctor; touched by nothing in this class)
//     +0x90  System::String — released by the destructor, and by nothing else
//                             here. It has no serializer and no reader in any
//                             of this class's ten slots, so all this file can
//                             say is that it is an owned String.
//
// THE CTOR'S +0x6c STORE IS AN UNALIGNED QWORD (`MOV qword [RCX+0x6c],1` at
// 0x18011e824), so it writes _gridB = 1 AND +0x70 = 0 in one instruction and
// stops at +0x73. +0x74 is the gap that leaves.
//
// TWO NEW COLLECTION HELPERS, appended to runtime/src/collection/array.cpp:
//   0x1800799a0  Array<Cell>::SetSize -> kSpec_TableCell10, whose default tail
//                is {-1, -1, nullptr} and NOT zero (see that row's comment)
//   0x180079870  ~Array<Cell>, the canonical free-and-zero body
// ??0TableElement@UI@@ (0x18011e2f0) is already ported (ui_factory_wave28.cpp).
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/TableDescription.h"
#include "../../types/ui/TableElement.h"
#include "../../types/io/Serializer.h"

// ENDGAME B.3.6 block C4: the layout moved to runtime/types/ui/
// TableDescription.h (the real class). Every slot body below is a MEMBER; each
// registered door is a one-line QUALIFIED delegation. The ctor and dtor stay
// DOORS (B4b-2). RULE B6-5: every export the members reach is cached in a
// namespace-scope std::atomic slot, never in a function-local static.

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
std::atomic<void*> g_vtTableDescription{nullptr};
std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_memFree{nullptr};
std::atomic<void*> g_externalDtor{nullptr};

void* VtBase()               { return AtomicExport(g_vtBase, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return AtomicExport(g_vtExternal, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return AtomicExport(g_vtElementDescription, "??_7ElementDescription@UI@@6B@"); }
void* VtTableDescription()   { return AtomicExport(g_vtTableDescription, "??_7TableDescription@UI@@6B@"); }
const uint8_t* TableDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@TableDescription@UI@@0VBaseTypeInfo@Object@@A"));
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

using MemFreeFn = void (*)(void*);

void ReleaseStringField(char** field) {
    char* s = *field;
    if (s == nullptr) return;
    if (--*reinterpret_cast<int32_t*>(s - 4) == 0) {
        *field = s - 8;
        reinterpret_cast<MemFreeFn>(AtomicExport(
            g_memFree, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"))(*field);
    } else {
        *field = nullptr;
    }
}

}  // namespace

// ============================================================================
// ??0TableDescription@UI@@QEAA@XZ   retail 0x18011e7a0 — a DOOR (B4b-2)
// ============================================================================
extern "C" void* RtUi_TableDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18011e7b1
    *v = VtExternal();                                          // 18011e7bb
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 18011e7d0
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18011e7d3
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18011e7d7
    *v = VtElementDescription();                                // 18011e7db
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 18011e7e5
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 18011e7e9
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 18011e7ed
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 18011e7f1  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 18011e7f4  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 18011e7fb  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 18011e803
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 18011e807
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 18011e80b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 18011e80f
    *v = VtTableDescription();                                  // 18011e813

    UI::TableDescription* self = reinterpret_cast<UI::TableDescription*>(p);   // B.3.6 bridge
    self->_columnCount = 1;                                     // 18011e81d  DWORD
    // ONE UNALIGNED QWORD covering +0x6c..+0x73: _rowCount = 1 and _spacing = 0.
    // _pad74 is left as it was — see the header.
    self->_rowCount = 1;                                        // 18011e824
    self->_spacing  = 0;
    self->_cells._data     = nullptr;                           // 18011e82c
    self->_cells._size     = 0;                                 // 18011e830  _size + _cap
    self->_cells._capacity = 0;
    self->_unknown88 = 0;                                       // 18011e837
    self->_unknown90 = nullptr;                                 // 18011e83e
    return p;                                                   // 18011e845
}

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t TableDescription::Type() const {
    const uint8_t* ti = TableDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* TableDescription::TypeName() const {
    const uint8_t* ti = TableDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t TableDescription::GetFileVersion() const { return 0x204; }

// 0x180079070 — the null test stays in the door.
Object::External* TableDescription::Construct() {
    return static_cast<Object::External*>(RtUi_TableDescription_Ctor(this));
}
// 0x180079050 — slot 0 (VIRTUAL, the minted row) with flag 0, then the ctor.
Object::External* TableDescription::Reconstruct() {
    DeletingDtor(0);
    return static_cast<Object::External*>(RtUi_TableDescription_Ctor(this));
}
// 0x180079080
void TableDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@TableDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x18011e590
// ============================================================================
// Three ints, then the cell count into a LOCAL, the read-path SetSize, and a
// per-cell loop that emits {int, int, ExternalPtr}. The String at +0x90 is NOT
// serialized — the destructor is the only body in the class that touches it.
bool TableDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 18011e5aa (qualified)

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 18011e5da

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_columnCount, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_rowCount, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_spacing, nullptr);

    int32_t n = _cells._size;                                  // 18011e61e
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                               // 18011e643 (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x1800799a0ull)(&_cells, n);         // 18011e651
    }

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    Cell* cell = _cells._data;                                 // 18011e656 (once)
    for (int32_t i = 0; i < n; ++i, ++cell) {                  // 18011e65e / 18011e6ae
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &cell->column, nullptr);
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &cell->row,    nullptr);
        RtAddr<FieldSerFn>(0x180038f90ull)(&cell->desc, &ser); // 18011e6a2
    }

    if (rw != nullptr) {                                       // 18011e6b8
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@TableDescription@UI@@...   retail 0x18011e350
// ============================================================================
// TableElement is 0xf0 bytes (`MOV EDX,0xf0` at 0x18011e398); its ctor is CALLED
// out of line (0x18011e2f0, the tableelement.cpp door).
//
// This is a GRID BUILDER. For every (a, b) with a in [0, _columnCount) and b in
// [0, _rowCount) it picks ONE cell description out of the flat cell list and
// instantiates it at the flat index `_columnCount * b + a`.
//
// *** THE PICK IS A FOUR-TIER SEARCH AND THE TIERS ARE NOT INTERCHANGEABLE. ***
// One pass over the cells, tracking three candidates:
//   exact     cell.column == a && cell.row == b     -> taken IMMEDIATELY,
//                                                      the pass stops
//   wildcardA cell.column <  0 && cell.row == b     -> FIRST such cell only
//   wildcardB cell.row <  0 && cell.column == a     -> FIRST such cell only
//   wildcardAB cell.column < 0 && cell.row < 0      -> the LAST such cell
// and the fallback order is exact, wildcardA, wildcardB, wildcardAB; if none
// matched, THE CELL IS LEFT EMPTY (no element is created for it).
//
// The "first such" / "last such" asymmetry is retail's and is the kind of thing
// a tidy-up destroys: the two wildcard branches are guarded by
// `TEST R9D/R10D,R9D/R10D; JNS <skip>` (keep the first), while the both-wildcard
// branch is a bare `CMOVL R8D,EAX` with NO such guard (keep the last). Three
// lines apart in the shipped code, and deliberate.
//
// THE FLAT INDEX IS `_columnCount * b + a`, computed with the DESCRIPTION's
// count (0x18011e4f2), and then recomputed a second time from 64-bit copies of
// the same counters (0x18011e524) for the parent write. Retail computes it
// twice; so does this.
Object::InternalPtr<Element>* TableDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xf0, 8);   // 18011e398 (inlined)

    TableElement* elem = nullptr;
    if (raw != nullptr) {                                      // 18011e3c3
        using TableElementCtorFn = void* (*)(void*);
        elem = reinterpret_cast<TableElement*>(                // B.3.6 bridge
            RtAddr<TableElementCtorFn>(0x18011e2f0ull)(raw));  // 18011e3c8
    }

    void* slot = elem;                                         // 18011e3d5
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(this, &slot);   // 18011e3e2

    const int32_t columns = _columnCount;
    const int32_t rows    = _rowCount;

    elem->_columnCount = columns;                              // 18011e3ea  CROSSED:
    elem->_rowCount    = rows;                                 // 18011e3f4  +0x68->0xc4, +0x6c->0xc0

    using SetSizeFn = void (*)(void* array, int n);
    RtAddr<SetSizeFn>(0x18002fea0ull)(&elem->_rowSizes, rows);        // 18011e405  Array<int32>
    RtAddr<SetSizeFn>(0x18002fea0ull)(&elem->_columnSizes, columns);  // 18011e414  Array<int32>

    elem->_spacing = _spacing;                                 // 18011e41c

    using PtrSetSizeFn = void (*)(void* array, int n);
    RtAddr<PtrSetSizeFn>(0x1800764c0ull)(&elem->_children, columns * rows);  // 18011e431

    for (int32_t a = 0; a < _columnCount; ++a) {
        for (int32_t b = 0; b < _rowCount; ++b) {
            int32_t wildA  = -1;                               // 18011e460  R9
            int32_t wildB  = -1;                               // 18011e464  R10
            int32_t wildAB = -1;                               // 18011e467  R8
            int32_t pick   = -1;

            const int32_t cells = _cells._size;
            bool exact = false;
            if (cells > 0) {                                   // 18011e473
                const Cell* c = _cells._data;
                for (int32_t i = 0; i < cells; ++i, ++c) {
                    const int32_t col = c->column;
                    const int32_t row = c->row;

                    if (a == col && b == row) {                // 18011e482 / 18011e486
                        pick = i; exact = true; break;         // 18011e4cb
                    }
                    if (wildA < 0 && col < 0 && b == row) {    // 18011e48b..18011e499
                        wildA = i;
                    } else if (wildB < 0 && row < 0 && a == col) {  // 18011e49e..18011e4ad
                        wildB = i;
                    } else if (col < 0 && row < 0) {           // 18011e4b2..18011e4ba
                        wildAB = i;    // NO "first only" guard — the LAST wins
                    }
                }
            }

            if (!exact) {                                      // 18011e4cf
                pick = wildA;
                if (pick < 0) pick = wildB;
                if (pick < 0) pick = wildAB;
                if (pick < 0) continue;                        // 18011e4e5 JS -> skip
            }

            const Cell* cell = _cells._data + pick;

            const int32_t flat = _columnCount * b + a;         // 18011e4f2

            Element** kids = elem->_children._data;            // 18011e4fd
            Element** dst  = kids + flat;                      // 18011e504

            using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
            void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(
                dialog, &slot, cell->desc);                    // 18011e519

            *dst = static_cast<Element*>(*made);               // 18011e521

            // The flat index RECOMPUTED, and the array data pointer RE-READ,
            // before the parent write (0x18011e524..0x18011e53a). Retail does
            // not reuse `dst`; neither does this.
            const int64_t flat2 = static_cast<int64_t>(_columnCount) *
                                  static_cast<int64_t>(b) + static_cast<int64_t>(a);
            Element* child = elem->_children._data[flat2];
            child->_parent = elem;                             // 18011e53a
        }
    }

    *reinterpret_cast<void**>(ret) = elem;                     // 18011e562 / 18011e567
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
using UI::TableDescription;

extern "C" uint32_t RtUi_TableDescription_Type(const TableDescription* self) {
    return self->UI::TableDescription::Type();
}
extern "C" const char* RtUi_TableDescription_TypeName(const TableDescription* self) {
    return self->UI::TableDescription::TypeName();
}
extern "C" int32_t RtUi_TableDescription_GetFileVersion(const TableDescription* self) {
    return self->UI::TableDescription::GetFileVersion();
}
extern "C" void* RtUi_TableDescription_Construct(TableDescription* self) {
    if (self == nullptr) return nullptr;                       // 180079070
    return self->UI::TableDescription::Construct();
}
extern "C" void* RtUi_TableDescription_Reconstruct(TableDescription* self) {
    return self->UI::TableDescription::Reconstruct();
}
extern "C" void RtUi_TableDescription_Destruct(TableDescription* self) {
    self->UI::TableDescription::Destruct();
}
extern "C" bool RtUi_TableDescription_Serialize(TableDescription* self, void* stream) {
    return self->UI::TableDescription::Serialize(stream);
}
// RDX is the caller's slot; the member writes it and hands it back (B6-2).
extern "C" void** RtUi_TableDescription_CreateInstance(const TableDescription* self, void** ret,
                                                       const void* dialog) {
    return reinterpret_cast<void**>(self->UI::TableDescription::CreateInstance(
        reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),        // B.3.6 bridge
        static_cast<const UI::Dialog*>(dialog)));
}

// ??1TableDescription@UI@@UEAA@XZ  0x18011e6f0 — a DOOR (B4b-2). Release the
// String at +0x90 FIRST, then destroy the cell Array, then the base's _textName
// at +0x60 under the base vtable, then tail JMP.
extern "C" void RtUi_TableDescription_Dtor(void* selfv) {
    TableDescription* self = reinterpret_cast<TableDescription*>(selfv);   // B.3.6 bridge
    void** v = reinterpret_cast<void**>(selfv);

    *v = VtTableDescription();                                 // 18011e706
    ReleaseStringField(&self->_unknown90);                     // 18011e710

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180079870ull)(&self->_cells);        // 18011e74a

    *v = VtElementDescription();                               // 18011e750
    ReleaseStringField(&self->_textName);                      // 18011e75a

    using ExtDtorFn = void (*)(void*);
    reinterpret_cast<ExtDtorFn>(AtomicExport(
        g_externalDtor, "??1External@Object@@UEAA@XZ"))(selfv);   // 18011e78c JMP
}

int RtUiTableDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@TableDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_TableDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180078eb0ull, (void*)&RtUi_TableDescription_Type);

    RegisterRuntimeExport("?TypeName@TableDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_TableDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180078ed0ull, (void*)&RtUi_TableDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@TableDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_TableDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x18011e6e0ull, (void*)&RtUi_TableDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@TableDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_TableDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180079070ull, (void*)&RtUi_TableDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@TableDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_TableDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180079050ull, (void*)&RtUi_TableDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@TableDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_TableDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180079080ull, (void*)&RtUi_TableDescription_Destruct);

    RegisterRuntimeExport("??0TableDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_TableDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x18011e7a0ull, (void*)&RtUi_TableDescription_Ctor);

    RegisterRuntimeExport("??1TableDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_TableDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x18011e6f0ull, (void*)&RtUi_TableDescription_Dtor);

    RegisterRuntimeExport("?Serialize@TableDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_TableDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x18011e590ull, (void*)&RtUi_TableDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@TableDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_TableDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x18011e350ull, (void*)&RtUi_TableDescription_CreateInstance);

    return n;
}
