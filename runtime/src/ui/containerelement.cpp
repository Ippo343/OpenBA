// ============================================================================
// runtime/src/ui/containerelement.cpp — UI::ContainerElement, the four
// behavioural slots
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7ContainerElement@UI@@6B@` (retail
// 0x1801564b0). The scroll panel: exactly three children — `_children[0]` a
// `UI::ClipRegionElement`, `_children[1]` and `_children[2]` two
// `UI::ScrollElement`s — and the whole class is the arithmetic that keeps them
// consistent.
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x18003a050  deleting_dtor.cpp row
//    1    TypeName              0x180039cc0  here
//    2    Type                  0x180039ca0  here
//    4    ComputeSize           0x1800cc850  here
//    5    ComputeLayout         0x1800cc620  here
//    6    Draw                  0x1800cc590  here
//    7    Update                0x1800cc2d0  here
//
// plus ??0ContainerElement@UI@@QEAA@XZ         0x1800ccb40
//      ??0ContainerElement@UI@@QEAA@AEBV01@@Z  0x180039f00
//      ??1ContainerElement@UI@@UEAA@XZ         0x1800ccb10
//
// `?SetOffsets@ContainerElement@UI@@QEAAXHH@Z` (0x1800cc1e0) is ALREADY PORTED
// in runtime/src/ui/element.cpp (Wave 7 lane A) and is deliberately not
// repeated here — one address, one owner.
//
// LAYOUT — 0xd8 bytes (`MOV EDX,0xd8` at 0x18003a082):
//
//   +0xc0 int32  requested width   — NOT initialised by either constructor
//   +0xc4 int32  requested height  — NOT initialised by either constructor
//   +0xc8 int32  scroll-bar thickness (the ctor zeroes it)
//   +0xcc uint8  clamp width to the content
//   +0xcd uint8  clamp height to the content
//   +0xd0 int32  pending mouse-wheel impulse, in 7-tick units
//
// The +0xc0/+0xc4 pair being uninitialised is not an oversight to work around:
// `UI::ContainerDescription::CreateInstance` writes them before anything reads
// them. Do not byte-compare a freshly constructed ContainerElement there.
//
// TWO TYPE IDS ARE READ AS GLOBALS, both already named by element.cpp's
// `SetOffsets` port: `?_typeInfo@ClipRegionElement@UI@@` (retail 0x1801f9d68)
// and `?_typeInfo@ScrollElement@UI@@` (retail 0x1801fad98). They reach
// `Object::Base::IsDerivedFrom` (0x1800c0390), and a FAILED test yields a NULL
// that retail then dereferences — see ComputeSize.
// ============================================================================
#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kReqWidth   = 0xc0;
constexpr int kReqHeight  = 0xc4;
constexpr int kBarSize    = 0xc8;
constexpr int kClampW     = 0xcc;   // uint8
constexpr int kClampH     = 0xcd;   // uint8
constexpr int kWheelAccum = 0xd0;

// UI::ClipRegionElement's four (see clipregionelement.cpp for the derivation).
constexpr int kClipReqW   = 0xc0;
constexpr int kClipReqH   = 0xc4;
constexpr int kClipInnerW = 0xc8;
constexpr int kClipInnerH = 0xcc;
constexpr int kClipOffX   = 0xd0;
constexpr int kClipOffY   = 0xd4;

// UI::ScrollElement's three floats, named by what this class writes into them.
constexpr int kScrollPos      = 0x160;   // float, current scroll position
constexpr int kScrollFraction = 0x164;   // float, visible fraction, clamped to 1
constexpr int kScrollRange    = 0x168;   // float, scrollable amount, clamped to 0

// UI::Element::UpdateState
constexpr int kStateWheel = 0x14;   // int32

RTUI_TYPE_SLOTS(ContainerElement)

uint32_t ClipRegionTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}
uint32_t ScrollTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}

using IsDerivedFromFn   = bool  (*)(const void* self, uint32_t typeId);
using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);
using ElemUpdateFn      = void  (*)(void* self, const void* time, void* ui,
                                    void** dlg, const void* state);

void* AsClipRegion(void* p) {
    return RtAddr<IsDerivedFromFn>(0x1800c0390ull)(p, ClipRegionTypeId()) ? p : nullptr;
}
void* AsScroll(void* p) {
    return RtAddr<IsDerivedFromFn>(0x1800c0390ull)(p, ScrollTypeId()) ? p : nullptr;
}

// The screen-size override half of the inlined GetPaddedSize. Retail's copies
// in this file are ONE-AXIS or TWO-AXIS depending on the child, so the walk is
// factored out and the two conversions are done at the call site.
const uint8_t* ScaleOwnerOf(void* e) {
    void* root = e;
    while (P(root, kElemParent) != nullptr) root = P(root, kElemParent);
    return static_cast<const uint8_t*>(Pc(P(root, kElemDialogParent), 0x10));
}

}  // namespace

RTUI_TYPE_BODIES(ContainerElement)

// ============================================================================
// ?ComputeSize@ContainerElement@UI@@UEAAXXZ   0x1800cc850
// ============================================================================
// Measure the clip region and the two scroll bars, then decide both the
// container's own size and the clip region's requested size.
//
// *** `_children[0]` IS DEREFERENCED THROUGH A POINTER THAT IS NULL WHEN THE
// TYPE TEST FAILS. *** `IsDerivedFrom` returns false -> `CMOVNZ` leaves the
// register at zero -> the very next instruction is `CALL [RAX+0x20]` on it
// (0x1800cc8a7). Retail relies on `_children[0]` always being a
// ClipRegionElement; a guard here would skip the sizing rather than restore it.
//
// *** THE HEIGHT USES THE SECOND SCROLL BAR'S *WIDTH*. *** At 0x1800ccab0
// retail adds `R13D` — the padded WIDTH of `_children[2]` — to the container's
// height, while the clip region's height (0x1800cca7d) subtracts `R12D`, its
// padded HEIGHT. The two are different registers and neither is reloaded. This
// is retail's arithmetic, not a transcription slip; for a square scroll bar
// the two are equal, which is why it has never shown.
//
// The clip region's SNAPSHOT size (its +0xc8/+0xcc as they were when this body
// started) is what the clamps compare against — not the container's freshly
// written +0x60/+0x64, which are the same values only until the bars adjust.
extern "C" void RtUi_ContainerElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    void* clip = AsClipRegion(Child(self, 0));
    VSlot<ComputeSizeFn>(clip, kSlotComputeSize)(clip);      // faithful: may be null

    const int32_t snapW = I32(clip, kClipInnerW);
    const int32_t snapH = I32(clip, kClipInnerH);
    I32(self, kElemLayoutWidth)  = snapW;
    I32(self, kElemLayoutHeight) = snapH;

    // ---- _children[1]: WIDTH ONLY (retail measures no height for it) ------
    int barAW = 0;      // its padded width
    int barAGap = 0;    // this->_barSize, but only when the bar exists
    void* barA = Child(self, 1);
    if (barA != nullptr) {
        VSlot<ComputeSizeFn>(barA, kSlotComputeSize)(barA);
        int w = I32(barA, kElemLayoutWidth) + I32(barA, kElemRightPad) + I32(barA, kElemLeftPad);
        if (w < I32(barA, kElemMinWidth)) w = I32(barA, kElemMinWidth);
        const uint32_t f = U32(barA, kElemFlags);
        if ((f & (kFlagScreenH | kFlagScreenW)) != 0) {
            const uint8_t* owner = ScaleOwnerOf(barA);
            if (f & kFlagScreenW) {
                const uint8_t* dev = static_cast<const uint8_t*>(VideoDeviceSingleton());
                w = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x04)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
            }
        }
        barAW = w;
        barAGap = I32(self, kBarSize);
    }

    // ---- _children[2]: BOTH AXES ------------------------------------------
    int barBW = 0, barBH = 0, barBGap = 0;
    void* barB = Child(self, 2);
    if (barB != nullptr) {
        VSlot<ComputeSizeFn>(barB, kSlotComputeSize)(barB);
        int w = I32(barB, kElemLayoutWidth)  + I32(barB, kElemRightPad)  + I32(barB, kElemLeftPad);
        int h = I32(barB, kElemLayoutHeight) + I32(barB, kElemBottomPad) + I32(barB, kElemTopPad);
        if (w < I32(barB, kElemMinWidth))  w = I32(barB, kElemMinWidth);
        if (h < I32(barB, kElemMinHeight)) h = I32(barB, kElemMinHeight);
        const uint32_t f = U32(barB, kElemFlags);
        if ((f & (kFlagScreenH | kFlagScreenW)) != 0) {
            const uint8_t* owner = ScaleOwnerOf(barB);
            const uint8_t* dev = static_cast<const uint8_t*>(VideoDeviceSingleton());
            if (f & kFlagScreenW)
                w = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x04)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
            if (f & kFlagScreenH)
                h = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x08)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
        }
        barBW = w;
        barBH = h;
        barBGap = I32(self, kBarSize);
    }

    const int32_t reqH = I32(self, kReqHeight);
    int32_t clampedH = reqH;
    if (self[kClampH] != 0 && reqH > 0 && reqH > snapH) clampedH = snapH;

    const int32_t reqW = I32(self, kReqWidth);
    int32_t clampedW = reqW;
    if (self[kClampW] != 0 && reqW > 0 && reqW > snapW) clampedW = snapW;

    const int32_t clipW = (reqW > 0) ? (clampedW - barAGap - barAW)
                                     : I32(self, kElemLayoutWidth);
    const int32_t clipH = (reqH > 0) ? (clampedH - barBGap - barBH)
                                     : I32(self, kElemLayoutHeight);

    I32(self, kElemLayoutWidth)  = (reqW > 0) ? clampedW
                                  : I32(self, kElemLayoutWidth)  + barAGap + barAW;
    I32(self, kElemLayoutHeight) = (reqH > 0) ? clampedH
                                  : I32(self, kElemLayoutHeight) + barBGap + barBW;

    if (I32(clip, kClipReqW) != clipW || I32(clip, kClipReqH) != clipH) {
        I32(clip, kClipReqW) = clipW;
        I32(clip, kClipReqH) = clipH;
        SetNeedsSizeCalc(clip);
    }

    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@ContainerElement@UI@@UEAAXHHHH@Z   0x1800cc620
// ============================================================================
// The standard aligned rect, then the three children placed around it: the clip
// region at the top-left corner at its own requested size, `_children[1]` in the
// right-hand gutter and `_children[2]` in the bottom one.
//
// The clip region is fetched TWICE — once through the type test (which is what
// supplies `+0xc0`/`+0xc4` to the other two placements) and once raw for its
// own ComputeLayout call (0x1800cc765). Both loads are transcribed; they are
// the same object unless `_children[0]` is not a ClipRegionElement, in which
// case retail lays out the raw child and reads the offsets off a null.
extern "C" void RtUi_ContainerElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    ElementComputeAlignedRect(self, x, y, w, h);

    void* clip = AsClipRegion(Child(self, 0));

    void* raw = Child(self, 0);
    const int l = I32(self, kElemLayoutLeft);
    const int t = I32(self, kElemLayoutTop);
    VSlot<ComputeLayoutFn>(raw, kSlotComputeLayout)(
        raw, l, t, I32(clip, kClipReqW) + l, I32(clip, kClipReqH) + t);

    void* barA = Child(self, 1);
    if (barA != nullptr) {
        VSlot<ComputeLayoutFn>(barA, kSlotComputeLayout)(
            barA,
            I32(self, kBarSize) + I32(self, kElemLayoutLeft) + I32(clip, kClipReqW),
            I32(self, kElemLayoutTop),
            I32(self, kElemLayoutRight),
            I32(clip, kClipReqH) + I32(self, kElemLayoutTop));
    }

    void* barB = Child(self, 2);
    if (barB != nullptr) {
        VSlot<ComputeLayoutFn>(barB, kSlotComputeLayout)(
            barB,
            I32(self, kElemLayoutLeft),
            I32(self, kBarSize) + I32(clip, kClipReqH) + I32(self, kElemLayoutTop),
            I32(clip, kClipReqW) + I32(self, kElemLayoutLeft),
            I32(self, kElemLayoutBottom));
    }
}

// ============================================================================
// ?Draw@ContainerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800cc590
// ============================================================================
// EXACTLY THREE children, by a hard-coded `while (offset < 0x18)` — not by
// `_childCount`. A fourth child of a ContainerElement is never drawn.
// The visibility flag is NOT tested here (unlike GroupElement's loop); only the
// null test is.
extern "C" void RtUi_ContainerElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    alpha = DimAlpha(self, alpha);

    for (int i = 0; i < 3; ++i) {
        void* child = Child(self, i);
        if (child != nullptr) VSlot<DrawFn>(child, kSlotDraw)(child, ctx, alpha);
    }
}

// ============================================================================
// ?Update@ContainerElement@UI@@   0x1800cc2d0
// ============================================================================
// Publish the clip region's scroll geometry into the two scroll bars, apply the
// mouse wheel, read the bars' positions back into the clip region's offsets,
// then run Element::Update.
//
// For each bar: `fraction = visible / content` clamped to 1, and
// `range = content - visible` clamped to 0. `_children[1]` is driven by the
// VERTICAL pair (clip +0xc4 against +0xcc) and `_children[2]` by the HORIZONTAL
// one (clip +0xc0 against +0xc8) — the crossing is retail's, read off the two
// blocks at 0x1800cc361 and 0x1800cc3e9.
//
// THE WHEEL IS AN IMPULSE COUNTER, NOT A POSITION. `_wheelAccum` (+0xd0) takes
// ±7 on a wheel event and is spent one unit per frame, each unit moving the
// bar by `(range / (range / visible)) / 7 * 0.33` — i.e. a third of a page,
// eased over seven frames. The division-by-a-division is what the bytes do
// (0x1800cc4d2 then 0x1800cc4d6); simplifying it to `visible` changes the
// rounding.
extern "C" void RtUi_ContainerElement_Update(void* selfv, const void* time, void* ui,
                                             void** dlg, const void* statev) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* st = static_cast<const uint8_t*>(statev);

    void* clip = AsClipRegion(Child(self, 0));
    VSlot<ComputeSizeFn>(clip, kSlotComputeSize)(clip);      // faithful: may be null

    const int32_t innerW = I32(clip, kClipInnerW);
    const int32_t innerH = I32(clip, kClipInnerH);

    int posB = 0;   // R12D — _children[2]'s position, published into offset X
    int posA = 0;   // R13D — _children[1]'s position, published into offset Y

    if (Child(self, 1) != nullptr) {
        void* bar = AsScroll(Child(self, 1));
        const int32_t visible = I32(clip, kClipReqH);
        posA = static_cast<int>(*reinterpret_cast<const float*>(
                   static_cast<const uint8_t*>(bar) + kScrollPos));
        float frac = static_cast<float>(visible) / static_cast<float>(innerH);
        *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollFraction) = frac;
        if (frac > 1.0f)
            *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollFraction) = 1.0f;
        const float range = static_cast<float>(innerH - I32(clip, kClipReqH));
        *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollRange) = range;
        if (range < 0.0f)
            *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollRange) = 0.0f;
    }

    if (Child(self, 2) != nullptr) {
        void* bar = AsScroll(Child(self, 2));
        posB = static_cast<int>(*reinterpret_cast<const float*>(
                   static_cast<const uint8_t*>(bar) + kScrollPos));
        float frac = static_cast<float>(I32(clip, kClipReqW)) / static_cast<float>(innerW);
        *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollFraction) = frac;
        if (frac > 1.0f)
            *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollFraction) = 1.0f;
        const float range = static_cast<float>(innerW - I32(clip, kClipReqW));
        *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollRange) = range;
        if (range < 0.0f)
            *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollRange) = 0.0f;
    }

    if ((U32(self, kElemFlags) & 0x20000u) != 0) {
        const int32_t wheel = I32c(st, kStateWheel);
        if (wheel != 0) I32(self, kWheelAccum) += (wheel > 0) ? -7 : 7;
    }

    if (I32(self, kWheelAccum) != 0 && Child(self, 1) != nullptr) {
        void* bar = AsScroll(Child(self, 1));
        const float range = static_cast<float>(innerH - I32(clip, kClipReqH));
        if (range > 0.0f) {
            int32_t acc = I32(self, kWheelAccum);
            float step = ((range / (range / static_cast<float>(I32(clip, kClipReqH)))) / 7.0f)
                         * 0.33f;
            if (acc < 0) { step = -step; acc += 1; } else { acc -= 1; }
            I32(self, kWheelAccum) = acc;
            float pos = step + *reinterpret_cast<const float*>(
                            static_cast<const uint8_t*>(bar) + kScrollPos);
            if (pos < 0.0f)        pos = 0.0f;
            else if (pos > range)  pos = range;
            *reinterpret_cast<float*>(static_cast<uint8_t*>(bar) + kScrollPos) = pos;
        }
    }

    I32(clip, kClipOffX) = posB;
    I32(clip, kClipOffY) = posA;

    void* dlgCopy = *dlg;
    RtAddr<ElemUpdateFn>(0x180116270ull)(self, time, ui, &dlgCopy, st);
}

// ============================================================================
// Constructors / destructor
// ============================================================================
// The default constructor writes THREE things: +0xc8 (dword 0), +0xcc as a
// SINGLE WORD (`MOV word [RBX+0xcc],AX` — both bool flags in one store) and
// +0xd0. It does NOT touch +0xc0/+0xc4; see the banner.
extern "C" void* RtUi_ContainerElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtContainerElement();
    I32(self, kBarSize) = 0;
    self[kClampW] = 0;
    self[kClampH] = 0;
    I32(self, kWheelAccum) = 0;
    return self;
}

extern "C" void* RtUi_ContainerElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtContainerElement();
    I32(self, kReqWidth)  = I32c(other, kReqWidth);
    I32(self, kReqHeight) = I32c(other, kReqHeight);
    I32(self, kBarSize)   = I32c(other, kBarSize);
    self[kClampW] = other[kClampW];
    self[kClampH] = other[kClampH];
    I32(self, kWheelAccum) = I32c(other, kWheelAccum);
    return self;
}

extern "C" void RtUi_ContainerElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtContainerElement();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiContainerElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@ContainerElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ContainerElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180039cc0ull, (void*)&RtUi_ContainerElement_TypeName);

    RegisterRuntimeExport("?Type@ContainerElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ContainerElement_Type);          ++n;
    RegisterRuntimeAddr(0x180039ca0ull, (void*)&RtUi_ContainerElement_Type);

    RegisterRuntimeExport("?ComputeSize@ContainerElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ContainerElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800cc850ull, (void*)&RtUi_ContainerElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ContainerElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ContainerElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800cc620ull, (void*)&RtUi_ContainerElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ContainerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ContainerElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800cc590ull, (void*)&RtUi_ContainerElement_Draw);

    RegisterRuntimeExport(
        "?Update@ContainerElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_ContainerElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1800cc2d0ull, (void*)&RtUi_ContainerElement_Update);

    RegisterRuntimeExport("??0ContainerElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ContainerElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1800ccb40ull, (void*)&RtUi_ContainerElement_Ctor);

    RegisterRuntimeExport("??0ContainerElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_ContainerElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180039f00ull, (void*)&RtUi_ContainerElement_CopyCtor);

    RegisterRuntimeExport("??1ContainerElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ContainerElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800ccb10ull, (void*)&RtUi_ContainerElement_Dtor);
    return n;
}
