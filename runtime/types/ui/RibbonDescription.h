#pragma once
// ============================================================================
// runtime/types/ui/RibbonDescription.h — UI::RibbonDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C5. RULE B6-8: this class had NO header; its layout
// lived as raw offsets in runtime/src/ui/ribbondescription.cpp, MOVED here.
// Evidence, all in that file (read from the retail bytes when it was ported):
//   * the inlined ctor 0x18004448a..0x180044501 (`??0RibbonDescription@UI@@
//     QEAA@XZ` 0x1800d5bf0 is the same stores — ui_factory_wave28.cpp's
//     Ctor_RibbonDescription): ONE 16-bit store at +0x68 (both bools), a dword
//     at +0x6c, two qwords at +0x70/+0x78 — so +0x6a..+0x6b is never written;
//   * `?Serialize@` 0x1800d5f00 — kScalarBool +0x68, kScalarBool +0x69,
//     kScalarInt +0x6c, then the Array<ExternalPtr<ElementDescription>> at
//     +0x70 (SetSize 0x1800492d0, elements 0x1800493d0);
//   * `?CreateInstance@` 0x1800d6000 — +0x68 -> RibbonElement +0xc0,
//     +0x6c -> +0xc4, +0x69 -> +0xc1, and one child per array entry. The names
//     below are the RibbonElement fields each one seeds (RibbonElement.h), the
//     convention ElementDescription.h's banner uses for its own +0x20..+0x34.
//   * sizeof 0x80 — `MOV EDX,0x80` in the slot-0 vector deleting dtor
//     0x180044620 (B36_SLOTS.md section 1).
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@RibbonDescription@UI@@CAPEBDXZ       @0x1800440f0 -> "RibbonDescription"
//     ?_ParentTypeName@RibbonDescription@UI@@CAPEBDXZ @0x180044100 -> "ElementDescription"
//
// `??_7RibbonDescription@UI@@6B@` @ 0x180156ed0, TEN slots:
//   [0] 0x180044620 deleting dtor (RT_DELDTOR row)  [1] 0x180044130 TypeName
//   [2] 0x180044110 Type            [3] 0x1800d5ef0 GetFileVersion (0x204)
//   [4] External::Compile (inherited, NOT declared — B6-1.5)
//   [5] 0x1800d5f00 Serialize       [6] 0x1800443c0 Reconstruct
//   [7] 0x180044520 Destruct        [8] 0x1800d6000 CreateInstance RETSLOT (B6-2)
//   [9] 0x180044470 Construct — INTRODUCED here (B6-1.1)
//
// ABSTRACT (slot 0 `= 0`), NO real ctor. The destructor is declared only:
// `??1` (0x1800d5c90) is the registered door.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"

namespace UI {

class RibbonDescription : public ElementDescription {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180044620
    const char* TypeName() const override;                        // [1] 0x180044130
    uint32_t    Type() const override;                            // [2] 0x180044110
    int32_t     GetFileVersion() const override;                  // [3] 0x1800d5ef0
    bool        Serialize(void* stream) override;                 // [5] 0x1800d5f00 (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x1800443c0
    void        Destruct() override;                              // [7] 0x180044520
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800d6000 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180044470

    RibbonDescription(const RibbonDescription& src);              // 0x180044530  not ported
    RibbonDescription& operator=(const RibbonDescription& src);   // 0x1800445a0  not ported
    static uint32_t TypeId();                                     // not ported
    static Object::Base* CreateNewObject();                       // factory door
    static Object::Base* CreateInPlaceObject(void* place);        // factory door

protected:
    RibbonDescription() = default;   // NOT ??0RibbonDescription (0x1800d5bf0, a door)
    ~RibbonDescription();            // ??1RibbonDescription 0x1800d5c90 — the door

private:
    static const char* _TypeName();                               // 0x1800440f0
    static const char* _ParentTypeName();                         // 0x180044100
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    uint8_t  _vertical;        // +0x68  kScalarBool -> RibbonElement::_vertical (+0xc0)
    uint8_t  _uniform;         // +0x69  kScalarBool -> RibbonElement::_uniform  (+0xc1)
    uint8_t  _pad6a[2];        // +0x6a  written by nothing
    int32_t  _spacing;         // +0x6c  kScalarInt  -> RibbonElement::_spacing  (+0xc4)
    // +0x70 Collection::Array<ExternalPtr<UI::ElementDescription>>, one child
    // element per entry. Raw header: its SetSize / Serialize / ~Array are
    // addressed helpers (0x1800492d0 / 0x1800493d0 / 0x180039440).
    void**   _itemsData;       // +0x70
    int32_t  _itemsCount;      // +0x78
    int32_t  _itemsCapacity;   // +0x7c
};

static_assert(sizeof(RibbonDescription) == 0x80,
              "UI::RibbonDescription is 0x80 (MOV EDX,0x80 in 0x180044620)");
static_assert(alignof(RibbonDescription) == 8, "UI::RibbonDescription aligns to 8");
static_assert(offsetof(RibbonDescription, _vertical) == sizeof(ElementDescription), "");
static_assert(offsetof(RibbonDescription, _uniform)    == 0x69, "");
static_assert(offsetof(RibbonDescription, _spacing)    == 0x6c, "");
static_assert(offsetof(RibbonDescription, _itemsData)  == 0x70, "");
static_assert(offsetof(RibbonDescription, _itemsCount) == 0x78, "");

}  // namespace UI
