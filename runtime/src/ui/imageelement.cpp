// ============================================================================
// runtime/src/ui/imageelement.cpp — UI::ImageElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3 (UI::*Element, first half). `??_7ImageElement@UI@@6B@`
// (retail 0x1801560b0) is twelve slots; the eight rows this class owns are all
// here, plus both constructors, the destructor, and the deleting-destructor row
// appended to runtime/src/object/deleting_dtor.cpp.
//
//   slot  method                       retail       where
//   ----  ---------------------------  -----------  --------------------------
//    0    vector deleting dtor         0x180036600  deleting_dtor.cpp row
//    1    TypeName                     0x1800362b0  here
//    2    Type                         0x180036290  here
//    3    Element::Persist             0x180116870  already ported (element.cpp)
//    4    ComputeSize                  0x180118650  here
//    5    ComputeLayout                0x1801186f0  here
//    6    Draw                         0x180118850  here
//    7    Element::Update              0x180116270  already ported
//    8    Element::Enable              0x180116910  already ported
//    9    Element::Contains            0x1801169e0  already ported
//   10    Element::SetColor            0x180035620  already ported
//   11    Element::GetBorder           0x180035630  already ported
//
// plus ??0ImageElement@UI@@QEAA@XZ         0x180118410
//      ??0ImageElement@UI@@QEAA@AEBV01@@Z  0x1800364c0
//      ??1ImageElement@UI@@UEAA@XZ         0x180118470
//
// Layout: runtime/types/ui/ImageElement.h (0xE0 = UI::Element 0xC0 + a
// UI::Sprite at +0xC0, a float rotation at +0xD0 and two int overrides at
// +0xD4 / +0xD8). The deleting destructor at 0x180036600 names `MOV EDX,0xe0`,
// which is the third independent confirmation of that size.
//
// SLOTS 4/5/6 ARE ALSO ANIMATEDIMAGEELEMENT'S. `??_7AnimatedImageElement@UI@@6B@`
// repeats these three addresses (the subclass overrides only Update), so they
// are registered ONCE, here, and animatedimageelement.cpp does not re-register
// them — two owners for one address is the merge hazard deleting_dtor.cpp's
// banner describes one file over.
//
// `??0ImageElement@UI@@QEAA@XZ` (0x180118410) is NOT here: it is already
// ported and registered, table-driven, by runtime/src/ui/ui_factory_wave28.cpp
// (the factory roster). One address, one owner.
// ============================================================================
#include "widget_common.h"
#include "../../types/ui/ImageElement.h"

using namespace rtui;

static_assert(sizeof(::ImageElement) == 0xE0, "UI::ImageElement is 0xE0 bytes");
static_assert(offsetof(::ImageElement, spriteSheet) == 0xC0, "sprite sheet @ +0xC0");
static_assert(offsetof(::ImageElement, spriteIndex) == 0xC8, "sprite index @ +0xC8");
static_assert(offsetof(::ImageElement, rotation)    == 0xD0, "rotation @ +0xD0");
static_assert(offsetof(::ImageElement, fieldD4)     == 0xD4, "width override @ +0xD4");
static_assert(offsetof(::ImageElement, fieldD8)     == 0xD8, "height override @ +0xD8");

namespace {

constexpr int kSprite         = 0xc0;   // UI::Sprite, 0x10 bytes
constexpr int kSpriteIndex    = 0xc8;
constexpr int kRotation       = 0xd0;
constexpr int kWidthOverride  = 0xd4;
constexpr int kHeightOverride = 0xd8;

RTUI_TYPE_SLOTS(ImageElement)

// `?AddSprite@SpriteContext@UI@@` — the ROTATED overload
// (ctx, const Sprite&, left, top, right, bottom, color, float rotation).
// Not ported here: SpriteContext belongs to the sprite lane
// (runtime/src/ui/spritecontext.cpp); reached by address so a later port of it
// is picked up with no change here.
using AddSpriteRotFn = void (*)(void* ctx, const void* sprite,
                                int l, int t, int r, int b,
                                uint32_t color, float rotation);

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);

}  // namespace

RTUI_TYPE_BODIES(ImageElement)

// ============================================================================
// ?ComputeSize@ImageElement@UI@@UEAAXXZ   0x180118650
// ============================================================================
// Early-out on flag bit 27, then: width  = override>0 ? override : sprite width,
//                                 height = override>0 ? override : sprite height.
//
// *** THE EARLY RETURN IN THE HEIGHT ARM IS RETAIL'S, NOT A TIDY-UP. *** When
// the height override is <= 0 AND the sprite index is negative, retail clears
// bit 27, writes ZERO to +0x64 and RETURNS — it does not fall through to the
// common tail. The observable state is the same as falling through would
// produce, but the flag store happens BEFORE the height store on that path and
// AFTER it on the others, so the two arms are kept separate here.
//
// `flags` is snapshotted once (`uVar1`) and the clear is written from the
// snapshot, so a ComputeSize on a child that dirties us again during the call
// would be overwritten. Nothing in this body calls out, so that cannot happen;
// the snapshot is transcribed because it is what the bytes do.
extern "C" void RtUi_ImageElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint32_t flags = U32(self, kElemFlags);
    if (((flags >> 27) & 1u) == 0) return;

    int w = I32(self, kWidthOverride);
    if (w < 1) w = SpriteW(self + kSprite);
    I32(self, kElemLayoutWidth) = w;

    int h = I32(self, kHeightOverride);
    if (h < 1) {
        if (I32(self, kSpriteIndex) < 0) {
            U32(self, kElemFlags) = flags & ~kFlagNeedsSizeCalc;
            I32(self, kElemLayoutHeight) = 0;
            return;
        }
        h = SpriteH(self + kSprite);
    }
    I32(self, kElemLayoutHeight) = h;
    U32(self, kElemFlags) = flags & ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@ImageElement@UI@@UEAAXHHHH@Z   0x1801186f0
// ============================================================================
// The standard body, transcribed once in widget_common.h.
extern "C" void RtUi_ImageElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    ElementComputeAlignedRect(self, x, y, w, h);
}

// ============================================================================
// ?Draw@ImageElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x180118850
// ============================================================================
// Border first (through ITS slot 6), the dim-alpha halve, then one rotated
// AddSprite (0x18011cb10) covering the whole layout rect.
//
// The rotation is loaded into XMM0 and spilled to the outgoing float slot
// BEFORE the colour maths runs (0x18011888b / 0x18011889d); that ordering is
// invisible and is not reproduced.
extern "C" void RtUi_ImageElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    const uint32_t color = U32c(self, kElemColor);
    alpha = DimAlpha(self, alpha);

    RtAddr<AddSpriteRotFn>(0x18011cb10ull)(
        ctx, self + kSprite,
        I32c(self, kElemLayoutLeft),  I32c(self, kElemLayoutTop),
        I32c(self, kElemLayoutRight), I32c(self, kElemLayoutBottom),
        ComputeColor(color, alpha),
        *reinterpret_cast<const float*>(self + kRotation));
}

// ============================================================================
// ??0ImageElement@UI@@QEAA@XZ   0x180118410
// ============================================================================
// `Element::Element`, the vtable stamp, then five stores. +0xD4 and +0xD8 are
// written as ONE QWORD of -1 (`MOV qword [RBX+0xd4],-1`), which is why the
// struct's two "override" ints both start at -1 and why +0xDC (the trailing
// pad) is left as the allocator found it.

// ??0ImageElement@UI@@QEAA@AEBV01@@Z   0x1800364c0
// The copy constructor. +0xC0 and +0xC8 are copied as two overlapping qwords
// (+0xC0 covers the sheet, +0xC8 covers index AND pad), then +0xD0/+0xD4/+0xD8
// as three separate dwords. Transcribed as written.
extern "C" void* RtUi_ImageElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtImageElement();
    *reinterpret_cast<uint64_t*>(self + 0xc0) = *reinterpret_cast<const uint64_t*>(other + 0xc0);
    *reinterpret_cast<uint64_t*>(self + 0xc8) = *reinterpret_cast<const uint64_t*>(other + 0xc8);
    I32(self, 0xd0) = I32c(other, 0xd0);
    I32(self, 0xd4) = I32c(other, 0xd4);
    I32(self, 0xd8) = I32c(other, 0xd8);
    return self;
}

// ============================================================================
// ??1ImageElement@UI@@UEAA@XZ   0x180118470
// ============================================================================
// Re-stamp own vtable, then TAIL-JUMP to ~Element. The class owns no
// refcounted member: the Sprite is two plain words.
extern "C" void RtUi_ImageElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtImageElement();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration — both doors for every row.
// ============================================================================
int RtUiImageElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@ImageElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ImageElement_TypeName);       ++n;
    RegisterRuntimeAddr(0x1800362b0ull, (void*)&RtUi_ImageElement_TypeName);

    RegisterRuntimeExport("?Type@ImageElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ImageElement_Type);           ++n;
    RegisterRuntimeAddr(0x180036290ull, (void*)&RtUi_ImageElement_Type);

    RegisterRuntimeExport("?ComputeSize@ImageElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ImageElement_ComputeSize);    ++n;
    RegisterRuntimeAddr(0x180118650ull, (void*)&RtUi_ImageElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ImageElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ImageElement_ComputeLayout);  ++n;
    RegisterRuntimeAddr(0x1801186f0ull, (void*)&RtUi_ImageElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ImageElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ImageElement_Draw);           ++n;
    RegisterRuntimeAddr(0x180118850ull, (void*)&RtUi_ImageElement_Draw);

    RegisterRuntimeExport("??0ImageElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_ImageElement_CopyCtor);       ++n;
    RegisterRuntimeAddr(0x1800364c0ull, (void*)&RtUi_ImageElement_CopyCtor);

    RegisterRuntimeExport("??1ImageElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ImageElement_Dtor);           ++n;
    RegisterRuntimeAddr(0x180118470ull, (void*)&RtUi_ImageElement_Dtor);
    return n;
}
