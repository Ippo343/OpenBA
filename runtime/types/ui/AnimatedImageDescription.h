#pragma once
// ============================================================================
// runtime/types/ui/AnimatedImageDescription.h — the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C2. Before this header the layout lived only as
// raw `+0x..` reads in runtime/src/ui/animatedimagedescription.cpp (RULE
// B6-8); they are MOVED here under the class.
//
// PARENT `UI::ImageDescription` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_ParentTypeName@AnimatedImageDescription@UI@@CAPEBDXZ @0x1800366a0 -> "ImageDescription"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED:
//     ?_TypeName@AnimatedImageDescription@UI@@CAPEBDXZ       @0x180036690 -> "AnimatedImageDescription"
//     ?_ParentTypeName@AnimatedImageDescription@UI@@CAPEBDXZ @0x1800366a0 -> "ImageDescription"
// The derivation is also visible in the bytes: the ctor stamps
// ??_7ImageDescription@UI@@6B@ before its own table (0x1800cb763 / 0x1800cb77d),
// the dtor tail-jumps to ~ImageDescription, Serialize calls
// ImageDescription::Serialize first.
//
// `??_7AnimatedImageDescription@UI@@6B@` @ 0x180156060, TEN slots:
//   [0] 0x1800369e0  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x1800366d0  TypeName
//   [2] 0x1800366b0  Type
//   [3] 0x1800cb6a0  GetFileVersion  (`return 0x10204`)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared (B6-1.5)
//   [5] 0x1800cb5c0  Serialize
//   [6] 0x180036850  Reconstruct
//   [7] 0x180036880  Destruct
//   [8] 0x1800cb480  CreateInstance(const Dialog*) -> InternalPtr<Element>  RETSLOT (B6-2)
//   [9] 0x180036870  Construct       (override of ImageDescription's slot 9)
//
// ---------------------------------------------------------------------------
// LAYOUT — 0x98 bytes (`MOV EDX,0x98` at 0x180036a12)
// ---------------------------------------------------------------------------
// `??0AnimatedImageDescription@UI@@QEAA@XZ` (0x1800cb6f0, read as disassembly
// this pass) ends: stamp ImageDescription, +0x68/+0x70 qword 0, +0x78 qword -1,
// stamp AnimatedImageDescription (0x1800cb784), `MOV qword [RCX+0x80],RDX`,
// `MOV qword [RCX+0x88],RDX`, `MOV dword [RCX+0x90],0x1e` (0x1800cb795). So
// +0x94 is written by no ctor.
//   +0x80  _frameNames       Array<System::String> — the per-frame sprite
//                            names; Serialize sizes it (0x180029a80) and walks
//                            it (0x180029bb0); CreateInstance looks each name
//                            up; ~AnimatedImageDescription destroys it with the
//                            element-destroying Array dtor (0x180010ab0). Each
//                            element is the String's one pointer, so `char*`.
//   +0x90  _framesPerSecond  ctor 30; one kScalarInt; CreateInstance copies it
//                            to `AnimatedImageElement::_framesPerSecond`
//                            (+0xf0), which Update divides the frame count by —
//                            that member's name.
//   +0x94  _pad94            written by nothing.
//
// ABSTRACT (slot 0 `= 0`); no real ctor — `??0AnimatedImageDescription` is
// factory_ui1.cpp's door, which `Construct` and `Reconstruct` tail-jump into.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ImageDescription.h"
#include "../collection/Collection.h"

namespace UI {

class Dialog;
class Element;

class AnimatedImageDescription : public ImageDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x1800369e0
    const char* TypeName() const override;                        // [1] 0x1800366d0
    uint32_t    Type() const override;                            // [2] 0x1800366b0
    int32_t     GetFileVersion() const override;                  // [3] 0x1800cb6a0
    // [4] Compile — Object::External's, inherited.
    bool        Serialize(void* stream) override;                 // [5] 0x1800cb5c0  (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x180036850
    void        Destruct() override;                              // [7] 0x180036880
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800cb480  RETSLOT
    Object::External* Construct() override;                       // [9] 0x180036870

    // ---- exported, NOT PORTED as members ----------------------------------
    AnimatedImageDescription(const AnimatedImageDescription& src);            // 0x180036890
    AnimatedImageDescription& operator=(const AnimatedImageDescription& src); // 0x180036930
    static uint32_t TypeId();                                     // 0x1800366c0  not ported
    static Object::Base* CreateNewObject();                       // 0x1800367c0  factory door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180036840  factory door

protected:
    // NOT retail's `??0AnimatedImageDescription@UI@@QEAA@XZ` (0x1800cb6f0, a door).
    AnimatedImageDescription() = default;
    // `??1AnimatedImageDescription@UI@@UEAA@XZ` (0x1800cb6b0). NON-VIRTUAL, NOT
    // defined in the real build; the retail body is the door
    // `RtUi_AnimatedImageDescription_Dtor`.
    ~AnimatedImageDescription();

private:
    static const char* _TypeName();                               // 0x180036690
    static const char* _ParentTypeName();                         // 0x1800366a0 -> "ImageDescription"
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    Collection::Array<char*> _frameNames;   // +0x80 .. +0x8f  Array<System::String>
    int32_t  _framesPerSecond;              // +0x90  ctor 0x1e
    uint32_t _pad94;                        // +0x94  written by nothing
};

static_assert(sizeof(AnimatedImageDescription) == 0x98,
              "UI::AnimatedImageDescription is 0x98 bytes (MOV EDX,0x98 at 0x180036a12)");
static_assert(alignof(AnimatedImageDescription) == 8, "aligns to 8");
static_assert(!std::has_virtual_destructor<AnimatedImageDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(AnimatedImageDescription, _frameNames) == sizeof(ImageDescription),
              "AnimatedImageDescription's own fields begin at the end of ImageDescription");
static_assert(offsetof(AnimatedImageDescription, _frameNames)      == 0x80, "");
static_assert(offsetof(AnimatedImageDescription, _frameNames) +
              offsetof(Collection::Array<char*>, _size)            == 0x88, "");
static_assert(offsetof(AnimatedImageDescription, _framesPerSecond) == 0x90, "");

}  // namespace UI
