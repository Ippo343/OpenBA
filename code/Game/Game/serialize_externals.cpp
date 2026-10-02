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

#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "system/Filename.h"
#include "system/String.h"
#include "Object/factory_registration.h"
#include "Component/EnvironmentDescription.h"   // caller batch CB6: TypeId()
#include "Game/Toolbar.h"   // Toolbar::TypeId() (ENDGAME B.3.9 TR)
#include "Game/Weather.h"   // Weather::TypeId() (ENDGAME B.3.9 TR)
#include "graphics/GraphicsMesh.h"   // Graphics::GraphicsMesh::TypeId() (ENDGAME B.3.9 TR)
#include "ui/StringTable.h"   // UI::StringTable::TypeId() (D5, ENDGAME B.3.9 CG3)
#include "Game/serialize_externals.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A — Object::External / ExternalFactory
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstdio>
#include "Game/exe_bodies.h"

// Shared body — mirrors FUN_14001b8e0 / FUN_1400196a0 line by line.
// Exposed (external linkage) via serialize_externals.h so the Tutorial
// ExternalPtr-array serialize helpers can reuse this exact body.
//
// Cascade safety: if typeIdPtr is null OR *typeIdPtr is 0 (Runtime DLL hasn't
// registered the underlying type yet), we still call FindOrLoad but skip the
// IsDerivedFrom check. The original would deref a NULL BaseTypeInfo and crash;
// our reimpl gracefully falls through with a one-shot warning.
// B.4 GAME-b: the per-reference body is typed (an Object::External* slot and an
// IO::Serializer&) and shared by every ExternalPtr serializer of this family —
// ExternalPtrSerializeGeneric, the 0x140007140 copy (serialize_externals_typed.cpp)
// and the MaterialInstance array (serialize_struct60b_serialize.cpp); they
// differed only in their log tags. The C entry points keep `void*` (about 120
// callers hold the fields with their own types): their two LEFT casts live in
// ForwardRef alone.
void SerializeExternalRefChecked(::Object::External*& ref, IO::Serializer& serializer,
                                 uint32_t typeId, const char* typeName, const char* tag) {
    if (!::Object::ExternalFactory::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[%s<%s>] runtime primitives unwired — "
                   "skipping serialize (cascade-safe no-op)\n",
                   tag, typeName ? typeName : "?");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    bool typeIdResolved = (typeId != 0);
    // The type name is resolved through the factory's lazy lookup when the type
    // id is not registered yet (strip any namespace prefix: "Graphics::X" -> "X").
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
            printf("[%s<%s>] typeId not yet registered — "
                   "loading without type validation\n",
                   tag, typeName ? typeName : "?");
            fflush(stdout);
            s_typeWarned = true;
        }
    }
    SerializeExternalRef(ref, serializer, typeId, typeIdResolved);
}

// The body FUN_14001b8e0 / FUN_1400196a0 / FUN_140007140 share, one reference.
void SerializeExternalRef(::Object::External*& ref, IO::Serializer& serializer,
                          uint32_t typeId, bool typeIdResolved) {
    ::Object::ExternalFactory* efactory = ::Object::ExternalFactory::_singleton;

    // Loads one path and keeps it when it derives from the type (or the type is
    // not registered yet).
    auto load = [&](const System::Filename& path) -> ::Object::External* {
        // LEFT: FindOrLoad returns a const External*; the slot holds External*.
        ::Object::External* loaded = const_cast<::Object::External*>(efactory->FindOrLoad(path));
        if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId)))
            return loaded;
        return nullptr;
    };

    // Encoding vtable +0x10 is slot 2 == IO::Encoding::GetKind() (B34_RULES §1.1).
    const int encState = serializer.GetEncoding().GetKind();
    if (encState == 3) {
        // Path-export (read-only) mode: walk the existing reference only.
        if (!ref) return;
        System::Filename pathBuf = ref->GetPath();
        const ::Object::External* found = efactory->FindOrLoad(pathBuf);
        if (found && typeIdResolved) {
            // LEFT: IsDerivedFromOrTrust takes a non-const void* (factory_registration.h).
            ::Object::IsDerivedFromOrTrust(const_cast<::Object::External*>(found), typeId);
        }
        return;
    }

    const int direction = static_cast<int>(serializer.GetDirection());   // 0 = loading
    System::Filename fnameBuf;
    if (efactory->IsReferenceModeEnabled()) {
        uint32_t refId = 0xFFFFFFFFu;
        if (ref) refId = efactory->GetReferenceId(ref);
        bool needsLoad = false;
        if (!ref || direction != 0 || refId != 0xFFFFFFFFu) {
            serializer.Serialize(refId, nullptr);
            if (refId != 0xFFFFFFFFu) {
                if (direction == 0) {
                    System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                    fnameBuf = tmpFn;
                }
                needsLoad = true;
            }
        } else {
            serializer.Serialize(refId, nullptr);
        }
        if (needsLoad && static_cast<int>(serializer.GetDirection()) == 0)
            ref = load(fnameBuf);
    } else {
        bool hasRef = (ref != nullptr);
        serializer.Serialize(hasRef, nullptr);
        if (hasRef) {
            if (ref) {
                System::Filename pathBuf = ref->GetPath();
                fnameBuf = pathBuf;
            }
            fnameBuf.AsString().Serialize(&serializer);
            if (static_cast<int>(serializer.GetDirection()) == 0)
                ref = load(fnameBuf);
        }
    }
}

namespace {
// The C entry points' two LEFT casts: the slot and the serializer arrive as void*
// from callers holding them with their own types.
void ForwardRef(void* refField, void* serializer, uint32_t typeId, const char* typeName) {
    SerializeExternalRefChecked(*reinterpret_cast<::Object::External**>(refField),
                                *reinterpret_cast<IO::Serializer*>(serializer),
                                typeId, typeName, "ExternalPtr::Serialize");
}
} // namespace

void ExternalPtrSerializeGeneric(void** refField, void* serializer,
                                 const uint32_t* typeIdPtr,
                                 const char* typeName) {
    if (!refField || !serializer) return;
    ForwardRef(refField, serializer, typeIdPtr ? *typeIdPtr : 0u, typeName);
}

extern "C" void Reimpl_ExternalPtr_Serialize_GraphicsMesh(
    void* refField, void* serializer) {
    if (!refField || !serializer) return;
    ForwardRef(refField, serializer, Graphics::GraphicsMesh::TypeId(), "Graphics::GraphicsMesh");
}

extern "C" void Reimpl_ExternalPtr_Serialize_Weather(
    void* refField, void* serializer) {
    if (!refField || !serializer) return;
    ForwardRef(refField, serializer, Weather::TypeId(), "Weather");
}

extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(
    void* refField, void* serializer) {
    if (!refField || !serializer) return;
    ForwardRef(refField, serializer, UI::StringTable::TypeId(), "StringTable");
}

extern "C" void Reimpl_ExternalPtr_Serialize_Toolbar(
    void* refField, void* serializer) {
    if (!refField || !serializer) return;
    ForwardRef(refField, serializer, Toolbar::TypeId(), "Toolbar");
}

extern "C" void Reimpl_ExternalPtr_Serialize_EnvironmentDesc(
    void* refField, void* serializer) {
    if (!refField || !serializer) return;
    ForwardRef(refField, serializer, EnvironmentDescription::TypeId(), "EnvironmentDescription");
}
