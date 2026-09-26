#pragma once
// ============================================================================
// runtime/types/ui/TabDescription.h — UI::TabDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C5. RULE B6-8: this class had NO header; its layout
// lived as raw offsets in runtime/src/ui/tabdescription.cpp, MOVED here.
// Evidence, all in that file (read from the retail bytes when it was ported):
//   * `??0TabDescription@UI@@QEAA@XZ` 0x1800daa00 — every store below; it
//     jumps from +0x98 straight to +0xa8 (0x1800daa9e -> 0x1800daaa5), so
//     +0xa0 (serialized, and copied to the element) and +0xa4 are NEVER
//     written by the ctor;
//   * `?Serialize@` 0x1800da760 — three ExternalPtr<ElementDescription>
//     (0x180038f90), ExternalPtr<SpriteSheet> (0x180004590), three
//     System::Strings, kScalarInt +0xa0, then the pair array at +0xa8 (SetSize
//     0x18004cdf0, both halves of every 0x10-byte element through 0x180038f90);
//   * `?CreateInstance@` 0x1800da410 — +0x68/+0x70/+0x78 become children 0/1/2
//     of the TabElement, each pair becomes a (button, container) child pair,
//     the three names its three edge sprites, and CROSSED: +0xb0 (the pair
//     count) -> TabElement::_tabCount, +0xa0 -> TabElement::_gap. The names
//     below follow TabElement.h's paired child list;
//   * sizeof 0xb8 — `MOV EDX,0xb8` in the slot-0 vector deleting dtor
//     0x18004c640 (B36_SLOTS.md section 1).
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@TabDescription@UI@@CAPEBDXZ       @0x18004c1b0 -> "TabDescription"
//     ?_ParentTypeName@TabDescription@UI@@CAPEBDXZ @0x18004c1c0 -> "ElementDescription"
//
// `??_7TabDescription@UI@@6B@` @ 0x1801577f0, TEN slots:
//   [0] 0x18004c640 deleting dtor (RT_DELDTOR row)  [1] 0x18004c1f0 TypeName
//   [2] 0x18004c1d0 Type            [3] 0x1800da8d0 GetFileVersion (0x104)
//   [4] External::Compile (inherited, NOT declared — B6-1.5)
//   [5] 0x1800da760 Serialize       [6] 0x18004c370 Reconstruct
//   [7] 0x18004c3a0 Destruct        [8] 0x1800da410 CreateInstance RETSLOT (B6-2)
//   [9] 0x18004c390 Construct — INTRODUCED here (B6-1.1)
//
// ABSTRACT (slot 0 `= 0`), NO real ctor. The destructor is declared only:
// `??1` (0x1800da8e0) is the registered door.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"

namespace UI {

class TabDescription : public ElementDescription {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x18004c640
    const char* TypeName() const override;                        // [1] 0x18004c1f0
    uint32_t    Type() const override;                            // [2] 0x18004c1d0
    int32_t     GetFileVersion() const override;                  // [3] 0x1800da8d0
    bool        Serialize(void* stream) override;                 // [5] 0x1800da760 (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x18004c370
    void        Destruct() override;                              // [7] 0x18004c3a0
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800da410 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x18004c390

    TabDescription(const TabDescription& src);                    // 0x18004c3b0  not ported
    TabDescription& operator=(const TabDescription& src);         // 0x18004c4a0  not ported
    static uint32_t TypeId();                                     // not ported
    static Object::Base* CreateNewObject();                       // factory door
    static Object::Base* CreateInPlaceObject(void* place);        // factory door

protected:
    TabDescription() = default;   // NOT ??0TabDescription (0x1800daa00, a door)
    ~TabDescription();            // ??1TabDescription 0x1800da8e0 — the door

private:
    static const char* _TypeName();                               // 0x18004c1b0
    static const char* _ParentTypeName();                         // 0x18004c1c0
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    void*    _stripBackdropDescription;  // +0x68  ExternalPtr<ElementDescription> -> children[0]
    void*    _pageBackdropDescription;   // +0x70  -> children[1]
    void*    _pageFrameDescription;      // +0x78  -> children[2]
    void*    _spriteSheet;               // +0x80  ExternalPtr<UI::SpriteSheet>
    char*    _edgeLeftName;              // +0x88  System::String -> TabElement::_edgeLeft
    char*    _edgeMiddleName;            // +0x90  -> _edgeMiddle
    char*    _edgeRightName;             // +0x98  -> _edgeRight
    int32_t  _gap;                       // +0xa0  NOT written by the ctor -> TabElement::_gap
    uint32_t _padA4;                     // +0xa4  written by nothing
    // +0xa8 Collection::Array<{ExternalPtr<ElementDescription> button,
    // ExternalPtr<ElementDescription> container}>, 0x10 per tab. Raw header:
    // its SetSize / ~Array are addressed helpers (0x18004cdf0 / 0x18004cc90).
    void**   _tabsData;                  // +0xa8  pairs: [2i] button, [2i+1] container
    int32_t  _tabsCount;                 // +0xb0  -> TabElement::_tabCount
    int32_t  _tabsCapacity;              // +0xb4
};

static_assert(sizeof(TabDescription) == 0xb8,
              "UI::TabDescription is 0xb8 (MOV EDX,0xb8 in 0x18004c640)");
static_assert(alignof(TabDescription) == 8, "UI::TabDescription aligns to 8");
static_assert(offsetof(TabDescription, _stripBackdropDescription) == sizeof(ElementDescription), "");
static_assert(offsetof(TabDescription, _pageFrameDescription) == 0x78, "");
static_assert(offsetof(TabDescription, _spriteSheet)          == 0x80, "");
static_assert(offsetof(TabDescription, _edgeLeftName)         == 0x88, "");
static_assert(offsetof(TabDescription, _edgeRightName)        == 0x98, "");
static_assert(offsetof(TabDescription, _gap)                  == 0xa0, "");
static_assert(offsetof(TabDescription, _tabsData)             == 0xa8, "");
static_assert(offsetof(TabDescription, _tabsCount)            == 0xb0, "");

}  // namespace UI
