#pragma once
// ============================================================================
// runtime/types/ui/SpinnerDescription.h — UI::SpinnerDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C5. RULE B6-8: this class had NO header; its layout
// lived as raw offsets in runtime/src/ui/spinnerdescription.cpp, MOVED here.
// Evidence, all in that file (read from the retail bytes when it was ported):
//   * `??0SpinnerDescription@UI@@QEAA@XZ` 0x1800d89c0 — +0x68/+0x70/+0x78 = 0
//     (qwords), +0x80 = 1 (a DWORD; +0x84 is never written);
//   * `?Serialize@` 0x1800d8880 — three ExternalPtr<ElementDescription>
//     (0x180038f90) then one kScalarInt;
//   * `?CreateInstance@` 0x1800d86b0 — the three descriptions become children
//     0, 1, 2 of the SpinnerElement (the NumberEdit, the up and the down
//     button — SpinnerElement.h), and +0x80 overwrites the element's
//     `_increment`;
//   * sizeof 0x88 — `MOV EDX,0x88` in the slot-0 vector deleting dtor
//     0x18004b050 (B36_SLOTS.md section 1).
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@SpinnerDescription@UI@@CAPEBDXZ       @0x18004ad20 -> "SpinnerDescription"
//     ?_ParentTypeName@SpinnerDescription@UI@@CAPEBDXZ @0x18004ad30 -> "ElementDescription"
//
// `??_7SpinnerDescription@UI@@6B@` @ 0x1801576c0, TEN slots:
//   [0] 0x18004b050 deleting dtor (RT_DELDTOR row)  [1] 0x18004ad60 TypeName
//   [2] 0x18004ad40 Type            [3] 0x1800d8940 GetFileVersion (0x204)
//   [4] External::Compile (inherited, NOT declared — B6-1.5)
//   [5] 0x1800d8880 Serialize       [6] 0x18004aee0 Reconstruct
//   [7] 0x18004af10 Destruct        [8] 0x1800d86b0 CreateInstance RETSLOT (B6-2)
//   [9] 0x18004af00 Construct — INTRODUCED here (B6-1.1)
//
// ABSTRACT (slot 0 `= 0`), NO real ctor. The destructor is declared only:
// `??1` (0x1800d8950) is the registered door.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"

namespace UI {

class SpinnerDescription : public ElementDescription {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x18004b050
    const char* TypeName() const override;                        // [1] 0x18004ad60
    uint32_t    Type() const override;                            // [2] 0x18004ad40
    int32_t     GetFileVersion() const override;                  // [3] 0x1800d8940
    bool        Serialize(void* stream) override;                 // [5] 0x1800d8880 (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x18004aee0
    void        Destruct() override;                              // [7] 0x18004af10
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800d86b0 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x18004af00

    SpinnerDescription(const SpinnerDescription& src);            // 0x18004af20  not ported
    SpinnerDescription& operator=(const SpinnerDescription& src); // 0x18004af80  not ported
    static uint32_t TypeId();                                     // not ported
    static Object::Base* CreateNewObject();                       // factory door
    static Object::Base* CreateInPlaceObject(void* place);        // factory door

protected:
    SpinnerDescription() = default;   // NOT ??0SpinnerDescription (0x1800d89c0, a door)
    ~SpinnerDescription();            // ??1SpinnerDescription 0x1800d8950 — the door

private:
    static const char* _TypeName();                               // 0x18004ad20
    static const char* _ParentTypeName();                         // 0x18004ad30
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    void*    _editDescription;   // +0x68  ExternalPtr<UI::ElementDescription> -> children[0]
    void*    _upDescription;     // +0x70  ExternalPtr<UI::ElementDescription> -> children[1]
    void*    _downDescription;   // +0x78  ExternalPtr<UI::ElementDescription> -> children[2]
    int32_t  _increment;         // +0x80  ctor 1 -> SpinnerElement::_increment; kScalarInt
    uint32_t _pad84;             // +0x84  written by nothing
};

static_assert(sizeof(SpinnerDescription) == 0x88,
              "UI::SpinnerDescription is 0x88 (MOV EDX,0x88 in 0x18004b050)");
static_assert(alignof(SpinnerDescription) == 8, "UI::SpinnerDescription aligns to 8");
static_assert(offsetof(SpinnerDescription, _editDescription) == sizeof(ElementDescription), "");
static_assert(offsetof(SpinnerDescription, _editDescription) == 0x68, "");
static_assert(offsetof(SpinnerDescription, _upDescription)   == 0x70, "");
static_assert(offsetof(SpinnerDescription, _downDescription) == 0x78, "");
static_assert(offsetof(SpinnerDescription, _increment)       == 0x80, "");

}  // namespace UI
