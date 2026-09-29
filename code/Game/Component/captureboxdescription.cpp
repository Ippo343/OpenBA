// code/Game/Component/captureboxdescription.cpp
// Clean reimplementation of CaptureBoxDescription (UI element description block).
//
// CaptureBoxDescription (0x78 bytes) — sprite-capture-box UI element descriptor.
// Extends ElementDescription (Object::External base, 0x20). Vtable at 0x14015d4a0.
// Parent type "ElementDescription".
//   +0x68: ExternalPtr (sprite sheet)
//   +0x70: String      (sprite name)
//
// Factory  : MemAlloc 0x78 + ElementDescription ctor + vtable + zero +0x68/+0x70.
// Serialize (slot 5, 0x1400791a0):
//   ElementDescription::Serialize(self, stream)  [base]
//   then: ExternalPtr @ +0x68  +  String @ +0x70
// GetFileVersion (slot 3, 0x140079210): ElementDescription::GetFileVersion(self) | 0x100.
// CreateInstance (slot 8, 0x1400790a0) — builds a CaptureBoxElement.
// Construct (slot 9, 0x140013df0): the ctor on `this`.
// Dtor (slot 0, 0x140014010): ~String(+0x70) + ElementDescription::~ + free.
//
// ENDGAME B.3.8 VIS: a REAL class (CaptureBoxDescription.h). Before: an 11-slot
// hand-built g_CaptureBoxDescription_Vtable registered at 0x14015d4a0, the
// object built through the `??0ElementDescription@UI@@` DOOR and re-stamped.
// After: the compiler's 10-slot vtable; ElementDescription() is the base's real
// protected ctor (block RB). Nothing reads 0x14015d4a0 back by address, so its
// RegisterExeAddr row is gone (B38 D5).

#include "ui/UserInterface.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "system/String.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "object/Internal.h"  // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include "Component/CaptureBoxDescription.h"
#include "Component/CaptureBoxElement.h"
#include "ui/Sprite.h"
#include <cstdint>
#include <new>
#include "Game/exe_bodies.h"

// 0x140079230: ElementDescription() is `??0ElementDescription@UI@@`; the
// 0x14015d4a0 stamp is the compiler's; +0x68 = 0, +0x70 = String() (null).
CaptureBoxDescription::CaptureBoxDescription() : _spriteSheet(nullptr), _spriteName() {}

// [3] 0x140079210: ElementDescription::GetFileVersion() | 0x100 — the 0x100 bit
// flags a custom serialize format on top of the base ElementDescription version.
int32_t CaptureBoxDescription::GetFileVersion() const {
    return static_cast<int32_t>(
        static_cast<uint32_t>(UI::ElementDescription::GetFileVersion()) | 0x100u);
}

// [5] 0x1400791a0: base, then ExternalPtr +0x68, String +0x70; returns 1. (The
// hand-built slot was a `void` function, so its AL was whatever the Serializer
// dtor left there.)
bool CaptureBoxDescription::Serialize(void* stream) {
    UI::ElementDescription::Serialize(stream);   // base — QUALIFIED (slot 5)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge (stream is void* in the slot)
    Reimpl_ExternalPtr_Serialize_StringTable(&_spriteSheet, &ser);   // ExternalPtr +0x68
    _spriteName.Serialize(&ser);                                      // String      +0x70
    return true;
}

// [6] 0x140013e40: retail destroys and rebuilds in place: a virtual
// `~T()` (slot 0, flags 0), then the ctor again on `this` (B38_RULES §2.2, D9 #10).
Object::External* CaptureBoxDescription::Reconstruct() {
    this->~CaptureBoxDescription();
    return new (static_cast<void*>(this)) CaptureBoxDescription();
}
// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0).
void CaptureBoxDescription::Destruct() { this->~CaptureBoxDescription(); }

// [8] 0x1400790a0 — CreateInstance (RETSLOT: RCX this, RDX the caller's
// InternalPtr<Element>). Transcribed:
//   1. MemAlloc(0xe0, 8) + the CaptureBoxElement ctor (inlined in retail);
//   2. wrap it in an InternalBasePtr and run the non-virtual
//      ElementDescription::CreateInstance (0x1401518d0: the generic fields);
//   3. element+0xc0 = Sprite(ExternalPtr +0x68, String +0x70) (0x1401514c8);
//   4. *ret = the element; return ret.
// (The frustum twin, 0x14006c340, has the same shape plus the size copy.)
Object::InternalPtr<UI::Element>* CaptureBoxDescription::CreateInstance(
        Object::InternalPtr<UI::Element>* ret, const UI::Dialog* /*dialog*/) const {
    // MemAlloc(0xe0, 8) and the ctor, inlined in retail as create 0x140013eb0 does.
    void* p = Object::detail::InternalMemAlloc(sizeof(CaptureBoxElement), 8);
    CaptureBoxElement* elem = p ? new (p) CaptureBoxElement() : nullptr;

    Object::InternalPtr<UI::Element> ibp(elem);
    // Non-virtual applier; QUALIFIED (the slot-8 override hides the base overload).
    UI::ElementDescription::CreateInstance(&ibp);
    CaptureBoxElement* e = static_cast<CaptureBoxElement*>(ibp.Get());

    e->_lineSprite = UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(_spriteSheet), _spriteName);
    *ret = Object::InternalPtr<UI::Element>(e);
    return ret;
}

// [9] 0x140013df0 — placement construction on `this` (the createInPlace body).
Object::External* CaptureBoxDescription::Construct() {
    return new (static_cast<void*>(this)) CaptureBoxDescription();
}

// create 0x140013d90: MemAlloc(0x78, 8), then the ctor.
void* CaptureBoxDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CaptureBoxDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CaptureBoxDescription());
}
// createInPlace 0x140013df0: the ctor on `storage`.
void* CaptureBoxDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CaptureBoxDescription());
}

DEFINE_EXE_TYPE(CaptureBoxDescription, "ElementDescription")
