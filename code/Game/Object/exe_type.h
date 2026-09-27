#pragma once
// ============================================================================
// code/Game/Object/exe_type.h — ENDGAME B.3.8 (B38_RULES.md D4, D5),
//                               ENDGAME B.3.9 RF (B39_RULES.md D6)
//
// The reflection block and the vtable handle of an EXE-phase class that has
// become a real C++ class with a compiler-generated vtable.
//
// THE RECORD IS A REAL `Object::BaseTypeInfo`, as the Runtime's own
// DECLARE_TYPE / DEFINE_TYPE build it (runtime/types/object/TypeInfo.h). It is
// a class static constructed at CRT init through the ported BaseTypeInfo ctor
// (runtime/src/object/type_reflection.cpp), which head-inserts it into
// `BaseTypeInfo::_firstTypeInfo`. There is no staging list and no
// "InvokeAllRegistrations" step any more: the B.3.8 FactoryRegistration list
// existed only to keep registration ORDER unchanged, and the type table was
// measured ORDER-INDEPENDENT (b39_session/ORDER_MEASUREMENT.md: ids are
// ~CRC32(name), the tree is keyed by id, parents link in a second pass).
//
// Why static init is safe here (B39_RULES D6, block RF):
//   * `_firstTypeInfo` is `= nullptr`, i.e. constant-initialised: it is zero
//     before ANY dynamic initialiser of ANY translation unit runs, so each
//     record's head-insert is correct whatever the TU order.
//   * the ctor stores six fields and scans the parent string; it calls no
//     allocator, registry or resolver (none exists before main).
//   * the mint (standalone/rt_data_mint.cpp MintStorage) SPLICES its chain in
//     front of whatever CRT init linked; it never assigns the head.
//   * every record is emitted in every build: the ctor has side effects, so
//     its dynamic initialiser cannot be dropped, and every code/Game object
//     is on the link line (no archive). The boot PROVES it: each expansion
//     also leaves a constant-initialised pointer to its record in the linker
//     section `.data$exetype_b`, and Object::VerifyExeTypeInfos()
//     (factory_registration.cpp) aborts unless every one of them is in the
//     chain with its CRC id after Factory::Startup.
//
//   DECLARE_EXE_TYPE(Class)
//       slot 1 `TypeName()` and slot 2 `Type()` overrides (retail: `MOV RAX,
//       [ti+8]; RET` / `MOV EAX,[ti]; RET` over the class's BaseTypeInfo), the
//       static `TypeId()` / `GetTypeInfo()`, `CreateNewObject` /
//       `CreateInPlaceObject` and `Vtable()`.
//   DEFINE_EXE_TYPE(Class, "ParentLiteral")
//       the record. The NAME literal is `#Class` and the parent literal is
//       copied from B38_SLOTS.md §3 EXACTLY (quirks included): saves and .rsc
//       files carry ~CRC32(name), so a name is ABI and is never respelt.
//   DEFINE_EXE_ABSTRACT_TYPE(Class, "ParentLiteral")
//       the same for an abstract base, with retail's abstract factory.
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
// `X::TypeId()` returns `_typeInfo.typeId`: the ctor's 0xFFFFFFFF sentinel
// until `Factory::Startup` writes the CRC (B33_RULES §2.5 — not
// self-initialising), exactly as a Runtime DEFINE_TYPE record does.
//
// The Itanium vtable group GCC emits for a single-inheritance class is
// `{ offset-to-top, &typeinfo, slot0, slot1, ... }`; the vptr of an object
// points at slot0, two words in. tests/unit/exe_class_abi_test.cpp proves
// `Vtable() == *(void**)&obj` on this toolchain.
// ============================================================================

#include <cstdint>
#include "object/TypeInfo.h"
#include "Object/factory_registration.h"   // Object::AbstractTypeFactory

namespace Object { class Base; }

#define DECLARE_EXE_TYPE(Class)                                                   \
public:                                                                           \
    /* slot 1 — retail `MOV RAX,[ti+8]; RET`. Content-only (a name, never     */ \
    /* compared by pointer).                                                  */ \
    const char* TypeName() const override { return _typeInfo.name; }             \
    /* slot 2 — retail `MOV EAX,[ti]; RET`. 0xFFFFFFFF until Factory::Startup.*/ \
    uint32_t Type() const override { return _typeInfo.typeId; }                  \
    static uint32_t TypeId() { return _typeInfo.typeId; }                        \
    static const ::Object::BaseTypeInfo& GetTypeInfo() { return _typeInfo; }     \
    static void* CreateNewObject();                                               \
    static void* CreateInPlaceObject(void* storage);                              \
    static const void* Vtable();                                                  \
private:                                                                          \
    static ::Object::BaseTypeInfo _typeInfo;                                      \
    /* the record's slot in the linker section .data$exetype_b (the count     */ \
    /* proof, Object::VerifyExeTypeInfos); constant-initialised.             */ \
    static ::Object::BaseTypeInfo* _exeTypeInfoEntry;

// The section every record's slot goes to; the two markers that bracket it
// are in factory_registration.cpp (`.data$exetype_a` / `.data$exetype_c`).
// `used`: the slot has no reader but the section walk, so it must be kept.
#define OBJECT_EXE_TYPE_ENTRY __attribute__((used, section(".data$exetype_b")))

// The record (a CRT-init head-insert into _firstTypeInfo) and its section slot.
#define DEFINE_EXE_TYPE_EXT(Class, ParentLiteral, FileExt)                        \
    ::Object::BaseTypeInfo Class::_typeInfo(                                      \
        #Class, (ParentLiteral), (FileExt),                                       \
        reinterpret_cast< ::Object::BaseTypeInfo::FactoryFn>(&Class::CreateNewObject), \
        reinterpret_cast< ::Object::BaseTypeInfo::InPlaceFn>(&Class::CreateInPlaceObject)); \
    OBJECT_EXE_TYPE_ENTRY ::Object::BaseTypeInfo* Class::_exeTypeInfoEntry = &Class::_typeInfo;

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
