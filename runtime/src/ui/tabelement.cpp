// ============================================================================
// runtime/src/ui/tabelement.cpp — UI::TabElement, the whole class
// ============================================================================
//
// `??_7TabElement@UI@@6B@` (retail 0x180157790):
//
//   0  vector deleting dtor  0x18004ca90  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x18004c700  here
//   2  Type                  0x18004c6e0  here
//   4  ComputeSize           0x1800d9ea0  here
//   5  ComputeLayout         0x1800d9b20  here
//   6  Draw                  0x1800d98b0  here
//   7  Update                0x1800d9810  here
//
// plus ??0TabElement (0x1800da390), ??1TabElement (0x1800da360) and the
// private `?UpdateContainers@TabElement@UI@@AEAAXXZ` (0x1800d9710), which IS
// this file's: it is not on the UI non-slot callee list and
// runtime/types/ui/TabElement.h's note that it is "NOT reimplemented here --
// called via RT_ORIGINAL_ONCE" was true of Wave 7 lane E and is not any more.
//
// NOT HERE: `?AddTab@TabElement@UI@@` (0x1800da130) — already ported, by
// runtime/src/ui/tabelement_addtab.cpp. `?SetActiveTab@` (0x18004c890) and
// `?GetActiveTab@` (0x18004c880) are lane E's, in widgets_layout.cpp.
//
// ---------------------------------------------------------------------------
// LAYOUT. sizeof == 0x108 (deleting dtor `MOV EDX,0x108`):
//
//   +0xC0  _tabCount      int32  } one qword store of 0 in the ctor
//   +0xC4  _gap           int32  }  the margin used everywhere as the inset
//   +0xC8  _edgeLeft      Sprite (sheet +0xC8, index +0xD0)
//   +0xD8  _edgeMiddle    Sprite
//   +0xE8  _edgeRight     Sprite     the three that "erase" the active tab's
//                                    bottom border into the page
//   +0xF8  _activeTab     int32  } one qword store of 0
//   +0xFC  _tabBarHeight  int32  }  computed by ComputeSize
//   +0x100 _tabBarWidth   int32
//
// THE CHILD LIST IS PAIRED, and that is what makes every index below readable:
//
//   children[0]              the tab-strip backdrop
//   children[1]              the page backdrop
//   children[2]              the page frame
//   children[3 + 2*i]        tab i's BUTTON
//   children[4 + 2*i]        tab i's CONTAINER
//
// Retail addresses them by BYTE offset into `_children._data` with a stride of
// 0x10 (two pointers) — 0x18 for the first button, 0x20 for the first
// container — and picks the active pair with `(activeTab + 2) * 0x10` for the
// container and `(activeTab * 2 + 3) * 8` for the button. Both spellings are
// the same pairing seen from two directions, which is how it is known and not
// guessed.
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/TabElement.h"

using namespace rtui;

namespace {

constexpr int kTabCount     = 0xC0;
constexpr int kGap          = 0xC4;
constexpr int kEdgeLeft     = 0xC8;
constexpr int kEdgeMiddle   = 0xD8;
constexpr int kEdgeRight    = 0xE8;
constexpr int kActiveTab    = 0xF8;
constexpr int kTabBarHeight = 0xFC;
constexpr int kTabBarWidth  = 0x100;

static_assert(kActiveTab == UI::TabElementOffsets::kActiveTab, "");

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@TabElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7TabElement@UI@@6B@"); }

void** Kids(void* self) { return reinterpret_cast<void**>(PTR(self, kElChildren)); }
void*  TabButton(void* self, int32_t i)    { return Kids(self)[3 + 2 * i]; }
void*  TabContainer(void* self, int32_t i) { return Kids(self)[4 + 2 * i]; }

}  // namespace

extern "C" uint32_t RtUi_TabElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_TabElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0TabElement@UI@@QEAA@XZ   0x1800da390
extern "C" void RtUi_TabElement_Ctor(void* self) {
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    I32(self, kTabCount) = 0;
    I32(self, kGap)      = 0;
    for (int off : { kEdgeLeft, kEdgeMiddle, kEdgeRight }) {
        PTR(self, off + kSpriteSheet) = nullptr;
        I32(self, off + kSpriteIndex) = -1;
    }
    I32(self, kActiveTab)    = 0;
    I32(self, kTabBarHeight) = 0;
    I32(self, kTabBarWidth)  = 0;
}

// ??1TabElement@UI@@UEAA@XZ   0x1800da360
extern "C" void RtUi_TabElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    ElementDtor(self);
}

// ---------------------------------------------------------------------------
// ?UpdateContainers@TabElement@UI@@AEAAXXZ   0x1800d9710
//
// Hide every container but the active one, and lay the active one out inside
// the page frame. The "dirty walk if bit 0 flipped" guard is Element's.
// ---------------------------------------------------------------------------
extern "C" void RtUi_TabElement_UpdateContainers(void* self) {
    const int32_t n = I32(self, kTabCount);
    for (int32_t i = 0; i < n; ++i) {
        void* c = TabContainer(self, i);
        const uint32_t old = U32(c, kElFlags);
        if (i == I32(self, kActiveTab)) {
            U32(c, kElFlags) = old & ~kFlagHidden;
            if ((old & kFlagHidden) != 0) DirtyWalk(c);
            const int32_t gap = I32(self, kGap);
            void* cc = TabContainer(self, i);
            reinterpret_cast<ComputeLayoutSlotFn>(VTable(cc)[kVtComputeLayout / 8])(
                cc, gap + I32(self, kElLayoutLeft),
                I32(self, kTabBarHeight) + gap * 2 + I32(self, kElLayoutTop),
                I32(self, kElLayoutRight)  - gap,
                I32(self, kElLayoutBottom) - gap);
        } else {
            U32(c, kElFlags) = old | kFlagHidden;
            if ((((old | kFlagHidden) ^ old) & kFlagHidden) != 0) DirtyWalk(c);
        }
    }
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@TabElement@UI@@UEAAXXZ   0x1800d9ea0
// ---------------------------------------------------------------------------
extern "C" void RtUi_TabElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    I32(self, kTabBarWidth)  = I32(self, kGap);
    I32(self, kTabBarHeight) = 0;

    int32_t maxW = 0, maxH = 0;

    const int32_t n = I32(self, kTabCount);
    for (int32_t i = 0; i < n; ++i) {
        void* button = TabButton(self, i);
        if ((U32(button, kElFlags) & kFlagHidden) != 0) continue;

        int32_t bw = 0, bh = 0;
        InlineGetPaddedSize(button, &bw, &bh);
        if (bh < I32(self, kTabBarHeight)) bh = I32(self, kTabBarHeight);
        I32(self, kTabBarHeight) = bh;
        I32(self, kTabBarWidth) = I32(self, kTabBarWidth) + I32(self, kGap) + bw;

        int32_t cw = 0, ch = 0;
        InlineGetPaddedSize(TabContainer(self, i), &cw, &ch);
        if (maxW <= cw) maxW = cw;
        if (maxH <= ch) maxH = ch;
    }

    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElLayoutHeight) = I32(self, kGap) * 3 + I32(self, kTabBarHeight) + maxH;
    I32(self, kElLayoutWidth)  = maxW + I32(self, kGap) * 2;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@TabElement@UI@@UEAAXHHHH@Z   0x1800d9b20
// ---------------------------------------------------------------------------
extern "C" void RtUi_TabElement_ComputeLayout(void* self, int32_t x, int32_t y,
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

    const int32_t rx = px + w;
    const int32_t by = py + h;
    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutRight)  = rx;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutBottom) = by;

    const int32_t barBottom = I32(self, kTabBarHeight) + I32(self, kGap) + py;

    void** kids = Kids(self);
    reinterpret_cast<ComputeLayoutSlotFn>(VTable(kids[0])[kVtComputeLayout / 8])(
        kids[0], px, py, I32(self, kTabBarWidth) + px, barBottom);

    kids = Kids(self);
    reinterpret_cast<ComputeLayoutSlotFn>(VTable(kids[1])[kVtComputeLayout / 8])(
        kids[1], px, barBottom, rx, by);

    {
        const int32_t gap = I32(self, kGap);
        kids = Kids(self);
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(kids[2])[kVtComputeLayout / 8])(
            kids[2], gap + px, gap + barBottom, rx - gap, by - gap);
    }

    int32_t cursor = I32(self, kGap) + px;
    const int32_t n = I32(self, kTabCount);
    for (int32_t i = 0; i < n; ++i) {
        void* button = TabButton(self, i);
        if ((U32(button, kElFlags) & kFlagHidden) != 0) continue;

        int32_t bw = 0, bh = 0;
        InlineGetPaddedSize(button, &bw, &bh);

        button = TabButton(self, i);
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(button)[kVtComputeLayout / 8])(
            button, cursor, I32(self, kGap) + py,
            bw + cursor, I32(self, kGap) + bh + py);
        cursor = cursor + I32(self, kGap) + bw;
    }

    {
        void* active = TabContainer(self, I32(self, kActiveTab));
        const int32_t gap = I32(self, kGap);
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(active)[kVtComputeLayout / 8])(
            active, gap + px, barBottom + gap, rx - gap, by - gap);
    }
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@TabElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d98b0
//
// The three edge sprites are drawn with the element's RAW colour — no
// ComputeColor, no dim test. Retail passes `[RDI+0x38]` straight through.
// ---------------------------------------------------------------------------
extern "C" void RtUi_TabElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    void** kids = Kids(me);
    reinterpret_cast<DrawSlotFn>(VTable(kids[0])[kVtDraw / 8])(kids[0], ctx, alpha);
    kids = Kids(me);
    reinterpret_cast<DrawSlotFn>(VTable(kids[1])[kVtDraw / 8])(kids[1], ctx, alpha);
    kids = Kids(me);
    reinterpret_cast<DrawSlotFn>(VTable(kids[2])[kVtDraw / 8])(kids[2], ctx, alpha);

    const int32_t n = I32(me, kTabCount);
    for (int32_t i = 0; i < n; ++i) {
        void* button = TabButton(me, i);
        if ((U32(button, kElFlags) & kFlagHidden) == 0)
            reinterpret_cast<DrawSlotFn>(VTable(button)[kVtDraw / 8])(button, ctx, alpha);
    }

    void* activeBtn = Kids(me)[I32(me, kActiveTab) * 2 + 3];
    const int32_t bl = I32(activeBtn, kElLayoutLeft);
    const int32_t br = I32(activeBtn, kElLayoutRight);
    const int32_t y0 = I32(activeBtn, kElLayoutBottom) - 4;
    const int32_t y1 = I32(Kids(me)[2], kElLayoutTop) + 4;
    const uint32_t c = U32(me, kElColor);

    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kEdgeLeft),
                                         bl, y0, SpriteWidth(me, kEdgeLeft) + bl, y1, c);

    const int32_t rw = SpriteWidth(me, kEdgeRight);
    const int32_t lw = SpriteWidth(me, kEdgeLeft);
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kEdgeMiddle),
                                         bl + lw, y0, br - rw, y1, c);

    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kEdgeRight),
                                         br - SpriteWidth(me, kEdgeRight), y0, br, y1, c);

    void* active = TabContainer(me, I32(me, kActiveTab));
    reinterpret_cast<DrawSlotFn>(VTable(active)[kVtDraw / 8])(active, ctx, alpha);
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@TabElement@UI@@ ...   0x1800d9810
// ---------------------------------------------------------------------------
extern "C" void RtUi_TabElement_Update(void* self, const void* time, void* ui,
                                       void** dlgPtr, const void* state) {
    void* slot = *dlgPtr;
    ElementUpdate(self, time, ui, &slot, state);

    const int32_t before = I32(self, kActiveTab);
    const int32_t n = I32(self, kTabCount);
    for (int32_t i = 0; i < n; ++i) {
        void* button = TabButton(self, i);
        if ((U32(button, kElFlags) & kFlagHidden) == 0 &&
            (U32(button, kElFlags) & kFlagWasActivated) != 0) {
            I32(self, kActiveTab) = i;
            break;
        }
    }
    if (before == I32(self, kActiveTab)) return;
    RtUi_TabElement_UpdateContainers(self);
}

// ============================================================================
int RtUiTabElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@TabElement@UI@@UEBAIXZ",
                          (void*)&RtUi_TabElement_Type);              ++n;
    RegisterRuntimeAddr(0x18004c6e0ull, (void*)&RtUi_TabElement_Type);

    RegisterRuntimeExport("?TypeName@TabElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_TabElement_TypeName);          ++n;
    RegisterRuntimeAddr(0x18004c700ull, (void*)&RtUi_TabElement_TypeName);

    RegisterRuntimeExport("??0TabElement@UI@@QEAA@XZ",
                          (void*)&RtUi_TabElement_Ctor);              ++n;
    RegisterRuntimeAddr(0x1800da390ull, (void*)&RtUi_TabElement_Ctor);

    RegisterRuntimeExport("??1TabElement@UI@@UEAA@XZ",
                          (void*)&RtUi_TabElement_Dtor);              ++n;
    RegisterRuntimeAddr(0x1800da360ull, (void*)&RtUi_TabElement_Dtor);

    RegisterRuntimeExport("?UpdateContainers@TabElement@UI@@AEAAXXZ",
                          (void*)&RtUi_TabElement_UpdateContainers);  ++n;
    RegisterRuntimeAddr(0x1800d9710ull, (void*)&RtUi_TabElement_UpdateContainers);

    RegisterRuntimeExport("?ComputeSize@TabElement@UI@@UEAAXXZ",
                          (void*)&RtUi_TabElement_ComputeSize);       ++n;
    RegisterRuntimeAddr(0x1800d9ea0ull, (void*)&RtUi_TabElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@TabElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_TabElement_ComputeLayout);     ++n;
    RegisterRuntimeAddr(0x1800d9b20ull, (void*)&RtUi_TabElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@TabElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_TabElement_Draw);              ++n;
    RegisterRuntimeAddr(0x1800d98b0ull, (void*)&RtUi_TabElement_Draw);

    RegisterRuntimeExport(
        "?Update@TabElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_TabElement_Update);                              ++n;
    RegisterRuntimeAddr(0x1800d9810ull, (void*)&RtUi_TabElement_Update);

    return n;
}
