// ============================================================================
// runtime/src/ui/listelement.cpp — UI::ListElement, the whole class
// ============================================================================
//
// `??_7ListElement@UI@@6B@` (retail 0x180156dc0):
//
//   0  vector deleting dtor  0x180045320  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180044f90  here
//   2  Type                  0x180044f70  here
//   4  ComputeSize           0x1800d4380  here
//   5  ComputeLayout         0x1800d4210  here
//   6  Draw                  0x1800d3eb0  here
//   7  Update                0x1800d3c20  here
//
// plus ??0ListElement (0x1800d4570) and ??1ListElement (0x1800d4530).
//
// NOT HERE: `?DrawHighlight@ListElement@UI@@QEBAXAEAVSpriteContext@2@HI@Z`
// (0x1800d3f70), `?AddItem@` (0x1800d44b0) and `?GetCellLayout@` (0x1800d4120)
// — the UI non-slot callee list is another agent's. Draw REACHES DrawHighlight
// by address.
//
// ---------------------------------------------------------------------------
// LAYOUT.  sizeof(ListElement) == 0x100 (the deleting destructor's
// `MOV EDX,0x100` at 0x180045352). Fields, each from the ctor (0x1800d4570) or
// from the body that reads it:
//
//   +0xC0  _itemRows      Array-ish head, ctor zeroes the qword
//   +0xC8  _rowCount      int32, ctor -1
//   +0xD0  _hoverColor    uint32, ctor 0xff808080   } one qword store,
//   +0xD4  _selectColor   uint32, ctor 0xffffffff   } 0xffffffffff808080
//   +0xD8  _selectedIndex int32, ctor -1            } one qword store
//   +0xDC  _hoverIndex    int32, ctor -1            }
//   +0xE0  _items         Array<...>, the ONE member ~ListElement destroys
//   +0xF0  _grid          UI::RibbonElement*  (Update reads its +0xC0 vertical
//                         byte, +0xC4 spacing and +0xC8 int array — exactly
//                         RibbonElement's own three fields, which is how the
//                         type is known and not guessed)
//   +0xF8  _clip          UI::ClipRegionElement*, the visible window
//
// Update's own two children are _children[0] (drawn first, the backdrop) and
// _children[1] (the content, and the one ComputeSize measures).
// ============================================================================

#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kHoverColor    = 0xD0;
constexpr int kSelectColor   = 0xD4;
constexpr int kSelectedIndex = 0xD8;
constexpr int kHoverIndex    = 0xDC;
constexpr int kItems         = 0xE0;
constexpr int kGrid          = 0xF0;
constexpr int kClip          = 0xF8;

// The RibbonElement-shaped fields Update reads through _grid.
constexpr int kGridVertical = 0xC0;   // byte
constexpr int kGridSpacing  = 0xC4;   // int32
constexpr int kGridExtents  = 0xC8;   // Array<int32>::_data

// UpdateState (the 5th argument), offsets named by the bodies that read them.
constexpr int kUsPressed = 0x01;      // byte: a click began this frame
constexpr int kUsMouseX  = 0x18;
constexpr int kUsMouseY  = 0x1c;

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ListElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7ListElement@UI@@6B@"); }

// `?DrawHighlight@ListElement@UI@@QEBAXAEAVSpriteContext@2@HI@Z` 0x1800d3f70
using DrawHighlightFn = void (*)(const void* self, void* ctx, int32_t index, uint32_t color);

// `?OnElementEvent@DialogInstance@UI@@` — the DialogInstance vtable's slot 8
// (+0x40). Seven arguments after `this`, read straight off 0x1800d3e2a..3e5e:
// RDX = &InternalPtr<Element>, R8D = event kind (0x14), R9D = the element id,
// [RSP+0x20] = the row index, and three zeroed dwords/qwords.
using OnElementEventFn = void (*)(void* dlg, void** elem, int32_t kind, int32_t elemId,
                                  int32_t a, int32_t b, int32_t c, int64_t d);

}  // namespace

extern "C" uint32_t RtUi_ListElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ListElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0ListElement@UI@@QEAA@XZ   0x1800d4570
extern "C" void RtUi_ListElement_Ctor(void* self) {
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    PTR(self, 0xC0) = nullptr;
    I32(self, 0xC8) = -1;
    U32(self, kHoverColor)    = 0xff808080u;   // the low half of the qword
    U32(self, kSelectColor)   = 0xffffffffu;
    I32(self, kSelectedIndex) = -1;
    I32(self, kHoverIndex)    = -1;
    PTR(self, kItems)     = nullptr;
    PTR(self, kItems + 8) = nullptr;
    PTR(self, kGrid) = nullptr;
    PTR(self, kClip) = nullptr;
}

// ??1ListElement@UI@@UEAA@XZ   0x1800d4530
extern "C" void RtUi_ListElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    RtAddr<ArrayDtorFn>(0x180024aa0ull)(B(self, kItems));   // already ours
    ElementDtor(self);
}

// slot 4 — 0x1800d4380
extern "C" void RtUi_ListElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;
    void* content = reinterpret_cast<void**>(PTR(self, kElChildren))[1];
    RtAddr<GetPaddedSizeFn>(0x180115f10ull)(
        content,
        reinterpret_cast<int32_t*>(B(self, kElLayoutWidth)),
        reinterpret_cast<int32_t*>(B(self, kElLayoutHeight)));
    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
}

// slot 5 — 0x1800d4210
extern "C" void RtUi_ListElement_ComputeLayout(void* self, int32_t x, int32_t y,
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

    switch (align) {
        case 2: case 5: case 8:
            px = (right - I32(self, kElRightPad)) - aw;
            break;
        default:
            break;
    }
    if (align > 2 && align > 5 && align < 9)
        py = (bottom - I32(self, kElBottomPad)) - ah;

    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutRight)  = aw + px;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutBottom) = ah + py;

    int32_t n = I32(self, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[i];
        if (child != nullptr && (U32(child, kElFlags) & kFlagHidden) == 0) {
            reinterpret_cast<ComputeLayoutSlotFn>(VTable(child)[kVtComputeLayout / 8])(
                child, I32(self, kElLayoutLeft), I32(self, kElLayoutTop),
                I32(self, kElLayoutRight), I32(self, kElLayoutBottom));
        }
        n = I32(self, kElChildCount);
    }
}

// slot 6 — 0x1800d3eb0
extern "C" void RtUi_ListElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    void* back = reinterpret_cast<void**>(PTR(me, kElChildren))[0];
    if (back != nullptr && (U32(back, kElFlags) & kFlagHidden) == 0)
        reinterpret_cast<DrawSlotFn>(VTable(back)[kVtDraw / 8])(back, ctx, alpha);

    RtAddr<DrawHighlightFn>(0x1800d3f70ull)(me, ctx, I32(me, kHoverIndex),
                                            U32(me, kHoverColor));
    RtAddr<DrawHighlightFn>(0x1800d3f70ull)(me, ctx, I32(me, kSelectedIndex),
                                            U32(me, kSelectColor));

    void* content = reinterpret_cast<void**>(PTR(me, kElChildren))[1];
    if (content != nullptr && (U32(content, kElFlags) & kFlagHidden) == 0)
        reinterpret_cast<DrawSlotFn>(VTable(content)[kVtDraw / 8])(content, ctx, alpha);
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@ListElement@UI@@ ...   0x1800d3c20
//
// ONE stack slot holds two different things in sequence, and that is retail's
// own reuse, not a simplification: [RSP+0x70] first holds the caller's
// InternalPtr<DialogInstance> copy that Element::Update is handed (0x1800d3c40),
// and later `this` as an InternalPtr<Element> for the event (0x1800d3e37).
// ---------------------------------------------------------------------------
extern "C" void RtUi_ListElement_Update(void* self, const void* time, void* ui,
                                        void** dlgPtr, const void* state) {
    void* slot = *dlgPtr;                                 // 1800d3c2a / 3c40
    ElementUpdate(self, time, ui, &slot, state);          // 1800d3c4a

    I32(self, kHoverIndex) = -1;                          // 1800d3c56
    if ((U32(self, kElFlags) & kFlagInteractive) == 0) return;   // TEST ...,0x20000

    const int32_t mouseX = I32c(state, kUsMouseX);
    const int32_t mouseY = I32c(state, kUsMouseY);

    for (int32_t i = 0; i < I32(PTR(self, kGrid), kElChildCount); ++i) {
        if (i < 0) continue;                              // TEST EBX,EBX / JS

        void* grid = PTR(self, kGrid);

        // The running offset of row i: the sum of the previous rows' extents
        // plus one spacing each. Retail unrolls this by four; the rolled form
        // computes the same sum in the same order.
        int32_t off = 0;
        for (int32_t k = 0; k < i; ++k)
            off += reinterpret_cast<int32_t*>(PTR(grid, kGridExtents))[k]
                   + I32(grid, kGridSpacing);

        int32_t x0 = I32(grid, kElLayoutLeft);
        int32_t y0 = I32(grid, kElLayoutTop);
        int32_t x1, y1;
        if (U8(grid, kGridVertical) == 0) {
            y1 = I32(grid, kElLayoutBottom);
            x0 = x0 + off;
            x1 = reinterpret_cast<int32_t*>(PTR(grid, kGridExtents))[i] + x0;
        } else {
            x1 = I32(grid, kElLayoutRight);
            y0 = y0 + off;
            y1 = reinterpret_cast<int32_t*>(PTR(grid, kGridExtents))[i] + y0;
        }

        void* clip = PTR(self, kClip);
        int32_t cl = I32(clip, kElLayoutLeft);
        int32_t cr = I32(clip, kElLayoutRight);
        int32_t ct = I32(clip, kElLayoutTop);
        int32_t cb = I32(clip, kElLayoutBottom);

        if (cl <= x1 && x0 <= cr && ct <= y1 && y0 <= cb) {
            if (cb < y1) y1 = cb + 1;
            if (y0 < ct) y0 = ct;
            if (cr < x1) x1 = cr + 1;
            if (x0 < cl) x0 = cl;
            if (U8(grid, kGridVertical) != 0) {
                // A vertical list hit-tests the full clip width and the
                // clamped row band; a horizontal one the clamped row band and
                // the full clip height. Retail expresses that by overwriting
                // four registers here rather than by two separate tests.
                x1 = cr;
                cb = y1;
                ct = y0;
                x0 = cl;
            }
            if (x0 <= mouseX && mouseX <= x1 && ct <= mouseY && mouseY <= cb) {
                I32(self, kHoverIndex) = i;
                if (U8c(state, kUsPressed) != 0) {
                    I32(self, kSelectedIndex) = i;
                    slot = self;                                   // 1800d3e37
                    void* dlg = *dlgPtr;                           // 1800d3e2a
                    reinterpret_cast<OnElementEventFn>(VTable(dlg)[0x40 / 8])(
                        dlg, &slot, 0x14, I32(self, kElElementId), i, 0, 0, 0);
                }
            }
        }
    }
}

// ============================================================================
int RtUiListElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ListElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ListElement_Type);            ++n;
    RegisterRuntimeAddr(0x180044f70ull, (void*)&RtUi_ListElement_Type);

    RegisterRuntimeExport("?TypeName@ListElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ListElement_TypeName);        ++n;
    RegisterRuntimeAddr(0x180044f90ull, (void*)&RtUi_ListElement_TypeName);

    RegisterRuntimeExport("??0ListElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ListElement_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d4570ull, (void*)&RtUi_ListElement_Ctor);

    RegisterRuntimeExport("??1ListElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ListElement_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d4530ull, (void*)&RtUi_ListElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@ListElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ListElement_ComputeSize);     ++n;
    RegisterRuntimeAddr(0x1800d4380ull, (void*)&RtUi_ListElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ListElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ListElement_ComputeLayout);   ++n;
    RegisterRuntimeAddr(0x1800d4210ull, (void*)&RtUi_ListElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ListElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ListElement_Draw);            ++n;
    RegisterRuntimeAddr(0x1800d3eb0ull, (void*)&RtUi_ListElement_Draw);

    RegisterRuntimeExport(
        "?Update@ListElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_ListElement_Update);                            ++n;
    RegisterRuntimeAddr(0x1800d3c20ull, (void*)&RtUi_ListElement_Update);

    return n;
}
