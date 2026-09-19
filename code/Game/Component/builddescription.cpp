// code/Game/Component/builddescription.cpp
// Clean reimplementation of BuildDescription (entity description block).
//
// BuildDescription (0x38 bytes) — construction-cost parameters for a building.
// Extends Object::External (0x20 base). Vtable at 0x1401622f0.
// Parent type "Description".  (ToolKit: BuildDescription = construction costs.)
//
// Factory  : 0x14002f320  (MemAlloc 0x38 + base init)  fields:
//   void* @ +0x20 = 0   (dynamic array base: {External* @0, int @8} elements)
//   int   @ +0x28 = 0   (array count, also doubles as capacity slot @+0x2c)
//   int   @ +0x30 = 0
//   bool  @ +0x34 = 0
// CreateInPlace: 0x14002f390
// Serialize (vtable +0x28, 0x140109140):
//   int  count           @ +0x28   (also resizes array to `count` on load)
//   for each element (stride 0x10 at base +0x20):
//     int          @ elem+0x08     (cost quantity)
//     ExternalRef  @ elem+0x00     (FUN_140002340 — resource/material description ref)
//   int  @ +0x30
//   bool @ +0x34
// GetVersion (vtable +0x18, 0x140109220): mov eax,2; ret -> file version 2.
// Dtor (vtable +0x00, 0x14002f460): vtable reset + MemFree(array @+0x20) +
//   External::~External + free.
//
// STANDALONE notes:
//  - Fresh-load assumption (Init zeroed the array header): the exe resize helper
//    FUN_14000da70 collapses to "alloc count*0x10 + zero each element". Mirrors
//    MapGenerator's MG_AllocArray.
//  - The per-element ExternalRef serialize (FUN_140002340) is not mapped; we
//    reuse the proven generic ExternalPtr serialize reimpl
//    (Reimpl_ExternalPtr_Serialize_StringTable). The typeId only drives an
//    IsDerivedFrom validation that Object::IsDerivedFromOrTrust no-ops in
//    standalone, so the body is stream-identical.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

static const uintptr_t kBuildDescriptionVtable = 0x1401622f0;

static void BuildDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kBuildDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;  // array base ptr (+0x20) and count@+0x28 cleared
    *(int32_t*) (s + 0x30) = 0;  // int
    *(uint8_t*) (s + 0x34) = 0;  // bool
}

// ============================================================
// BuildDescription_Serialize (vtable +0x28, 0x140109140)
// ============================================================
void BuildDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        return;
    }
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Preserve any already-populated requirement array across re-loads. The engine
    // re-invokes this Serialize for an already-loaded resource.
    //
    // [CORRECTED 2026-09-06, runtime port Wave 3 lane C — this comment used to say
    // "on EVERY FindOrLoad", and the mechanism is narrower than that.]
    // ExternalFactory::FindOrLoad (Runtime 0x1800bf320) does NOT reload or re-parse a
    // cached resource: on a Find hit it returns the cached External, and it calls
    // ExternalFactory::Add (0x1800be570) again only when EnableLoad (factory+0x41) is
    // set. Add is what re-invokes Serialize — vtable slot +0x28, driven with an
    // IO::VisitorStream, i.e. a reference-VISIT pass over the object rather than a
    // re-read of the file — and it does so only when `1 << factory->_cleanBitIndex`
    // was NOT already set in the entry's usage mask (entry+0x10). So the re-invoke is
    // once per resource per clean generation, not once per FindOrLoad, and the stream
    // it runs against is a visitor, which is exactly why the re-passes below carry no
    // data and yield empty elements.
    // The 6× count measured for the Pasture is left as recorded; nobody has re-measured
    // it against this mechanism, and a re-measurement is worth doing before trusting
    // either number. See docs/runtime_port/WAVE3_LANE_C.md.
    //
    // The first (path-mode) load resolves each _rawMaterial correctly; later
    // (reference-mode) re-loads read the ref through the standalone's incomplete
    // reference table and yield an empty element {material=0, count=0}. Without a
    // guard the last (empty) write wins. Verified for the CreateFenced Pasture,
    // whose `:build` sub-resource is FindOrLoad'd 6×: the first pass populated
    // {RawMaterialWood,1}; the 5 re-passes clobbered it to {0,0}. Building
    // descriptions are immutable templates, so a re-read can only legitimately
    // reproduce identical data — therefore an empty re-load must never overwrite a
    // populated array. (Non-fenced buildings are loaded once and were unaffected,
    // which is why only the Pasture lost its icon.)
    void*   oldArr   = *(void**)(s + 0x20);
    int32_t oldCount = *(int32_t*)(s + 0x28);
    bool oldPopulated = (oldArr && oldCount > 0 && *(uint64_t*)oldArr != 0);

    // count @ +0x28 (read from stream)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    int count = *(int32_t*)(s + 0x28);

    // Fresh-load: alloc count * 0x10 element buffer. Each element = {External* @0,
    // int @8}. Populate a fresh buffer; commit to +0x20 only after confirming it
    // isn't an empty reference-mode re-load (below).
    uint8_t* arr = nullptr;
    if (count > 0) {
        arr = reinterpret_cast<uint8_t*>(
            fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 0x10, 8));
    }

    uint8_t* e = arr;
    for (int i = 0; i < count; ++i, e += 0x10) {
        *(uint64_t*)(e + 0)  = 0;   // External* (resolved ref)
        *(int32_t*) (e + 8)  = 0;   // int cost qty
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(e + 8), nullptr);   // B.3.4 bridge  (int @ elem+0x08)
        Reimpl_ExternalPtr_Serialize_StringTable(e + 0, serializer);    // ref  @ elem+0x00
    }

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge  (int +0x30)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x34), nullptr);   // B.3.4 bridge  (bool +0x34)

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge

    // Commit, keeping a populated array over an empty re-load.
    bool newPopulated = (count > 0 && arr && *(uint64_t*)arr != 0);
    if (oldPopulated && !newPopulated) {
        if (arr && fn_MemFreeStatic) fn_MemFreeStatic(arr);  // discard empty re-load
        arr = reinterpret_cast<uint8_t*>(oldArr);
        count = oldCount;
    } else if (oldArr && oldArr != arr && fn_MemFreeStatic) {
        fn_MemFreeStatic(oldArr);                            // replaced -> free prior
    }
    *(void**)(s + 0x20)   = arr;
    *(int32_t*)(s + 0x28) = count;
    *(int32_t*)(s + 0x2c) = count;
}

// ============================================================
// BuildDescription_Dtor (vtable +0x00, 0x14002f460)
// ============================================================
void* BuildDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kBuildDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    // Free the element array (+0x20). Elements hold weak External* refs (no
    // per-element dtor in the original) — only the buffer itself is freed.
    if (fn_MemFreeStatic && *(void**)(s + 0x20)) {
        fn_MemFreeStatic(*(void**)(s + 0x20));
    }
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t BuildDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("BuildDescription");
    return cached;
}

static int   BuildDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   BuildDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* BuildDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* BuildDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* BuildDescription_GetComponentTypeId(void*, void* out) {
    return ::Object::WriteComponentTypeId(out, "BuildComponent");
}

static void* g_BuildDescription_Vtable[11] = {
    reinterpret_cast<void*>(&BuildDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&BuildDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&BuildDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&BuildDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&BuildDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&BuildDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&BuildDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&BuildDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&BuildDescription_GetComponentTypeId),// +0x40 GetComponentTypeId
    reinterpret_cast<void*>(&BuildDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&BuildDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterBuildDescriptionVtable() {
    RegisterExeAddr(kBuildDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_BuildDescription_Vtable));
}

extern "C" void* BuildDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    BuildDescription_Init(p);
    return p;
}

static void* BuildDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    BuildDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(BuildDescription, "Description",
              &BuildDescription_Factory, &BuildDescription_CreateInPlace);

