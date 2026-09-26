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
// ENDGAME B.3.6 block C1: the seven slot bodies are MEMBERS of
// `UI::ClipRegionElement` (runtime/types/ui/ClipRegionElement.h, which now
// owns the six-dword layout and its evidence). The base calls in Update and
// Contains are QUALIFIED member calls (they were `ResolveAddr` by address).
// ============================================================================
#include "element_common.h"
#include "../../types/ui/ClipRegionElement.h"
// ENDGAME B.3.5 block VC: `Video::Viewport`, which replaces this file's local
// duplicate of its four fields (RULE B5-2's duplicate-view collapse).
#include "../../types/video/Context.h"

using namespace rtuielem;

namespace {

// UI::UserInterface's current clip rectangle, four dwords at +0xfc..+0x108
// (block E's class, reached at raw offsets). Proven by this class: Update and
// Contains save it, overwrite it with their own layout rect, run the base body,
// and put the old one back.
constexpr int kUiClipLeft   = 0xfc;
constexpr int kUiClipTop    = 0x100;
constexpr int kUiClipRight  = 0x104;
constexpr int kUiClipBottom = 0x108;

// B6-5: namespace-scope atomic door cache for the member bodies.
std::atomic<void*> g_clipTypeInfo{nullptr};
const uint8_t* ClipRegionElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_clipTypeInfo, "?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A"));
}
// The ctor/dtor doors only.
void* VtClipRegionElement() { return RT_DATA_SLOT(void*, "??_7ClipRegionElement@UI@@6B@"); }

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);
// UI::SpriteContext::SetScissor(ctx, Viewport& out, const Viewport& in) —
// returns the PREVIOUS scissor by value through the hidden slot in RDX.
// Block D's class, reached by address.
using SetScissorFn      = void* (*)(void* ctx, void* outOld, const void* in);

// ENDGAME B.3.5 block VC: the REAL `Video::Viewport` (runtime/types/video/
// Context.h), field-identical to the local duplicate this file once had.
using Viewport = ::Video::Viewport;

}  // namespace

namespace UI {

uint32_t ClipRegionElement::Type() const { return TypeIdOf(ClipRegionElementTypeInfo()); }
const char* ClipRegionElement::TypeName() const { return TypeNameOf(ClipRegionElementTypeInfo()); }

// ============================================================================
// ?ComputeSize@ClipRegionElement@UI@@UEAAXXZ   0x1800cbc60
// ============================================================================
// The viewport is whatever size it was told to be; the CHILD's padded size is
// measured into +0xc8/+0xcc for the scroll bars to read.
//
// `_children[0]` is dereferenced with NO null test (0x1800cbc7a loads it and
// passes it straight to GetPaddedSize). A ClipRegionElement always has its one
// child, built by ContainerDescription::CreateInstance.
void ClipRegionElement::ComputeSize() {
    if (((_flags >> 27) & 1u) == 0) return;

    _layoutWidth  = _reqWidth;
    _layoutHeight = _reqHeight;

    _children._data[0]->GetPaddedSize(_innerWidth, _innerHeight);   // 0x180115f10

    _flags &= ~kFlagNeedsSizeCalc;
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
void ClipRegionElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    ElementComputeAlignedRect(this, x, y, w, h);

    Element* child = _children._data[0];
    if (child == nullptr) return;
    if ((child->_flags & kFlagHidden) != 0) return;

    int cw = 0, ch = 0;
    PaddedSizeInline(child, &cw, &ch);

    Element* again = _children._data[0];
    const int cx = _layoutLeft - _offsetX;
    const int cy = _layoutTop  - _offsetY;
    again->ComputeLayout(cx, cy, cx + cw, cy + ch);
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
void ClipRegionElement::Draw(SpriteContext& context, uint8_t alpha) const {
    const Element* border = static_cast<const Element*>(_border);
    if (border != nullptr) border->Draw(context, alpha);

    alpha = DimAlpha(this, alpha);

    Viewport rect;
    rect.x = _layoutLeft;
    rect.y = _layoutTop;
    rect.width  = (_layoutRight  - rect.x) + 1;
    rect.height = (_layoutBottom - rect.y) + 1;

    void* ctx = &context;
    Viewport saved;
    const auto setScissor = RtAddr<SetScissorFn>(0x18011d050ull);
    setScissor(ctx, &saved, &rect);

    const int32_t count = _children._size;
    for (int32_t i = 0; i < count; ++i) {
        const Element* child = _children._data[i];
        if (child == nullptr) continue;
        if ((child->_flags & kFlagHidden) != 0) continue;
        child->Draw(context, alpha);
    }

    setScissor(ctx, &rect, &saved);
}

// ============================================================================
// ?Update@ClipRegionElement@UI@@   0x1800cbd70
// ?Contains@ClipRegionElement@UI@@UEBA_NHHPEAVUserInterface@2@@Z  0x1800cbcc0
// ============================================================================
// The same idiom twice: install THIS element's layout rect as the
// UserInterface's clip rectangle, run the base body (QUALIFIED — whose hit
// testing consults it), then put the caller's rectangle back. Without this,
// an element scrolled out of a panel would still take the mouse.
//
// The save reads the four dwords BEFORE any of them is overwritten; a
// field-by-field save/overwrite would corrupt the second and later reads.
void ClipRegionElement::Update(const System::Time& time, UserInterface* ui,
                               Object::InternalPtr<DialogInstance>* dialog,
                               const UpdateState& state) {
    uint8_t* u = reinterpret_cast<uint8_t*>(ui);

    const int32_t l = _layoutLeft;
    const int32_t t = _layoutTop;
    const int32_t r = _layoutRight;
    const int32_t b = _layoutBottom;

    const int32_t sl  = I32(u, kUiClipLeft);
    const int32_t st_ = I32(u, kUiClipTop);
    const int32_t sr  = I32(u, kUiClipRight);
    const int32_t sb  = I32(u, kUiClipBottom);

    I32(u, kUiClipLeft)   = l;
    I32(u, kUiClipTop)    = t;
    I32(u, kUiClipRight)  = r;
    I32(u, kUiClipBottom) = b;

    Object::InternalPtr<DialogInstance> dlgCopy = *dialog;
    Element::Update(time, ui, &dlgCopy, state);

    I32(u, kUiClipLeft)   = sl;
    I32(u, kUiClipTop)    = st_;
    I32(u, kUiClipRight)  = sr;
    I32(u, kUiClipBottom) = sb;
}

bool ClipRegionElement::Contains(int32_t x, int32_t y, UserInterface* ui) const {
    uint8_t* u = reinterpret_cast<uint8_t*>(ui);

    const int32_t l = _layoutLeft;
    const int32_t t = _layoutTop;
    const int32_t r = _layoutRight;
    const int32_t b = _layoutBottom;

    const int32_t sl  = I32(u, kUiClipLeft);
    const int32_t st_ = I32(u, kUiClipTop);
    const int32_t sr  = I32(u, kUiClipRight);
    const int32_t sb  = I32(u, kUiClipBottom);

    I32(u, kUiClipLeft)   = l;
    I32(u, kUiClipTop)    = t;
    I32(u, kUiClipRight)  = r;
    I32(u, kUiClipBottom) = b;

    const bool inside = Element::Contains(x, y, ui);

    I32(u, kUiClipLeft)   = sl;
    I32(u, kUiClipTop)    = st_;
    I32(u, kUiClipRight)  = sr;
    I32(u, kUiClipBottom) = sb;
    return inside;
}

}  // namespace UI

// ============================================================================
// The DOORS
// ============================================================================
extern "C" uint32_t RtUi_ClipRegionElement_Type(const void* self) {
    return reinterpret_cast<const UI::ClipRegionElement*>(self)->UI::ClipRegionElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_ClipRegionElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::ClipRegionElement*>(self)->UI::ClipRegionElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_ClipRegionElement_ComputeSize(void* selfv) {
    reinterpret_cast<UI::ClipRegionElement*>(selfv)->UI::ClipRegionElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_ClipRegionElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    reinterpret_cast<UI::ClipRegionElement*>(selfv)->UI::ClipRegionElement::ComputeLayout(x, y, w, h);   // B.3.6 bridge
}
extern "C" void RtUi_ClipRegionElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::ClipRegionElement*>(selfv)->UI::ClipRegionElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}
extern "C" void RtUi_ClipRegionElement_Update(void* selfv, const void* time, void* ui,
                                              void** dlg, const void* statev) {
    reinterpret_cast<UI::ClipRegionElement*>(selfv)->UI::ClipRegionElement::Update(   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),   // B.3.6 bridge
        *static_cast<const UI::Element::UpdateState*>(statev));
}
extern "C" bool RtUi_ClipRegionElement_Contains(const void* selfv, int x, int y, void* ui) {
    return reinterpret_cast<const UI::ClipRegionElement*>(selfv)          // B.3.6 bridge
        ->UI::ClipRegionElement::Contains(x, y, static_cast<UI::UserInterface*>(ui));
}

// ============================================================================
// Constructors / destructor — DOORS (B4b-2), unchanged
// ============================================================================
// The default constructor writes +0xc4 and +0xc8 as ONE QWORD of 100
// (`MOV qword [RCX+0xc4],0x64`), so +0xc8 is set to 0 by the upper half — the
// inner width starts at zero, not at 100. Transcribed as the two fields it
// covers rather than as the qword, which is byte-identical.
extern "C" void* RtUi_ClipRegionElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtClipRegionElement();
    UI::ClipRegionElement* e = reinterpret_cast<UI::ClipRegionElement*>(self);   // B.3.6 bridge
    e->_reqWidth    = 100;
    e->_reqHeight   = 100;
    e->_innerWidth  = 0;
    e->_innerHeight = 0;
    e->_offsetX     = 0;
    e->_offsetY     = 0;
    return self;
}

extern "C" void* RtUi_ClipRegionElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtClipRegionElement();
    for (int off = 0xc0; off <= 0xd4; off += 4)             // _reqWidth .. _offsetY
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
