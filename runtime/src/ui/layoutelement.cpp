// ============================================================================
// runtime/src/ui/layoutelement.cpp — UI::LayoutElement, the whole class
// ============================================================================
//
// `??_7LayoutElement@UI@@6B@` (retail 0x180156c60), and the ctor/dtor that
// stamp it. Vtable extent and per-slot targets from the batch survey:
//
//   0  vector deleting dtor  0x180043570  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180043290  here
//   2  Type                  0x180043270  here
//   3  Persist               0x180116870  UI::Element's own (not overridden)
//   4  ComputeSize           0x1800d3100  here
//   5  ComputeLayout         0x1800d2af0  here
//   6  Draw                  0x1800d2a40  here
//   7..11                    UI::Element's own
//
// plus ??0LayoutElement@UI@@QEAA@XZ (0x1800d32e0) and
// ??1LayoutElement@UI@@UEAA@XZ (0x1800d32a0).
//
// NOT HERE: `?GetChildLayout@LayoutElement@UI@@AEAAXHHHHHH_N@Z` (0x1800d2dc0)
// and its two helpers GetChildLayoutX/Y — the batch brief assigns the UI
// non-slot callee list to the UI-text agent, so this file REACHES
// GetChildLayout by address instead of porting a second copy.
//
// ---------------------------------------------------------------------------
// LAYOUT.  sizeof(LayoutElement) == 0xD0, and it is proven, not inferred: the
// vector deleting destructor at 0x180043570 passes `MOV EDX,0xd0` as the array
// stride to `eh vector destructor iterator`, and ~LayoutElement destroys
// exactly one member — the array header at +0xC0 — before tail-calling
// ~Element. UI::Element is 0xC0 (runtime/types/ui/Element.h), so the class
// adds exactly one 0x10-byte Array header:
//
//   +0xC0  _rows._data   the per-child layout record buffer, stride 0x44
//   +0xC8  _rows._size / _capacity
//
// The STRIDE is 0x44 and the record's fields are read at +0x18, +0x1c, +0x20,
// +0x24, +0x28, +0x2c, +0x30, +0x34, +0x38, +0x3c, +0x40 by the three bodies
// below; the record's own writer is GetChildLayout, which is not this file's.
// The COUNT the loops use is UI::Element::_children._size (+0xA8), not the
// array's own +0xC8 — the two are kept parallel by GetChildLayout's caller.
// ============================================================================

#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kRows       = 0xC0;   // Array<LayoutRow> header
constexpr int kRowStride  = 0x44;

// Record fields, each named by the body that reads it (0x1800d3100 /
// 0x1800d2af0).
constexpr int kRowForce   = 0x18;   // byte: "lay this child out even if hidden"
constexpr int kRowSizeW   = 0x1c;   // GetPaddedSize out-param 1
constexpr int kRowSizeH   = 0x20;   // GetPaddedSize out-param 2
constexpr int kRowLeft    = 0x24;
constexpr int kRowSizeMinX= 0x28;   // ComputeSize's running min-x
constexpr int kRowTop     = 0x2c;
constexpr int kRowSizeMinY= 0x30;
constexpr int kRowRight   = 0x34;
constexpr int kRowSizeMaxX= 0x38;
constexpr int kRowBottom  = 0x3c;
constexpr int kRowSizeMaxY= 0x40;

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@LayoutElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7LayoutElement@UI@@6B@"); }

// `?GetChildLayout@LayoutElement@UI@@AEAAXHHHHHH_N@Z` (0x1800d2dc0).
using GetChildLayoutFn = void (*)(void* self, int32_t index, int32_t sizePass,
                                  int32_t left, int32_t top, int32_t right,
                                  int32_t bottom, bool visible);

// Array<LayoutRow>::~Array — 0x180043cb0, one of the parameterless `~Array`
// copies; registered from runtime/src/collection/array.cpp (this batch adds
// the J1 row), so ResolveAddr hands back OUR body.
void RowsDtor(void* hdr) { RtAddr<ArrayDtorFn>(0x180043cb0ull)(hdr); }

}  // namespace

// ---------------------------------------------------------------------------
// slot 2 / slot 1
// ---------------------------------------------------------------------------
extern "C" uint32_t RtUi_LayoutElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_LayoutElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ---------------------------------------------------------------------------
// ??0LayoutElement@UI@@QEAA@XZ   0x1800d32e0
// ---------------------------------------------------------------------------
extern "C" void* RtUi_LayoutElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);                       // 1800d32f6  CALL 0x180115e50
    PTR(self, 0x00) = Vtable();              // 1800d32fb  LEA/MOV
    PTR(self, kRows)     = nullptr;          // +0xC0
    PTR(self, kRows + 8) = nullptr;          // +0xC8  size and capacity, one qword
    return self;
}

// ---------------------------------------------------------------------------
// ??1LayoutElement@UI@@UEAA@XZ   0x1800d32a0
// ---------------------------------------------------------------------------
extern "C" void RtUi_LayoutElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    RowsDtor(B(self, kRows));
    ElementDtor(self);
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@LayoutElement@UI@@UEAAXXZ   0x1800d3100
// ---------------------------------------------------------------------------
extern "C" void RtUi_LayoutElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    // One qword store clears both size fields.
    I32(self, kElLayoutWidth)  = 0;
    I32(self, kElLayoutHeight) = 0;

    int32_t minX = 0, maxX = 0, minY = 0, maxY = 0;

    const int32_t n = I32(self, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        void*    child = reinterpret_cast<void**>(PTR(self, kElChildren))[i];
        uint8_t* row   = static_cast<uint8_t*>(PTR(self, kRows)) + i * kRowStride;

        // `(~child->_flags & 1) == 0` is "child IS hidden".
        const bool hidden  = (U32(child, kElFlags) & kFlagHidden) != 0;
        const bool visible = !(hidden && row[kRowForce] == 0);

        if (!visible) {
            I32(row, kRowSizeW) = 0;
            I32(row, kRowSizeH) = 0;
        } else {
            RtAddr<GetPaddedSizeFn>(0x180115f10ull)(
                child,
                reinterpret_cast<int32_t*>(row + kRowSizeW),
                reinterpret_cast<int32_t*>(row + kRowSizeH));
        }

        RtAddr<GetChildLayoutFn>(0x1800d2dc0ull)(self, i, 1, 0, 0, 0, 0, visible);

        // Re-read the base: GetChildLayout may have grown the array.
        row = static_cast<uint8_t*>(PTR(self, kRows)) + i * kRowStride;
        if (I32(row, kRowSizeMinX) <= minX) minX = I32(row, kRowSizeMinX);
        if (maxX <= I32(row, kRowSizeMaxX)) maxX = I32(row, kRowSizeMaxX);
        if (I32(row, kRowSizeMinY) <= minY) minY = I32(row, kRowSizeMinY);
        if (maxY <= I32(row, kRowSizeMaxY)) maxY = I32(row, kRowSizeMaxY);
    }

    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElLayoutWidth)  = maxX - minX;
    I32(self, kElLayoutHeight) = maxY - minY;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@LayoutElement@UI@@UEAAXHHHH@Z   0x1800d2af0
// ---------------------------------------------------------------------------
extern "C" void RtUi_LayoutElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                 int32_t right, int32_t bottom) {
    if (void* border = PTR(self, kElBorder))
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(border)[kVtComputeLayout / 8])(
            border, x, y, right, bottom);

    reinterpret_cast<ComputeSizeSlotFn>(VTable(self)[kVtComputeSize / 8])(self);

    const int32_t align = I32(self, kElAlignment);
    if (right  < I32(self, kElLayoutWidth)  + x) x = right  - I32(self, kElLayoutWidth);
    if (bottom < I32(self, kElLayoutHeight) + y) y = bottom - I32(self, kElLayoutHeight);

    int32_t px = x + I32(self, kElLeftPad);
    int32_t py = y + I32(self, kElTopPad);
    const int32_t aw = (right  - I32(self, kElRightPad))  - px;
    const int32_t ah = (bottom - I32(self, kElBottomPad)) - py;

    // LayoutElement takes neither stretch test and neither centring arm: the
    // only surviving case of the aligned-position switch is 2/5/8, and its
    // body re-derives the value it already has. Transcribed as shipped.
    switch (align) {
        case 2: case 5: case 8:
            px = (right - I32(self, kElRightPad)) - aw;
            break;
        default:
            break;
    }
    if (align > 2 && align > 5 && align < 9)
        py = (bottom - I32(self, kElBottomPad)) - ah;

    I32(self, kElLayoutRight)  = aw + px;
    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutBottom) = ah + py;

    int32_t minX = 10000, maxX = -10000, minY = 10000, maxY = -10000;

    int32_t n = I32(self, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        void*          child = reinterpret_cast<void**>(PTR(self, kElChildren))[i];
        const uint8_t* row0  = static_cast<uint8_t*>(PTR(self, kRows)) + i * kRowStride;

        const bool hidden  = (U32(child, kElFlags) & kFlagHidden) != 0;
        const bool visible = !(hidden && row0[kRowForce] == 0);

        RtAddr<GetChildLayoutFn>(0x1800d2dc0ull)(
            self, i, 0,
            I32(self, kElLayoutLeft), I32(self, kElLayoutTop),
            I32(self, kElLayoutRight), I32(self, kElLayoutBottom), visible);

        uint8_t* row = static_cast<uint8_t*>(PTR(self, kRows)) + i * kRowStride;
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(child)[kVtComputeLayout / 8])(
            child, I32(row, kRowLeft), I32(row, kRowTop),
            I32(row, kRowRight), I32(row, kRowBottom));

        row = static_cast<uint8_t*>(PTR(self, kRows)) + i * kRowStride;
        if (I32(row, kRowLeft)   <= minX) minX = I32(row, kRowLeft);
        if (maxX <= I32(row, kRowRight))  maxX = I32(row, kRowRight);
        if (I32(row, kRowTop)    <= minY) minY = I32(row, kRowTop);
        if (maxY <= I32(row, kRowBottom)) maxY = I32(row, kRowBottom);
        n = I32(self, kElChildCount);
    }

    const int32_t dy = I32(self, kElLayoutTop)  - minY;
    const int32_t dx = I32(self, kElLayoutLeft) - minX;

    n = I32(self, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[i];
        I32(child, kElLayoutLeft)   += dx;
        I32(child, kElLayoutRight)  += dx;
        I32(child, kElLayoutTop)    += dy;
        I32(child, kElLayoutBottom) += dy;

        const int32_t gn = I32(child, kElChildCount);
        for (int32_t g = 0; g < gn; ++g) {
            void* grand = reinterpret_cast<void**>(PTR(child, kElChildren))[g];
            if (grand != nullptr)
                RtAddr<AdjustUpperLeftFn>(0x180115840ull)(grand, dx, dy);
        }
        if (void* b = PTR(child, kElBorder))
            RtAddr<AdjustUpperLeftFn>(0x180115840ull)(b, dx, dy);
        n = I32(self, kElChildCount);
    }
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@LayoutElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d2a40
// ---------------------------------------------------------------------------
extern "C" void RtUi_LayoutElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    const int32_t n = I32(me, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        void* child = reinterpret_cast<void**>(PTR(me, kElChildren))[i];
        if ((U32(child, kElFlags) & kFlagHidden) == 0)
            reinterpret_cast<DrawSlotFn>(VTable(child)[kVtDraw / 8])(child, ctx, alpha);
    }
}

// ============================================================================
// Registration — both doors for every body.
// ============================================================================
int RtUiLayoutElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@LayoutElement@UI@@UEBAIXZ",
                          (void*)&RtUi_LayoutElement_Type);            ++n;
    RegisterRuntimeAddr(0x180043270ull, (void*)&RtUi_LayoutElement_Type);

    RegisterRuntimeExport("?TypeName@LayoutElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_LayoutElement_TypeName);        ++n;
    RegisterRuntimeAddr(0x180043290ull, (void*)&RtUi_LayoutElement_TypeName);

    RegisterRuntimeExport("??0LayoutElement@UI@@QEAA@XZ",
                          (void*)&RtUi_LayoutElement_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d32e0ull, (void*)&RtUi_LayoutElement_Ctor);

    RegisterRuntimeExport("??1LayoutElement@UI@@UEAA@XZ",
                          (void*)&RtUi_LayoutElement_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d32a0ull, (void*)&RtUi_LayoutElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@LayoutElement@UI@@UEAAXXZ",
                          (void*)&RtUi_LayoutElement_ComputeSize);     ++n;
    RegisterRuntimeAddr(0x1800d3100ull, (void*)&RtUi_LayoutElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@LayoutElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_LayoutElement_ComputeLayout);   ++n;
    RegisterRuntimeAddr(0x1800d2af0ull, (void*)&RtUi_LayoutElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@LayoutElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_LayoutElement_Draw);            ++n;
    RegisterRuntimeAddr(0x1800d2a40ull, (void*)&RtUi_LayoutElement_Draw);

    return n;
}
