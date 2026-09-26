#pragma once
// ============================================================================
// runtime/types/ui/RadioDescription.h — UI::RadioDescription, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C2. Before this header the layout lived only as
// raw `+0x..` reads in runtime/src/ui/radiodescription.cpp (RULE B6-8); they
// are MOVED here under the class.
//
// PARENT `UI::GroupDescription` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_ParentTypeName@RadioDescription@UI@@CAPEBDXZ  @0x180047640 -> "GroupDescription"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED:
//     ?_TypeName@RadioDescription@UI@@CAPEBDXZ        @0x180047630 -> "RadioDescription"
//     ?_ParentTypeName@RadioDescription@UI@@CAPEBDXZ  @0x180047640 -> "GroupDescription"
// The derivation is also visible in the bytes: ~RadioDescription (0x1800d5b20)
// tail-jumps to ~GroupDescription and Serialize (0x1800d5a80) opens with
// GroupDescription::Serialize (0x180117d00).
//
// `??_7RadioDescription@UI@@6B@` @ 0x180157420, TEN slots:
//   [0] 0x180047930  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x180047670  TypeName
//   [2] 0x180047650  Type
//   [3] 0x1800d5b10  GetFileVersion  (`MOV EAX,0x1104`)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared (B6-1.5)
//   [5] 0x1800d5a80  Serialize
//   [6] 0x1800477f0  Reconstruct
//   [7] 0x180047820  Destruct
//   [8] 0x1800d58d0  CreateInstance(const Dialog*) -> InternalPtr<Element>  RETSLOT (B6-2)
//   [9] 0x180047810  Construct       (override of GroupDescription's slot 9)
//
// ---------------------------------------------------------------------------
// LAYOUT — 0x80 bytes (`MOV EDX,0x80` at 0x180047962), GroupDescription's 0x78
// plus ONE dword and four bytes nothing writes.
// ---------------------------------------------------------------------------
// `??0RadioDescription@UI@@QEAA@XZ` (0x1800d5b50) inlines GroupDescription's
// nineteen stores and ends with `MOV dword [RCX+0x78],-1` at 0x1800d5bdf.
//   +0x78  _activeRadio  ctor -1; one kScalarInt in Serialize (0x1800d5a80);
//                        CreateInstance (0x1800d5a13..0x1800d5a1b) compares it
//                        with and stores it into `RadioElement::_activeIndex`
//                        (+0xd0), the field `?GetActiveRadio@RadioElement@UI@@`
//                        (0x1800d56e0, `MOV EAX,[RCX+0xd0]`) returns — the
//                        name is that accessor's.
//   +0x7c  _pad7c        written by nothing (alignment tail).
//
// ABSTRACT (slot 0 `= 0`), no real ctor: `??0RadioDescription` (0x1800d5b50)
// is the door `RtUi_RadioDescription_Ctor`; `Construct` and `Reconstruct`
// tail-jump into it.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "GroupDescription.h"

namespace UI {

class Dialog;
class Element;

class RadioDescription : public GroupDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180047930
    const char* TypeName() const override;                        // [1] 0x180047670
    uint32_t    Type() const override;                            // [2] 0x180047650
    int32_t     GetFileVersion() const override;                  // [3] 0x1800d5b10
    // [4] Compile — Object::External's, inherited.
    bool        Serialize(void* stream) override;                 // [5] 0x1800d5a80  (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x1800477f0
    void        Destruct() override;                              // [7] 0x180047820
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800d58d0  RETSLOT
    Object::External* Construct() override;                       // [9] 0x180047810

    // ---- exported, NOT PORTED as members ----------------------------------
    RadioDescription(const RadioDescription& src);                // 0x180047830  not ported
    RadioDescription& operator=(const RadioDescription& src);     // 0x1800478a0  not ported
    static uint32_t TypeId();                                     // 0x180047660  not ported
    static Object::Base* CreateNewObject();                       // 0x180047760  factory door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x1800477e0  factory door

protected:
    // NOT retail's `??0RadioDescription@UI@@QEAA@XZ` (0x1800d5b50, a door).
    RadioDescription() = default;
    // `??1RadioDescription@UI@@UEAA@XZ` (0x1800d5b20). NON-VIRTUAL, NOT defined
    // in the real build; the retail body is the door `RtUi_RadioDescription_Dtor`.
    ~RadioDescription();

private:
    static const char* _TypeName();                               // 0x180047630 -> "RadioDescription"
    static const char* _ParentTypeName();                         // 0x180047640 -> "GroupDescription"
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    int32_t  _activeRadio;    // +0x78  ctor -1 (0x1800d5bdf)
    uint32_t _pad7c;          // +0x7c  written by nothing
};

static_assert(sizeof(RadioDescription) == 0x80,
              "UI::RadioDescription is 0x80 bytes (MOV EDX,0x80 at 0x180047962)");
static_assert(alignof(RadioDescription) == 8, "UI::RadioDescription aligns to 8");
static_assert(!std::has_virtual_destructor<RadioDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(RadioDescription, _activeRadio) == sizeof(GroupDescription),
              "RadioDescription's own field begins at the end of GroupDescription");
static_assert(offsetof(RadioDescription, _activeRadio) == 0x78, "");

}  // namespace UI
