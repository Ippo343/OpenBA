// ============================================================================
// runtime/src/ui/checkelement.cpp — UI::CheckElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7CheckElement@UI@@6B@` (retail 0x180157260),
// a UI::ButtonElement subclass that adds one bool and one extra child.
//
//   slot  method                       retail       where
//   ----  ---------------------------  -----------  --------------------------
//    0    vector deleting dtor         0x180048d80  deleting_dtor.cpp row
//    1    TypeName                     0x180048a80  here
//    2    Type                         0x180048a60  here
//    4    ComputeSize                  0x180111740  here
//    5    ComputeLayout                0x1801117b0  here
//    6    Draw                         0x180111800  here
//    7    Update                       0x180111870  here
//    8    ButtonElement::Enable        0x180110cc0  buttonelement.cpp
//
// plus ??0CheckElement@UI@@QEAA@XZ         0x1801116a0
//      ??0CheckElement@UI@@QEAA@AEBV01@@Z  0x180048ca0
//      ??1CheckElement@UI@@UEAA@XZ         0x180111700
//
// LAYOUT — 0xe0 bytes (`MOV EDX,0xe0` at 0x180048db2): ButtonElement's 0xd8,
// then ONE BYTE at +0xd8, the check state, and seven bytes of tail padding.
// `_children[5]` is the tick mark: the only child index past ButtonElement's
// five, drawn only while +0xd8 is set.
// ============================================================================
#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kChecked   = 0xd8;   // uint8_t
constexpr int kChildTick = 5;
constexpr int kKeyIndex  = 0xc0;   // inherited from ButtonElement

RTUI_TYPE_SLOTS(CheckElement)
void* VtButtonElementBase() { return RT_DATA_SLOT(void*, "??_7ButtonElement@UI@@6B@"); }

using ElementCtorFn      = void* (*)(void* self);
using ButtonCopyCtorFn   = void* (*)(void* self, const void* other);
using ElementDtorFn      = void  (*)(void* self);
using ButtonComputeSize  = void  (*)(void* self);
using ButtonDrawFn       = void  (*)(const void* self, void* ctx, uint8_t alpha);
using ButtonUpdateFn     = void  (*)(void* self, const void* time, void* ui,
                                     void** dlg, const void* state);
using KeyPressedFn       = bool  (*)(void* keyMap, int a, int b);

constexpr int kStateMouseUp = 0x04;
constexpr int kStateKeyMap  = 0x28;

}  // namespace

RTUI_TYPE_BODIES(CheckElement)

// ============================================================================
// ?ComputeSize@CheckElement@UI@@UEAAXXZ   0x180111740
// ============================================================================
// Run the button's own sizing, then widen to fit the tick.
//
// The tick's contribution is its RAW `_layoutWidth`/`_layoutHeight` — NOT its
// padded size. This is the one place in the batch where a child's size is
// taken without the padding, and it is what 0x180111785..0x1801117a3 does.
//
// Bit 27 is cleared TWICE on this path: once by ButtonElement::ComputeSize and
// again at the end here. Reproduced; the second clear is a no-op unless the
// tick's own ComputeSize dirtied us back, which it can (it walks parents).
extern "C" void RtUi_CheckElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    RtAddr<ButtonComputeSize>(0x180110d10ull)(self);

    void* tick = Child(self, kChildTick);
    if (tick != nullptr) {
        VSlot<ComputeSizeFn>(tick, kSlotComputeSize)(tick);
        const int w = I32(tick, kElemLayoutWidth);
        const int h = I32(tick, kElemLayoutHeight);
        if (I32(self, kElemLayoutWidth)  < w) I32(self, kElemLayoutWidth)  = w;
        if (I32(self, kElemLayoutHeight) < h) I32(self, kElemLayoutHeight) = h;
    }

    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@CheckElement@UI@@UEAAXHHHH@Z   0x1801117b0
// ============================================================================
// ButtonElement's layout, then the tick over the same rect. Retail's tail is a
// JUMP into the tick's slot 5 (0x1801117f0), which is why Ghidra loses the
// fourth argument; the disassembly shows all four plus the stack slot.
extern "C" void RtUi_CheckElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ComputeLayoutFn>(0x180110e70ull)(self, x, y, w, h);

    void* tick = Child(self, kChildTick);
    if (tick != nullptr) {
        VSlot<ComputeLayoutFn>(tick, kSlotComputeLayout)(
            tick, I32(self, kElemLayoutLeft), I32(self, kElemLayoutTop),
            I32(self, kElemLayoutRight), I32(self, kElemLayoutBottom));
    }
}

// ============================================================================
// ?Draw@CheckElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x180111800
// ============================================================================
// *** THE ALPHA IS HALVED TWICE WHEN THE DIM FLAG IS SET. *** This body halves
// it BEFORE calling ButtonElement::Draw, which tests the same bit on the same
// object and halves it again. That is what the shipped bytes do (0x180111800
// tests +0x34 and shifts, then 0x180111020 tests +0x34 and shifts), so a
// dimmed CheckElement is drawn at a quarter alpha and its tick at a quarter of
// that. Not a transcription slip — do not "fix" it.
extern "C" void RtUi_CheckElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);
    alpha = DimAlpha(self, alpha);

    RtAddr<ButtonDrawFn>(0x180111020ull)(self, ctx, alpha);

    if (self[kChecked] != 0) {
        void* tick = Child(self, kChildTick);
        if (tick != nullptr) VSlot<DrawFn>(tick, kSlotDraw)(tick, ctx, alpha);
    }
}

// ============================================================================
// ?Update@CheckElement@UI@@   0x180111870
// ============================================================================
// Toggle on a click or on the shortcut key, then run ButtonElement::Update —
// which is what actually raises the activation event. The toggle happens
// FIRST, so the event a listener sees already reflects the new check state.
extern "C" void RtUi_CheckElement_Update(void* selfv, const void* time, void* ui,
                                         void** dlg, const void* statev) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* st = static_cast<const uint8_t*>(statev);

    const uint32_t flags = U32(self, kElemFlags);
    bool toggle = false;
    if ((flags & kFlagDisabledMask) == 0) {
        if (st[kStateMouseUp] != 0 && ((flags >> 17) & 1u) != 0 &&
            ((flags >> 21) & 1u) != 0) {
            toggle = true;
        } else {
            const int32_t key = I32(self, kKeyIndex);
            if (key >= 0 &&
                RtAddr<KeyPressedFn>(0x1800a7a80ull)(Pc(st, kStateKeyMap), 0, key)) {
                toggle = true;
            }
        }
    }
    if (toggle) self[kChecked] = (self[kChecked] == 0) ? 1 : 0;

    void* dlgCopy = *dlg;
    RtAddr<ButtonUpdateFn>(0x1801110b0ull)(self, time, ui, &dlgCopy, st);
}

// ============================================================================
// Constructors / destructor
// ============================================================================
// The default constructor INLINES ButtonElement's body (Element ctor, the base
// vtable stamp, +0xc0 = -1, +0xc4 = 0) rather than calling ??0ButtonElement,
// then stamps its own vtable and clears +0xd8. The copy constructor DOES call
// ??0ButtonElement. Both spellings are retail's and are kept as they are.
extern "C" void* RtUi_CheckElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtButtonElementBase();
    I32(self, 0xc0) = -1;
    I32(self, 0xc4) = 0;
    P(self, 0x00) = VtCheckElement();
    self[kChecked] = 0;
    return self;
}

extern "C" void* RtUi_CheckElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ButtonCopyCtorFn>(0x180048460ull)(self, other);
    P(self, 0x00) = VtCheckElement();
    self[kChecked] = other[kChecked];
    return self;
}

// ??1CheckElement@UI@@UEAA@XZ  0x180111700 — own vtable, then the BASE vtable
// over it (dead, nothing reads it), then ~Element. Both stamps reproduced.
extern "C" void RtUi_CheckElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtCheckElement();
    P(selfv, 0x00) = VtButtonElementBase();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration — slot 8 is ButtonElement's address and is NOT re-registered.
// ============================================================================
int RtUiCheckElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@CheckElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_CheckElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180048a80ull, (void*)&RtUi_CheckElement_TypeName);

    RegisterRuntimeExport("?Type@CheckElement@UI@@UEBAIXZ",
                          (void*)&RtUi_CheckElement_Type);          ++n;
    RegisterRuntimeAddr(0x180048a60ull, (void*)&RtUi_CheckElement_Type);

    RegisterRuntimeExport("?ComputeSize@CheckElement@UI@@UEAAXXZ",
                          (void*)&RtUi_CheckElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x180111740ull, (void*)&RtUi_CheckElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@CheckElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_CheckElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1801117b0ull, (void*)&RtUi_CheckElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@CheckElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_CheckElement_Draw);          ++n;
    RegisterRuntimeAddr(0x180111800ull, (void*)&RtUi_CheckElement_Draw);

    RegisterRuntimeExport(
        "?Update@CheckElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_CheckElement_Update);                          ++n;
    RegisterRuntimeAddr(0x180111870ull, (void*)&RtUi_CheckElement_Update);

    RegisterRuntimeExport("??0CheckElement@UI@@QEAA@XZ",
                          (void*)&RtUi_CheckElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1801116a0ull, (void*)&RtUi_CheckElement_Ctor);

    RegisterRuntimeExport("??0CheckElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_CheckElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180048ca0ull, (void*)&RtUi_CheckElement_CopyCtor);

    RegisterRuntimeExport("??1CheckElement@UI@@UEAA@XZ",
                          (void*)&RtUi_CheckElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180111700ull, (void*)&RtUi_CheckElement_Dtor);
    return n;
}
