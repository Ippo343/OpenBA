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

// Look up a registered type's typeId by name. Returns the value at
// storage[0..3] of the matching FactoryRegistration — i.e. BaseTypeInfo.typeId,
// which Object::Factory::Startup populates with the CRC32 of the type name.
// Before Factory::Startup runs, returns 0xFFFFFFFF (the sentinel set by
// BaseTypeInfo::BaseTypeInfo). Returns 0 if `name` was never registered.
//
// Use case: each class's GetTypeId virtual (vtable slot +0x10) calls this,
// caching the result in a function-local static after first hit.
uint32_t GetTypeIdByName(const char* name);

// Reverse of the above: the registered type NAME for a runtime type id, or nullptr if
// no registered type carries it. Diagnostics only — this is a linear walk of the whole
// registration list, so never call it on a hot path. It exists because a raw type id in
// a log ("component tid=0x9d9af271") is unreadable, and because this codebase has
// repeatedly mis-labelled a DAT_ type-id global in comments (0x1401c8070 is called
// WorkComponent, StorageLocationComponent AND MatDesc in different files). Printing the
// name the RUNNING binary resolves settles which one is right.
const char* GetTypeNameById(uint32_t typeId);

// Diagnostic (BANISHED_DUMP_SLOT3=1): walk every registered *Component type,
// invoke its factory once, and report whether vtable slot [3] (+0x18, the
// stream Serialize the save/load persist machinery vcalls) is a real per-class
// serializer or still the base-only Component serialize (= per-instance state
// silently resets on save/load). Must run AFTER Object::Factory::Startup —
// factories cache GetTypeIdByName results, which are sentinel 0xFFFFFFFF
// before Startup. Leaks one small object per class; diagnostics only.
void DumpSlot3Inventory();

// The BaseTypeInfo pointer for a registered type. Component vtable slot 1 (GetTypeInfo)
// returns this; in retail it is a per-class .rdata global 8 bytes after the type id.
void* GetTypeInfoByName(const char* name);

// Description vtable +0x40 ("GetComponentTypeId") contract. The real exe slot is
// `void f(self=RCX, uint32_t* out=RDX) { *out = <Component>TypeId; return out; }`
// — ComponentEntity_CopyCtor (0x1400c3720) calls it on each of a template's
// description sub-blocks and only instantiates a component when *out != 0. Our
// Foundation-C description factories stubbed this slot to return null, so every
// spawned entity came out with zero components. Each description that maps to a
// component installs a +0x40 that calls this with its component's registered name;
// data-only descriptions (Size/Value/CreatePlaced/...) leave the stub (returns
// null → no component). For an unregistered name GetTypeIdByName returns 0, which
// also correctly yields "no component".
inline void* WriteComponentTypeId(void* out, const char* componentName) {
    if (out) *reinterpret_cast<uint32_t*>(out) = GetTypeIdByName(componentName);
    return out;
}

// Lazy resolve a type ID at runtime via Object::Factory::GetTypeIdFromName
// (Runtime DLL). Walks the live factory tree, so it sees both exe-side
// REGISTER_TYPE entries AND Runtime-DLL-self-registered types like
// Graphics::MaterialInstance / Graphics::GraphicsMesh / Graphics::Particle.
//
// Used by the External-serialize helpers in code/Game/Game/serialize_*.cpp
// when their .rdata BaseTypeInfo* slot is uninitialized in standalone mode.
//
// `simpleName` is the class name without namespace (e.g. "MaterialInstance",
// not "Graphics::MaterialInstance"). The helper caches per-name results in
// a small static table to avoid re-walking the factory tree every serialize.
//
// Returns 0 when the Factory singleton or fn_Factory_GetTypeIdFromName is
// not yet wired (caller should fall back to skip-validation).
uint32_t ResolveTypeIdByNameLazy(const char* simpleName);

// IsDerivedFrom wrapper. Returns true if obj is non-null. Per
// `feedback_factory_cascade_strategy.md`, the standalone Runtime factory
// tree is incomplete — calling fn_IsDerivedFrom can return false (or
// NULL-deref) for legit objects, nullifying their loads. Trust
// ExternalFactory::FindOrLoad's return instead.
//
// Used by serialize_externals.cpp / serialize_arrays.cpp / etc. as the gate
// after a typed FindOrLoad. Also used in path-export (encState==3) for
// side-effect calls that the original exe makes; those become no-ops in
// standalone.
bool IsDerivedFromOrTrust(void* obj, uint32_t typeId);

// Registry-based type-derivation test: true when the registered type whose
// typeId matches `typeId` has `ancestorName` anywhere in its REGISTER_TYPE
// parent-name chain (inclusive: a type "derives from" its own name). This is
// the standalone-reliable substitute for the engine's IsDerivedFrom when the
// question is about EXE-side classes (all 310 are registered with full parent
// chains in factory_registrations_bulk.cpp) — e.g. the StateMachineComponent
// controller scan replicating retail 0x14012b6a0's
// IsDerivedFrom(comp, ControllerComponent). Returns false for typeId 0 /
// unregistered ids.
bool IsTypeDerivedFromByName(uint32_t typeId, const char* ancestorName);

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
