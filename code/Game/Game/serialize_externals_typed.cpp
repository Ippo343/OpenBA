// code/Game/Game/serialize_externals_typed.cpp
//
// M3.5 Path B session 3 — additional typed-ExternalPtr serialize helper.
//
// Helper:
//   FUN_140007140 — typed ExternalPtr serialize (CASCADE, ~120 instr)
//
// Body is byte-identical to FUN_14001b8e0 (helper #9, GraphicsMesh — see
// code/Game/Game/serialize_externals.cpp) except for the typeInfo source
// slot. FUN_140007140 reads BaseTypeInfo* from .rdata slot 0x140151350,
// which is Graphics::MaterialInstance (confirmed via shared xrefs:
// CharacterComponent_impl_08c0 FindOrLoads "Terrain/Selection.rsc" and
// then calls Graphics::MaterialInstance::Setup on the result, validating
// against the same slot).
//
// TODO: factor with code/Game/Game/serialize_externals.cpp once that
// file's `ExternalPtrSerializeGeneric` is exposed (it's currently in an
// anonymous namespace, so we duplicate the body here under a new name).

#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>

namespace {

// Shared body — duplicate of ExternalPtrSerializeGeneric in
// serialize_externals.cpp. See that file for the per-line provenance
// against FUN_14001b8e0. Renamed here to avoid an ODR clash.
void ExternalPtrSerializeGeneric_7140(void** refField, void* serializer,
                                      const uint32_t* typeIdPtr,
                                      const char* typeName) {
    if (!refField || !serializer) return;

    if (!fn_Serializer_GetEncoding ||
        !fn_IsDerivedFrom || !fn_Serializer_Serialize_Uint || !g_fn_Serialize_Bool ||
        !g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[ExternalPtr::Serialize_7140<%s>] runtime primitives unwired — "
                   "skipping serialize (cascade-safe no-op)\n",
                   typeName ? typeName : "?");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    void* factory = *g_pExternalFactorySingleton;
    // ENDGAME B.3.3 step 6A: member calls on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guards
    // above still gate the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

    uint32_t typeId = 0;
    bool typeIdResolved = false;
    if (typeIdPtr) {
        typeId = *typeIdPtr;
        typeIdResolved = (typeId != 0);
    }
    // Lazy resolution path — see serialize_externals.cpp for rationale.
    if (!typeIdResolved && typeName) {
        const char* simple = typeName;
        for (const char* p = typeName; *p; ++p) {
            if (p[0] == ':' && p[1] == ':') simple = p + 2;
        }
        typeId = ::Object::ResolveTypeIdByNameLazy(simple);
        typeIdResolved = (typeId != 0);
    }
    if (!typeIdResolved) {
        static bool s_typeWarned = false;
        if (!s_typeWarned) {
            printf("[ExternalPtr::Serialize_7140<%s>] typeId not yet registered — "
                   "loading without type validation\n",
                   typeName ? typeName : "?");
            fflush(stdout);
            s_typeWarned = true;
        }
    }

    void* encoding = fn_Serializer_GetEncoding(serializer);
    if (!encoding) return;
    using EncodingStateFn = int (*)(void*);
    EncodingStateFn getStateFn = reinterpret_cast<EncodingStateFn>(
        *reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(encoding)) + 0x10));
    int encState = getStateFn ? getStateFn(encoding) : 0;

    if (encState == 3) {
        // Path-export (read-only) mode.
        if (!*refField) return;
        // `GetPath` returns by value through a hidden return slot and the callee
        // CONSTRUCTS into it; copy-initialising the local is the same store.
        System::Filename pathBuf =
            reinterpret_cast<const ::Object::External*>(*refField)->GetPath();  // B.3.3 bridge
        const void* found = efactory->FindOrLoad(pathBuf);
        if (found && typeIdResolved) {
            ::Object::IsDerivedFromOrTrust(const_cast<void*>(found), typeId);
        }
        return;
    }

    System::Filename fnameBuf;

    bool refMode = efactory->IsReferenceModeEnabled();

    if (refMode) {
        uint32_t refId = 0xFFFFFFFFu;
        if (*refField) {
            refId = efactory->GetReferenceId(
                reinterpret_cast<const ::Object::External*>(*refField));   // B.3.3 bridge
        }

        int serializerInner = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);

        bool needsLoad = false;
        if (!*refField || serializerInner != 0 || refId != 0xFFFFFFFFu) {
            fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
            if (refId != 0xFFFFFFFFu) {
                if (serializerInner == 0) {
                    System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                    fnameBuf = tmpFn;
                }
                needsLoad = true;
            }
        } else {
            fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
        }

        if (needsLoad) {
            int innerCheck = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
            if (innerCheck == 0) {
                void* loaded = const_cast<::Object::External*>(
                    efactory->FindOrLoad(fnameBuf));                        // B.3.3 bridge
                void* result = nullptr;
                if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                    result = loaded;
                }
                *refField = result;
            }
        }
    } else {
        bool hasRef = (*refField != nullptr);
        g_fn_Serialize_Bool(serializer, &hasRef, nullptr);
        if (hasRef) {
            if (*refField) {
                System::Filename pathBuf =
                    reinterpret_cast<const ::Object::External*>(*refField)->GetPath();  // B.3.3 bridge
                fnameBuf = pathBuf;
            }
            fnameBuf.AsString().Serialize(serializer);

            int innerCheck = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
            if (innerCheck == 0) {
                void* loaded = const_cast<::Object::External*>(
                    efactory->FindOrLoad(fnameBuf));                        // B.3.3 bridge
                void* result = nullptr;
                if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                    result = loaded;
                }
                *refField = result;
            }
        }
    }

}

} // namespace

// ============================================================
// FUN_140007140 — typed ExternalPtr serialize for Graphics::MaterialInstance
// (typeInfo slot 0x140151350)
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_Typed_7140(
    void* refField, void* serializer) {

    // 0x140007140 resolves back to ourselves via the address registry, so we
    // always run the cleaned body here.

    const uint32_t* typeIdPtr = nullptr;
    if (g_ppTypeInfo_MaterialInstance && *g_ppTypeInfo_MaterialInstance) {
        typeIdPtr = *g_ppTypeInfo_MaterialInstance;
    }
    ExternalPtrSerializeGeneric_7140(reinterpret_cast<void**>(refField),
                                     serializer, typeIdPtr,
                                     "Graphics::MaterialInstance");
}

// ============================================================
// Scenario's typed ExternalPtr serializers (menu-loader factory cascade).
// ============================================================
// FUN_140019ec0 (MapGenerator), FUN_14001a0c0 (TerrainGenerator),
// FUN_1400194a0 (ClimateGenerator) — same body as 7140, differing only by the
// hardcoded TypeInfo_<Gen> the original uses for IsDerivedFrom. Registered at
// their exe addresses (address_registry.cpp) so Scenario_Serialize resolves
// them; without these the three ExtPtr fields (Scenario +0x70/+0x78/+0x80)
// aren't read and the loader stream misaligns -> downstream garbage typeId ->
// crash. The typeId is resolved lazily by name (the generators are registered
// elsewhere), so no .rdata typeInfo slot is needed here.
extern "C" void Reimpl_ExternalPtr_Serialize_MapGenerator(void* refField, void* serializer) {
    ExternalPtrSerializeGeneric_7140(reinterpret_cast<void**>(refField),
                                     serializer, nullptr, "MapGenerator");
}
extern "C" void Reimpl_ExternalPtr_Serialize_TerrainGenerator(void* refField, void* serializer) {
    ExternalPtrSerializeGeneric_7140(reinterpret_cast<void**>(refField),
                                     serializer, nullptr, "TerrainGenerator");
}
extern "C" void Reimpl_ExternalPtr_Serialize_ClimateGenerator(void* refField, void* serializer) {
    ExternalPtrSerializeGeneric_7140(reinterpret_cast<void**>(refField),
                                     serializer, nullptr, "ClimateGenerator");
}
