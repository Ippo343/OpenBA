// ============================================================================
// runtime/src/ui/buttonelement.cpp — UI::ButtonElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7ButtonElement@UI@@6B@` (retail 0x180157310).
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x1800485b0  deleting_dtor.cpp row
//    1    TypeName              0x180048260  here
//    2    Type                  0x180048240  here
//    4    ComputeSize           0x180110d10  here
//    5    ComputeLayout         0x180110e70  here
//    6    Draw                  0x180111020  here
//    7    Update                0x1801110b0  here
//    8    Enable                0x180110cc0  here
//
// plus ??0ButtonElement@UI@@QEAA@XZ         0x180110c40
//      ??0ButtonElement@UI@@QEAA@AEBV01@@Z  0x180048460
//      ??1ButtonElement@UI@@UEAA@XZ         0x180110c90
//
// Slots 4/5/6/8 are ALSO ComboElement's and slot 8 is ALSO CheckElement's — the
// two subclasses repeat these addresses in their own tables. They are
// registered once, here.
//
// ---------------------------------------------------------------------------
// LAYOUT — 0xd8 bytes (`MOV EDX,0xd8` at 0x1800485e2). Six dwords past
// UI::Element, and all six are NAMED BY USE, not guessed:
//
//   +0xc0 int32  keyboard shortcut index; -1 in the constructor, and the
//                argument `Input::KeyMap::Pressed` receives in Update.
//   +0xc4 int32  the CURRENT STATE, 0..3, and simultaneously the INDEX of the
//                child element drawn for it (Draw/ComputeLayout index
//                `_children[_state]`). 0 normal, 1 disabled, 2 hovered,
//                3 pressed — read off the four assignments at
//                0x1801111ce / 0x18011117e / 0x1801111bc / 0x1801111a6.
//   +0xc8 uint32 colour for state 0    (0x1801111c8)
//   +0xcc uint32 colour for state 1    (0x180111178, and the value
//                `?Enable@ButtonElement@` pushes into the label)
//   +0xd0 uint32 colour for state 2    (0x1801111b6)
//   +0xd4 uint32 colour for state 3    (0x1801111a0)
//
// The four colours are handed to `_children[4]`'s virtual `SetColor` (slot 10),
// which is what makes `_children[4]` the button's LABEL and `_children[0..3]`
// its four state backgrounds.
//
// *** NEITHER CONSTRUCTOR INITIALISES +0xc8..+0xd4. *** The default ctor writes
// only +0xc0 and +0xc4; the four colours come from ButtonDescription. Do not
// byte-compare a freshly constructed ButtonElement past +0xc8.
// ============================================================================
#include "element_common.h"

using namespace rtuielem;

namespace {

constexpr int kKeyIndex = 0xc0;
constexpr int kState    = 0xc4;
constexpr int kColor0   = 0xc8;
constexpr int kColor1   = 0xcc;
constexpr int kColor2   = 0xd0;
constexpr int kColor3   = 0xd4;

// `_children` indices this class dispatches on.
constexpr int kChildLabel = 4;

RTELEM_TYPE_SLOTS(ButtonElement)

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);
using ElemUpdateFn      = void  (*)(void* self, const void* time, void* ui,
                                    void** dlg, const void* state);
using ElemEnableFn      = void  (*)(void* self, bool enable);
using SetColorFn        = void  (*)(void* self, uint32_t color);
using PaddedSizeFn      = void  (*)(void* self, int* w, int* h);
using PlayFn            = void  (*)(void* ui, int soundId);
using KeyPressedFn      = bool  (*)(void* keyMap, int a, int b);
// Collection::Array<UI::UserInterface::Event>::SetSize(array, newSize, &fill)
using EventPushFn       = void  (*)(void* array, int newSize, const void* fill);

// UI::Element::UpdateState, the fifth Update parameter. Only the fields these
// bodies read are named; the struct is the DLL's and nothing here owns it.
constexpr int kStateMouseDown   = 0x01;   // byte
constexpr int kStateMouseUp     = 0x04;   // byte
constexpr int kStateMouseHeld   = 0x07;   // byte
constexpr int kStateMouseX      = 0x18;   // int32
constexpr int kStateMouseY      = 0x1c;   // int32
constexpr int kStateKeyMap      = 0x28;   // Input::KeyMap*

// UI::UserInterface: the pending-event array at +0x48 {data, size, capacity}
// (runtime/types/ui/UserInterface.h, `Collection::Array<Event> _events`).
constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

// UI::UserInterface::Event, 0x30 bytes (the stride UserInterface.h pins from
// CleanRemovedDialogs). The seven stores at 0x180111356..0x18011139e.
struct Event {
    void*    dialog;      // +0x00  the InternalPtr<DialogInstance> value
    void*    element;     // +0x08
    int32_t  kind;        // +0x10
    int32_t  elementId;   // +0x14
    int32_t  x;           // +0x18
    int32_t  y;           // +0x1c
    int32_t  index;       // +0x20
    int32_t  pad24;       // +0x24  NEVER WRITTEN by this path
    uint64_t tail;        // +0x28
};
static_assert(sizeof(Event) == 0x30, "UI event record stride");

constexpr int32_t kEventActivated = 0x12;

}  // namespace

RTELEM_TYPE_BODIES(ButtonElement)

// ============================================================================
// ?ComputeSize@ButtonElement@UI@@UEAAXXZ   0x180110d10
// ============================================================================
// The union of the label's padded size and the current state background's,
// floored by the element's own minimum.
//
// TWO CALLS, TWO SHAPES, AND THAT IS RETAIL: the LABEL goes through the
// out-of-line `?GetPaddedSize@Element@UI@@` (0x180115f10, already ported in
// element.cpp) writing straight into +0x60/+0x64; the STATE BACKGROUND has the
// same function INLINED and compared field by field. Collapsing the two into
// one helper would hide that the first writes `this` and the second does not.
//
// `_children[4]` and `_children[_state]` are both dereferenced WITHOUT a bounds
// check — a ButtonElement always has five children because ButtonDescription
// builds them.
extern "C" void RtUi_ButtonElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    *reinterpret_cast<uint64_t*>(self + kElemLayoutWidth) = 0;   // +0x60 and +0x64

    void* label = Child(self, kChildLabel);
    if (label != nullptr) {
        RtAddr<PaddedSizeFn>(0x180115f10ull)(
            label,
            reinterpret_cast<int*>(self + kElemLayoutWidth),
            reinterpret_cast<int*>(self + kElemLayoutHeight));
    }

    void* bg = Child(self, I32(self, kState));
    if (bg != nullptr) {
        int w = 0, h = 0;
        PaddedSizeInline(bg, &w, &h);
        if (I32(self, kElemLayoutWidth)  < w) I32(self, kElemLayoutWidth)  = w;
        if (I32(self, kElemLayoutHeight) < h) I32(self, kElemLayoutHeight) = h;
    }

    if (I32(self, kElemLayoutWidth)  < I32(self, kElemMinWidth))
        I32(self, kElemLayoutWidth)  = I32(self, kElemMinWidth);
    if (I32(self, kElemLayoutHeight) < I32(self, kElemMinHeight))
        I32(self, kElemLayoutHeight) = I32(self, kElemMinHeight);

    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@ButtonElement@UI@@UEAAXHHHH@Z   0x180110e70
// ============================================================================
// The standard aligned body, then the LABEL laid out over the computed rect
// (from the registers still holding it) and the STATE BACKGROUND laid out over
// the rect RE-READ FROM THE FIELDS. The two are the same numbers; the second
// re-read is retail's and is kept.
extern "C" void RtUi_ButtonElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    ElementComputeAlignedRect(self, x, y, w, h);

    void* label = Child(self, kChildLabel);
    if (label != nullptr) {
        VSlot<ComputeLayoutFn>(label, kSlotComputeLayout)(
            label, I32(self, kElemLayoutLeft), I32(self, kElemLayoutTop),
            I32(self, kElemLayoutRight), I32(self, kElemLayoutBottom));
    }

    void* bg = Child(self, I32(self, kState));
    if (bg != nullptr) {
        VSlot<ComputeLayoutFn>(bg, kSlotComputeLayout)(
            bg, I32(self, kElemLayoutLeft), I32(self, kElemLayoutTop),
            I32(self, kElemLayoutRight), I32(self, kElemLayoutBottom));
    }
}

// ============================================================================
// ?Draw@ButtonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x180111020
// ============================================================================
// Border, dim, then the STATE BACKGROUND and the LABEL — in that order, so the
// label paints on top.
extern "C" void RtUi_ButtonElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    alpha = DimAlpha(self, alpha);

    void* bg = Child(self, I32c(self, kState));
    if (bg != nullptr) VSlot<DrawFn>(bg, kSlotDraw)(bg, ctx, alpha);

    void* label = Child(self, kChildLabel);
    if (label != nullptr) VSlot<DrawFn>(label, kSlotDraw)(label, ctx, alpha);
}

// ============================================================================
// ?Update@ButtonElement@UI@@   0x1801110b0
// ============================================================================
// The whole button state machine. Reading order matters throughout: retail
// RE-READS `_flags` from memory between almost every step, because the dirty
// walk it runs in between writes to it.
//
//   Element::Update(this, time, ui, &dlgCopy, state)
//   if (_flags & hidden) return
//   prev = _state
//   _flags &= ~bit22                                 (was-activated, cleared)
//   if (state.mouseDown && (_flags & bit17))         bit17 == "mouse is over"
//        _flags |= bit21                             bit21 == "press started here"
//   _state = disabled ? 1
//          : (bit17 && bit21 && state.mouseHeld) ? 3
//          : bit17 ? 2 : 0                           ... and `colour` follows it
//   if (_state != prev) { SetNeedsSizeCalc(this);
//                         _children[_state]->ComputeLayout(rect) }
//   if (_children[4]) _children[4]->SetColor(colour)
//   if (!(_flags & bit17)) _flags &= ~bit21
//   if (_state != 1) {                               (not disabled)
//        if (state.mouseDown && bit17 && bit21)  UserInterface::Play(ui, 4)
//        if ((state.mouseUp && bit17 && bit21) || (_key >= 0 && KeyMap::Pressed(...))) {
//             _flags &= ~bit21 ; _flags |= bit22
//             push Event{dlg, this, 0x12, _elementId, mouseX, mouseY, 0, 0}
//             UserInterface::Play(ui, 5)
//        }
//   }
//
// *** THE `_children[_state]` DEREF AFTER A STATE CHANGE IS UNGUARDED IN
// RETAIL *** (0x180111223 loads the vtable with no null test, unlike the
// `_children[4]` test two instructions later). A ButtonElement always has its
// four state children; the asymmetry is retail's own and no guard is added.
extern "C" void RtUi_ButtonElement_Update(void* selfv, const void* time, void* ui,
                                          void** dlg, const void* statev) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* st = static_cast<const uint8_t*>(statev);

    void* dlgCopy = *dlg;
    RtAddr<ElemUpdateFn>(0x180116270ull)(self, time, ui, &dlgCopy, st);

    uint32_t flags = U32(self, kElemFlags);
    if ((flags & kFlagHidden) != 0) return;

    const int prevState = I32(self, kState);
    StoreFlags(self, flags & ~0x00400000u);

    if (st[kStateMouseDown] != 0) {
        const uint32_t f = U32(self, kElemFlags);
        if (((f >> 17) & 1u) != 0) StoreFlags(self, f | 0x00200000u);
    }

    uint32_t colour;
    flags = U32(self, kElemFlags);
    if ((flags & kFlagDisabledMask) != 0) {
        colour = U32(self, kColor1);
        I32(self, kState) = 1;
    } else if ((flags & 0x20000u) != 0 && ((flags >> 21) & 1u) != 0 &&
               st[kStateMouseHeld] != 0) {
        colour = U32(self, kColor3);
        I32(self, kState) = 3;
    } else if ((flags & 0x20000u) != 0) {
        colour = U32(self, kColor2);
        I32(self, kState) = 2;
    } else {
        colour = U32(self, kColor0);
        I32(self, kState) = 0;
    }

    if (I32(self, kState) != prevState) {
        SetNeedsSizeCalc(self);
        void* bg = Child(self, I32(self, kState));
        VSlot<ComputeLayoutFn>(bg, kSlotComputeLayout)(
            bg, I32(self, kElemLayoutLeft), I32(self, kElemLayoutTop),
            I32(self, kElemLayoutRight), I32(self, kElemLayoutBottom));
    }

    void* label = Child(self, kChildLabel);
    if (label != nullptr) VSlot<SetColorFn>(label, kSlotSetColor)(label, colour);

    flags = U32(self, kElemFlags);
    if (((flags >> 17) & 1u) == 0) StoreFlags(self, flags & ~0x00200000u);

    if (I32(self, kState) == 1) return;

    if (st[kStateMouseDown] != 0) {
        const uint32_t f = U32(self, kElemFlags);
        if (((f >> 17) & 1u) != 0 && ((f >> 21) & 1u) != 0)
            RtAddr<PlayFn>(0x1801223d0ull)(ui, 4);
    }

    bool fire = false;
    if (st[kStateMouseUp] != 0) {
        const uint32_t f = U32(self, kElemFlags);
        if (((f >> 17) & 1u) != 0 && ((f >> 21) & 1u) != 0) fire = true;
    }
    if (!fire) {
        const int32_t key = I32(self, kKeyIndex);
        if (key < 0) return;                                   // 1801112d4 JS -> end
        if (!RtAddr<KeyPressedFn>(0x1800a7a80ull)(Pc(st, kStateKeyMap), 0, key)) return;
    }

    StoreFlags(self, U32(self, kElemFlags) & ~0x00200000u);
    StoreFlags(self, U32(self, kElemFlags) |  0x00400000u);

    Event ev;
    ev.dialog    = *dlg;
    ev.element   = self;
    ev.kind      = kEventActivated;
    ev.elementId = I32(self, kElemElementId);
    ev.x         = I32c(st, kStateMouseX);
    ev.y         = I32c(st, kStateMouseY);
    ev.index     = 0;
    ev.tail      = 0;
    RtAddr<EventPushFn>(0x180033690ull)(static_cast<uint8_t*>(ui) + kUiEvents,
                                        I32(ui, kUiEventCount) + 1, &ev);
    RtAddr<PlayFn>(0x1801223d0ull)(ui, 5);
}

// ============================================================================
// ?Enable@ButtonElement@UI@@UEAAX_N@Z   0x180110cc0
// ============================================================================
// Element::Enable first, then — only when the element ended up disabled — push
// the DISABLED colour (+0xcc) into the label. Retail's tail is an indirect
// JUMP through slot 10, which is why Ghidra prints "could not recover
// jumptable"; behaviourally a call plus return.
extern "C" void RtUi_ButtonElement_Enable(void* selfv, bool enable) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElemEnableFn>(0x180116910ull)(self, enable);

    void* label = Child(self, kChildLabel);
    if (label != nullptr && (U32(self, kElemFlags) & kFlagDisabledMask) != 0)
        VSlot<SetColorFn>(label, kSlotSetColor)(label, U32(self, kColor1));
}

// ============================================================================
// Constructors / destructor
// ============================================================================
extern "C" void* RtUi_ButtonElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtButtonElement();
    I32(self, kKeyIndex) = -1;
    I32(self, kState)    = 0;
    return self;
}

extern "C" void* RtUi_ButtonElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtButtonElement();
    for (int off = kKeyIndex; off <= kColor3; off += 4)
        I32(self, off) = I32c(other, off);
    return self;
}

extern "C" void RtUi_ButtonElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtButtonElement();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiButtonElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@ButtonElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ButtonElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180048260ull, (void*)&RtUi_ButtonElement_TypeName);

    RegisterRuntimeExport("?Type@ButtonElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ButtonElement_Type);          ++n;
    RegisterRuntimeAddr(0x180048240ull, (void*)&RtUi_ButtonElement_Type);

    RegisterRuntimeExport("?ComputeSize@ButtonElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ButtonElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x180110d10ull, (void*)&RtUi_ButtonElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ButtonElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ButtonElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x180110e70ull, (void*)&RtUi_ButtonElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ButtonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ButtonElement_Draw);          ++n;
    RegisterRuntimeAddr(0x180111020ull, (void*)&RtUi_ButtonElement_Draw);

    RegisterRuntimeExport(
        "?Update@ButtonElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_ButtonElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1801110b0ull, (void*)&RtUi_ButtonElement_Update);

    RegisterRuntimeExport("?Enable@ButtonElement@UI@@UEAAX_N@Z",
                          (void*)&RtUi_ButtonElement_Enable);        ++n;
    RegisterRuntimeAddr(0x180110cc0ull, (void*)&RtUi_ButtonElement_Enable);

    RegisterRuntimeExport("??0ButtonElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ButtonElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x180110c40ull, (void*)&RtUi_ButtonElement_Ctor);

    RegisterRuntimeExport("??0ButtonElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_ButtonElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180048460ull, (void*)&RtUi_ButtonElement_CopyCtor);

    RegisterRuntimeExport("??1ButtonElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ButtonElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180110c90ull, (void*)&RtUi_ButtonElement_Dtor);
    return n;
}
