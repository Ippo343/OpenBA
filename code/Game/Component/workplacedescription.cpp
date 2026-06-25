// code/Game/Component/workplacedescription.cpp
// Clean reimplementation of WorkPlaceDescription (entity description block).
//
// WorkPlaceDescription (0x30 bytes) — workplace job slots (two Profession refs).
// Extends Object::External (0x20 base). Vtable at 0x140164dc0.
// Parent type "Description".
//
// Factory  : 0x14003a090  (MemAlloc 0x30 + ctor)  fields +0x20=0, +0x28=0
// CreateInPlace: 0x14003a100
// Serialize (vtable +0x28, 0x14012f420):
//   ExternalPtr<Profession> @ +0x20   (FUN_140018020, typeId TypeInfo_Profession)
//   ExternalPtr<Profession> @ +0x28   (FUN_140018020)
//   Both are 8-byte resolved External* fields stored in place.
// GetVersion (vtable +0x18, 0x14012f480): mov eax,2; ret -> file version 2.
// Dtor (vtable +0x00, 0x14003a1c0): vtable reset + External::~External + free
//   (the two +0x20/+0x28 fields are weak External* refs, not owned — no dtor).
//
// STANDALONE: the exe's per-field ExternalPtr serialize (FUN_140018020) is not
// mapped. We reuse the proven generic ExternalPtr serialize reimpl
// (Reimpl_ExternalPtr_Serialize_StringTable in code/Game/Game/serialize_externals.cpp).
// The typeId only drives an IsDerivedFrom validation that
// Object::IsDerivedFromOrTrust makes a no-op in standalone, so the body is
// stream-identical regardless of the (Profession vs StringTable) typeId source.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Generic single-ExternalPtr serialize (exported from serialize_externals.cpp).
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

static const uintptr_t kWorkPlaceDescriptionVtable = 0x140164dc0;

static void WorkPlaceDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kWorkPlaceDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;  // ExternalPtr<Profession> (resolved External*)
    *(uint64_t*)(s + 0x28) = 0;  // ExternalPtr<Profession>
}

// ============================================================
// WorkPlaceDescription_Serialize (vtable +0x28, 0x14012f420)
// ============================================================
void WorkPlaceDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x20, serializer);  // Profession ref
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x28, serializer);  // Profession ref

#if defined(_DEBUG)
    // WAVE 1 diagnostic (env BANISHED_PROBE_WORKPLACE_KEY, same env as the parent's
    // [probe::workplace] probe). Pins the exact failure branch for the null Profession
    // key: if NO [probe::wpdesc-serialize] line appears, Serialize is never called
    // during template load (gap = description-Serialize not dispatched). If a line
    // appears with +0x20=NULL, Serialize ran but FindOrLoad(Profession) failed
    // (innerMode!=0 = not load mode / no path; innerMode==0 = load mode but resource
    // missing). If +0x20 is NON-null here but the parent probe later reads NULL, the
    // live component sees a different/cleared description (copy/clobber). Read-only.
    {
        const char* e = std::getenv("BANISHED_PROBE_WORKPLACE_KEY");
        static int n = 0;
        if (e && e[0] && e[0] != '0' && n < 16) {
            ++n;
            void* p0 = *reinterpret_cast<void**>(s + 0x20);
            void* p1 = *reinterpret_cast<void**>(s + 0x28);
            int innerMode = -999;                        // load mode == 0 (canonical idiom)
            void* enc = *reinterpret_cast<void**>(serializer);
            if (enc) innerMode = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(enc) + 8);
            fprintf(stderr, "[probe::wpdesc-serialize] self=%p afterSerialize "
                            "+0x20=%p +0x28=%p innerMode=%d (0=load)\n",
                    self, p0, p1, innerMode);
            fflush(stderr);
        }
    }
#endif

    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

// ============================================================
// WorkPlaceDescription_Dtor (vtable +0x00, 0x14003a1c0)
// ============================================================
void* WorkPlaceDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kWorkPlaceDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t WorkPlaceDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WorkPlaceDescription");
    return cached;
}

static int   WorkPlaceDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   WorkPlaceDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* WorkPlaceDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* WorkPlaceDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* WorkPlaceDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "WorkPlaceComponent"); }

static void* g_WorkPlaceDescription_Vtable[11] = {
    reinterpret_cast<void*>(&WorkPlaceDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&WorkPlaceDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&WorkPlaceDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&WorkPlaceDescription_GetComponentTypeId),// +0x40 Slot8
    reinterpret_cast<void*>(&WorkPlaceDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterWorkPlaceDescriptionVtable() {
    RegisterExeAddr(kWorkPlaceDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_WorkPlaceDescription_Vtable));
}

extern "C" void* WorkPlaceDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    WorkPlaceDescription_Init(p);
    return p;
}

static void* WorkPlaceDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    WorkPlaceDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(WorkPlaceDescription, "Description",
              &WorkPlaceDescription_Factory, &WorkPlaceDescription_CreateInPlace);

#endif // STANDALONE_EXE
