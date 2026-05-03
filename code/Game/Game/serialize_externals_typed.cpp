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
#include "resolve.h"
#include "Object/factory_registration.h"
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

    if (!fn_Serializer_GetEncoding || !fn_ExternalFactory_FindOrLoad ||
        !fn_IsDerivedFrom || !fn_ExternalFactory_IsReferenceModeEnabled ||
        !fn_External_GetPath || !fn_Filename_Ctor || !fn_Filename_Dtor ||
        !fn_Filename_AssignOp || !g_fn_String_Serialize ||
        !fn_Serializer_Serialize_Uint || !g_fn_Serialize_Bool ||
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
        uint8_t pathBuf[8] = {0};
        void* path = fn_External_GetPath(*refField, pathBuf);
        void* found = fn_ExternalFactory_FindOrLoad(factory, path);
        if (found && typeIdResolved) {
            ::Object::IsDerivedFromOrTrust(found, typeId);
        }
        fn_Filename_Dtor(pathBuf);
        return;
    }

    uint8_t fnameBuf[8] = {0};
    fn_Filename_Ctor(fnameBuf);

    bool refMode = fn_ExternalFactory_IsReferenceModeEnabled(factory);

    if (refMode) {
        uint32_t refId = 0xFFFFFFFFu;
        if (*refField) {
            refId = fn_ExternalFactory_GetReferenceId(factory, *refField);
        }

        int serializerInner = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);

        bool needsLoad = false;
        if (!*refField || serializerInner != 0 || refId != 0xFFFFFFFFu) {
            fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
            if (refId != 0xFFFFFFFFu) {
                if (serializerInner == 0) {
                    uint8_t tmpFn[8] = {0};
                    fn_ExternalFactory_GetPathForReferenceId(factory, tmpFn, refId);
                    fn_Filename_AssignOp(fnameBuf, tmpFn);
                    fn_Filename_Dtor(tmpFn);
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
                void* loaded = fn_ExternalFactory_FindOrLoad(factory, fnameBuf);
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
                uint8_t pathBuf[8] = {0};
                void* path = fn_External_GetPath(*refField, pathBuf);
                fn_Filename_AssignOp(fnameBuf, path);
                fn_Filename_Dtor(pathBuf);
            }
            g_fn_String_Serialize(fnameBuf, serializer);

            int innerCheck = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
            if (innerCheck == 0) {
                void* loaded = fn_ExternalFactory_FindOrLoad(factory, fnameBuf);
                void* result = nullptr;
                if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                    result = loaded;
                }
                *refField = result;
            }
        }
    }

    fn_Filename_Dtor(fnameBuf);
}

} // namespace

// ============================================================
// FUN_140007140 — typed ExternalPtr serialize for Graphics::MaterialInstance
// (typeInfo slot 0x140151350)
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_Typed_7140(
    void* refField, void* serializer) {

    // Prefer trampoline to original when available (DLL mode). In standalone,
    // fn_ExternalPtr_Serialize_Typed_7140 resolves back to ourselves via the
    // address registry, so we always run the cleaned body here.
#ifndef STANDALONE_EXE
    if (fn_ExternalPtr_Serialize_Typed_7140) {
        fn_ExternalPtr_Serialize_Typed_7140(refField, serializer);
        return;
    }
#endif

    const uint32_t* typeIdPtr = nullptr;
    if (g_ppTypeInfo_MaterialInstance && *g_ppTypeInfo_MaterialInstance) {
        typeIdPtr = *g_ppTypeInfo_MaterialInstance;
    }
    ExternalPtrSerializeGeneric_7140(reinterpret_cast<void**>(refField),
                                     serializer, typeIdPtr,
                                     "Graphics::MaterialInstance");
}
