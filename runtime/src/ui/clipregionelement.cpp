// ============================================================================
// runtime/src/ui/clipregionelement.cpp — UI::ClipRegionElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7ClipRegionElement@UI@@6B@` (retail
// 0x1801562f0). The scrolling viewport: a fixed-size window onto one child that
// may be larger than it, with a scissor rectangle around the drawing and a
// scroll offset subtracted from the child's layout.
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x180038370  deleting_dtor.cpp row
//    1    TypeName              0x180037f70  here
//    2    Type                  0x180037f50  here
//    4    ComputeSize           0x1800cbc60  here
//    5    ComputeLayout         0x1800cba00  here
//    6    Draw                  0x1800cb900  here
//    7    Update                0x1800cbd70  here
//    9    Contains              0x1800cbcc0  here
//
// plus ??0ClipRegionElement@UI@@QEAA@XZ         0x1800cbe60
//      ??0ClipRegionElement@UI@@QEAA@AEBV01@@Z  0x180038220
//      ??1ClipRegionElement@UI@@UEAA@XZ         0x1800cbe30
//
// LAYOUT — 0xd8 bytes (`MOV EDX,0xd8` at 0x1800383a2), six dwords:
//
//   +0xc0 int32  requested width   (constructor default 100)
//   +0xc4 int32  requested height  (constructor default 100)
//   +0xc8 int32  the CHILD's padded width   — an OUTPUT of ComputeSize
//   +0xcc int32  the CHILD's padded height  — an OUTPUT of ComputeSize
//   +0xd0 int32  scroll offset X  (subtracted from the child's left)
//   +0xd4 int32  scroll offset Y
//
// +0xc8/+0xcc are what `UI::ContainerElement` reads back to size its scroll
// bars (runtime/src/ui/containerelement.cpp), which is the proof of what they
// hold; +0xd0/+0xd4 are what `?SetOffsets@ContainerElement@UI@@` (0x1800cc1e0,
// already ported in element.cpp) writes.
// ============================================================================
#include "element_common.h"

using namespace rtuielem;

namespace {

constexpr int kReqWidth   = 0xc0;
constexpr int kReqHeight  = 0xc4;
constexpr int kInnerWidth = 0xc8;
constexpr int kInnerHeight = 0xcc;
constexpr int kOffsetX    = 0xd0;
constexpr int kOffsetY    = 0xd4;

// UI::UserInterface's current clip rectangle, four dwords at +0xfc..+0x108.
// Proven by this class: Update and Contains save it, overwrite it with their
// own layout rect, run the base body, and put the old one back.
constexpr int kUiClipLeft   = 0xfc;
constexpr int kUiClipTop    = 0x100;
constexpr int kUiClipRight  = 0x104;
constexpr int kUiClipBottom = 0x108;

RTELEM_TYPE_SLOTS(ClipRegionElement)

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);
using ElemUpdateFn      = void  (*)(void* self, const void* time, void* ui,
                                    void** dlg, const void* state);
using ElemContainsFn    = bool  (*)(const void* self, int x, int y, void* ui);
using PaddedSizeFn      = void  (*)(void* self, int* w, int* h);
// UI::SpriteContext::SetScissor(ctx, Viewport& out, const Viewport& in) —
// returns the PREVIOUS scissor by value through the hidden slot in RDX.
using SetScissorFn      = void* (*)(void* ctx, void* outOld, const void* in);

struct Viewport { int32_t x, y, w, h; };
static_assert(sizeof(Viewport) == 16, "SetScissor's value type is four dwords");

}  // namespace

RTELEM_TYPE_BODIES(ClipRegionElement)

// ============================================================================
// ?ComputeSize@ClipRegionElement@UI@@UEAAXXZ   0x1800cbc60
// ============================================================================
// The viewport is whatever size it was told to be; the CHILD's padded size is
// measured into +0xc8/+0xcc for the scroll bars to read.
//
// `_children[0]` is dereferenced with NO null test (0x1800cbc7a loads it and
// passes it straight to GetPaddedSize). A ClipRegionElement always has its one
// child, built by ContainerDescription::CreateInstance.
extern "C" void RtUi_ClipRegionElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    I32(self, kElemLayoutWidth)  = I32(self, kReqWidth);
    I32(self, kElemLayoutHeight) = I32(self, kReqHeight);

    RtAddr<PaddedSizeFn>(0x180115f10ull)(
        Child(self, 0),
        reinterpret_cast<int*>(self + kInnerWidth),
        reinterpret_cast<int*>(self + kInnerHeight));

    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@ClipRegionElement@UI@@UEAAXHHHH@Z   0x1800cba00
// ============================================================================
// The standard aligned rect for the viewport, then the child laid out at its
// FULL padded size, shifted by the scroll offset. The child overhangs the
// viewport by design — Draw's scissor is what hides the overhang.
//
// The child pointer is loaded TWICE: once for the visibility test and the size
// measurement, and again for the ComputeLayout call (0x1800cc7a8 re-reads
// `_children[0]`). Both are transcribed.
extern "C" void RtUi_ClipRegionElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    ElementComputeAlignedRect(self, x, y, w, h);

    void* child = Child(self, 0);
    if (child == nullptr) return;
    if ((U32(child, kElemFlags) & kFlagHidden) != 0) return;

    int cw = 0, ch = 0;
    PaddedSizeInline(child, &cw, &ch);

    void* again = Child(self, 0);
    const int cx = I32(self, kElemLayoutLeft) - I32(self, kOffsetX);
    const int cy = I32(self, kElemLayoutTop)  - I32(self, kOffsetY);
    VSlot<ComputeLayoutFn>(again, kSlotComputeLayout)(again, cx, cy, cx + cw, cy + ch);
}

// ============================================================================
// ?Draw@ClipRegionElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800cb900
// ============================================================================
// Push the viewport as the sprite scissor, draw every visible child, restore.
//
// *** THE SCISSOR IS A {x, y, WIDTH, HEIGHT} RECTANGLE, NOT {left, top, right,
// bottom}, AND THE EXTENTS ARE INCLUSIVE. *** Retail builds it as
// `{left, top, right - left + 1, bottom - top + 1}` (0x1800cb943..0x1800cb96a).
// Dropping the `+ 1` loses the last row and column of every scrolled panel.
//
// The two SetScissor calls SWAP their two buffers: the first writes the old
// viewport into scratch and takes the new one from `rect`; the second writes
// into `rect` (now dead) and takes the saved one back. Reproduced with the same
// two buffers rather than one, because SetScissor writes through its out slot.
extern "C" void RtUi_ClipRegionElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    alpha = DimAlpha(self, alpha);

    Viewport rect;
    rect.x = I32c(self, kElemLayoutLeft);
    rect.y = I32c(self, kElemLayoutTop);
    rect.w = (I32c(self, kElemLayoutRight)  - rect.x) + 1;
    rect.h = (I32c(self, kElemLayoutBottom) - rect.y) + 1;

    Viewport saved;
    const auto setScissor = RtAddr<SetScissorFn>(0x18011d050ull);
    setScissor(ctx, &saved, &rect);

    const int32_t count = I32c(self, kElemChildCount);
    for (int32_t i = 0; i < count; ++i) {
        void* child = Child(self, i);
        if (child == nullptr) continue;
        if ((U32(child, kElemFlags) & kFlagHidden) != 0) continue;
        VSlot<DrawFn>(child, kSlotDraw)(child, ctx, alpha);
    }

    setScissor(ctx, &rect, &saved);
}

// ============================================================================
// ?Update@ClipRegionElement@UI@@   0x1800cbd70
// ?Contains@ClipRegionElement@UI@@UEBA_NHHPEAVUserInterface@2@@Z  0x1800cbcc0
// ============================================================================
// The same idiom twice: install THIS element's layout rect as the
// UserInterface's clip rectangle, run the base body (whose hit testing consults
// it), then put the caller's rectangle back. Without this, an element scrolled
// out of a panel would still take the mouse.
//
// The save reads the four dwords BEFORE any of them is overwritten; a
// field-by-field save/overwrite would corrupt the second and later reads.
extern "C" void RtUi_ClipRegionElement_Update(void* selfv, const void* time, void* ui,
                                              void** dlg, const void* state) {
    uint8_t* self = static_cast<uint8_t*>(selfv);

    const int32_t l = I32(self, kElemLayoutLeft);
    const int32_t t = I32(self, kElemLayoutTop);
    const int32_t r = I32(self, kElemLayoutRight);
    const int32_t b = I32(self, kElemLayoutBottom);

    const int32_t sl = I32(ui, kUiClipLeft);
    const int32_t st_ = I32(ui, kUiClipTop);
    const int32_t sr = I32(ui, kUiClipRight);
    const int32_t sb = I32(ui, kUiClipBottom);

    I32(ui, kUiClipLeft)   = l;
    I32(ui, kUiClipTop)    = t;
    I32(ui, kUiClipRight)  = r;
    I32(ui, kUiClipBottom) = b;

    void* dlgCopy = *dlg;
    RtAddr<ElemUpdateFn>(0x180116270ull)(self, time, ui, &dlgCopy, state);

    I32(ui, kUiClipLeft)   = sl;
    I32(ui, kUiClipTop)    = st_;
    I32(ui, kUiClipRight)  = sr;
    I32(ui, kUiClipBottom) = sb;
}

extern "C" bool RtUi_ClipRegionElement_Contains(const void* selfv, int x, int y, void* ui) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    const int32_t l = I32c(self, kElemLayoutLeft);
    const int32_t t = I32c(self, kElemLayoutTop);
    const int32_t r = I32c(self, kElemLayoutRight);
    const int32_t b = I32c(self, kElemLayoutBottom);

    const int32_t sl = I32(ui, kUiClipLeft);
    const int32_t st_ = I32(ui, kUiClipTop);
    const int32_t sr = I32(ui, kUiClipRight);
    const int32_t sb = I32(ui, kUiClipBottom);

    I32(ui, kUiClipLeft)   = l;
    I32(ui, kUiClipTop)    = t;
    I32(ui, kUiClipRight)  = r;
    I32(ui, kUiClipBottom) = b;

    const bool inside = RtAddr<ElemContainsFn>(0x1801169e0ull)(self, x, y, ui);

    I32(ui, kUiClipLeft)   = sl;
    I32(ui, kUiClipTop)    = st_;
    I32(ui, kUiClipRight)  = sr;
    I32(ui, kUiClipBottom) = sb;
    return inside;
}

// ============================================================================
// Constructors / destructor
// ============================================================================
// The default constructor writes +0xc4 and +0xc8 as ONE QWORD of 100
// (`MOV qword [RCX+0xc4],0x64`), so +0xc8 is set to 0 by the upper half — the
// inner width starts at zero, not at 100. Transcribed as the two fields it
// covers rather than as the qword, which is byte-identical.
extern "C" void* RtUi_ClipRegionElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtClipRegionElement();
    I32(self, kReqWidth)   = 100;
    I32(self, kReqHeight)  = 100;
    I32(self, kInnerWidth) = 0;
    I32(self, kInnerHeight) = 0;
    I32(self, kOffsetX)    = 0;
    I32(self, kOffsetY)    = 0;
    return self;
}

extern "C" void* RtUi_ClipRegionElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtClipRegionElement();
    for (int off = kReqWidth; off <= kOffsetY; off += 4)
        I32(self, off) = I32c(other, off);
    return self;
}

extern "C" void RtUi_ClipRegionElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtClipRegionElement();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiClipRegionElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@ClipRegionElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ClipRegionElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180037f70ull, (void*)&RtUi_ClipRegionElement_TypeName);

    RegisterRuntimeExport("?Type@ClipRegionElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ClipRegionElement_Type);          ++n;
    RegisterRuntimeAddr(0x180037f50ull, (void*)&RtUi_ClipRegionElement_Type);

    RegisterRuntimeExport("?ComputeSize@ClipRegionElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ClipRegionElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800cbc60ull, (void*)&RtUi_ClipRegionElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ClipRegionElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ClipRegionElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800cba00ull, (void*)&RtUi_ClipRegionElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ClipRegionElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ClipRegionElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800cb900ull, (void*)&RtUi_ClipRegionElement_Draw);

    RegisterRuntimeExport(
        "?Update@ClipRegionElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_ClipRegionElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1800cbd70ull, (void*)&RtUi_ClipRegionElement_Update);

    RegisterRuntimeExport("?Contains@ClipRegionElement@UI@@UEBA_NHHPEAVUserInterface@2@@Z",
                          (void*)&RtUi_ClipRegionElement_Contains);      ++n;
    RegisterRuntimeAddr(0x1800cbcc0ull, (void*)&RtUi_ClipRegionElement_Contains);

    RegisterRuntimeExport("??0ClipRegionElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ClipRegionElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1800cbe60ull, (void*)&RtUi_ClipRegionElement_Ctor);

    RegisterRuntimeExport("??0ClipRegionElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_ClipRegionElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180038220ull, (void*)&RtUi_ClipRegionElement_CopyCtor);

    RegisterRuntimeExport("??1ClipRegionElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ClipRegionElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800cbe30ull, (void*)&RtUi_ClipRegionElement_Dtor);
    return n;
}
