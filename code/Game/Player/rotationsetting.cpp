// code/Game/Player/rotationsetting.cpp — ENDGAME B.3.8 TOOL
// The members of `ToolSetting` and `RotationSetting` (ToolSetting.h,
// RotationSetting.h). Retail addresses on every body.
//
// History kept from the hand-built table this replaces: the old slot 0 once
// shared a generic Tool dtor that DeletePtr'd an "owner IBP" at +0x10. For a
// RotationSetting +0x10 is the rotation VALUE (0..3), so once the player had
// rotated a placement that dtor dereferenced 0x1/0x2 during an in-game Load.
// Retail's dtor (0x1400dea40) never touches +0x10, and neither does the
// compiler chain below.

#include "Player/RotationSetting.h"
#include "Object/factory_registration.h"
#include <new>

// ============================================================================
// ToolSetting
// ============================================================================

// [0] 0x14002e960: stamp 0x140161ee8, `~Internal`, MemFree on &1.
Object::Base* ToolSetting::DeletingDtor(unsigned flags) {
    this->~ToolSetting();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] 0x14002e930 / 0x14002e920. The abstract registration stays in
// factory_registrations_bulk.cpp (B38 D4), so the base answers by name.
const char* ToolSetting::TypeName() const { return "ToolSetting"; }
uint32_t    ToolSetting::Type() const { return ::Object::GetTypeIdByName("ToolSetting"); }

// ============================================================================
// RotationSetting
// ============================================================================

// [0] 0x140026dd0: the dtor 0x1400dea40 (restamps only), MemFree on &1.
Object::Base* RotationSetting::DeletingDtor(unsigned flags) {
    this->~RotationSetting();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140026d20: MemAlloc(0x18, 8), then the (inlined) ctor.
void* RotationSetting::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RotationSetting), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RotationSetting());
}
// createInPlace 0x140026d80: the ctor on `storage`.
void* RotationSetting::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RotationSetting());
}

DEFINE_EXE_VTABLE(RotationSetting, "_ZTV15RotationSetting")
DEFINE_EXE_TYPE(RotationSetting, "ToolSetting")
