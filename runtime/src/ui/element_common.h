#pragma once
// ============================================================================
// runtime/src/ui/element_common.h — the four sequences every UI::*Element body
// inlines, transcribed ONCE
// ============================================================================
//
// 2026-09-12, vtable batch 3 (the first half of the `UI::*Element` widget
// runtime: AnimatedImage, Border, Button, Check, ClipRegion, Combo, Container,
// DocumentText, Edit, Graph, Group, Image, Label).
//
// Retail compiles `UI::Element`'s small helpers INTO every widget body, so the
// same four sequences appear verbatim in a dozen `ComputeSize` / `ComputeLayout`
// / `Draw` implementations. They are written once here rather than a dozen
// times, because a transcription error in one copy would be invisible against
// the other eleven. Each helper cites the instruction range of ONE of the
// copies it was read from; every other copy was diffed against it.
//
// WHAT IS *NOT* HERE, deliberately:
//   * anything that differs between widgets. `GroupElement::ComputeLayout`
//     (0x180118050) uses a DIFFERENT alignment sequence — it stretches
//     unconditionally and never consults flag bits 2/3 — so it is written out
//     in groupelement.cpp instead of being bent into `ElementComputeAlignedRect`.
//   * `ContainerElement`'s two ONE-AXIS copies of the padded-size sequence:
//     they read only the width (or only the height) and the second half of
//     `PaddedSizeInline` would be a read retail does not perform. They are
//     written out in containerelement.cpp.
//
// NAME NOTE: a SECOND `runtime/src/ui/widget_common.h` exists, written
// independently by the elem_B batch for the other fourteen widget classes.
// The two headers overlap heavily but not exactly, so this one keeps its own
// file name and its own namespace (`rtuielem`, not `rtui`) rather than being
// merged by hand into a single header that neither batch has re-verified.
// Folding them is a readability job for after cutover, not a merge-time one.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
// ENDGAME B.3.6 block A: the real `UI::Element` class, so the two inline
// helpers below dispatch slots 4/5 as member calls instead of raw slot reads.
#include "../../types/ui/Element.h"
#include "../../types/video/Device.h"   // ENDGAME B.3.9 SS1: Video::Device::_singleton
#include "../../types/system/MemoryAllocator.h"   // B3R RT-UI: MemAlloc / MemFree, direct

namespace rtuielem {

// (B3X W0: `VTableOf`, `VSlot<Fn>(o, INDEX)`, the `kSlot*` Element slot
// indices and the four slot typedefs are gone -- every UI::Element slot is
// dispatched by name, `static_cast<UI::Element*>(e)->ComputeSize()`.)

// ---- UI::Element field offsets, all proven in runtime/types/ui/Element.h ---
constexpr int kElemName          = 0x10;
constexpr int kElemLeftPad       = 0x18;
constexpr int kElemRightPad      = 0x1c;
constexpr int kElemTopPad        = 0x20;
constexpr int kElemBottomPad     = 0x24;
constexpr int kElemMinWidth      = 0x28;
constexpr int kElemMinHeight     = 0x2c;
constexpr int kElemAlignment     = 0x30;
constexpr int kElemFlags         = 0x34;
constexpr int kElemColor         = 0x38;
constexpr int kElemLayoutWidth   = 0x60;
constexpr int kElemLayoutHeight  = 0x64;
constexpr int kElemLayoutLeft    = 0x68;
constexpr int kElemLayoutTop     = 0x6c;
constexpr int kElemLayoutRight   = 0x70;
constexpr int kElemLayoutBottom  = 0x74;
constexpr int kElemElementId     = 0x88;
constexpr int kElemDialogParent  = 0x90;
constexpr int kElemParent        = 0x98;
constexpr int kElemChildData     = 0xa0;
constexpr int kElemChildCount    = 0xa8;
constexpr int kElemBorder        = 0xb0;

constexpr uint32_t kFlagHidden        = 0x00000001;
constexpr uint32_t kFlagAlignArg1     = 0x00000004;
constexpr uint32_t kFlagAlignArg2     = 0x00000008;
constexpr uint32_t kFlagScreenH       = 0x00000010;
constexpr uint32_t kFlagScreenW       = 0x00000020;
constexpr uint32_t kFlagDimColor      = 0x00040000;
constexpr uint32_t kFlagDisabledMask  = 0x01800000;   // bits 23|24
constexpr uint32_t kFlagNeedsSizeCalc = 0x08000000;

inline int32_t&  I32(void* p, int off) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(p) + off); }
inline uint32_t& U32(void* p, int off) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(p) + off); }
inline int32_t   I32c(const void* p, int off) { return *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(p) + off); }
inline uint32_t  U32c(const void* p, int off) { return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(p) + off); }
inline void*&    P(void* p, int off) { return *reinterpret_cast<void**>(static_cast<uint8_t*>(p) + off); }
inline void*     Pc(const void* p, int off) { return *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(p) + off); }
inline void*     Child(const void* self, int index) {
    return reinterpret_cast<void* const*>(Pc(self, kElemChildData))[index];
}

// ---------------------------------------------------------------------------
// `?SetNeedsSizeCalc@Element@UI@@` (0x180035470), inlined everywhere.
// `OR dword [RCX+0x34],0x8000000 ; RCX = [RCX+0x98]` until the chain ends.
// It is a DO-WHILE: `this` is always marked, even when `this` has no parent.
// ---------------------------------------------------------------------------
inline void SetNeedsSizeCalc(void* self) {
    void* n = self;
    do {
        U32(n, kElemFlags) |= kFlagNeedsSizeCalc;
        n = P(n, kElemParent);
    } while (n != nullptr);
}

// ---------------------------------------------------------------------------
// `?SetFlagsOnOff@Element@UI@@` inlined: write the new flag word, and run the
// dirty walk ONLY IF BIT 0 CHANGED (`XOR old,new ; TEST AL,1`). Every widget
// Update that touches +0x34 uses this exact shape; getting the test wrong
// (walking unconditionally) is invisible in a screenshot and costs a parent
// walk per element per frame.
// ---------------------------------------------------------------------------
inline void StoreFlags(void* self, uint32_t newFlags) {
    const uint32_t old = U32(self, kElemFlags);
    U32(self, kElemFlags) = newFlags;
    if (((old ^ newFlags) & 1u) != 0) SetNeedsSizeCalc(self);
}

// ---------------------------------------------------------------------------
// `Video::Device::_singleton` (exported data, retail 0x1801fe7a8). The screen
// size lives at +0x04 / +0x08 of the pointed-to Device.
// ---------------------------------------------------------------------------
// ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh every
// call -- InlineGetPaddedSize reaches this from member bodies that run on the
// LOADER THREAD (B9-10: no lazy cache).
inline void* VideoDeviceSingleton() {
    return ::Video::Device::_singleton;
}

// ---------------------------------------------------------------------------
// UI::Sprite (runtime/types/ui/ImageElement.h): {SpriteSheet* +0x00,
// int index +0x08, int pad +0x0c}. A sprite's pixel size is read out of the
// sheet's frame table: `sheet->[+0x20] + index*0x38`, width at +0x1c and
// height at +0x20 of the entry.
//
// Read from `?ComputeSize@ImageElement@UI@@` 0x180118650:
//     iVar2 = *(int*)(idx*0x38 + 0x1c + *(longlong*)(*(longlong*)(this+0xc0)+0x20))
// A NEGATIVE index yields 0 AND DOES NOT TOUCH THE SHEET POINTER — retail
// tests the index first, so a sprite with index -1 and a null sheet is legal.
// ---------------------------------------------------------------------------
inline int SpriteW(const void* sprite) {
    const int32_t idx = I32c(sprite, 0x08);
    if (idx < 0) return 0;
    const uint8_t* sheet = static_cast<const uint8_t*>(Pc(sprite, 0x00));
    const uint8_t* frames = static_cast<const uint8_t*>(Pc(sheet, 0x20));
    return *reinterpret_cast<const int32_t*>(frames + static_cast<int64_t>(idx) * 0x38 + 0x1c);
}
inline int SpriteH(const void* sprite) {
    const int32_t idx = I32c(sprite, 0x08);
    if (idx < 0) return 0;
    const uint8_t* sheet = static_cast<const uint8_t*>(Pc(sprite, 0x00));
    const uint8_t* frames = static_cast<const uint8_t*>(Pc(sheet, 0x20));
    return *reinterpret_cast<const int32_t*>(frames + static_cast<int64_t>(idx) * 0x38 + 0x20);
}

// ---------------------------------------------------------------------------
// `?ComputeColor@Element@UI@@` (0x180035490), inlined by every Draw.
// Read from 0x180118886..0x1801188d6 (ImageElement::Draw):
//
//    ECX = (color >> 8) & 0xffff0000 ; ECX *= alpha ; ECX ^= color
//    ECX &= 0xffffff
//    R8D = (color >> 8) & 0xff0000   ; R8D *= alpha ; ECX ^= R8D
//
// The two masks 0xffff0000 and 0xff0000 select the SAME bits once `color >> 8`
// has already cleared the top byte, so both products are `alphaByte * alpha`
// shifted into bits 16.., and the result is
// `(color & 0xffffff) | (((alphaByte*alpha) >> 8) << 24)`. The shipped bit
// juggling is transcribed rather than the simplification, because the
// simplification is only equal while the top byte of `color >> 8` is zero and
// nothing in this file enforces that.
// ---------------------------------------------------------------------------
inline uint32_t ComputeColor(uint32_t color, uint8_t alpha) {
    uint32_t c = ((color >> 8) & 0xffff0000u) * static_cast<uint32_t>(alpha);
    c ^= color;
    c &= 0x00ffffffu;
    const uint32_t hi = ((color >> 8) & 0x00ff0000u) * static_cast<uint32_t>(alpha);
    return c ^ hi;
}

// The `TEST [this+0x34],0x40000 / SHR alpha,1` prologue every Draw runs.
inline uint8_t DimAlpha(const void* self, uint8_t alpha) {
    return (U32c(self, kElemFlags) & kFlagDimColor) ? static_cast<uint8_t>(alpha >> 1) : alpha;
}

// ---------------------------------------------------------------------------
// `?GetPaddedSize@Element@UI@@` (0x180115f10), inlined.
// Transcribed from `?ComputeSize@ButtonElement@UI@@` 0x180110d5a..0x180110e0a;
// identical to the already-ported out-of-line body in runtime/src/ui/element.cpp.
// ---------------------------------------------------------------------------
inline void PaddedSizeInline(void* e, int* outW, int* outH) {
    reinterpret_cast<UI::Element*>(e)->ComputeSize();   // B.3.6 bridge — slot 4, virtual

    *outW = I32(e, kElemLayoutWidth)  + I32(e, kElemRightPad)  + I32(e, kElemLeftPad);
    *outH = I32(e, kElemLayoutHeight) + I32(e, kElemBottomPad) + I32(e, kElemTopPad);
    if (*outW < I32(e, kElemMinWidth))  *outW = I32(e, kElemMinWidth);
    if (*outH < I32(e, kElemMinHeight)) *outH = I32(e, kElemMinHeight);

    const uint32_t f = U32(e, kElemFlags);
    if ((f & (kFlagScreenH | kFlagScreenW)) != 0) {
        void* root = e;
        while (P(root, kElemParent) != nullptr) root = P(root, kElemParent);
        const uint8_t* dev = static_cast<const uint8_t*>(VideoDeviceSingleton());
        if (f & kFlagScreenW) {
            const uint8_t* owner = static_cast<const uint8_t*>(Pc(P(root, kElemDialogParent), 0x10));
            *outW = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x04)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
        }
        if (f & kFlagScreenH) {
            const uint8_t* owner = static_cast<const uint8_t*>(Pc(P(root, kElemDialogParent), 0x10));
            *outH = static_cast<int>(static_cast<float>(*reinterpret_cast<const int32_t*>(dev + 0x08)) *
                                     *reinterpret_cast<const float*>(owner + 0xdc));
        }
    }
}

// ---------------------------------------------------------------------------
// The standard `ComputeLayout` body: the border forward, `ComputeSize` through
// virtual slot 4, and the inlined member `?GetAlignedPosition@Element@UI@@`
// (0x180115ff0 -> the static 0x1801158e0), writing the four layout fields.
//
// Transcribed from `?ComputeLayout@ImageElement@UI@@` 0x1801186f0 and diffed
// instruction for instruction against ButtonElement (0x180110e70),
// ClipRegionElement (0x1800cba00), ContainerElement (0x1800cc620),
// DocumentTextElement (0x1800cda00), EditElement (0x1800d0ab0),
// GraphElement (0x1800d16b0) and LabelElement (0x1800d2330). The eight differ
// only in the ORDER of the four stores at the end, which is register-allocation
// noise: the values are identical and the fields do not alias.
//
// TWO FAITHFULNESS TRAPS, the same two runtime/src/ui/element.cpp names for the
// out-of-line copy:
//   * the centring halves are ARITHMETIC shifts (`SAR`), not `/ 2`;
//   * `sw`/`sh` are read, possibly REPLACED by the stretch, then read again by
//     the placement — the order is load-bearing.
//
// The border call forwards the ORIGINAL x/y/w/h (EDX/R8D/R9D are untouched at
// 0x18011872a and h is re-spilled to the outgoing slot), not the clamped ones.
// ---------------------------------------------------------------------------
inline void ElementComputeAlignedRect(void* self, int x, int y, int w, int h) {
    void* border = P(self, kElemBorder);
    if (border != nullptr)
        reinterpret_cast<UI::Element*>(border)->ComputeLayout(x, y, w, h);   // B.3.6 bridge — slot 5

    reinterpret_cast<UI::Element*>(self)->ComputeSize();   // B.3.6 bridge — slot 4

    int sw = I32(self, kElemLayoutWidth);
    int sh = I32(self, kElemLayoutHeight);
    const int align = I32(self, kElemAlignment);

    if (w < sw + x) x = w - sw;
    if (h < sh + y) y = h - sh;

    int cx = x + I32(self, kElemLeftPad);
    int cy = y + I32(self, kElemTopPad);
    const int availW = (w - I32(self, kElemRightPad))  - cx;
    const int availH = (h - I32(self, kElemBottomPad)) - cy;

    const uint32_t flags = U32(self, kElemFlags);
    if ((flags & kFlagAlignArg1) != 0) sw = availW;
    if ((flags & kFlagAlignArg2) != 0) sh = availH;

    switch (align) {
    case 1: case 4: case 7: cx = ((availW >> 1) - (sw >> 1)) + cx;      break;
    case 2: case 5: case 8: cx = (w - I32(self, kElemRightPad)) - sw;   break;
    default: break;
    }
    if (align > 2) {
        if (align < 6)      cy = ((availH >> 1) - (sh >> 1)) + cy;
        else if (align < 9) cy = (h - I32(self, kElemBottomPad)) - sh;
    }

    I32(self, kElemLayoutLeft)   = cx;
    I32(self, kElemLayoutTop)    = cy;
    I32(self, kElemLayoutRight)  = cx + sw;
    I32(self, kElemLayoutBottom) = cy + sh;
}

// ---------------------------------------------------------------------------
// `?Type@<Class>@UI@@` / `?TypeName@<Class>@UI@@`: the FIRST DWORD of the
// class's `Object::BaseTypeInfo` static and the `const char*` at its +0x08.
// Retail loads them as two separate globals (`?_typeInfo@ImageElement@UI@@` at
// 0x1801fac48 and the label Ghidra prints as `PTR_s_ImageElement_1801fac50`,
// which is that static + 8) — the same two fields runtime/types/object/TypeInfo.h
// pins, and the same shape runtime/src/ui/groupdescription.cpp documents.
// ---------------------------------------------------------------------------
inline uint32_t TypeIdOf(const uint8_t* typeInfo) {
    uint32_t v = 0; __builtin_memcpy(&v, typeInfo, sizeof v); return v;
}
inline const char* TypeNameOf(const uint8_t* typeInfo) {
    const char* v = nullptr; __builtin_memcpy(&v, typeInfo + 8, sizeof v); return v;
}

}  // namespace rtuielem

// (ENDGAME B.3.9 TI-UI: RTELEM_TYPE_SLOTS / RTELEM_TYPE_BODIES, the by-name
// `?_typeInfo@<NAME>@UI@@` / `??_7<NAME>@UI@@` accessors and the Type/TypeName
// slot bodies over them, had no expansion left; deleted -- every class carries
// its own `DECLARE_TYPE` record.)
