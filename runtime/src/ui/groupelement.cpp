// ============================================================================
// runtime/src/ui/groupelement.cpp — UI::GroupElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7GroupElement@UI@@6B@` (retail 0x180157130),
// the plain stacking container: no fields of its own, sized to the union of its
// visible children, and every child given the WHOLE group rectangle.
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x180046e10  deleting_dtor.cpp row
//    1    TypeName              0x180046bb0  here
//    2    Type                  0x180046b90  here
//    4    ComputeSize           0x180117f00  here
//    5    ComputeLayout         0x180118050  here
//    6    Draw                  0x1801181c0  here
//
// plus ??0GroupElement@UI@@QEAA@XZ         0x180117c80
//      ??0GroupElement@UI@@QEAA@AEBV01@@Z  0x180046d70
//      ??1GroupElement@UI@@UEAA@XZ         0x180117cc0
//
// Slots 4/5/6 are ALSO PageElement's and RadioElement's — both repeat these
// three addresses in their own tables (batch 4). Registered once, here.
//
// LAYOUT: 0xc0 bytes — `MOV EDX,0xc0` at 0x180046e42, i.e. exactly
// `sizeof(UI::Element)`. The class adds NO data, which is the third proof of
// Element's size after the two in runtime/types/ui/Element.h.
// ============================================================================
#include "element_common.h"

using namespace rtuielem;

namespace {

RTELEM_TYPE_SLOTS(GroupElement)

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);

}  // namespace

RTELEM_TYPE_BODIES(GroupElement)

// ============================================================================
// ?ComputeSize@GroupElement@UI@@UEAAXXZ   0x180117f00
// ============================================================================
// The union of every VISIBLE child's padded size. Hidden children (flag bit 0)
// contribute nothing — this is the one sizing loop in the batch that tests the
// flag, and it is why hiding a child in a group actually shrinks the group.
extern "C" void RtUi_GroupElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    *reinterpret_cast<uint64_t*>(self + kElemLayoutWidth) = 0;   // +0x60 and +0x64

    const int32_t count = I32(self, kElemChildCount);
    for (int32_t i = 0; i < count; ++i) {
        void* child = Child(self, i);
        if (child == nullptr) continue;
        if ((U32(child, kElemFlags) & kFlagHidden) != 0) continue;

        int w = 0, h = 0;
        PaddedSizeInline(child, &w, &h);
        if (I32(self, kElemLayoutWidth)  < w) I32(self, kElemLayoutWidth)  = w;
        if (I32(self, kElemLayoutHeight) < h) I32(self, kElemLayoutHeight) = h;
    }

    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@GroupElement@UI@@UEAAXHHHH@Z   0x180118050
// ============================================================================
// *** THIS IS *NOT* THE STANDARD ALIGNED BODY *** and must not be folded into
// `ElementComputeAlignedRect`. Three differences, all at 0x1801180a0..0x180118120:
//
//   * flag bits 2 and 3 are NOT consulted; the group ALWAYS takes the available
//     size, so `sw`/`sh` below are the available width/height unconditionally;
//   * there is consequently no `case 1/4/7` centring arm on the X axis — the
//     jump table has only the right-aligned cases;
//   * the Y test is `align > 2 && align > 5 && align < 9`, i.e. only the BOTTOM
//     row, with no middle-row arm.
//
// A group is a full-bleed container: aligning it does nothing but move a
// zero-slack box, which is why the compiler could fold the arms away.
extern "C" void RtUi_GroupElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    uint8_t* self = static_cast<uint8_t*>(selfv);

    void* border = P(self, kElemBorder);
    if (border != nullptr)
        VSlot<ComputeLayoutFn>(border, kSlotComputeLayout)(border, x, y, w, h);

    VSlot<ComputeSizeFn>(self, kSlotComputeSize)(self);

    const int align = I32(self, kElemAlignment);

    if (w < I32(self, kElemLayoutWidth)  + x) x = w - I32(self, kElemLayoutWidth);
    if (h < I32(self, kElemLayoutHeight) + y) y = h - I32(self, kElemLayoutHeight);

    int cx = x + I32(self, kElemLeftPad);
    int cy = y + I32(self, kElemTopPad);
    const int availW = (w - I32(self, kElemRightPad))  - cx;
    const int availH = (h - I32(self, kElemBottomPad)) - cy;

    switch (align) {
    case 2: case 5: case 8:
        cx = (w - I32(self, kElemRightPad)) - availW;
        break;
    default: break;
    }
    if (align > 2 && align > 5 && align < 9)
        cy = (h - I32(self, kElemBottomPad)) - availH;

    I32(self, kElemLayoutLeft)   = cx;
    I32(self, kElemLayoutRight)  = availW + cx;
    I32(self, kElemLayoutTop)    = cy;
    I32(self, kElemLayoutBottom) = availH + cy;

    const int32_t count = I32(self, kElemChildCount);
    for (int32_t i = 0; i < count; ++i) {
        void* child = Child(self, i);
        if (child == nullptr) continue;
        if ((U32(child, kElemFlags) & kFlagHidden) != 0) continue;
        VSlot<ComputeLayoutFn>(child, kSlotComputeLayout)(
            child, I32(self, kElemLayoutLeft), I32(self, kElemLayoutTop),
            I32(self, kElemLayoutRight), I32(self, kElemLayoutBottom));
    }
}

// ============================================================================
// ?Draw@GroupElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1801181c0
// ============================================================================
extern "C" void RtUi_GroupElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    alpha = DimAlpha(self, alpha);

    const int32_t count = I32c(self, kElemChildCount);
    for (int32_t i = 0; i < count; ++i) {
        void* child = Child(self, i);
        if (child == nullptr) continue;
        if ((U32(child, kElemFlags) & kFlagHidden) != 0) continue;
        VSlot<DrawFn>(child, kSlotDraw)(child, ctx, alpha);
    }
}

// ============================================================================
// Constructors / destructor — all three are the base call plus one stamp.
// ============================================================================
extern "C" void* RtUi_GroupElement_Ctor(void* selfv) {
    RtAddr<ElementCtorFn>(0x180115e50ull)(selfv);
    P(selfv, 0x00) = VtGroupElement();
    return selfv;
}

extern "C" void* RtUi_GroupElement_CopyCtor(void* selfv, const void* otherv) {
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(selfv, otherv);
    P(selfv, 0x00) = VtGroupElement();
    return selfv;
}

extern "C" void RtUi_GroupElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtGroupElement();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiGroupElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@GroupElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_GroupElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180046bb0ull, (void*)&RtUi_GroupElement_TypeName);

    RegisterRuntimeExport("?Type@GroupElement@UI@@UEBAIXZ",
                          (void*)&RtUi_GroupElement_Type);          ++n;
    RegisterRuntimeAddr(0x180046b90ull, (void*)&RtUi_GroupElement_Type);

    RegisterRuntimeExport("?ComputeSize@GroupElement@UI@@UEAAXXZ",
                          (void*)&RtUi_GroupElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x180117f00ull, (void*)&RtUi_GroupElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@GroupElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_GroupElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x180118050ull, (void*)&RtUi_GroupElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@GroupElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_GroupElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1801181c0ull, (void*)&RtUi_GroupElement_Draw);

    RegisterRuntimeExport("??0GroupElement@UI@@QEAA@XZ",
                          (void*)&RtUi_GroupElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x180117c80ull, (void*)&RtUi_GroupElement_Ctor);

    RegisterRuntimeExport("??0GroupElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_GroupElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180046d70ull, (void*)&RtUi_GroupElement_CopyCtor);

    RegisterRuntimeExport("??1GroupElement@UI@@UEAA@XZ",
                          (void*)&RtUi_GroupElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180117cc0ull, (void*)&RtUi_GroupElement_Dtor);
    return n;
}
