// ============================================================================
// runtime/src/ui/layoutelement.cpp — UI::LayoutElement, the whole class
// ============================================================================
//
// `??_7LayoutElement@UI@@6B@` (retail 0x180156c60), and the ctor/dtor that
// stamp it. Vtable extent and per-slot targets from the batch survey:
//
//   0  vector deleting dtor  0x180043570  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180043290  here
//   2  Type                  0x180043270  here
//   3  Persist               0x180116870  UI::Element's own (not overridden)
//   4  ComputeSize           0x1800d3100  here
//   5  ComputeLayout         0x1800d2af0  here
//   6  Draw                  0x1800d2a40  here
//   7..11                    UI::Element's own
//
// plus ??0LayoutElement@UI@@QEAA@XZ (0x1800d32e0) and
// ??1LayoutElement@UI@@UEAA@XZ (0x1800d32a0).
//
// NOT HERE: `?GetChildLayout@LayoutElement@UI@@AEAAXHHHHHH_N@Z` (0x1800d2dc0)
// and its two helpers GetChildLayoutX/Y — the batch brief assigns the UI
// non-slot callee list to the UI-text agent, so this file REACHES
// GetChildLayout by address instead of porting a second copy.
//
// ---------------------------------------------------------------------------
// LAYOUT.  sizeof(LayoutElement) == 0xD0, and it is proven, not inferred: the
// vector deleting destructor at 0x180043570 passes `MOV EDX,0xd0` as the array
// stride to `eh vector destructor iterator`, and ~LayoutElement destroys
// exactly one member — the array header at +0xC0 — before tail-calling
// ~Element. UI::Element is 0xC0 (runtime/types/ui/Element.h), so the class
// adds exactly one 0x10-byte Array header:
//
//   +0xC0  _rows._data   the per-child layout record buffer, stride 0x44
//   +0xC8  _rows._size / _capacity
//
// The STRIDE is 0x44 and the record's fields are read at +0x18, +0x1c, +0x20,
// +0x24, +0x28, +0x2c, +0x30, +0x34, +0x38, +0x3c, +0x40 by the three bodies
// below; the record's own writer is GetChildLayout, which is not this file's.
// The COUNT the loops use is UI::Element::_children._size (+0xA8), not the
// array's own +0xC8 — the two are kept parallel by GetChildLayout's caller.
//
// ENDGAME B.3.6 block C4: the layout above (and the 0x44-byte record) moved to
// runtime/types/ui/LayoutElement.h (the real class). Every slot body below is a
// MEMBER; each registered door is a one-line QUALIFIED delegation; ctor/dtor
// stay DOORS. RULE B6-5: the vtable and type-info record are cached in
// namespace-scope std::atomic slots.
// ============================================================================

#include <atomic>

#include "widget_common.h"
#include "../../types/ui/LayoutElement.h"

using namespace rtui;

namespace {

std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_vtable{nullptr};

void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
const uint8_t* TypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@LayoutElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* Vtable() { return AtomicExport(g_vtable, "??_7LayoutElement@UI@@6B@"); }

// `?GetChildLayout@LayoutElement@UI@@AEAAXHHHHHH_N@Z` (0x1800d2dc0) — declared
// on the class, body still element_bodies_b5.cpp's door (fragment pass).
using GetChildLayoutFn = void (*)(void* self, int32_t index, int32_t sizePass,
                                  int32_t left, int32_t top, int32_t right,
                                  int32_t bottom, bool visible);

// Array<ChildRecord>::~Array — 0x180043cb0, one of the parameterless `~Array`
// copies; registered from runtime/src/collection/array.cpp, so ResolveAddr
// hands back OUR body.
void RowsDtor(void* hdr) { RtAddr<ArrayDtorFn>(0x180043cb0ull)(hdr); }

}  // namespace

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t LayoutElement::Type() const {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* LayoutElement::TypeName() const {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@LayoutElement@UI@@UEAAXXZ   0x1800d3100
// ---------------------------------------------------------------------------
void LayoutElement::ComputeSize() {
    if ((_flags & kFlagNeedsSizeCalc) == 0) return;

    // One qword store clears both size fields.
    _layoutWidth  = 0;
    _layoutHeight = 0;

    int32_t minX = 0, maxX = 0, minY = 0, maxY = 0;

    const int32_t n = _children.GetSize();
    for (int32_t i = 0; i < n; ++i) {
        Element*     child = _children[i];
        ChildRecord* row   = _childRecords._data + i;

        // `(~child->_flags & 1) == 0` is "child IS hidden".
        const bool hidden  = (child->_flags & kFlagHidden) != 0;
        const bool visible = !(hidden && row->force == 0);

        if (!visible) {
            row->sizeX = 0;
            row->sizeY = 0;
        } else {
            RtAddr<GetPaddedSizeFn>(0x180115f10ull)(child, &row->sizeX, &row->sizeY);
        }

        RtAddr<GetChildLayoutFn>(0x1800d2dc0ull)(this, i, 1, 0, 0, 0, 0, visible);

        // Re-read the base: GetChildLayout may have grown the array.
        row = _childRecords._data + i;
        if (row->left[1]  <= minX) minX = row->left[1];
        if (maxX <= row->right[1]) maxX = row->right[1];
        if (row->top[1]   <= minY) minY = row->top[1];
        if (maxY <= row->bottom[1]) maxY = row->bottom[1];
    }

    _flags &= ~kFlagNeedsSizeCalc;
    _layoutWidth  = maxX - minX;
    _layoutHeight = maxY - minY;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@LayoutElement@UI@@UEAAXHHHH@Z   0x1800d2af0
// ---------------------------------------------------------------------------
void LayoutElement::ComputeLayout(int32_t x, int32_t y, int32_t right, int32_t bottom) {
    if (Element* border = static_cast<Element*>(_border))
        border->ComputeLayout(x, y, right, bottom);

    ComputeSize();                                            // slot 4, virtual

    const int32_t align = _alignment;
    if (right  < _layoutWidth  + x) x = right  - _layoutWidth;
    if (bottom < _layoutHeight + y) y = bottom - _layoutHeight;

    int32_t px = x + _leftPad;
    int32_t py = y + _topPad;
    const int32_t aw = (right  - _rightPad)  - px;
    const int32_t ah = (bottom - _bottomPad) - py;

    // LayoutElement takes neither stretch test and neither centring arm: the
    // only surviving case of the aligned-position switch is 2/5/8, and its
    // body re-derives the value it already has. Transcribed as shipped.
    switch (align) {
        case 2: case 5: case 8:
            px = (right - _rightPad) - aw;
            break;
        default:
            break;
    }
    if (align > 2 && align > 5 && align < 9)
        py = (bottom - _bottomPad) - ah;

    _layoutRight  = aw + px;
    _layoutLeft   = px;
    _layoutTop    = py;
    _layoutBottom = ah + py;

    int32_t minX = 10000, maxX = -10000, minY = 10000, maxY = -10000;

    int32_t n = _children.GetSize();
    for (int32_t i = 0; i < n; ++i) {
        Element*           child = _children[i];
        const ChildRecord* row0  = _childRecords._data + i;

        const bool hidden  = (child->_flags & kFlagHidden) != 0;
        const bool visible = !(hidden && row0->force == 0);

        RtAddr<GetChildLayoutFn>(0x1800d2dc0ull)(
            this, i, 0, _layoutLeft, _layoutTop, _layoutRight, _layoutBottom, visible);

        ChildRecord* row = _childRecords._data + i;
        child->ComputeLayout(row->left[0], row->top[0], row->right[0], row->bottom[0]);

        row = _childRecords._data + i;
        if (row->left[0]   <= minX) minX = row->left[0];
        if (maxX <= row->right[0])  maxX = row->right[0];
        if (row->top[0]    <= minY) minY = row->top[0];
        if (maxY <= row->bottom[0]) maxY = row->bottom[0];
        n = _children.GetSize();
    }

    const int32_t dy = _layoutTop  - minY;
    const int32_t dx = _layoutLeft - minX;

    n = _children.GetSize();
    for (int32_t i = 0; i < n; ++i) {
        Element* child = _children[i];
        child->_layoutLeft   += dx;
        child->_layoutRight  += dx;
        child->_layoutTop    += dy;
        child->_layoutBottom += dy;

        const int32_t gn = child->_children.GetSize();
        for (int32_t g = 0; g < gn; ++g) {
            Element* grand = child->_children[g];
            if (grand != nullptr) grand->AdjustUpperLeft(dx, dy);   // 0x180115840
        }
        if (Element* b = static_cast<Element*>(child->_border))
            b->AdjustUpperLeft(dx, dy);                             // 0x180115840
        n = _children.GetSize();
    }
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@LayoutElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d2a40
// ---------------------------------------------------------------------------
void LayoutElement::Draw(SpriteContext& context, uint8_t alpha) const {
    if (Element* border = static_cast<Element*>(_border))
        border->Draw(context, alpha);

    const int32_t n = _children.GetSize();
    for (int32_t i = 0; i < n; ++i) {
        Element* child = _children[i];
        if ((child->_flags & kFlagHidden) == 0) child->Draw(context, alpha);
    }
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
using UI::LayoutElement;

extern "C" uint32_t RtUi_LayoutElement_Type(const LayoutElement* self) {
    return self->UI::LayoutElement::Type();
}
extern "C" const char* RtUi_LayoutElement_TypeName(const LayoutElement* self) {
    return self->UI::LayoutElement::TypeName();
}
extern "C" void RtUi_LayoutElement_ComputeSize(LayoutElement* self) {
    self->UI::LayoutElement::ComputeSize();
}
extern "C" void RtUi_LayoutElement_ComputeLayout(LayoutElement* self, int32_t x, int32_t y,
                                                 int32_t right, int32_t bottom) {
    self->UI::LayoutElement::ComputeLayout(x, y, right, bottom);
}
extern "C" void RtUi_LayoutElement_Draw(const LayoutElement* self, void* ctx, uint8_t alpha) {
    self->UI::LayoutElement::Draw(*static_cast<UI::SpriteContext*>(ctx), alpha);
}

// ---------------------------------------------------------------------------
// ??0LayoutElement@UI@@QEAA@XZ   0x1800d32e0 — a DOOR (B4b-2)
// ---------------------------------------------------------------------------
extern "C" void* RtUi_LayoutElement_Ctor(void* selfv) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(selfv);                      // 1800d32f6  CALL 0x180115e50
    PTR(selfv, 0x00) = Vtable();             // 1800d32fb  LEA/MOV
    LayoutElement* self = reinterpret_cast<LayoutElement*>(selfv);   // B.3.6 bridge
    self->_childRecords._data     = nullptr; // +0xC0
    self->_childRecords._size     = 0;       // +0xC8  size and capacity, one qword
    self->_childRecords._capacity = 0;
    return selfv;
}

// ---------------------------------------------------------------------------
// ??1LayoutElement@UI@@UEAA@XZ   0x1800d32a0 — a DOOR (B4b-2)
// ---------------------------------------------------------------------------
extern "C" void RtUi_LayoutElement_Dtor(void* selfv) {
    PTR(selfv, 0x00) = Vtable();
    RowsDtor(&reinterpret_cast<LayoutElement*>(selfv)->_childRecords);   // B.3.6 bridge
    ElementDtor(selfv);
}


// ============================================================================
// Registration — both doors for every body.
// ============================================================================
int RtUiLayoutElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@LayoutElement@UI@@UEBAIXZ",
                          (void*)&RtUi_LayoutElement_Type);            ++n;
    RegisterRuntimeAddr(0x180043270ull, (void*)&RtUi_LayoutElement_Type);

    RegisterRuntimeExport("?TypeName@LayoutElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_LayoutElement_TypeName);        ++n;
    RegisterRuntimeAddr(0x180043290ull, (void*)&RtUi_LayoutElement_TypeName);

    RegisterRuntimeExport("??0LayoutElement@UI@@QEAA@XZ",
                          (void*)&RtUi_LayoutElement_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d32e0ull, (void*)&RtUi_LayoutElement_Ctor);

    RegisterRuntimeExport("??1LayoutElement@UI@@UEAA@XZ",
                          (void*)&RtUi_LayoutElement_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d32a0ull, (void*)&RtUi_LayoutElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@LayoutElement@UI@@UEAAXXZ",
                          (void*)&RtUi_LayoutElement_ComputeSize);     ++n;
    RegisterRuntimeAddr(0x1800d3100ull, (void*)&RtUi_LayoutElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@LayoutElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_LayoutElement_ComputeLayout);   ++n;
    RegisterRuntimeAddr(0x1800d2af0ull, (void*)&RtUi_LayoutElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@LayoutElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_LayoutElement_Draw);            ++n;
    RegisterRuntimeAddr(0x1800d2a40ull, (void*)&RtUi_LayoutElement_Draw);

    return n;
}
