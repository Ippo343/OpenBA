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
// runtime/types/ui/TabElement.h's note that it is "NOT reimplemented here"
// was true of Wave 7 lane E and is not any more.
//
// NOT HERE: `?AddTab@TabElement@UI@@` (0x1800da130) — already ported, by
// runtime/src/ui/tabelement_addtab.cpp. `?SetActiveTab@` (0x18004c890) and
// `?GetActiveTab@` (0x18004c880) are lane E's, in widgets_layout.cpp.
//
// LAYOUT and the paired child list: runtime/types/ui/TabElement.h (sizeof
// 0x108; RULE B6-8 moved this file's offsets there under the retail name).
//
// ENDGAME B.3.6 block C5: the six slot bodies and UpdateContainers MOVED into
// the members of the real `UI::TabElement`; each registered door is a
// one-line QUALIFIED delegation. The ctor / dtor stay doors (B4b-2).
// ============================================================================

#include <atomic>

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
static_assert(kTabCount     == offsetof(UI::TabElement, _tabCount), "");
static_assert(kGap          == offsetof(UI::TabElement, _gap), "");
static_assert(kEdgeLeft     == offsetof(UI::TabElement, _edgeLeft), "");
static_assert(kEdgeMiddle   == offsetof(UI::TabElement, _edgeMiddle), "");
static_assert(kEdgeRight    == offsetof(UI::TabElement, _edgeRight), "");
static_assert(kTabBarHeight == offsetof(UI::TabElement, _tabBarHeight), "");
static_assert(kTabBarWidth  == offsetof(UI::TabElement, _tabBarWidth), "");

// RULE B6-5: namespace-scope atomics, never a function-local first-call cache.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_vtable{nullptr};
const uint8_t* TabTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@TabElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* VtTabElement() { return AtomicExport(g_vtable, "??_7TabElement@UI@@6B@"); }

int32_t SpriteW(const ::Sprite& s) { return SpriteWidth(&s, 0); }

}  // namespace

namespace UI {

// [1] 0x18004c700 / [2] 0x18004c6e0
const char* TabElement::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, TabTypeInfo() + 8, sizeof v); return v;
}
uint32_t TabElement::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, TabTypeInfo(), sizeof v); return v;
}

// ---------------------------------------------------------------------------
// ?UpdateContainers@TabElement@UI@@AEAAXXZ   0x1800d9710
//
// Hide every container but the active one, and lay the active one out inside
// the page frame. The "dirty walk if bit 0 flipped" guard is Element's.
// ---------------------------------------------------------------------------
void TabElement::UpdateContainers() {
    const int32_t n = _tabCount;
    for (int32_t i = 0; i < n; ++i) {
        Element* c = _children.GetData()[GetContainerIndex(i)];
        const uint32_t old = c->_flags;
        if (i == _activeTab) {
            c->_flags = old & ~kFlagHidden;
            if ((old & kFlagHidden) != 0) DirtyWalk(c);
            const int32_t gap = _gap;
            Element* cc = _children.GetData()[GetContainerIndex(i)];
            cc->ComputeLayout(gap + _layoutLeft,
                              _tabBarHeight + gap * 2 + _layoutTop,
                              _layoutRight  - gap,
                              _layoutBottom - gap);                      // slot 5
        } else {
            c->_flags = old | kFlagHidden;
            if ((((old | kFlagHidden) ^ old) & kFlagHidden) != 0) DirtyWalk(c);
        }
    }
}

// ---------------------------------------------------------------------------
// [4] ?ComputeSize@TabElement@UI@@UEAAXXZ   0x1800d9ea0
// ---------------------------------------------------------------------------
void TabElement::ComputeSize() {
    if ((_flags & kFlagNeedsSizeCalc) == 0) return;

    _tabBarWidth  = _gap;
    _tabBarHeight = 0;

    int32_t maxW = 0, maxH = 0;

    const int32_t n = _tabCount;
    for (int32_t i = 0; i < n; ++i) {
        Element* button = _children.GetData()[GetButtonIndex(i)];
        if ((button->_flags & kFlagHidden) != 0) continue;

        int32_t bw = 0, bh = 0;
        InlineGetPaddedSize(button, &bw, &bh);
        if (bh < _tabBarHeight) bh = _tabBarHeight;
        _tabBarHeight = bh;
        _tabBarWidth = _tabBarWidth + _gap + bw;

        int32_t cw = 0, ch = 0;
        InlineGetPaddedSize(_children.GetData()[GetContainerIndex(i)], &cw, &ch);
        if (maxW <= cw) maxW = cw;
        if (maxH <= ch) maxH = ch;
    }

    _flags &= ~kFlagNeedsSizeCalc;
    _layoutHeight = _gap * 3 + _tabBarHeight + maxH;
    _layoutWidth  = maxW + _gap * 2;
}

// ---------------------------------------------------------------------------
// [5] ?ComputeLayout@TabElement@UI@@UEAAXHHHH@Z   0x1800d9b20
// ---------------------------------------------------------------------------
void TabElement::ComputeLayout(int32_t x, int32_t y, int32_t right, int32_t bottom) {
    if (_border != nullptr)
        reinterpret_cast<Element*>(_border)->ComputeLayout(x, y, right, bottom);   // B.3.6 bridge — slot 5

    ComputeSize();   // slot 4, VIRTUAL — `CALL [RAX+0x20]` in retail

    int32_t w = _layoutWidth;
    int32_t h = _layoutHeight;
    const int32_t align = _alignment;

    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + _leftPad;
    int32_t py = y + _topPad;
    const int32_t aw = (right  - _rightPad)  - px;
    const int32_t ah = (bottom - _bottomPad) - py;

    if ((_flags & kFlagStretchX) != 0) w = aw;
    if ((_flags & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - _rightPad) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - _bottomPad) - h;
    }

    const int32_t rx = px + w;
    const int32_t by = py + h;
    _layoutLeft   = px;
    _layoutRight  = rx;
    _layoutTop    = py;
    _layoutBottom = by;

    const int32_t barBottom = _tabBarHeight + _gap + py;

    // The array is RE-READ before each call (slot 5, virtual).
    _children.GetData()[0]->ComputeLayout(px, py, _tabBarWidth + px, barBottom);
    _children.GetData()[1]->ComputeLayout(px, barBottom, rx, by);
    {
        const int32_t gap = _gap;
        _children.GetData()[2]->ComputeLayout(gap + px, gap + barBottom, rx - gap, by - gap);
    }

    int32_t cursor = _gap + px;
    const int32_t n = _tabCount;
    for (int32_t i = 0; i < n; ++i) {
        Element* button = _children.GetData()[GetButtonIndex(i)];
        if ((button->_flags & kFlagHidden) != 0) continue;

        int32_t bw = 0, bh = 0;
        InlineGetPaddedSize(button, &bw, &bh);

        button = _children.GetData()[GetButtonIndex(i)];
        button->ComputeLayout(cursor, _gap + py, bw + cursor, _gap + bh + py);
        cursor = cursor + _gap + bw;
    }

    {
        Element* active = _children.GetData()[GetContainerIndex(_activeTab)];
        const int32_t gap = _gap;
        active->ComputeLayout(gap + px, barBottom + gap, rx - gap, by - gap);
    }
}

// ---------------------------------------------------------------------------
// [6] ?Draw@TabElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d98b0
//
// The three edge sprites are drawn with the element's RAW colour — no
// ComputeColor, no dim test. Retail passes `[RDI+0x38]` straight through.
// ---------------------------------------------------------------------------
void TabElement::Draw(SpriteContext& context, uint8_t alpha) const {
    void* const ctx = &context;
    if (_border != nullptr)
        reinterpret_cast<const Element*>(_border)->Draw(context, alpha);   // B.3.6 bridge — slot 6

    _children.GetData()[0]->Draw(context, alpha);   // slot 6
    _children.GetData()[1]->Draw(context, alpha);
    _children.GetData()[2]->Draw(context, alpha);

    const int32_t n = _tabCount;
    for (int32_t i = 0; i < n; ++i) {
        const Element* button = _children.GetData()[GetButtonIndex(i)];
        if ((button->_flags & kFlagHidden) == 0)
            button->Draw(context, alpha);
    }

    const Element* activeBtn = _children.GetData()[_activeTab * 2 + 3];
    const int32_t bl = activeBtn->_layoutLeft;
    const int32_t br = activeBtn->_layoutRight;
    const int32_t y0 = activeBtn->_layoutBottom - 4;
    const int32_t y1 = _children.GetData()[2]->_layoutTop + 4;
    const uint32_t c = _color;

    // UI::SpriteContext::AddSprite (block D's class) — by address, doors stay.
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &_edgeLeft,
                                         bl, y0, SpriteW(_edgeLeft) + bl, y1, c);

    const int32_t rw = SpriteW(_edgeRight);
    const int32_t lw = SpriteW(_edgeLeft);
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &_edgeMiddle, bl + lw, y0, br - rw, y1, c);

    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &_edgeRight,
                                         br - SpriteW(_edgeRight), y0, br, y1, c);

    _children.GetData()[GetContainerIndex(_activeTab)]->Draw(context, alpha);
}

// ---------------------------------------------------------------------------
// [7] ?Update@TabElement@UI@@ ...   0x1800d9810
// ---------------------------------------------------------------------------
void TabElement::Update(const System::Time& time, UserInterface* ui,
                        Object::InternalPtr<DialogInstance>* dialog,
                        const UpdateState& state) {
    // The base Update, QUALIFIED (this object's slot 7 is this very body), with
    // the handle copied into a temporary whose ADDRESS is passed — retail's shape.
    Object::InternalPtr<DialogInstance> slot = *dialog;
    Element::Update(time, ui, &slot, state);

    const int32_t before = _activeTab;
    const int32_t n = _tabCount;
    for (int32_t i = 0; i < n; ++i) {
        const Element* button = _children.GetData()[GetButtonIndex(i)];
        if ((button->_flags & kFlagHidden) == 0 &&
            (button->_flags & kFlagWasActivated) != 0) {
            _activeTab = i;
            break;
        }
    }
    if (before == _activeTab) return;
    UpdateContainers();
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, QUALIFIED delegations.
// ============================================================================
extern "C" uint32_t RtUi_TabElement_Type(const void* self) {
    return reinterpret_cast<const UI::TabElement*>(self)->UI::TabElement::Type();
}
extern "C" const char* RtUi_TabElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::TabElement*>(self)->UI::TabElement::TypeName();
}
extern "C" void RtUi_TabElement_UpdateContainers(void* self) {
    reinterpret_cast<UI::TabElement*>(self)->UI::TabElement::UpdateContainers();
}
extern "C" void RtUi_TabElement_ComputeSize(void* self) {
    reinterpret_cast<UI::TabElement*>(self)->UI::TabElement::ComputeSize();
}
extern "C" void RtUi_TabElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                              int32_t right, int32_t bottom) {
    reinterpret_cast<UI::TabElement*>(self)->UI::TabElement::ComputeLayout(x, y, right, bottom);
}
extern "C" void RtUi_TabElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::TabElement*>(self)->UI::TabElement::Draw(
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}
extern "C" void RtUi_TabElement_Update(void* self, const void* time, void* ui,
                                       void** dlgPtr, const void* state) {
    reinterpret_cast<UI::TabElement*>(self)->UI::TabElement::Update(
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlgPtr),   // B.3.6 bridge
        *static_cast<const UI::Element::UpdateState*>(state));
}

// ??0TabElement@UI@@QEAA@XZ   0x1800da390 — a DOOR (B4b-2), unchanged.
extern "C" void* RtUi_TabElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = VtTabElement();
    I32(self, kTabCount) = 0;
    I32(self, kGap)      = 0;
    const int edgeSprites[3] = { kEdgeLeft, kEdgeMiddle, kEdgeRight };
    for (int off : edgeSprites) {
        PTR(self, off + kSpriteSheet) = nullptr;
        I32(self, off + kSpriteIndex) = -1;
    }
    I32(self, kActiveTab)    = 0;
    I32(self, kTabBarHeight) = 0;
    I32(self, kTabBarWidth)  = 0;
    return self;
}

// ??1TabElement@UI@@UEAA@XZ   0x1800da360 — a DOOR (B4b-2), unchanged.
extern "C" void RtUi_TabElement_Dtor(void* self) {
    PTR(self, 0x00) = VtTabElement();
    ElementDtor(self);
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
