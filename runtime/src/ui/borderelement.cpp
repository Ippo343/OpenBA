// ============================================================================
// runtime/src/ui/borderelement.cpp — UI::BorderElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7BorderElement@UI@@6B@`, six owned rows:
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x1800414f0  deleting_dtor.cpp row
//    1    TypeName              0x1800410b0  here
//    2    Type                  0x180041090  here
//    4    ComputeSize           0x1801101d0  here
//    5    ComputeLayout         0x180110360  here
//    6    Draw                  0x1801103e0  here
//   (3, 7..11 are UI::Element's own bodies, already ported in element*.cpp)
//
// plus ??0BorderElement@UI@@QEAA@AEBV01@@Z  0x180041250 (copy)
//      ??1BorderElement@UI@@UEAA@XZ         0x180041230
// (`??0BorderElement@UI@@QEAA@XZ` 0x18010fd40 is factory_ui1.cpp's door.)
//
// ENDGAME B.3.6 block C1: the five slot bodies are MEMBERS of
// `UI::BorderElement` (runtime/types/ui/BorderElement.h, which now owns the
// nine-slice layout — nine UI::Sprite at +0xc0..+0x14f, row major — and its
// evidence). The doors are one-line QUALIFIED delegations.
// ============================================================================
#include "element_common.h"
#include "../../types/ui/BorderElement.h"

using namespace rtuielem;

namespace {

// B6-5: namespace-scope atomic door cache for the member bodies.
std::atomic<void*> g_borderTypeInfo{nullptr};
const uint8_t* BorderElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_borderTypeInfo, "?_typeInfo@BorderElement@UI@@0VBaseTypeInfo@Object@@A"));
}
// The copy-ctor door only.
void* VtBorderElement() { return RT_DATA_SLOT(void*, "??_7BorderElement@UI@@6B@"); }

// UI::SpriteContext (block D's class), reached by address.
using AddSpritePtFn   = void (*)(void* ctx, const void* sprite, int x, int y, uint32_t color);
using AddSpriteRectFn = void (*)(void* ctx, const void* sprite,
                                 int l, int t, int r, int b, uint32_t color);

using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);

}  // namespace

namespace UI {

uint32_t BorderElement::Type() const { return TypeIdOf(BorderElementTypeInfo()); }
const char* BorderElement::TypeName() const { return TypeNameOf(BorderElementTypeInfo()); }

// ============================================================================
// ?ComputeSize@BorderElement@UI@@UEAAXXZ   0x1801101d0
// ============================================================================
// The minimum box the frame needs: the wider of the two corner PAIRS on each
// axis. Only the FOUR CORNERS participate — the stretchable edges contribute
// nothing to the minimum, which is why a border with only edge sprites
// measures 0x0.
//
//   width  = max(bottomLeft.w + bottomRight.w, topRight.w + topLeft.w)
//   height = max(bottomRight.h + topRight.h,   bottomLeft.h + topLeft.h)
//
// *** THE TWO PAIRINGS ARE NOT THE OBVIOUS ONES *** — the height compares a
// RIGHT-column pair against a LEFT-column pair while the width compares a
// BOTTOM-row pair against a mixed pair. That is what 0x180110276..0x1801102e2
// computes and it is transcribed rather than normalised.
void BorderElement::ComputeSize() {
    if (((_flags >> 27) & 1u) == 0) return;

    const int tlW = SpriteW(&_topLeft);
    const int trW = SpriteW(&_topRight);
    const int blW = SpriteW(&_bottomLeft);
    const int brW = SpriteW(&_bottomRight);

    const int tlH = SpriteH(&_topLeft);
    const int blH = SpriteH(&_bottomLeft);
    const int trH = SpriteH(&_topRight);
    const int brH = SpriteH(&_bottomRight);

    int w = blW + brW;
    if (w < trW + tlW) w = trW + tlW;

    int h = brH + trH;
    if (h < blH + tlH) h = blH + tlH;

    _layoutWidth = w;
    _flags &= ~kFlagNeedsSizeCalc;
    _layoutHeight = h;
}

// ============================================================================
// ?ComputeLayout@BorderElement@UI@@UEAAXHHHH@Z   0x180110360
// ============================================================================
// NOT the standard aligned body: a border always fills the box it is given,
// inset by its own padding. No ComputeSize call, no alignment, no clamping.
void BorderElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    Element* border = static_cast<Element*>(_border);
    if (border != nullptr) border->ComputeLayout(x, y, w, h);

    _layoutRight  = w - _rightPad;
    _layoutLeft   = _leftPad + x;
    _layoutBottom = h - _bottomPad;
    _layoutTop    = _topPad + y;
}

// ============================================================================
// ?Draw@BorderElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1801103e0
// ============================================================================
// The nine-slice, in retail's order: top-left corner, top edge, top-right
// corner, left edge, centre, right edge, bottom-left corner, bottom edge,
// bottom-right corner. Corners go through the POINT form of AddSprite
// (0x18011cf00, drawn at natural size); edges and centre through the RECT form
// (0x18011d3c0, stretched).
//
// Every extent is recomputed from the sprite sheet at its point of use — retail
// re-reads e.g. `SpriteH(bottomLeft)` in three separate places rather than
// caching it, and the reads are interleaved with the calls. The calls can
// change a sprite sheet (they do not today), so the re-reads are kept.
void BorderElement::Draw(SpriteContext& context, uint8_t alpha) const {
    const Element* border = static_cast<const Element*>(_border);
    if (border != nullptr) border->Draw(context, alpha);

    const uint32_t base = _color;
    alpha = DimAlpha(this, alpha);
    const uint32_t c = rtuielem::ComputeColor(base, alpha);

    auto L = [&] { return _layoutLeft; };
    auto T = [&] { return _layoutTop; };
    auto R = [&] { return _layoutRight; };
    auto B = [&] { return _layoutBottom; };

    void* ctx = &context;
    const auto pt   = RtAddr<AddSpritePtFn>(0x18011cf00ull);
    const auto rect = RtAddr<AddSpriteRectFn>(0x18011d3c0ull);

    pt(ctx, &_topLeft, L(), T(), c);                                         // 180110469

    rect(ctx, &_top,                                                         // 18011050d
         L() + SpriteW(&_topLeft), T(),
         R() - SpriteW(&_topRight), T() + SpriteH(&_top), c);

    pt(ctx, &_topRight, R() - SpriteW(&_topRight), T(), c);                  // 180110545

    rect(ctx, &_left,                                                        // 1801106b8
         L(), T() + SpriteH(&_topLeft),
         L() + SpriteW(&_left), B() - SpriteH(&_bottomLeft), c);

    rect(ctx, &_centre,                                                      // 1801106b8 (2nd)
         L() + SpriteW(&_left), T() + SpriteH(&_top),
         R() - SpriteW(&_right), B() - SpriteH(&_bottom), c);

    rect(ctx, &_right,                                                       // 18011074e
         R() - SpriteW(&_right), T() + SpriteH(&_topRight),
         R(), B() - SpriteH(&_bottomLeft), c);

    pt(ctx, &_bottomLeft, L(), B() - SpriteH(&_bottomLeft), c);              // 180110786

    rect(ctx, &_bottom,                                                      // 180110826
         L() + SpriteW(&_bottomLeft), B() - SpriteH(&_bottom),
         R() - SpriteW(&_bottomRight), B(), c);

    pt(ctx, &_bottomRight,                                                   // 180110877
       R() - SpriteW(&_bottomRight), B() - SpriteH(&_bottomRight), c);
}

}  // namespace UI

// ============================================================================
// The DOORS
// ============================================================================
extern "C" uint32_t RtUi_BorderElement_Type(const void* self) {
    return reinterpret_cast<const UI::BorderElement*>(self)->UI::BorderElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_BorderElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::BorderElement*>(self)->UI::BorderElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_BorderElement_ComputeSize(void* selfv) {
    reinterpret_cast<UI::BorderElement*>(selfv)->UI::BorderElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_BorderElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    reinterpret_cast<UI::BorderElement*>(selfv)->UI::BorderElement::ComputeLayout(x, y, w, h);   // B.3.6 bridge
}
extern "C" void RtUi_BorderElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::BorderElement*>(selfv)->UI::BorderElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}

// ============================================================================
// Copy constructor / destructor — DOORS (B4b-2), unchanged
// ============================================================================
// ??0BorderElement@UI@@QEAA@AEBV01@@Z   0x180041250
// Eighteen qword copies, i.e. the nine sprites verbatim INCLUDING each one's
// pad dword — a memcpy of 0x90 bytes would be equivalent, but the loop keeps
// the per-field spelling the other copy constructors in this batch use.
extern "C" void* RtUi_BorderElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtBorderElement();
    for (size_t off = offsetof(UI::BorderElement, _topLeft); off < sizeof(UI::BorderElement); off += 8)
        *reinterpret_cast<uint64_t*>(self + off) =
            *reinterpret_cast<const uint64_t*>(other + off);
    return self;
}

// ============================================================================
// ??1BorderElement@UI@@UEAA@XZ   0x180041230
// ============================================================================
// *** IT DOES NOT RE-STAMP ITS OWN VTABLE. *** Five instructions, all of them
// frame setup, then `JMP ~Element`. Every other destructor in this batch stamps
// first; this one does not, and adding the stamp "for consistency" would be a
// divergence. (The scalar arm of the deleting destructor at 0x180041541 calls
// ~Element DIRECTLY for the same reason — there is nothing else to do.)
extern "C" void RtUi_BorderElement_Dtor(void* selfv) {
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiBorderElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@BorderElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_BorderElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x1800410b0ull, (void*)&RtUi_BorderElement_TypeName);

    RegisterRuntimeExport("?Type@BorderElement@UI@@UEBAIXZ",
                          (void*)&RtUi_BorderElement_Type);          ++n;
    RegisterRuntimeAddr(0x180041090ull, (void*)&RtUi_BorderElement_Type);

    RegisterRuntimeExport("?ComputeSize@BorderElement@UI@@UEAAXXZ",
                          (void*)&RtUi_BorderElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1801101d0ull, (void*)&RtUi_BorderElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@BorderElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_BorderElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x180110360ull, (void*)&RtUi_BorderElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@BorderElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_BorderElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1801103e0ull, (void*)&RtUi_BorderElement_Draw);

    RegisterRuntimeExport("??0BorderElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_BorderElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180041250ull, (void*)&RtUi_BorderElement_CopyCtor);

    RegisterRuntimeExport("??1BorderElement@UI@@UEAA@XZ",
                          (void*)&RtUi_BorderElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180041230ull, (void*)&RtUi_BorderElement_Dtor);
    return n;
}
