// code/Game/Component/welldescription.cpp
// WellDescription (entity description block) — well marker.
// ENDGAME B.3.8 pilot: a REAL class (Component/WellDescription.h) with a
// compiler-generated vtable. The retail evidence for every slot is in the
// header. Before: an 11-slot hand-built `g_WellDescription_Vtable` (slot 10 a
// "guard" past retail's end), registered at 0x14015af20 although nothing in the
// tree reads that address back (B38_RULES D5: a table with no by-address reader
// loses its registration with the table).

#include "Component/WellDescription.h"
#include "Component/Description.h"
#include "Component/WellComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// Description's own members (the abstract base) live in description.cpp.

// [0] 0x14000fdf0: ~WellDescription (0x14005d1c0) -> ~Description -> ~External,
// then MemFree on &1. Retail's chain; the old table's slot 0 did the same by hand.
Object::Base* WellDescription::DeletingDtor(unsigned flags) {
    this->~WellDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14005d170: one EMPTY Serializer(stream, 0) scope (no payload), then
// `MOV AL,1`. The old slot was declared `void` and returned whatever RAX held.
bool WellDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x14000fd90 / [7] 0x140001640 — PORT GAP (B38_RULES D3, D9 list). Retail:
// Reconstruct = DeletingDtor(0) then the ctor again on `this`; Destruct =
// DeletingDtor(0). The hand-built table had `return 0` stubs in both slots; the
// pilot keeps them so it changes no behaviour. Block K1 transcribes retail's
// bodies for all 78 descriptions in ONE titled behaviour-change commit.
Object::External* WellDescription::Reconstruct() { return nullptr; }
void              WellDescription::Destruct() {}

// [8] 0x14000fde0: WellComponent's type id through the hidden return slot.
Object::Type* WellDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = WellComponent::TypeId();
    return ret;
}

// [9] 0x14000fd40: placement-new on `this` (null-checked by MSVC's placement new).
// The old table put the static CreateInPlace in this slot, i.e. the same
// behaviour; retail folded the two identical bodies.
Object::External* WellDescription::Construct() {
    return new (static_cast<void*>(this)) WellDescription();
}

// create 0x14000fce0: MemAlloc(0x20, 8), then the (inlined) ctor.
void* WellDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(WellDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) WellDescription());
}
// createInPlace 0x14000fd40.
void* WellDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) WellDescription());
}

DEFINE_EXE_VTABLE(WellDescription, "_ZTV15WellDescription")
DEFINE_EXE_TYPE(WellDescription, "Description")

// B38_RULES D5 — the registration shape a converted class uses while an
// unconverted reader may still stamp its retail table by address: the exe
// address serves the COMPILER's vtable. Nothing in the tree reads 0x14015af20
// back today (SEAM_AND_FILEMAP §1.3), so this row is reader-less; it is kept as
// the pilot's exemplar of the seam and goes with the other reader-less rows in
// the closing pass.
#include "resolve.h"
extern "C" void RegisterWellDescriptionVtable() {
    RegisterExeAddr(0x14015af20, reinterpret_cast<uintptr_t>(WellDescription::Vtable()));
}
