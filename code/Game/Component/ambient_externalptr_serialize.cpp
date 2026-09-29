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

#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "audio/SoundEffect.h"
#include <cstdint>
#include "Component/exe_bodies.h"

// field = &ExternalPtr<SoundEffect> (the pointer stored in place), serializer = the
// caller's IO::Serializer.
extern "C" void AmbientExternalPtrSerialize_Reimpl(Audio::SoundEffect** field, IO::Serializer* serializer) {
    if (!field || !serializer) return;

    ::Object::ExternalFactory* efactory = ::Object::ExternalFactory::_singleton;

    // Encoding mode 3 — re-resolve from the existing pointer's path (no stream I/O).
    if (serializer->GetEncoding().GetKind() == 3) {   // Encoding slot 2 (+0x10)
        if (*field == nullptr) return;
        if (efactory) {
            System::Filename path = (*field)->GetPath();
            (void)efactory->FindOrLoad(path);   // touch/refcount (result unused)
        }
        return;
    }

    // IO::Serializer::GetDirection, retail 0x180006210: 0 = loading, 1 = saving.
    const bool loading = static_cast<int>(serializer->GetDirection()) == 0;

    System::Filename pathBuf;

    bool refMode = efactory && efactory->IsReferenceModeEnabled();

    bool doLoad = false;

    if (refMode) {
        uint32_t refId = 0xffffffffu;
        if (*field != nullptr)
            refId = efactory->GetReferenceId(*field);
        // Serialize the reference id (matches the exe's guarded Serialize(uint)).
        serializer->Serialize(refId, nullptr);
        if ((int)refId != -1) {
            if (loading) {
                pathBuf = efactory->GetPathForReferenceId(refId);
            }
            doLoad = true;
        }
    } else {
        // Non-reference mode: bool "present" flag, then the path string.
        bool present = (*field != nullptr);
        serializer->Serialize(present, nullptr);
        if (present) {
            if (*field != nullptr)
                pathBuf = (*field)->GetPath();                // save: fill path from object
            pathBuf.AsString().Serialize(serializer);        // read/write the path string
            doLoad = true;
        }
    }

    if (doLoad && loading) {
        const ::Object::External* loaded = nullptr;
        if (efactory)
            loaded = efactory->FindOrLoad(pathBuf);
        // RELAXED type check (see file header): keep the loaded External.
        *field = static_cast<Audio::SoundEffect*>(const_cast<::Object::External*>(loaded));
    }
}
