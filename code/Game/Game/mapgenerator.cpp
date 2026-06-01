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
// STANDALONE mode: the original (0x1400a2980) isn't in our process (we ARE the
// exe), so the FULL deserialize is reimplemented in clean C++ below. This reads
// the entire MapGenerator stream — scalar header AND every dynamic pool — so the
// start-condition data (citizen families, structures, resource/cull lists, env
// template refs) is actually present for the start-town ECS spawn.
//
// FRESH-LOAD ASSUMPTION: Serialize runs once on a freshly-constructed
// MapGenerator (Init zeroed every count/cap/ptr). So the exe array-resize
// helpers (FUN_14001b270/340/4e0/6c0) collapse to "alloc count*stride + init
// each element" — the copy/destruct (grow/shrink) paths never execute. See
// MG_AllocArray below. (If Serialize were ever re-run on a populated generator,
// the old buffers would leak; not a concern for new-game load.)
//
// RESOLVE DEFERRED: the original resolves each ExternalRef name to a template
// during deserialize (FUN_14001b1d0). We SKIP that here — every start-town
// spawn pass re-resolves each entry lazily via ResolveExternalByName before use
// (e.g. ComponentEntity_impl_09e0/_1fc0/_02f0, FUN_1400a12b0). So the pools just
// store the names; resolution happens at spawn time.
//
// Stream order mirrors the original 0x1400a2980 EXACTLY (note the non-monotonic
// tail: +0x118, +0x138, +0x130, +0x150, +0x158, +0x170).
#ifdef STANDALONE_EXE
namespace {
// Fresh-load dynamic array: set header {ptr@0, count@8, cap@0xC} and allocate
// count*stride bytes. Returns the new buffer (uninitialized — caller inits).
void* MG_AllocArray(void* header, int count, int stride) {
    uint8_t* h = reinterpret_cast<uint8_t*>(header);
    *(int32_t*)(h + 8)   = count;
    *(int32_t*)(h + 0xc) = count;
    void* buf = (count > 0 && fn_MemAlloc && g_pMemAllocSingleton)
                ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * (uint32_t)stride, 8)
                : nullptr;
    *(void**)h = buf;
    return buf;
}
// Read a count int from the stream into *countField; return it.
int MG_ReadCount(void* serializer, void* countField) {
    if (g_fn_Serialize_Int) g_fn_Serialize_Int(serializer, countField, nullptr);
    return *(int32_t*)countField;
}
// Read a pool of ExternalRef entries (stride 0x18: String@0, gen@8=-1, ptr@0x10=0).
// header/countField are the array header and its count int in the struct.
void MG_ReadRefPool(void* serializer, void* header, void* countField) {
    int n = MG_ReadCount(serializer, countField);
    uint8_t* e = reinterpret_cast<uint8_t*>(MG_AllocArray(header, n, 0x18));
    for (int i = 0; i < n; ++i, e += 0x18) {
        *(uint64_t*)e        = 0;            // String (empty)
        *(int32_t*)(e + 8)   = -1;           // factory generation (unresolved)
        *(uint64_t*)(e + 0x10) = 0;          // resolved template ptr
        if (g_fn_String_Serialize) g_fn_String_Serialize(e, serializer);  // read name
    }
}
} // namespace
#endif

void MapGenerator_Serialize(MapGenerator* self, void* stream) {
    EnsureFuncsResolved();
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !g_fn_String_Serialize || !g_fn_Serialize_Int ||
        !g_fn_Serialize_Bool || !fn_Serializer_Serialize_Float || !fn_ExternalPtr_Serialize2) {
        return;  // missing a Runtime serialize primitive — can't proceed
    }
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // --- scalar header (order matches Easy.rsc: _stringTable,_name,_description,
    //     _exposed,_startAreaSize,_citizenRadius,_citizen) ---
    fn_ExternalPtr_Serialize2(s + 0x20, serializer);   // +0x20 _stringTable ref
    g_fn_String_Serialize(s + 0x28, serializer);       // +0x28 _name
    g_fn_String_Serialize(s + 0x30, serializer);       // +0x30 _description
    g_fn_Serialize_Bool(serializer, s + 0x38, nullptr);// +0x38 _exposed
    g_fn_Serialize_Int(serializer, s + 0x3c, nullptr); // +0x3c _startAreaSize (townSize)
    fn_Serializer_Serialize_Float(serializer, &self->scale, nullptr);     // +0x40 _citizenRadius
    g_fn_String_Serialize(s + 0x48, serializer);       // +0x48 _citizen template ref

    // --- _citizens: InitialFamily[] (stride 8: bool _educated@0, int _maxChildCount@4) ---
    {
        int n = MG_ReadCount(serializer, s + 0x68);
        uint8_t* e = reinterpret_cast<uint8_t*>(MG_AllocArray(s + 0x60, n, 8));
        for (int i = 0; i < n; ++i, e += 8) {
            *(uint64_t*)e = 0;
            g_fn_Serialize_Bool(serializer, e, nullptr);        // _educated
            g_fn_Serialize_Int(serializer, e + 4, nullptr);     // _maxChildCount
        }
    }

    // --- refArray1 (+0x70): ExternalRef pool ---
    MG_ReadRefPool(serializer, s + 0x70, s + 0x78);

    // --- _structures: InitialStructure[] (stride 0x30: String@0, gen@8, ptr@0x10,
    //     int _offsetX@0x18, _offsetY@0x1c, _width@0x20, _height@0x24, _rotation@0x28) ---
    {
        int n = MG_ReadCount(serializer, s + 0x88);
        uint8_t* e = reinterpret_cast<uint8_t*>(MG_AllocArray(s + 0x80, n, 0x30));
        for (int i = 0; i < n; ++i, e += 0x30) {
            *(uint64_t*)e          = 0;          // String (empty)
            *(int32_t*)(e + 8)     = -1;         // gen
            *(uint64_t*)(e + 0x10) = 0;          // ptr
            *(uint64_t*)(e + 0x18) = 0;          // _offsetX, _offsetY
            *(uint64_t*)(e + 0x20) = 0xFFFFFFFFFFFFFFFFULL;  // _width=-1, _height=-1
            *(int32_t*)(e + 0x28)  = 0;          // _rotation
            g_fn_String_Serialize(e, serializer);            // _structure ref name
            g_fn_Serialize_Int(serializer, e + 0x18, nullptr);  // _offsetX
            g_fn_Serialize_Int(serializer, e + 0x1c, nullptr);  // _offsetY
            g_fn_Serialize_Int(serializer, e + 0x20, nullptr);  // _width
            g_fn_Serialize_Int(serializer, e + 0x24, nullptr);  // _height
            g_fn_Serialize_Int(serializer, e + 0x28, nullptr);  // _rotation
        }
    }

    // --- refArray3..refArray7 (+0x90,+0xA0,+0xB0,+0xC0,+0xD0): ExternalRef pools ---
    MG_ReadRefPool(serializer, s + 0x90, s + 0x98);
    MG_ReadRefPool(serializer, s + 0xa0, s + 0xa8);
    MG_ReadRefPool(serializer, s + 0xb0, s + 0xb8);
    MG_ReadRefPool(serializer, s + 0xc0, s + 0xc8);
    MG_ReadRefPool(serializer, s + 0xd0, s + 0xd8);

    // --- scalar tail (cull counts, initial resource counts, livestock params) ---
    g_fn_Serialize_Int(serializer, s + 0xe0, nullptr);
    g_fn_Serialize_Int(serializer, s + 0xe4, nullptr);
    g_fn_Serialize_Int(serializer, s + 0xe8, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0xec), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0xf0), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0xf4), nullptr);
    g_fn_Serialize_Int(serializer, s + 0xf8, nullptr);
    g_fn_Serialize_Int(serializer, s + 0xfc, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x100, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x104, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x108, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x10c, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x110, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x114, nullptr);

    // --- env template refs + interleaved scalars (EXACT stream order) ---
    g_fn_String_Serialize(s + 0x118, serializer);       // descRef1 (terrain)
    g_fn_String_Serialize(s + 0x138, serializer);       // descRef2 (climate)
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x130), nullptr); // terrainAmplitude
    g_fn_Serialize_Int(serializer, s + 0x150, nullptr); // chunkSize
    g_fn_String_Serialize(s + 0x158, serializer);       // descRef3 (vegetation)
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x170), nullptr);

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
