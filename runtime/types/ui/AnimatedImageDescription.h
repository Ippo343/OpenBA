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
//   [0] 0x1800369e0  (unexported) vector deleting dtor — the class's own (B.3.9)
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
// (Until ENDGAME B.3.9:) ABSTRACT (slot 0 `= 0`); no real ctor — `??0AnimatedImageDescription` is
// factory_ui1.cpp's door, which `Construct` and `Reconstruct` tail-jump into.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0AnimatedImageDescription@UI@@QEAA@XZ` (0x1800cb6f0) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~AnimatedImageDescription is public and
// virtual. The factory pair is the class's own; the minted
// `??_7AnimatedImageDescription@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ImageDescription.h"
#include "../collection/Collection.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class Dialog;
class Element;

class AnimatedImageDescription : public ImageDescription {
public:
    // ??0AnimatedImageDescription@UI@@QEAA@XZ 0x1800cb6f0 — the REAL
    // constructor: ImageDescription's stores (inlined in retail), then the
    // frame-name array's two QWORD zeros (Collection::Array's own default ctor)
    // and `MOV dword [+0x90],0x1e`. +0x94 is NOT written.
    AnimatedImageDescription() : ImageDescription() {
        _framesPerSecond = 0x1e;
    }

    // ---- the ten slots, in measured order ---------------------------------
    // [0] 0x1800369e0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x98`, the element size).
    // `??1AnimatedImageDescription@UI@@UEAA@XZ` (0x1800cb6b0). VIRTUAL (B3X W1)
    // (animatedimagedescription.cpp): destroys the frame names through retail's
    // element-destroying Array dtor; the compiler chains ~ImageDescription.
    ~AnimatedImageDescription() override;
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x1800366d0, Type 0x1800366b0, TypeId 0x1800366c0, _TypeName 0x180036690
    //         _ParentTypeName 0x1800366a0, CreateNewObject 0x1800367c0, CreateInPlaceObject 0x180036840
    DECLARE_TYPE(AnimatedImageDescription)
public:
    int32_t     GetFileVersion() const override;                  // [3] 0x1800cb6a0
    // [4] Compile — Object::External's, inherited.
    bool        Serialize(IO::Stream& stream) override;                 // [5] 0x1800cb5c0  (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x180036850
    void        Destruct() override;                              // [7] 0x180036880
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800cb480  RETSLOT
    Object::External* Construct() override;                       // [9] 0x180036870

    // ---- exported, NOT PORTED as members ----------------------------------
    AnimatedImageDescription(const AnimatedImageDescription& src);            // 0x180036890
    AnimatedImageDescription& operator=(const AnimatedImageDescription& src); // 0x180036930

private:

public:
    Collection::Array<char*> _frameNames;   // +0x80 .. +0x8f  Array<System::String>
    int32_t  _framesPerSecond;              // +0x90  ctor 0x1e
    uint32_t _pad94;                        // +0x94  written by nothing
};

static_assert(sizeof(AnimatedImageDescription) == 0x98,
              "UI::AnimatedImageDescription is 0x98 bytes (MOV EDX,0x98 at 0x180036a12)");
static_assert(alignof(AnimatedImageDescription) == 8, "aligns to 8");
static_assert(std::has_virtual_destructor<AnimatedImageDescription>::value, "B3X W1: the normal-virtual cutover");
static_assert(!std::is_abstract<AnimatedImageDescription>::value, "B39 D1: concrete");
static_assert(offsetof(AnimatedImageDescription, _frameNames) == sizeof(ImageDescription),
              "AnimatedImageDescription's own fields begin at the end of ImageDescription");
static_assert(offsetof(AnimatedImageDescription, _frameNames)      == 0x80, "");
static_assert(offsetof(AnimatedImageDescription, _frameNames) +
              offsetof(Collection::Array<char*>, _size)            == 0x88, "");
static_assert(offsetof(AnimatedImageDescription, _framesPerSecond) == 0x90, "");

}  // namespace UI
