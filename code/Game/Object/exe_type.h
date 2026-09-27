#pragma once
// ============================================================================
// code/Game/Object/exe_type.h — ENDGAME B.3.8 (B38_RULES.md D4, D5)
//
// The reflection block and the vtable handle of an EXE-phase class that has
// become a real C++ class with a compiler-generated vtable.
//
// WHY NOT runtime/types/object/TypeInfo.h's DECLARE_TYPE / DEFINE_TYPE.
//   Those build a static `Object::BaseTypeInfo` at CRT-init time. The exe
//   classes are registered through `Object::FactoryRegistration`
//   (code/Game/Object/factory_registration.h, the REGISTER_TYPE list) which
//   builds each record at `InvokeAllRegistrations()` through the Runtime
//   BaseTypeInfo constructor, in the order the registration objects were
//   linked. Moving an exe class to DEFINE_TYPE would change WHEN and IN WHAT
//   ORDER its record enters `BaseTypeInfo::_firstTypeInfo`, which the resave
//   gate's type table observes. B38 D4: step 8 keeps the FactoryRegistration
//   list and makes the class OWN its registration object instead of an
//   anonymous REGISTER_TYPE global. Same literal (saves hash the NAME), same
//   list, same position in the TU, so the registration order is unchanged.
//
//   DECLARE_EXE_TYPE(Class)
//       slot 1 `TypeName()` and slot 2 `Type()` overrides (retail: `MOV RAX,
//       [ti+8]; RET` / `MOV EAX,[ti]; RET` over the class's BaseTypeInfo), the
//       static `TypeId()` (replaces the per-class lazy
//       `static uint32_t cached; if (!cached) cached = GetTypeIdByName("X")`
//       caches — B6-5-safe: a plain read of a record written once at boot),
//       `CreateNewObject` / `CreateInPlaceObject` and `Vtable()`.
//   DEFINE_EXE_TYPE(Class, "ParentLiteral")
//       the registration object itself. Put it WHERE the file's REGISTER_TYPE
//       line was, so the link-order position is unchanged. The parent literal
//       is copied from B38_SLOTS.md §3 EXACTLY (quirks included).
//   DEFINE_EXE_VTABLE(Class, "_ZTV<len><Class>")
//       `Class::Vtable()`: the address of the COMPILER's vtable (its address
//       point, i.e. the value a constructed object's vptr holds), without
//       constructing an object. B38 D5: a converted class whose retail vtable
//       address is still read back by another file keeps
//       `RegisterExeAddr(kVtable, (uintptr_t)Class::Vtable())`, so every
//       unconverted by-address reader stamps the compiler table. The symbol is
//       spelled by hand and the LINKER checks it: a misspelling is an
//       undefined reference, not a silent wrong pointer.
//
// The Itanium vtable group GCC emits for a single-inheritance class is
// `{ offset-to-top, &typeinfo, slot0, slot1, ... }`; the vptr of an object
// points at slot0, two words in. tests/unit/exe_class_abi_test.cpp proves
// `Vtable() == *(void**)&obj` on this toolchain.
// ============================================================================

#include <cstdint>
#include "Object/factory_registration.h"

namespace Object { class Base; }

#define DECLARE_EXE_TYPE(Class)                                                   \
public:                                                                           \
    /* slot 1 — retail `MOV RAX,[ti+8]; RET`. Content-only (a name, never     */ \
    /* compared by pointer).                                                  */ \
    const char* TypeName() const override { return _registration.typeName; }     \
    /* slot 2 — retail `MOV EAX,[ti]; RET`. 0 before InvokeAllRegistrations,  */ \
    /* 0xFFFFFFFF until Factory::Startup: exactly what GetTypeIdByName and     */ \
    /* the retail record answer at those times.                              */ \
    uint32_t Type() const override { return TypeId(); }                          \
    static uint32_t TypeId() {                                                    \
        return *reinterpret_cast<const uint32_t*>(&_registration.storage);        \
    }                                                                             \
    static void* CreateNewObject();                                               \
    static void* CreateInPlaceObject(void* storage);                              \
    static const void* Vtable();                                                  \
private:                                                                          \
    static ::Object::FactoryRegistration _registration;

#define DEFINE_EXE_TYPE_EXT(Class, ParentLiteral, FileExt)                        \
    ::Object::FactoryRegistration Class::_registration(                           \
        #Class, (ParentLiteral), (FileExt),                                       \
        &Class::CreateNewObject, &Class::CreateInPlaceObject);

#define DEFINE_EXE_TYPE(Class, ParentLiteral)                                     \
    DEFINE_EXE_TYPE_EXT(Class, ParentLiteral, nullptr)

// An ABSTRACT exe base (Action, Work, Tool, Component, ...). Retail registers
// these with a `_purecall`-shaped factory (0x140003730): the record exists so
// the base is a parent in the tree and answers slots 1/2, but nothing ever
// builds one. Create = the logging stub, in-place = a function answering null
// -- exactly the pair the old REGISTER_ABSTRACT_FACTORY rows passed
// (ENDGAME B.3.9 RF, B39_RULES D6).
#define DEFINE_EXE_ABSTRACT_TYPE(Class, ParentLiteral)                            \
    void* Class::CreateNewObject() { return ::Object::AbstractTypeFactory(#Class); } \
    void* Class::CreateInPlaceObject(void*) { return nullptr; }                   \
    DEFINE_EXE_TYPE(Class, ParentLiteral)

// `TVSymbol` is the Itanium name of the class's vtable group, e.g.
// "_ZTV13WellComponent" (13 = strlen("WellComponent")). x86_64-w64-mingw32 has
// no user-label prefix, so the asm label is the symbol verbatim.
#define DEFINE_EXE_VTABLE(Class, TVSymbol)                                        \
    extern const void* const Class##_VtableGroup[] __asm__(TVSymbol);             \
    const void* Class::Vtable() { return &Class##_VtableGroup[2]; }
