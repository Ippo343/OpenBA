// code/Game/Game/scenario.cpp
// Clean reimplementation of Scenario game logic.
//
// Scenario (0x90 bytes) -- game scenario definition.
// Extends Achievement::Achievement (0x58 base).
// Vtable at 0x14015f020.
//
// Holds references to MapGenerator, TerrainGenerator, ClimateGenerator,
// and scenario name/description strings. The Achievement base provides
// goal lists and achievement tracking.
//
// Decompiled from Application-x64.exe.

#include "Game/scenario_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "achievement/AchievementManager.h"   // ENDGAME B.3.7 callers (K2): QUALIFIED base calls
#include <cstdint>
#include <new>

// ============================================================
// Achievement base class functions (Runtime DLL exports)
// Declared locally -- should be moved to engine_funcs.h later.
// ============================================================

// Achievement::Achievement::Achievement() -- ordinal 0x26d (621)
// ?Achievement@0@QEAA@XZ
typedef void (*fn_Achievement_Ctor_t)(void* self);
static fn_Achievement_Ctor_t s_fn_Achievement_Ctor = nullptr;

// Typed ExternalPtr serialize for MapGenerator (0x140019ec0)
typedef void (*fn_ExtPtr_MapGen_Serialize_t)(void* field, void* serializer);
static fn_ExtPtr_MapGen_Serialize_t s_fn_ExtPtr_MapGen_Serialize = nullptr;

// Typed ExternalPtr serialize for TerrainGenerator (0x14001a0c0)
typedef void (*fn_ExtPtr_TerrainGen_Serialize_t)(void* field, void* serializer);
static fn_ExtPtr_TerrainGen_Serialize_t s_fn_ExtPtr_TerrainGen_Serialize = nullptr;

// Typed ExternalPtr serialize for ClimateGenerator (0x1400194a0)
typedef void (*fn_ExtPtr_ClimateGen_Serialize_t)(void* field, void* serializer);
static fn_ExtPtr_ClimateGen_Serialize_t s_fn_ExtPtr_ClimateGen_Serialize = nullptr;

// ============================================================
// Lazy-init resolver for local function pointers
// ============================================================
static void EnsureResolved() {
    if (s_fn_Achievement_Ctor) return;  // already resolved

    // Achievement::Achievement base methods. The old ordinals (0x26d..0x309)
    // are STALE — in the shipped Runtime-x64.dll they resolve to unrelated
    // functions (0x26d -> ~LayoutElement, 0x26e -> ~Light, 0x309 ->
    // EdgeMeshInstance::operator new), so Scenario_Init called ~LayoutElement on
    // the fresh Scenario and freed garbage -> heap corruption. Resolve by
    // verified Runtime address instead (ResolveAddr applies the ASLR delta).
    // Addresses confirmed in Ghidra:
    //   ctor 0x18009e240, ~Achievement 0x18009e180,
    //   GetFileVersion (vtable+0x18) 0x18009e170, Serialize 0x18009e070.
    s_fn_Achievement_Ctor = reinterpret_cast<fn_Achievement_Ctor_t>(
        ResolveAddr(Module::Runtime, 0x18009e240));

    s_fn_ExtPtr_MapGen_Serialize = reinterpret_cast<fn_ExtPtr_MapGen_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140019ec0));
    s_fn_ExtPtr_TerrainGen_Serialize = reinterpret_cast<fn_ExtPtr_TerrainGen_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14001a0c0));
    s_fn_ExtPtr_ClimateGen_Serialize = reinterpret_cast<fn_ExtPtr_ClimateGen_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400194a0));
}

// ============================================================
// Scenario::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x14001eb40
//
// Restores vtable, destroys two String fields (+0x88, +0x58),
// calls Achievement base dtor, conditionally frees memory.
void* Scenario_Dtor(Scenario* self, uint32_t flags) {
    EnsureResolved();

    // Restore vtable to Scenario (for correct virtual dispatch during destruction)
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ScenarioAddr::Vtable);
    self->vtable = s_vtable;

    // Destroy String at +0x88 then +0x58 (reverse order of construction)
    reinterpret_cast<System::String*>(&self->_field88)->~String();
    reinterpret_cast<System::String*>(&self->scenarioName)->~String();

    // Achievement::Achievement::~Achievement (0x18009e180) — a QUALIFIED member
    // call (B7-3/B7-13): retail CALLs the base dtor unconditionally.
    reinterpret_cast<Achievement::Achievement*>(self)->Achievement::Achievement::~Achievement();   // B.3.7 bridge

    // Conditional free
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Scenario::Slot3 (vtable +0x18) -- GetFileVersion
// ============================================================
// Original: 0x1400a9460
//
// Calls Achievement base GetFileVersion and OR's with 0x600.
// The 0x600 represents Scenario's own version flags.
uint32_t Scenario_Slot3(Scenario* self) {
    EnsureResolved();

    // Achievement::Achievement::GetFileVersion (slot 3, 0x18009e170) — QUALIFIED:
    // an unqualified call would dispatch through this exe table back into this slot.
    uint32_t baseVersion = (uint32_t)reinterpret_cast<const Achievement::Achievement*>(self)   // B.3.7 bridge
        ->Achievement::Achievement::GetFileVersion();
    return baseVersion | 0x600;
}

// ============================================================
// Scenario::Slot5 (vtable +0x28) -- Serialize
// ============================================================
// Original: 0x1400a9370
//
// First serializes Achievement base, then Scenario-specific fields:
//   int +0x60, int +0x64, String +0x58, ExtPtr<MapGen> +0x70,
//   ExtPtr<TerrainGen> +0x78, ExtPtr<ClimateGen> +0x80,
//   String +0x88, bool +0x68, bool +0x69
void Scenario_Serialize(Scenario* self, void* stream) {
    EnsureResolved();

    // Serialize Achievement base first: Achievement::Achievement::Serialize (slot 5,
    // 0x18009e070) — QUALIFIED (B7-3).
    reinterpret_cast<Achievement::Achievement*>(self)->Achievement::Achievement::Serialize(stream);   // B.3.7 bridge

    // Create serializer on stack (24 bytes), version 0
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize Scenario-specific fields
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->_field60, nullptr);   // B.3.4 bridge (cast dropped: _field60 is int32_t)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->_field64, nullptr);   // B.3.4 bridge (cast dropped: _field64 is int32_t)

    reinterpret_cast<System::String*>(&self->scenarioName)->Serialize(serializer);

    if (s_fn_ExtPtr_MapGen_Serialize) {
        s_fn_ExtPtr_MapGen_Serialize(&self->mapGenerator, serializer);
    }

    if (s_fn_ExtPtr_TerrainGen_Serialize) {
        s_fn_ExtPtr_TerrainGen_Serialize(&self->terrainGenerator, serializer);
    }

    if (s_fn_ExtPtr_ClimateGen_Serialize) {
        s_fn_ExtPtr_ClimateGen_Serialize(&self->climateGenerator, serializer);
    }

    reinterpret_cast<System::String*>(&self->_field88)->Serialize(serializer);

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->_field68, nullptr);   // B.3.4 bridge (cast dropped: _field68 is bool)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->_field69, nullptr);   // B.3.4 bridge (cast dropped: _field69 is bool)

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Scenario::Slot6 (vtable +0x30) -- Cleanup
// ============================================================
// Original: 0x14001ead0
//
// Destroys (via Dtor with flags=0), then re-inits in place.
void Scenario_Cleanup(Scenario* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    Scenario_Init(self);
}

// ============================================================
// Scenario::Init (vtable +0x40, Slot8) -- Initialize fields
// ============================================================
// Original: 0x14001ea70
//
// Constructs Achievement base, sets Scenario vtable, zeroes all
// Scenario-specific fields. Also used as Slot8 (null-safe variant
// in factory is separate, but the init logic is the same).
//
// Note: The original Slot8 has a null check (if self == null return null).
// Scenario_Init does not -- the null check is handled by the caller/factory.
void Scenario_Init(Scenario* self) {
    EnsureResolved();

    // Initialize Achievement base class
    if (s_fn_Achievement_Ctor) {
        s_fn_Achievement_Ctor(self);
    }

    // Set Scenario vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ScenarioAddr::Vtable);
    self->vtable = s_vtable;

    // Zero Scenario-specific fields
    // Original writes: *(uint64*)(+0x58)=0, *(uint64*)(+0x60)=0,
    //   *(uint16*)(+0x68)=1, *(uint64*)(+0x70)=0, *(uint64*)(+0x78)=0,
    //   *(uint64*)(+0x80)=0, *(uint64*)(+0x88)=0
    self->scenarioName       = nullptr;  // +0x58: String (zeroed = empty)
    self->_field60           = 0;        // +0x60
    self->_field64           = 0;        // +0x64
    self->_field68           = true;     // +0x68: uint16 write of 1 = byte 0x68=1, 0x69=0
    self->_field69           = false;    // +0x69
    self->mapGenerator       = nullptr;  // +0x70
    self->terrainGenerator   = nullptr;  // +0x78
    self->climateGenerator   = nullptr;  // +0x80
    self->_field88           = nullptr;  // +0x88: String (zeroed = empty)
}

// ============================================================
// Standalone scaffolding — vtable + factory + type registration
// ============================================================
// Mirrors the StatGoal pattern (code/Game/Goal/statgoal.cpp). Required so the
// menu-startup engine Loader thread can instantiate a Scenario when External::
// Load reads one out of the Game/Global resource tree. Without it the loader
// hits `[NullFactory] Hit for type: Scenario` -> IsDerivedFrom(NULL) -> crash.
//
// Original registration (Scenario_RegisterType @0x14014cd10):
//   BaseTypeInfo(&TypeInfo_Scenario, "Scenario", "::Achievement::Achievement",
//                null, Scenario_Factory(0x14001e9f0), Scenario_Cleanup).
//
// Only the load-critical vtable slots are wired (Dtor/GetTypeId/GetFileVersion/
// Serialize, plus Cleanup/Init). The base Achievement::Serialize (0x18009e070)
// serializes its own fields without virtual-calling any Scenario slot, so the
// inherited base-thunk slots stay null during load — same as StatGoal.

#include "Object/factory_registration.h"
#include "address_registry.h"

// Slot2 (vtable +0x10): GetTypeId — original returns the static TypeInfo_Scenario
// typeId; standalone reads it from REGISTER_TYPE storage.
static uint32_t Scenario_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("Scenario");
    return cached;
}

// Slot8 (vtable +0x40): in-place Init with the original's null guard, returning
// self (raw vtable slot signature).
static void* Scenario_Slot8_Init(void* self) {
    if (self) Scenario_Init(reinterpret_cast<Scenario*>(self));
    return self;
}

static void* g_Scenario_Vtable[13] = {
    reinterpret_cast<void*>(&Scenario_Dtor),          // +0x00 Dtor
    nullptr,                                           // +0x08 Slot1 (BaseTypeInfo ptr)
    reinterpret_cast<void*>(&Scenario_GetTypeId),      // +0x10 Slot2 GetTypeId
    reinterpret_cast<void*>(&Scenario_Slot3),          // +0x18 Slot3 GetFileVersion
    nullptr,                                           // +0x20 Slot4 (base JMP thunk)
    reinterpret_cast<void*>(&Scenario_Serialize),      // +0x28 Slot5 Serialize
    reinterpret_cast<void*>(&Scenario_Cleanup),        // +0x30 Slot6 Cleanup
    nullptr,                                           // +0x38 Slot7 (base stub)
    reinterpret_cast<void*>(&Scenario_Slot8_Init),     // +0x40 Slot8 Init
    nullptr,                                           // +0x48 Slot9 (null in original)
    nullptr,                                           // +0x50 Slot10 (base thunk)
    nullptr,                                           // +0x58 Slot11 (base thunk)
    nullptr,                                           // +0x60 Slot12 (base thunk)
};

extern "C" void RegisterScenarioVtable() {
    RegisterExeAddr(ScenarioAddr::Vtable, reinterpret_cast<uintptr_t>(g_Scenario_Vtable));
}

extern "C" void* Scenario_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x90, 8);  // Scenario size = 0x90
    if (!p) return nullptr;
    Scenario_Init(reinterpret_cast<Scenario*>(p));
    return p;
}

extern "C" void* Scenario_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    Scenario_Init(reinterpret_cast<Scenario*>(memory));
    return memory;
}

// Parent string matches the original Scenario_RegisterType exactly (leading "::");
// Factory::Startup resolves the parent by literal name against registered types.
REGISTER_TYPE(Scenario, "::Achievement::Achievement",
              &Scenario_Factory, &Scenario_CreateInPlace);

