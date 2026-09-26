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
// plus ??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z 0x180036c30 (copy ctor, a DOOR)
//      ??1AnimatedImageElement@UI@@UEAA@XZ        0x1800cb3a0 (a DOOR)
// `??0AnimatedImageElement@UI@@QEAA@XZ` (0x1800cb3f0) is factory_ui1.cpp's.
//
// LAYOUT: runtime/types/ui/AnimatedImageElement.h — ImageElement (0xE0) plus
// `_frames` (Array<Sprite>, +0xE0), `_framesPerSecond` (+0xF0, ctor 30) and
// `_accumTicks` (+0xF8); sizeof 0x100. (The file-local constants this file
// carried until B.3.6 moved there, RULE B6-8.)
//
// ENDGAME B.3.6 block C2: slots 1/2/7 are MEMBERS of
// `UI::AnimatedImageElement`; the registered doors are one-line QUALIFIED
// delegations. Update's base call (by address 0x180116270 before) is the
// qualified `ImageElement::Update`, i.e. `Element::Update`.
// ============================================================================
#include "element_common.h"
#include "../../types/ui/AnimatedImageElement.h"

using namespace rtuielem;

namespace {

constexpr int kFrames      = 0xe0;   // Array<Sprite>::_data
constexpr int kFrameCount  = 0xe8;   // Array<Sprite>::_size
constexpr int kFps         = 0xf0;
constexpr int kAccumTicks  = 0xf8;
static_assert(kFrames     == offsetof(UI::AnimatedImageElement, _frames), "");
static_assert(kFps        == offsetof(UI::AnimatedImageElement, _framesPerSecond), "");
static_assert(kAccumTicks == offsetof(UI::AnimatedImageElement, _accumTicks), "");

// RULE B6-5: namespace-scope atomic caches for the member bodies.
std::atomic<void*> g_animatedImageElementTypeInfo{nullptr};
std::atomic<void*> g_secondsPerTick{nullptr};
const uint8_t* AnimatedImageElementTypeInfoAtomic() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_animatedImageElementTypeInfo,
        "?_typeInfo@AnimatedImageElement@UI@@0VBaseTypeInfo@Object@@A"));
}
// `System::Timer::_secondsPerTick`, an exported DOUBLE (ordinal 4797, read at
// 0x1800cb2d2 as `MOVSD XMM3,[0x1801fe728]`). The same slot
// runtime/src/audio/audio_scene_update.cpp reads.
double SecondsPerTick() {
    const double* p = static_cast<const double*>(
        AtomicExport(g_secondsPerTick, "?_secondsPerTick@Timer@System@@0NA"));
    return p ? *p : 0.0;
}

// Door-only.
void* VtAnimatedImageElement() { return RT_DATA_SLOT(void*, "??_7AnimatedImageElement@UI@@6B@"); }
void* VtImageElementBase()     { return RT_DATA_SLOT(void*, "??_7ImageElement@UI@@6B@"); }

using ArrayDtorFn     = void* (*)(void* array);
using ArrayCopyFn     = void* (*)(void* dst, const void* src);
using ImageCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn   = void (*)(void* self);

}  // namespace

namespace UI {

uint32_t AnimatedImageElement::Type() const {                   // 0x180036a90
    return rtuielem::TypeIdOf(AnimatedImageElementTypeInfoAtomic());
}
// The key function: GCC emits `vtable for UI::AnimatedImageElement` here.
const char* AnimatedImageElement::TypeName() const {            // 0x180036ab0
    return rtuielem::TypeNameOf(AnimatedImageElementTypeInfoAtomic());
}

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
//   if (sprite.sheet != _sprite.sheet || sprite.index != _sprite.index)
//        SetNeedsSizeCalc(this)
//   _sprite = sprite                                  ; the same MOVUPS
//
// *** TWO THINGS A TIDIED PORT GETS WRONG. ***
//
//  1. `frameCount` is read TWICE from memory (0x1800cb2da for the period and
//     0x1800cb318 for the frame index), both times after Element::Update has
//     returned. Caching it in one local would assert that the base Update
//     cannot change the frame list.
//  2. THE PARENT WALK CLOBBERS THE `this` REGISTER. Retail computes
//     `RCX = this + 0xc0` BEFORE the walk (0x1800cb340), walks through RBX
//     — which ends at nullptr — and then stores the sprite through the SAVED
//     RCX. The walk here uses its own cursor.
//
// The comparison is `sheet` (a full qword) and `index` (a dword) only; the
// 4 pad bytes at +0xcc are copied but never compared.
//
// The InternalPtr<DialogInstance> parameter arrives BY HIDDEN REFERENCE (RULE
// B6-11; `MOV RAX,[R9]` at 0x1800cb29a); retail copies the pointed-to value
// into its own home slot and forwards the address of THAT. The base call is
// QUALIFIED: this object wears the minted table, whose slot 7 is this body.
void AnimatedImageElement::Update(const System::Time& time, UserInterface* ui,
                                  Object::InternalPtr<DialogInstance>* dialog,
                                  const UpdateState& state) {
    Object::InternalPtr<DialogInstance> dlgCopy = *dialog;     // 1800cb2a5
    ImageElement::Update(time, ui, &dlgCopy, state);          // 1800cb2b7  Element::Update

    const double spt = SecondsPerTick();

    _accumTicks += *reinterpret_cast<const int64_t*>(&time);   // 1800cb2c2  System::Time's ticks

    float elapsed = static_cast<float>(static_cast<double>(_accumTicks) * spt);
    const float period = static_cast<float>(_frames._size) /   // 1800cb2da
                         static_cast<float>(_framesPerSecond);

    if (elapsed >= period) {
        elapsed = elapsed - period;
        _accumTicks = static_cast<int64_t>(static_cast<double>(elapsed) / spt);
    }

    const int frame = static_cast<int>((elapsed / period) *
                                       static_cast<float>(_frames._size));   // 1800cb318

    const Sprite next = _frames._data[frame];                  // 1800cb33c  MOVUPS

    if (_sprite.spriteSheet != next.spriteSheet ||
        _sprite.spriteIndex != next.spriteIndex) {
        SetNeedsSizeCalc();                                    // 1800cb370..1800cb384
    }
    _sprite = next;                                            // 1800cb386  MOVUPS
}

}  // namespace UI

// ============================================================================
// The DOORS — each a one-line QUALIFIED delegation.
// ============================================================================
extern "C" uint32_t RtUi_AnimatedImageElement_Type(const void* self) {
    return reinterpret_cast<const UI::AnimatedImageElement*>(self)                          // B.3.6 bridge
        ->UI::AnimatedImageElement::Type();
}
extern "C" const char* RtUi_AnimatedImageElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::AnimatedImageElement*>(self)                          // B.3.6 bridge
        ->UI::AnimatedImageElement::TypeName();
}
extern "C" void RtUi_AnimatedImageElement_Update(void* selfv, const void* time,
                                                 void* ui, void** dlg,
                                                 const void* state) {
    reinterpret_cast<UI::AnimatedImageElement*>(selfv)->UI::AnimatedImageElement::Update(   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),
        *static_cast<const UI::Element::UpdateState*>(state));
}

// ============================================================================
// ??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z   0x180036c30   — a CTOR DOOR
// ============================================================================
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
// ??1AnimatedImageElement@UI@@UEAA@XZ   0x1800cb3a0   — a DTOR DOOR (B4b-2)
// ============================================================================
// Own vtable, destroy the frame array, re-stamp the BASE (ImageElement) vtable,
// then ~Element.
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

    RegisterRuntimeExport("??0AnimatedImageElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_AnimatedImageElement_CopyCtor);    ++n;
    RegisterRuntimeAddr(0x180036c30ull, (void*)&RtUi_AnimatedImageElement_CopyCtor);

    RegisterRuntimeExport("??1AnimatedImageElement@UI@@UEAA@XZ",
                          (void*)&RtUi_AnimatedImageElement_Dtor);        ++n;
    RegisterRuntimeAddr(0x1800cb3a0ull, (void*)&RtUi_AnimatedImageElement_Dtor);
    return n;
}
