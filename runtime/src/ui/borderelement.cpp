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
//   (3, 7..11 are UI::Element's own bodies, already ported in element.cpp)
//
// plus ??0BorderElement@UI@@QEAA@XZ         0x18010fd40
//      ??0BorderElement@UI@@QEAA@AEBV01@@Z  0x180041250
//      ??1BorderElement@UI@@UEAA@XZ         0x180041230
//
// LAYOUT — 0x150 bytes (`MOV EDX,0x150` at 0x180041522), which is exactly
// UI::Element's 0xC0 plus NINE UI::Sprite values of 0x10 each. The constructor
// at 0x18010fd40 writes all nine as {sheet = 0, index = -1}, so the count is
// not inferred from the size, it is enumerated.
//
// THE NINE ARE A 3x3 NINE-SLICE, ROW MAJOR:
//
//     +0x0c0 topLeft     +0x0d0 top        +0x0e0 topRight
//     +0x0f0 left        +0x100 centre     +0x110 right
//     +0x120 bottomLeft  +0x130 bottom     +0x140 bottomRight
//
// That assignment is READ OUT OF Draw (0x1801103e0), not guessed: the corner
// sprites are the ones drawn with the point-form AddSprite at the four extreme
// coordinates, and the edges are the ones stretched between them.
// ============================================================================
#include "widget_common.h"

using namespace rtui;

namespace {

// The nine slots, in table order.
constexpr int kTopLeft     = 0x0c0;
constexpr int kTop         = 0x0d0;
constexpr int kTopRight    = 0x0e0;
constexpr int kLeft        = 0x0f0;
constexpr int kCentre      = 0x100;
constexpr int kRight       = 0x110;
constexpr int kBottomLeft  = 0x120;
constexpr int kBottom      = 0x130;
constexpr int kBottomRight = 0x140;
constexpr int kSizeOf      = 0x150;

static_assert(kBottomRight + 0x10 == kSizeOf,
              "nine 0x10-byte UI::Sprite values fill BorderElement to 0x150");

RTUI_TYPE_SLOTS(BorderElement)

// UI::SpriteContext, reached by address — it is the sprite lane's class.
using AddSpritePtFn   = void (*)(void* ctx, const void* sprite, int x, int y, uint32_t color);
using AddSpriteRectFn = void (*)(void* ctx, const void* sprite,
                                 int l, int t, int r, int b, uint32_t color);

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);

}  // namespace

RTUI_TYPE_BODIES(BorderElement)

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
extern "C" void RtUi_BorderElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    const int tlW = SpriteW(self + kTopLeft);
    const int trW = SpriteW(self + kTopRight);
    const int blW = SpriteW(self + kBottomLeft);
    const int brW = SpriteW(self + kBottomRight);

    const int tlH = SpriteH(self + kTopLeft);
    const int blH = SpriteH(self + kBottomLeft);
    const int trH = SpriteH(self + kTopRight);
    const int brH = SpriteH(self + kBottomRight);

    int w = blW + brW;
    if (w < trW + tlW) w = trW + tlW;

    int h = brH + trH;
    if (h < blH + tlH) h = blH + tlH;

    I32(self, kElemLayoutWidth) = w;
    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElemLayoutHeight) = h;
}

// ============================================================================
// ?ComputeLayout@BorderElement@UI@@UEAAXHHHH@Z   0x180110360
// ============================================================================
// NOT the standard aligned body: a border always fills the box it is given,
// inset by its own padding. No ComputeSize call, no alignment, no clamping.
extern "C" void RtUi_BorderElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    uint8_t* self = static_cast<uint8_t*>(selfv);

    void* border = P(self, kElemBorder);
    if (border != nullptr)
        VSlot<ComputeLayoutFn>(border, kSlotComputeLayout)(border, x, y, w, h);

    I32(self, kElemLayoutRight)  = w - I32(self, kElemRightPad);
    I32(self, kElemLayoutLeft)   = I32(self, kElemLeftPad) + x;
    I32(self, kElemLayoutBottom) = h - I32(self, kElemBottomPad);
    I32(self, kElemLayoutTop)    = I32(self, kElemTopPad) + y;
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
extern "C" void RtUi_BorderElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    const uint32_t base = U32c(self, kElemColor);
    alpha = DimAlpha(self, alpha);
    const uint32_t c = ComputeColor(base, alpha);

    auto L = [&] { return I32c(self, kElemLayoutLeft); };
    auto T = [&] { return I32c(self, kElemLayoutTop); };
    auto R = [&] { return I32c(self, kElemLayoutRight); };
    auto B = [&] { return I32c(self, kElemLayoutBottom); };

    const auto pt   = RtAddr<AddSpritePtFn>(0x18011cf00ull);
    const auto rect = RtAddr<AddSpriteRectFn>(0x18011d3c0ull);

    pt(ctx, self + kTopLeft, L(), T(), c);                                   // 180110469

    rect(ctx, self + kTop,                                                   // 18011050d
         L() + SpriteW(self + kTopLeft), T(),
         R() - SpriteW(self + kTopRight), T() + SpriteH(self + kTop), c);

    pt(ctx, self + kTopRight, R() - SpriteW(self + kTopRight), T(), c);      // 180110545

    rect(ctx, self + kLeft,                                                  // 1801106b8
         L(), T() + SpriteH(self + kTopLeft),
         L() + SpriteW(self + kLeft), B() - SpriteH(self + kBottomLeft), c);

    rect(ctx, self + kCentre,                                                // 1801106b8 (2nd)
         L() + SpriteW(self + kLeft), T() + SpriteH(self + kTop),
         R() - SpriteW(self + kRight), B() - SpriteH(self + kBottom), c);

    rect(ctx, self + kRight,                                                 // 18011074e
         R() - SpriteW(self + kRight), T() + SpriteH(self + kTopRight),
         R(), B() - SpriteH(self + kBottomLeft), c);

    pt(ctx, self + kBottomLeft, L(), B() - SpriteH(self + kBottomLeft), c);  // 180110786

    rect(ctx, self + kBottom,                                                // 180110826
         L() + SpriteW(self + kBottomLeft), B() - SpriteH(self + kBottom),
         R() - SpriteW(self + kBottomRight), B(), c);

    pt(ctx, self + kBottomRight,                                             // 180110877
       R() - SpriteW(self + kBottomRight), B() - SpriteH(self + kBottomRight), c);
}

// ============================================================================
// ??0BorderElement@UI@@QEAA@XZ   0x18010fd40
// ============================================================================
extern "C" void* RtUi_BorderElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtBorderElement();
    for (int off = kTopLeft; off < kSizeOf; off += 0x10) {
        *reinterpret_cast<uint64_t*>(self + off) = 0;   // sheet
        I32(self, off + 8) = -1;                        // index
    }
    return self;
}

// ??0BorderElement@UI@@QEAA@AEBV01@@Z   0x180041250
// Eighteen qword copies, i.e. the nine sprites verbatim INCLUDING each one's
// pad dword — a memcpy of 0x90 bytes would be equivalent, but the loop keeps
// the per-field spelling the other copy constructors in this batch use.
extern "C" void* RtUi_BorderElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtBorderElement();
    for (int off = kTopLeft; off < kSizeOf; off += 8)
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

    RegisterRuntimeExport("??0BorderElement@UI@@QEAA@XZ",
                          (void*)&RtUi_BorderElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x18010fd40ull, (void*)&RtUi_BorderElement_Ctor);

    RegisterRuntimeExport("??0BorderElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_BorderElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180041250ull, (void*)&RtUi_BorderElement_CopyCtor);

    RegisterRuntimeExport("??1BorderElement@UI@@UEAA@XZ",
                          (void*)&RtUi_BorderElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180041230ull, (void*)&RtUi_BorderElement_Dtor);
    return n;
}
