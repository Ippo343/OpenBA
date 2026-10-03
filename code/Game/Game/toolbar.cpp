// code/Game/Game/toolbar.cpp
// Clean reimplementation of Toolbar game logic.
//
// Toolbar (0x68 bytes) -- toolbar UI container.
// Extends Object::External (0x20 base).
// Vtable at 0x140160120.
//
// Contains toolbar items, layout configuration, selection state,
// and references to building descriptions.
//
// Required so the in-game resource preload (UpdateLoop::LoadResources) can
// instantiate a Toolbar when External::Load reads Game\Toolbar.rsc out of the
// resource tree. Without the reflected type the loader hits a NullFactory ->
// IsDerivedFrom(NULL) -> crash.
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/Toolbar.h, field layout
// and retail slot map there) with a compiler-generated vtable. Gone: the
// hand-built 9-slot `g_Toolbar_Vtable` (three null slots: TypeName, Compile,
// Destruct), the lazy vtable resolves in the old Init/Slot8, the lazily-resolved
// typed-ExternalPtr door (a non-atomic first-call cache, B6-5) and the C entry
// points.
//
// Decompiled from Application-x64.exe.

#include "Game/Toolbar.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "system/String.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <new>

// FUN_140024c70 — Object::ExternalPtr<Toolbar>::Serialize (serialize_externals.cpp;
// registered at that address in reg_game_leaf.cpp). Called directly: the old
// lazily-resolved pointer resolved to it.
extern "C" void Reimpl_ExternalPtr_Serialize_Toolbar(void* refField, void* serializer);

// ============================================================
// Toolbar::Toolbar (0x1400d2130)
// ============================================================
// External() and the 0x140160120 stamp are the compiler's; then the defaults.
Toolbar::Toolbar() {
    _field20      = nullptr;  // +0x20: ExternalPtr
    _field28      = nullptr;  // +0x28: ExternalPtr
    _field30      = nullptr;  // +0x30: String (zeroed = empty)
    _field38      = nullptr;  // +0x38: String (zeroed = empty)
    _field40      = 8;        // +0x40: default 8
    _field44      = true;     // +0x44: default true (0x101 as uint16 = both bytes set)
    _field45      = true;     // +0x45: default true
    _field48      = 0;        // +0x48: zeroed
    selectedIndex = -1;       // +0x4C: -1 sentinel (no selection)
    _field58      = nullptr;  // +0x58: ExternalPtr
    _field60      = nullptr;  // +0x60: ExternalPtr
}

// ============================================================
// Toolbar::~Toolbar / DeletingDtor (vtable +0x00)
// ============================================================
// Original: 0x140024c10
// Destroys two String fields (+0x38, +0x30) in reverse order; External::
// ~External is the compiler's chain; MemFree if flags & 1.
Toolbar::~Toolbar() {
    // LEFT (H5): the two Strings are raw storage in Toolbar.h until a behaviour
    // commit declares them System::String (their ctor would then run).
    reinterpret_cast<System::String*>(&_field38)->~String();   // LEFT: H5 raw String storage
    reinterpret_cast<System::String*>(&_field30)->~String();   // LEFT: H5 raw String storage
}

// ============================================================
// Toolbar::Serialize (vtable +0x28, Slot5) -- Serialize fields
// ============================================================
// Original: 0x1400d2010
bool Toolbar::Serialize(IO::Stream& stream) {
    // The serializer on the stack, version 0; destroyed at the return (retail's order).
    IO::Serializer ser(stream, 0);
    IO::Serializer* serializer = &ser;

    // ExternalPtr fields
    Reimpl_ExternalPtr_Serialize_StringTable(&_field20, serializer);
    Reimpl_ExternalPtr_Serialize_StringTable(&_field28, serializer);

    // String fields
    reinterpret_cast<System::String*>(&_field30)->Serialize(serializer);   // LEFT: H5 raw String storage
    reinterpret_cast<System::String*>(&_field38)->Serialize(serializer);   // LEFT: H5 raw String storage

    // Encoding vcall 0x98 (the uint door) for +0x48
    serializer->Serialize(_field48, nullptr);

    // Typed ExternalPtr serialize for Toolbar at +0x60
    Reimpl_ExternalPtr_Serialize_Toolbar(&_field60, serializer);

    // ExternalPtr<BuildingDescription> at +0x58
    Reimpl_ExternalPtr_Serialize_StringTable(&_field58, serializer);

    // Encoding vcall 0x98 (the uint door) for +0x40
    serializer->Serialize(_field40, nullptr);

    // Bool fields
    serializer->Serialize(_field45, nullptr);
    serializer->Serialize(_field44, nullptr);

    // Int fields
    serializer->Serialize(selectedIndex, nullptr);
    serializer->Serialize(_field50, nullptr);

    return true;   // ~Serializer
}

// ============================================================
// Toolbar::Reconstruct (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x140024bf0. Destroys (slot 0, flags 0) then re-constructs in place.
Object::External* Toolbar::Reconstruct() {
    this->~Toolbar();
    return ::new (static_cast<void*>(this)) Toolbar();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void Toolbar::Destruct() { this->~Toolbar(); }

// ============================================================
// Toolbar::Construct (vtable +0x40)
// ============================================================
// Original: 0x140024be0 — placement-new of the class on `this` (the same
// defaults as the ctor; MSVC's placement new null-checks `this`).
Object::External* Toolbar::Construct() {
    return ::new (static_cast<void*>(this)) Toolbar();
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
// Original registration (Toolbar_RegisterType @0x14014d410):
//   BaseTypeInfo(&TypeInfo_Toolbar, "Toolbar", "Object::External",
//                null, Toolbar_Factory(0x140024ba0), Toolbar_Cleanup).
void* Toolbar::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Toolbar), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) Toolbar());
}
void* Toolbar::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) Toolbar());
}


DEFINE_EXE_TYPE(Toolbar, "Object::External")
