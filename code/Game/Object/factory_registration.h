#pragma once
// ============================================================================
// code/Game/Object/factory_registration.h — the exe's type records and the
// by-name / by-id helpers over Object::Factory.
//
// ENDGAME B.3.9 RF (B39_RULES.md D6): the FactoryRegistration staging list,
// InvokeAllRegistrations() and REGISTER_TYPE are gone. Every exe class's
// reflection record is a real `Object::BaseTypeInfo` class static
// (DEFINE_EXE_TYPE, Object/exe_type.h) that CRT init links into
// `BaseTypeInfo::_firstTypeInfo`; Engine::Startup -> Object::Factory::Startup
// then assigns each typeId = ~CRC32(name) and builds the id-keyed tree that
// every lookup below walks.
//
// Lookup mechanism (Object::External::Load and friends): typeId is read from
// the .rsc payload, then the tree at Object::Factory::_singleton is searched
// by id. The matching node holds a pointer to the class's BaseTypeInfo; its
// +0x20 slot is the class's CreateNewObject.
// ============================================================================

#include <cstdint>

namespace Object {

class BaseTypeInfo;

// The factory of an ABSTRACT exe base (DEFINE_EXE_ABSTRACT_TYPE, exe_type.h):
// retail's is a `_purecall` stub; ours logs the name to stderr and answers null.
void* AbstractTypeFactory(const char* typeName);

// The exe records (one per DEFINE_EXE_TYPE expansion), as the linker collected
// them in `.data$exetype_b`. Walks the section, not the chain, so it is valid
// from CRT init on; `typeId` is the CRC only after Object::Factory::Startup.
// Diagnostics and route code only (standalone/dlgctrl_route.cpp).
typedef void (*ExeTypeInfoVisitFn)(const BaseTypeInfo& info, void* ctx);
void ForEachExeTypeInfo(ExeTypeInfoVisitFn fn, void* ctx);

// THE COUNT PROOF (B39_RULES D6 / RF item 1c). Call once, after
// Object::Factory::Startup. Every exe record the linker collected must be in
// `_firstTypeInfo` (its CRT-init constructor ran and linked it), carry
// typeId == ~CRC32(name), and every id in the whole chain must be unique
// (Factory::Startup's Insert silently keeps the first of two equal ids). Any
// failure is FATAL: stderr + abort(), in every build (not a printf, which the
// perf build strips). Returns the number of exe records.
int VerifyExeTypeInfos();

// A registered type's id by NAME: Object::Factory::GetTypeIdFromName (retail
// 0x1800c0580 — CRC the name, answer it iff the tree holds it). 0 for a name
// the tree does not hold, and 0 before Engine::Startup has built the Factory.
uint32_t GetTypeIdByName(const char* name);

// Reverse of the above: the registered type NAME for a type id, or nullptr if
// the tree does not hold it (or for 0 / 0xFFFFFFFF). Diagnostics only — a raw
// type id in a log ("component tid=0x9d9af271") is unreadable, and this
// codebase has repeatedly mis-labelled a DAT_ type-id global in comments.
const char* GetTypeNameById(uint32_t typeId);

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

// Lazy resolve a type ID at runtime via Object::Factory::GetTypeIdFromName.
// Walks the live factory tree, so it sees the exe records AND the Runtime
// classes' records (Graphics::MaterialInstance / GraphicsMesh / Particle ...).
//
// Used by the External-serialize helpers in code/Game/Game/serialize_*.cpp
// when their .rdata BaseTypeInfo* slot is uninitialized in standalone mode.
//
// `simpleName` is the class name without namespace (e.g. "MaterialInstance",
// not "Graphics::MaterialInstance"). The helper caches per-name results in
// a small static table to avoid re-walking the factory tree every serialize.
//
// Returns 0 when the Factory singleton is not yet wired (caller should fall
// back to skip-validation).
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

// Type-derivation test by ancestor NAME over the Factory tree
// (Factory::IsDerivedFrom's parent links, which Startup's second pass built):
// true when the type `typeId` is `ancestorName` or derives from it. false for
// 0 / 0xFFFFFFFF, for an id the tree does not hold and for an ancestor name
// the tree does not hold (where Factory::IsDerivedFrom itself would fault) —
// e.g. the StateMachineComponent controller scan replicating retail
// 0x14012b6a0's IsDerivedFrom(comp, ControllerComponent).
bool IsTypeDerivedFromByName(uint32_t typeId, const char* ancestorName);

}  // namespace Object
