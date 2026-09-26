// ============================================================================
// runtime/src/ui/imageelement.cpp — UI::ImageElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3 (UI::*Element, first half). `??_7ImageElement@UI@@6B@`
// (retail 0x1801560b0) is twelve slots:
//
//   slot  method                       retail       where
//   ----  ---------------------------  -----------  --------------------------
//    0    vector deleting dtor         0x180036600  deleting_dtor.cpp row
//    1    TypeName                     0x1800362b0  here
//    2    Type                         0x180036290  here
//    3    Element::Persist             0x180116870  element_vtable.cpp
//    4    ComputeSize                  0x180118650  here
//    5    ComputeLayout                0x1801186f0  here
//    6    Draw                         0x180118850  here
//    7..11                             Element's
//
// plus ??0ImageElement@UI@@QEAA@AEBV01@@Z  0x1800364c0  (copy ctor, a DOOR)
//      ??1ImageElement@UI@@UEAA@XZ         0x180118470  (a DOOR)
//
// Layout: runtime/types/ui/ImageElement.h (0xE0 = UI::Element 0xC0 + a
// UI::Sprite at +0xC0, a float rotation at +0xD0 and the width/height
// overrides at +0xD4 / +0xD8).
//
// SLOTS 4/5/6 ARE ALSO ANIMATEDIMAGEELEMENT'S. `??_7AnimatedImageElement@UI@@6B@`
// repeats these three addresses (the subclass overrides only Update), so they
// are registered ONCE, here; in C++ the subclass simply does not override them.
//
// `??0ImageElement@UI@@QEAA@XZ` (0x180118410) is NOT here: it is ported and
// registered, table-driven, by runtime/src/ui/ui_factory_wave28.cpp.
// `GetSprite` / `SetRotation` / `SetSprite` / `SetWidthHeight` are
// widgets_layout.cpp's doors (fragment pass).
//
// ENDGAME B.3.6 block C2: slots 1/2/4/5/6 are MEMBERS of `UI::ImageElement`;
// the registered doors are one-line QUALIFIED delegations. The border's raw
// slot-6 read is a virtual member call; the rotated AddSprite (by address
// 0x18011cb10 before) is the real `UI::SpriteContext` member.
// ============================================================================
#include "element_common.h"
#include "../../types/ui/ImageElement.h"
#include "../../types/ui/SpriteContext.h"

using namespace rtuielem;

namespace {

// RULE B6-5: a namespace-scope atomic cache for the member bodies.
std::atomic<void*> g_imageElementTypeInfo{nullptr};
const uint8_t* ImageElementTypeInfoAtomic() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_imageElementTypeInfo, "?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A"));
}

// Door-only.
void* VtImageElement() { return RT_DATA_SLOT(void*, "??_7ImageElement@UI@@6B@"); }

using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);

}  // namespace

namespace UI {

uint32_t ImageElement::Type() const {                           // 0x180036290
    return rtuielem::TypeIdOf(ImageElementTypeInfoAtomic());
}
// The key function: GCC emits `vtable for UI::ImageElement` here.
const char* ImageElement::TypeName() const {                    // 0x1800362b0
    return rtuielem::TypeNameOf(ImageElementTypeInfoAtomic());
}

// ============================================================================
// ?ComputeSize@ImageElement@UI@@UEAAXXZ   0x180118650
// ============================================================================
// Early-out on flag bit 27, then: width  = _width  > 0 ? _width  : sprite width,
//                                 height = _height > 0 ? _height : sprite height.
//
// *** THE EARLY RETURN IN THE HEIGHT ARM IS RETAIL'S, NOT A TIDY-UP. *** When
// the height override is <= 0 AND the sprite index is negative, retail clears
// bit 27, writes ZERO to +0x64 and RETURNS — the flag store happens BEFORE the
// height store on that path and AFTER it on the others, so the two arms are
// kept separate here.
//
// `flags` is snapshotted once and the clear is written from the snapshot.
void ImageElement::ComputeSize() {
    const uint32_t flags = _flags;
    if (((flags >> 27) & 1u) == 0) return;

    int w = _width;
    if (w < 1) w = SpriteW(&_sprite);
    _layoutWidth = w;

    int h = _height;
    if (h < 1) {
        if (_sprite.spriteIndex < 0) {
            _flags = flags & ~static_cast<uint32_t>(kElementFlagNeedsSizeCalc);
            _layoutHeight = 0;
            return;
        }
        h = SpriteH(&_sprite);
    }
    _layoutHeight = h;
    _flags = flags & ~static_cast<uint32_t>(kElementFlagNeedsSizeCalc);
}

// ============================================================================
// ?ComputeLayout@ImageElement@UI@@UEAAXHHHH@Z   0x1801186f0
// ============================================================================
// The standard body, transcribed once in element_common.h.
void ImageElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    ElementComputeAlignedRect(this, x, y, w, h);
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
void ImageElement::Draw(SpriteContext& context, uint8_t alpha) const {
    const Element* border = static_cast<const Element*>(_border);
    if (border != nullptr)
        border->Draw(context, alpha);

    const uint32_t color = _color;
    alpha = DimAlpha(this, alpha);

    context.AddSprite(_sprite,
                      _layoutLeft,  _layoutTop,
                      _layoutRight, _layoutBottom,
                      rtuielem::ComputeColor(color, alpha),
                      _rotation);
}

}  // namespace UI

// ============================================================================
// The DOORS — each a one-line QUALIFIED delegation.
// ============================================================================
extern "C" uint32_t RtUi_ImageElement_Type(const void* self) {
    return reinterpret_cast<const UI::ImageElement*>(self)->UI::ImageElement::Type();       // B.3.6 bridge
}
extern "C" const char* RtUi_ImageElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::ImageElement*>(self)->UI::ImageElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_ImageElement_ComputeSize(void* selfv) {
    reinterpret_cast<UI::ImageElement*>(selfv)->UI::ImageElement::ComputeSize();           // B.3.6 bridge
}
extern "C" void RtUi_ImageElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    reinterpret_cast<UI::ImageElement*>(self)                                              // B.3.6 bridge
        ->UI::ImageElement::ComputeLayout(x, y, w, h);
}
extern "C" void RtUi_ImageElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::ImageElement*>(selfv)                                       // B.3.6 bridge
        ->UI::ImageElement::Draw(*static_cast<UI::SpriteContext*>(ctx), alpha);
}

// ============================================================================
// ??0ImageElement@UI@@QEAA@AEBV01@@Z   0x1800364c0   — a CTOR DOOR (B4b-2)
// ============================================================================
// +0xC0 and +0xC8 are copied as two qwords (+0xC8 covers index AND pad), then
// +0xD0/+0xD4/+0xD8 as three separate dwords. Transcribed as written.
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
// ??1ImageElement@UI@@UEAA@XZ   0x180118470   — a DTOR DOOR (B4b-2)
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
