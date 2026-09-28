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
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include "Component/exe_bodies.h"

namespace {

inline int SerializerMode(void* serializer) {
    // *(int*)(*(Serializer**)serializer + 8): 0 = loading, 1 = saving.
    return static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection());   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
}

inline int EncodingType(void* serializer) {
    void* enc = const_cast<IO::Encoding*>(&reinterpret_cast<IO::Serializer*>(serializer)->GetEncoding());   // B.3.4 bridge
    if (!enc) return 0;
    void** vt = *reinterpret_cast<void***>(enc);           // Encoding vtable
    return reinterpret_cast<int(*)(void*)>(vt[2])(enc);    // +0x10 GetType
}

}  // namespace

// param_1 = &field (External** stored in place), param_2 = IO::Serializer*.
extern "C" void AmbientExternalPtrSerialize_Reimpl(void* fieldPtr, void* serializer) {
    if (!fieldPtr || !serializer) return;
    void** field = reinterpret_cast<void**>(fieldPtr);

    void* factory = ::Object::ExternalFactory::_singleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge

    // Encoding mode 3 — re-resolve from the existing pointer's path (no stream I/O).
    if (EncodingType(serializer) == 3) {
        if (*field == nullptr) return;
        if (factory) {
            System::Filename path =
                reinterpret_cast<const ::Object::External*>(*field)->GetPath();  // B.3.3 bridge
            (void)efactory->FindOrLoad(path);   // touch/refcount (result unused)
        }
        return;
    }

    int mode = SerializerMode(serializer);
    bool loading = (mode == 0);

    System::Filename pathBuf;

    bool refMode = factory &&
                   efactory->IsReferenceModeEnabled();

    bool doLoad = false;

    if (refMode) {
        uint32_t refId = 0xffffffffu;
        if (*field != nullptr)
            refId = efactory->GetReferenceId(
                reinterpret_cast<const ::Object::External*>(*field));  // B.3.3 bridge
        // Serialize the reference id (matches the exe's guarded Serialize(uint)).
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(&refId), nullptr);   // B.3.4 bridge
        if ((int)refId != -1) {
            if (loading) {
                pathBuf = efactory->GetPathForReferenceId(refId);
            }
            doLoad = true;
        }
    } else {
        // Non-reference mode: bool "present" flag, then the path string.
        uint8_t present = (*field != nullptr) ? 1 : 0;
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(&present), nullptr);   // B.3.4 bridge
        if (present) {
            if (*field != nullptr)
                pathBuf = reinterpret_cast<const ::Object::External*>(*field)->GetPath();  // B.3.3 bridge — save: fill path from object
            pathBuf.AsString().Serialize(serializer);        // read/write the path string
            doLoad = true;
        }
    }

    if (doLoad && loading) {
        void* loaded = nullptr;
        if (factory)
            loaded = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(pathBuf)));
        // RELAXED type check (see file header): keep the loaded External.
        *field = loaded;
    }
}

#include "address_registry.h"
// NOTE: the 0x28-stride biome-table array resize (FUN_140002540 /
// AmbientArrayResize_Reimpl) is already registered in reg_component_visuals.cpp.
extern "C" void RegisterAmbientExternalPtrSerialize() {
    RegisterExeAddr(0x1400019a0, reinterpret_cast<uintptr_t>(&AmbientExternalPtrSerialize_Reimpl));
}
