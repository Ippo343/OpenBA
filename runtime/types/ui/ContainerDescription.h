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
// ENDGAME B.3 step 9 (block UI-B, B39_RULES D1/D2): CONCRETE. A real public
// ctor (retail's 0x1800cd060 stores), its own slot 0 and the factory pair as
// the class's own statics: every live description wears the COMPILER's table.
// The minted `??_7ContainerDescription@UI@@6B@`, its RT_DELDTOR row 0x180039c00,
// the FACTUI1 row, the ??0 / ??1 doors and the slot doors are retired
// (tools/rt_mint_retired/ui-b.txt).
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
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::ContainerDescription aliases this class

namespace UI {

class Dialog;
class Element;

class ContainerDescription : public ElementDescription {
public:
    // ??0ContainerDescription@UI@@QEAA@XZ 0x1800cd060 -- the REAL constructor:
    // ElementDescription's (inlined in retail) then the tail stores listed in
    // the banner. The vptrs are the compiler's.
    ContainerDescription() : ElementDescription() {
        _requestedWidth  = 0x64;
        _requestedHeight = 0x64;
        _clampWidth      = 0;          // one WORD store with +0x71
        _clampHeight     = 0;
        _barSize         = 0;
        _partDescs[0]    = nullptr;
        _partDescs[1]    = nullptr;
        _partDescs[2]    = nullptr;
    }

    // ---- the ten slots, in measured order ----------------------------------
    // [0] 0x180039c00 -- MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x90` = the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<ContainerDescription*>(self + static_cast<int64_t>(i) * 0x90)->~ContainerDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~ContainerDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
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
    // 0x1800398c0 -- over the minted `?_typeInfo@` record until the W3 fold.
    static uint32_t      TypeId();
    // Retail's factory pair (B39_RULES D2), containerdescription.cpp.
    static Object::Base* CreateNewObject();                       // 0x1800399c0
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180039a40

protected:
    // `??1ContainerDescription@UI@@UEAA@XZ` (0x1800ccff0). NON-VIRTUAL; retail's
    // body is the restamps and ~ElementDescription's +0x60 release, all the
    // compiler's chain (containerdescription.cpp).
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

// RtUI::ContainerDescription is an ALIAS of this class since fragment pass F2; the
// POD's offsets are the absolute asserts above.
static_assert(offsetof(ContainerDescription, _pad72)           == 0x72, "");

}  // namespace UI
