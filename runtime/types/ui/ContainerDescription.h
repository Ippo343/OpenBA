#pragma once
// ============================================================================
// runtime/types/ui/ContainerDescription.h — UI::ContainerDescription, the REAL
// C++ class (ENDGAME B.3 step 6, block C4)
// ============================================================================
//
// PARENT `UI::ElementDescription` — READ, not inferred:
//     ?_ParentTypeName@ContainerDescription@UI@@CAPEBDXZ @0x1800398a0 -> "ElementDescription"
//
// RULE B5-15 / B6-7 — THE TWO NAME LITERALS, RECORDED, NOT USED (no
// `DEFINE_TYPE` in step 6; slots 1/2 keep reading the minted
// `?_typeInfo@ContainerDescription@UI@@0VBaseTypeInfo@Object@@A`, 0x1801f9da0):
//     ?_TypeName@ContainerDescription@UI@@CAPEBDXZ       @0x180039890 -> "ContainerDescription"
//     ?_ParentTypeName@ContainerDescription@UI@@CAPEBDXZ @0x1800398a0 -> "ElementDescription"
//
// `??_7ContainerDescription@UI@@6B@` @ 0x180156510, TEN slots (B36_SLOTS.md
// section 1; sizeof from `MOV EDX,0x90` in slot 0):
//   [0] 0x180039c00  vector deleting dtor  (deleting_dtor.cpp row)
//   [1] 0x1800398d0  TypeName        [2] 0x1800398b0  Type
//   [3] 0x1800ccfe0  GetFileVersion  [4] 0x1800066e0  External::Compile INHERITED
//   [5] 0x1800cced0  Serialize       [6] 0x180039a50  Reconstruct
//   [7] 0x180039a80  Destruct
//   [8] 0x1800ccb90  CreateInstance  RETSLOT (RULE B6-2)
//   [9] 0x180039a70  Construct       INTRODUCED by the leaf (RULE B6-1.1)
//
// ABSTRACT (slot 0 pure; its body is the RT_DELDTOR row). Every live one is
// built by the `??0ContainerDescription` DOOR (factory_ui1.cpp) and wears the
// MINTED table, so every door in containerdescription.cpp delegates with a
// QUALIFIED call. No real constructor (B4b-2).
//
// LAYOUT — the layout was already documented by the POD
// `RtUI::ContainerDescription` in DescriptionsBatch1.h (0x90, agrees with
// B36_SLOTS). That header is shared by four clusters and is NOT edited here
// (RULE B6-4.4): this class repeats the tail under names taken from what
// `CreateInstance` (0x1800ccb90) copies each field INTO on the ContainerElement
// (runtime/types/ui/ContainerElement.h), and cross-asserts every offset against
// the POD. The base already owns `_width`/`_height`/`_childDesc`, so the POD's
// tail names cannot be reused without hiding them.
//
// Stores of `??0ContainerDescription@UI@@QEAA@XZ` (0x1800cd060), tail:
//   MOV dword [RCX+0x68],0x64 ; MOV dword [RCX+0x6c],0x64 ; MOV word [RCX+0x70],DX
//   MOV dword [RCX+0x74],EDX  ; MOV qword [RCX+0x78/0x80/0x88],RDX
// +0x72/+0x73 are written by nothing: an explicit pad.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"
#include "DescriptionsBatch1.h"   // RtUI::ContainerDescription — cross-assert only

namespace UI {

class Dialog;
class Element;

class ContainerDescription : public ElementDescription {
public:
    // ---- the ten slots, in measured order ----------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180039c00
    const char*   TypeName() const override;                      // [1] 0x1800398d0
    uint32_t      Type() const override;                          // [2] 0x1800398b0
    int32_t       GetFileVersion() const override;                // [3] 0x1800ccfe0
    // [4] Compile — Object::External's own body, inherited unchanged.
    bool          Serialize(void* stream) override;               // [5] 0x1800cced0  (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x180039a50
    void          Destruct() override;                            // [7] 0x180039a80
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800ccb90 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180039a70

    // ---- exported, not ported: declared, deliberately undefined ------------
    ContainerDescription(const ContainerDescription& src);        // 0x180039a90
    ContainerDescription& operator=(const ContainerDescription&); // 0x180039b10
    static uint32_t      TypeId();                                // 0x1800398c0
    static Object::Base* CreateNewObject();                       // 0x1800399c0  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180039a40  factory_ui1.cpp door

protected:
    // NOT retail's `??0ContainerDescription@UI@@QEAA@XZ` (0x1800cd060, the
    // factory_ui1.cpp door).
    ContainerDescription() = default;
    // `??1ContainerDescription@UI@@UEAA@XZ` (0x1800ccff0). NON-VIRTUAL and NOT
    // defined in the real build; the retail body is the registered door
    // `RtUi_ContainerDescription_Dtor`.
    ~ContainerDescription();

private:
    static const char* _TypeName();                               // 0x180039890 -> "ContainerDescription"
    static const char* _ParentTypeName();                         // 0x1800398a0 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801f9da0 (minted)

public:
    // ---- the 0x28 bytes after ElementDescription ---------------------------
    // Serialize's ORDER is +0x68, +0x6c, +0x74, the three descriptions, and only
    // then +0x70/+0x71 — see containerdescription.cpp.
    int32_t  _requestedWidth;     // +0x68  ctor 0x64; -> ContainerElement::_requestedWidth (+0xc0)
    int32_t  _requestedHeight;    // +0x6c  ctor 0x64; -> ContainerElement::_requestedHeight (+0xc4)
    uint8_t  _clampWidth;         // +0x70  kScalarBool; -> ContainerElement +0xcc (BYTE)
    uint8_t  _clampHeight;        // +0x71  kScalarBool; -> ContainerElement +0xcd (BYTE)
    uint8_t  _pad72[2];           // +0x72  written by nothing
    int32_t  _barSize;            // +0x74  kScalarInt; -> ContainerElement::_barSize (+0xc8)
    // ExternalPtr<UI::ElementDescription> x3 (0x180038f90 each). CreateInstance
    // hands [2] to the clip region, then [1] and [0] to the container, in that
    // DESCENDING order.
    void*    _partDescs[3];       // +0x78, +0x80, +0x88
};

static_assert(sizeof(ContainerDescription) == 0x90,
              "UI::ContainerDescription is 0x90 bytes (MOV EDX,0x90 in its slot-0 "
              "vector deleting destructor 0x180039c00)");
static_assert(alignof(ContainerDescription) == 8, "UI::ContainerDescription aligns to 8");
static_assert(offsetof(ContainerDescription, _requestedWidth) == sizeof(ElementDescription), "");
static_assert(offsetof(ContainerDescription, _requestedWidth)  == 0x68, "");
static_assert(offsetof(ContainerDescription, _requestedHeight) == 0x6c, "");
static_assert(offsetof(ContainerDescription, _clampWidth)      == 0x70, "");
static_assert(offsetof(ContainerDescription, _clampHeight)     == 0x71, "");
static_assert(offsetof(ContainerDescription, _barSize)         == 0x74, "");
static_assert(offsetof(ContainerDescription, _partDescs)       == 0x78, "");

// The class and the DescriptionsBatch1.h POD describe the same bytes.
static_assert(sizeof(ContainerDescription) == sizeof(RtUI::ContainerDescription), "");
static_assert(offsetof(ContainerDescription, _requestedWidth)  == offsetof(RtUI::ContainerDescription, _width), "");
static_assert(offsetof(ContainerDescription, _requestedHeight) == offsetof(RtUI::ContainerDescription, _height), "");
static_assert(offsetof(ContainerDescription, _clampWidth)      == offsetof(RtUI::ContainerDescription, _flag70), "");
static_assert(offsetof(ContainerDescription, _clampHeight)     == offsetof(RtUI::ContainerDescription, _flag71), "");
static_assert(offsetof(ContainerDescription, _pad72)           == offsetof(RtUI::ContainerDescription, _pad72), "");
static_assert(offsetof(ContainerDescription, _barSize)         == offsetof(RtUI::ContainerDescription, _word74), "");
static_assert(offsetof(ContainerDescription, _partDescs)       == offsetof(RtUI::ContainerDescription, _childDesc), "");

}  // namespace UI
