#pragma once
// ============================================================================
// runtime/types/ui/ContainerElement.h — UI::ContainerElement, the REAL C++
// class (ENDGAME B.3 step 6, block C4)
// ============================================================================
//
// PARENT `UI::Element` — READ, not inferred:
//     ?_ParentTypeName@ContainerElement@UI@@CAPEBDXZ @0x180039c90 -> "Element"
//
// RULE B5-15 / B6-7 — THE TWO NAME LITERALS, RECORDED, NOT USED (slots 1/2 keep
// reading the minted `?_typeInfo@ContainerElement@UI@@…`, 0x1801f9dd8):
//     ?_TypeName@ContainerElement@UI@@CAPEBDXZ       @0x180039c80 -> "ContainerElement"
//     ?_ParentTypeName@ContainerElement@UI@@CAPEBDXZ @0x180039c90 -> "Element"
//
// `??_7ContainerElement@UI@@6B@` @ 0x1801564b0, TWELVE slots (B36_SLOTS.md
// section 1; sizeof from `MOV EDX,0xd8` in slot 0 0x18003a050):
//   [0] 0x18003a050  vector deleting dtor (deleting_dtor.cpp row)
//   [1] 0x180039cc0  TypeName      [2] 0x180039ca0  Type
//   [3] 0x180116870  Element::Persist        INHERITED
//   [4] 0x1800cc850  ComputeSize   [5] 0x1800cc620  ComputeLayout
//   [6] 0x1800cc590  Draw          [7] 0x1800cc2d0  Update
//   [8] Enable, [9] Contains, [10] SetColor, [11] GetBorder  INHERITED from Element
//
// ABSTRACT (slot 0 pure), no real ctor: every live one is built by the
// `??0ContainerElement` DOOR (containerelement.cpp) or inline by
// `ContainerDescription::CreateInstance`, and wears the MINTED table.
//
// LAYOUT (RULE B6-8) — MOVED here from containerelement.cpp's file-local
// constants. Evidence: `??0ContainerElement@UI@@QEAA@XZ` (0x1800ccb40), read as
// disassembly:
//     CALL ??0Element ; LEA RAX,[0x1801564b0] ; MOV [RBX],RAX ; XOR EAX,EAX
//     MOV dword [RBX+0xc8],EAX ; MOV word [RBX+0xcc],AX ; MOV dword [RBX+0xd0],EAX
// and the one-instruction accessors `?GetWidth@` (0x180039ee0, `MOV EAX,
// [RCX+0xc0]`), `?GetHeight@` (0x180039ef0, +0xc4), `?SetSize@` (0x180039eb0:
// +0xc0 = w, +0xc4 = h, then the needs-size-calc walk).
//   +0xc0/+0xc4 are NOT written by the ctor: ContainerDescription::CreateInstance
//     writes them before anything reads them.
//   +0xce/+0xcf and +0xd4..+0xd7 are written by nothing: explicit pads.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "Element.h"

namespace UI {

class ContainerElement : public Element {
public:
    // ---- the twelve slots, in measured order -------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x18003a050
    const char* TypeName() const override;                        // [1] 0x180039cc0
    uint32_t    Type() const override;                            // [2] 0x180039ca0
    // [3] Persist — Element's, inherited.
    void ComputeSize() override;                                  // [4] 0x1800cc850
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x1800cc620
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1800cc590
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;               // [7] 0x1800cc2d0
    // [8]..[11] — Element's, inherited.

    // ---- non-virtual members -----------------------------------------------
    int32_t GetWidth() const  { return _requestedWidth; }         // 0x180039ee0 MOV EAX,[RCX+0xc0]
    int32_t GetHeight() const { return _requestedHeight; }        // 0x180039ef0 MOV EAX,[RCX+0xc4]
    // PORTED, body in ANOTHER BLOCK'S FILE (element.cpp, `RtUi_ContainerElement_
    // SetOffsets`) — fragment pass; declared so no later block edits this header.
    void SetOffsets(int32_t x, int32_t y);                        // 0x1800cc1e0
    // Exported, not ported (inlined by ComboDialog's ctor): declared, undefined.
    void SetSize(int32_t w, int32_t h);                           // 0x180039eb0

    ContainerElement(const ContainerElement& src);                // 0x180039f00  containerelement.cpp door
    ContainerElement& operator=(const ContainerElement& src);     // 0x180039f90  not ported
    static uint32_t      TypeId();                                // 0x180039cb0  not ported
    static Object::Base* CreateNewObject();                       // 0x180039db0  not ported
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180039e50  not ported

protected:
    // NOT retail's `??0ContainerElement@UI@@QEAA@XZ` (0x1800ccb40, the door).
    ContainerElement() = default;
    // `??1ContainerElement@UI@@UEAA@XZ` (0x1800ccb10): non-virtual, not defined
    // in the real build; the retail body is the door `RtUi_ContainerElement_Dtor`.
    ~ContainerElement();

private:
    static const char* _TypeName();                               // 0x180039c80 -> "ContainerElement"
    static const char* _ParentTypeName();                         // 0x180039c90 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801f9dd8 (minted)

public:
    // ---- the 0x18 bytes after Element --------------------------------------
    int32_t  _requestedWidth;    // +0xc0  NOT written by the ctor; GetWidth
    int32_t  _requestedHeight;   // +0xc4  NOT written by the ctor; GetHeight
    int32_t  _barSize;           // +0xc8  ctor 0; the scroll-bar gutter
    uint8_t  _clampWidth;        // +0xcc  ctor 0 (one WORD store with +0xcd)
    uint8_t  _clampHeight;       // +0xcd  ctor 0
    uint8_t  _padce[2];          // +0xce  written by nothing
    int32_t  _wheelImpulse;      // +0xd0  ctor 0; ±7 per wheel event, spent one per frame
    uint32_t _padd4;             // +0xd4  written by nothing
};

static_assert(sizeof(ContainerElement) == 0xd8,
              "UI::ContainerElement is 0xd8 bytes (MOV EDX,0xd8 in its slot-0 "
              "vector deleting destructor 0x18003a050)");
static_assert(alignof(ContainerElement) == 8, "UI::ContainerElement aligns to 8");
static_assert(offsetof(ContainerElement, _requestedWidth) == sizeof(Element), "");
static_assert(offsetof(ContainerElement, _requestedWidth)  == 0xc0, "");
static_assert(offsetof(ContainerElement, _requestedHeight) == 0xc4, "");
static_assert(offsetof(ContainerElement, _barSize)         == 0xc8, "");
static_assert(offsetof(ContainerElement, _clampWidth)      == 0xcc, "");
static_assert(offsetof(ContainerElement, _clampHeight)     == 0xcd, "");
static_assert(offsetof(ContainerElement, _wheelImpulse)    == 0xd0, "");

}  // namespace UI
