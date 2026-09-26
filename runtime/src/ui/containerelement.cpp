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
//
// ENDGAME B.3.6 block C4: the layout above moved to runtime/types/ui/
// ContainerElement.h (the real class). Every body below is a MEMBER; each
// registered door is a one-line QUALIFIED delegation. Hand-inlined slot calls
// onto children became virtual member calls; the scroll bars are reached as
// `UI::ScrollElement` (this block's class) after retail's type test, the clip
// region's own fields stay raw offsets (block C1's class, fragment list).
// RULE B6-5: the type-info records, the vtable and the Video::Device singleton
// the members read are cached in namespace-scope std::atomic slots.
// ============================================================================
#include <atomic>

#include "element_common.h"
#include "../../types/ui/ContainerElement.h"
#include "../../types/ui/ScrollElement.h"

namespace UI { class ClipRegionElement; }   // block C1

using namespace rtuielem;

namespace {

// UI::ClipRegionElement's own fields (see clipregionelement.cpp for the
// derivation) — block C1's header owns them; raw here until the fragment pass.
constexpr int kClipReqW   = 0xc0;
constexpr int kClipReqH   = 0xc4;
constexpr int kClipInnerW = 0xc8;
constexpr int kClipInnerH = 0xcc;
constexpr int kClipOffX   = 0xd0;
constexpr int kClipOffY   = 0xd4;

std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_vtable{nullptr};
std::atomic<void*> g_clipTypeInfo{nullptr};
std::atomic<void*> g_scrollTypeInfo{nullptr};
std::atomic<void*> g_deviceSingleton{nullptr};

const uint8_t* ContainerElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@ContainerElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* VtContainerElement() {
    return AtomicExport(g_vtable, "??_7ContainerElement@UI@@6B@");
}
uint32_t ClipRegionTypeId() {
    const uint32_t* p = static_cast<const uint32_t*>(AtomicExport(
        g_clipTypeInfo, "?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A"));
    return p ? *p : 0u;
}
uint32_t ScrollTypeId() {
    const uint32_t* p = static_cast<const uint32_t*>(AtomicExport(
        g_scrollTypeInfo, "?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A"));
    return p ? *p : 0u;
}
const uint8_t* VideoDevice() {
    void** p = static_cast<void**>(AtomicExport(
        g_deviceSingleton, "?_singleton@Device@Video@@0PEAV12@EA"));
    return static_cast<const uint8_t*>(p ? *p : nullptr);
}

using IsDerivedFromFn   = bool  (*)(const void* self, uint32_t typeId);
using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);

// `CMOVNZ`: the child on a passed type test, NULL on a failed one — and retail
// then dereferences the NULL (see ComputeSize).
UI::Element* AsClipRegion(UI::Element* p) {
    return RtAddr<IsDerivedFromFn>(0x1800c0390ull)(p, ClipRegionTypeId()) ? p : nullptr;
}
UI::ScrollElement* AsScroll(UI::Element* p) {
    return RtAddr<IsDerivedFromFn>(0x1800c0390ull)(p, ScrollTypeId())
               ? reinterpret_cast<UI::ScrollElement*>(p)             // B.3.6 bridge
               : nullptr;
}

int32_t& ClipI32(UI::Element* clip, int off) { return I32(clip, off); }

// The screen-size override half of the inlined GetPaddedSize. Retail's copies
// in this file are ONE-AXIS or TWO-AXIS depending on the child, so the walk is
// factored out and the two conversions are done at the call site.
const uint8_t* ScaleOwnerOf(const UI::Element* e) {
    const UI::Element* root = e;
    while (root->_parent != nullptr) root = root->_parent;
    return static_cast<const uint8_t*>(Pc(root->_dialogParent, 0x10));
}

}  // namespace

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t ContainerElement::Type() const { return TypeIdOf(ContainerElementTypeInfo()); }
const char* ContainerElement::TypeName() const { return TypeNameOf(ContainerElementTypeInfo()); }

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
void ContainerElement::ComputeSize() {
    if ((_flags & kFlagNeedsSizeCalc) == 0) return;

    Element* clip = AsClipRegion(_children[0]);
    clip->ComputeSize();                                     // faithful: may be null

    const int32_t snapW = ClipI32(clip, kClipInnerW);
    const int32_t snapH = ClipI32(clip, kClipInnerH);
    _layoutWidth  = snapW;
    _layoutHeight = snapH;

    // ---- _children[1]: WIDTH ONLY (retail measures no height for it) ------
    int barAW = 0;      // its padded width
    int barAGap = 0;    // this->_barSize, but only when the bar exists
    Element* barA = _children[1];
    if (barA != nullptr) {
        barA->ComputeSize();
        int w = barA->_layoutWidth + barA->_rightPad + barA->_leftPad;
        if (w < barA->_minWidth) w = barA->_minWidth;
        const uint32_t f = barA->_flags;
        if ((f & (kFlagScreenH | kFlagScreenW)) != 0) {
            const uint8_t* owner = ScaleOwnerOf(barA);
            if (f & kFlagScreenW) {
                const uint8_t* dev = VideoDevice();
                w = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x04)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
            }
        }
        barAW = w;
        barAGap = _barSize;
    }

    // ---- _children[2]: BOTH AXES ------------------------------------------
    int barBW = 0, barBH = 0, barBGap = 0;
    Element* barB = _children[2];
    if (barB != nullptr) {
        barB->ComputeSize();
        int w = barB->_layoutWidth  + barB->_rightPad  + barB->_leftPad;
        int h = barB->_layoutHeight + barB->_bottomPad + barB->_topPad;
        if (w < barB->_minWidth)  w = barB->_minWidth;
        if (h < barB->_minHeight) h = barB->_minHeight;
        const uint32_t f = barB->_flags;
        if ((f & (kFlagScreenH | kFlagScreenW)) != 0) {
            const uint8_t* owner = ScaleOwnerOf(barB);
            const uint8_t* dev = VideoDevice();
            if (f & kFlagScreenW)
                w = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x04)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
            if (f & kFlagScreenH)
                h = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x08)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
        }
        barBW = w;
        barBH = h;
        barBGap = _barSize;
    }

    const int32_t reqH = _requestedHeight;
    int32_t clampedH = reqH;
    if (_clampHeight != 0 && reqH > 0 && reqH > snapH) clampedH = snapH;

    const int32_t reqW = _requestedWidth;
    int32_t clampedW = reqW;
    if (_clampWidth != 0 && reqW > 0 && reqW > snapW) clampedW = snapW;

    const int32_t clipW = (reqW > 0) ? (clampedW - barAGap - barAW) : _layoutWidth;
    const int32_t clipH = (reqH > 0) ? (clampedH - barBGap - barBH) : _layoutHeight;

    _layoutWidth  = (reqW > 0) ? clampedW : _layoutWidth  + barAGap + barAW;
    _layoutHeight = (reqH > 0) ? clampedH : _layoutHeight + barBGap + barBW;

    if (ClipI32(clip, kClipReqW) != clipW || ClipI32(clip, kClipReqH) != clipH) {
        ClipI32(clip, kClipReqW) = clipW;
        ClipI32(clip, kClipReqH) = clipH;
        clip->SetNeedsSizeCalc();
    }

    _flags &= ~kFlagNeedsSizeCalc;
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
void ContainerElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    ElementComputeAlignedRect(this, x, y, w, h);

    Element* clip = AsClipRegion(_children[0]);

    Element* raw = _children[0];
    const int l = _layoutLeft;
    const int t = _layoutTop;
    raw->ComputeLayout(l, t, ClipI32(clip, kClipReqW) + l, ClipI32(clip, kClipReqH) + t);

    Element* barA = _children[1];
    if (barA != nullptr) {
        barA->ComputeLayout(_barSize + _layoutLeft + ClipI32(clip, kClipReqW),
                            _layoutTop,
                            _layoutRight,
                            ClipI32(clip, kClipReqH) + _layoutTop);
    }

    Element* barB = _children[2];
    if (barB != nullptr) {
        barB->ComputeLayout(_layoutLeft,
                            _barSize + ClipI32(clip, kClipReqH) + _layoutTop,
                            ClipI32(clip, kClipReqW) + _layoutLeft,
                            _layoutBottom);
    }
}

// ============================================================================
// ?Draw@ContainerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800cc590
// ============================================================================
// EXACTLY THREE children, by a hard-coded `while (offset < 0x18)` — not by
// `_childCount`. A fourth child of a ContainerElement is never drawn.
// The visibility flag is NOT tested here (unlike GroupElement's loop); only the
// null test is.
void ContainerElement::Draw(SpriteContext& context, uint8_t alpha) const {
    Element* border = static_cast<Element*>(_border);
    if (border != nullptr) border->Draw(context, alpha);

    alpha = DimAlpha(this, alpha);

    for (int i = 0; i < 3; ++i) {
        Element* child = _children[i];
        if (child != nullptr) child->Draw(context, alpha);
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
// THE WHEEL IS AN IMPULSE COUNTER, NOT A POSITION. `_wheelImpulse` (+0xd0) takes
// ±7 on a wheel event and is spent one unit per frame, each unit moving the
// bar by `(range / (range / visible)) / 7 * 0.33` — i.e. a third of a page,
// eased over seven frames. The division-by-a-division is what the bytes do
// (0x1800cc4d2 then 0x1800cc4d6); simplifying it to `visible` changes the
// rounding.
void ContainerElement::Update(const System::Time& time, UserInterface* ui,
                              Object::InternalPtr<DialogInstance>* dialog,
                              const UpdateState& state) {
    Element* clip = AsClipRegion(_children[0]);
    clip->ComputeSize();                                     // faithful: may be null

    const int32_t innerW = ClipI32(clip, kClipInnerW);
    const int32_t innerH = ClipI32(clip, kClipInnerH);

    int posB = 0;   // R12D — _children[2]'s position, published into offset X
    int posA = 0;   // R13D — _children[1]'s position, published into offset Y

    if (_children[1] != nullptr) {
        ScrollElement* bar = AsScroll(_children[1]);
        const int32_t visible = ClipI32(clip, kClipReqH);
        posA = static_cast<int>(bar->_currentValue);
        const float frac = static_cast<float>(visible) / static_cast<float>(innerH);
        bar->_displayPercent = frac;
        if (frac > 1.0f) bar->_displayPercent = 1.0f;
        const float range = static_cast<float>(innerH - ClipI32(clip, kClipReqH));
        bar->_maxValue = range;
        if (range < 0.0f) bar->_maxValue = 0.0f;
    }

    if (_children[2] != nullptr) {
        ScrollElement* bar = AsScroll(_children[2]);
        posB = static_cast<int>(bar->_currentValue);
        const float frac = static_cast<float>(ClipI32(clip, kClipReqW)) / static_cast<float>(innerW);
        bar->_displayPercent = frac;
        if (frac > 1.0f) bar->_displayPercent = 1.0f;
        const float range = static_cast<float>(innerW - ClipI32(clip, kClipReqW));
        bar->_maxValue = range;
        if (range < 0.0f) bar->_maxValue = 0.0f;
    }

    if ((_flags & 0x20000u) != 0) {
        const int32_t wheel = state.wheel;
        if (wheel != 0) _wheelImpulse += (wheel > 0) ? -7 : 7;
    }

    if (_wheelImpulse != 0 && _children[1] != nullptr) {
        ScrollElement* bar = AsScroll(_children[1]);
        const float range = static_cast<float>(innerH - ClipI32(clip, kClipReqH));
        if (range > 0.0f) {
            int32_t acc = _wheelImpulse;
            float step = ((range / (range / static_cast<float>(ClipI32(clip, kClipReqH)))) / 7.0f)
                         * 0.33f;
            if (acc < 0) { step = -step; acc += 1; } else { acc -= 1; }
            _wheelImpulse = acc;
            float pos = step + bar->_currentValue;
            if (pos < 0.0f)        pos = 0.0f;
            else if (pos > range)  pos = range;
            bar->_currentValue = pos;
        }
    }

    ClipI32(clip, kClipOffX) = posB;
    ClipI32(clip, kClipOffY) = posA;

    // Retail copies the caller's handle into its own frame and passes THAT
    // address on (slot 7's pointer-to-temporary ABI, Element.h banner).
    Object::InternalPtr<DialogInstance> dialogCopy = *dialog;
    UI::Element::Update(time, ui, &dialogCopy, state);     // qualified: 0x180116270
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
using UI::ContainerElement;

extern "C" uint32_t RtUi_ContainerElement_Type(const ContainerElement* self) {
    return self->UI::ContainerElement::Type();
}
extern "C" const char* RtUi_ContainerElement_TypeName(const ContainerElement* self) {
    return self->UI::ContainerElement::TypeName();
}
extern "C" void RtUi_ContainerElement_ComputeSize(ContainerElement* self) {
    self->UI::ContainerElement::ComputeSize();
}
extern "C" void RtUi_ContainerElement_ComputeLayout(ContainerElement* self,
                                                    int x, int y, int w, int h) {
    self->UI::ContainerElement::ComputeLayout(x, y, w, h);
}
extern "C" void RtUi_ContainerElement_Draw(const ContainerElement* self, void* ctx, uint8_t alpha) {
    self->UI::ContainerElement::Draw(*static_cast<UI::SpriteContext*>(ctx), alpha);
}
extern "C" void RtUi_ContainerElement_Update(ContainerElement* self, const void* time, void* ui,
                                             void** dlg, const UI::Element::UpdateState* state) {
    self->UI::ContainerElement::Update(
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),   // B.3.6 bridge
        *state);
}

// ============================================================================
// Constructors / destructor — DOORS (B4b-2), unchanged in behaviour
// ============================================================================
// The default constructor writes THREE things: +0xc8 (dword 0), +0xcc as a
// SINGLE WORD (`MOV word [RBX+0xcc],AX` — both bool flags in one store) and
// +0xd0. It does NOT touch +0xc0/+0xc4; see the header.
extern "C" void* RtUi_ContainerElement_Ctor(void* selfv) {
    RtAddr<ElementCtorFn>(0x180115e50ull)(selfv);
    *reinterpret_cast<void**>(selfv) = VtContainerElement();
    ContainerElement* self = reinterpret_cast<ContainerElement*>(selfv);   // B.3.6 bridge
    self->_barSize      = 0;
    self->_clampWidth   = 0;
    self->_clampHeight  = 0;
    self->_wheelImpulse = 0;
    return self;
}

extern "C" void* RtUi_ContainerElement_CopyCtor(void* selfv, const void* otherv) {
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(selfv, otherv);
    *reinterpret_cast<void**>(selfv) = VtContainerElement();
    ContainerElement* self = reinterpret_cast<ContainerElement*>(selfv);             // B.3.6 bridge
    const ContainerElement* other = reinterpret_cast<const ContainerElement*>(otherv); // B.3.6 bridge
    self->_requestedWidth  = other->_requestedWidth;
    self->_requestedHeight = other->_requestedHeight;
    self->_barSize         = other->_barSize;
    self->_clampWidth      = other->_clampWidth;
    self->_clampHeight     = other->_clampHeight;
    self->_wheelImpulse    = other->_wheelImpulse;
    return self;
}

extern "C" void RtUi_ContainerElement_Dtor(void* selfv) {
    *reinterpret_cast<void**>(selfv) = VtContainerElement();
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
