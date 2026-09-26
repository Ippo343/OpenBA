// code/Game/Component/description.cpp
// Description — the abstract per-component resource base (Object::External).
// ENDGAME B.3.8: a REAL class (Component/Description.h) with a compiler-generated
// vtable; the retail evidence for every slot is in the header. Block K1 moved
// these members here from welldescription.cpp, where the pilot had parked them.

#include "Component/Description.h"
#include "object/Internal.h"      // Object::detail::InternalMemFree
#include "Object/factory_registration.h"

// [0] 0x140031c70: stamp 0x1401629a0, `~External`, MemFree on &1.
Object::Base* Description::DeletingDtor(unsigned flags) {
    this->~Description();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] over Description's BaseTypeInfo; the abstract registration stays in
// factory_registrations_bulk.cpp (B38 D4). Every leaf overrides both.
const char* Description::TypeName() const { return "Description"; }          // 0x140031c60
uint32_t    Description::Type() const {                                      // 0x140031c50
    return ::Object::GetTypeIdByName("Description");
}
