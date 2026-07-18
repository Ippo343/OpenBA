// code/Game/Game/climategenerator.cpp
// Clean reimplementation of ClimateGenerator game logic.
//
// ClimateGenerator (0x110 bytes) -- climate/weather generation parameters.
// Extends Object::External (0x20 base).
// Vtable at 0x14015e140.
//
// Contains temperature ranges, precipitation params, wind speed,
// and a 0xC0-byte block of seasonal/biome data (48 floats).
//
// Field layout (from constructor + serialize):
//   +0x00: vtable
//   +0x08: External base (0x18 bytes)
//   +0x20: ExternalPtr (8 bytes, serialized via FUN_14000bf40)
//   +0x28: System::String (8 bytes, name/reference)
//   +0x30: bool enabled (default 1)
//   +0x34: float temperatureBase (default 50.0f)
//   +0x38: float tempVariation (default 30.0f)
//   +0x3C: float precipitation (default 5.0f)
//   +0x40: float windSpeed (default 10.0f)
//   +0x44: float[48] climateData (0xC0 bytes, default 0)
//   +0x104: float field104 (default 0.0f)
//   +0x108: float field108 (default 1.0f)
//   +0x10C: float field10C (default 1.0f)
//
// Decompiled from Application-x64.exe.

#include "Game/climategenerator_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

// ============================================================
// Local function pointers (resolved once)
// ============================================================

// IO::Serializer::Serialize(float&, const char*) -- via IAT at 0x140152a60
static void (*s_fn_Serialize_Float)(void* serializer, void* field, const char* name) = nullptr;

// IO::Serializer::Serialize(int count, void* buffer, const char* name) -- bulk serialize
// Via IAT at 0x140151ea8 (used for climate data block)
static void (*s_fn_SerializeBulk)(void* serializer, int count, void* buffer, const char* name) = nullptr;

// ============================================================
// ClimateGenerator_InitFuncs -- resolve local function pointers
// ============================================================
void ClimateGenerator_InitFuncs() {
    // Float serialize via IAT dispatch slot
    uintptr_t iatSerFloat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iatSerFloat)
        s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iatSerFloat);

    // Bulk serialize via IAT dispatch slot (for climate data array)
    uintptr_t iatSerBulk = ResolveAddr(Module::Exe, 0x140151ea8);
    if (iatSerBulk)
        s_fn_SerializeBulk = *reinterpret_cast<void(**)(void*, int, void*, const char*)>(iatSerBulk);
}

// ============================================================
// Lazy init guard
// ============================================================
static bool s_inited = false;
static void EnsureInit() {
    if (!s_inited) {
        ClimateGenerator_InitFuncs();
        s_inited = true;
    }
}

// ============================================================
// ClimateGenerator::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x140018f60
// Destroys the String at +0x28, calls External::~External, then
// conditionally frees memory if flags & 1.
void* ClimateGenerator_Dtor(ClimateGenerator* self, uint32_t flags) {
    EnsureInit();
    // Destroy System::String at +0x28
    if (fn_String_Dtor) {
        fn_String_Dtor(reinterpret_cast<uint8_t*>(self) + 0x28);
    }

    // Call External base destructor
    fn_External_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// ClimateGenerator::Cleanup (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x140018f40
// Destroys the object (without freeing) then re-constructs in place.
void ClimateGenerator_Cleanup(ClimateGenerator* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    ClimateGenerator_Init(self);
}

// ============================================================
// ClimateGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x140092fa0
// Serializes all fields using IO::Serializer with version 0.
// Returns 1 (success) -- original returns AL=1.
void ClimateGenerator_Serialize(ClimateGenerator* self, void* stream) {
    EnsureInit();
    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) g_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize ExternalPtr at +0x20
    if (fn_ExternalPtr_Serialize2)
        fn_ExternalPtr_Serialize2(reinterpret_cast<uint8_t*>(self) + 0x20, serializer);

    // Serialize String at +0x28
    if (g_fn_String_Serialize)
        g_fn_String_Serialize(reinterpret_cast<uint8_t*>(self) + 0x28, serializer);

    // Serialize bool at +0x30
    if (g_fn_Serialize_Bool)
        g_fn_Serialize_Bool(serializer, reinterpret_cast<uint8_t*>(self) + 0x30, nullptr);

    // Serialize float fields: temperatureBase(+0x34), tempVariation(+0x38),
    // precipitation(+0x3C), windSpeed(+0x40)
    if (s_fn_Serialize_Float) {
        s_fn_Serialize_Float(serializer, &self->temperatureBase, nullptr);
        s_fn_Serialize_Float(serializer, &self->tempVariation, nullptr);
        s_fn_Serialize_Float(serializer, &self->precipitation, nullptr);
        s_fn_Serialize_Float(serializer, &self->windSpeed, nullptr);
    }

    // Serialize climate data block: 0x30 (48) floats = 0xC0 bytes at +0x44
    if (s_fn_SerializeBulk)
        s_fn_SerializeBulk(serializer, 0x30, self->_climateData, nullptr);

    // Serialize float fields at end: field104(+0x104), field108(+0x108), field10C(+0x10C)
    if (s_fn_Serialize_Float) {
        s_fn_Serialize_Float(serializer, &self->_field104, nullptr);
        s_fn_Serialize_Float(serializer, &self->_field108, nullptr);
        s_fn_Serialize_Float(serializer, &self->_field10C, nullptr);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// ClimateGenerator::Slot8 (vtable +0x40)
// ============================================================
// Original: 0x140018f30
// Null-check: if self is null, return null.
// Otherwise, call External ctor, set vtable, set defaults, return self.
void* ClimateGenerator_Slot8(ClimateGenerator* self) {
    if (!self) return nullptr;

    ClimateGenerator_Init(self);
    return self;
}

// ============================================================
// ClimateGenerator::Init -- Initialize all fields to defaults
// ============================================================
// Mirrors ClimateGenerator_Ctor at 0x1400930c0.
// Calls Object::External::External for base init, then sets
// ClimateGenerator-specific fields to their default values.
void ClimateGenerator_Init(ClimateGenerator* self) {
    // Initialize External base class
    fn_External_External(self);

    // Set ClimateGenerator vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ClimateGeneratorAddr::Vtable);
    self->vtable = s_vtable;

    // ExternalPtr at +0x20 = null
    self->_field20 = nullptr;
    // String at +0x28 = null (8-byte COW string)
    self->_field28 = nullptr;

    // Climate parameters
    self->_field30 = 1;              // enabled
    self->temperatureBase = 50.0f;   // base temperature
    self->tempVariation = 30.0f;     // temperature variation
    self->precipitation = 5.0f;      // precipitation amount
    self->windSpeed = 10.0f;         // wind speed

    // Trailing float fields
    self->_field104 = 0;             // zeroed (float 0.0 or uint 0)
    self->_field108 = 1.0f;
    self->_field10C = 1.0f;

    // Zero the 0xC0-byte climate data block at +0x44
    memset(self->_climateData, 0, 0xC0);
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================
// ClimateGenerator is an exe-defined Object::External subclass (name
// "ClimateGenerator", parent "Object::External", typeId ~CRC32 = 0x1cf9f3b3,
// size 0x110). In standalone the exe's *_RegisterType never runs, so without
// this block ExternalFactory::FindOrLoad("Game/Climate/ClimateList.rsc")'s
// per-element load hits a NullFactory and crashes inside Runtime IsDerivedFrom.
//
// Vtable @ 0x14015e140 (9 Object::External slots; verified via read_memory):
//   +0x00 Dtor       0x140018f60  -> ClimateGenerator_Dtor      (reimpl)
//   +0x08 GetTypeInfo 0x140018ee0 -> ReturnNull stub            (returns BaseTypeInfo*; null safe, not used on load)
//   +0x10 GetTypeId  0x140018ed0  -> ClimateGenerator_GetTypeId (returns ~CRC32 typeId)
//   +0x18 GetVersion 0x1400930b0  -> Return3 stub               (exe body = `mov eax,3; ret` — file format v3)
//   +0x20 Compile    0x14003a912  -> Return0 stub               (shared External::Compile thunk; not used on load)
//   +0x28 Serialize  0x140092fa0  -> ClimateGenerator_Serialize (reimpl)
//   +0x30 Cleanup    0x140018f40  -> ClimateGenerator_Cleanup   (reimpl)
//   +0x38 GetDesc    0x140001640  -> Return0 stub               (shared base forwarder; not used on load)
//   +0x40 Slot8      0x140018f30  -> ClimateGenerator_Slot8     (reimpl)
// Mirrors the StatGoal/MenuScene standalone pattern.
//
// IMPORTANT: GetVersion MUST return 3. Object::External::Load gates the element
// Serialize on `fileVersion == obj->GetVersion()`; a wrong constant silently
// drops the element and the climate combo comes up empty.


#include "Object/factory_registration.h"
#include "address_registry.h"

// GetTypeId (+0x10): return the runtime CRC32 typeId for "ClimateGenerator".
static uint32_t ClimateGenerator_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ClimateGenerator");
    return cached;
}

// Adapter trampolines so vtable slots match the raw (void*, ...) shape.
static void* ClimateGenerator_Dtor_Trampoline(void* self, uint32_t flags) {
    return ClimateGenerator_Dtor(reinterpret_cast<ClimateGenerator*>(self), flags);
}
static void ClimateGenerator_Serialize_Trampoline(void* self, void* stream) {
    ClimateGenerator_Serialize(reinterpret_cast<ClimateGenerator*>(self), stream);
}
static void ClimateGenerator_Cleanup_Trampoline(void* self) {
    ClimateGenerator_Cleanup(reinterpret_cast<ClimateGenerator*>(self));
}
static void* ClimateGenerator_Slot8_Trampoline(void* self) {
    return ClimateGenerator_Slot8(reinterpret_cast<ClimateGenerator*>(self));
}

// Safe const-return stubs for non-reimplemented slots (mirror exe behavior).
static int   ClimateGenerator_VtblStub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   ClimateGenerator_VtblStub_Return0(void* /*self*/) { return 0; }
static void* ClimateGenerator_VtblStub_ReturnNull(void* /*self*/, ...) { return nullptr; }

static void* g_ClimateGenerator_Vtable_Standalone[11] = {
    reinterpret_cast<void*>(&ClimateGenerator_Dtor_Trampoline),     // +0x00 Dtor
    reinterpret_cast<void*>(&ClimateGenerator_VtblStub_ReturnNull), // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ClimateGenerator_GetTypeId),           // +0x10 GetTypeId
    reinterpret_cast<void*>(&ClimateGenerator_VtblStub_Return3),    // +0x18 GetVersion (mov eax,3)
    reinterpret_cast<void*>(&ClimateGenerator_VtblStub_Return0),    // +0x20 Compile (unused on load)
    reinterpret_cast<void*>(&ClimateGenerator_Serialize_Trampoline),// +0x28 Serialize
    reinterpret_cast<void*>(&ClimateGenerator_Cleanup_Trampoline),  // +0x30 Cleanup
    reinterpret_cast<void*>(&ClimateGenerator_VtblStub_Return0),    // +0x38 GetDesc forwarder (unused on load)
    reinterpret_cast<void*>(&ClimateGenerator_Slot8_Trampoline),    // +0x40 Slot8
    reinterpret_cast<void*>(&ClimateGenerator_VtblStub_ReturnNull), // +0x48 (over-read guard)
    reinterpret_cast<void*>(&ClimateGenerator_VtblStub_ReturnNull), // +0x50 (over-read guard)
};

extern "C" void RegisterClimateGeneratorVtable() {
    RegisterExeAddr(ClimateGeneratorAddr::Vtable,
                    reinterpret_cast<uintptr_t>(g_ClimateGenerator_Vtable_Standalone));
}

extern "C" void* ClimateGenerator_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(ClimateGenerator), 8);
    if (!p) return nullptr;
    ClimateGenerator_Init(reinterpret_cast<ClimateGenerator*>(p));
    return p;
}

extern "C" void* ClimateGenerator_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    ClimateGenerator_Init(reinterpret_cast<ClimateGenerator*>(memory));
    return memory;
}

REGISTER_TYPE(ClimateGenerator, "Object::External",
              &ClimateGenerator_Factory, &ClimateGenerator_CreateInPlace);

