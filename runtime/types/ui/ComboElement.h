#pragma once
// ============================================================================
// runtime/types/ui/ComboElement.h — UI::ComboElement, the REAL C++ class
// ============================================================================
//
// Wave 7 lane D wrote the array layout below as a POD embedding the Element
// base (`RtUI::Element base;` since B.3.6 block A). ENDGAME B.3 step 6 block
// C1 turned it into the real class: it DERIVES from `UI::ButtonElement` (its
// proved parent) instead of embedding a base, adds the two description
// handles comboelement.cpp carried as file-local constants (RULE B6-8), and
// asserts retail's sizeof. The POD is gone: its only user, widgets_input.cpp,
// reaches the same names through the class (one `self->base._children` read
// there became `self->_children`).
//
// PARENT `UI::ButtonElement` — READ (B36_SLOTS.md section 3), an INTERMEDIATE
// leaf (RULE B6-9):
//     ?_ParentTypeName@ComboElement@UI@@CAPEBDXZ  @0x180074f00 -> "ButtonElement"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@ComboElement@UI@@…`
// 0x1801faa50 stays the minted record):
//     ?_TypeName@ComboElement@UI@@CAPEBDXZ        @0x180074ef0 -> "ComboElement"
//     ?_ParentTypeName@ComboElement@UI@@CAPEBDXZ  @0x180074f00 -> "ButtonElement"
//
// `??_7ComboElement@UI@@6B@` @ 0x180159030, TWELVE slots (sizeof from
// `MOV EDX,0x128` in slot 0 0x180075390). It overrides ONLY slot 7:
//
//   [ 0] 0x180075390  (unexported) vector deleting dtor — the class's own (B.3.9)
//   [ 1] 0x180074f30  TypeName
//   [ 2] 0x180074f10  Type
//   [ 3] 0x180116870  Element::Persist              INHERITED
//   [ 4] 0x180110d10  ButtonElement::ComputeSize    INHERITED
//   [ 5] 0x180110e70  ButtonElement::ComputeLayout  INHERITED
//   [ 6] 0x180111020  ButtonElement::Draw           INHERITED
//   [ 7] 0x1801125d0  Update
//   [ 8] 0x180110cc0  ButtonElement::Enable         INHERITED
//   [ 9] 0x1801169e0  Element::Contains             INHERITED
//   [10] 0x180035620  Element::SetColor             INHERITED
//   [11] 0x180035630  Element::GetBorder            INHERITED (RETSLOT)
//
// (Until ENDGAME B.3.9:) ABSTRACT, no real ctor: `??0ComboElement@UI@@QEAA@XZ` (0x1801123f0) is the
// factory_ui1.cpp door ComboDescription::CreateInstance calls, and every live
// ComboElement wears the MINTED table.
//
// ---------------------------------------------------------------------------
// LAYOUT — 0x128 bytes
// ---------------------------------------------------------------------------
// `??0ComboElement@UI@@QEAA@XZ` (0x1801123f0) inlines ButtonElement's ctor
// (stamp, +0xc0 = -1, +0xc4 = 0), stamps its own table, then (the rows in
// runtime/src/ui/factory_ui1_tables.inc, srcVa in that function):
//     +0xd8 q 0 (0x180112432)   +0xe0 q 0 (0x180112439)
//     +0xe8 q 0 (0x180112440)   +0xf0 q 0 (0x180112447)
//     +0xf8 q 0 (0x18011244e)   +0x100 q 0 (0x180112455)
//     +0x108 q 0 (0x18011245c)  +0x110 q 0 (0x180112463)
//     +0x118 DWORD -1 (0x18011246a)                 +0x120 q 0 (0x180112474)
// +0x11c is the one dword NOTHING writes (the -1 is a dword store).
//
// Three PARALLEL Collection::Array<T>, grown in lockstep by AddItem
// (0x1800750b0, three growth helpers, one per array):
//   +0xE8  _strings   Array<System::String>  data+0xE8 / size+0xF0 / cap+0xF4
//   +0xF8  _sprites   Array<UI::Sprite>      data+0xF8 / size+0x100 / cap+0x104
//   +0x108 _userData  Array<void*>           data+0x108 / size+0x110 / cap+0x114
// `?GetItemCount@` (0x180075180) reads `*(int*)(this+0xf0)`, `?GetItemData@`
// (0x180075170) reads `*(void**)(*(long*)(this+0x108) + index*8)`, and
// `?RemoveAllItems@` (0x180075120) tears all three down and resets +0x118 to -1.
// `Collection::Array<T>` is T-INDEPENDENT in size (always 16 bytes), so
// `_strings`/`_sprites` are typed `Array<void*>` purely for the CONTAINER shape;
// nothing indexes their element bytes through these types.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0ComboElement@UI@@QEAA@XZ` (0x1801123f0) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~ComboElement is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7ComboElement@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ButtonElement.h"
#include "Sprite.h"                     // UI::Sprite (the global ::Sprite is its alias)
#include "../collection/Collection.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class ComboElement : public ButtonElement {
public:
    // ??0ComboElement@UI@@QEAA@XZ 0x1801123f0 — the REAL constructor: the
    // ButtonElement stores (inlined in retail), then +0xd8..+0x110 QWORD zeros
    // (the two handles and the three arrays -- each Collection::Array's own
    // default ctor is exactly its two zero QWORDs), +0x118 DWORD -1, +0x120
    // QWORD 0 (0x180112432..0x180112474). +0x11c stays unwritten.
    ComboElement() : ButtonElement() {
        _listDesc    = nullptr;
        _itemDesc    = nullptr;
        _currentItem = -1;
        _openDialog  = nullptr;
    }

    // ---- the twelve slots, in measured order ------------------------------
    // [0] 0x180075390 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x128`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<ComboElement*>(self + static_cast<int64_t>(i) * 0x128)
                    ->~ComboElement();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~ComboElement();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    const char* TypeName() const override;                        // [1] 0x180074f30
    uint32_t    Type() const override;                            // [2] 0x180074f10
    // [4] ComputeSize, [5] ComputeLayout, [6] Draw — ButtonElement's, inherited.
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;               // [7] 0x1801125d0
    // [8] Enable — ButtonElement's, inherited.

    // ---- PORTED; the member bodies are DEFINED in widgets_input.cpp (moved
    // there by the fragment pass), whose registered doors delegate to them.
    int32_t AddItem(const UI::Sprite& sprite, const System::String& text,
                    void* userData);                              // 0x1800750b0
    int32_t GetCurrentItem() const;                               // 0x180075160
    int32_t GetItemCount() const;                                 // 0x180075180
    void*   GetItemData(int32_t index) const;                     // 0x180075170
    void    RemoveAllItems();                                     // 0x180075120
    void    SetCurrentItem(int32_t index);                        // 0x180112540

    // ---- exported, NOT PORTED anywhere in the tree: declared, deliberately
    // undefined (nothing in the tree calls them through a member).
    void DropDownClosed();                                        // 0x180112520
    System::String GetItemText(int32_t index) const;              // 0x180075190  RETSLOT

    ComboElement(const ComboElement& src);                        // 0x1800751e0  not ported (door retired: no caller)
    ComboElement& operator=(const ComboElement& src);             // 0x1800752c0  not ported

    static uint32_t TypeId();                                     // 0x180074f20  ported over the minted record
    static Object::Base* CreateNewObject();                       // 0x180075020  the class's own (B39 D2)
    static Object::Base* CreateInPlaceObject(void* place);        // 0x1800750a0  the class's own (B39 D2)

protected:
    // `??1ComboElement@UI@@UEAA@XZ` (0x180112490). NON-VIRTUAL; comboelement.cpp
    // holds retail's body (close the popup if open, destroy the three arrays
    // with their element destructors); the compiler chains the (then empty)
    // member arrays and ~ButtonElement / ~Element.
    ~ComboElement();

public:
    // `?UpdateText@ComboElement@UI@@AEAAXXZ` (0x180112180, `AEAA` = private —
    // EXPORTED, not "unexported" as widgets_input.cpp's banner says). PUBLIC
    // here only because its registered door delegates to it (fragment pass F1;
    // access is not part of the GCC ABI). Body: widgets_input.cpp.
    void UpdateText();

private:
    static const char* _TypeName();                               // 0x180074ef0 -> "ComboElement"
    static const char* _ParentTypeName();                         // 0x180074f00 -> "ButtonElement"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801faa50 (minted)

public:
    void* _listDesc;                    // +0xD8  ExternalPtr<UI::ElementDescription>: the
                                        //        popup LIST. ComboDescription::CreateInstance
                                        //        copies its +0xa0 here (0x180112b18);
                                        //        Update calls its slot 8.
    void* _itemDesc;                    // +0xE0  ExternalPtr<UI::ElementDescription>: one
                                        //        popup ITEM (desc +0xa8, 0x180112b26).
    Collection::Array<void*> _strings;  // +0xE8  element = System::String (see above)
    Collection::Array<void*> _sprites;  // +0xF8  element = UI::Sprite (see above)
    Collection::Array<void*> _userData; // +0x108 element = void*
    int32_t  _currentItem;              // +0x118 ctor -1 (dword); ?GetCurrentItem@
    uint32_t _pad11c;                   // +0x11C written by nothing
    void*    _openDialog;               // +0x120 InternalPtr<UI::ComboDialog> the popup;
                                        //        ctor 0; Update stores the new dialog
};

static_assert(sizeof(ComboElement) == 0x128,
              "UI::ComboElement is 0x128 bytes (MOV EDX,0x128 in 0x180075390)");
static_assert(alignof(ComboElement) == 8, "UI::ComboElement aligns to 8");
static_assert(!std::has_virtual_destructor<ComboElement>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<ComboElement>::value, "B39 D1: concrete");
static_assert(offsetof(ComboElement, _listDesc)    == sizeof(ButtonElement),
              "ComboElement's own fields begin at the end of ButtonElement");
static_assert(offsetof(ComboElement, _listDesc)    == 0xD8,  "");
static_assert(offsetof(ComboElement, _itemDesc)    == 0xE0,  "");
static_assert(offsetof(ComboElement, _strings)     == 0xE8,  "ComboElement::_strings @ +0xE8");
static_assert(offsetof(ComboElement, _sprites)     == 0xF8,  "ComboElement::_sprites @ +0xF8");
static_assert(offsetof(ComboElement, _userData)    == 0x108, "ComboElement::_userData @ +0x108");
static_assert(offsetof(ComboElement, _currentItem) == 0x118, "ComboElement::_currentItem @ +0x118");
static_assert(offsetof(ComboElement, _openDialog)  == 0x120, "ComboElement::_openDialog @ +0x120");

} // namespace UI
