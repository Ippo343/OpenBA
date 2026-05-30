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
// DLL mode calls the original function directly since the internal helpers
// (array resize with copy, ExternalRef factory resolution) are tightly coupled
// to the exe's memory allocator and factory system.
//
// STANDALONE mode: the original (0x1400a2980) doesn't exist, so we deserialize
// the SCALAR PREFIX only — enough for the NewGameDialog start-condition combo,
// which displays the name String (+0x28) and gates visibility on the enabled
// bool (+0x38). We deliberately STOP before the 7 dynamic-array loops:
//   - their resize helpers (FUN_14001b270/340/4e0/6c0) are exe-code (NULL here),
//   - and the original's loop bodies are INLINE and deref the array pointer, so
//     running them with a skipped (NULL) resize would crash on a null array.
// Per the ExternalList load model each generator loads from its OWN .rsc file
// with its own stream that closes when Serialize returns, so stopping early
// leaves later fields default WITHOUT corrupting siblings or the list. The full
// field set (resource lists, descRefs) is only needed once map generation
// actually runs (M5 / GameUpdateLoop::Load) — wire the array helpers then.
//
// Prefix order mirrors the original 0x1400a2980 head exactly:
//   Serializer(local,stream,0); ExternalPtr(+0x20); String(+0x28 name);
//   String(+0x30); bool(+0x38 enabled); int(+0x3c subdivisions);
//   float(+0x40 scale); ~Serializer.
void MapGenerator_Serialize(MapGenerator* self, void* stream) {
    EnsureFuncsResolved();
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) g_fn_Serializer_Ctor(serializer, stream, 0);

    // +0x20 ExternalPtr (CRITICAL: advances the cursor so the name reads from
    // the correct offset). fn_ExternalPtr_Serialize2 == exe 0x14000bf40.
    if (fn_ExternalPtr_Serialize2)
        fn_ExternalPtr_Serialize2(s + 0x20, serializer);

    // +0x28 name String (shown in the combo), +0x30 String
    if (g_fn_String_Serialize) {
        g_fn_String_Serialize(s + 0x28, serializer);
        g_fn_String_Serialize(s + 0x30, serializer);
    }
    // +0x38 enabled bool (combo visibility gate at visibilityOffset 0x38)
    if (g_fn_Serialize_Bool)
        g_fn_Serialize_Bool(serializer, s + 0x38, nullptr);
    // +0x3c subdivisions int
    if (g_fn_Serialize_Int)
        g_fn_Serialize_Int(serializer, s + 0x3c, nullptr);
    // +0x40 scale float
    if (fn_Serializer_Serialize_Float)
        fn_Serializer_Serialize_Float(serializer, &self->scale, nullptr);

    // STOP — do not read the +0x68 array count (would enter null-array loops).
    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
    return;
#else
    if (s_fn_OrigSerialize) {
        s_fn_OrigSerialize(self, stream);
    }
#endif
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

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================
// MapGenerator is an exe-defined Object::External subclass (name "MapGenerator",
// parent "Object::External", typeId ~CRC32 = 0xe3a3ae93, size 0x178). Loaded as
// each element of Game/StartCondition/StartConditions.rsc (and the per-start
// MapGenerator .rsc files: Easy/Medium/Hard). Without this block
// ExternalFactory::FindOrLoad's per-element load hits a NullFactory and crashes
// in Runtime IsDerivedFrom.
//
// Vtable @ 0x14015ed80 (9 Object::External slots; verified via read_memory):
//   +0x00 Dtor       0x14001ae70 -> MapGenerator_Dtor       (reimpl)
//   +0x08 GetTypeInfo 0x14001add0 -> ReturnNull stub        (mov rax,[0x1401c59c0]; ret; null safe on load)
//   +0x10 GetTypeId  0x14001adc0 -> MapGenerator_GetTypeId  (mov eax,[0x1401c59b8]; returns ~CRC32 typeId)
//   +0x18 GetVersion 0x1400a3020 -> Return8 stub            (exe body = `mov eax,8; ret` — file format v8)
//   +0x20 Compile    0x14003a912 -> Return0 stub            (shared External::Compile thunk; unused on load)
//   +0x28 Serialize  0x1400a2980 -> MapGenerator_Serialize  (reimpl)
//   +0x30 Cleanup    0x14001ae30 -> MapGenerator_Cleanup    (reimpl)
//   +0x38 GetDesc    0x140001640 -> Return0 stub            (shared base forwarder; unused on load)
//   +0x40 Slot8      0x14001ae20 -> MapGenerator_Slot8      (reimpl)
//
// IMPORTANT: GetVersion MUST return 8 (verified: exe body is `mov eax,8; ret`).
// Object::External::Load gates the element Serialize on
// `fileVersion == obj->GetVersion()`; a wrong constant silently drops the
// element and the start-condition combo comes up empty.

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

// GetTypeId (+0x10): runtime CRC32 typeId for "MapGenerator".
static uint32_t MapGenerator_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MapGenerator");
    return cached;
}

// Adapter trampolines so vtable slots match the raw (void*, ...) shape.
static void* MapGenerator_Dtor_Trampoline(void* self, uint32_t flags) {
    return MapGenerator_Dtor(reinterpret_cast<MapGenerator*>(self), flags);
}
static void MapGenerator_Serialize_Trampoline(void* self, void* stream) {
    MapGenerator_Serialize(reinterpret_cast<MapGenerator*>(self), stream);
}
static void MapGenerator_Cleanup_Trampoline(void* self) {
    MapGenerator_Cleanup(reinterpret_cast<MapGenerator*>(self));
}
static void* MapGenerator_Slot8_Trampoline(void* self) {
    return MapGenerator_Slot8(reinterpret_cast<MapGenerator*>(self));
}

// Safe const-return stubs for non-reimplemented slots (mirror exe behavior).
static int   MapGenerator_VtblStub_Return8(void* /*self*/) { return 8; } // GetVersion
static int   MapGenerator_VtblStub_Return0(void* /*self*/) { return 0; }
static void* MapGenerator_VtblStub_ReturnNull(void* /*self*/, ...) { return nullptr; }

static void* g_MapGenerator_Vtable_Standalone[11] = {
    reinterpret_cast<void*>(&MapGenerator_Dtor_Trampoline),     // +0x00 Dtor
    reinterpret_cast<void*>(&MapGenerator_VtblStub_ReturnNull), // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&MapGenerator_GetTypeId),           // +0x10 GetTypeId
    reinterpret_cast<void*>(&MapGenerator_VtblStub_Return8),    // +0x18 GetVersion (mov eax,8)
    reinterpret_cast<void*>(&MapGenerator_VtblStub_Return0),    // +0x20 Compile (unused on load)
    reinterpret_cast<void*>(&MapGenerator_Serialize_Trampoline),// +0x28 Serialize
    reinterpret_cast<void*>(&MapGenerator_Cleanup_Trampoline),  // +0x30 Cleanup
    reinterpret_cast<void*>(&MapGenerator_VtblStub_Return0),    // +0x38 GetDesc forwarder (unused on load)
    reinterpret_cast<void*>(&MapGenerator_Slot8_Trampoline),    // +0x40 Slot8
    reinterpret_cast<void*>(&MapGenerator_VtblStub_ReturnNull), // +0x48 (over-read guard)
    reinterpret_cast<void*>(&MapGenerator_VtblStub_ReturnNull), // +0x50 (over-read guard)
};

extern "C" void RegisterMapGeneratorVtable() {
    RegisterExeAddr(MapGeneratorAddr::Vtable,
                    reinterpret_cast<uintptr_t>(g_MapGenerator_Vtable_Standalone));
}

extern "C" void* MapGenerator_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(MapGenerator), 8);
    if (!p) return nullptr;
    MapGenerator_Init(reinterpret_cast<MapGenerator*>(p));
    return p;
}

extern "C" void* MapGenerator_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    MapGenerator_Init(reinterpret_cast<MapGenerator*>(memory));
    return memory;
}

REGISTER_TYPE(MapGenerator, "Object::External",
              &MapGenerator_Factory, &MapGenerator_CreateInPlace);

#endif // STANDALONE_EXE
