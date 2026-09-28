// code/Game/Component/createpathdescription.cpp
// Clean reimplementation of CreatePathDescription (entity description block).
//
// CreatePathDescription (0x20 bytes) — path-drawing placement tool. No extra fields.
// Extends Object::External (0x20 base). Vtable at 0x140159210. Parent "Description".
//
// Factory  : MemAlloc 0x20 + External ctor + vtable.
// Serialize (vtable +0x28, 0x140047a70): Serializer ctor/dtor only — returns 1.
// GetVersion (vtable +0x18, 0x140047ab0): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x140004d30): External::~External + free.
//
// Field knowledge harvested from engine/reimpl/createpathdescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (CreatePathDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140159210) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/CreatePathDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [5] 0x140047a70: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool CreatePathDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x140004ce0: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* CreatePathDescription::Reconstruct() {
    this->~CreatePathDescription();
    return new (static_cast<void*>(this)) CreatePathDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void CreatePathDescription::Destruct() { this->~CreatePathDescription(); }

// [8] 0x140004950 (shared by 19 tables): `MOV dword [RDX],0; MOV RAX,RDX; RET` —
// a data-only description builds no component.
Object::Type* CreatePathDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = 0;
    return ret;
}

// [9] 0x140004c90: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* CreatePathDescription::Construct() {
    return new (static_cast<void*>(this)) CreatePathDescription();
}

// create 0x140004c30: MemAlloc(0x20, 8), then the (inlined) ctor.
void* CreatePathDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CreatePathDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CreatePathDescription());
}
// createInPlace 0x140004c90.
void* CreatePathDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CreatePathDescription());
}

DEFINE_EXE_TYPE(CreatePathDescription, "Description")
