// ============================================================================
// runtime/src/ui/spinnerelement.cpp — UI::SpinnerElement, the whole class
// ============================================================================
//
// `??_7SpinnerElement@UI@@6B@` (retail 0x180157660):
//
//   0  vector deleting dtor  0x18004b3b0  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x18004b110  here
//   2  Type                  0x18004b0f0  here
//   4  ComputeSize           0x1800d83b0  here
//   5  ComputeLayout         0x1800d80c0  here
//   6  Draw                  0x1800d8030  here
//   7  Update                0x1800d7e70  here
//
// plus ??0SpinnerElement (0x1800d8670) and ??1SpinnerElement (0x1800d8640).
//
// A spinner is THREE children and one int, exactly as
// runtime/types/ui/SpinnerElement.h describes: children[0] is the
// NumberEditElement, children[1] the "up" ButtonElement and children[2] the
// "down" ButtonElement, each confirmed at run time by
// `?IsDerivedFrom@Base@Object@@` against its class's type id.
//
//   +0xC0  _increment  int32, ctor 1, ?SetIncrement@ 0x1800d7d30
//
// sizeof == 0xC8 (deleting dtor `MOV EDX,0xc8`).
//
// THE TWO TYPE IDS ARE READ, NOT ASSUMED: 0x1800d7eab loads
// `?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A` (0x1801fa190)
// and 0x1800d7ed4 / 0x1800d7f23 load
// `?_typeInfo@ButtonElement@UI@@0VBaseTypeInfo@Object@@A` (0x1801fa938); both
// VAs were resolved in the DLL's export directory, by address.
//
// A FAITHFUL CRASH THAT STAYS: when IsDerivedFrom says no, retail keeps the
// null in RDI (`XOR EDI,EDI; CMOVNZ RDI,RBX`) and then dereferences it for the
// vtable. That is retail's unguarded deref and CLAUDE.md says not to guard it.
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/SpinnerElement.h"

using namespace rtui;

namespace {

constexpr int kIncrement = 0xC0;
static_assert(kIncrement == offsetof(UI::SpinnerElement, _increment), "");

// NumberEditElement's own fields, reached through children[0].
constexpr int kNeText = 0xC8;
constexpr int kNeMin  = 0xF8;
constexpr int kNeMax  = 0xFC;

constexpr int kUsMouseX = 0x18;
constexpr int kUsMouseY = 0x1c;
constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

struct WidgetEvent {
    void*    dialog;
    void*    element;
    int32_t  kind;
    int32_t  elementId;
    int32_t  mouseX;
    int32_t  mouseY;
    int32_t  value;
    int32_t  pad24;
    int64_t  tail;
};
static_assert(sizeof(WidgetEvent) == 0x30, "");

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@SpinnerElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7SpinnerElement@UI@@6B@"); }

// The two type ids the IsDerivedFrom calls pass, each the FIRST DWORD of the
// named BaseTypeInfo static (0x1801fa190 and 0x1801fa938 in retail).
uint32_t NumberEditTypeId() {
    const uint8_t* ti = RT_DATA_SLOT(const uint8_t*,
        "?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A");
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
uint32_t ButtonTypeId() {
    const uint8_t* ti = RT_DATA_SLOT(const uint8_t*,
        "?_typeInfo@ButtonElement@UI@@0VBaseTypeInfo@Object@@A");
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}

using SetValueFn = void (*)(void* self, int32_t v);

}  // namespace

extern "C" uint32_t RtUi_SpinnerElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_SpinnerElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0SpinnerElement@UI@@QEAA@XZ   0x1800d8670
extern "C" void RtUi_SpinnerElement_Ctor(void* self) {
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    I32(self, kIncrement) = 1;
}

// ??1SpinnerElement@UI@@UEAA@XZ   0x1800d8640
extern "C" void RtUi_SpinnerElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    ElementDtor(self);
}

// slot 4 — 0x1800d83b0
extern "C" void RtUi_SpinnerElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    void** kids = reinterpret_cast<void**>(PTR(self, kElChildren));

    int32_t w0 = 0, h0 = 0, w1 = 0, h1 = 0, w2 = 0, h2 = 0;
    InlineGetPaddedSize(kids[0], &w0, &h0);
    InlineGetPaddedSize(kids[1], &w1, &h1);
    InlineGetPaddedSize(kids[2], &w2, &h2);

    if (w2 < w1) w2 = w1;
    I32(self, kElLayoutWidth) = w2 + w0;

    int32_t h = h2 + h1;
    if (h < h0) h = h0;
    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElLayoutHeight) = h;
}

// slot 5 — 0x1800d80c0
extern "C" void RtUi_SpinnerElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                  int32_t right, int32_t bottom) {
    if (void* border = PTR(self, kElBorder))
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(border)[kVtComputeLayout / 8])(
            border, x, y, right, bottom);

    reinterpret_cast<ComputeSizeSlotFn>(VTable(self)[kVtComputeSize / 8])(self);

    int32_t w = I32(self, kElLayoutWidth);
    int32_t h = I32(self, kElLayoutHeight);
    const int32_t align = I32(self, kElAlignment);

    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + I32(self, kElLeftPad);
    int32_t py = y + I32(self, kElTopPad);
    const int32_t aw = (right  - I32(self, kElRightPad))  - px;
    const int32_t ah = (bottom - I32(self, kElBottomPad)) - py;

    if ((U32(self, kElFlags) & kFlagStretchX) != 0) w = aw;
    if ((U32(self, kElFlags) & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - I32(self, kElRightPad)) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - I32(self, kElBottomPad)) - h;
    }

    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutRight)  = px + w;
    I32(self, kElLayoutBottom) = py + h;

    void** kids = reinterpret_cast<void**>(PTR(self, kElChildren));
    int32_t ew = 0, eh = 0;
    InlineGetPaddedSize(kids[0], &ew, &eh);          // only the WIDTH is used
    int32_t bw = 0, bh = 0;
    InlineGetPaddedSize(kids[1], &bw, &bh);          // only the HEIGHT is used
    int32_t dw = 0, dh = 0;
    InlineGetPaddedSize(kids[2], &dw, &dh);          // measured and discarded

    const int32_t xmid = ew + I32(self, kElLayoutLeft);
    const int32_t ymid = I32(self, kElLayoutTop) + bh;

    kids = reinterpret_cast<void**>(PTR(self, kElChildren));
    reinterpret_cast<ComputeLayoutSlotFn>(VTable(kids[0])[kVtComputeLayout / 8])(
        kids[0], I32(self, kElLayoutLeft), I32(self, kElLayoutTop),
        xmid, I32(self, kElLayoutBottom));

    kids = reinterpret_cast<void**>(PTR(self, kElChildren));
    reinterpret_cast<ComputeLayoutSlotFn>(VTable(kids[1])[kVtComputeLayout / 8])(
        kids[1], xmid, I32(self, kElLayoutTop),
        I32(self, kElLayoutRight), ymid);

    kids = reinterpret_cast<void**>(PTR(self, kElChildren));
    reinterpret_cast<ComputeLayoutSlotFn>(VTable(kids[2])[kVtComputeLayout / 8])(
        kids[2], xmid, ymid,
        I32(self, kElLayoutRight), I32(self, kElLayoutBottom));
}

// slot 6 — 0x1800d8030
extern "C" void RtUi_SpinnerElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    const int32_t n = I32(me, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        void* child = reinterpret_cast<void**>(PTR(me, kElChildren))[i];
        reinterpret_cast<DrawSlotFn>(VTable(child)[kVtDraw / 8])(child, ctx, alpha);
    }
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@SpinnerElement@UI@@ ...   0x1800d7e70
// ---------------------------------------------------------------------------
extern "C" void RtUi_SpinnerElement_Update(void* self, const void* time, void* ui,
                                           void** dlgPtr, const void* state) {
    void* slot = *dlgPtr;
    ElementUpdate(self, time, ui, &slot, state);

    const uint32_t kNumberEditTypeId = NumberEditTypeId();
    const uint32_t kButtonTypeId     = ButtonTypeId();

    void* edit = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
    if (!RtAddr<IsDerivedFromFn>(0x1800c0390ull)(edit, kNumberEditTypeId)) return;
    if (edit == nullptr) return;

    // The "up" button is enabled only while the value is below the maximum.
    void* upRaw = reinterpret_cast<void**>(PTR(self, kElChildren))[1];
    void* up = RtAddr<IsDerivedFromFn>(0x1800c0390ull)(upRaw, kButtonTypeId)
               ? upRaw : nullptr;
    {
        void** vt = VTable(up);
        const int32_t v = RtAddr<ToIntegerFn>(0x18003f620ull)(
            static_cast<const char*>(PTR(edit, kNeText)));
        reinterpret_cast<EnableSlotFn>(vt[kVtEnable / 8])(up, v < I32(edit, kNeMax));
    }

    // ... and the "down" button only while it is above the minimum.
    void* downRaw = reinterpret_cast<void**>(PTR(self, kElChildren))[2];
    void* down = RtAddr<IsDerivedFromFn>(0x1800c0390ull)(downRaw, kButtonTypeId)
                 ? downRaw : nullptr;
    {
        void** vt = VTable(down);
        const int32_t v = RtAddr<ToIntegerFn>(0x18003f620ull)(
            static_cast<const char*>(PTR(edit, kNeText)));
        reinterpret_cast<EnableSlotFn>(vt[kVtEnable / 8])(down, I32(edit, kNeMin) < v);
    }

    int32_t step;
    if ((U32(reinterpret_cast<void**>(PTR(self, kElChildren))[1], kElFlags)
         & kFlagWasActivated) != 0) {
        step = I32(self, kIncrement);
    } else if ((U32(reinterpret_cast<void**>(PTR(self, kElChildren))[2], kElFlags)
                & kFlagWasActivated) != 0) {
        step = -I32(self, kIncrement);
    } else {
        return;
    }
    if (step == 0) return;

    const int32_t v = RtAddr<ToIntegerFn>(0x18003f620ull)(
        static_cast<const char*>(PTR(edit, kNeText)));
    RtAddr<SetValueFn>(0x1800d4bb0ull)(edit, step + v);   // ?SetValue@NumberEditElement@

    WidgetEvent ev;
    ev.dialog    = *dlgPtr;
    ev.element   = self;
    ev.kind      = 0x15;
    ev.elementId = I32(self, kElElementId);
    ev.mouseX    = I32c(state, kUsMouseX);
    ev.mouseY    = I32c(state, kUsMouseY);
    ev.value     = 0;
    ev.tail      = 0;
    RtAddr<ArraySetSizeFillFn>(0x180033690ull)(
        B(ui, kUiEvents), I32(ui, kUiEventCount) + 1, &ev);
}

// ============================================================================
int RtUiSpinnerElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@SpinnerElement@UI@@UEBAIXZ",
                          (void*)&RtUi_SpinnerElement_Type);          ++n;
    RegisterRuntimeAddr(0x18004b0f0ull, (void*)&RtUi_SpinnerElement_Type);

    RegisterRuntimeExport("?TypeName@SpinnerElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_SpinnerElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x18004b110ull, (void*)&RtUi_SpinnerElement_TypeName);

    RegisterRuntimeExport("??0SpinnerElement@UI@@QEAA@XZ",
                          (void*)&RtUi_SpinnerElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1800d8670ull, (void*)&RtUi_SpinnerElement_Ctor);

    RegisterRuntimeExport("??1SpinnerElement@UI@@UEAA@XZ",
                          (void*)&RtUi_SpinnerElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800d8640ull, (void*)&RtUi_SpinnerElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@SpinnerElement@UI@@UEAAXXZ",
                          (void*)&RtUi_SpinnerElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800d83b0ull, (void*)&RtUi_SpinnerElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@SpinnerElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_SpinnerElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800d80c0ull, (void*)&RtUi_SpinnerElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@SpinnerElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_SpinnerElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800d8030ull, (void*)&RtUi_SpinnerElement_Draw);

    RegisterRuntimeExport(
        "?Update@SpinnerElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_SpinnerElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1800d7e70ull, (void*)&RtUi_SpinnerElement_Update);

    return n;
}
