// code/Game/Game/profession.cpp
// Clean reimplementation of Profession game logic.
//
// Profession (0x60 bytes) -- citizen profession definition.
// Extends Object::External (0x20 base).
// Vtable at 0x14015efa0.
//
// Defines a profession with name, description, priority, and skill multipliers.
// Fields include ExternalPtrs at +0x20/+0x28, System::Strings at +0x30/+0x38/+0x40,
// and numeric fields (int, bools, floats) from +0x48 to +0x58.
//
// Profession is an Object::External type referenced by CitizenDescription (the
// citizen-template closure FindOrLoads Profession\*.rsc).
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/Profession.h, field
// layout and retail slot map there) with a compiler-generated vtable. Gone: the
// hand-built 9-slot `g_Prof_Vtable`, its stubs, the lazy vtable resolve in the
// old Init, the lazily-resolved ExternalPtr doors (a non-atomic first-call cache
// on the loader thread, B6-5) and the C entry points.
//
// Decompiled from Application-x64.exe.

#include "Game/Profession.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <cstdio>
#include <new>

// FUN_14000bf40 / FUN_14000bd30 -- the two ExternalPtr serialize variants.
// Both addresses are registered to this one body (address_registry.cpp); it is
// called directly (the old lazily-resolved pointers resolved to it).
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

// ============================================================
// Profession::Profession (0x1400a9030)
// ============================================================
// External() and the 0x14015efa0 stamp are the compiler's; then the defaults.
Profession::Profession() {
    // ExternalPtr fields
    externalPtr1 = nullptr;
    externalPtr2 = nullptr;

    // String fields (zero = empty COW string)
    stringName = 0;
    stringDesc = 0;
    stringTool = 0;

    // Numeric fields
    field48 = 0;
    field4C = 0;
    field4D = 0;
    isAvailable = 1;
    priority = 100;
    skillMult1 = 1.0f;
    skillMult2 = 1.0f;
}

// ============================================================
// Profession::~Profession / DeletingDtor (vtable +0x00)
// ============================================================
// Original: 0x14001e6f0
// Destroys 3 strings at +0x40, +0x38, +0x30 (in reverse order); External::
// ~External is the compiler's chain; MemFree if flags & 1.
Profession::~Profession() {
    // B.3.1 bridge: Profession's String fields are still POD members.
    reinterpret_cast<System::String*>(&stringTool)->~String();  // +0x40
    reinterpret_cast<System::String*>(&stringDesc)->~String();  // +0x38
    reinterpret_cast<System::String*>(&stringName)->~String();  // +0x30
}

Object::Base* Profession::DeletingDtor(unsigned flags) {
    this->~Profession();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// Profession::Reconstruct (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x14001e580. Destroys (slot 0, flags 0) then re-constructs in place.
Object::External* Profession::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) Profession();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void Profession::Destruct() { DeletingDtor(0); }

// ============================================================
// Profession::Construct (vtable +0x40)
// ============================================================
// Original: 0x14001e570 — placement-new of the class on `this`.
Object::External* Profession::Construct() {
    return ::new (static_cast<void*>(this)) Profession();
}

// ============================================================
// Profession::Serialize (vtable +0x28, Slot5) -- Serialize all fields
// ============================================================
// Original: 0x1400a8f10
// Creates a Serializer(stream, version=0), then serializes each field.
bool Profession::Serialize(void* stream) {
    // Stack-allocated serializer (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize ExternalPtr fields
    Reimpl_ExternalPtr_Serialize_StringTable(&externalPtr1, serializer);  // +0x20 (0x14000bf40)
    Reimpl_ExternalPtr_Serialize_StringTable(&externalPtr2, serializer);  // +0x28 (0x14000bd30)

    // Serialize String fields
    reinterpret_cast<System::String*>(&stringName)->Serialize(serializer);  // +0x30
    reinterpret_cast<System::String*>(&stringDesc)->Serialize(serializer);  // +0x38
    reinterpret_cast<System::String*>(&stringTool)->Serialize(serializer);  // +0x40

    // Serialize numeric fields
    // field48 declared int32_t == int door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(field48, nullptr);     // +0x48   // B.3.4 bridge
    // field4C/field4D declared uint8_t, not exactly bool: cast kept
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&field4C), nullptr);    // +0x4C   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&field4D), nullptr);    // +0x4D   // B.3.4 bridge
    // skillMult1/2 declared float == float door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(skillMult1, nullptr); // +0x54   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(skillMult2, nullptr); // +0x58   // B.3.4 bridge
    // priority declared int32_t == int door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(priority, nullptr);     // +0x50   // B.3.4 bridge
    // isAvailable declared uint8_t, not exactly bool: cast kept
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&isAvailable), nullptr); // +0x4E   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
void* Profession::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Profession), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) Profession());
}
void* Profession::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) Profession());
}

DEFINE_EXE_VTABLE(Profession, "_ZTV10Profession")

DEFINE_EXE_TYPE(Profession, "Object::External")
