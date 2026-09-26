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
#include "engine_funcs.h"
#include "resolve.h"
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
    reinterpret_cast<System::String*>(&_field38)->~String();
    reinterpret_cast<System::String*>(&_field30)->~String();
}

Object::Base* Toolbar::DeletingDtor(unsigned flags) {
    this->~Toolbar();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// Toolbar::Serialize (vtable +0x28, Slot5) -- Serialize fields
// ============================================================
// Original: 0x1400d2010
bool Toolbar::Serialize(void* stream) {
    // Create serializer on stack (24 bytes), version 0
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // ExternalPtr fields
    if (fn_ExternalPtr_Serialize2) {
        fn_ExternalPtr_Serialize2(&_field20, serializer);
    }
    if (fn_ExternalPtr_Serialize) {
        fn_ExternalPtr_Serialize(&_field28, serializer);
    }

    // String fields
    reinterpret_cast<System::String*>(&_field30)->Serialize(serializer);
    reinterpret_cast<System::String*>(&_field38)->Serialize(serializer);

    // Encoding vcall 0x98 for int at +0x48
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(
        *reinterpret_cast<unsigned int*>(&_field48), nullptr);   // B.3.4 bridge  (0x98 = uint door)

    // Typed ExternalPtr serialize for Toolbar at +0x60
    Reimpl_ExternalPtr_Serialize_Toolbar(&_field60, serializer);

    // ExternalPtr<BuildingDescription> at +0x58
    if (fn_ExternalPtr_BuildSerialize) {
        fn_ExternalPtr_BuildSerialize(&_field58, serializer);
    }

    // Encoding vcall 0x98 for int at +0x40
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(
        *reinterpret_cast<unsigned int*>(&_field40), nullptr);   // B.3.4 bridge  (0x98 = uint door)

    // Bool fields
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(_field45, nullptr);   // B.3.4 bridge (cast dropped: _field45 is bool)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(_field44, nullptr);   // B.3.4 bridge (cast dropped: _field44 is bool)

    // Int fields
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(selectedIndex, nullptr);   // B.3.4 bridge (cast dropped: selectedIndex is int32_t)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(_field50, nullptr);   // B.3.4 bridge (cast dropped: _field50 is int32_t)

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// Toolbar::Reconstruct (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x140024bf0. Destroys (slot 0, flags 0) then re-constructs in place.
Object::External* Toolbar::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) Toolbar();
}

// [7] 0x140001640 — PORT GAP (B38 D3.2): retail runs DeletingDtor(0); the old
// table's slot was null. Kept until the family's titled commit.
void Toolbar::Destruct() {}

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

DEFINE_EXE_VTABLE(Toolbar, "_ZTV7Toolbar")

// B38_RULES D5: nothing in the tree reads 0x140160120 back (swept by hex and
// owner); this row is reader-less and serves the compiler table only so the
// coordinator-owned call keeps linking. The closing pass deletes it with the call.
extern "C" void RegisterToolbarVtable() {
    RegisterExeAddr(0x140160120, reinterpret_cast<uintptr_t>(Toolbar::Vtable()));
}

DEFINE_EXE_TYPE(Toolbar, "Object::External")
