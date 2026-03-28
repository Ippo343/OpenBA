// code/Game/Game/mapgenerator.cpp
// Clean reimplementation of MapGenerator game logic.
//
// MapGenerator (0x178 bytes) — world map generation parameters.
// Extends Object::External (0x20 base).
// Vtable at 0x14015ed80.
//
// MapGenerator defines all procedural world generation settings:
// terrain parameters, resource lists (trees, stones, ores, herbs, animals),
// biome configs, and references to description files.
//
// Decompiled from Application-x64.exe.

#include "Game/mapgenerator_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

// ============================================================
// Locally resolved function pointers (not in shared engine_funcs.h)
// ============================================================

// FUN_14001aea0 — MapGenerator destructor body (cleans up all fields before External::~External)
typedef void (*fn_MapGenerator_DtorBody_t)(void* self);
static fn_MapGenerator_DtorBody_t s_fn_DtorBody = nullptr;

// FUN_14001b030 — destroy dynamic array of 0x18-byte ExternalRef elements
typedef void (*fn_DestroyRefArray_t)(void* arrayHeader);
static fn_DestroyRefArray_t s_fn_DestroyRefArray = nullptr;

// FUN_14001b0a0 — destroy dynamic array of 0x30-byte complex elements
typedef void (*fn_DestroyRefArray30_t)(void* arrayHeader);
static fn_DestroyRefArray30_t s_fn_DestroyRefArray30 = nullptr;

// The original Serialize function — called directly since it has complex
// array resize/ExternalRef resolution helpers that are internal to the exe.
typedef void (*fn_MapGenerator_Serialize_t)(void* self, void* stream);
static fn_MapGenerator_Serialize_t s_fn_OrigSerialize = nullptr;

static bool s_funcsResolved = false;

static void EnsureFuncsResolved() {
    if (s_funcsResolved) return;
    s_fn_DtorBody = reinterpret_cast<fn_MapGenerator_DtorBody_t>(
        ResolveAddr(Module::Exe, MapGeneratorAddr::DtorBody));
    s_fn_DestroyRefArray = reinterpret_cast<fn_DestroyRefArray_t>(
        ResolveAddr(Module::Exe, 0x14001b030));
    s_fn_DestroyRefArray30 = reinterpret_cast<fn_DestroyRefArray30_t>(
        ResolveAddr(Module::Exe, 0x14001b0a0));
    s_fn_OrigSerialize = reinterpret_cast<fn_MapGenerator_Serialize_t>(
        ResolveAddr(Module::Exe, MapGeneratorAddr::Serialize));
    s_funcsResolved = true;
}

// ============================================================
// MapGenerator::Dtor (vtable +0x00) — Scalar deleting destructor
// ============================================================
// Original: 0x14001ae70
// Calls the destructor body (FUN_14001aea0) which cleans up all
// MapGenerator-specific fields (strings, arrays, ExternalRefs),
// then calls Object::External::~External.
// Conditionally frees memory if flags & 1.
void* MapGenerator_Dtor(MapGenerator* self, uint32_t flags) {
    EnsureFuncsResolved();

    // The dtor body destroys all MapGenerator fields in reverse order:
    // - String dtors for descRef3, descRef2, descRef1 (+0x158, +0x138, +0x118)
    // - Array dtors for refArray7..refArray1 (+0xD0..+0x70)
    // - MemFree for boolIntArray (+0x60)
    // - String dtors for templateRef, _field30, _field28 (+0x48, +0x30, +0x28)
    // - External::~External for base class
    if (s_fn_DtorBody) {
        s_fn_DtorBody(self);
    }

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// MapGenerator::Cleanup (vtable +0x30, Slot6) — Reset to defaults
// ============================================================
// Original: 0x14001ae30
// Calls vtable Dtor with flags=0 (destroy without freeing),
// then re-initializes all fields via Init (jump to constructor).
void MapGenerator_Cleanup(MapGenerator* self) {
    // Call Dtor with flags=0 (don't free memory) via vtable
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    MapGenerator_Init(self);
}

// ============================================================
// MapGenerator::Init — Initialize all fields to defaults
// ============================================================
// Mirrors MapGenerator_Ctor (0x1400a3030).
// Calls Object::External::External for base init, then sets
// all MapGenerator-specific fields to their default values.
void MapGenerator_Init(MapGenerator* self) {
    // Initialize External base class
    fn_External_External(self);

    // Set MapGenerator vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, MapGeneratorAddr::Vtable);
    self->vtable = s_vtable;

    // ExternalPtr at +0x20
    self->_field20 = nullptr;

    // Strings at +0x28, +0x30 (zeroed = empty string)
    self->_field28 = nullptr;
    self->_field30 = nullptr;

    // Generation parameters
    self->enabled = 1;          // bool, default true
    self->subdivisions = 8;     // grid subdivision count
    self->scale = 2.0f;         // terrain scale

    // ExternalRef at +0x48 (String + factoryGen + resolvedPtr)
    self->templateRef.strPtr = nullptr;
    self->templateRef.factoryGen = -1;  // 0xFFFFFFFF = unresolved
    self->templateRef.resolved = nullptr;

    // BoolInt array at +0x60
    self->boolIntArray = nullptr;
    self->boolIntCount = 0;

    // Dynamic arrays at +0x70 through +0xD8 (all zeroed)
    memset(&self->refArray1, 0, sizeof(DynArrayHeader));  // +0x70
    memset(&self->refArray2, 0, sizeof(DynArrayHeader));  // +0x80
    memset(&self->refArray3, 0, sizeof(DynArrayHeader));  // +0x90
    memset(&self->refArray4, 0, sizeof(DynArrayHeader));  // +0xA0
    memset(&self->refArray5, 0, sizeof(DynArrayHeader));  // +0xB0
    memset(&self->refArray6, 0, sizeof(DynArrayHeader));  // +0xC0
    memset(&self->refArray7, 0, sizeof(DynArrayHeader));  // +0xD0

    // Scalar generation parameters (+0xE0 through +0x114)
    self->paramE0 = 0;
    self->paramE4 = 0;
    self->paramE8 = 0;
    self->paramEC = 0.0f;
    self->paramF0 = 0.0f;
    self->paramF4 = 0.0f;
    self->paramF8 = 0;
    self->paramFC = 0;
    self->param100 = 0;
    self->param104 = 0;
    self->param108 = 0;
    self->param10C = 0;
    self->param110 = 0;
    self->param114 = 0;

    // ExternalRef at +0x118 (terrain description)
    self->descRef1.strPtr = nullptr;
    self->descRef1.factoryGen = -1;
    self->descRef1.resolved = nullptr;

    // Terrain amplitude
    self->terrainAmplitude = 6.0f;

    // ExternalRef at +0x138 (climate description)
    self->descRef2.strPtr = nullptr;
    self->descRef2.factoryGen = -1;
    self->descRef2.resolved = nullptr;

    // Chunk size
    self->chunkSize = 16;

    // ExternalRef at +0x158 (vegetation description)
    self->descRef3.strPtr = nullptr;
    self->descRef3.factoryGen = -1;
    self->descRef3.resolved = nullptr;

    return;
}

// ============================================================
// MapGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x1400a2980
//
// This is a very complex serialization function with:
// - ExternalPtr serialization (+0x20)
// - String serialization (+0x28, +0x30, +0x48, +0x118, +0x138, +0x158)
// - Bool/Int scalar serialization
// - 7 dynamic array resize+serialize loops with different element sizes
// - ExternalRef resolution callbacks (FUN_14001b1d0) on load
// - Complex array resize helpers (FUN_14001b270, FUN_14001b340, FUN_14001b4e0)
//
// Calls the original function directly since the internal helpers
// (array resize with copy, ExternalRef factory resolution) are
// tightly coupled to the exe's memory allocator and factory system.
void MapGenerator_Serialize(MapGenerator* self, void* stream) {
    EnsureFuncsResolved();
    if (s_fn_OrigSerialize) {
        s_fn_OrigSerialize(self, stream);
    }
}

// ============================================================
// MapGenerator::Slot8 (vtable +0x40) — Reinitialize
// ============================================================
// Original: 0x14001ae20
// If self is null, returns null. Otherwise reinitializes via Init
// (tail-calls to the constructor).
void* MapGenerator_Slot8(MapGenerator* self) {
    if (self == nullptr) {
        return nullptr;
    }
    MapGenerator_Init(self);
    return self;
}
