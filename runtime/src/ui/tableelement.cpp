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
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/TableElement.h"

using namespace rtui;

namespace {

constexpr int kRowCount = 0xC0;
constexpr int kColCount = 0xC4;
constexpr int kSpacing  = 0xC8;
constexpr int kRowSizes = 0xD0;
constexpr int kColSizes = 0xE0;

static_assert(kRowCount == UI::TableElementOffsets::kRowCount, "");
static_assert(kColCount == UI::TableElementOffsets::kColCount, "");
static_assert(kRowSizes == UI::TableElementOffsets::kRowSizes, "");
static_assert(kColSizes == UI::TableElementOffsets::kColSizes, "");

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@TableElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7TableElement@UI@@6B@"); }

using ExtentFn = int32_t (*)(void* self, int32_t index);

}  // namespace

extern "C" uint32_t RtUi_TableElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_TableElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0TableElement@UI@@QEAA@XZ   0x18011e2f0
extern "C" void* RtUi_TableElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    I32(self, kRowCount) = 0;
    I32(self, kColCount) = 0;
    I32(self, kSpacing)  = 0;
    PTR(self, kRowSizes)     = nullptr;
    PTR(self, kRowSizes + 8) = nullptr;
    PTR(self, kColSizes)     = nullptr;
    PTR(self, kColSizes + 8) = nullptr;
    return self;
}

// ??1TableElement@UI@@UEAA@XZ   0x18011e2a0
extern "C" void RtUi_TableElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    RtAddr<ArrayDtorFn>(0x18001c750ull)(B(self, kColSizes));
    RtAddr<ArrayDtorFn>(0x18001c750ull)(B(self, kRowSizes));
    ElementDtor(self);
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@TableElement@UI@@UEAAXXZ   0x18011df60
//
// Total = sum of extents + one spacing between each pair of NON-EMPTY ones.
// The "first non-empty" flag is what keeps a leading zero-width column from
// contributing a gap.
// ---------------------------------------------------------------------------
extern "C" void RtUi_TableElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    I32(self, kElLayoutWidth)  = 0;     // one qword store
    I32(self, kElLayoutHeight) = 0;

    bool first = true;
    for (int32_t c = 0; c < I32(self, kColCount); ++c) {
        int32_t* cols = reinterpret_cast<int32_t*>(PTR(self, kColSizes));
        cols[c] = RtAddr<ExtentFn>(0x18011e1a0ull)(self, c);   // GetColumnWidth
        cols = reinterpret_cast<int32_t*>(PTR(self, kColSizes));
        I32(self, kElLayoutWidth) = I32(self, kElLayoutWidth) + cols[c];
        if (cols[c] != 0) {
            if (first) first = false;
            else I32(self, kElLayoutWidth) = I32(self, kSpacing) + I32(self, kElLayoutWidth);
        }
    }

    first = true;
    for (int32_t r = 0; r < I32(self, kRowCount); ++r) {
        int32_t* rows = reinterpret_cast<int32_t*>(PTR(self, kRowSizes));
        rows[r] = RtAddr<ExtentFn>(0x18011e090ull)(self, r);   // GetRowHeight
        rows = reinterpret_cast<int32_t*>(PTR(self, kRowSizes));
        I32(self, kElLayoutHeight) = rows[r] + I32(self, kElLayoutHeight);
        if (rows[r] != 0) {
            if (first) first = false;
            else I32(self, kElLayoutHeight) = I32(self, kSpacing) + I32(self, kElLayoutHeight);
        }
    }

    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@TableElement@UI@@UEAAXHHHH@Z   0x18011dd10
// ---------------------------------------------------------------------------
extern "C" void RtUi_TableElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                int32_t right, int32_t bottom) {
    if (void* border = PTR(self, kElBorder))
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(border)[kVtComputeLayout / 8])(
            border, x, y, right, bottom);

    reinterpret_cast<ComputeSizeSlotFn>(VTable(self)[kVtComputeSize / 8])(self);

    int32_t w = I32(self, kElLayoutWidth);
    int32_t h = I32(self, kElLayoutHeight);
    const int32_t align = I32(self, kElAlignment);

    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + I32(self, kElLeftPad);
    int32_t py = y + I32(self, kElTopPad);
    const int32_t aw = (right  - I32(self, kElRightPad))  - px;
    const int32_t ah = (bottom - I32(self, kElBottomPad)) - py;

    if ((U32(self, kElFlags) & kFlagStretchX) != 0) w = aw;
    if ((U32(self, kElFlags) & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - I32(self, kElRightPad)) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - I32(self, kElBottomPad)) - h;
    }

    I32(self, kElLayoutRight)  = px + w;
    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutBottom) = py + h;

    int32_t cellTop = py;
    for (int32_t r = 0; r < I32(self, kRowCount); ++r) {
        const int32_t rowH = reinterpret_cast<int32_t*>(PTR(self, kRowSizes))[r];
        int32_t cellLeft = px;
        for (int32_t c = 0; c < I32(self, kColCount); ++c) {
            const int32_t colW = reinterpret_cast<int32_t*>(PTR(self, kColSizes))[c];
            void* cell = reinterpret_cast<void**>(PTR(self, kElChildren))
                             [I32(self, kColCount) * r + c];
            if (cell != nullptr && (U32(cell, kElFlags) & kFlagHidden) == 0) {
                reinterpret_cast<ComputeLayoutSlotFn>(VTable(cell)[kVtComputeLayout / 8])(
                    cell, cellLeft, cellTop, colW + cellLeft, cellTop + rowH);
            }
            cellLeft += colW;
            if (colW > 0) cellLeft += I32(self, kSpacing);
        }
        cellTop += rowH;
        if (rowH > 0) cellTop += I32(self, kSpacing);
    }
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@TableElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x18011dc20
// ---------------------------------------------------------------------------
extern "C" void RtUi_TableElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;

    for (int32_t r = 0; r < I32(me, kRowCount); ++r) {
        int32_t cols = I32(me, kColCount);
        for (int32_t c = 0; c < cols; ++c) {
            void* cell = reinterpret_cast<void**>(PTR(me, kElChildren))[cols * r + c];
            if (cell != nullptr && (U32(cell, kElFlags) & kFlagHidden) == 0)
                reinterpret_cast<DrawSlotFn>(VTable(cell)[kVtDraw / 8])(cell, ctx, alpha);
            cols = I32(me, kColCount);
        }
    }
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
