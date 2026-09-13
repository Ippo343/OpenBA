// code/Game/Game/terraingenerator.cpp
// Clean reimplementation of TerrainGenerator game logic.
//
// TerrainGenerator (0x90 bytes) -- terrain height/noise generation parameters.
// Extends Object::External (0x20 base).
// Vtable at 0x14015f170.
//
// Contains ExternalPtr, 3 noise layer filename strings (each with a -1 sentinel int),
// and a dynamic array of terrain data entries (0x44 bytes each).
//
// Field layout (from constructor + serialize + dtor):
//   +0x00: vtable
//   +0x08: External base (0x18 bytes)
//   +0x20: ExternalPtr (8 bytes)
//   +0x28: System::String noiseFile1 (8 bytes)
//   +0x30: bool enabled (default 1)
//   +0x31: pad (7 bytes)
//   +0x38: System::String noiseFile2 (8 bytes)
//   +0x40: int32_t sentinel1 (default -1)
//   +0x44: pad (4 bytes)
//   +0x48: void* field48 (default 0)
//   +0x50: System::String noiseFile3 (8 bytes)
//   +0x58: int32_t sentinel2 (default -1)
//   +0x5C: pad (4 bytes)
//   +0x60: void* field60 (default 0)
//   +0x68: System::String noiseFile4 (8 bytes)
//   +0x70: int32_t sentinel3 (default -1)
//   +0x74: pad (4 bytes)
//   +0x78: void* field78 (default 0)
//   +0x80: void* terrainDataArray (dynamic array ptr, freed in dtor)
//   +0x88: void* terrainDataInfo (count/capacity)
//
// Decompiled from Application-x64.exe.

#include "Game/terraingenerator_funcs.h"
#include "engine_funcs.h"
#include "system/Filename.h"
#include "resolve.h"
#include "system/String.h"
#include <cstdint>
#include <cstring>

// ============================================================
// Local function pointers (resolved once)
// ============================================================

// FUN_14001fd10 -- resolve filename string to ExternalPtr
// Called after deserializing a filename string when version==0.
// Signature: void (String* fieldAtStringOffset)
static void (*s_fn_ResolveFilenameString)(void* stringField) = nullptr;

// FUN_1400206b0 -- resize terrain data array at +0x80
// Signature: void (void* arrayFieldAt80, int newCount)
static void (*s_fn_TerrainDataResize)(void* arrayField, int newCount) = nullptr;

// FUN_140020870 -- serialize terrain data array elements
// Signature: void (void* serializer, uint32_t count, void* arrayPtr)
static void (*s_fn_TerrainDataSerialize)(void* serializer, uint32_t count, void* arrayPtr) = nullptr;

// ============================================================
// Standalone reimplementations of the +0x80 terrain-data-array helpers.
// In standalone these exe addresses aren't mapped, so ResolveAddr returns
// null and the size table never deserializes (gen+0x80 stays null -> worldgen
// reads dim=0). These faithful reimpls (decompiled from FUN_1400206b0 /
// FUN_140020870) populate the size table so the in-game terrain build works.
// Each row is a SizeRow: 17 ints, stride 0x44; row[0] = map dimension.
// ============================================================

// FUN_1400206b0 -- stride-0x44 dynamic-array resize. New rows get the engine
// default SizeRow template (row[0]=256, etc.).
static void Reimpl_TerrainDataResize(void* arrayField, int newCount) {
    if (!arrayField || !fn_MemAlloc || !g_pMemAllocSingleton) return;
    uint8_t* hdr = reinterpret_cast<uint8_t*>(arrayField);
    int oldCount = *reinterpret_cast<int*>(hdr + 8);
    if (newCount == oldCount) return;
    void* oldPtr = *reinterpret_cast<void**>(hdr + 0);
    *reinterpret_cast<int*>(hdr + 8) = newCount;
    if (*reinterpret_cast<int*>(hdr + 0xc) < newCount) {
        *reinterpret_cast<int*>(hdr + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0)
            *reinterpret_cast<int*>(hdr + 0xc) = newCount * 3;
        int cap = *reinterpret_cast<int*>(hdr + 0xc);
        *reinterpret_cast<void**>(hdr + 0) =
            fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(cap) * 0x44, 4);
    }
    int start = (oldPtr == *reinterpret_cast<void**>(hdr + 0)) ? oldCount : 0;
    uint8_t* dstBase = *reinterpret_cast<uint8_t**>(hdr + 0);
    uint8_t* srcBase = reinterpret_cast<uint8_t*>(oldPtr);
    static const int32_t kDefaultRow[17] = {
        0x100, 0x32, 10, 0x14, 0, 10, 0x14, 3, 1, 1000000, 0,
        1, 10, 0x14, 0x23, 6, 0xc
    };
    for (long long i = start; i < newCount; ++i) {
        int32_t* d = reinterpret_cast<int32_t*>(dstBase + i * 0x44);
        if (!d) continue;
        if (i < oldCount) {
            const int32_t* s = reinterpret_cast<const int32_t*>(srcBase + i * 0x44);
            for (int k = 0; k < 17; ++k) d[k] = s[k];
        } else {
            for (int k = 0; k < 17; ++k) d[k] = kDefaultRow[k];
        }
    }
    if (oldPtr != *reinterpret_cast<void**>(hdr + 0) && oldPtr) fn_MemFreeStatic(oldPtr);
}

// FUN_140020870 -- serialize `count` rows of 17 ints each. NOTE row[9] is
// serialized LAST (after row[16]) -- this field order must match the .rsc write
// order exactly or the stream misaligns.
static void Reimpl_TerrainDataSerialize(void* serializer, uint32_t count, void* arrayPtr) {
    if (static_cast<int>(count) <= 0 || !arrayPtr || !g_fn_Serialize_Int) return;
    static const int kOrder[17] = { 0,1,2,3,4,5,6,7,8,10,11,12,13,14,15,16,9 };
    int32_t* row = reinterpret_cast<int32_t*>(arrayPtr);
    for (uint32_t r = 0; r < count; ++r) {
        for (int k = 0; k < 17; ++k)
            g_fn_Serialize_Int(serializer, &row[kOrder[k]], nullptr);
        row += 17;
    }
}

// ============================================================
// TerrainGenerator_InitFuncs -- resolve local function pointers
// ============================================================
void TerrainGenerator_InitFuncs() {
    // FUN_14001fd10 -- resolve filename string
    s_fn_ResolveFilenameString = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x14001fd10));

    // FUN_1400206b0 -- terrain data array resize (standalone fallback to reimpl)
    s_fn_TerrainDataResize = reinterpret_cast<void(*)(void*, int)>(
        ResolveAddr(Module::Exe, 0x1400206b0));
    if (!s_fn_TerrainDataResize) s_fn_TerrainDataResize = &Reimpl_TerrainDataResize;

    // FUN_140020870 -- terrain data array serialize (standalone fallback to reimpl)
    s_fn_TerrainDataSerialize = reinterpret_cast<void(*)(void*, uint32_t, void*)>(
        ResolveAddr(Module::Exe, 0x140020870));
    if (!s_fn_TerrainDataSerialize) s_fn_TerrainDataSerialize = &Reimpl_TerrainDataSerialize;

}

// ============================================================
// Lazy init guard
// ============================================================
static bool s_inited = false;
static void EnsureInit() {
    if (!s_inited) {
        TerrainGenerator_InitFuncs();
        s_inited = true;
    }
}

// ============================================================
// Helper: Get serializer version
// ============================================================
// The Serializer struct (24 bytes) has an encoding sub-object at +0x00.
// encoding+0x08 = version (int). Returns the version stored in the serializer.
static inline int GetSerializerVersion(void* serializer) {
    // serializer[0] = pointer to encoding object
    // encoding+8 = version
    void* encoding = *reinterpret_cast<void**>(serializer);
    return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(encoding) + 8);
}

// ============================================================
// Helper: Serialize a filename string with version-0 resolution
// ============================================================
// Pattern from TerrainGenerator Slot5: Filename ctor, String::Serialize,
// if version==0 resolve filename, Filename dtor.
static void SerializeFilenameString(void* self_offset, void* serializer) {
    System::Filename filenameObj;   // retail's scope temp; only ctor + dtor
    (void)filenameObj;

    reinterpret_cast<System::String*>(self_offset)->Serialize(serializer);

    if (GetSerializerVersion(serializer) == 0) {
        if (s_fn_ResolveFilenameString)
            s_fn_ResolveFilenameString(self_offset);
    }
}

// ============================================================
// TerrainGenerator::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x140020600
// Calls internal cleanup helper FUN_140020630 then conditionally frees.
// FUN_140020630 frees the terrain data array at +0x80, destroys 4 Strings
// at +0x68, +0x50, +0x38, +0x28, then calls External::~External.
void* TerrainGenerator_Dtor(TerrainGenerator* self, uint32_t flags) {
    EnsureInit();
    // Free terrain data array at +0x80
    void* arrayPtr = self->_field80;
    if (arrayPtr) {
        fn_MemFreeStatic(arrayPtr);
    }
    self->_field80 = nullptr;
    self->_field88 = nullptr;

    // Destroy strings in reverse order: +0x68, +0x50, +0x38, +0x28
    reinterpret_cast<System::String*>(reinterpret_cast<uint8_t*>(self) + 0x68)->~String();
    reinterpret_cast<System::String*>(reinterpret_cast<uint8_t*>(self) + 0x50)->~String();
    reinterpret_cast<System::String*>(reinterpret_cast<uint8_t*>(self) + 0x38)->~String();
    reinterpret_cast<System::String*>(reinterpret_cast<uint8_t*>(self) + 0x28)->~String();

    // Call External base destructor
    fn_External_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// TerrainGenerator::Cleanup (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x1400205e0
// Destroys the object (without freeing) then re-constructs in place.
void TerrainGenerator_Cleanup(TerrainGenerator* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    TerrainGenerator_Init(self);
}

// ============================================================
// TerrainGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x1400b1690
// Complex serialization: ExternalPtr, 3 filename strings with version-0
// resolution, and a dynamic terrain data array.
void TerrainGenerator_Serialize(TerrainGenerator* self, void* stream) {
    EnsureInit();
    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) g_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize ExternalPtr at +0x20
    if (fn_ExternalPtr_Serialize2)
        fn_ExternalPtr_Serialize2(reinterpret_cast<uint8_t*>(self) + 0x20, serializer);

    // Serialize String at +0x28
    reinterpret_cast<System::String*>(reinterpret_cast<uint8_t*>(self) + 0x28)->Serialize(serializer);

    // Serialize bool at +0x30
    if (g_fn_Serialize_Bool)
        g_fn_Serialize_Bool(serializer, reinterpret_cast<uint8_t*>(self) + 0x30, nullptr);

    // Serialize 3 filename strings with version-0 resolution pattern:
    // +0x38, +0x50, +0x68
    SerializeFilenameString(reinterpret_cast<uint8_t*>(self) + 0x38, serializer);
    SerializeFilenameString(reinterpret_cast<uint8_t*>(self) + 0x50, serializer);
    SerializeFilenameString(reinterpret_cast<uint8_t*>(self) + 0x68, serializer);

    // Serialize terrain data array count
    // Read current count from +0x88 into a local, serialize it,
    // then if version==0 resize the array, then serialize array contents.
    uint32_t count = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x88);
    if (g_fn_Serialize_Int)
        g_fn_Serialize_Int(serializer, &count, nullptr);

    if (GetSerializerVersion(serializer) == 0) {
        // Resize terrain data array to deserialized count
        if (s_fn_TerrainDataResize)
            s_fn_TerrainDataResize(reinterpret_cast<uint8_t*>(self) + 0x80, count);
    }

    // Serialize terrain data array contents
    if (s_fn_TerrainDataSerialize) {
        void* arrayPtr = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self) + 0x80);
        s_fn_TerrainDataSerialize(serializer, count, arrayPtr);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TerrainGenerator::Slot8 (vtable +0x40)
// ============================================================
// Original: 0x1400205d0
// Null-check: if self is null, return null.
// Otherwise, call External ctor, set vtable, set defaults, return self.
void* TerrainGenerator_Slot8(TerrainGenerator* self) {
    if (!self) return nullptr;

    TerrainGenerator_Init(self);
    return self;
}

// ============================================================
// TerrainGenerator::Init -- Initialize all fields to defaults
// ============================================================
// Mirrors TerrainGenerator_Ctor at 0x1400b1810.
// Calls Object::External::External for base init, then sets
// TerrainGenerator-specific fields to their default values.
void TerrainGenerator_Init(TerrainGenerator* self) {
    // Initialize External base class
    fn_External_External(self);

    // Set TerrainGenerator vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, TerrainGeneratorAddr::Vtable);
    self->vtable = s_vtable;

    // ExternalPtr at +0x20
    self->_field20 = nullptr;
    // String at +0x28
    self->_field28 = nullptr;

    // Bool enabled at +0x30
    self->_field30 = 1;

    // Noise layer 1: String at +0x38, sentinel at +0x40, ptr at +0x48
    self->_field38 = nullptr;
    self->_field40 = -1;
    self->_field48 = nullptr;

    // Noise layer 2: String at +0x50, sentinel at +0x58, ptr at +0x60
    self->_field50 = nullptr;
    self->_field58 = -1;
    self->_field60 = nullptr;

    // Noise layer 3: String at +0x68, sentinel at +0x70, ptr at +0x78
    self->_field68 = nullptr;
    self->_field70 = -1;
    self->_field78 = nullptr;

    // Terrain data array at +0x80, count/info at +0x88
    self->_field80 = nullptr;
    self->_field88 = nullptr;
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================
// TerrainGenerator is an exe-defined Object::External subclass (name
// "TerrainGenerator", parent "Object::External", typeId ~CRC32 = 0x562a2462,
// size 0x90). Loaded as each element of Game/Terrain/TerrainList.rsc
// (Valleys/Mountains .rsc). Without this block ExternalFactory::FindOrLoad's
// per-element load hits a NullFactory and crashes in Runtime IsDerivedFrom.
//
// Vtable @ 0x14015f170 (9 Object::External slots; verified via read_memory):
//   +0x00 Dtor       0x140020600 -> TerrainGenerator_Dtor      (reimpl)
//   +0x08 GetTypeInfo 0x140020580 -> ReturnNull stub           (mov rax,[0x1401c5b48]; ret; null safe on load)
//   +0x10 GetTypeId  0x140020570 -> TerrainGenerator_GetTypeId (mov eax,[0x1401c5b40]; returns ~CRC32 typeId)
//   +0x18 GetVersion 0x1400b1800 -> Return4 stub               (exe body = `mov eax,4; ret` — file format v4)
//   +0x20 Compile    0x14003a912 -> Return0 stub               (shared External::Compile thunk; unused on load)
//   +0x28 Serialize  0x1400b1690 -> TerrainGenerator_Serialize (reimpl)
//   +0x30 Cleanup    0x1400205e0 -> TerrainGenerator_Cleanup   (reimpl)
//   +0x38 GetDesc    0x140001640 -> Return0 stub               (shared base forwarder; unused on load)
//   +0x40 Slot8      0x1400205d0 -> TerrainGenerator_Slot8     (reimpl)
//
// IMPORTANT: GetVersion MUST return 4 (verified: exe body is `mov eax,4; ret`).
// Object::External::Load gates the element Serialize on
// `fileVersion == obj->GetVersion()`; a wrong constant silently drops the
// element and the terrain combo comes up empty.


#include "Object/factory_registration.h"
#include "address_registry.h"

// GetTypeId (+0x10): runtime CRC32 typeId for "TerrainGenerator".
static uint32_t TerrainGenerator_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TerrainGenerator");
    return cached;
}

// Adapter trampolines so vtable slots match the raw (void*, ...) shape.
static void* TerrainGenerator_Dtor_Trampoline(void* self, uint32_t flags) {
    return TerrainGenerator_Dtor(reinterpret_cast<TerrainGenerator*>(self), flags);
}
static void TerrainGenerator_Serialize_Trampoline(void* self, void* stream) {
    TerrainGenerator_Serialize(reinterpret_cast<TerrainGenerator*>(self), stream);
}
static void TerrainGenerator_Cleanup_Trampoline(void* self) {
    TerrainGenerator_Cleanup(reinterpret_cast<TerrainGenerator*>(self));
}
static void* TerrainGenerator_Slot8_Trampoline(void* self) {
    return TerrainGenerator_Slot8(reinterpret_cast<TerrainGenerator*>(self));
}

// Safe const-return stubs for non-reimplemented slots (mirror exe behavior).
static int   TerrainGenerator_VtblStub_Return4(void* /*self*/) { return 4; } // GetVersion
static int   TerrainGenerator_VtblStub_Return0(void* /*self*/) { return 0; }
static void* TerrainGenerator_VtblStub_ReturnNull(void* /*self*/, ...) { return nullptr; }

static void* g_TerrainGenerator_Vtable_Standalone[11] = {
    reinterpret_cast<void*>(&TerrainGenerator_Dtor_Trampoline),     // +0x00 Dtor
    reinterpret_cast<void*>(&TerrainGenerator_VtblStub_ReturnNull), // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&TerrainGenerator_GetTypeId),           // +0x10 GetTypeId
    reinterpret_cast<void*>(&TerrainGenerator_VtblStub_Return4),    // +0x18 GetVersion (mov eax,4)
    reinterpret_cast<void*>(&TerrainGenerator_VtblStub_Return0),    // +0x20 Compile (unused on load)
    reinterpret_cast<void*>(&TerrainGenerator_Serialize_Trampoline),// +0x28 Serialize
    reinterpret_cast<void*>(&TerrainGenerator_Cleanup_Trampoline),  // +0x30 Cleanup
    reinterpret_cast<void*>(&TerrainGenerator_VtblStub_Return0),    // +0x38 GetDesc forwarder (unused on load)
    reinterpret_cast<void*>(&TerrainGenerator_Slot8_Trampoline),    // +0x40 Slot8
    reinterpret_cast<void*>(&TerrainGenerator_VtblStub_ReturnNull), // +0x48 (over-read guard)
    reinterpret_cast<void*>(&TerrainGenerator_VtblStub_ReturnNull), // +0x50 (over-read guard)
};

extern "C" void RegisterTerrainGeneratorVtable() {
    RegisterExeAddr(TerrainGeneratorAddr::Vtable,
                    reinterpret_cast<uintptr_t>(g_TerrainGenerator_Vtable_Standalone));
}

extern "C" void* TerrainGenerator_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(TerrainGenerator), 8);
    if (!p) return nullptr;
    TerrainGenerator_Init(reinterpret_cast<TerrainGenerator*>(p));
    return p;
}

extern "C" void* TerrainGenerator_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    TerrainGenerator_Init(reinterpret_cast<TerrainGenerator*>(memory));
    return memory;
}

REGISTER_TYPE(TerrainGenerator, "Object::External",
              &TerrainGenerator_Factory, &TerrainGenerator_CreateInPlace);

// Register the two terrain-data-array helpers to our existing faithful reimpls
// (FUN_1400206b0 resize / FUN_140020870 serialize). These already serve as a
// runtime fallback in TerrainGenerator_InitFuncs when ResolveAddr returns 0;
// registering the exe addresses makes the ResolveAddr call sites resolve them
// directly (and closes the Game-module gap for these two addresses).
void RegisterGameTerrainReimpls() {
    RegisterExeAddr(0x1400206b0, reinterpret_cast<uintptr_t>(&Reimpl_TerrainDataResize));
    RegisterExeAddr(0x140020870, reinterpret_cast<uintptr_t>(&Reimpl_TerrainDataSerialize));
}

