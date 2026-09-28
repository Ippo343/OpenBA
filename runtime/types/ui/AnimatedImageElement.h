#pragma once
// ============================================================================
// runtime/types/ui/AnimatedImageElement.h — the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C2. Before this header the layout lived only as
// file-local constants in runtime/src/ui/animatedimageelement.cpp (RULE B6-8);
// they are MOVED here under the class.
//
// PARENT `UI::ImageElement` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_ParentTypeName@AnimatedImageElement@UI@@CAPEBDXZ  @0x180036a80 -> "ImageElement"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED:
//     ?_TypeName@AnimatedImageElement@UI@@CAPEBDXZ        @0x180036a70 -> "AnimatedImageElement"
//     ?_ParentTypeName@AnimatedImageElement@UI@@CAPEBDXZ  @0x180036a80 -> "ImageElement"
//
// `??_7AnimatedImageElement@UI@@6B@` @ 0x180156000, TWELVE slots; it
// overrides ONLY Update:
//   [0] 0x180036db0  (unexported) vector deleting dtor — the class's own (B.3.9)
//   [1] 0x180036ab0  TypeName
//   [2] 0x180036a90  Type
//   [4..6]           ImageElement's (0x180118650 / 0x1801186f0 / 0x180118850)
//   [7] 0x1800cb290  Update
//   [3], [8..11]     Element's
//
// ---------------------------------------------------------------------------
// LAYOUT — 0x100 bytes (`MOV EDX,0x100` at 0x180036de2, and the allocation in
// AnimatedImageDescription::CreateInstance at 0x1800cb4c1)
// ---------------------------------------------------------------------------
// `??0AnimatedImageElement@UI@@QEAA@XZ` (0x1800cb3f0, read as disassembly this
// pass): ??0Element, ImageElement's stamp and five stores INLINED, its own
// stamp (0x1800cb441), then `MOV qword [RBX+0xe0],0`, `MOV qword [RBX+0xe8],0`,
// `MOV dword [RBX+0xf0],0x1e`, `MOV qword [RBX+0xf8],0` — +0xf4 is written by
// no ctor.
//   +0xE0  _frames           Collection::Array<UI::Sprite>: data +0xe0, size
//                            +0xe8 (the frame count), capacity +0xec. Update
//                            copies `_frames._data[frame]` (one 16-byte MOVUPS
//                            at 0x1800cb33c) into ImageElement::_sprite.
//   +0xF0  _framesPerSecond  ctor 30. Update: period = (float)frameCount /
//                            (float)[+0xf0] (0x1800cb2da..0x1800cb2f8).
//   +0xF4  _padF4            written by nothing.
//   +0xF8  _accumTicks       int64, ctor 0. Update adds the System::Time's
//                            ticks to it (`ADD qword [RBX+0xf8],RAX` at
//                            0x1800cb2c2) and rewinds it on wrap.
//
// (Until ENDGAME B.3.9:) ABSTRACT (slot 0 `= 0`); no real ctor — `??0AnimatedImageElement` is
// factory_ui1.cpp's door.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0AnimatedImageElement@UI@@QEAA@XZ` (0x1800cb3f0) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~AnimatedImageElement is public and
// virtual. The factory pair is the class's own; the minted
// `??_7AnimatedImageElement@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ImageElement.h"
#include "Sprite.h"
#include "../collection/Collection.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace System { class Time; }

namespace UI {

class AnimatedImageElement : public ImageElement {
public:
    // ??0AnimatedImageElement@UI@@QEAA@XZ 0x1800cb3f0 — the REAL constructor:
    // ImageElement's stores (inlined in retail; ImageElement() makes the same
    // ones), then the frame array's two QWORD zeros (Collection::Array's own
    // default ctor), `MOV dword [+0xf0],0x1e`, `MOV qword [+0xf8],0`. +0xf4 is
    // NOT written.
    AnimatedImageElement() : ImageElement() {
        _framesPerSecond = 0x1e;
        _accumTicks      = 0;
    }

    // ---- the slots it owns ------------------------------------------------
    // [0] 0x180036db0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x100`, the element size).
    // `??1AnimatedImageElement@UI@@UEAA@XZ` (0x1800cb3a0). VIRTUAL (B3X W1)
    // (animatedimageelement.cpp): destroys the frame array through retail's
    // Array<Sprite> dtor; the compiler restamps and chains ~ImageElement.
    ~AnimatedImageElement() override;
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180036ab0, Type 0x180036a90, TypeId 0x180036aa0, _TypeName 0x180036a70
    //         _ParentTypeName 0x180036a80, CreateNewObject 0x180036ba0, CreateInPlaceObject 0x180036c20
    DECLARE_TYPE(AnimatedImageElement)
public:
    // [4..6] ImageElement's, inherited.
    // [7] `InternalPtr<DialogInstance>` arrives as a POINTER to the caller's
    // temporary (RULE B6-11; `MOV RAX,[R9]` at 0x1800cb29a).
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;               // [7] 0x1800cb290
    // [3], [8..11] Element's, inherited.

    // ---- exported, NOT PORTED as members ----------------------------------
    AnimatedImageElement(const AnimatedImageElement& src);        // 0x180036c30  not ported (door retired: no caller)
    AnimatedImageElement& operator=(const AnimatedImageElement& src); // 0x180036cb0  not ported

private:

public:
    Collection::Array<Sprite> _frames;   // +0xE0 .. +0xEF
    int32_t  _framesPerSecond;           // +0xF0  ctor 0x1e
    uint32_t _padF4;                     // +0xF4  written by nothing
    int64_t  _accumTicks;                // +0xF8  ctor 0
};

static_assert(sizeof(AnimatedImageElement) == 0x100,
              "UI::AnimatedImageElement is 0x100 bytes (MOV EDX,0x100 at 0x180036de2)");
static_assert(alignof(AnimatedImageElement) == 8, "aligns to 8");
static_assert(std::has_virtual_destructor<AnimatedImageElement>::value, "B3X W1: the normal-virtual cutover");
static_assert(!std::is_abstract<AnimatedImageElement>::value, "B39 D1: concrete");
static_assert(offsetof(AnimatedImageElement, _frames) == sizeof(ImageElement),
              "AnimatedImageElement's own fields begin at the end of ImageElement");
static_assert(offsetof(AnimatedImageElement, _frames)          == 0xE0, "");
static_assert(offsetof(AnimatedImageElement, _frames) +
              offsetof(Collection::Array<Sprite>, _size)       == 0xE8, "");
static_assert(offsetof(AnimatedImageElement, _framesPerSecond) == 0xF0, "");
static_assert(offsetof(AnimatedImageElement, _accumTicks)      == 0xF8, "");

}  // namespace UI
