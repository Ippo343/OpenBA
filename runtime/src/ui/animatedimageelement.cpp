// ============================================================================
// runtime/src/ui/animatedimageelement.cpp — UI::AnimatedImageElement
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7AnimatedImageElement@UI@@6B@` (retail
// 0x180156000). The subclass overrides exactly ONE behavioural slot — Update —
// and inherits ImageElement's ComputeSize/ComputeLayout/Draw, whose three
// addresses the table repeats verbatim:
//
//   slot  method                       retail       where
//   ----  ---------------------------  -----------  --------------------------
//    0    vector deleting dtor         0x180036db0  deleting_dtor.cpp row
//    1    TypeName                     0x180036ab0  here
//    2    Type                         0x180036a90  here
//    4    ImageElement::ComputeSize    0x180118650  imageelement.cpp
//    5    ImageElement::ComputeLayout  0x1801186f0  imageelement.cpp
//    6    ImageElement::Draw           0x180118850  imageelement.cpp
//    7    Update                       0x1800cb290  here
//
// plus ??0AnimatedImageElement@UI@@QEAA@XZ        0x1800cb3f0
//      ??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z 0x180036c30
//      ??1AnimatedImageElement@UI@@UEAA@XZ        0x1800cb3a0
//
// LAYOUT — 0x100 bytes, from the deleting destructor's `MOV EDX,0x100`
// (0x180036de2) and confirmed field by field by the constructor at 0x1800cb3f0:
//
//   0x000 .. 0x0df   UI::ImageElement
//   0x0e0            Collection::Array<UI::Sprite>::_data
//   0x0e8            ..._size        (the frame count)
//   0x0ec            ..._capacity
//   0x0f0  int32     frames per second; the constructor's default is 30 (0x1e)
//   0x0f4            NEVER WRITTEN by the constructor
//   0x0f8  int64     accumulated ticks
//
// There is no type header for this class: every offset above is cited at its
// use below, which is the same standard runtime/src/ui/widgets_layout.cpp
// applies to the eight classes it ports without one.
// ============================================================================
#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kFrames      = 0xe0;   // Array<Sprite>::_data
constexpr int kFrameCount  = 0xe8;   // Array<Sprite>::_size
constexpr int kFps         = 0xf0;
constexpr int kAccumTicks  = 0xf8;

constexpr int kSprite      = 0xc0;   // the inherited UI::Sprite
constexpr int kSpriteIndex = 0xc8;

RTUI_TYPE_SLOTS(AnimatedImageElement)
void* VtImageElementBase() { return RT_DATA_SLOT(void*, "??_7ImageElement@UI@@6B@"); }

// `System::Timer::_secondsPerTick`, an exported DOUBLE (ordinal 4797). The
// same slot runtime/src/audio/audio_scene_update.cpp reads.
double SecondsPerTick() {
    double* p = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    return p ? *p : 0.0;
}

using ArrayDtorFn  = void* (*)(void* array);
using ArrayCopyFn  = void* (*)(void* dst, const void* src);
using ElementCtorFn = void* (*)(void* self);
using ImageCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn = void (*)(void* self);
using ElemUpdateFn  = void (*)(void* self, const void* time, void* ui,
                               void** dlg, const void* state);

}  // namespace

RTUI_TYPE_BODIES(AnimatedImageElement)

// ============================================================================
// ?Update@AnimatedImageElement@UI@@   0x1800cb290
// ============================================================================
// Advance the animation clock, pick a frame, and publish it into the inherited
// UI::Sprite — marking the parent chain dirty only when the frame actually
// changed.
//
//   Element::Update(this, time, ui, &dlgCopy, state)
//   accum += *(int64*)time                            ; System::Time is its ticks
//   elapsed = (float)((double)accum * _secondsPerTick)
//   period  = (float)frameCount / (float)fps
//   if (elapsed >= period) { elapsed -= period;
//                            accum = (int64)((double)elapsed / _secondsPerTick) }
//   frame   = (int)((elapsed / period) * (float)frameCount)
//   sprite  = frames[frame]                           ; ONE 16-byte MOVUPS
//   if (sprite.sheet != this->sheet || sprite.index != this->index)
//        SetNeedsSizeCalc(this)
//   this->sprite = sprite                             ; the same MOVUPS
//
// *** TWO THINGS A TIDIED PORT GETS WRONG. ***
//
//  1. `frameCount` is read TWICE from memory (0x1800cb2da for the period and
//     0x1800cb318 for the frame index), both times after Element::Update has
//     returned. Caching it in one local would assert that the base Update
//     cannot change the frame list, which nothing here establishes.
//  2. THE PARENT WALK CLOBBERS THE `this` REGISTER. Retail computes
//     `RCX = this + 0xc0` BEFORE the walk (0x1800cb340), walks through RBX
//     — which ends at nullptr — and then stores the sprite through the SAVED
//     RCX. Writing `self->sprite = s` after a walk that used `self` as its
//     cursor would store through null. Kept as a separate saved pointer here.
//
// The comparison is `sheet` (a full qword) and `index` (a dword) only; the
// 4 pad bytes at +0xcc are copied but never compared.
extern "C" void RtUi_AnimatedImageElement_Update(void* selfv, const void* time,
                                                 void* ui, void** dlg,
                                                 const void* state) {
    uint8_t* self = static_cast<uint8_t*>(selfv);

    // The InternalPtr<DialogInstance> parameter arrives BY HIDDEN REFERENCE
    // (MSVC passes a non-trivially-copyable class that way); retail copies the
    // pointed-to value into its own home slot and forwards the address of THAT.
    void* dlgCopy = *dlg;
    RtAddr<ElemUpdateFn>(0x180116270ull)(self, time, ui, &dlgCopy, state);

    const double spt = SecondsPerTick();

    int64_t& accum = *reinterpret_cast<int64_t*>(self + kAccumTicks);
    accum += *static_cast<const int64_t*>(time);

    float elapsed = static_cast<float>(static_cast<double>(accum) * spt);
    const float period = static_cast<float>(I32(self, kFrameCount)) /
                         static_cast<float>(I32(self, kFps));

    if (elapsed >= period) {
        elapsed = elapsed - period;
        accum = static_cast<int64_t>(static_cast<double>(elapsed) / spt);
    }

    const int frame = static_cast<int>((elapsed / period) *
                                       static_cast<float>(I32(self, kFrameCount)));

    const uint8_t* src = static_cast<const uint8_t*>(P(self, kFrames)) +
                         static_cast<int64_t>(frame) * 0x10;
    uint64_t lo = 0, hi = 0;
    __builtin_memcpy(&lo, src + 0, 8);
    __builtin_memcpy(&hi, src + 8, 8);

    uint8_t* dst = self + kSprite;                       // saved BEFORE the walk
    if (*reinterpret_cast<const uint64_t*>(self + kSprite) != lo ||
        I32(self, kSpriteIndex) != static_cast<int32_t>(hi)) {
        SetNeedsSizeCalc(self);
    }
    __builtin_memcpy(dst + 0, &lo, 8);
    __builtin_memcpy(dst + 8, &hi, 8);
}

// ============================================================================
// ??0AnimatedImageElement@UI@@QEAA@XZ   0x1800cb3f0
// ============================================================================
// Element::Element, the ImageElement vtable stamp and ImageElement's five
// stores INLINED (retail does not call ??0ImageElement here), then its own
// vtable and four more stores. The dead intermediate stamp is reproduced for
// the same reason runtime/src/ui/groupdescription.cpp reproduces its three.
extern "C" void* RtUi_AnimatedImageElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtImageElementBase();
    *reinterpret_cast<uint64_t*>(self + 0xc0) = 0;
    I32(self, 0xc8) = -1;
    I32(self, 0xd0) = 0;
    *reinterpret_cast<uint64_t*>(self + 0xd4) = 0xffffffffffffffffull;
    P(self, 0x00) = VtAnimatedImageElement();
    *reinterpret_cast<uint64_t*>(self + kFrames)     = 0;
    *reinterpret_cast<uint64_t*>(self + kFrameCount) = 0;   // _size + _capacity
    I32(self, kFps) = 0x1e;
    *reinterpret_cast<uint64_t*>(self + kAccumTicks) = 0;
    return self;
}

// ??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z   0x180036c30
// Chains to ??0ImageElement (the COPY ctor, out of line this time), then
// deep-copies the frame array with Collection::Array<Sprite>::operator=
// (0x180037020) onto a zeroed target.
extern "C" void* RtUi_AnimatedImageElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ImageCopyCtorFn>(0x1800364c0ull)(self, other);
    P(self, 0x00) = VtAnimatedImageElement();
    *reinterpret_cast<uint64_t*>(self + kFrames)     = 0;
    *reinterpret_cast<uint64_t*>(self + kFrameCount) = 0;
    RtAddr<ArrayCopyFn>(0x180037020ull)(self + kFrames, other + kFrames);
    I32(self, kFps) = I32c(other, kFps);
    *reinterpret_cast<uint64_t*>(self + kAccumTicks) =
        *reinterpret_cast<const uint64_t*>(other + kAccumTicks);
    return self;
}

// ============================================================================
// ??1AnimatedImageElement@UI@@UEAA@XZ   0x1800cb3a0
// ============================================================================
// Own vtable, destroy the frame array, re-stamp the BASE (ImageElement) vtable,
// then ~Element. The intermediate stamp matters: ~Element dispatches nothing,
// but the object is momentarily an ImageElement and retail says so.
extern "C" void RtUi_AnimatedImageElement_Dtor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    P(self, 0x00) = VtAnimatedImageElement();
    RtAddr<ArrayDtorFn>(0x180037310ull)(self + kFrames);
    P(self, 0x00) = VtImageElementBase();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(self);
}

// ============================================================================
// Registration. Slots 4/5/6 are NOT registered here — they are ImageElement's
// addresses and imageelement.cpp owns them.
// ============================================================================
int RtUiAnimatedImageElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@AnimatedImageElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_AnimatedImageElement_TypeName);   ++n;
    RegisterRuntimeAddr(0x180036ab0ull, (void*)&RtUi_AnimatedImageElement_TypeName);

    RegisterRuntimeExport("?Type@AnimatedImageElement@UI@@UEBAIXZ",
                          (void*)&RtUi_AnimatedImageElement_Type);       ++n;
    RegisterRuntimeAddr(0x180036a90ull, (void*)&RtUi_AnimatedImageElement_Type);

    RegisterRuntimeExport(
        "?Update@AnimatedImageElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_AnimatedImageElement_Update);                        ++n;
    RegisterRuntimeAddr(0x1800cb290ull, (void*)&RtUi_AnimatedImageElement_Update);

    RegisterRuntimeExport("??0AnimatedImageElement@UI@@QEAA@XZ",
                          (void*)&RtUi_AnimatedImageElement_Ctor);        ++n;
    RegisterRuntimeAddr(0x1800cb3f0ull, (void*)&RtUi_AnimatedImageElement_Ctor);

    RegisterRuntimeExport("??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_AnimatedImageElement_CopyCtor);    ++n;
    RegisterRuntimeAddr(0x180036c30ull, (void*)&RtUi_AnimatedImageElement_CopyCtor);

    RegisterRuntimeExport("??1AnimatedImageElement@UI@@UEAA@XZ",
                          (void*)&RtUi_AnimatedImageElement_Dtor);        ++n;
    RegisterRuntimeAddr(0x1800cb3a0ull, (void*)&RtUi_AnimatedImageElement_Dtor);
    return n;
}
