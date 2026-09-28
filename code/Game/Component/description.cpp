// code/Game/Component/description.cpp
// Description — the abstract per-component resource base (Object::External).
// ENDGAME B.3.8: a REAL class (Component/Description.h) with a compiler-generated
// vtable; the retail evidence for every slot is in the header. Block K1 moved
// these members here from welldescription.cpp, where the pilot had parked them.

#include "Component/Description.h"
#include "object/Internal.h"      // Object::detail::InternalMemFree
#include "Object/factory_registration.h"
#include "Object/exe_type.h"   // DEFINE_EXE_ABSTRACT_TYPE

// [1] TypeName / [2] Type, the record and retail's abstract factory (create =
// the logging stub, in-place = null): ENDGAME B.3.9 RF.
DEFINE_EXE_ABSTRACT_TYPE(Description, "Object::External")
