// code/Game/Component/componentdescription.cpp
// Foundation C keystone — ComponentDescription (the entity-template container).
//
// ComponentDescription (0x38 bytes) extends Object::External. It is the top-level
// type of every entity Template (Template\Citizen.rsc etc.). Layout:
//   +0x00 vtable (0x140162a00)
//   +0x08 Object::External base
//   +0x20 int   category (default 6)
//   +0x28 void* subDescArray  — array of sub-description External* (stride 8)
//   +0x30 int   subDescCount
//   +0x34 int   subDescCap
//
// Loading a template: Factory creates a ComponentDescription, Serialize reads the
// category + sub-description count, resizes the array, then loads each sub-
// description as its own External resource via ExternalFactory::FindOrLoad
// (FUN_140032090). Each sub-description (CitizenDescription, ModelDescription, …)
// has its own factory/Serialize — see the other Foundation C files.
//
// Reimplemented for standalone (the original 0x14014ea90 / 0x140118af0 / 0x140032090
// aren't mapped). Mirrors mapgenerator.cpp (deserialize) + statgoal.cpp (scaffolding)
// + serialize_struct60b_serialize.cpp (the proven External-array FindOrLoad body).

#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "resolve.h"
#include <cstdint>
#include <cstdio>

static constexpr uintptr_t kVtableAddr = 0x140162a00;
static constexpr int       kSize       = 0x38;
static constexpr int       OFF_CATEGORY = 0x20;
static constexpr int       OFF_ARRAY    = 0x28;
static constexpr int       OFF_COUNT    = 0x30;
static constexpr int       OFF_CAP      = 0x34;

// ---- internal: serializer "stream error" word (serializer->stream + 8 == 0 ⇒ ok)
static inline int SerStreamErr(void* serializer) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
}

// ============================================================
// Sub-description array deserialize (FUN_140032090)
// ============================================================
// Per element: load the referenced sub-description External via FindOrLoad
// (reference-mode or path-string mode, plus the encoding==3 export walk). This is
// the proven body from serialize_struct60b_serialize.cpp, minus type validation
// (sub-descriptions self-identify their type in the package; we trust it).
// Exported so other description types with External-ref arrays (AnimalDescription,
// etc.) reuse this exact proven loop.
extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase) {
    if (!serializer || count <= 0 || !arrayBase) return;
    if (!fn_Serializer_GetEncoding ||
        !fn_Serializer_Serialize_Uint || !g_fn_Serialize_Bool ||
        !g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        static bool warned = false;
        if (!warned) { fprintf(stderr, "[ComponentDescription] sub-array primitives unwired — skip %d\n", count); fflush(stderr); warned = true; }
        return;
    }
    void* factory = *g_pExternalFactorySingleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge

    void* encoding = fn_Serializer_GetEncoding(serializer);
    if (!encoding) return;
    using EncFn = int (*)(void*);
    EncFn getState = reinterpret_cast<EncFn>(
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(encoding)) + 0x10));
    int encState = getState ? getState(encoding) : 0;

    void** slots = reinterpret_cast<void**>(arrayBase);
    for (int i = 0; i < count; ++i) {
        void** slot = &slots[i];

        if (encState == 3) {                       // path-export walk (read-only)
            if (!*slot) continue;
            System::Filename pathBuf =
                reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
            (void)efactory->FindOrLoad(pathBuf);
            continue;
        }

        System::Filename fnameBuf;
        bool refMode = efactory->IsReferenceModeEnabled();

        if (refMode) {
            uint32_t refId = 0xFFFFFFFFu;
            if (*slot) refId = efactory->GetReferenceId(
                reinterpret_cast<const ::Object::External*>(*slot));  // B.3.3 bridge
            bool needsPath = false;
            if (!*slot || SerStreamErr(serializer) != 0 || refId != 0xFFFFFFFFu) {
                fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
                if (refId != 0xFFFFFFFFu) {
                    if (SerStreamErr(serializer) == 0) {
                        System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                        fnameBuf = tmpFn;
                    }
                    needsPath = true;
                }
            } else {
                fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
            }
            if (needsPath && SerStreamErr(serializer) == 0) {
                *slot = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fnameBuf)));
            }
        } else {
            bool hasRef = (*slot != nullptr);
            g_fn_Serialize_Bool(serializer, &hasRef, nullptr);
            if (hasRef) {
                if (*slot) {
                    fnameBuf = reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
                }
                fnameBuf.AsString().Serialize(serializer);
                if (SerStreamErr(serializer) == 0) {
                    *slot = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fnameBuf)));
                }
            }
        }
    }
}

// ============================================================
// Ctor / Serialize / Dtor
// ============================================================
extern "C" void* ComponentDescription_CtorImpl(void* mem) {
    if (!mem) return nullptr;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    if (fn_External_External) fn_External_External(mem);     // Object::External base
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    *(int32_t*)(s + OFF_CATEGORY) = 6;                       // default category
    *(void**)(s + OFF_ARRAY)      = nullptr;
    *(int32_t*)(s + OFF_COUNT)    = 0;
    *(int32_t*)(s + OFF_CAP)      = 0;
    return mem;
}

extern "C" void ComponentDescription_Serialize(void* self, void* stream) {
    if (!self) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t serBuf[24];
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int) return;
    g_fn_Serializer_Ctor(serBuf, stream, 0);

    // category enum (+0x20) via serializer vtable+0x98 (enum serialize)
    void* serObj = *(void**)(serBuf + 8);
    if (serObj) {
        void* vt = *(void**)serObj;
        typedef void (*fnEnum_t)(void*, void*, const char*);
        fnEnum_t serEnum = *(fnEnum_t*)((uint8_t*)vt + 0x98);
        if (serEnum) serEnum(serObj, s + OFF_CATEGORY, nullptr);
    }

    // sub-description count (+0x30) + fresh array resize (+0x28, stride 8)
    g_fn_Serialize_Int(serBuf, s + OFF_COUNT, nullptr);
    int count = *(int32_t*)(s + OFF_COUNT);
    if (SerStreamErr(serBuf) == 0 && count > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * 8, 8);
        *(void**)(s + OFF_ARRAY) = buf;
        *(int32_t*)(s + OFF_CAP) = count;
        if (buf) for (int i = 0; i < count; ++i) ((void**)buf)[i] = nullptr;
    }

    // load each sub-description (FUN_140032090)
    Desc_LoadExternalArray(serBuf, count, *(void**)(s + OFF_ARRAY));

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serBuf);
}

extern "C" void* ComponentDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtableAddr);
    // free the sub-description pointer array (the sub-descriptions themselves are
    // owned by the ExternalFactory cache, not freed here)
    void* arr = *(void**)(s + OFF_ARRAY);
    if (arr && fn_MemFreeStatic) fn_MemFreeStatic(arr);
    *(void**)(s + OFF_ARRAY) = nullptr;
    *(int32_t*)(s + OFF_COUNT) = 0;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// Standalone factory scaffolding
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ComponentDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ComponentDescription");
    return cached;
}
static int   CD_VtblStub_Return2(void* /*self*/) { return 2; }  // GetVersion = 2
static int   CD_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CD_VtblStub_ReturnNull(void*, ...)  { return nullptr; }
extern "C" void* ComponentDescription_CreateInPlace(void* mem);

// vtable @ 0x140162a00. Every slot filled with a safe stub (the Runtime
// External::Load calls GetVersion at +0x18 before Serialize — a null slot there
// faults). GetVersion=2 (verified: exe body mov eax,2; ret).
static void* g_CD_Vtable[11] = {
    reinterpret_cast<void*>(&ComponentDescription_Dtor),     // +0x00 dtor
    reinterpret_cast<void*>(&CD_VtblStub_ReturnNull),        // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ComponentDescription_GetTypeId),// +0x10 GetTypeId
    reinterpret_cast<void*>(&CD_VtblStub_Return2),           // +0x18 GetVersion=2
    reinterpret_cast<void*>(&CD_VtblStub_Return0),           // +0x20 Compile thunk
    reinterpret_cast<void*>(&ComponentDescription_Serialize),// +0x28 Serialize
    reinterpret_cast<void*>(&CD_VtblStub_Return0),           // +0x30 Cleanup
    reinterpret_cast<void*>(&CD_VtblStub_Return0),           // +0x38 GetDesc forwarder
    reinterpret_cast<void*>(&CD_VtblStub_ReturnNull),        // +0x40
    reinterpret_cast<void*>(&ComponentDescription_CreateInPlace), // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CD_VtblStub_ReturnNull),        // +0x50 guard
};

extern "C" void RegisterComponentDescriptionVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_CD_Vtable));
}

extern "C" void* ComponentDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, kSize, 8);
    return p ? ComponentDescription_CtorImpl(p) : nullptr;
}
extern "C" void* ComponentDescription_CreateInPlace(void* mem) {
    return mem ? ComponentDescription_CtorImpl(mem) : nullptr;
}

REGISTER_TYPE(ComponentDescription, "Object::External",
              &ComponentDescription_Factory, &ComponentDescription_CreateInPlace);
