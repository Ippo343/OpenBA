#pragma once
// Factory registration framework — long-term replacement for the original exe's
// 308 *_RegisterType static initializers.
//
// Each `code/Game/<Module>/<Class>.cpp` declares its registration via
// REGISTER_TYPE(...). At CRT-init time the registration links itself into a
// pending list (no Runtime-DLL calls, since the DLL isn't resolved yet).
// After EngineFuncsInit, standalone main calls Object::InvokeAllRegistrations(),
// which walks the list and calls the Runtime DLL's Object::BaseTypeInfo
// constructor for each entry. That builds Runtime DLL's _firstTypeInfo linked
// list, which Engine::Engine::Startup -> Object::Factory::Startup later walks
// to populate the type-id -> factory tree.
//
// Lookup mechanism (Object::External::Load and friends): typeId is read from
// .rsc payload, then the tree at Object::Factory::_singleton is searched with
// typeId = ~CRC32(typeName_widened_to_uint16). Matching tree node holds a
// pointer to our BaseTypeInfo storage; its +0x20 slot is the factoryFn we
// passed at registration.

#include <cstdint>

namespace Object {

// Mirrors the 0x38-byte BaseTypeInfo layout in Runtime DLL.
// Field offsets (set by Runtime ctor at InvokeAll time):
//   +0x00 typeId       (uint, set to 0xFFFFFFFF; Factory::Startup overwrites with CRC32)
//   +0x08 name         (const char*)
//   +0x10 parent       (const char*, colon-stripped)
//   +0x18 abstract     (const char*, may be null)
//   +0x20 factoryFn    (Base* (*)(void))
//   +0x28 inPlaceFn    (Base* (*)(void*))
//   +0x30 next         (BaseTypeInfo*) link in _firstTypeInfo
struct BaseTypeInfoStorage {
    uint8_t bytes[0x38];
};

typedef void* (*FactoryFn)(void);
typedef void* (*InPlaceCreateFn)(void* mem);

// Static-init linked list node. One per registered type.
struct FactoryRegistration {
    BaseTypeInfoStorage  storage;
    const char*          typeName;
    const char*          parentName;
    const char*          abstractName;
    FactoryFn            factoryFn;
    InPlaceCreateFn      inPlaceCreateFn;
    FactoryRegistration* next;

    FactoryRegistration(const char* name, const char* parent, const char* abstract,
                        FactoryFn factory, InPlaceCreateFn inPlace);
};

// Walk the pending list and invoke Runtime DLL's BaseTypeInfo ctor for each.
// Idempotent — safe to call once, NOPs on subsequent calls.
// Must run AFTER EngineFuncsInit (needs fn_BaseTypeInfo_Ctor) and BEFORE
// Engine::Engine::Startup (Factory::Startup walks _firstTypeInfo and freezes
// the typeIds).
// Returns count of successful registrations.
int InvokeAllRegistrations();

}  // namespace Object

// Convenience macros. Expand to a static-init FactoryRegistration object.
//   REGISTER_TYPE(Citizen, "Component", &Citizen_Factory, &Citizen_CreateInPlace);
//   REGISTER_TYPE_FULL(NameDatabase, "Object::External", "ndb",
//                      &NameDatabase_Factory, &NameDatabase_CreateInPlace);
//
// The macro builds a unique name per file via __LINE__ to allow multiple
// registrations in the same translation unit (e.g. base + derived in one file).
#define REGISTER_TYPE_IMPL2(name, parent, abstract, factory, inplace, lineNum)  \
    namespace {                                                                 \
        ::Object::FactoryRegistration g_factory_reg_##lineNum(                  \
            name, parent, abstract,                                             \
            reinterpret_cast<::Object::FactoryFn>(factory),                     \
            reinterpret_cast<::Object::InPlaceCreateFn>(inplace));              \
    }
#define REGISTER_TYPE_IMPL1(name, parent, abstract, factory, inplace, lineNum)  \
    REGISTER_TYPE_IMPL2(name, parent, abstract, factory, inplace, lineNum)
#define REGISTER_TYPE_FULL(Name, Parent, Abstract, Factory, InPlace)            \
    REGISTER_TYPE_IMPL1(#Name, Parent, Abstract, Factory, InPlace, __LINE__)
#define REGISTER_TYPE(Name, Parent, Factory, InPlace)                           \
    REGISTER_TYPE_FULL(Name, Parent, nullptr, Factory, InPlace)
