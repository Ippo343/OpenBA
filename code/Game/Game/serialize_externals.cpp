// code/Game/Game/serialize_externals.cpp
// Single ExternalPtr serialize helpers used by MenuScene::Slot5.
//
// Helpers (M3.5 / notes/M35_recon.md):
//   #9  ExternalPtr serialize (+0x70 GraphicsMesh) — exe 0x14001b8e0  (CASCADE)
//   #10 ExternalPtr serialize (+0x80 Weather)      — exe 0x1400196a0  (CASCADE)
//
// Bodies are byte-identical except for the typeId source:
//   #9  reads BaseTypeInfo* from the .rdata slot at 0x140151f20 (GraphicsMesh,
//       same slot as helper #7), then dereferences *(uint*)BaseTypeInfo for typeId.
//   #10 reads typeId directly from a dword at 0x1401c6198 (Weather — exe-resident
//       BaseTypeInfo whose typeId field is patched in by Weather_RegisterType
//       at runtime).
//
// We extract the shared body into ExternalPtrSerializeGeneric, parameterised
// by the typeIdPtr.

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>

namespace {

// Shared body — mirrors FUN_14001b8e0 / FUN_1400196a0 line by line.
//
// Cascade safety: if typeIdPtr is null OR *typeIdPtr is 0 (Runtime DLL hasn't
// registered the underlying type yet), we still call FindOrLoad but skip the
// IsDerivedFrom check. The original would deref a NULL BaseTypeInfo and crash;
// our reimpl gracefully falls through with a one-shot warning.
void ExternalPtrSerializeGeneric(void** refField, void* serializer,
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
            printf("[ExternalPtr::Serialize<%s>] runtime primitives unwired — "
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
    // Lazy resolution path: in standalone mode the .rdata BaseTypeInfo* slot
    // is uninitialized, so typeIdPtr stays null. Ask Object::Factory directly
    // by simple class name (strip leading "Graphics::" / etc.).
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
            printf("[ExternalPtr::Serialize<%s>] typeId not yet registered — "
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
// Helper #9 — ExternalPtr serialize for GraphicsMesh (FUN_14001b8e0)
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_GraphicsMesh(
    void* refField, void* serializer) {

#ifndef STANDALONE_EXE
    if (fn_ExternalPtr_Serialize_GraphicsMesh) {
        fn_ExternalPtr_Serialize_GraphicsMesh(refField, serializer);
        return;
    }
#endif

    const uint32_t* typeIdPtr = nullptr;
    if (g_ppTypeInfo_GraphicsMesh && *g_ppTypeInfo_GraphicsMesh) {
        typeIdPtr = *g_ppTypeInfo_GraphicsMesh;
    }
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(refField), serializer,
                                typeIdPtr, "Graphics::GraphicsMesh");
}

// ============================================================
// Helper #10 — ExternalPtr serialize for Weather (FUN_1400196a0)
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_Weather(
    void* refField, void* serializer) {

#ifndef STANDALONE_EXE
    if (fn_ExternalPtr_Serialize_Weather) {
        fn_ExternalPtr_Serialize_Weather(refField, serializer);
        return;
    }
#endif

    // Weather typeId is a direct dword (no double-deref).
    const uint32_t* typeIdPtr = g_pTypeId_Weather;
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(refField), serializer,
                                typeIdPtr, "Weather");
}

// ============================================================
// 0x14000bf40 — Object::ExternalPtr<StringTable>::Serialize
//   (project name fn_ExternalPtr_Serialize2). Byte-identical to
//   FUN_14001b8e0 / FUN_140007140; only the type-validation slot differs
//   (0x140151488 = g_TypeId_StringTable). Every generator's +0x20 ExternalPtr
//   field is serialized through this. In standalone fn_ExternalPtr_Serialize2
//   was NULL (0x14000bf40 unregistered) so the generators SKIPPED their +0x20
//   read, mis-advancing the stream cursor for every subsequent field. Wiring
//   this keeps each generator's element-deserialize byte-aligned.
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(
    void* refField, void* serializer) {
#ifndef STANDALONE_EXE
    if (fn_ExternalPtr_Serialize2) {           // trampoline in DLL mode
        fn_ExternalPtr_Serialize2(refField, serializer);
        return;
    }
#endif
    // g_TypeId_StringTable is wired in engine_funcs.cpp (exref 0x140151488).
    // May be null in standalone until StringTable registers — the generic body
    // handles that (loads without type validation rather than crashing).
    const uint32_t* typeIdPtr = g_TypeId_StringTable;
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(refField), serializer,
                                typeIdPtr, "StringTable");
}
