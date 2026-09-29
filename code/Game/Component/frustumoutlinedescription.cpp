// code/Game/Component/frustumoutlinedescription.cpp
// Clean reimplementation of FrustumOutlineDescription (UI element description block).
//
// FrustumOutlineDescription (0x80 bytes) — camera-frustum outline UI element descriptor.
// Extends ElementDescription (Object::External base, 0x20). Vtable at 0x14015c300.
// Parent type "ElementDescription".
//   +0x68: ExternalPtr (sprite sheet)
//   +0x70: String      (sprite name)
//   +0x78: int          (default -1)
//   +0x7c: int
//
// Factory  : MemAlloc 0x80 + ElementDescription ctor + vtable + defaults.
// Serialize (vtable +0x28, 0x14006c460):
//   ElementDescription::Serialize(self, stream)  [base]
//   then: ExternalPtr @ +0x68  +  String @ +0x70  +  int @ +0x78  +  int @ +0x7c
// GetVersion (vtable +0x18, 0x14006c500): ElementDescription::GetFileVersion(self) | 0x100.
// CreateInPlace (vtable +0x48, 0x140012330): ctor + zero +0x68/+0x70 + (+0x78 = -1).
// Dtor (vtable +0x00, 0x140012500): ~String(+0x70) + ElementDescription::~ + free.
//
// Field knowledge harvested from engine/reimpl/frustumoutlinedescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 VIS: a REAL class (FrustumOutlineDescription.h). Before: an
// 11-slot hand-built g_FrustumOutlineDescription_Vtable registered at
// 0x14015c300, the object built through the `??0ElementDescription@UI@@` DOOR
// and re-stamped. After: the compiler's 10-slot vtable; ElementDescription() is
// the base's real protected ctor (block RB). Nothing reads 0x14015c300 back by
// address, so its RegisterExeAddr row is gone (B38 D5).

#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Component/exe_bodies.h"
#include "system/String.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "ui/ElementDescription.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Sprite.h"               // ENDGAME B.3.6 UI callers (K4)
#include "object/External.h"         // ENDGAME B.3.6 UI callers (K4)
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/FrustumOutlineDescription.h"
#include "Component/FrustumOutlineElement.h"
#include "Game/exe_bodies.h"

// 0x14006c520: ElementDescription() is `??0ElementDescription@UI@@`; the
// 0x14015c300 stamp is the compiler's; +0x68 = 0, +0x70 = String() (null),
// +0x78 (qword) = -1.
FrustumOutlineDescription::FrustumOutlineDescription()
    : _spriteSheet(nullptr), _spriteName(), _width(-1), _height(-1) {}

// [3] 0x14006c500: ElementDescription::GetFileVersion() | 0x100.
int32_t FrustumOutlineDescription::GetFileVersion() const {
    return static_cast<int32_t>(
        static_cast<uint32_t>(UI::ElementDescription::GetFileVersion()) | 0x100u);
}

// [5] 0x14006c460: base, then ExternalPtr +0x68, String +0x70, int +0x78,
// int +0x7c; returns 1. (The hand-built slot was a `void` function, so its AL
// was whatever the Serializer dtor left there.)
bool FrustumOutlineDescription::Serialize(void* stream) {
    UI::ElementDescription::Serialize(stream);   // base — QUALIFIED (slot 5)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    Reimpl_ExternalPtr_Serialize_StringTable(&_spriteSheet, &ser);   // ExternalPtr +0x68
    _spriteName.Serialize(&ser);                                     // String      +0x70
    ser.Serialize(_width, nullptr);                                  // int +0x78
    ser.Serialize(_height, nullptr);                                 // int +0x7c
    return true;
}

// [6] 0x140012390: retail destroys and rebuilds in place: a virtual
// `~T()` (slot 0, flags 0), then the ctor again on `this` (B38_RULES §2.2, D9 #10).
Object::External* FrustumOutlineDescription::Reconstruct() {
    this->~FrustumOutlineDescription();
    return new (static_cast<void*>(this)) FrustumOutlineDescription();
}
// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0).
void FrustumOutlineDescription::Destruct() { this->~FrustumOutlineDescription(); }

// ============================================================
// CreateElement (vtable +0x40, exe 0x14006c340)
// ============================================================
// The element-tree build calls the ElementDescription's create-element slot
// (+0x40) to instantiate each element. For the minimap's `frustum` element this
// produces a FrustumOutlineElement; leaving it a ReturnNull stub made building
// `Dialog/Map.rsc:content` (Group{imageMap,imageOverlay,frustum}) yield a null
// child, so the whole content sub-tree was never inserted at `userGroup0`.
//
// Faithful port of FrustumOutlineDescription_CreateElement (0x14006c340):
//   1. Alloc + construct the FrustumOutlineElement.
//   2. Wrap in an InternalBasePtr, run ElementDescription::CreateInstance(desc,&ibp)
//      (applies the generic ElementDescription fields to the new element).
//   3. width  <- desc+0x78  ->  element+0xd0
//      height <- desc+0x7c  ->  element+0xd4
//   4. Set element+0xc0 sprite via Sprite::Sprite(spriteSheet@desc+0x68,
//      name@desc+0x70) — the frustum outline's line sprite (now wired below).
//   5. Return the element pointer through param_2 (RDX out-buffer).
Object::InternalPtr<UI::Element>* FrustumOutlineDescription::CreateInstance(
        Object::InternalPtr<UI::Element>* ret, const UI::Dialog* /*dialog*/) const {
    void* mem = Object::detail::InternalMemAlloc(sizeof(FrustumOutlineElement), 8);
    FrustumOutlineElement* elem = mem ? ::new (mem) FrustumOutlineElement() : nullptr;

    ::Object::InternalPtr<UI::Element> ibp(elem);
    // Non-virtual applier; QUALIFIED (the slot-8 override hides the base overload).
    UI::ElementDescription::CreateInstance(&ibp);

    if (elem) {
        // Build the outline's line Sprite from the description's spriteSheet + name
        // and store it in the element. The ctor installed a default (spriteless)
        // Sprite, and SpriteContext::AddLine no-ops when the sprite's spritesheet is
        // null — so without this the frustum outline never draws. Guard on a non-null
        // spriteSheet so a Map.rsc that omits the sprite stays crash-safe.
        if (_spriteSheet) {
            elem->_lineSprite = UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(_spriteSheet), _spriteName);
        }
        elem->_width  = _width;    // width
        elem->_height = _height;   // height
    }
    if (ret) *ret = ibp;
    return ret;
}

// [9] 0x140012330 — placement construction on `this` (the createInPlace body).
Object::External* FrustumOutlineDescription::Construct() {
    return new (static_cast<void*>(this)) FrustumOutlineDescription();
}

// create 0x1400122c0: MemAlloc(0x80, 8), then the ctor.
void* FrustumOutlineDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FrustumOutlineDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FrustumOutlineDescription());
}
// createInPlace 0x140012330: the ctor on `storage`.
void* FrustumOutlineDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FrustumOutlineDescription());
}

DEFINE_EXE_TYPE(FrustumOutlineDescription, "ElementDescription")
