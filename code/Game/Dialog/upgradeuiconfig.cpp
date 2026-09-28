// code/Game/Dialog/upgradeuiconfig.cpp
// Clean reimplementation of UpgradeUIConfig (the upgrade panel's config resource).
//
// UpgradeUIConfig (0x28 bytes) — extends UI::DialogControllerConfig (byte-wise
// just Object::External's 0x20 base; the subclass adds one field).
// Registered by retail 0x14014ca00 into type-id slot DWORD_1401c5868.
// Vtable at 0x14015df60. NO "UpgradeDescription" class exists in retail — this
// config IS the upgrade definition:
//
//   +0x20  Object::ExternalPtr<ComponentEntity> — the upgrade TARGET BUILDING
//          template (e.g. the stone house). UpgradeUI's click handler
//          (0x140086c90) copies it into DestroyComponent+0x60 before pumping
//          the state machine; state-5 tick 0x14011c120 clones it via
//          ComponentEntity_CopyCtor to spawn the replacement construction site.
//
// Factory        0x140016c00: MemAlloc(0x28) + base ctor + vtable + clear +0x20
// CreateInPlace  0x140016c60: same, no alloc
// Serialize      0x140086fb0 (vtable +0x28): IO::Serializer(stream, 0) +
//                ExternalPtr<ComponentEntity>::Serialize on +0x20 — ported via
//                the canonical ExternalPtrSerializeGeneric body.
// GetVersion     (vtable +0x18, 0x140087000): return 1.
// GetTypeId      (vtable +0x10, 0x140016be0): return DWORD_1401c5868.
//
// Replaces the UI_DIALOG_STUB(UpgradeUIConfig, ...) zero-fill stub, whose +0x20
// stayed null — the upgrade button could never resolve a target.
//
// Decompiled from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "Game/serialize_externals.h"
#include "Object/factory_registration.h"
#include "address_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

#include "object/Internal.h"
#include "Dialog/UpgradeUIConfig.h"
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.9 CG2)

// ============================================================================
// ENDGAME B.3.8 PNL — UpgradeUIConfig as a REAL class (UpgradeUIConfig.h). The hand-built
// 9-slot table, its Init/Factory/CreateInPlace entry points and the lazy
// GetTypeId cache are gone; the compiler emits the table.
// ============================================================================

// [0] 0x140016d00: ~UpgradeUIConfig (restamp + ~DialogControllerConfig + ~External, all
// the compiler's), MemFree on &1.
Object::Base* UpgradeUIConfig::DeletingDtor(unsigned flags) {
    this->~UpgradeUIConfig();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [4] not declared: retail's slot is the imported External::Compile, which
// the class inherits.

// [5] 0x140086fb0 — deserialize the target-template reference
// (ExternalPtr<ComponentEntity>, the canonical ExternalPtrSerializeGeneric body).
bool UpgradeUIConfig::Serialize(void* stream) {
    IO::Serializer serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    // ENDGAME B.3.9 CG2: TypeId_ComponentEntity (0x1401c5d70) -> ComponentEntity::TypeId().
    uint32_t componentEntityTypeId = ComponentEntity::TypeId();
    ExternalPtrSerializeGeneric(&_target, &serializer, &componentEntityTypeId,
                                "ComponentEntity");
    return true;
}

// [6] 0x140016cb0 — destroy in place, construct again.
Object::External* UpgradeUIConfig::Reconstruct() {
    this->~UpgradeUIConfig();
    return new (this) UpgradeUIConfig();
}

// [7] 0x140001640, the body shared by 119 exe tables:
// `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` = destroy in place, no free.
void UpgradeUIConfig::Destruct() { DeletingDtor(0); }

// [8] 0x140016c60 — placement construction (MSVC folded it with createInPlace).
Object::External* UpgradeUIConfig::Construct() { return new (this) UpgradeUIConfig(); }

// create 0x140016c00: MemAlloc(0x28, 8), then the ctor.
void* UpgradeUIConfig::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(UpgradeUIConfig), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) UpgradeUIConfig());
}
// createInPlace 0x140016c60.
void* UpgradeUIConfig::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) UpgradeUIConfig());
}


DEFINE_EXE_TYPE(UpgradeUIConfig, "UI::DialogControllerConfig")
