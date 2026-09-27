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
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "Component/EnvironmentDescription.h"   // caller batch CB6: TypeId()
#include "Game/Toolbar.h"   // Toolbar::TypeId() (ENDGAME B.3.9 TR)
#include "ui/StringTable.h"   // UI::StringTable::TypeId() (D5, ENDGAME B.3.9 CG3)
#include "Game/serialize_externals.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A — Object::External / ExternalFactory
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstdio>

// Shared body — mirrors FUN_14001b8e0 / FUN_1400196a0 line by line.
// Exposed (external linkage) via serialize_externals.h so the Tutorial
// ExternalPtr-array serialize helpers can reuse this exact body.
//
// Cascade safety: if typeIdPtr is null OR *typeIdPtr is 0 (Runtime DLL hasn't
// registered the underlying type yet), we still call FindOrLoad but skip the
// IsDerivedFrom check. The original would deref a NULL BaseTypeInfo and crash;
// our reimpl gracefully falls through with a one-shot warning.
void ExternalPtrSerializeGeneric(void** refField, void* serializer,
                                 const uint32_t* typeIdPtr,
                                 const char* typeName) {
    if (!refField || !serializer) return;

    if (!fn_IsDerivedFrom ||
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
    // ENDGAME B.3.3 step 6A: the resolved-pointer calls below are now member
    // calls on the real `Object::ExternalFactory` / `Object::External`. The
    // singleton is still read through the EXISTING `g_pExternalFactorySingleton`
    // door (not `ExternalFactory::Get()`), so the guard above still gates the
    // same cell and nothing about WHICH body runs changes: the class members
    // forward to the same registered `RtObject_ExternalFactory_*` entry points
    // these pointers resolve to.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

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

    IO::Encoding* encoding = const_cast<IO::Encoding*>(
        &reinterpret_cast<IO::Serializer*>(serializer)->GetEncoding());   // B.3.4 bridge
    if (!encoding) return;
    // Encoding vtable +0x10 is slot 2 == IO::Encoding::GetKind() (B34_RULES §1.1).
    int encState = encoding->GetKind();   // B.3.4 bridge  (+0x10 = Encoding::GetKind)

    if (encState == 3) {
        // Path-export (read-only) mode.
        if (!*refField) return;
        // `GetPath` returns by value through a hidden return slot and the callee
        // CONSTRUCTS into it; copy-initialising the local is the same store
        // (reference_hidden_retslot_wild_write.md).
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
            // *** RETAIL DEREFERENCES THE FOUND NODE WITHOUT A NULL CHECK *** —
            // an External the factory never Add()ed faults reading +0x14, in
            // retail and here. No guard is added (CLAUDE.md hard rule).
            refId = efactory->GetReferenceId(
                reinterpret_cast<const ::Object::External*>(*refField));   // B.3.3 bridge
        }

        int serializerInner = *reinterpret_cast<int*>(
            reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);

        bool needsLoad = false;
        if (!*refField || serializerInner != 0 || refId != 0xFFFFFFFFu) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
            if (refId != 0xFFFFFFFFu) {
                if (serializerInner == 0) {
                    System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                    fnameBuf = tmpFn;
                }
                needsLoad = true;
            }
        } else {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
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
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(hasRef, nullptr);   // B.3.4 bridge
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

// ============================================================
// Helper #9 — ExternalPtr serialize for GraphicsMesh (FUN_14001b8e0)
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_GraphicsMesh(
    void* refField, void* serializer) {


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
    // Read directly (D5, ENDGAME B.3.9 CG3): UI::StringTable::TypeId() is the same
    // Runtime record engine_funcs' g_TypeId_StringTable (exref 0x140151488) points
    // at. The generic body still handles an unresolved (0) id gracefully.
    const uint32_t tid = UI::StringTable::TypeId();
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(refField), serializer,
                                tid ? &tid : nullptr, "StringTable");
}

// ============================================================
// 0x140024c70 — Object::ExternalPtr<Toolbar>::Serialize. Byte-identical to the
// generic ExternalPtr<T>::Serialize; validation type = Toolbar: retail reads
// `MOV EDX,dword ptr [0x1401c6128]` (Toolbar's record) at 0x140024cd5 before the
// IsDerivedFrom call at 0x140024cde (disassembly, Application-x64.exe). Used by
// Toolbar_Serialize for the +0x60 field.
//
// ENDGAME B.3.9 TR (behaviour): this read a retired engine_funcs type-id pointer
// ("ToolbarExternal") bound to 0x14015f980 -- which is NOT a type-info import
// slot but slot 2 of the vtable at 0x14015f970 (it holds code, 0x140021310), never
// registered, so the pointer was always NULL and the generic body fell back to a
// by-name lookup of "Toolbar". Now the class's own id, as retail.
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_Toolbar(
    void* refField, void* serializer) {
    const uint32_t tid = Toolbar::TypeId();
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(refField), serializer,
                                tid ? &tid : nullptr, "Toolbar");
}

// ============================================================
// 0x1400192a0 — Object::ExternalPtr<EnvironmentDescription>::Serialize.
// Byte-identical to the generic ExternalPtr<T>::Serialize; validation type =
// EnvironmentDescription (DAT_1401c5980, written by
// EnvironmentDescription_RegisterType). Used by the weather-sim serializer
// (0x140093c90, save/load wave 2026-08-27) for its +0x00 environment resource.
// ============================================================
extern "C" void Reimpl_ExternalPtr_Serialize_EnvironmentDesc(
    void* refField, void* serializer) {
    // Caller batch CB6: the class's TypeId() (was a lazy by-name cache: this
    // runs on the loader thread, so the function-local latch was a race, B6-5).
    const uint32_t tid = EnvironmentDescription::TypeId();
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(refField), serializer,
                                tid ? &tid : nullptr, "EnvironmentDescription");
}
