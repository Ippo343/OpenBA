#pragma once
// ============================================================================
// runtime/types/ui/PageDescription.h — UI::PageDescription, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C2. Before this header the layout lived only as
// raw `+0x..` reads in runtime/src/ui/pagedescription.cpp (RULE B6-8); they are
// MOVED here under the class.
//
// PARENT `UI::GroupDescription` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_ParentTypeName@PageDescription@UI@@CAPEBDXZ  @0x180046eb0 -> "GroupDescription"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED:
//     ?_TypeName@PageDescription@UI@@CAPEBDXZ        @0x180046ea0 -> "PageDescription"
//     ?_ParentTypeName@PageDescription@UI@@CAPEBDXZ  @0x180046eb0 -> "GroupDescription"
// The derivation is also visible in the bytes: ~PageDescription (0x1800d54b0)
// tail-jumps to ~GroupDescription (0x180117c00) and Serialize (0x1800d5410)
// opens with GroupDescription::Serialize (0x180117d00).
//
// `??_7PageDescription@UI@@6B@` @ 0x1801570e0, TEN slots:
//   [0] 0x1800471a0  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x180046ee0  TypeName
//   [2] 0x180046ec0  Type
//   [3] 0x1800d54a0  GetFileVersion  (`return 0x1104`)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared (B6-1.5)
//   [5] 0x1800d5410  Serialize
//   [6] 0x180047060  Reconstruct
//   [7] 0x180047090  Destruct
//   [8] 0x1800d52b0  CreateInstance(const Dialog*) -> InternalPtr<Element>  RETSLOT (B6-2)
//   [9] 0x180047080  Construct       (override of GroupDescription's slot 9)
//
// ---------------------------------------------------------------------------
// LAYOUT — 0x80 bytes (`MOV EDX,0x80` at 0x1800471d2), GroupDescription's 0x78
// plus ONE dword and four bytes nothing writes.
// ---------------------------------------------------------------------------
// `??0PageDescription@UI@@QEAA@XZ` (0x1800d54e0) inlines GroupDescription's
// nineteen stores and ends with `MOV dword [RCX+0x78],EDX` (EDX zero) at
// 0x1800d556f — a DWORD, so +0x7c is never written by any ctor.
//   +0x78  _activePage  ctor 0; one kScalarInt in Serialize (0x1800d5410);
//                       CreateInstance's tail passes it to
//                       `?SetActivePage@PageElement@UI@@` (0x1800d53df), which
//                       stores it into `PageElement::_activePage` (+0xc0) —
//                       the name is that member's.
//   +0x7c  _pad7c       written by nothing (alignment tail).
//
// ABSTRACT (slot 0 `= 0`), no real ctor: `??0PageDescription` (0x1800d54e0)
// is the door `RtUi_PageDescription_Ctor`, which `Construct`/`Reconstruct`
// call and ui_factory_wave28.cpp's factory steps reach.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "GroupDescription.h"

namespace UI {

class Dialog;
class Element;

class PageDescription : public GroupDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x1800471a0
    const char* TypeName() const override;                        // [1] 0x180046ee0
    uint32_t    Type() const override;                            // [2] 0x180046ec0
    int32_t     GetFileVersion() const override;                  // [3] 0x1800d54a0
    // [4] Compile — Object::External's, inherited.
    bool        Serialize(void* stream) override;                 // [5] 0x1800d5410  (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x180047060
    void        Destruct() override;                              // [7] 0x180047090
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800d52b0  RETSLOT
    Object::External* Construct() override;                       // [9] 0x180047080

    // ---- exported, NOT PORTED as members ----------------------------------
    PageDescription(const PageDescription& src);                  // 0x1800470a0  not ported
    PageDescription& operator=(const PageDescription& src);       // 0x180047110  not ported
    static uint32_t TypeId();                                     // 0x180046ed0  not ported
    static Object::Base* CreateNewObject();                       // 0x180046fd0  factory door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180047050  factory door

protected:
    // NOT retail's `??0PageDescription@UI@@QEAA@XZ` (0x1800d54e0, a door).
    PageDescription() = default;
    // `??1PageDescription@UI@@UEAA@XZ` (0x1800d54b0). NON-VIRTUAL, NOT defined
    // in the real build; the retail body is the door `RtUi_PageDescription_Dtor`.
    ~PageDescription();

private:
    static const char* _TypeName();                               // 0x180046ea0 -> "PageDescription"
    static const char* _ParentTypeName();                         // 0x180046eb0 -> "GroupDescription"
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    int32_t  _activePage;     // +0x78  ctor 0 (0x1800d556f, DWORD store)
    uint32_t _pad7c;          // +0x7c  written by nothing
};

static_assert(sizeof(PageDescription) == 0x80,
              "UI::PageDescription is 0x80 bytes (MOV EDX,0x80 at 0x1800471d2)");
static_assert(alignof(PageDescription) == 8, "UI::PageDescription aligns to 8");
static_assert(!std::has_virtual_destructor<PageDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(PageDescription, _activePage) == sizeof(GroupDescription),
              "PageDescription's own field begins at the end of GroupDescription");
static_assert(offsetof(PageDescription, _activePage) == 0x78, "");

}  // namespace UI
