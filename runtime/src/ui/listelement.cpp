// ============================================================================
// runtime/src/ui/listelement.cpp — UI::ListElement, the whole class
// ============================================================================
//
// `??_7ListElement@UI@@6B@` (retail 0x180156dc0):
//
//   0  vector deleting dtor  0x180045320  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180044f90  here
//   2  Type                  0x180044f70  here
//   4  ComputeSize           0x1800d4380  here
//   5  ComputeLayout         0x1800d4210  here
//   6  Draw                  0x1800d3eb0  here
//   7  Update                0x1800d3c20  here
//
// plus ??0ListElement (0x1800d4570) and ??1ListElement (0x1800d4530).
//
// NOT HERE: `?DrawHighlight@ListElement@UI@@QEBAXAEAVSpriteContext@2@HI@Z`
// (0x1800d3f70), `?AddItem@` (0x1800d44b0) and `?GetCellLayout@` (0x1800d4120)
// — the UI non-slot callee list is another agent's. Draw REACHES DrawHighlight
// by address.
//
// ---------------------------------------------------------------------------
// LAYOUT.  sizeof(ListElement) == 0x100 (the deleting destructor's
// `MOV EDX,0x100` at 0x180045352). Fields, each from the ctor (0x1800d4570) or
// from the body that reads it:
//
//   +0xC0  _itemRows      Array-ish head, ctor zeroes the qword
//   +0xC8  _rowCount      int32, ctor -1
//   +0xD0  _hoverColor    uint32, ctor 0xff808080   } one qword store,
//   +0xD4  _selectColor   uint32, ctor 0xffffffff   } 0xffffffffff808080
//   +0xD8  _selectedIndex int32, ctor -1            } one qword store
//   +0xDC  _hoverIndex    int32, ctor -1            }
//   +0xE0  _items         Array<...>, the ONE member ~ListElement destroys
//   +0xF0  _grid          UI::RibbonElement*  (Update reads its +0xC0 vertical
//                         byte, +0xC4 spacing and +0xC8 int array — exactly
//                         RibbonElement's own three fields, which is how the
//                         type is known and not guessed)
//   +0xF8  _clip          UI::ClipRegionElement*, the visible window
//
// Update's own two children are _children[0] (drawn first, the backdrop) and
// _children[1] (the content, and the one ComputeSize measures).
//
// ENDGAME B.3.6 block C4: the layout above moved to runtime/types/ui/
// ListElement.h (the real class). Every slot body below is a MEMBER; each
// registered door is a one-line QUALIFIED delegation; ctor/dtor stay DOORS.
// The ribbon's own fields (UI::RibbonElement, block C5) and the dialog's slot
// 8 (UI::DialogInstance, block B) stay raw here — fragment list.
// RULE B6-5: the vtable and type-info record are cached in namespace-scope
// std::atomic slots.
// ============================================================================

#include <atomic>
#include <new>

#include "widget_common.h"
#include "../../types/ui/ListElement.h"

using namespace rtui;

namespace {

// The RibbonElement-shaped fields Update reads through _ribbon (block C5's).
constexpr int kGridVertical = 0xC0;   // byte
constexpr int kGridSpacing  = 0xC4;   // int32
constexpr int kGridExtents  = 0xC8;   // Array<int32>::_data

std::atomic<void*> g_typeInfo{nullptr};

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
        g_typeInfo, "?_typeInfo@ListElement@UI@@0VBaseTypeInfo@Object@@A"));
}

// `?DrawHighlight@ListElement@UI@@QEBAXAEAVSpriteContext@2@HI@Z` 0x1800d3f70 —
// declared on the class, body still element_bodies_b5.cpp's door.
using DrawHighlightFn = void (*)(const void* self, void* ctx, int32_t index, uint32_t color);

// `?OnElementEvent@DialogInstance@UI@@` — the DialogInstance vtable's slot 8
// (+0x40). Seven arguments after `this`, read straight off 0x1800d3e2a..3e5e:
// RDX = &InternalPtr<Element>, R8D = event kind (0x14), R9D = the element id,
// [RSP+0x20] = the row index, and three zeroed dwords/qwords.
using OnElementEventFn = void (*)(void* dlg, void** elem, int32_t kind, int32_t elemId,
                                  int32_t a, int32_t b, int32_t c, int64_t d);

}  // namespace

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t ListElement::Type() const {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// static -- the same read as slot 2 (the minted record until W3).
uint32_t ListElement::TypeId() {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* ListElement::TypeName() const {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// slot 4 — 0x1800d4380
void ListElement::ComputeSize() {
    if ((_flags & kFlagNeedsSizeCalc) == 0) return;
    Element* content = _children[1];
    RtAddr<GetPaddedSizeFn>(0x180115f10ull)(content, &_layoutWidth, &_layoutHeight);
    _flags &= ~kFlagNeedsSizeCalc;
}

// slot 5 — 0x1800d4210
void ListElement::ComputeLayout(int32_t x, int32_t y, int32_t right, int32_t bottom) {
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

    switch (align) {
        case 2: case 5: case 8:
            px = (right - _rightPad) - aw;
            break;
        default:
            break;
    }
    if (align > 2 && align > 5 && align < 9)
        py = (bottom - _bottomPad) - ah;

    _layoutLeft   = px;
    _layoutRight  = aw + px;
    _layoutTop    = py;
    _layoutBottom = ah + py;

    int32_t n = _children.GetSize();
    for (int32_t i = 0; i < n; ++i) {
        Element* child = _children[i];
        if (child != nullptr && (child->_flags & kFlagHidden) == 0)
            child->ComputeLayout(_layoutLeft, _layoutTop, _layoutRight, _layoutBottom);
        n = _children.GetSize();
    }
}

// slot 6 — 0x1800d3eb0
void ListElement::Draw(SpriteContext& context, uint8_t alpha) const {
    if (Element* border = static_cast<Element*>(_border))
        border->Draw(context, alpha);

    Element* back = _children[0];
    if (back != nullptr && (back->_flags & kFlagHidden) == 0)
        back->Draw(context, alpha);

    RtAddr<DrawHighlightFn>(0x1800d3f70ull)(this, &context, _hoverIndex, _hoverColor);
    RtAddr<DrawHighlightFn>(0x1800d3f70ull)(this, &context, _selectedIndex, _selectColor);

    Element* content = _children[1];
    if (content != nullptr && (content->_flags & kFlagHidden) == 0)
        content->Draw(context, alpha);
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@ListElement@UI@@ ...   0x1800d3c20
//
// ONE stack slot holds two different things in sequence, and that is retail's
// own reuse, not a simplification: [RSP+0x70] first holds the caller's
// InternalPtr<DialogInstance> copy that Element::Update is handed (0x1800d3c40),
// and later `this` as an InternalPtr<Element> for the event (0x1800d3e37).
// ---------------------------------------------------------------------------
void ListElement::Update(const System::Time& time, UserInterface* ui,
                         Object::InternalPtr<DialogInstance>* dialog,
                         const UpdateState& state) {
    void* slot = *reinterpret_cast<void* const*>(dialog);      // 1800d3c2a / 3c40
    UI::Element::Update(time, ui,
                        reinterpret_cast<Object::InternalPtr<DialogInstance>*>(&slot),
                        state);                                // 1800d3c4a (qualified)

    _hoverIndex = -1;                                          // 1800d3c56
    if ((_flags & kFlagInteractive) == 0) return;              // TEST ...,0x20000

    const int32_t mouseX = state.mouseX;
    const int32_t mouseY = state.mouseY;

    for (int32_t i = 0; i < _ribbon->_children.GetSize(); ++i) {
        if (i < 0) continue;                                   // TEST EBX,EBX / JS

        Element* grid = _ribbon;

        // The running offset of row i: the sum of the previous rows' extents
        // plus one spacing each. Retail unrolls this by four; the rolled form
        // computes the same sum in the same order.
        int32_t off = 0;
        for (int32_t k = 0; k < i; ++k)
            off += reinterpret_cast<int32_t*>(PTR(grid, kGridExtents))[k]
                   + I32(grid, kGridSpacing);

        int32_t x0 = grid->_layoutLeft;
        int32_t y0 = grid->_layoutTop;
        int32_t x1, y1;
        if (U8(grid, kGridVertical) == 0) {
            y1 = grid->_layoutBottom;
            x0 = x0 + off;
            x1 = reinterpret_cast<int32_t*>(PTR(grid, kGridExtents))[i] + x0;
        } else {
            x1 = grid->_layoutRight;
            y0 = y0 + off;
            y1 = reinterpret_cast<int32_t*>(PTR(grid, kGridExtents))[i] + y0;
        }

        Element* clip = _clip;
        int32_t cl = clip->_layoutLeft;
        int32_t cr = clip->_layoutRight;
        int32_t ct = clip->_layoutTop;
        int32_t cb = clip->_layoutBottom;

        if (cl <= x1 && x0 <= cr && ct <= y1 && y0 <= cb) {
            if (cb < y1) y1 = cb + 1;
            if (y0 < ct) y0 = ct;
            if (cr < x1) x1 = cr + 1;
            if (x0 < cl) x0 = cl;
            if (U8(grid, kGridVertical) != 0) {
                // A vertical list hit-tests the full clip width and the
                // clamped row band; a horizontal one the clamped row band and
                // the full clip height. Retail expresses that by overwriting
                // four registers here rather than by two separate tests.
                x1 = cr;
                cb = y1;
                ct = y0;
                x0 = cl;
            }
            if (x0 <= mouseX && mouseX <= x1 && ct <= mouseY && mouseY <= cb) {
                _hoverIndex = i;
                if (state.buttonPressed != 0) {
                    _selectedIndex = i;
                    slot = this;                                   // 1800d3e37
                    void* dlg = *reinterpret_cast<void* const*>(dialog);   // 1800d3e2a
                    // UI::DialogInstance slot 8 (block B): hand-inlined, as before.
                    reinterpret_cast<OnElementEventFn>(VTable(dlg)[0x40 / 8])(
                        dlg, &slot, 0x14, _elementId, i, 0, 0, 0);
                }
            }
        }
    }
}

// ??1ListElement@UI@@UEAA@XZ   0x1800d4530 — retail's body, moved from the
// retired door (ENDGAME B.3.9 UI-B): the user-data Array through 0x180024aa0
// (frees and zeroes the header; the compiler's `_userData.~Array()` is then a
// no-op). The restamp and the tail into ~Element are the compiler's.
ListElement::~ListElement() {
    RtAddr<ArrayDtorFn>(0x180024aa0ull)(&_userData);
}

// ?CreateNewObject@ListElement@UI@@SAPEAVBase@Object@@XZ 0x180045080 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0x100, align 8; retail's null
// branch; the ctor. Replaces the factory interpreter row (B39_RULES D2).
Object::Base* ListElement::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ListElement), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ListElement();
}
// ?CreateInPlaceObject@ListElement@UI@@SAPEAVBase@Object@@PEAX@Z 0x180045100
Object::Base* ListElement::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ListElement();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors, the ctor / dtor doors and the
// factory-interpreter row are gone. Their only readers were the minted
// `??_7ListElement@UI@@6B@`, its RT_DELDTOR row and FACTUI2: every live
// ListElement now wears the compiler's table.
// ============================================================================

int RtUiListElementRegister() {
    int n = 0;
    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address; the minted `?_typeInfo@ListElement` record resolves them.
    RegisterRuntimeExport("?CreateNewObject@ListElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::ListElement::CreateNewObject);      ++n;
    RegisterRuntimeAddr(0x180045080ull, (void*)&UI::ListElement::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@ListElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::ListElement::CreateInPlaceObject);  ++n;
    RegisterRuntimeAddr(0x180045100ull, (void*)&UI::ListElement::CreateInPlaceObject);
    return n;
}
