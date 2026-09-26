// ============================================================================
// runtime/src/ui/tableelement.cpp — UI::TableElement, the whole class
// ============================================================================
//
// `??_7TableElement@UI@@6B@` (retail 0x180159260):
//
//   0  vector deleting dtor  0x180079600  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x1800792d0  here
//   2  Type                  0x1800792b0  here
//   4  ComputeSize           0x18011df60  here
//   5  ComputeLayout         0x18011dd10  here
//   6  Draw                  0x18011dc20  here
//
// plus ??0TableElement (0x18011e2f0) and ??1TableElement (0x18011e2a0).
//
// NOT HERE: `?GetRowHeight@` (0x18011e090) and `?GetColumnWidth@`
// (0x18011e1a0) — both on the UI non-slot callee list; ComputeSize reaches
// them by address. `?SetChild@` / `?SetSize@` are lane E's, in
// widgets_layout.cpp.
//
// ---------------------------------------------------------------------------
// LAYOUT (runtime/types/ui/TableElement.h, every offset already pinned there
// by the three exported mutators; the ctor at 0x18011e2f0 confirms them and
// adds the one it did not have):
//
//   +0xC0  _rowCount  int32   ?GetRowCount@    0x180079450
//   +0xC4  _colCount  int32   ?GetColumnCount@ 0x180079460
//   +0xC8  _spacing   int32   the gap inserted between two NON-EMPTY rows or
//                             columns — ctor 0, and the field TableElement.h
//                             did not name
//   +0xD0  _rowSizes  Collection::Array<int32>
//   +0xE0  _colSizes  Collection::Array<int32>
//
// sizeof == 0xF0 (deleting dtor `MOV EDX,0xf0`). ~TableElement destroys
// _colSizes FIRST and then _rowSizes — reverse declaration order, as C++
// requires — through the shared `~Array` copy at 0x18001c750.
//
// The CELLS are UI::Element::_children itself (+0xA0), indexed
// `_colCount * row + col`; there is no second array.
//
// ENDGAME B.3.6 block C4: the layout is the real class in runtime/types/ui/
// TableElement.h. Every slot body below is a MEMBER; each registered door is a
// one-line QUALIFIED delegation; ctor/dtor stay DOORS. RULE B6-5: the vtable
// and type-info record are cached in namespace-scope std::atomic slots.
// ============================================================================

#include <atomic>

#include "widget_common.h"
#include "../../types/ui/TableElement.h"

using namespace rtui;

namespace {

std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_vtable{nullptr};

void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
const uint8_t* TypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@TableElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* Vtable() { return AtomicExport(g_vtable, "??_7TableElement@UI@@6B@"); }

// `?GetRowHeight@` (0x18011e090) / `?GetColumnWidth@` (0x18011e1a0) — declared
// on the class, bodies still element_bodies_b5.cpp's doors (fragment pass).
using ExtentFn = int32_t (*)(void* self, int32_t index);

}  // namespace

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t TableElement::Type() const {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* TableElement::TypeName() const {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@TableElement@UI@@UEAAXXZ   0x18011df60
//
// Total = sum of extents + one spacing between each pair of NON-EMPTY ones.
// The "first non-empty" flag is what keeps a leading zero-width column from
// contributing a gap.
// ---------------------------------------------------------------------------
void TableElement::ComputeSize() {
    if ((_flags & kFlagNeedsSizeCalc) == 0) return;

    _layoutWidth  = 0;     // one qword store
    _layoutHeight = 0;

    bool first = true;
    for (int32_t c = 0; c < _columnCount; ++c) {
        int32_t* cols = _columnSizes._data;                                 // loaded BEFORE the call
        cols[c] = RtAddr<ExtentFn>(0x18011e1a0ull)(this, c);                // GetColumnWidth
        cols = _columnSizes._data;                                          // re-read
        _layoutWidth = _layoutWidth + cols[c];
        if (cols[c] != 0) {
            if (first) first = false;
            else _layoutWidth = _spacing + _layoutWidth;
        }
    }

    first = true;
    for (int32_t r = 0; r < _rowCount; ++r) {
        int32_t* rows = _rowSizes._data;                                    // loaded BEFORE the call
        rows[r] = RtAddr<ExtentFn>(0x18011e090ull)(this, r);                // GetRowHeight
        rows = _rowSizes._data;                                             // re-read
        _layoutHeight = rows[r] + _layoutHeight;
        if (rows[r] != 0) {
            if (first) first = false;
            else _layoutHeight = _spacing + _layoutHeight;
        }
    }

    _flags &= ~kFlagNeedsSizeCalc;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@TableElement@UI@@UEAAXHHHH@Z   0x18011dd10
// ---------------------------------------------------------------------------
void TableElement::ComputeLayout(int32_t x, int32_t y, int32_t right, int32_t bottom) {
    if (Element* border = static_cast<Element*>(_border))
        border->ComputeLayout(x, y, right, bottom);

    ComputeSize();                                            // slot 4, virtual

    int32_t w = _layoutWidth;
    int32_t h = _layoutHeight;
    const int32_t align = _alignment;

    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + _leftPad;
    int32_t py = y + _topPad;
    const int32_t aw = (right  - _rightPad)  - px;
    const int32_t ah = (bottom - _bottomPad) - py;

    if ((_flags & kFlagStretchX) != 0) w = aw;
    if ((_flags & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - _rightPad) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - _bottomPad) - h;
    }

    _layoutRight  = px + w;
    _layoutLeft   = px;
    _layoutTop    = py;
    _layoutBottom = py + h;

    int32_t cellTop = py;
    for (int32_t r = 0; r < _rowCount; ++r) {
        const int32_t rowH = _rowSizes._data[r];
        int32_t cellLeft = px;
        for (int32_t c = 0; c < _columnCount; ++c) {
            const int32_t colW = _columnSizes._data[c];
            Element* cell = _children[_columnCount * r + c];
            if (cell != nullptr && (cell->_flags & kFlagHidden) == 0)
                cell->ComputeLayout(cellLeft, cellTop, colW + cellLeft, cellTop + rowH);
            cellLeft += colW;
            if (colW > 0) cellLeft += _spacing;
        }
        cellTop += rowH;
        if (rowH > 0) cellTop += _spacing;
    }
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@TableElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x18011dc20
// ---------------------------------------------------------------------------
void TableElement::Draw(SpriteContext& context, uint8_t alpha) const {
    if (Element* border = static_cast<Element*>(_border))
        border->Draw(context, alpha);

    if ((_flags & kFlagDimColor) != 0) alpha >>= 1;

    for (int32_t r = 0; r < _rowCount; ++r) {
        int32_t cols = _columnCount;
        for (int32_t c = 0; c < cols; ++c) {
            Element* cell = _children[cols * r + c];
            if (cell != nullptr && (cell->_flags & kFlagHidden) == 0)
                cell->Draw(context, alpha);
            cols = _columnCount;
        }
    }
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
using UI::TableElement;

extern "C" uint32_t RtUi_TableElement_Type(const TableElement* self) {
    return self->UI::TableElement::Type();
}
extern "C" const char* RtUi_TableElement_TypeName(const TableElement* self) {
    return self->UI::TableElement::TypeName();
}
extern "C" void RtUi_TableElement_ComputeSize(TableElement* self) {
    self->UI::TableElement::ComputeSize();
}
extern "C" void RtUi_TableElement_ComputeLayout(TableElement* self, int32_t x, int32_t y,
                                                int32_t right, int32_t bottom) {
    self->UI::TableElement::ComputeLayout(x, y, right, bottom);
}
extern "C" void RtUi_TableElement_Draw(const TableElement* self, void* ctx, uint8_t alpha) {
    self->UI::TableElement::Draw(*static_cast<UI::SpriteContext*>(ctx), alpha);
}

// ??0TableElement@UI@@QEAA@XZ   0x18011e2f0 — a DOOR (B4b-2)
extern "C" void* RtUi_TableElement_Ctor(void* selfv) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(selfv);
    PTR(selfv, 0x00) = Vtable();
    TableElement* self = reinterpret_cast<TableElement*>(selfv);   // B.3.6 bridge
    self->_rowCount    = 0;
    self->_columnCount = 0;
    self->_spacing     = 0;
    self->_rowSizes._data        = nullptr;
    self->_rowSizes._size        = 0;
    self->_rowSizes._capacity    = 0;
    self->_columnSizes._data     = nullptr;
    self->_columnSizes._size     = 0;
    self->_columnSizes._capacity = 0;
    return selfv;
}

// ??1TableElement@UI@@UEAA@XZ   0x18011e2a0 — a DOOR (B4b-2)
extern "C" void RtUi_TableElement_Dtor(void* selfv) {
    PTR(selfv, 0x00) = Vtable();
    TableElement* self = reinterpret_cast<TableElement*>(selfv);   // B.3.6 bridge
    RtAddr<ArrayDtorFn>(0x18001c750ull)(&self->_columnSizes);
    RtAddr<ArrayDtorFn>(0x18001c750ull)(&self->_rowSizes);
    ElementDtor(selfv);
}

// ============================================================================
int RtUiTableElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@TableElement@UI@@UEBAIXZ",
                          (void*)&RtUi_TableElement_Type);            ++n;
    RegisterRuntimeAddr(0x1800792b0ull, (void*)&RtUi_TableElement_Type);

    RegisterRuntimeExport("?TypeName@TableElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_TableElement_TypeName);        ++n;
    RegisterRuntimeAddr(0x1800792d0ull, (void*)&RtUi_TableElement_TypeName);

    RegisterRuntimeExport("??0TableElement@UI@@QEAA@XZ",
                          (void*)&RtUi_TableElement_Ctor);            ++n;
    RegisterRuntimeAddr(0x18011e2f0ull, (void*)&RtUi_TableElement_Ctor);

    RegisterRuntimeExport("??1TableElement@UI@@UEAA@XZ",
                          (void*)&RtUi_TableElement_Dtor);            ++n;
    RegisterRuntimeAddr(0x18011e2a0ull, (void*)&RtUi_TableElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@TableElement@UI@@UEAAXXZ",
                          (void*)&RtUi_TableElement_ComputeSize);     ++n;
    RegisterRuntimeAddr(0x18011df60ull, (void*)&RtUi_TableElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@TableElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_TableElement_ComputeLayout);   ++n;
    RegisterRuntimeAddr(0x18011dd10ull, (void*)&RtUi_TableElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@TableElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_TableElement_Draw);            ++n;
    RegisterRuntimeAddr(0x18011dc20ull, (void*)&RtUi_TableElement_Draw);

    return n;
}
