// code/Game/Component/foresterdescription.cpp
// ForesterDescription (entity description block) — forester lodge parameters.
// Extends Object::External (0x20 base). Vtable 0x140163510. Parent "Description".
// Size 0x30. GetVersion=2 (slot +0x18 @ 0x14011fb00: mov eax,2; ret).
//
// Ctor defaults (ForesterDescription_Factory):
//   ExternalPtr @ +0x20 = 0   (tree/sapling material ref)
//   int/enum    @ +0x28 = 0
// Serialize (vtable +0x28, 0x14011fa80) — Serializer ctor with version=1, stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 -> fn_ExternalPtr_BuildSerialize)
//   if (serializer.GetVersion() > 0): enum @ +0x28 (encoding vcall +0x98)
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (ForesterDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163510) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ForesterDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x1400348e0 / createInPlace 0x140034950):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
ForesterDescription::ForesterDescription()
    : Description(),
      _ref20(nullptr),
      _type(0) {}

// [0] 0x140034a10: ~ForesterDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* ForesterDescription::DeletingDtor(unsigned flags) {
    this->~ForesterDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14011fa80: stream order as retail.
bool ForesterDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_BuildSerialize) return true;   // standalone: the door is unwired
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.8 bridge (External::Serialize takes void*)
    fn_ExternalPtr_BuildSerialize(&_ref20, &ser);
    if (ser.GetVersion() > 0) ser.Serialize(_type, nullptr);   // version-gated, as retail
    return true;
}

// [6] 0x1400349b0 — PORT GAP (retail: DeletingDtor(0), then the ctor again on
// `this`, B38_RULES §2.2). The hand-built table had a `return 0` stub; kept so
// this commit changes no behaviour. D9 #10 transcribes it for every description.
Object::External* ForesterDescription::Reconstruct() { return nullptr; }

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void ForesterDescription::Destruct() {}

// [8] 0x140034a00: ForesterComponent's type id through the hidden return slot.
Object::Type* ForesterDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("ForesterComponent");
    return ret;
}

// [9] 0x140034950: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* ForesterDescription::Construct() {
    return new (static_cast<void*>(this)) ForesterDescription();
}

// create 0x1400348e0: MemAlloc(0x30, 8), then the (inlined) ctor.
void* ForesterDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ForesterDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ForesterDescription());
}
// createInPlace 0x140034950.
void* ForesterDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ForesterDescription());
}

DEFINE_EXE_VTABLE(ForesterDescription, "_ZTV19ForesterDescription")
DEFINE_EXE_TYPE(ForesterDescription, "Description")
