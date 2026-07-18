// code/Game/Component/ambient_externalptr_serialize.cpp
// Standalone reimpl of FUN_1400019a0 — Object::ExternalPtr<SoundEffect>::Serialize.
//
// Resolves one 8-byte ExternalPtr<External> field in place (param_1 = &field).
// It is byte-for-byte identical to Object_ExternalPtr_BuildComponentRef_Serialize
// (fn_ExternalPtr_BuildSerialize, 0x140002340) EXCEPT the type global it validates
// the loaded object against: FUN_1400019a0 validates SoundEffect (TypeInfo ext-ref
// at dispatch slot 0x140152a80), BuildSerialize validates BuildComponent
// (DWORD_1401c7318). Substituting BuildSerialize therefore loads the sound then
// NULLs it (wrong-type check) — which is exactly why the ambient biome day/night
// sound refs came back 0 in standalone (the description per-element serialize loops
// were wired to the BuildSerialize substitute / left unresolved).
//
// This reimpl performs the identical stream I/O (three modes: encoding==3 re-resolve,
// reference mode, path-string mode) using the resolved engine helpers, but RELAXES
// the final type check: it keeps the FindOrLoad'd External regardless of type. The
// standalone factory graph does not register the runtime "SoundEffect" TypeInfo, so
// a strict check would still null a genuine sound; every field this function serves
// is an audio/external ExternalPtr, so keeping the loaded object is correct. This
// matches the retail net effect (a valid SoundEffect is kept).
//
// Registered at exe 0x1400019a0 so every caller (AmbientEnvironmentDescription /
// AmbientEmitterComponent / AnimAudioEvent / MenuScene serialize) resolves it.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {

inline int SerializerMode(void* serializer) {
    // *(int*)(*(Serializer**)serializer + 8): 0 = loading, 1 = saving.
    return *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(serializer) + 8);
}

inline int EncodingType(void* serializer) {
    if (!fn_Serializer_GetEncoding) return 0;
    void* enc = fn_Serializer_GetEncoding(serializer);
    if (!enc) return 0;
    void** vt = *reinterpret_cast<void***>(enc);           // Encoding vtable
    return reinterpret_cast<int(*)(void*)>(vt[2])(enc);    // +0x10 GetType
}

}  // namespace

// param_1 = &field (External** stored in place), param_2 = IO::Serializer*.
extern "C" void AmbientExternalPtrSerialize_Reimpl(void* fieldPtr, void* serializer) {
    if (!fieldPtr || !serializer) return;
    void** field = reinterpret_cast<void**>(fieldPtr);

    void* factory = (g_pExternalFactorySingleton && *g_pExternalFactorySingleton)
        ? *g_pExternalFactorySingleton : nullptr;

    // Encoding mode 3 — re-resolve from the existing pointer's path (no stream I/O).
    if (EncodingType(serializer) == 3) {
        if (*field == nullptr) return;
        if (fn_External_GetPath && fn_ExternalFactory_FindOrLoad && factory) {
            uint8_t path[8] = {0};
            fn_External_GetPath(*field, path);
            fn_ExternalFactory_FindOrLoad(factory, path);   // touch/refcount (result unused)
            if (fn_Filename_Dtor) fn_Filename_Dtor(path);
        }
        return;
    }

    int mode = SerializerMode(serializer);
    bool loading = (mode == 0);

    uint8_t pathBuf[8] = {0};                               // System::Filename
    if (fn_Filename_Ctor) fn_Filename_Ctor(pathBuf);

    bool refMode = fn_ExternalFactory_IsReferenceModeEnabled && factory &&
                   fn_ExternalFactory_IsReferenceModeEnabled(factory);

    bool doLoad = false;

    if (refMode) {
        uint32_t refId = 0xffffffffu;
        if (*field != nullptr && fn_ExternalFactory_GetReferenceId)
            refId = fn_ExternalFactory_GetReferenceId(factory, *field);
        // Serialize the reference id (matches the exe's guarded Serialize(uint)).
        if (fn_Serializer_Serialize_Uint)
            fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
        if ((int)refId != -1) {
            if (loading && fn_ExternalFactory_GetPathForReferenceId) {
                fn_ExternalFactory_GetPathForReferenceId(factory, pathBuf, refId);
            }
            doLoad = true;
        }
    } else {
        // Non-reference mode: bool "present" flag, then the path string.
        uint8_t present = (*field != nullptr) ? 1 : 0;
        if (g_fn_Serialize_Bool)
            g_fn_Serialize_Bool(serializer, &present, nullptr);
        if (present) {
            if (*field != nullptr && fn_External_GetPath)
                fn_External_GetPath(*field, pathBuf);        // save: fill path from object
            if (g_fn_String_Serialize)
                g_fn_String_Serialize(pathBuf, serializer);  // read/write the path string
            doLoad = true;
        }
    }

    if (doLoad && loading) {
        void* loaded = nullptr;
        if (fn_ExternalFactory_FindOrLoad && factory)
            loaded = fn_ExternalFactory_FindOrLoad(factory, pathBuf);
        // RELAXED type check (see file header): keep the loaded External.
        *field = loaded;
    }

    if (fn_Filename_Dtor) fn_Filename_Dtor(pathBuf);
}

#include "address_registry.h"
// NOTE: the 0x28-stride biome-table array resize (FUN_140002540 /
// AmbientArrayResize_Reimpl) is already registered in reg_component_visuals.cpp.
extern "C" void RegisterAmbientExternalPtrSerialize() {
    RegisterExeAddr(0x1400019a0, reinterpret_cast<uintptr_t>(&AmbientExternalPtrSerialize_Reimpl));
}
